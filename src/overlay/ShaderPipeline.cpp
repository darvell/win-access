/**
 * Clarity Layer - Shader Pipeline Implementation
 */

#include "ShaderPipeline.h"
#include "util/Logger.h"

#include <d3dcompiler.h>
#include <fstream>
#include <filesystem>

namespace clarity {

// Vertex structure
struct Vertex {
    float x, y;
    float u, v;
};

ShaderPipeline::ShaderPipeline() = default;
ShaderPipeline::~ShaderPipeline() = default;

bool ShaderPipeline::Initialize(ID3D11Device* device, const std::wstring& shadersPath) {
    if (!device) {
        LOG_ERROR("ShaderPipeline::Initialize - null device");
        return false;
    }

    m_device = device;
    m_device->GetImmediateContext(&m_context);
    m_shadersPath = shadersPath;

    // Load shaders
    if (!LoadVertexShader(L"fullscreen_vs.cso")) {
        LOG_ERROR("Failed to load vertex shader");
        return false;
    }

    if (!LoadPixelShader(L"contrast.cso", m_contrastShader)) {
        LOG_ERROR("Failed to load contrast shader");
        return false;
    }

    if (!LoadPixelShader(L"invert.cso", m_invertShader)) {
        LOG_ERROR("Failed to load invert shader");
        return false;
    }

    if (!LoadPixelShader(L"edge_enhance.cso", m_edgeShader)) {
        LOG_WARN("Edge enhance shader not found - edge enhancement disabled");
        // Not fatal - edge enhancement is optional
    }

    if (!LoadPixelShader(L"passthrough.cso", m_passthroughShader)) {
        LOG_ERROR("Failed to load passthrough shader");
        return false;
    }

    // Create constant buffer
    if (!CreateConstantBuffer()) {
        LOG_ERROR("Failed to create constant buffer");
        return false;
    }

    // Create fullscreen quad
    if (!CreateFullscreenQuad()) {
        LOG_ERROR("Failed to create fullscreen quad");
        return false;
    }

    m_ready = true;
    LOG_INFO("ShaderPipeline initialized");
    return true;
}

ID3D11Texture2D* ShaderPipeline::Process(ID3D11Texture2D* input) {
    if (!m_ready || !input) return input;

    // Get input size
    D3D11_TEXTURE2D_DESC inputDesc;
    input->GetDesc(&inputDesc);

    // Resize intermediate textures if needed
    if (inputDesc.Width != m_outputWidth || inputDesc.Height != m_outputHeight) {
        if (!CreateIntermediateTextures(inputDesc.Width, inputDesc.Height)) {
            LOG_ERROR("Failed to create intermediate textures");
            return input;
        }
    }

    // Update constant buffer if parameters changed
    if (m_paramsDirty) {
        UpdateParameters();
    }

    // Create SRV for input texture
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = inputDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ComPtr<ID3D11ShaderResourceView> inputSRV;
    HRESULT hr = m_device->CreateShaderResourceView(input, &srvDesc, &inputSRV);
    if (FAILED(hr)) {
        LOG_WARN("Failed to create input SRV");
        return input;
    }

    // Set common state
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_inputLayout.Get());

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
    m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

    // Multi-pass rendering
    ID3D11ShaderResourceView* currentInput = inputSRV.Get();

    // Pass 1: Contrast/brightness/gamma/saturation adjustment
    RenderPass(currentInput, m_intermediateRTV.Get(), m_contrastShader.Get(),
               m_outputWidth, m_outputHeight);
    currentInput = m_intermediateSRV.Get();

    // Pass 2: Inversion (if enabled)
    if (m_params.invertMode != 0) {
        // Swap intermediate and output for ping-pong
        RenderPass(currentInput, m_outputRTV.Get(), m_invertShader.Get(),
                   m_outputWidth, m_outputHeight);
        currentInput = m_outputSRV.Get();
    }

    // Pass 3: Edge enhancement (if enabled and shader available)
    if (m_params.edgeStrength > 0.0f && m_edgeShader) {
        ID3D11RenderTargetView* finalTarget = (currentInput == m_outputSRV.Get())
            ? m_intermediateRTV.Get() : m_outputRTV.Get();

        RenderPass(currentInput, finalTarget, m_edgeShader.Get(),
                   m_outputWidth, m_outputHeight);

        // Update output reference
        if (finalTarget == m_intermediateRTV.Get()) {
            return m_intermediateTexture.Get();
        }
    }

    // If we ended on intermediate, copy to output
    if (currentInput == m_intermediateSRV.Get()) {
        RenderPass(currentInput, m_outputRTV.Get(), m_passthroughShader.Get(),
                   m_outputWidth, m_outputHeight);
    }

    return m_outputTexture.Get();
}

void ShaderPipeline::SetContrast(float contrast) {
    m_params.contrast = std::clamp(contrast, 0.0f, 4.0f);
    m_paramsDirty = true;
}

void ShaderPipeline::SetBrightness(float brightness) {
    m_params.brightness = std::clamp(brightness, -1.0f, 1.0f);
    m_paramsDirty = true;
}

void ShaderPipeline::SetGamma(float gamma) {
    m_params.gamma = std::clamp(gamma, 0.1f, 4.0f);
    m_paramsDirty = true;
}

void ShaderPipeline::SetSaturation(float saturation) {
    m_params.saturation = std::clamp(saturation, 0.0f, 2.0f);
    m_paramsDirty = true;
}

