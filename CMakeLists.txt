cmake_minimum_required(VERSION 3.21)
project(ClarityLayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Detect compiler
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(USING_CLANG TRUE)
    message(STATUS "Using Clang compiler: ${CMAKE_CXX_COMPILER}")
else()
    set(USING_CLANG FALSE)
    message(STATUS "Using MSVC compiler: ${CMAKE_CXX_COMPILER}")
endif()

# Compiler-specific flags
if(USING_CLANG)
    # Clang-cl flags (Windows clang uses MSVC-compatible flags)
    add_compile_options(
        /W4                     # High warning level
        /EHsc                   # Exception handling
        -Wno-unknown-pragmas    # Ignore unknown pragmas
        -Wno-unused-parameter   # Allow unused parameters
        -Wno-missing-field-initializers
    )

    # C++/WinRT coroutine support
    add_compile_options(/await:strict)

    # Enable address sanitizer in debug builds (optional)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND ENABLE_ASAN)
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()
else()
    # MSVC flags
    add_compile_options(
        /W4                     # High warning level
        /EHsc                   # Exception handling
        /permissive-            # Strict conformance
        /Zc:__cplusplus         # Correct __cplusplus macro
        /Zc:preprocessor        # Use new preprocessor
        /await:strict           # C++20 coroutines
    )

    # Enable multi-processor compilation
    add_compile_options(/MP)
endif()

# Find vcpkg packages
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Windows SDK libraries
set(WINRT_LIBS
    windowsapp
    dwmapi
    magnification
    sapi
    ole32
    oleaut32
    uuid
    propsys
    shcore
    winmm
)

# Source files
set(SOURCES
    src/main.cpp
    src/core/Controller.cpp
    src/core/ProfileManager.cpp
    src/core/HotkeyService.cpp
    src/overlay/CaptureManager.cpp
    src/overlay/OverlayWindow.cpp
    src/overlay/ShaderPipeline.cpp
    src/magnifier/MagnifierController.cpp
    src/magnifier/FocusTracker.cpp
    src/magnifier/LensRenderer.cpp
    src/reader/AccessibilityReader.cpp
    src/reader/OcrReader.cpp
    src/reader/SpeechEngine.cpp
    src/ui/SettingsWindow.cpp
    src/ui/TrayIcon.cpp
    src/util/Logger.cpp
    src/util/AudioFeedback.cpp
    src/util/SafeMode.cpp
)

# Header files
set(HEADERS
    src/core/Controller.h
    src/core/ProfileManager.h
    src/core/HotkeyService.h
    src/overlay/CaptureManager.h
    src/overlay/OverlayWindow.h
    src/overlay/ShaderPipeline.h
    src/magnifier/MagnifierController.h
    src/magnifier/FocusTracker.h
    src/magnifier/LensRenderer.h
    src/reader/AccessibilityReader.h
    src/reader/OcrReader.h
    src/reader/SpeechEngine.h
    src/ui/SettingsWindow.h
    src/ui/TrayIcon.h
    src/util/Logger.h
    src/util/AudioFeedback.h
    src/util/SafeMode.h
)

add_executable(ClarityLayer WIN32 ${SOURCES} ${HEADERS})

target_include_directories(ClarityLayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ClarityLayer PRIVATE
    ${WINRT_LIBS}
    d3d11
    dxgi
    d3dcompiler
    dxguid
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

# Compile definitions
target_compile_definitions(ClarityLayer PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    WINRT_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    # Enable C++/WinRT fast ABI
    WINRT_FAST_ABI_SIZE=999999
    # Define 'interface' for UIAutomation headers (needed with /permissive-)
    interface=struct
)

# Debug definitions
target_compile_definitions(ClarityLayer PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# Set subsystem to Windows
set_target_properties(ClarityLayer PROPERTIES
    WIN32_EXECUTABLE TRUE
    VS_DPI_AWARE "PerMonitor"
    # Use unicode runtime
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

# Find DirectX Shader Compiler (dxc) or fall back to fxc
find_program(DXC_EXECUTABLE dxc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{WindowsSdkDir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
find_program(FXC_EXECUTABLE fxc HINTS "$ENV{WindowsSdkDir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")

if(DXC_EXECUTABLE)
    set(SHADER_COMPILER ${DXC_EXECUTABLE})
    set(SHADER_COMPILER_NAME "dxc")
    message(STATUS "Found DXC shader compiler: ${DXC_EXECUTABLE}")
elseif(FXC_EXECUTABLE)
    set(SHADER_COMPILER ${FXC_EXECUTABLE})
    set(SHADER_COMPILER_NAME "fxc")
    message(STATUS "Found FXC shader compiler: ${FXC_EXECUTABLE}")
else()
    # Fall back to hoping it's in PATH
    set(SHADER_COMPILER "fxc")
    set(SHADER_COMPILER_NAME "fxc")
    message(WARNING "Could not find shader compiler, using fxc from PATH")
endif()

# Compile shaders
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/overlay/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

set(PIXEL_SHADER_FILES
    ${SHADER_DIR}/contrast.hlsl
    ${SHADER_DIR}/invert.hlsl
    ${SHADER_DIR}/edge_enhance.hlsl
    ${SHADER_DIR}/lens.hlsl
    ${SHADER_DIR}/passthrough.hlsl
)

# Compile pixel shaders
foreach(SHADER ${PIXEL_SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
    set(SHADER_OUTPUT ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.cso)

    if(SHADER_COMPILER_NAME STREQUAL "dxc")
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${SHADER_COMPILER} -T ps_5_0 -E main -Fo ${SHADER_OUTPUT} ${SHADER}
            DEPENDS ${SHADER}
            COMMENT "Compiling pixel shader ${SHADER_NAME} with dxc"
            VERBATIM
        )
    else()
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${SHADER_COMPILER} /nologo /T ps_5_0 /E main /Fo ${SHADER_OUTPUT} ${SHADER}
            DEPENDS ${SHADER}
            COMMENT "Compiling pixel shader ${SHADER_NAME} with fxc"
            VERBATIM
        )
    endif()
    list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT})
endforeach()

# Compile vertex shader
set(VS_OUTPUT ${SHADER_OUTPUT_DIR}/fullscreen_vs.cso)
if(SHADER_COMPILER_NAME STREQUAL "dxc")
    add_custom_command(
        OUTPUT ${VS_OUTPUT}
        COMMAND ${SHADER_COMPILER} -T vs_5_0 -E main -Fo ${VS_OUTPUT} ${SHADER_DIR}/fullscreen_vs.hlsl
        DEPENDS ${SHADER_DIR}/fullscreen_vs.hlsl
        COMMENT "Compiling vertex shader with dxc"
        VERBATIM
    )
else()
    add_custom_command(
        OUTPUT ${VS_OUTPUT}
        COMMAND ${SHADER_COMPILER} /nologo /T vs_5_0 /E main /Fo ${VS_OUTPUT} ${SHADER_DIR}/fullscreen_vs.hlsl
        DEPENDS ${SHADER_DIR}/fullscreen_vs.hlsl
        COMMENT "Compiling vertex shader with fxc"
        VERBATIM
    )
endif()
list(APPEND COMPILED_SHADERS ${VS_OUTPUT})

add_custom_target(Shaders ALL DEPENDS ${COMPILED_SHADERS})
add_dependencies(ClarityLayer Shaders)

# Copy shaders to output directory
add_custom_command(TARGET ClarityLayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${SHADER_OUTPUT_DIR}
    $<TARGET_FILE_DIR:ClarityLayer>/shaders
    COMMENT "Copying shaders to output directory"
)

# Copy assets to output directory
add_custom_command(TARGET ClarityLayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:ClarityLayer>/assets
    COMMENT "Copying assets to output directory"
)

# Copy default profile
add_custom_command(TARGET ClarityLayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/profiles
    $<TARGET_FILE_DIR:ClarityLayer>/profiles
    COMMENT "Copying profiles to output directory"
)

# Generate compile_commands.json for clangd/IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Tests (optional, enable with -DBUILD_TESTS=ON)
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install rules
install(TARGETS ClarityLayer
    RUNTIME DESTINATION bin
)
install(DIRECTORY ${SHADER_OUTPUT_DIR}/
    DESTINATION bin/shaders
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/
    DESTINATION bin/assets
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/profiles/
    DESTINATION bin/profiles
)

# Print configuration summary
message(STATUS "")
message(STATUS "Clarity Layer Configuration:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shader compiler: ${SHADER_COMPILER_NAME}")
message(STATUS "")
