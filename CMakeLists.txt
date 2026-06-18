cmake_minimum_required(VERSION 3.24)
project(LightRec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compile Shader
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/RgbToNv12.cso
    COMMAND fxc.exe /T cs_5_0 /E main /Fo ${CMAKE_CURRENT_BINARY_DIR}/RgbToNv12.cso ${CMAKE_CURRENT_SOURCE_DIR}/shaders/RgbToNv12.hlsl
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/RgbToNv12.hlsl
    COMMENT "Compiling RgbToNv12.hlsl"
)

# ImGui static library
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp
    third_party/imgui/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC third_party/imgui third_party/imgui/backends)

# Main executable with WIN32 subsystem
set(SOURCES
    src/main.cpp
    src/audio/AudioEncoder.cpp
    src/audio/WASAPICapture.cpp
    src/capture/CaptureFactory.cpp
    src/capture/DXGICapture.cpp
    src/capture/WGCCapture.cpp
    src/clip/AudioClipBuffer.cpp
    src/clip/RingBuffer.cpp
    src/config/Config.cpp
    src/core/Application.cpp
    src/core/D3D11Device.cpp
    src/core/GpuConverter.cpp
    src/core/Pipeline.cpp
    src/core/TexturePool.cpp
    src/encoder/AMFEncoder.cpp
    src/encoder/EncoderFactory.cpp
    src/encoder/MFTEncoder.cpp
    src/encoder/NVENCEncoder.cpp
    src/encoder/QSVEncoder.cpp
    src/export/Exporter.cpp
    src/mux/MP4Muxer.cpp
    src/ui/HotkeyManager.cpp
    src/ui/OverlayWindow.cpp
    src/ui/SettingsDialog.cpp
    src/ui/TrayIcon.cpp
    src/ui/UIConfig.cpp
    src/util/Log.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/RgbToNv12.cso
)

add_executable(LightRec WIN32 ${SOURCES})

# Fetch encoder headers
include(FetchContent)

# ffnvcodec
FetchContent_Declare(
    ffnvcodec
    GIT_REPOSITORY https://github.com/FFmpeg/nv-codec-headers.git
    GIT_TAG n12.2.72.0
)
FetchContent_GetProperties(ffnvcodec)
if(NOT ffnvcodec_POPULATED)
    FetchContent_Populate(ffnvcodec)
endif()

# AMF
FetchContent_Declare(
    amf
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/AMF.git
    GIT_TAG v1.4.33
)
FetchContent_GetProperties(amf)
if(NOT amf_POPULATED)
    FetchContent_Populate(amf)
endif()

# libvpl
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    libvpl
    GIT_REPOSITORY https://github.com/intel/libvpl.git
    GIT_TAG v2.10.2
)
FetchContent_MakeAvailable(libvpl)

# Include directories
target_include_directories(LightRec PRIVATE 
    src 
    third_party/imgui
    third_party/imgui/backends
    third_party/json/include
    ${ffnvcodec_SOURCE_DIR}/include
    ${amf_SOURCE_DIR}/amf/public/include
    ${libvpl_SOURCE_DIR}/api
)

# Link libraries
target_link_libraries(LightRec PRIVATE
    d3d11 dxgi d3dcompiler dxguid
    mf mfplat mfreadwrite mfuuid
    windowsapp RuntimeObject
    shell32
    ole32 uuid
    imgui
    VPL
)

# MSVC Compile Options
target_compile_options(LightRec PRIVATE /W4 /permissive- /await:strict)
target_compile_definitions(LightRec PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)

# Post-build copy shader to output directory
add_custom_command(TARGET LightRec POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/RgbToNv12.cso
    $<TARGET_FILE_DIR:LightRec>/RgbToNv12.cso
)
