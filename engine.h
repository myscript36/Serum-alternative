#pragma once
#include <cstdint>
#include <memory>

namespace sx {

class Engine {
public:
    explicit Engine(float sampleRate);
    ~Engine();

    // ---- thread-safe control API (UI / MIDI threads) ----
    void setParam(int id, float value);
    float getParam(int id) const;
    void midi(uint8_t status, uint8_t d1, uint8_t d2);
    void noteOn(int note, int vel) { midi(0x90, (uint8_t)note, (uint8_t)vel); }
    void noteOff(int note) { midi(0x80, (uint8_t)note, 0); }
    void allNotesOff() { midi(0xB0, 123, 0); }

    // ---- audio thread ----
    void render(float* interleavedStereo, int frames);

    // ---- read-only monitoring ----
    void getScope(float* out, int n) const;
    float peakLevel() const;
    int activeVoices() const;

    // Preview of a wavetable frame (mip 0), decimated to n points. pos 0..1.
    static void tableFrame(int table, float pos, float* out, int n);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace sx