void ShaderPipeline::SetInvertMode(InvertMode mode) {
    m_params.invertMode = static_cast<int>(mode);
    m_paramsDirty = true;
}

void ShaderPipeline::SetEdgeStrength(float strength) {
    m_params.edgeStrength = std::clamp(strength, 0.0f, 1.0f);
    m_paramsDirty = true;
}

void ShaderPipeline::ApplyProfile(const VisualSettings& settings) {
    SetContrast(settings.contrast);
    SetBrightness(settings.brightness);
    SetGamma(settings.gamma);
    SetSaturation(settings.saturation);
    SetInvertMode(settings.invertMode);
    SetEdgeStrength(settings.edgeStrength);
}

void ShaderPipeline::UpdateParameters() {
    if (!m_constantBuffer) return;

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, &m_params, sizeof(TransformParams));
        m_context->Unmap(m_constantBuffer.Get(), 0);
        m_paramsDirty = false;
    }
}

bool ShaderPipeline::LoadVertexShader(const std::wstring& filename) {
    auto path = std::filesystem::path(m_shadersPath) / filename;

    // Read compiled shader
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open shader: {}", path.string());
        return false;
    }

    std::vector<char> data((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();

    // Create vertex shader
    HRESULT hr = m_device->CreateVertexShader(data.data(), data.size(), nullptr, &m_vertexShader);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create vertex shader: 0x{:08X}", hr);
        return false;
    }

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = m_device->CreateInputLayout(layout, 2, data.data(), data.size(), &m_inputLayout);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create input layout: 0x{:08X}", hr);
        return false;
    }

    return true;
}

bool ShaderPipeline::LoadPixelShader(const std::wstring& filename, ComPtr<ID3D11PixelShader>& shader) {
    auto path = std::filesystem::path(m_shadersPath) / filename;

    // Read compiled shader
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        LOG_WARN("Shader file not found: {}", path.string());
        return false;
    }

    std::vector<char> data((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();

    // Create pixel shader
    HRESULT hr = m_device->CreatePixelShader(data.data(), data.size(), nullptr, &shader);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create pixel shader: 0x{:08X}", hr);
        return false;
    }

    return true;
}

bool ShaderPipeline::CreateIntermediateTextures(int width, int height) {
    m_outputWidth = width;
    m_outputHeight = height;

    // Release old textures
    m_intermediateTexture.Reset();
    m_intermediateRTV.Reset();
    m_intermediateSRV.Reset();
    m_outputTexture.Reset();
    m_outputRTV.Reset();
    m_outputSRV.Reset();

    // Create intermediate texture
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = m_device->CreateTexture2D(&texDesc, nullptr, &m_intermediateTexture);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create intermediate texture: 0x{:08X}", hr);
        return false;
    }

    hr = m_device->CreateRenderTargetView(m_intermediateTexture.Get(), nullptr, &m_intermediateRTV);
    if (FAILED(hr)) return false;

    hr = m_device->CreateShaderResourceView(m_intermediateTexture.Get(), nullptr, &m_intermediateSRV);
    if (FAILED(hr)) return false;

    // Create output texture
    hr = m_device->CreateTexture2D(&texDesc, nullptr, &m_outputTexture);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create output texture: 0x{:08X}", hr);
        return false;
    }

    hr = m_device->CreateRenderTargetView(m_outputTexture.Get(), nullptr, &m_outputRTV);
    if (FAILED(hr)) return false;

    hr = m_device->CreateShaderResourceView(m_outputTexture.Get(), nullptr, &m_outputSRV);
    if (FAILED(hr)) return false;

    LOG_DEBUG("Created intermediate textures: {}x{}", width, height);
    return true;
}

bool ShaderPipeline::CreateConstantBuffer() {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(TransformParams);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = &m_params;

    HRESULT hr = m_device->CreateBuffer(&desc, &data, &m_constantBuffer);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create constant buffer: 0x{:08X}", hr);
        return false;
    }

    return true;
}

bool ShaderPipeline::CreateFullscreenQuad() {
    // Fullscreen quad vertices
    Vertex vertices[] = {
        { -1.0f,  1.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 1.0f, 0.0f },
        {  1.0f, -1.0f, 1.0f, 1.0f },
        { -1.0f, -1.0f, 0.0f, 1.0f },
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;

    HRESULT hr = m_device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create vertex buffer: 0x{:08X}", hr);
        return false;
    }

    // Index buffer
    uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;

    hr = m_device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create index buffer: 0x{:08X}", hr);
        return false;
    }

    // Sampler state
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = m_device->CreateSamplerState(&samplerDesc, &m_sampler);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create sampler: 0x{:08X}", hr);
        return false;
    }

    return true;
}

void ShaderPipeline::RenderPass(ID3D11ShaderResourceView* input,
                                 ID3D11RenderTargetView* output,
                                 ID3D11PixelShader* shader,
                                 int width, int height) {
    // Set render target
    m_context->OMSetRenderTargets(1, &output, nullptr);

    // Set viewport
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    // Clear
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->ClearRenderTargetView(output, clearColor);

    // Set shader and texture
    m_context->PSSetShader(shader, nullptr, 0);
    m_context->PSSetShaderResources(0, 1, &input);

    // Draw
    m_context->DrawIndexed(6, 0, 0);

    // Unbind SRV to allow it to be used as RTV in next pass
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullSRV);
}

} // namespace clarity
