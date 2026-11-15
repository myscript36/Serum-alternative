#include "audio_android.h"
#include <android/log.h>
#include <thread>
#include <chrono>

#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

#define LOG_TAG "WaveForge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace sx {

static inline void enableFlushToZero() {
#if defined(__aarch64__)
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    fpcr |= (1ull << 24);  // FZ
    __asm__ volatile("msr fpcr, %0" ::"r"(fpcr));
#elif defined(__x86_64__) || defined(__i386__)
    _mm_setcsr(_mm_getcsr() | 0x8040);  // FTZ + DAZ
#endif
}

aaudio_data_callback_result_t AudioOut::onData(AAudioStream*, void* user, void* data, int32_t frames) {
    enableFlushToZero();
    auto* self = static_cast<AudioOut*>(user);
    self->engine_->render(static_cast<float*>(data), frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AudioOut::onError(AAudioStream*, void* user, aaudio_result_t err) {
    LOGE("AAudio error %d", err);
    auto* self = static_cast<AudioOut*>(user);
    if (err == AAUDIO_ERROR_DISCONNECTED) self->restartAsync();
}

void AudioOut::restartAsync() {
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        if (!wanted_.load()) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (stream_) { AAudioStream_close(stream_); stream_ = nullptr; }
        if (wanted_.load()) open();
    }).detach();
}

bool AudioOut::open() {
    AAudioStreamBuilder* b = nullptr;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return false;
    AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(b, 2);
    AAudioStreamBuilder_setSampleRate(b, sampleRate_);
    AAudioStreamBuilder_setDataCallback(b, &AudioOut::onData, this);
    AAudioStreamBuilder_setErrorCallback(b, &AudioOut::onError, this);
    aaudio_result_t r = AAudioStreamBuilder_openStream(b, &stream_);
    if (r != AAUDIO_OK) {  // exclusive may be refused: retry shared
        AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
        r = AAudioStreamBuilder_openStream(b, &stream_);
    }
    AAudioStreamBuilder_delete(b);
    if (r != AAUDIO_OK) { LOGE("open failed: %s", AAudio_convertResultToText(r)); stream_ = nullptr; return false; }
    burst_ = AAudioStream_getFramesPerBurst(stream_);
    AAudioStream_setBufferSizeInFrames(stream_, burst_ * 2);
    LOGI("stream: rate=%d burst=%d", AAudioStream_getSampleRate(stream_), burst_);
    r = AAudioStream_requestStart(stream_);
    if (r != AAUDIO_OK) { LOGE("start failed"); AAudioStream_close(stream_); stream_ = nullptr; return false; }
    return true;
}

bool AudioOut::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    wanted_ = true;
    if (stream_) return true;
    return open();
}

void AudioOut::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    wanted_ = false;
    if (stream_) {
        AAudioStream_requestStop(stream_);
        AAudioStream_close(stream_);
        stream_ = nullptr;
    }
}

}  // namespace sx
