#include "NVENCEncoder.h"
#include <stdexcept>
#include <iostream>

typedef NVENCSTATUS(NVENCAPI* PNVENCODEAPICREATEINSTANCE)(NV_ENCODE_API_FUNCTION_LIST*);

NVENCEncoder::~NVENCEncoder() {
    cleanup();
}

void NVENCEncoder::cleanup() {
    initialized_ = false;

    if (encoder_ && nvencApi_.nvEncDestroyEncoder) {
        // Unregister events and destroy bitstream buffers
        for (auto& buf : outputBuffers_) {
            if (buf.bitstream && nvencApi_.nvEncDestroyBitstreamBuffer) {
                nvencApi_.nvEncDestroyBitstreamBuffer(encoder_, buf.bitstream);
            }
            if (buf.event) {
                if (nvencApi_.nvEncUnregisterAsyncEvent) {
                    NV_ENC_EVENT_PARAMS eventParams = {};
                    eventParams.version = NV_ENC_EVENT_PARAMS_VER;
                    eventParams.completionEvent = buf.event;
                    nvencApi_.nvEncUnregisterAsyncEvent(encoder_, &eventParams);
                }
                CloseHandle(buf.event);
            }
        }
        outputBuffers_.clear();

        // Unregister resources
        if (nvencApi_.nvEncUnregisterResource) {
            for (auto& pair : registeredResources_) {
                if (pair.second) {
                    nvencApi_.nvEncUnregisterResource(encoder_, pair.second);
                }
            }
        }
        registeredResources_.clear();

        nvencApi_.nvEncDestroyEncoder(encoder_);
        encoder_ = nullptr;
    }

    if (nvencLib_) {
        FreeLibrary(nvencLib_);
        nvencLib_ = nullptr;
    }
}

void NVENCEncoder::init(D3D11Device& device, const Config& config) {
    cleanup();
    config_ = config;
    d3d11Device_ = device.device();

    nvencLib_ = LoadLibraryW(L"nvEncodeAPI64.dll");
    if (!nvencLib_) {
        throw std::runtime_error("NVENCEncoder: Failed to load nvEncodeAPI64.dll");
    }

    auto createInstance = reinterpret_cast<PNVENCODEAPICREATEINSTANCE>(GetProcAddress(nvencLib_, "NvEncodeAPICreateInstance"));
    if (!createInstance) {
        cleanup();
        throw std::runtime_error("NVENCEncoder: GetProcAddress failed for NvEncodeAPICreateInstance");
    }

    nvencApi_.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    NVENCSTATUS status = createInstance(&nvencApi_);
    if (status != NV_ENC_SUCCESS) {
        cleanup();
        throw std::runtime_error("NVENCEncoder: NvEncodeAPICreateInstance failed");
    }

    // Open encode session
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS openParams = {};
    openParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    openParams.device = device.device();
    openParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    openParams.apiVersion = NVENCAPI_VERSION;

    status = nvencApi_.nvEncOpenEncodeSessionEx(&openParams, &encoder_);
    if (status != NV_ENC_SUCCESS) {
        cleanup();
        throw std::runtime_error("NVENCEncoder: nvEncOpenEncodeSessionEx failed");
    }

    // Initialize encoder parameters
    NV_ENC_INITIALIZE_PARAMS initParams = {};
    initParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
    initParams.encodeGUID = NV_ENC_CODEC_H264_GUID;
    initParams.presetGUID = NV_ENC_PRESET_P3_GUID;
    initParams.encodeWidth = config.width;
    initParams.encodeHeight = config.height;
    initParams.darWidth = config.width;
    initParams.darHeight = config.height;
    initParams.frameRateNum = config.fps;
    initParams.frameRateDen = 1;
    initParams.enablePTD = 1; // Let NVENC handle Picture Type Decision
    initParams.enableEncodeAsync = 1; // Asynchronous mode

    // Fetch default config for preset
    NV_ENC_PRESET_CONFIG presetConfig = {};
    presetConfig.version = NV_ENC_PRESET_CONFIG_VER;
    presetConfig.presetCfg.version = NV_ENC_CONFIG_VER;
    status = nvencApi_.nvEncGetEncodePresetConfig(encoder_, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_P3_GUID, &presetConfig);
    
    NV_ENC_CONFIG encodeConfig = {};
    encodeConfig.version = NV_ENC_CONFIG_VER;
    if (status == NV_ENC_SUCCESS) {
        encodeConfig = presetConfig.presetCfg;
    } else {
        encodeConfig.gopLength = config.fps * 2;
        encodeConfig.frameIntervalP = 1;
    }

    encodeConfig.gopLength = config.fps * 2;
    encodeConfig.frameIntervalP = 1; // No B-frames
    encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
    encodeConfig.rcParams.averageBitRate = config.bitrateMbps * 1000 * 1000;
    encodeConfig.rcParams.maxBitRate = config.bitrateMbps * 1000 * 1000;

    initParams.encodeConfig = &encodeConfig;

    status = nvencApi_.nvEncInitializeEncoder(encoder_, &initParams);
    if (status != NV_ENC_SUCCESS) {
        cleanup();
        throw std::runtime_error("NVENCEncoder: nvEncInitializeEncoder failed");
    }

    // Allocate output buffers
    outputBuffers_.resize(4);
    for (auto& buf : outputBuffers_) {
        NV_ENC_CREATE_BITSTREAM_BUFFER createParams = {};
        createParams.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
        status = nvencApi_.nvEncCreateBitstreamBuffer(encoder_, &createParams);
        if (status != NV_ENC_SUCCESS) {
            cleanup();
            throw std::runtime_error("NVENCEncoder: nvEncCreateBitstreamBuffer failed");
        }
        buf.bitstream = createParams.bitstreamBuffer;

        buf.event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!buf.event) {
            cleanup();
            throw std::runtime_error("NVENCEncoder: Failed to create event");
        }

        NV_ENC_EVENT_PARAMS eventParams = {};
        eventParams.version = NV_ENC_EVENT_PARAMS_VER;
        eventParams.completionEvent = buf.event;
        status = nvencApi_.nvEncRegisterAsyncEvent(encoder_, &eventParams);
        if (status != NV_ENC_SUCCESS) {
            cleanup();
            throw std::runtime_error("NVENCEncoder: nvEncRegisterAsyncEvent failed");
        }

        buf.busy = false;
    }

    currentBufferIdx_ = 0;
    lastPts_ = -1;
    initialized_ = true;
}

