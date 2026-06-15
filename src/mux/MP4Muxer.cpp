#include "MP4Muxer.h"
#include <cstring>
#include <iostream>

namespace lightrec::mux {

class Box {
public:
    Box(const char* type) {
        std::memcpy(m_type, type, 4);
    }
    void write8(uint8_t v) { m_data.push_back(v); }
    void write16(uint16_t v) { 
        m_data.push_back(v >> 8); 
        m_data.push_back(v & 0xFF); 
    }
    void write24(uint32_t v) {
        m_data.push_back((v >> 16) & 0xFF);
        m_data.push_back((v >> 8) & 0xFF);
        m_data.push_back(v & 0xFF);
    }
    void write32(uint32_t v) {
        m_data.push_back((v >> 24) & 0xFF);
        m_data.push_back((v >> 16) & 0xFF);
        m_data.push_back((v >> 8) & 0xFF);
        m_data.push_back(v & 0xFF);
    }
    void write64(uint64_t v) {
        write32(v >> 32);
        write32(v & 0xFFFFFFFF);
    }
    void writeString(const std::string& s) {
        m_data.insert(m_data.end(), s.begin(), s.end());
    }
    void writeData(const uint8_t* d, size_t s) {
        if (s > 0 && d != nullptr) {
            m_data.insert(m_data.end(), d, d + s);
        }
    }
    void addBox(const Box& b) {
        auto d = b.finalize();
        m_data.insert(m_data.end(), d.begin(), d.end());
    }
    std::vector<uint8_t> finalize() const {
        std::vector<uint8_t> out;
        uint32_t size = static_cast<uint32_t>(m_data.size() + 8);
        out.push_back((size >> 24) & 0xFF);
        out.push_back((size >> 16) & 0xFF);
        out.push_back((size >> 8) & 0xFF);
        out.push_back(size & 0xFF);
        out.insert(out.end(), m_type, m_type + 4);
        out.insert(out.end(), m_data.begin(), m_data.end());
        return out;
    }
private:
    char m_type[4];
    std::vector<uint8_t> m_data;
};

static std::vector<uint8_t> annexBtoAVCC(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> out;
    out.reserve(data.size());

    auto findStartCode = [](const uint8_t* p, size_t size, size_t& offset, size_t& scLen) {
        for (size_t i = 0; i + 2 < size; ++i) {
            if (p[i] == 0 && p[i+1] == 0) {
                if (p[i+2] == 1) {
                    offset = i;
                    scLen = 3;
                    return true;
                }
                if (i + 3 < size && p[i+2] == 0 && p[i+3] == 1) {
                    offset = i;
                    scLen = 4;
                    return true;
                }
            }
        }
        return false;
    };

    size_t pos = 0;
    size_t scOffset, scLen;
    if (findStartCode(data.data(), data.size(), scOffset, scLen)) {
        pos = scOffset + scLen;
        while (pos < data.size()) {
            size_t nextScOffset, nextScLen;
            bool found = findStartCode(data.data() + pos, data.size() - pos, nextScOffset, nextScLen);
            size_t naluSize = found ? nextScOffset : (data.size() - pos);
            
            // Filter out SPS (7) and PPS (8) and AUD (9) if they are in the stream
            // fMP4 expects them in avcC. 
            // Although having them in stream usually works, it's safer to strip AUD
            if (naluSize > 0) {
                uint8_t naluType = data[pos] & 0x1F;
                if (naluType != 7 && naluType != 8 && naluType != 9) {
                    out.push_back((naluSize >> 24) & 0xFF);
                    out.push_back((naluSize >> 16) & 0xFF);
                    out.push_back((naluSize >> 8) & 0xFF);
                    out.push_back(naluSize & 0xFF);
                    out.insert(out.end(), data.data() + pos, data.data() + pos + naluSize);
                }
            }
            
            if (!found) break;
            pos += nextScOffset + nextScLen;
        }
    } else {
        uint32_t naluSize = static_cast<uint32_t>(data.size());
        out.push_back((naluSize >> 24) & 0xFF);
        out.push_back((naluSize >> 16) & 0xFF);
        out.push_back((naluSize >> 8) & 0xFF);
        out.push_back(naluSize & 0xFF);
        out.insert(out.end(), data.begin(), data.end());
    }
    return out;
}

MP4Muxer::MP4Muxer(const Config& config) 
    : m_config(config) 
{
    m_videoFrag.trackId = 1;
    m_audioFrag.trackId = 2;
}

MP4Muxer::~MP4Muxer() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

void MP4Muxer::addVideoPacket(const clip::EncodedPacket& packet) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_headerWritten) {
        if (!packet.isIDR || packet.sps.empty() || packet.pps.empty()) {
            // Wait for keyframe
            return;
        }
        m_sps = packet.sps;
        m_pps = packet.pps;
    }

    if (packet.isIDR && m_headerWritten) {
        flushFragment();
    }

    SampleInfo info;
    info.pts = (packet.pts * 90000) / 10000000;
    info.duration = (packet.duration * 90000) / 10000000;
    info.isKeyframe = packet.isIDR;
    info.data = annexBtoAVCC(packet.data);
    info.size = info.data.size();

    m_videoFrag.samples.push_back(std::move(info));
    m_currentVideoDts += info.duration;
}

