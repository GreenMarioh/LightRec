#include "QSVEncoder.h"
#include <stdexcept>
#include <iostream>

QSVEncoder::~QSVEncoder() {
    cleanup();
}

void QSVEncoder::cleanup() {
    if (session_) {
        MFXVideoENCODE_Close(session_);
        MFXClose(session_);
        session_ = nullptr;
    }
    if (loader_) {
        MFXUnload(loader_);
        loader_ = nullptr;
    }
    initialized_ = false;
}

void QSVEncoder::init(D3D11Device& device, const Config& config) {
    cleanup();

    loader_ = MFXLoad();
    if (!loader_) {
        throw std::runtime_error("QSVEncoder: Failed to load oneVPL");
    }

    mfxConfig cfg = MFXCreateConfig(loader_);
    if (!cfg) {
        cleanup();
        throw std::runtime_error("QSVEncoder: Failed to create VPL config");
    }

    mfxVariant implVal;
    implVal.Version.Version = MFX_VARIANT_VERSION;
    implVal.Type = MFX_VARIANT_TYPE_U32;
    implVal.Data.U32 = MFX_IMPL_HARDWARE;
    mfxStatus sts = MFXSetConfigFilterProperty(cfg, (const mfxU8*)"mfxImplDescription.Impl", implVal);
    if (sts != MFX_ERR_NONE) {
        cleanup();
        throw std::runtime_error("QSVEncoder: Failed to set VPL implementation filter");
    }

    sts = MFXCreateSession(loader_, 0, &session_);
    if (sts != MFX_ERR_NONE) {
        cleanup();
        throw std::runtime_error("QSVEncoder: Failed to create VPL session");
    }

    // Set D3D11 device handle
    sts = MFXVideoCORE_SetHandle(session_, MFX_HANDLE_D3D11_DEVICE, device.device());
    if (sts != MFX_ERR_NONE) {
        cleanup();
        throw std::runtime_error("QSVEncoder: Failed to set D3D11 device handle on VPL session");
    }

    // Configure H.264 Encoder
    encodeParams_.mfx.CodecId = MFX_CODEC_AVC;
    encodeParams_.mfx.TargetUsage = MFX_TARGETUSAGE_BALANCED;
    encodeParams_.mfx.TargetKbps = config.bitrateMbps * 1000;
    encodeParams_.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    encodeParams_.mfx.GopRefDist = 1; // IPPP GOP structure (no B-frames)
    encodeParams_.mfx.GopPicSize = static_cast<mfxU16>(config.fps * 2); // 2 second GOP size
    encodeParams_.mfx.NumRefFrame = 1;

    encodeParams_.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    encodeParams_.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    encodeParams_.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    encodeParams_.mfx.FrameInfo.Width = (config.width + 15) & ~15;
    encodeParams_.mfx.FrameInfo.Height = (config.height + 31) & ~31;
    encodeParams_.mfx.FrameInfo.CropW = config.width;
    encodeParams_.mfx.FrameInfo.CropH = config.height;
    encodeParams_.mfx.FrameInfo.FrameRateExtN = config.fps;
    encodeParams_.mfx.FrameInfo.FrameRateExtD = 1;

    encodeParams_.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    // Validate parameters
    sts = MFXVideoENCODE_Query(session_, &encodeParams_, &encodeParams_);
    if (sts < 0) {
        cleanup();
        throw std::runtime_error("QSVEncoder: Query failed, unsupported parameters");
    }

    // Initialize encoder
    sts = MFXVideoENCODE_Init(session_, &encodeParams_);
    if (sts != MFX_ERR_NONE) {
        cleanup();
        throw std::runtime_error("QSVEncoder: Failed to initialize VPL encoder");
    }

    // Query allocation request to get suggested buffer size
    sts = MFXVideoENCODE_QueryIOSurf(session_, &encodeParams_, &allocRequest_);
    if (sts != MFX_ERR_NONE) {
        cleanup();
        throw std::runtime_error("QSVEncoder: Failed to query IO surface info");
    }

    // Allocate bitstream buffer (typically width * height * 4 is extremely safe)
    size_t bufferSize = config.width * config.height * 4;
    bitstreamBuffer_.resize(bufferSize);
    bitstream_.Data = bitstreamBuffer_.data();
    bitstream_.MaxLength = static_cast<mfxU32>(bitstreamBuffer_.size());
    bitstream_.DataOffset = 0;
    bitstream_.DataLength = 0;

    initialized_ = true;
    lastPts_ = -1;
}

void QSVEncoder::encode(ID3D11Texture2D* texture, int64_t pts) {
    if (!initialized_) return;

    mfxFrameSurface1 surface{};
    surface.Info = encodeParams_.mfx.FrameInfo;
    surface.Data.MemId = reinterpret_cast<mfxMemId>(texture);
    surface.Data.TimeStamp = pts;

    mfxSyncPoint syncPoint = nullptr;
    bitstream_.DataOffset = 0;
    bitstream_.DataLength = 0;

    while (true) {
        mfxStatus sts = MFXVideoENCODE_EncodeFrameAsync(session_, nullptr, &surface, &bitstream_, &syncPoint);
        if (sts == MFX_ERR_NONE) {
            // Wait for encoding completion
            sts = MFXVideoCORE_SyncOperation(session_, syncPoint, 60000); // 60s timeout
            if (sts == MFX_ERR_NONE && callback_) {
                int64_t duration = (lastPts_ != -1) ? (pts - lastPts_) : (10000000 / encodeParams_.mfx.FrameInfo.FrameRateExtN);
                lastPts_ = pts;

                EncodedPacket packet = ConvertAnnexBToAvcc(bitstream_.Data + bitstream_.DataOffset, bitstream_.DataLength, pts, duration);
                callback_(std::move(packet));
            }
            break;
        } else if (sts == MFX_WRN_DEVICE_BUSY) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        } else if (sts == MFX_ERR_MORE_DATA) {
            // Need more inputs to produce a packet
            break;
        } else {
            // Encoding error
            break;
        }
    }
}

void QSVEncoder::flush() {
    if (!initialized_) return;

    mfxSyncPoint syncPoint = nullptr;
    while (true) {
        bitstream_.DataOffset = 0;
        bitstream_.DataLength = 0;
        mfxStatus sts = MFXVideoENCODE_EncodeFrameAsync(session_, nullptr, nullptr, &bitstream_, &syncPoint);
        if (sts == MFX_ERR_NONE) {
            sts = MFXVideoCORE_SyncOperation(session_, syncPoint, 60000);
            if (sts == MFX_ERR_NONE && callback_) {
                int64_t pts = bitstream_.TimeStamp;
                int64_t duration = 10000000 / encodeParams_.mfx.FrameInfo.FrameRateExtN;
                EncodedPacket packet = ConvertAnnexBToAvcc(bitstream_.Data + bitstream_.DataOffset, bitstream_.DataLength, pts, duration);
                callback_(std::move(packet));
            }
        } else if (sts == MFX_WRN_DEVICE_BUSY) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        } else {
            // MFX_ERR_MORE_DATA means everything is flushed
            break;
        }
    }
}

void QSVEncoder::setOutputCallback(OutputCallback cb) {
    callback_ = std::move(cb);
}
