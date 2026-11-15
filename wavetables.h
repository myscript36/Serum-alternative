#pragma once
#include <vector>
#include <cstdint>

namespace sx {

constexpr int kTableSize = 1024;   // samples per single-cycle frame
constexpr int kFrames = 64;        // frames per wavetable
constexpr int kMips = 9;           // band-limited levels: cap = 512 >> mip harmonics
constexpr int kTableStride = kTableSize + 1;  // +1 guard sample for interpolation

class WavetableBank {
public:
    static WavetableBank& get();
    // Generates all tables (takes a few hundred ms). Safe to call more than once.
    void init();
    bool ready() const { return ready_; }

    const float* frame(int table, int mip, int frame) const {
        return data_.data() + (((size_t)table * kMips + mip) * kFrames + frame) * kTableStride;
    }

    // mip level for a phase increment (cycles per sample) so no harmonic exceeds Nyquist
    static int mipFor(float inc) {
        int m = 0;
        while (m < kMips - 1 && (float)(512 >> m) * inc > 0.5f) m++;
        return m;
    }

    // Reads one sample: pos 0..1 (frame morph), ph 0..1 (cycle phase)
    inline float read(int table, int mip, float pos, float ph) const {
        float fp = pos * (float)(kFrames - 1);
        int f0 = (int)fp;
        if (f0 > kFrames - 2) f0 = kFrames - 2;
        float ff = fp - (float)f0;
        const float* a = frame(table, mip, f0);
        const float* b = a + kTableStride;  // next frame is adjacent in memory
        float idx = ph * (float)kTableSize;
        int i = (int)idx;
        if (i > kTableSize - 1) i = kTableSize - 1;
        float fr = idx - (float)i;
        float va = a[i] + (a[i + 1] - a[i]) * fr;
        float vb = b[i] + (b[i + 1] - b[i]) * fr;
        return va + (vb - va) * ff;
    }

private:
    WavetableBank() = default;
    std::vector<float> data_;
    bool ready_ = false;
};

}  // namespace sx