void MP4Muxer::addAudioSamples(const uint8_t* data, size_t size, int64_t pts, int64_t duration) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_headerWritten) {
        return; // Drop audio until video starts
    }

    SampleInfo info;
    info.pts = (pts * m_config.audioSampleRate) / 10000000;
    info.duration = (duration * m_config.audioSampleRate) / 10000000;
    if (info.duration == 0) {
        // fallback approximation if duration is 0
        if (m_config.audioFormat == AudioFormat::AAC) {
            info.duration = 1024; // AAC frame size
        } else {
            // PCM 16-bit 2-channel = 4 bytes per sample
            info.duration = size / (2 * m_config.audioChannels);
        }
    }
    info.isKeyframe = true; // Audio frames are always keyframes
    info.data.assign(data, data + size);
    info.size = size;

    m_audioFrag.samples.push_back(std::move(info));
    m_currentAudioDts += info.duration;
}

bool MP4Muxer::finalize(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_headerWritten && m_sps.empty()) {
        return false;
    }

    m_file.open(path, std::ios::binary);
    if (!m_file) return false;

    writeFtyp();
    writeMoov();
    m_headerWritten = true;

    if (!m_videoFrag.samples.empty() || !m_audioFrag.samples.empty()) {
        flushFragment();
    }

    m_file.close();
    return true;
}

void MP4Muxer::flushFragment() {
    if (!m_headerWritten || !m_file.is_open()) return;

    if (m_videoFrag.samples.empty() && m_audioFrag.samples.empty()) return;

    writeMoofAndMdat();

    m_videoFrag.samples.clear();
    m_videoFrag.baseDecodeTime = m_currentVideoDts;

    m_audioFrag.samples.clear();
    m_audioFrag.baseDecodeTime = m_currentAudioDts;

    m_sequenceNumber++;
}

