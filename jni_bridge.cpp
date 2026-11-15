#include <jni.h>
#include <memory>
#include <mutex>
#include "engine.h"
#include "audio_android.h"
#include "params.h"

using namespace sx;

static std::mutex g_mutex;
static std::unique_ptr<Engine> g_engine;
static std::unique_ptr<AudioOut> g_audio;

extern "C" {

JNIEXPORT void JNICALL Java_com_waveforge_synth_NativeSynth_create(JNIEnv*, jobject, jint sampleRate) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_engine) return;
    g_engine.reset(new Engine((float)sampleRate));
    g_audio.reset(new AudioOut(g_engine.get(), sampleRate));
}

JNIEXPORT jboolean JNICALL Java_com_waveforge_synth_NativeSynth_startAudio(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_audio && g_audio->start() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_waveforge_synth_NativeSynth_stopAudio(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_audio) g_audio->stop();
}

JNIEXPORT void JNICALL Java_com_waveforge_synth_NativeSynth_setParam(JNIEnv*, jobject, jint id, jfloat v) {
    if (g_engine) g_engine->setParam(id, v);
}

JNIEXPORT void JNICALL Java_com_waveforge_synth_NativeSynth_midi(JNIEnv*, jobject, jint status, jint d1, jint d2) {
    if (g_engine) g_engine->midi((uint8_t)status, (uint8_t)d1, (uint8_t)d2);
}

JNIEXPORT void JNICALL Java_com_waveforge_synth_NativeSynth_scope(JNIEnv* env, jobject, jfloatArray arr) {
    if (!g_engine) return;
    jsize n = env->GetArrayLength(arr);
    jfloat* p = env->GetFloatArrayElements(arr, nullptr);
    g_engine->getScope(p, n);
    env->ReleaseFloatArrayElements(arr, p, 0);
}

JNIEXPORT jfloat JNICALL Java_com_waveforge_synth_NativeSynth_peak(JNIEnv*, jobject) {
    return g_engine ? g_engine->peakLevel() : 0.f;
}

JNIEXPORT jint JNICALL Java_com_waveforge_synth_NativeSynth_voices(JNIEnv*, jobject) {
    return g_engine ? g_engine->activeVoices() : 0;
}

JNIEXPORT void JNICALL Java_com_waveforge_synth_NativeSynth_tableFrame(JNIEnv* env, jobject, jint table, jfloat pos, jfloatArray arr) {
    jsize n = env->GetArrayLength(arr);
    jfloat* p = env->GetFloatArrayElements(arr, nullptr);
    Engine::tableFrame(table, pos, p, n);
    env->ReleaseFloatArrayElements(arr, p, 0);
}

JNIEXPORT jint JNICALL Java_com_waveforge_synth_NativeSynth_paramCount(JNIEnv*, jobject) { return P_COUNT; }

}  // extern "C"
