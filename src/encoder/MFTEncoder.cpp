#include "MFTEncoder.h"
#include <mfidl.h>
#include <stdexcept>
#include <iostream>
#include <icodecapi.h>
#include <codecapi.h>
#include <mferror.h>

#ifndef FORMAT_MPEG2Video
// {e06d8026-db46-11cf-b4d1-00805f6cbbea}
static const GUID FORMAT_MPEG2Video = { 0xe06d8026, 0xdb46, 0x11cf, { 0xb4, 0xd1, 0x00, 0x80, 0x5f, 0x6c, 0xbb, 0xea } };
#endif

MFTEncoder::~MFTEncoder() {
    cleanup();
}

void MFTEncoder::cleanup() {
    initialized_ = false;
    transform_ = nullptr;
    dxgiMgr_ = nullptr;
}

void MFTEncoder::init(D3D11Device& device, const Config& config) {
    cleanup();
    config_ = config;

    HRESULT hr = MFCreateDXGIDeviceManager(&dxgiResetToken_, &dxgiMgr_);
    if (FAILED(hr)) {
        throw std::runtime_error("MFTEncoder: Failed to create DXGI Device Manager");
    }

    hr = dxgiMgr_->ResetDevice(device.device(), dxgiResetToken_);
    if (FAILED(hr)) {
        cleanup();
        throw std::runtime_error("MFTEncoder: Failed to reset DXGI device");
    }

    MFT_REGISTER_TYPE_INFO inputInfo = { MFMediaType_Video, MFVideoFormat_NV12 };
    MFT_REGISTER_TYPE_INFO outputInfo = { MFMediaType_Video, MFVideoFormat_H264 };

    IMFActivate** activates = nullptr;
    UINT32 count = 0;
    
    // First try hardware MFT H.264 encoder
    hr = MFTEnumEx(
        MFT_CATEGORY_VIDEO_ENCODER,
        MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
        &inputInfo,
        &outputInfo,
        &activates,
        &count
    );

    if (SUCCEEDED(hr) && count > 0) {
        hr = activates[0]->ActivateObject(IID_PPV_ARGS(&transform_));
        for (UINT32 i = 0; i < count; i++) {
            activates[i]->Release();
        }
        CoTaskMemFree(activates);
    }

    // Fallback to software MFT H.264 encoder if hardware failed or wasn't found
    if (!transform_) {
        hr = MFTEnumEx(
            MFT_CATEGORY_VIDEO_ENCODER,
            MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
            &inputInfo,
            &outputInfo,
            &activates,
            &count
        );
        if (SUCCEEDED(hr) && count > 0) {
            hr = activates[0]->ActivateObject(IID_PPV_ARGS(&transform_));
            for (UINT32 i = 0; i < count; i++) {
                activates[i]->Release();
            }
            CoTaskMemFree(activates);
        }
    }

    if (!transform_) {
        cleanup();
        throw std::runtime_error("MFTEncoder: H.264 MFT Encoder not found");
    }

    // Unlock async MFT if needed
    Microsoft::WRL::ComPtr<IMFAttributes> attributes;
    hr = transform_->GetAttributes(&attributes);
    if (SUCCEEDED(hr)) {
        attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
    }

    // Try setting D3D manager
    hr = transform_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(dxgiMgr_.Get()));
    if (FAILED(hr)) {
        // Some software encoders don't support sharing D3D textures directly
    }

    // Configure Output Media Type
    Microsoft::WRL::ComPtr<IMFMediaType> outType;
    hr = MFCreateMediaType(&outType);
    if (FAILED(hr)) {
        cleanup();
        throw std::runtime_error("MFTEncoder: Failed to create output media type");
    }

    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    outType->SetUINT32(MF_MT_AVG_BITRATE, config.bitrateMbps * 1000 * 1000);
    MFSetAttributeSize(outType.Get(), MF_MT_FRAME_SIZE, config.width, config.height);
    MFSetAttributeRatio(outType.Get(), MF_MT_FRAME_RATE, config.fps, 1);
    outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeRatio(outType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    hr = transform_->SetOutputType(0, outType.Get(), 0);
    if (FAILED(hr)) {
        cleanup();
        char errBuf[64];
        sprintf(errBuf, "MFTEncoder: Failed to set output media type, hr=0x%08X", (unsigned int)hr);
        throw std::runtime_error(errBuf);
    }

    // Configure Input Media Type
    Microsoft::WRL::ComPtr<IMFMediaType> inType;
    hr = MFCreateMediaType(&inType);
    if (FAILED(hr)) {
        cleanup();
        throw std::runtime_error("MFTEncoder: Failed to create input media type");
    }

    inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(inType.Get(), MF_MT_FRAME_SIZE, config.width, config.height);
    MFSetAttributeRatio(inType.Get(), MF_MT_FRAME_RATE, config.fps, 1);
    inType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeRatio(inType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    hr = transform_->SetInputType(0, inType.Get(), 0);
    if (FAILED(hr)) {
        cleanup();
        char errBuf[64];
        sprintf(errBuf, "MFTEncoder: Failed to set input media type, hr=0x%08X", (unsigned int)hr);
        throw std::runtime_error(errBuf);
    }

    // Configure Rate Control and GOP
    Microsoft::WRL::ComPtr<ICodecAPI> codecApi;
    hr = transform_.As(&codecApi);
    if (SUCCEEDED(hr)) {
        VARIANT val = {};
        val.vt = VT_UI4;
        val.ulVal = config.bitrateMbps * 1000 * 1000;
        codecApi->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &val);

        val.vt = VT_UI4;
        val.ulVal = eAVEncCommonRateControlMode_CBR;
        codecApi->SetValue(&CODECAPI_AVEncCommonRateControlMode, &val);

        val.vt = VT_UI4;
        val.ulVal = config.fps * 2;
        codecApi->SetValue(&CODECAPI_AVEncMPVGOPSize, &val);
    }

    lastPts_ = -1;
    initialized_ = true;
}

