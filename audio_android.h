#pragma once
#include "engine.h"
#include <aaudio/AAudio.h>
#include <atomic>
#include <mutex>

namespace sx {

// Low-latency float stereo output through AAudio, pulling audio from an Engine.
class AudioOut {
public:
    AudioOut(Engine* engine, int sampleRate) : engine_(engine), sampleRate_(sampleRate) {}
    ~AudioOut() { stop(); }
    bool start();
    void stop();
    int framesPerBurst() const { return burst_; }

private:
    static aaudio_data_callback_result_t onData(AAudioStream* s, void* user, void* data, int32_t frames);
    static void onError(AAudioStream* s, void* user, aaudio_result_t err);
    bool open();
    void restartAsync();

    Engine* engine_;
    int sampleRate_;
    int burst_ = 0;
    AAudioStream* stream_ = nullptr;
    std::mutex mutex_;
    std::atomic<bool> wanted_{false};
};

}  // namespace sx
