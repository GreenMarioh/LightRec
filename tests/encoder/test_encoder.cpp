#include <d3d11.h>
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <objbase.h>
#include <mfapi.h>
#include <wrl/client.h>

// Include our mock headers
#include "mock_headers/core/D3D11Device.h"
#include "mock_headers/config/Config.h"

// Include encoder headers
#include "../../src/encoder/IEncoder.h"
#include "../../src/encoder/EncoderFactory.h"
#include "../../src/encoder/QSVEncoder.h"
#include "../../src/encoder/NVENCEncoder.h"
#include "../../src/encoder/AMFEncoder.h"
#include "../../src/encoder/MFTEncoder.h"

void TestAnnexBParser() {
    std::cout << "Running TestAnnexBParser..." << std::endl;

    // 0x00 00 00 01 [SPS (type 7)] 0x00 00 01 [PPS (type 8)] 0x00 00 01 [IDR (type 5)]
    std::vector<uint8_t> annexB = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x11, 0x22, 0x33, // SPS (type 7, payload size 4)
        0x00, 0x00, 0x01, 0x68, 0x44, 0x55,             // PPS (type 8, payload size 3)
        0x00, 0x00, 0x01, 0x65, 0xaa, 0xbb, 0xcc, 0xdd  // IDR (type 5, payload size 5)
    };

    auto nals = IEncoder::ParseAnnexB(annexB.data(), annexB.size());
    assert(nals.size() == 3);
    assert(nals[0].type == 7);
    assert(nals[0].size == 4);
    assert(nals[1].type == 8);
    assert(nals[1].size == 3);
    assert(nals[2].type == 5);
    assert(nals[2].size == 5);

    EncodedPacket packet = IEncoder::ConvertAnnexBToAvcc(annexB.data(), annexB.size(), 100, 33);
    assert(packet.isIDR == true);
    assert(packet.pts == 100);
    assert(packet.duration == 33);
    assert(packet.sps.size() == 4);
    assert(packet.pps.size() == 3);
    
    // Packet data should contain only the IDR frame in AVCC format (4-byte length + 5-byte payload)
    assert(packet.data.size() == 9);
    assert(packet.data[0] == 0);
    assert(packet.data[1] == 0);
    assert(packet.data[2] == 0);
    assert(packet.data[3] == 5);
    assert(packet.data[4] == 0x65);
    assert(packet.data[5] == 0xaa);
    assert(packet.data[6] == 0xbb);
    assert(packet.data[7] == 0xcc);
    assert(packet.data[8] == 0xdd);

    std::cout << "TestAnnexBParser passed successfully!" << std::endl;
}

void TestFactoryAndEncoders() {
    std::cout << "Running TestFactoryAndEncoders..." << std::endl;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    assert(SUCCEEDED(hr));
    
    hr = MFStartup(MF_VERSION);
    assert(SUCCEEDED(hr));

    // Create D3D11 device
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context;
    D3D_FEATURE_LEVEL featureLevel;
    
    hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &d3d11Device,
        &featureLevel,
        &d3d11Context
    );

    if (FAILED(hr)) {
        std::cout << "Hardware D3D11 device creation failed, trying WARP..." << std::endl;
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &d3d11Device,
            &featureLevel,
            &d3d11Context
        );
    }

    assert(SUCCEEDED(hr));
    std::cout << "D3D11 Device created successfully." << std::endl;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    hr = d3d11Device.As(&dxgiDevice);
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    assert(SUCCEEDED(hr));

    D3D11Device device(d3d11Device, d3d11Context, adapter);

    Config config;
    config.width = 1280;
    config.height = 720;
    config.fps = 60;
    config.bitrateMbps = 4;

    GpuVendor vendor = EncoderFactory::probeGpuVendor(adapter.Get());
    std::cout << "Detected GPU Vendor: ";
    if (vendor == GpuVendor::Intel) std::cout << "Intel (0x8086)" << std::endl;
    else if (vendor == GpuVendor::Nvidia) std::cout << "Nvidia (0x10DE)" << std::endl;
    else if (vendor == GpuVendor::Amd) std::cout << "AMD (0x1002)" << std::endl;
    else std::cout << "Unknown/Software" << std::endl;

    // Create encoder through factory
    std::unique_ptr<IEncoder> encoder;
    try {
        encoder = EncoderFactory::create(device, config);
        assert(encoder != nullptr);
        std::cout << "Encoder created successfully through factory." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Encoder creation failed: " << e.what() << std::endl;
        assert(false);
    }

    bool gotPacket = false;
    encoder->setOutputCallback([&gotPacket](EncodedPacket packet) {
        gotPacket = true;
        std::cout << "Received packet: size=" << packet.data.size() 
                  << " bytes, keyframe=" << packet.isIDR 
                  << ", SPS=" << packet.sps.size() 
                  << ", PPS=" << packet.pps.size() << std::endl;
    });

    // Create a dummy NV12 texture to encode
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = config.width;
    texDesc.Height = config.height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_NV12;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12Texture;
    hr = d3d11Device->CreateTexture2D(&texDesc, nullptr, &nv12Texture);
    assert(SUCCEEDED(hr));

    std::cout << "Submitting dummy frames for encoding..." << std::endl;
    for (int i = 0; i < 15; ++i) {
        encoder->encode(nv12Texture.Get(), i * 166666); // 100ns units
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "Flushing encoder..." << std::endl;
    encoder->flush();

    std::cout << "Flushed. Packets received: " << (gotPacket ? "Yes" : "No") << std::endl;

    MFShutdown();
    CoUninitialize();

    std::cout << "TestFactoryAndEncoders passed successfully!" << std::endl;
}

int main() {
    try {
        TestAnnexBParser();
        TestFactoryAndEncoders();
        std::cout << "All tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
