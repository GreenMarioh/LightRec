#pragma once

#include "../core/Pipeline.h"
#include "../clip/AudioClipBuffer.h"
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <filesystem>

namespace lightrec::export_ {

class Exporter {
public:
    // We pass the pipeline to read from audioBuffer_ and videoBuffer_
    Exporter(core::Pipeline& pipeline, std::chrono::seconds maxDuration);
    ~Exporter();

    // Start background thread to drain audio SPSC queue
    void start();
    void stop();

    // Export the last `duration` seconds of video and audio
    // Saves to Videos/LightRec/Clips/GameName_YYYY-MM-DD_HH-MM-SS.mp4
    bool exportClip(std::chrono::seconds duration, const std::string& gameName);

private:
    void audioThreadFunc();

    core::Pipeline& pipeline_;
    clip::AudioClipBuffer audioClipBuffer_;

    std::atomic<bool> running_{false};
    std::thread audioThread_;
    int64_t qpcFreq_ = 0;
};

} // namespace lightrec::export_
