#include "../../src/clip/AudioClipBuffer.h"
#include "../../src/clip/EncodedPacket.h"
#include <cassert>
#include <iostream>

using namespace lightrec::clip;

void test_audio_clip_buffer() {
    AudioClipBuffer buf(std::chrono::seconds(2), 48000, 2);
    
    std::vector<float> data1(48000 * 2, 0.5f); // 1 second
    buf.push(data1.data(), data1.size(), 10'000'000); // Ends at 1s
    
    auto snap = buf.snapshot(5'000'000, 10'000'000); // 0.5s to 1s
    assert(snap.data.size() == 48000); // 0.5 seconds * 2 channels = 48000 samples
    assert(snap.start_pts == 5'000'000);
    
    std::vector<float> data2(48000 * 4, -0.5f); // 2 seconds
    buf.push(data2.data(), data2.size(), 30'000'000); // Overwrites previous
    
    auto snap2 = buf.snapshot(0, 30'000'000);
    assert(snap2.data.size() == 48000 * 4); // 2 seconds capacity
    assert(snap2.start_pts == 10'000'000); // base_pts should be 10M because it pushed 2s ending at 30M
    
    std::cout << "AudioClipBuffer tests passed." << std::endl;
}

int main() {
    test_audio_clip_buffer();
    return 0;
}
