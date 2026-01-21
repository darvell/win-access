/**
 * Clarity Layer - Lens Renderer Implementation
 */

#include "LensRenderer.h"
#include "util/Logger.h"

#include <fstream>
#include <filesystem>

namespace clarity {

struct Vertex {
    float x, y;
    float u, v;
};

LensRenderer::LensRenderer() = default;
LensRenderer::~LensRenderer() = default;

bool LensRenderer::Initialize(ID3D11Device* device, const std::wstring& shadersPath) {
    if (!device) return false;

    m_device = device;
    m_device->GetImmediateContext(&m_context);

    // Load vertex shader
    auto vsPath = std::filesystem::path(shadersPath) / L"fullscreen_vs.cso";
    std::ifstream vsFile(vsPath, std::ios::binary);
    if (!vsFile.is_open()) {
        LOG_ERROR("Failed to open vertex shader for lens");
        return false;
    }

    std::vector<char> vsData((std::istreambuf_iterator<char>(vsFile)),
                              std::istreambuf_iterator<char>());
    vsFile.close();

    HRESULT hr = m_device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &m_vertexShader);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create vertex shader: 0x{:08X}", hr);
        return false;
    }

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = m_device->CreateInputLayout(layout, 2, vsData.data(), vsData.size(), &m_inputLayout);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create input layout: 0x{:08X}", hr);
        return false;
    }

    // Load lens shader
    auto psPath = std::filesystem::path(shadersPath) / L"lens.cso";
    std::ifstream psFile(psPath, std::ios::binary);
    if (!psFile.is_open()) {
        LOG_ERROR("Failed to open lens shader");
        return false;
    }

    std::vector<char> psData((std::istreambuf_iterator<char>(psFile)),
                              std::istreambuf_iterator<char>());
    psFile.close();

    hr = m_device->CreatePixelShader(psData.data(), psData.size(), nullptr, &m_lensShader);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create lens shader: 0x{:08X}", hr);
        return false;
    }

    // Create constant buffer
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(LensParams);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_lensParamsBuffer);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create constant buffer: 0x{:08X}", hr);
        return false;
    }

    // Create fullscreen quad
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

    hr = m_device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer);
    if (FAILED(hr)) return false;

    uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;

    hr = m_device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer);
    if (FAILED(hr)) return false;

    // Create sampler
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = m_device->CreateSamplerState(&samplerDesc, &m_sampler);
    if (FAILED(hr)) return false;

    LOG_INFO("LensRenderer initialized");
    return true;
}

void LensRenderer::Render(ID3D11Texture2D* source,
                           ID3D11RenderTargetView* target,
                           POINT center,
                           float zoomLevel,
                           int lensSize) {
    // Validate all required resources
    if (!source || !target || !m_context || !m_device) return;

    // Get source texture dimensions
    D3D11_TEXTURE2D_DESC texDesc;
    source->GetDesc(&texDesc);

    // Validate texture dimensions to prevent division by zero
    if (texDesc.Width == 0 || texDesc.Height == 0) {
        return;
    }

    // Calculate lens parameters in UV space
    float centerU = static_cast<float>(center.x) / texDesc.Width;
    float centerV = static_cast<float>(center.y) / texDesc.Height;
    float radiusU = static_cast<float>(lensSize) / texDesc.Width / 2.0f;
    float radiusV = static_cast<float>(lensSize) / texDesc.Height / 2.0f;

    // Update constant buffer
    LensParams params;
    params.centerU = centerU;
    params.centerV = centerV;
    params.radiusU = radiusU;
    params.radiusV = radiusV;
    params.zoomLevel = zoomLevel;
    params.borderWidth = m_borderWidth / texDesc.Width;
    params.borderR = m_borderColor[0];
    params.borderG = m_borderColor[1];
    params.borderB = m_borderColor[2];
    params.borderA = m_borderColor[3];
    params.lensShape = (m_shape == Shape::Circular) ? 0 : 1;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(m_context->Map(m_lensParamsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, &params, sizeof(LensParams));
        m_context->Unmap(m_lensParamsBuffer.Get(), 0);
    }

    // Create SRV for source
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ComPtr<ID3D11ShaderResourceView> sourceSRV;
    m_device->CreateShaderResourceView(source, &srvDesc, &sourceSRV);

    // Set render state
    m_context->OMSetRenderTargets(1, &target, nullptr);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(texDesc.Width);
    viewport.Height = static_cast<float>(texDesc.Height);
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    // Set shaders and resources
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_lensShader.Get(), nullptr, 0);
    m_context->PSSetShaderResources(0, 1, sourceSRV.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
    m_context->PSSetConstantBuffers(0, 1, m_lensParamsBuffer.GetAddressOf());

    // Draw
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_inputLayout.Get());

    m_context->DrawIndexed(6, 0, 0);
}

void LensRenderer::SetBorderColor(float r, float g, float b, float a) {
    m_borderColor[0] = r;
    m_borderColor[1] = g;
    m_borderColor[2] = b;
    m_borderColor[3] = a;
}

void LensRenderer::SetBorderWidth(float width) {
    m_borderWidth = width;
}

} // namespace clarity
