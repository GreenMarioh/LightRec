#include "Exporter.h"
#include "../mux/MP4Muxer.h"
#include <windows.h>
#include <shlobj.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace lightrec::export_ {

Exporter::Exporter(core::Pipeline& pipeline, std::chrono::seconds maxDuration)
    : pipeline_(pipeline)
    , audioClipBuffer_(maxDuration, 48000, 2)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    qpcFreq_ = freq.QuadPart;
}

Exporter::~Exporter() {
    stop();
}

void Exporter::start() {
    if (running_) return;
    running_ = true;
    audioThread_ = std::thread(&Exporter::audioThreadFunc, this);
}

void Exporter::stop() {
    if (!running_) return;
    running_ = false;
    if (audioThread_.joinable()) {
        audioThread_.join();
    }
}

void Exporter::audioThreadFunc() {
    std::vector<float> tempBuf(48000); // Max 1s of audio per read
    while (running_) {
        size_t read_count = pipeline_.getAudioBuffer().read(tempBuf.data(), tempBuf.size());
        if (read_count > 0) {
            uint64_t latest_qpc = pipeline_.getLatestAudioQpc();
            int64_t latest_pts = (latest_qpc * 10'000'000) / qpcFreq_;
            audioClipBuffer_.push(tempBuf.data(), read_count, latest_pts);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

bool Exporter::exportClip(std::chrono::seconds duration, const std::string& gameName) {
    auto video_packets = pipeline_.getVideoBuffer().snapshot();
    if (video_packets.empty()) {
        return false;
    }

    int64_t target_start_pts = video_packets.back().pts - (duration.count() * 10'000'000);

    // Find nearest IDR frame BEFORE or AT target_start_pts
    auto best_idr = video_packets.end();

    for (auto it = video_packets.begin(); it != video_packets.end(); ++it) {
        if (it->isIDR) {
            if (it->pts <= target_start_pts) {
                best_idr = it;
            } else {
                if (best_idr == video_packets.end()) {
                    best_idr = it; // Fallback: earliest IDR if none before target
                }
                break;
            }
        }
    }

    if (best_idr == video_packets.end()) {
        return false; // No IDR found at all
    }

    int64_t clip_start_pts = best_idr->pts;
    int64_t clip_end_pts = video_packets.back().pts + video_packets.back().duration;

    auto audio_snap = audioClipBuffer_.snapshot(clip_start_pts, clip_end_pts);

    // Convert audio from float to int16_t for MP4Muxer (PCM_16)
    std::vector<int16_t> pcm16;
    pcm16.reserve(audio_snap.data.size());
    for (float sample : audio_snap.data) {
        float s = std::max(-1.0f, std::min(1.0f, sample));
        pcm16.push_back(static_cast<int16_t>(s * 32767.0f));
    }

    // Prepare Output Path
    PWSTR videosPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Videos, 0, NULL, &videosPath))) {
        return false;
    }
    std::filesystem::path outDir(videosPath);
    CoTaskMemFree(videosPath);

    outDir /= L"LightRec";
    outDir /= L"Clips";
    std::filesystem::create_directories(outDir);

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_s(&tm_now, &time_t_now);

    std::stringstream ss;
    ss << gameName << "_"
       << std::put_time(&tm_now, "%Y-%m-%d_%H-%M-%S") << ".mp4";
    
    std::filesystem::path outFile = outDir / ss.str();

    // Muxing
    mux::MP4Muxer::Config config;
    config.width = 1920; 
    config.height = 1080;
    config.videoFps = 60;
    config.audioFormat = mux::AudioFormat::PCM_16;
    config.audioSampleRate = 48000;
    config.audioChannels = 2;

    mux::MP4Muxer muxer(config);

    for (auto it = best_idr; it != video_packets.end(); ++it) {
        muxer.addVideoPacket(*it);
    }

    if (!pcm16.empty()) {
        int64_t audio_duration = (pcm16.size() / 2) * 10'000'000 / 48000;
        muxer.addAudioSamples(reinterpret_cast<const uint8_t*>(pcm16.data()), pcm16.size() * sizeof(int16_t), audio_snap.start_pts, audio_duration);
    }

    return muxer.finalize(outFile);
}

} // namespace lightrec::export_