void MP4Muxer::writeFtyp() {
    Box ftyp("ftyp");
    ftyp.writeString("isom");
    ftyp.write32(512);
    ftyp.writeString("isom");
    ftyp.writeString("iso2");
    ftyp.writeString("avc1");
    ftyp.writeString("mp41");
    
    auto data = ftyp.finalize();
    m_file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

void MP4Muxer::writeMoov() {
    Box moov("moov");

    Box mvhd("mvhd");
    mvhd.write8(0); // version
    mvhd.write24(0); // flags
    mvhd.write32(0); // creation_time
    mvhd.write32(0); // modification_time
    mvhd.write32(10000000); // timescale
    mvhd.write32(0); // duration
    mvhd.write32(0x00010000); // rate
    mvhd.write16(0x0100); // volume
    mvhd.write16(0); // reserved
    mvhd.write32(0); // reserved
    mvhd.write32(0); // reserved
    mvhd.write32(0x00010000); mvhd.write32(0); mvhd.write32(0); // matrix
    mvhd.write32(0); mvhd.write32(0x00010000); mvhd.write32(0);
    mvhd.write32(0); mvhd.write32(0); mvhd.write32(0x40000000);
    for (int i=0; i<6; ++i) mvhd.write32(0); // pre_defined
    mvhd.write32(3); // next_track_id
    moov.addBox(mvhd);

    // Video Track
    {
        Box trak("trak");
        Box tkhd("tkhd");
        tkhd.write8(0); // version
        tkhd.write24(3); // flags (enabled | in_movie)
        tkhd.write32(0); // creation
        tkhd.write32(0); // modification
        tkhd.write32(1); // track_id
        tkhd.write32(0); // reserved
        tkhd.write32(0); // duration
        tkhd.write32(0); tkhd.write32(0); // reserved
        tkhd.write16(0); // layer
        tkhd.write16(0); // alternate_group
        tkhd.write16(0); // volume
        tkhd.write16(0); // reserved
        tkhd.write32(0x00010000); tkhd.write32(0); tkhd.write32(0); // matrix
        tkhd.write32(0); tkhd.write32(0x00010000); tkhd.write32(0);
        tkhd.write32(0); tkhd.write32(0); tkhd.write32(0x40000000);
        tkhd.write32(m_config.width << 16); // width
        tkhd.write32(m_config.height << 16); // height
        trak.addBox(tkhd);

        Box mdia("mdia");
        Box mdhd("mdhd");
        mdhd.write8(0);
        mdhd.write24(0);
        mdhd.write32(0);
        mdhd.write32(0);
        mdhd.write32(90000); // timescale
        mdhd.write32(0); // duration
        mdhd.write16(0); // language
        mdhd.write16(0);
        mdia.addBox(mdhd);

        Box hdlr("hdlr");
        hdlr.write8(0);
        hdlr.write24(0);
        hdlr.write32(0);
        hdlr.writeString("vide");
        for(int i=0; i<3; ++i) hdlr.write32(0);
        hdlr.writeString("VideoHandler");
        hdlr.write8(0);
        mdia.addBox(hdlr);

        Box minf("minf");
        Box vmhd("vmhd");
        vmhd.write8(0);
        vmhd.write24(1);
        vmhd.write16(0);
        vmhd.write16(0);
        vmhd.write16(0);
        vmhd.write16(0);
        minf.addBox(vmhd);

        Box dinf("dinf");
        Box dref("dref");
        dref.write8(0);
        dref.write24(0);
        dref.write32(1);
        Box url("url ");
        url.write8(0);
        url.write24(1);
        dref.addBox(url);
        dinf.addBox(dref);
        minf.addBox(dinf);

        Box stbl("stbl");
        Box stsd("stsd");
        stsd.write8(0);
        stsd.write24(0);
        stsd.write32(1);
        
        Box avc1("avc1");
        for(int i=0; i<6; ++i) avc1.write8(0); // reserved
        avc1.write16(1); // data_reference_index
        avc1.write16(0); // pre_defined
        avc1.write16(0); // reserved
        for(int i=0; i<3; ++i) avc1.write32(0); // pre_defined
        avc1.write16(m_config.width);
        avc1.write16(m_config.height);
        avc1.write32(0x00480000); // horizres
        avc1.write32(0x00480000); // vertres
        avc1.write32(0); // reserved
        avc1.write16(1); // frame_count
        avc1.write8(0); // compressorname string length
        for(int i=0; i<31; ++i) avc1.write8(0); // compressorname
        avc1.write16(0x0018); // depth
        avc1.write16(0xFFFF); // pre_defined

        Box avcC("avcC");
        avcC.write8(1); // configurationVersion
        if (m_sps.size() >= 4) {
            avcC.write8(m_sps[1]); // AVCProfileIndication
            avcC.write8(m_sps[2]); // profile_compatibility
            avcC.write8(m_sps[3]); // AVCLevelIndication
        } else {
            avcC.write8(0x42); avcC.write8(0); avcC.write8(0x1F);
        }
        avcC.write8(0xFF); // lengthSizeMinusOne = 3
        avcC.write8(0xE1); // numOfSequenceParameterSets = 1
        avcC.write16(m_sps.size());
        avcC.writeData(m_sps.data(), m_sps.size());
        avcC.write8(1); // numOfPictureParameterSets = 1
        avcC.write16(m_pps.size());
        avcC.writeData(m_pps.data(), m_pps.size());
        
        avc1.addBox(avcC);
        stsd.addBox(avc1);
        stbl.addBox(stsd);

        Box stts("stts"); stts.write8(0); stts.write24(0); stts.write32(0); stbl.addBox(stts);
        Box stsc("stsc"); stsc.write8(0); stsc.write24(0); stsc.write32(0); stbl.addBox(stsc);
        Box stsz("stsz"); stsz.write8(0); stsz.write24(0); stsz.write32(0); stsz.write32(0); stbl.addBox(stsz);
        Box stco("stco"); stco.write8(0); stco.write24(0); stco.write32(0); stbl.addBox(stco);

        minf.addBox(stbl);
        mdia.addBox(minf);
        trak.addBox(mdia);
        moov.addBox(trak);
    }

    // Audio Track
    {
        Box trak("trak");
        Box tkhd("tkhd");
        tkhd.write8(0);
        tkhd.write24(3);
        tkhd.write32(0); tkhd.write32(0);
        tkhd.write32(2); // track_id
        tkhd.write32(0);
        tkhd.write32(0); // duration
        tkhd.write32(0); tkhd.write32(0);
        tkhd.write16(0); tkhd.write16(0);
        tkhd.write16(0x0100); tkhd.write16(0);
        tkhd.write32(0x00010000); tkhd.write32(0); tkhd.write32(0);
        tkhd.write32(0); tkhd.write32(0x00010000); tkhd.write32(0);
        tkhd.write32(0); tkhd.write32(0); tkhd.write32(0x40000000);
        tkhd.write32(0); tkhd.write32(0);
        trak.addBox(tkhd);

        Box mdia("mdia");
        Box mdhd("mdhd");
        mdhd.write8(0); mdhd.write24(0);
        mdhd.write32(0); mdhd.write32(0);
        mdhd.write32(m_config.audioSampleRate);
        mdhd.write32(0);
        mdhd.write16(0); mdhd.write16(0);
        mdia.addBox(mdhd);

        Box hdlr("hdlr");
        hdlr.write8(0); hdlr.write24(0); hdlr.write32(0);
        hdlr.writeString("soun");
        for(int i=0; i<3; ++i) hdlr.write32(0);
        hdlr.writeString("AudioHandler"); hdlr.write8(0);
        mdia.addBox(hdlr);

        Box minf("minf");
        Box smhd("smhd");
        smhd.write8(0); smhd.write24(0); smhd.write16(0); smhd.write16(0);
        minf.addBox(smhd);

        Box dinf("dinf");
        Box dref("dref");
        dref.write8(0); dref.write24(0); dref.write32(1);
        Box url("url "); url.write8(0); url.write24(1); dref.addBox(url);
        dinf.addBox(dref);
        minf.addBox(dinf);

        Box stbl("stbl");
        Box stsd("stsd");
        stsd.write8(0); stsd.write24(0); stsd.write32(1);
        
        if (m_config.audioFormat == AudioFormat::AAC) {
            Box mp4a("mp4a");
            for(int i=0; i<6; ++i) mp4a.write8(0);
            mp4a.write16(1); // data_reference_index
            for(int i=0; i<2; ++i) mp4a.write32(0); // reserved
            mp4a.write16(m_config.audioChannels); // channels
            mp4a.write16(16); // sample size
            mp4a.write16(0); // pre_defined
            mp4a.write16(0); // reserved
            mp4a.write32(m_config.audioSampleRate << 16); // samplerate

            Box esds("esds");
            esds.write8(0); esds.write24(0);
            esds.write8(0x03); esds.write8(0x19); // ES_DescrTag
            esds.write16(0); esds.write8(0);
            esds.write8(0x04); esds.write8(0x11); // DecoderConfigDescrTag
            esds.write8(0x40); // ObjectType (Audio ISO/IEC 14496-3)
            esds.write8(0x15); // StreamType (AudioStream)
            esds.write24(0); // bufferSizeDB
            esds.write32(0); // maxBitrate
            esds.write32(0); // avgBitrate
            esds.write8(0x05); esds.write8(0x02); // DecSpecificInfoTag
            
            // AAC LC config
            uint16_t asc = (2 << 11) | (3 << 7) | (2 << 3); // AAC LC, 48000, 2ch
            if (m_config.audioSampleRate == 44100) asc = (2 << 11) | (4 << 7) | (m_config.audioChannels << 3);
            esds.write16(asc);
            esds.write8(0x06); esds.write8(0x01); esds.write8(0x02); // SLConfigDescrTag
            
            mp4a.addBox(esds);
            stsd.addBox(mp4a);
        } else {
            // Uncompressed PCM
            Box pcm(m_config.audioFormat == AudioFormat::PCM_16 ? "sowt" : "raw ");
            for(int i=0; i<6; ++i) pcm.write8(0);
            pcm.write16(1); // data_reference_index
            for(int i=0; i<2; ++i) pcm.write32(0); // reserved
            pcm.write16(m_config.audioChannels);
            pcm.write16(16); // sample size
            pcm.write16(0); // pre_defined
            pcm.write16(0); // reserved
            pcm.write32(m_config.audioSampleRate << 16);
            stsd.addBox(pcm);
        }
        stbl.addBox(stsd);

        Box stts("stts"); stts.write8(0); stts.write24(0); stts.write32(0); stbl.addBox(stts);
        Box stsc("stsc"); stsc.write8(0); stsc.write24(0); stsc.write32(0); stbl.addBox(stsc);
        Box stsz("stsz"); stsz.write8(0); stsz.write24(0); stsz.write32(0); stsz.write32(0); stbl.addBox(stsz);
        Box stco("stco"); stco.write8(0); stco.write24(0); stco.write32(0); stbl.addBox(stco);

        minf.addBox(stbl); mdia.addBox(minf); trak.addBox(mdia); moov.addBox(trak);
    }

    Box mvex("mvex");
    {
        Box trex("trex");
        trex.write8(0); trex.write24(0);
        trex.write32(1); // track_id
        trex.write32(1); // default_sample_description_index
        trex.write32(0); // default_sample_duration
        trex.write32(0); // default_sample_size
        trex.write32(0); // default_sample_flags
        mvex.addBox(trex);
    }
    {
        Box trex("trex");
        trex.write8(0); trex.write24(0);
        trex.write32(2); // track_id
        trex.write32(1); // default_sample_description_index
        trex.write32(0); // default_sample_duration
        trex.write32(0); // default_sample_size
        trex.write32(0); // default_sample_flags
        mvex.addBox(trex);
    }
    moov.addBox(mvex);

    auto data = moov.finalize();
    m_file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

void MP4Muxer::writeMoofAndMdat() {
    Box moof("moof");
    Box mfhd("mfhd");
    mfhd.write8(0); mfhd.write24(0);
    mfhd.write32(m_sequenceNumber);
    moof.addBox(mfhd);

    uint32_t moofSizeEstimate = 8 + 16; 
    
    // Calculate sizes
    size_t videoMdatSize = 0;
    for (const auto& s : m_videoFrag.samples) videoMdatSize += s.size;
    size_t audioMdatSize = 0;
    for (const auto& s : m_audioFrag.samples) audioMdatSize += s.size;

    if (videoMdatSize > 0) {
        moofSizeEstimate += 8 + 16 + 20 + 20 + m_videoFrag.samples.size() * 16;
    }
    if (audioMdatSize > 0) {
        moofSizeEstimate += 8 + 16 + 20 + 20 + m_audioFrag.samples.size() * 16;
    }

    uint32_t mdatOffset = moofSizeEstimate + 8; // offset to mdat payload

    auto writeTraf = [&](const TrackFragment& frag, bool isVideo) {
        if (frag.samples.empty()) return;

        Box traf("traf");
        Box tfhd("tfhd");
        tfhd.write8(0);
        tfhd.write24(0x020000); // default-base-is-moof
        tfhd.write32(frag.trackId);
        traf.addBox(tfhd);

        Box tfdt("tfdt");
        tfdt.write8(1); // version 1 (64-bit baseMediaDecodeTime)
        tfdt.write24(0);
        tfdt.write64(frag.baseDecodeTime);
        traf.addBox(tfdt);

        Box trun("trun");
        trun.write8(0);
        trun.write24(0x000F01); // data-offset | sample-duration | sample-size | sample-flags
        trun.write32(static_cast<uint32_t>(frag.samples.size()));
        trun.write32(mdatOffset); // data offset

        for (const auto& s : frag.samples) {
            trun.write32(static_cast<uint32_t>(s.duration));
            trun.write32(static_cast<uint32_t>(s.size));
            if (isVideo) {
                uint32_t flags = s.isKeyframe ? 0x02000000 : 0x01010000;
                trun.write32(flags);
            } else {
                trun.write32(0x02000000); // audio always sync sample
            }
        }
        traf.addBox(trun);
        moof.addBox(traf);

        // Advance mdatOffset for the next track
        size_t fragSize = 0;
        for (const auto& s : frag.samples) fragSize += s.size;
        mdatOffset += static_cast<uint32_t>(fragSize);
    };

    writeTraf(m_videoFrag, true);
    writeTraf(m_audioFrag, false);

    auto moofData = moof.finalize();
    
    // Fix trun data offsets
    // Actual moof size might differ slightly from estimate due to var-int or Box internals (though it shouldn't here)
    // If it differs, we would need to patch it. Let's just patch it!
    uint32_t actualMoofSize = static_cast<uint32_t>(moofData.size());
    uint32_t actualMdatOffset = actualMoofSize + 8;
    
    // Quick offset patching
    size_t offsetPos = 0;
    for (int t = 0; t < 2; ++t) {
        const auto& frag = (t == 0) ? m_videoFrag : m_audioFrag;
        if (frag.samples.empty()) continue;
        
        // Find trun
        while (offsetPos + 8 < moofData.size()) {
            if (moofData[offsetPos+4] == 't' && moofData[offsetPos+5] == 'r' && moofData[offsetPos+6] == 'u' && moofData[offsetPos+7] == 'n') {
                // offset is at offsetPos + 16
                moofData[offsetPos+16] = (actualMdatOffset >> 24) & 0xFF;
                moofData[offsetPos+17] = (actualMdatOffset >> 16) & 0xFF;
                moofData[offsetPos+18] = (actualMdatOffset >> 8) & 0xFF;
                moofData[offsetPos+19] = actualMdatOffset & 0xFF;
                
                size_t fragSize = 0;
                for (const auto& s : frag.samples) fragSize += s.size;
                actualMdatOffset += static_cast<uint32_t>(fragSize);
                offsetPos += 8; // skip past trun header to continue search
                break;
            }
            offsetPos++;
        }
    }

    m_file.write(reinterpret_cast<const char*>(moofData.data()), moofData.size());

    // Write mdat
    uint32_t mdatSize = 8 + static_cast<uint32_t>(videoMdatSize + audioMdatSize);
    uint8_t mdatHeader[8] = {
        static_cast<uint8_t>((mdatSize >> 24) & 0xFF),
        static_cast<uint8_t>((mdatSize >> 16) & 0xFF),
        static_cast<uint8_t>((mdatSize >> 8) & 0xFF),
        static_cast<uint8_t>(mdatSize & 0xFF),
        'm', 'd', 'a', 't'
    };
    m_file.write(reinterpret_cast<const char*>(mdatHeader), 8);

    for (const auto& s : m_videoFrag.samples) {
        m_file.write(reinterpret_cast<const char*>(s.data.data()), s.size);
    }
    for (const auto& s : m_audioFrag.samples) {
        m_file.write(reinterpret_cast<const char*>(s.data.data()), s.size);
    }
}

} // namespace lightrec::mux
