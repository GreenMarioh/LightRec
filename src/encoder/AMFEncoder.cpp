#include "AMFEncoder.h"
#include <stdexcept>
#include <iostream>
#include <thread>

typedef AMF_RESULT(AMF_CDECL_CALL* AMFInit_Fn)(amf_uint64 version, amf::AMFFactory** ppFactory);

AMFEncoder::~AMFEncoder() {
    cleanup();
}

void AMFEncoder::cleanup() {
    initialized_ = false;

    if (encoder_) {
        encoder_->Terminate();
        encoder_ = nullptr;
    }
    context_ = nullptr;
    factory_ = nullptr;

    if (amfLib_) {
        FreeLibrary(amfLib_);
        amfLib_ = nullptr;
    }
}

void AMFEncoder::init(D3D11Device& device, const Config& config) {
    cleanup();
    config_ = config;

    amfLib_ = LoadLibraryW(L"amfrt64.dll");
    if (!amfLib_) {
        throw std::runtime_error("AMFEncoder: Failed to load amfrt64.dll");
    }

    AMFInit_Fn amfInit = reinterpret_cast<AMFInit_Fn>(GetProcAddress(amfLib_, AMF_INIT_FUNCTION_NAME));
    if (!amfInit) {
        cleanup();
        throw std::runtime_error("AMFEncoder: GetProcAddress failed for AMFInit");
    }

    AMF_RESULT res = amfInit(AMF_FULL_VERSION, &factory_);
    if (res != AMF_OK) {
        cleanup();
        throw std::runtime_error("AMFEncoder: AMFInit failed");
    }

    res = factory_->CreateContext(&context_);
    if (res != AMF_OK) {
        cleanup();
        throw std::runtime_error("AMFEncoder: Failed to create AMF context");
    }

    res = context_->InitDX11(device.device());
    if (res != AMF_OK) {
        cleanup();
        throw std::runtime_error("AMFEncoder: Failed to initialize DX11 context in AMF");
    }

    res = factory_->CreateComponent(context_, AMFVideoEncoderVCE_AVC, &encoder_);
    if (res != AMF_OK) {
        cleanup();
        throw std::runtime_error("AMFEncoder: Failed to create AMF H264 VCE component");
    }

    // Set Encoder properties
    encoder_->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_LOW_LATENCY);
    encoder_->SetProperty(AMF_VIDEO_ENCODER_PROFILE, AMF_VIDEO_ENCODER_PROFILE_MAIN);
    encoder_->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, amf_int64(config.bitrateMbps * 1000000));
    encoder_->SetProperty(AMF_VIDEO_ENCODER_PEAK_BITRATE, amf_int64(config.bitrateMbps * 1000000));
    encoder_->SetProperty(AMF_VIDEO_ENCODER_FRAMESIZE, ::AMFConstructSize(config.width, config.height));
    encoder_->SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, ::AMFConstructRate(config.fps, 1));
    encoder_->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, amf_int64(0)); // No B-frames
    encoder_->SetProperty(AMF_VIDEO_ENCODER_IDR_PERIOD, amf_int64(config.fps * 2)); // 2s GOP size
    encoder_->SetProperty(AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD, AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR);

    res = encoder_->Init(amf::AMF_SURFACE_NV12, config.width, config.height);
    if (res != AMF_OK) {
        cleanup();
        throw std::runtime_error("AMFEncoder: Failed to initialize AMF encoder component");
    }

    lastPts_ = -1;
    initialized_ = true;
}

void AMFEncoder::pollOutput() {
    while (true) {
        amf::AMFDataPtr data;
        AMF_RESULT res = encoder_->QueryOutput(&data);
        if (res == AMF_OK && data != nullptr) {
            amf::AMFBufferPtr buffer(data);
            uint8_t* ptr = static_cast<uint8_t*>(buffer->GetNative());
            size_t size = buffer->GetSize();
            int64_t pts = buffer->GetPts();

            int64_t duration = (lastPts_ != -1) ? (pts - lastPts_) : (10000000 / config_.fps);
            lastPts_ = pts;

            EncodedPacket packet = ConvertAnnexBToAvcc(ptr, size, pts, duration);
            if (callback_) {
                callback_(std::move(packet));
            }
        } else if (res == AMF_REPEAT) {
            // No more output ready yet
            break;
        } else {
            // Error or EOF
            break;
        }
    }
}

void AMFEncoder::encode(ID3D11Texture2D* texture, int64_t pts) {
    if (!initialized_) return;

    amf::AMFSurfacePtr surface;
    AMF_RESULT res = context_->CreateSurfaceFromDX11Native(texture, &surface, nullptr);
    if (res != AMF_OK) {
        return;
    }

    surface->SetPts(pts);

    while (true) {
        res = encoder_->SubmitInput(surface);
        if (res == AMF_OK) {
            break;
        } else if (res == AMF_INPUT_FULL) {
            // The input queue is full, poll outputs first and wait a bit
            pollOutput();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
            // Error
            return;
        }
    }

    pollOutput();
}

void AMFEncoder::flush() {
    if (!initialized_) return;

    AMF_RESULT res = encoder_->Drain();
    if (res == AMF_OK) {
        while (true) {
            amf::AMFDataPtr data;
            res = encoder_->QueryOutput(&data);
            if (res == AMF_OK && data != nullptr) {
                amf::AMFBufferPtr buffer(data);
                uint8_t* ptr = static_cast<uint8_t*>(buffer->GetNative());
                size_t size = buffer->GetSize();
                int64_t pts = buffer->GetPts();

                int64_t duration = 10000000 / config_.fps;
                EncodedPacket packet = ConvertAnnexBToAvcc(ptr, size, pts, duration);
                if (callback_) {
                    callback_(std::move(packet));
                }
            } else if (res == AMF_EOF) {
                // Drain finished
                break;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
}

void AMFEncoder::setOutputCallback(OutputCallback cb) {
    callback_ = std::move(cb);
}