void MFTEncoder::processOutput() {
    MFT_OUTPUT_STREAM_INFO streamInfo = {};
    HRESULT hr = transform_->GetOutputStreamInfo(0, &streamInfo);
    if (FAILED(hr)) return;

    while (true) {
        MFT_OUTPUT_DATA_BUFFER outputData = {};
        outputData.dwStreamID = 0;

        Microsoft::WRL::ComPtr<IMFSample> outputSample;
        if (!(streamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
            hr = MFCreateSample(&outputSample);
            if (FAILED(hr)) break;

            Microsoft::WRL::ComPtr<IMFMediaBuffer> outputBuffer;
            hr = MFCreateMemoryBuffer(streamInfo.cbSize, &outputBuffer);
            if (FAILED(hr)) break;

            outputSample->AddBuffer(outputBuffer.Get());
            outputData.pSample = outputSample.Get();
        }

        DWORD status = 0;
        hr = transform_->ProcessOutput(0, 1, &outputData, &status);
        if (hr == S_OK) {
            IMFSample* sample = outputData.pSample;
            if (sample) {
                LONGLONG sampleTime = 0;
                sample->GetSampleTime(&sampleTime);

                LONGLONG sampleDuration = 0;
                sample->GetSampleDuration(&sampleDuration);

                Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
                hr = sample->ConvertToContiguousBuffer(&mediaBuffer);
                if (SUCCEEDED(hr)) {
                    BYTE* ptr = nullptr;
                    DWORD currentLength = 0;
                    hr = mediaBuffer->Lock(&ptr, nullptr, &currentLength);
                    if (SUCCEEDED(hr)) {
                        int64_t duration = (lastPts_ != -1) ? (sampleTime - lastPts_) : sampleDuration;
                        lastPts_ = sampleTime;

                        EncodedPacket packet = ConvertAnnexBToAvcc(ptr, currentLength, sampleTime, duration);
                        if (callback_) {
                            callback_(std::move(packet));
                        }
                        mediaBuffer->Unlock();
                    }
                }
            }

            if (streamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) {
                if (outputData.pSample) {
                    outputData.pSample->Release();
                }
            }
            if (outputData.pEvents) {
                outputData.pEvents->Release();
            }
        } else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
            break;
        } else {
            break;
        }
    }
}

void MFTEncoder::encode(ID3D11Texture2D* texture, int64_t pts) {
    if (!initialized_) return;

    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), texture, 0, FALSE, &buffer);
    if (FAILED(hr)) {
        return;
    }

    Microsoft::WRL::ComPtr<IMFSample> sample;
    hr = MFCreateSample(&sample);
    if (FAILED(hr)) {
        return;
    }

    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(pts);
    sample->SetSampleDuration(10000000 / config_.fps);

    hr = transform_->ProcessInput(0, sample.Get(), 0);
    if (SUCCEEDED(hr)) {
        processOutput();
    }
}

void MFTEncoder::flush() {
    if (!initialized_) return;

    HRESULT hr = transform_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
    if (SUCCEEDED(hr)) {
        processOutput();
    }
}

void MFTEncoder::setOutputCallback(OutputCallback cb) {
    callback_ = std::move(cb);
}