void NVENCEncoder::processOutputBuffer(OutputBuffer& buf) {
    if (!buf.busy) return;

    NV_ENC_LOCK_BITSTREAM lockParams = {};
    lockParams.version = NV_ENC_LOCK_BITSTREAM_VER;
    lockParams.outputBitstream = buf.bitstream;
    lockParams.doNotWait = 0;

    NVENCSTATUS status = nvencApi_.nvEncLockBitstream(encoder_, &lockParams);
    if (status == NV_ENC_SUCCESS) {
        if (callback_) {
            EncodedPacket packet = ConvertAnnexBToAvcc(
                static_cast<uint8_t*>(lockParams.bitstreamBufferPtr),
                lockParams.bitstreamSizeInBytes,
                buf.pts,
                buf.duration
            );
            callback_(std::move(packet));
        }
        nvencApi_.nvEncUnlockBitstream(encoder_, buf.bitstream);
    }
    buf.busy = false;
    ResetEvent(buf.event);
}

void NVENCEncoder::encode(ID3D11Texture2D* texture, int64_t pts) {
    if (!initialized_) return;

    OutputBuffer& buf = outputBuffers_[currentBufferIdx_];
    if (buf.busy) {
        // Wait for the buffer to finish encoding
        WaitForSingleObject(buf.event, INFINITE);
        processOutputBuffer(buf);
    }

    // Register texture resource if not done already
    auto it = registeredResources_.find(texture);
    NV_ENC_REGISTERED_PTR regRes = nullptr;
    if (it == registeredResources_.end()) {
        NV_ENC_REGISTER_RESOURCE regParams = {};
        regParams.version = NV_ENC_REGISTER_RESOURCE_VER;
        regParams.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
        regParams.resourceToRegister = texture;
        regParams.width = config_.width;
        regParams.height = config_.height;
        regParams.bufferFormat = NV_ENC_BUFFER_FORMAT_NV12;
        regParams.bufferUsage = NV_ENC_INPUT_IMAGE;
        
        NVENCSTATUS status = nvencApi_.nvEncRegisterResource(encoder_, &regParams);
        if (status != NV_ENC_SUCCESS) {
            return;
        }
        regRes = regParams.registeredResource;
        registeredResources_[texture] = regRes;
    } else {
        regRes = it->second;
    }

    // Map resource
    NV_ENC_MAP_INPUT_RESOURCE mapParams = {};
    mapParams.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
    mapParams.registeredResource = regRes;
    NVENCSTATUS status = nvencApi_.nvEncMapInputResource(encoder_, &mapParams);
    if (status != NV_ENC_SUCCESS) {
        return;
    }
    NV_ENC_INPUT_PTR inputPtr = mapParams.mappedResource;

    // Submit for encoding
    NV_ENC_PIC_PARAMS picParams = {};
    picParams.version = NV_ENC_PIC_PARAMS_VER;
    picParams.inputWidth = config_.width;
    picParams.inputHeight = config_.height;
    picParams.inputBuffer = inputPtr;
    picParams.outputBitstream = buf.bitstream;
    picParams.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12;
    picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    picParams.completionEvent = buf.event;

    int64_t duration = (lastPts_ != -1) ? (pts - lastPts_) : (10000000 / config_.fps);
    lastPts_ = pts;

    buf.pts = pts;
    buf.duration = duration;
    buf.busy = true;

    status = nvencApi_.nvEncEncodePicture(encoder_, &picParams);

    // Unmap input resource (can be done immediately as NVENC has scheduled the read)
    nvencApi_.nvEncUnmapInputResource(encoder_, inputPtr);

    if (status == NV_ENC_SUCCESS) {
        // Rotate buffer
        currentBufferIdx_ = (currentBufferIdx_ + 1) % outputBuffers_.size();
    } else {
        buf.busy = false;
    }
}

void NVENCEncoder::flush() {
    if (!initialized_) return;

    // Process all pending busy buffers
    for (auto& buf : outputBuffers_) {
        if (buf.busy) {
            WaitForSingleObject(buf.event, INFINITE);
            processOutputBuffer(buf);
        }
    }

    // Send end-of-stream command if supported
    NV_ENC_PIC_PARAMS picParams = {};
    picParams.version = NV_ENC_PIC_PARAMS_VER;
    picParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
    picParams.completionEvent = outputBuffers_[currentBufferIdx_].event;
    
    nvencApi_.nvEncEncodePicture(encoder_, &picParams);
}

void NVENCEncoder::setOutputCallback(OutputCallback cb) {
    callback_ = std::move(cb);
}
