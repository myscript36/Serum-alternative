#include "engine.h"
#include "params.h"
#include "wavetables.h"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace sx {
namespace {

constexpr float kPi = 3.14159265358979f;
constexpr int kMaxVoices = 16;
constexpr int kBlock = 32;
constexpr int kMaxUni = 16;
constexpr int kCombSize = 2048;
constexpr int kQueueSize = 1024;
constexpr int kScopeSize = 2048;

// mod destinations (match Labels.modDst) and sources (match Labels.modSrc)
enum Dst { D_NONE, D_A_POS, D_B_POS, D_A_WARP, D_B_WARP, D_A_LEVEL, D_B_LEVEL, D_A_PITCH, D_B_PITCH,
           D_A_DET, D_B_DET, D_CUTOFF, D_RES, D_DRIVE, D_AMP, D_SUB, D_NOISE, D_A_PAN, D_B_PAN, D_PITCH_ALL, D_COUNT };
enum Src { S_NONE, S_ENV2, S_ENV3, S_LFO1, S_LFO2, S_LFO3, S_LFO4, S_VEL, S_KEY, S_MOD, S_AT, S_BEND,
           S_MAC1, S_MAC2, S_MAC3, S_MAC4, S_RAND, S_COUNT };

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float fastTanh(float x) {
    if (x > 3.f) return 1.f;
    if (x < -3.f) return -1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}
inline float timeFromNorm(float x) { return 0.001f * std::pow(10000.f, x); }
inline float noteToFreq(float n) { return 440.f * std::exp2((n - 69.f) / 12.f); }
inline float fract(float x) { return x - std::floor(x); }

inline float beatsForDiv(int d) {
    static const float b[] = {4.f, 2.f, 1.f, 0.5f, 0.25f, 0.125f, 8.f, 1.5f, 0.75f, 1.f / 3.f, 2.f / 3.f};
    return b[clampi(d, 0, 10)];
}

struct XorShift {
    uint32_t s = 2463534242u;
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni() { return (next() >> 8) * (1.0f / 16777216.0f); }       // 0..1
    inline float bi() { return uni() * 2.f - 1.f; }                            // -1..1
};

// ------------------------------------------------------------------ envelope
struct Env {
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
    float level = 0.f;
    void gateOn() { stage = Attack; }
    void gateOff() { if (stage != Idle) stage = Release; }
    // dt in seconds; a/d/r in seconds
    void tick(float dt, float a, float d, float s, float r) {
        switch (stage) {
        case Attack:
            level += dt / std::max(a, 0.0005f);
            if (level >= 1.f) { level = 1.f; stage = Decay; }
            break;
        case Decay: {
            float c = std::exp(-4.6f * dt / std::max(d, 0.001f));
            level = s + (level - s) * c;
            if (std::fabs(level - s) < 1e-4f) { level = s; stage = Sustain; }
            break;
        }
        case Sustain: level += (s - level) * 0.25f; break;
        case Release: level *= std::exp(-4.6f * dt / std::max(r, 0.001f)); break;
        default: break;
        }
    }
};

// ------------------------------------------------------------------ LFO
struct Lfo {
    float phase = 0.f, prev = 0.f, cur = 0.f, value = 0.f;
    void tick(int shape, float inc, XorShift& rng) {
        phase += inc;
        bool wrapped = false;
        while (phase >= 1.f) { phase -= 1.f; wrapped = true; }
        if (wrapped) { prev = cur; cur = rng.bi(); }
        float p = phase;
        switch (shape) {
        case 0: value = std::sin(2.f * kPi * p); break;
        case 1: value = p < 0.5f ? 4.f * p - 1.f : 3.f - 4.f * p; break;
        case 2: value = 2.f * p - 1.f; break;
        case 3: value = 1.f - 2.f * p; break;
        case 4: value = p < 0.5f ? 1.f : -1.f; break;
        case 5: value = cur; break;
        case 6: { float t = p * p * (3.f - 2.f * p); value = prev + (cur - prev) * t; break; }
        default: { float q = 1.f - p; value = 2.f * q * q - 1.f; break; }
        }
    }
};

// ------------------------------------------------------------------ filter
struct SvfCoef { float g, k, a1, a2, a3; };
inline SvfCoef makeSvf(float fc, float k, float sr) {
    fc = clampf(fc, 10.f, sr * 0.45f);
    SvfCoef c;
    c.g = std::tan(kPi * fc / sr);
    c.k = k;
    c.a1 = 1.f / (1.f + c.g * (c.g + k));
    c.a2 = c.g * c.a1;
    c.a3 = c.g * c.a2;
    return c;
}
// mode: 0 LP, 1 HP, 2 BP, 3 Notch, 4 Peak
inline float svfTick(float x, float& s1, float& s2, const SvfCoef& c, int mode) {
    float v3 = x - s2;
    float v1 = c.a1 * s1 + c.a2 * v3;
    float v2 = s2 + c.a2 * s1 + c.a3 * v3;
    s1 = 2.f * v1 - s1;
    s2 = 2.f * v2 - s2;
    switch (mode) {
    case 0: return v2;
    case 1: return x - c.k * v1 - v2;
    case 2: return v1 * c.k;
    case 3: return x - c.k * v1;
    default: return v2 - (x - c.k * v1 - v2);
    }
}

inline float polyblep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

// ------------------------------------------------------------------ voice
struct Voice {
    bool active = false, gate = false, keyDown = false;
    int note = 60;
    float vel = 1.f, curNote = 60.f, targetNote = 60.f, rndSrc = 0.f;
    uint64_t age = 0;
    float ph[2][kMaxUni];
    float subPh = 0.f;
    float pink[7] = {0, 0, 0, 0, 0, 0, 0};
    Env env[3];
    Lfo lfo[4];
    float s1[2][2] = {{0, 0}, {0, 0}}, s2[2][2] = {{0, 0}, {0, 0}};  // [channel][stage]
    float comb[2][kCombSize];
    int combPos = 0;
    float lastGain = 0.f;
    XorShift rng;
};

// ------------------------------------------------------------------ FX
struct Delay {
    std::vector<float> l, r;
    int w = 0, size = 0;
    float lpL = 0, lpR = 0, curTime = 0.25f;
    void init(int n) { size = n; l.assign(n, 0.f); r.assign(n, 0.f); w = 0; }
    void clear() { std::fill(l.begin(), l.end(), 0.f); std::fill(r.begin(), r.end(), 0.f); lpL = lpR = 0; }
    inline float read(const std::vector<float>& b, float d) const {
        float rp = (float)w - d;
        while (rp < 0) rp += (float)size;
        int i0 = (int)rp; float fr = rp - (float)i0;
        int i1 = i0 + 1; if (i1 >= size) i1 -= size;
        if (i0 >= size) i0 -= size;
        return b[i0] + (b[i1] - b[i0]) * fr;
    }
};

struct Comb {
    std::vector<float> buf; int pos = 0; float store = 0.f;
    void init(int n) { buf.assign(std::max(n, 1), 0.f); pos = 0; store = 0.f; }
    inline float tick(float in, float fb, float damp) {
        float out = buf[pos];
        store = out * (1.f - damp) + store * damp;
        buf[pos] = in + store * fb;
        if (++pos >= (int)buf.size()) pos = 0;
        return out;
    }
};
struct Allpass {
    std::vector<float> buf; int pos = 0;
    void init(int n) { buf.assign(std::max(n, 1), 0.f); pos = 0; }
    inline float tick(float in) {
        float b = buf[pos];
        float out = -in + b;
        buf[pos] = in + b * 0.5f;
        if (++pos >= (int)buf.size()) pos = 0;
        return out;
    }
};

struct MidiEvent { uint8_t status, d1, d2, pad; };

}  // namespace

// =====================================================================================
struct Engine::Impl {
    float sr;
    std::atomic<float> target[P_COUNT];
    float cur[P_COUNT];

    Voice* voices;  // heap, kMaxVoices
    uint64_t ageCounter = 0;
    Lfo gLfo[4];
    XorShift rng;

    // midi queue (multi producer, single consumer)
    MidiEvent queue[kQueueSize];
    std::atomic<uint32_t> qHead{0}, qTail{0};
    std::atomic_flag qLock = ATOMIC_FLAG_INIT;

    // performance state
    float bend = 0.f, modWheel = 0.f, aftertouch = 0.f;
    bool sustain = false;
    struct Held { int note; int vel; };
    std::vector<Held> noteStack;  // mono mode
    float lastNote = 60.f;

    // arpeggiator
    std::vector<Held> arpHeld;
    int arpStep = 0, arpCur = -1;
    double arpTimer = 0.0, arpGateTimer = 0.0;
    bool arpWasOn = false;

    // FX state
    float distLast[2] = {0, 0};
    std::vector<float> chL, chR;
    int chW = 0;
    float chPhase = 0.f;
    Delay dly;
    bool dlyWas = false, rvWas = false, chWas = false;
    Comb combL[8], combR[8];
    Allpass apL[4], apR[4];

    // monitoring
    float scope[kScopeSize];
    std::atomic<uint32_t> scopePos{0};
    std::atomic<float> peak{0.f};
    std::atomic<int> nVoices{0};

    explicit Impl(float sampleRate) : sr(sampleRate) {
        for (int i = 0; i < P_COUNT; i++) { target[i].store(kParams[i].def); cur[i] = kParams[i].def; }
        voices = new Voice[kMaxVoices];
        std::memset(scope, 0, sizeof(scope));
        for (int v = 0; v < kMaxVoices; v++) { std::memset(voices[v].comb, 0, sizeof(voices[v].comb)); voices[v].rng.s = 12345u + 777u * v; }
        for (int i = 0; i < 4; i++) { gLfo[i].cur = rng.bi(); gLfo[i].prev = rng.bi(); }
        chL.assign((int)(sr * 0.06f) + 8, 0.f); chR = chL;
        dly.init((int)(sr * 2.2f));
        static const int ct[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        static const int at[4] = {556, 441, 341, 225};
        float sc = sr / 44100.f;
        for (int i = 0; i < 8; i++) { combL[i].init((int)(ct[i] * sc)); combR[i].init((int)((ct[i] + 23) * sc)); }
        for (int i = 0; i < 4; i++) { apL[i].init((int)(at[i] * sc)); apR[i].init((int)((at[i] + 23) * sc)); }
        WavetableBank::get().init();
    }
    ~Impl() { delete[] voices; }

    // ------------------------------------------------------------- events
    void push(MidiEvent e) {
        while (qLock.test_and_set(std::memory_order_acquire)) {}
        uint32_t h = qHead.load(std::memory_order_relaxed);
        if (h - qTail.load(std::memory_order_acquire) < (uint32_t)kQueueSize) {
            queue[h % kQueueSize] = e;
            qHead.store(h + 1, std::memory_order_release);
        }
        qLock.clear(std::memory_order_release);
    }
    void drain() {
        uint32_t t = qTail.load(std::memory_order_relaxed);
        uint32_t h = qHead.load(std::memory_order_acquire);
        while (t != h) {
            handle(queue[t % kQueueSize]);
            t++;
            qTail.store(t, std::memory_order_release);
        }
    }
    void setT(int id, float v) { target[id].store(v, std::memory_order_relaxed); }

    void handle(const MidiEvent& e) {
        int cmd = e.status & 0xF0;
        switch (cmd) {
        case 0x90:
            if (e.d2 == 0) onNoteOff(e.d1); else onNoteOn(e.d1, e.d2);
            break;
        case 0x80: onNoteOff(e.d1); break;
        case 0xE0: bend = ((e.d1 | (e.d2 << 7)) - 8192) / 8192.f; break;
        case 0xD0: aftertouch = e.d1 / 127.f; break;
        case 0xA0: aftertouch = e.d2 / 127.f; break;
        case 0xB0: {
            float v = e.d2 / 127.f;
            switch (e.d1) {
            case 1: modWheel = v; break;
            case 64: {
                bool on = e.d2 >= 64;
                if (sustain && !on) {
                    sustain = false;
                    for (int i = 0; i < kMaxVoices; i++)
                        if (voices[i].active && !voices[i].keyDown && voices[i].gate) releaseVoice(voices[i]);
                } else sustain = on;
                break;
            }
            case 20: setT(P_MACRO1, v); break;
            case 21: setT(P_MACRO2, v); break;
            case 22: setT(P_MACRO3, v); break;
            case 23: setT(P_MACRO4, v); break;
            case 71: setT(P_F_RES, v); break;
            case 74: setT(P_F_CUTOFF, v); break;
            case 120: case 123: panic(); break;
            default: break;
            }
            break;
        }
        default: break;
        }
    }

    void panic() {
        noteStack.clear(); arpHeld.clear(); arpCur = -1; sustain = false;
        for (int i = 0; i < kMaxVoices; i++) if (voices[i].active) { voices[i].keyDown = false; releaseVoice(voices[i]); }
    }

    // ------------------------------------------------------------- note handling
    void onNoteOn(int note, int vel) {
        if (cur[P_ARP_ON] > 0.5f) {
            for (size_t i = 0; i < arpHeld.size(); i++) if (arpHeld[i].note == note) { arpHeld.erase(arpHeld.begin() + i); break; }
            arpHeld.push_back({note, vel});
            return;
        }
        voiceNoteOn(note, vel);
    }
    void onNoteOff(int note) {
        if (cur[P_ARP_ON] > 0.5f) {
            for (size_t i = 0; i < arpHeld.size(); i++) if (arpHeld[i].note == note) { arpHeld.erase(arpHeld.begin() + i); break; }
            return;
        }
        voiceNoteOff(note);
    }

    void releaseVoice(Voice& v) {
        v.gate = false;
        for (int e = 0; e < 3; e++) v.env[e].gateOff();
    }

    void startVoice(Voice& v, int note, int vel, bool retrigger) {
        const bool wasActive = v.active;
        v.note = note;
        v.targetNote = (float)note;
        v.vel = vel / 127.f;
        float glide = cur[P_GLIDE];
        if (!wasActive) v.curNote = glide > 0.001f ? lastNote : (float)note;
        else if (glide < 0.001f) v.curNote = (float)note;
        lastNote = (float)note;
        v.age = ++ageCounter;
        v.keyDown = true;
        v.gate = true;
        if (!retrigger) return;
        v.rndSrc = rng.uni();
        for (int o = 0; o < 2; o++) {
            const float* p = &cur[P_A_ON + o * O_STRIDE];
            float base = p[O_PHASE], rnd = p[O_RAND];
            for (int u = 0; u < kMaxUni; u++) v.ph[o][u] = fract(base + rnd * rng.uni());
        }
        if (!wasActive) {
            v.subPh = 0.f;
            for (int e = 0; e < 3; e++) { v.env[e].level = 0.f; v.env[e].stage = Env::Idle; }
            std::memset(v.s1, 0, sizeof(v.s1)); std::memset(v.s2, 0, sizeof(v.s2));
            std::memset(v.comb, 0, sizeof(v.comb));
            v.lastGain = 0.f;
        }
        for (int e = 0; e < 3; e++) v.env[e].gateOn();
        for (int i = 0; i < 4; i++) {
            const float* lp = &cur[P_L1_SHAPE + 6 * i];
            if (lp[4] > 0.5f) { v.lfo[i].phase = lp[5]; v.lfo[i].cur = v.rng.bi(); v.lfo[i].prev = v.rng.bi(); }
        }
        v.active = true;
    }

    int allocVoice() {
        int limit = clampi((int)(cur[P_VOICES] + 0.5f), 1, kMaxVoices);
        for (int i = 0; i < limit; i++) if (!voices[i].active) return i;
        int best = 0; float bestScore = 1e9f;
        for (int i = 0; i < limit; i++) {
            float score = voices[i].gate ? 10.f + (float)voices[i].age * 1e-9f : voices[i].env[0].level;
            if (score < bestScore) { bestScore = score; best = i; }
        }
        return best;
    }

    void voiceNoteOn(int note, int vel) {
        if (cur[P_MONO] > 0.5f) {
            for (size_t i = 0; i < noteStack.size(); i++) if (noteStack[i].note == note) { noteStack.erase(noteStack.begin() + i); break; }
            noteStack.push_back({note, vel});
            Voice& v = voices[0];
            bool legato = cur[P_LEGATO] > 0.5f && v.active && v.gate;
            if (legato) { v.targetNote = (float)note; v.note = note; v.keyDown = true; lastNote = (float)note; }
            else startVoice(v, note, vel, true);
            for (int i = 1; i < kMaxVoices; i++) if (voices[i].active && voices[i].gate) { voices[i].keyDown = false; releaseVoice(voices[i]); }
            return;
        }
        // same note already playing -> retrigger that voice
        int idx = -1;
        for (int i = 0; i < kMaxVoices; i++) if (voices[i].active && voices[i].note == note && voices[i].gate) { idx = i; break; }
        if (idx < 0) idx = allocVoice();
        startVoice(voices[idx], note, vel, true);
    }

    void voiceNoteOff(int note) {
        if (cur[P_MONO] > 0.5f) {
            for (size_t i = 0; i < noteStack.size(); i++) if (noteStack[i].note == note) { noteStack.erase(noteStack.begin() + i); break; }
            Voice& v = voices[0];
            if (!noteStack.empty()) {
                Held h = noteStack.back();
                if (cur[P_LEGATO] > 0.5f) { v.targetNote = (float)h.note; v.note = h.note; }
                else startVoice(v, h.note, h.vel, true);
            } else {
                v.keyDown = false;
                if (!sustain) releaseVoice(v);
            }
            return;
        }
        for (int i = 0; i < kMaxVoices; i++) {
            Voice& v = voices[i];
            if (v.active && v.note == note && v.keyDown) {
                v.keyDown = false;
                if (!sustain) releaseVoice(v);
            }
        }
    }

    // ------------------------------------------------------------- arpeggiator
    void arpTick(int n) {
        bool on = cur[P_ARP_ON] > 0.5f;
        if (!on) {
            if (arpWasOn) { if (arpCur >= 0) voiceNoteOff(arpCur); arpCur = -1; arpHeld.clear(); arpStep = 0; arpTimer = 0; }
            arpWasOn = false;
            return;
        }
        arpWasOn = true;
        if (arpHeld.empty()) {
            if (arpCur >= 0) { voiceNoteOff(arpCur); arpCur = -1; }
            arpTimer = 0; arpStep = 0;
            return;
        }
        static const float beats[6] = {1.f, 0.5f, 0.25f, 0.125f, 1.f / 3.f, 1.f / 6.f};
        double stepLen = (double)sr * 60.0 / std::max(cur[P_BPM], 20.f) * beats[clampi((int)(cur[P_ARP_RATE] + 0.5f), 0, 5)];
        if (arpCur >= 0) {
            arpGateTimer -= n;
            if (arpGateTimer <= 0) { voiceNoteOff(arpCur); arpCur = -1; }
        }
        arpTimer -= n;
        if (arpTimer <= 0) {
            arpTimer += stepLen;
            if (arpTimer <= 0) arpTimer = stepLen;
            if (arpCur >= 0) { voiceNoteOff(arpCur); arpCur = -1; }
            // build sequence
            std::vector<Held> base = arpHeld;
            int pat = clampi((int)(cur[P_ARP_PAT] + 0.5f), 0, 4);
            if (pat != 4) std::sort(base.begin(), base.end(), [](const Held& a, const Held& b) { return a.note < b.note; });
            int octs = clampi((int)(cur[P_ARP_OCT] + 0.5f), 1, 4);
            std::vector<Held> seq;
            for (int o = 0; o < octs; o++) for (auto& h : base) seq.push_back({h.note + 12 * o, h.vel});
            int len = (int)seq.size();
            int idx = 0;
            switch (pat) {
            case 0: case 4: idx = arpStep % len; break;
            case 1: idx = len - 1 - (arpStep % len); break;
            case 2: { int per = std::max(1, 2 * len - 2); int k = arpStep % per; idx = k < len ? k : per - k; break; }
            default: idx = (int)(rng.uni() * len) % len; break;
            }
            arpStep++;
            Held h = seq[clampi(idx, 0, len - 1)];
            int note = clampi(h.note, 0, 127);
            voiceNoteOn(note, h.vel);
            arpCur = note;
            arpGateTimer = stepLen * clampf(cur[P_ARP_GATE], 0.05f, 1.f);
        }
    }

    // ------------------------------------------------------------- params
    void smoothParams() {
        for (int i = 0; i < P_COUNT; i++) {
            float t = target[i].load(std::memory_order_relaxed);
            if (kParams[i].kind == 'f') cur[i] += (t - cur[i]) * 0.3f;
            else cur[i] = t;
        }
    }

    // ------------------------------------------------------------- voice render
    inline float warpPhase(int mode, float p, float w) const {
        switch (mode) {
        case 1: { float q = p * (1.f + w * 7.f); return q - std::floor(q); }
        case 2: return p + (p * p * p - p) * w;
        case 3: { float q = 1.f - p; return p + ((1.f - q * q * q) - p) * w; }
        case 4: { float t = p < 0.5f ? 2.f * p : 2.f - 2.f * p; return p + (t - p) * w; }
        case 5: { float c = 0.5f + w * 0.45f; return p < c ? p * 0.5f / c : 0.5f + (p - c) * 0.5f / (1.f - c); }
        case 6: { float q = p + w * 0.25f * std::sin(2.f * kPi * p); return q - std::floor(q); }
        default: return p;
        }
    }

    // renders one oscillator into outL/outR (overwrite)
    void renderOsc(Voice& v, int o, const float* mod, float bendSemi, int n, float* outL, float* outR) {
        const float* p = &cur[P_A_ON + o * O_STRIDE];
        const int dPos = o ? D_B_POS : D_A_POS, dWarp = o ? D_B_WARP : D_A_WARP, dLevel = o ? D_B_LEVEL : D_A_LEVEL;
        const int dPitch = o ? D_B_PITCH : D_A_PITCH, dDet = o ? D_B_DET : D_A_DET, dPan = o ? D_B_PAN : D_A_PAN;
        const WavetableBank& wb = WavetableBank::get();
        int table = clampi((int)(p[O_TABLE] + 0.5f), 0, kNumTables - 1);
        float pos = clampf(p[O_POS] + mod[dPos], 0.f, 1.f);
        float warp = clampf(p[O_WARP] + mod[dWarp], 0.f, 1.f);
        int wmode = clampi((int)(p[O_WMODE] + 0.5f), 0, 7);
        float semi = p[O_OCT] * 12.f + p[O_SEMI] + p[O_FINE] + mod[dPitch] * 24.f + mod[D_PITCH_ALL] * 24.f + bendSemi;
        float freq = noteToFreq(v.curNote + semi);
        int N = clampi((int)(p[O_UNI] + 0.5f), 1, kMaxUni);
        float det = clampf(p[O_DET] + mod[dDet], 0.f, 1.f);
        float detMax = 1.5f * det * std::sqrt(det);
        float blend = p[O_BLEND], spread = p[O_SPREAD];
        float level = clampf(p[O_LEVEL] + mod[dLevel], 0.f, 1.5f);
        float pan = clampf(p[O_PAN] + mod[dPan], -1.f, 1.f);

        float inc[kMaxUni], gl[kMaxUni], gr[kMaxUni], w[kMaxUni];
        float w2 = 0.f, maxInc = 0.f;
        int centre = N / 2;
        for (int u = 0; u < N; u++) {
            float pu = N > 1 ? 2.f * u / (N - 1) - 1.f : 0.f;
            inc[u] = std::min(0.45f, freq * std::exp2(pu * detMax / 12.f) / sr);
            maxInc = std::max(maxInc, inc[u]);
            w[u] = (N == 1 || u == centre) ? 1.f : blend;
            w2 += w[u] * w[u];
            float pp = clampf(pan + pu * spread, -1.f, 1.f);
            float ang = (pp + 1.f) * kPi * 0.25f;
            gl[u] = std::cos(ang); gr[u] = std::sin(ang);
        }
        float norm = level / std::sqrt(std::max(w2, 1e-6f));
        for (int u = 0; u < N; u++) { w[u] *= norm; gl[u] *= w[u]; gr[u] *= w[u]; }
        int mip = WavetableBank::mipFor(maxInc * (wmode == 1 ? 1.f + warp * 7.f : 1.f));
        float qlev = 0.f;
        if (wmode == 7) { float k = 1.f - warp; qlev = (3.f + k * k * 253.f) * 0.5f; }

        for (int i = 0; i < n; i++) { outL[i] = 0.f; outR[i] = 0.f; }
        for (int u = 0; u < N; u++) {
            float ph = v.ph[o][u], incu = inc[u], a = gl[u], b = gr[u];
            for (int i = 0; i < n; i++) {
                ph += incu;
                if (ph >= 1.f) ph -= 1.f;
                float wp = warpPhase(wmode, ph, warp);
                if (wp >= 1.f) wp -= 1.f;
                if (wp < 0.f) wp = 0.f;
                float s = wb.read(table, mip, pos, wp);
                if (wmode == 7) s = std::floor(s * qlev + 0.5f) / qlev;
                outL[i] += s * a;
                outR[i] += s * b;
            }
            v.ph[o][u] = ph;
        }
    }

    // returns false when the voice has finished
    bool renderVoice(Voice& v, int n, float gLfoVal[4], float* busL, float* busR) {
        const float dt = (float)n / sr;
        // glide
        float gl = cur[P_GLIDE];
        if (gl < 0.001f) v.curNote = v.targetNote;
        else { float tau = 0.003f + gl * gl * 1.5f; v.curNote += (v.targetNote - v.curNote) * (1.f - std::exp(-dt / tau)); }

        // envelopes
        for (int e = 0; e < 3; e++) {
            const float* ep = &cur[P_E1_A + 4 * e];
            v.env[e].tick(dt, timeFromNorm(ep[0]), timeFromNorm(ep[1]), ep[2], timeFromNorm(ep[3]));
        }
        {
            const Env& a = v.env[0];
            float s = cur[P_E1_S];
            if (a.level < 1e-4f && (a.stage == Env::Release || a.stage == Env::Idle ||
                                    ((a.stage == Env::Decay || a.stage == Env::Sustain) && s < 1e-4f))) {
                v.active = false; return false;
            }
        }
        // LFOs
        float lfoVal[4];
        for (int i = 0; i < 4; i++) {
            const float* lp = &cur[P_L1_SHAPE + 6 * i];
            if (lp[4] > 0.5f) {
                float hz = lp[2] > 0.5f ? cur[P_BPM] / 60.f / beatsForDiv((int)(lp[3] + 0.5f)) : 0.05f * std::pow(600.f, lp[1]);
                v.lfo[i].tick((int)(lp[0] + 0.5f), hz * dt, v.rng);
                lfoVal[i] = v.lfo[i].value;
            } else lfoVal[i] = gLfoVal[i];
        }
        // mod sources / matrix
        float src[S_COUNT];
        src[S_NONE] = 0.f; src[S_ENV2] = v.env[1].level; src[S_ENV3] = v.env[2].level;
        for (int i = 0; i < 4; i++) src[S_LFO1 + i] = lfoVal[i];
        src[S_VEL] = v.vel;
        src[S_KEY] = clampf((v.curNote - 60.f) / 36.f, -1.f, 1.f);
        src[S_MOD] = modWheel; src[S_AT] = aftertouch; src[S_BEND] = bend;
        src[S_MAC1] = cur[P_MACRO1]; src[S_MAC2] = cur[P_MACRO2]; src[S_MAC3] = cur[P_MACRO3]; src[S_MAC4] = cur[P_MACRO4];
        src[S_RAND] = v.rndSrc;
        float mod[D_COUNT];
        for (int i = 0; i < D_COUNT; i++) mod[i] = 0.f;
        for (int i = 0; i < 8; i++) {
            const float* mp = &cur[P_M1_SRC + 3 * i];
            int s = (int)(mp[0] + 0.5f), d = (int)(mp[1] + 0.5f);
            if (s > 0 && s < S_COUNT && d > 0 && d < D_COUNT) mod[d] += src[s] * mp[2];
        }
        float bendSemi = bend * cur[P_BEND];

        float oscL[2][kBlock], oscR[2][kBlock];
        float fL[kBlock], fR[kBlock], dL[kBlock], dR[kBlock];
        for (int i = 0; i < n; i++) fL[i] = fR[i] = dL[i] = dR[i] = 0.f;
        const bool filtOn = cur[P_F_ON] > 0.5f;
        const float* route = &cur[P_F_RA];

        for (int o = 0; o < 2; o++) {
            if (cur[P_A_ON + o * O_STRIDE] < 0.5f) continue;
            renderOsc(v, o, mod, bendSemi, n, oscL[o], oscR[o]);
            float *tl = (filtOn && route[o] > 0.5f) ? fL : dL, *tr = (filtOn && route[o] > 0.5f) ? fR : dR;
            for (int i = 0; i < n; i++) { tl[i] += oscL[o][i]; tr[i] += oscR[o][i]; }
        }
        // sub oscillator
        if (cur[P_SUB_ON] > 0.5f) {
            float semi = cur[P_SUB_OCT] * 12.f + mod[D_PITCH_ALL] * 24.f + bendSemi;
            float f = noteToFreq(v.curNote + semi);
            float inc = std::min(0.45f, f / sr);
            float lev = clampf(cur[P_SUB_LEVEL] + mod[D_SUB], 0.f, 1.f) * 0.7f;
            int shape = clampi((int)(cur[P_SUB_SHAPE] + 0.5f), 0, 3);
            float *tl = (filtOn && route[2] > 0.5f) ? fL : dL, *tr = (filtOn && route[2] > 0.5f) ? fR : dR;
            for (int i = 0; i < n; i++) {
                v.subPh += inc; if (v.subPh >= 1.f) v.subPh -= 1.f;
                float ph = v.subPh, s;
                switch (shape) {
                case 0: s = std::sin(2.f * kPi * ph); break;
                case 1: s = ph < 0.5f ? 4.f * ph - 1.f : 3.f - 4.f * ph; break;
                case 2: s = (ph < 0.5f ? 1.f : -1.f) + polyblep(ph, inc) - polyblep(fract(ph + 0.5f), inc); break;
                default: s = 2.f * ph - 1.f - polyblep(ph, inc); break;
                }
                tl[i] += s * lev; tr[i] += s * lev;
            }
        }
        // noise
        if (cur[P_NOISE_ON] > 0.5f) {
            float lev = clampf(cur[P_NOISE_LEVEL] + mod[D_NOISE], 0.f, 1.f) * 0.5f;
            bool pinkN = cur[P_NOISE_TYPE] > 0.5f;
            float *tl = (filtOn && route[3] > 0.5f) ? fL : dL, *tr = (filtOn && route[3] > 0.5f) ? fR : dR;
            for (int i = 0; i < n; i++) {
                float white = v.rng.bi(), s = white;
                if (pinkN) {
                    float* b = v.pink;
                    b[0] = 0.99886f * b[0] + white * 0.0555179f; b[1] = 0.99332f * b[1] + white * 0.0750759f;
                    b[2] = 0.96900f * b[2] + white * 0.1538520f; b[3] = 0.86650f * b[3] + white * 0.3104856f;
                    b[4] = 0.55000f * b[4] + white * 0.5329522f; b[5] = -0.7616f * b[5] - white * 0.0168980f;
                    s = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + white * 0.5362f) * 0.11f;
                    b[6] = white * 0.115926f;
                }
                float r2 = v.rng.bi();
                tl[i] += s * lev; tr[i] += (pinkN ? s : r2) * lev;
            }
        }
        // filter
        if (filtOn) {
            int type = clampi((int)(cur[P_F_TYPE] + 0.5f), 0, 7);
            float norm = cur[P_F_CUTOFF] + mod[D_CUTOFF] + cur[P_F_KEY] * (v.curNote - 60.f) / (12.f * 9.97f);
            norm = clampf(norm, 0.f, 1.f);
            float fc = 20.f * std::exp(6.9078f * norm);
            float res = clampf(cur[P_F_RES] + mod[D_RES], 0.f, 1.f);
            float k = 1.7f - 1.65f * std::pow(res, 0.6f);
            float drive = clampf(cur[P_F_DRIVE] + mod[D_DRIVE], 0.f, 1.f);
            float dr = 1.f + drive * 8.f, comp = 1.f / (1.f + drive * 1.5f);
            float mix = cur[P_F_MIX];
            SvfCoef c1 = makeSvf(fc, k, sr), c2 = makeSvf(fc, 1.4f, sr);
            float combD = clampf(sr / std::max(fc, 20.f), 2.f, (float)kCombSize - 2.f);
            float combFb = res * 0.95f;
            for (int ch = 0; ch < 2; ch++) {
                float* f = ch ? fR : fL;
                const float* d = ch ? dR : dL;  // unfiltered routing stays dry
                (void)d;
                for (int i = 0; i < n; i++) {
                    float x = f[i], dry = x;
                    if (drive > 0.001f) x = fastTanh(x * dr) * comp;
                    float y;
                    switch (type) {
                    case 0: y = svfTick(x, v.s1[ch][0], v.s2[ch][0], c1, 0); break;
                    case 1: y = svfTick(svfTick(x, v.s1[ch][0], v.s2[ch][0], c1, 0), v.s1[ch][1], v.s2[ch][1], c2, 0); break;
                    case 2: y = svfTick(x, v.s1[ch][0], v.s2[ch][0], c1, 1); break;
                    case 3: y = svfTick(svfTick(x, v.s1[ch][0], v.s2[ch][0], c1, 1), v.s1[ch][1], v.s2[ch][1], c2, 1); break;
                    case 4: y = svfTick(x, v.s1[ch][0], v.s2[ch][0], c1, 2); break;
                    case 5: y = svfTick(x, v.s1[ch][0], v.s2[ch][0], c1, 3); break;
                    case 6: y = svfTick(x, v.s1[ch][0], v.s2[ch][0], c1, 4); break;
                    default: {
                        int wp = v.combPos + i;
                        float rp = (float)wp - combD;
                        int i0 = (int)std::floor(rp); float fr = rp - (float)i0;
                        float a = v.comb[ch][((i0 % kCombSize) + kCombSize) % kCombSize];
                        float b = v.comb[ch][(((i0 + 1) % kCombSize) + kCombSize) % kCombSize];
                        float delayed = a + (b - a) * fr;
                        y = x + combFb * delayed;
                        v.comb[ch][wp % kCombSize] = y;
                        y *= (1.f - 0.7f * combFb);
                        break;
                    }
                    }
                    f[i] = dry + (y - dry) * mix;
                }
            }
            v.combPos = (v.combPos + n) % kCombSize;
        }
        // amplitude
        float vg = 0.35f + 0.65f * v.vel;
        float ampMod = clampf(1.f + mod[D_AMP], 0.f, 2.f);
        float g1 = v.env[0].level * vg * ampMod, g0 = v.lastGain;
        if (!v.active) return false;
        for (int i = 0; i < n; i++) {
            float g = g0 + (g1 - g0) * ((float)(i + 1) / (float)n);
            busL[i] += (fL[i] + dL[i]) * g;
            busR[i] += (fR[i] + dR[i]) * g;
        }
        v.lastGain = g1;
        return true;
    }

    // ------------------------------------------------------------- effects
    void processFx(float* L, float* R, int n) {
        // distortion
        if (cur[P_DIST_ON] > 0.5f) {
            int type = clampi((int)(cur[P_DIST_TYPE] + 0.5f), 0, 3);
            float amt = cur[P_DIST_AMT], mix = cur[P_DIST_MIX];
            float drive = 1.f + amt * 19.f, comp = 1.f / (1.f + amt * 1.5f);
            float lv = 3.f + std::pow(256.f, 1.f - amt);
            for (int i = 0; i < n; i++) {
                for (int ch = 0; ch < 2; ch++) {
                    float x = ch ? R[i] : L[i], y;
                    switch (type) {
                    case 0: y = fastTanh(x * drive) * comp * 1.2f; break;
                    case 1: y = clampf(x * drive, -1.f, 1.f) * comp; break;
                    case 2: y = std::sin(x * drive * 1.4f) * comp * 1.2f; break;
                    default: y = std::floor(x * lv * 0.5f + 0.5f) / (lv * 0.5f) * 0.9f; break;
                    }
                    y = x + (y - x) * mix;
                    if (ch) R[i] = y; else L[i] = y;
                }
            }
        }
        // chorus
        if (cur[P_CH_ON] > 0.5f) {
            if (!chWas) { std::fill(chL.begin(), chL.end(), 0.f); std::fill(chR.begin(), chR.end(), 0.f); }
            float rate = 0.1f + cur[P_CH_RATE] * 4.f, depth = (1.f + cur[P_CH_DEPTH] * 8.f) * 0.001f * sr;
            float base = 0.012f * sr, mix = cur[P_CH_MIX];
            int size = (int)chL.size();
            for (int i = 0; i < n; i++) {
                chPhase += rate / sr; if (chPhase >= 1.f) chPhase -= 1.f;
                chL[chW] = L[i]; chR[chW] = R[i];
                float dl = base + depth * std::sin(2.f * kPi * chPhase);
                float dr = base + depth * std::sin(2.f * kPi * (chPhase + 0.25f));
                float wl, wr;
                {
                    float rp = (float)chW - dl; while (rp < 0) rp += (float)size;
                    int i0 = (int)rp, i1 = (i0 + 1) % size; float fr = rp - (float)i0;
                    wl = chL[i0 % size] + (chL[i1] - chL[i0 % size]) * fr;
                    rp = (float)chW - dr; while (rp < 0) rp += (float)size;
                    i0 = (int)rp; i1 = (i0 + 1) % size; fr = rp - (float)i0;
                    wr = chR[i0 % size] + (chR[i1] - chR[i0 % size]) * fr;
                }
                if (++chW >= size) chW = 0;
                L[i] = L[i] * (1.f - 0.5f * mix) + wl * mix;
                R[i] = R[i] * (1.f - 0.5f * mix) + wr * mix;
            }
        }
        chWas = cur[P_CH_ON] > 0.5f;
        // delay (ping-pong)
        if (cur[P_DL_ON] > 0.5f) {
            if (!dlyWas) dly.clear();
            float tsec = cur[P_DL_SYNC] > 0.5f ? 60.f / std::max(cur[P_BPM], 20.f) * beatsForDiv((int)(cur[P_DL_DIV] + 0.5f))
                                              : 0.02f + cur[P_DL_TIME] * cur[P_DL_TIME] * 1.5f;
            float tgt = clampf(tsec * sr, 16.f, (float)dly.size - 4.f);
            float fb = cur[P_DL_FB], mix = cur[P_DL_MIX], lpA = 0.08f + cur[P_DL_TONE] * 0.9f;
            for (int i = 0; i < n; i++) {
                dly.curTime += (tgt - dly.curTime) * 0.0008f;
                float dL = dly.read(dly.l, dly.curTime), dR = dly.read(dly.r, dly.curTime);
                dly.lpL += lpA * (dL - dly.lpL); dly.lpR += lpA * (dR - dly.lpR);
                dly.l[dly.w] = L[i] + fb * dly.lpR;
                dly.r[dly.w] = R[i] + fb * dly.lpL;
                if (++dly.w >= dly.size) dly.w = 0;
                L[i] = L[i] * (1.f - 0.3f * mix) + dL * mix;
                R[i] = R[i] * (1.f - 0.3f * mix) + dR * mix;
            }
        }
        dlyWas = cur[P_DL_ON] > 0.5f;
        // reverb (freeverb)
        if (cur[P_RV_ON] > 0.5f) {
            if (!rvWas) {
                for (int i = 0; i < 8; i++) { combL[i].init((int)combL[i].buf.size()); combR[i].init((int)combR[i].buf.size()); }
                for (int i = 0; i < 4; i++) { apL[i].init((int)apL[i].buf.size()); apR[i].init((int)apR[i].buf.size()); }
            }
            float fb = 0.7f + cur[P_RV_SIZE] * 0.28f, damp = cur[P_RV_DAMP] * 0.4f, mix = cur[P_RV_MIX];
            for (int i = 0; i < n; i++) {
                float in = (L[i] + R[i]) * 0.015f, oL = 0.f, oR = 0.f;
                for (int c = 0; c < 8; c++) { oL += combL[c].tick(in, fb, damp); oR += combR[c].tick(in, fb, damp); }
                for (int a = 0; a < 4; a++) { oL = apL[a].tick(oL); oR = apR[a].tick(oR); }
                L[i] = L[i] * (1.f - 0.25f * mix) + oL * mix * 3.f;
                R[i] = R[i] * (1.f - 0.25f * mix) + oR * mix * 3.f;
            }
        }
        rvWas = cur[P_RV_ON] > 0.5f;
    }

    // ------------------------------------------------------------- main render
    void render(float* out, int frames) {
        for (int i = 0; i < P_COUNT; i++) if (kParams[i].kind != 'f') cur[i] = target[i].load(std::memory_order_relaxed);
        drain();
        int done = 0;
        float pk = 0.f;
        while (done < frames) {
            int n = std::min(kBlock, frames - done);
            smoothParams();
            const float dt = (float)n / sr;
            float gl[4];
            for (int i = 0; i < 4; i++) {
                const float* lp = &cur[P_L1_SHAPE + 6 * i];
                float hz = lp[2] > 0.5f ? cur[P_BPM] / 60.f / beatsForDiv((int)(lp[3] + 0.5f)) : 0.05f * std::pow(600.f, lp[1]);
                gLfo[i].tick((int)(lp[0] + 0.5f), hz * dt, rng);
                gl[i] = gLfo[i].value;
            }
            arpTick(n);
            float L[kBlock], R[kBlock];
            for (int i = 0; i < n; i++) L[i] = R[i] = 0.f;
            int count = 0;
            for (int v = 0; v < kMaxVoices; v++) if (voices[v].active) { if (renderVoice(voices[v], n, gl, L, R)) count++; }
            nVoices.store(count, std::memory_order_relaxed);
            processFx(L, R, n);
            float master = cur[P_MASTER];
            uint32_t sp = scopePos.load(std::memory_order_relaxed);
            for (int i = 0; i < n; i++) {
                float l = fastTanh(L[i] * master), r = fastTanh(R[i] * master);
                if (!(l == l)) l = 0.f;
                if (!(r == r)) r = 0.f;
                out[2 * (done + i)] = l; out[2 * (done + i) + 1] = r;
                pk = std::max(pk, std::max(std::fabs(l), std::fabs(r)));
                scope[sp % kScopeSize] = 0.5f * (l + r); sp++;
            }
            scopePos.store(sp, std::memory_order_release);
            done += n;
        }
        float old = peak.load(std::memory_order_relaxed);
        peak.store(std::max(pk, old * 0.9f), std::memory_order_relaxed);
    }
};

// =====================================================================================
Engine::Engine(float sampleRate) : impl_(new Impl(sampleRate)) {}
Engine::~Engine() = default;

void Engine::setParam(int id, float value) {
    if (id < 0 || id >= P_COUNT) return;
    const ParamInfo& pi = kParams[id];
    impl_->target[id].store(clampf(value, pi.lo, pi.hi), std::memory_order_relaxed);
}
float Engine::getParam(int id) const {
    if (id < 0 || id >= P_COUNT) return 0.f;
    return impl_->target[id].load(std::memory_order_relaxed);
}
void Engine::midi(uint8_t status, uint8_t d1, uint8_t d2) { impl_->push({status, d1, d2, 0}); }
void Engine::render(float* out, int frames) { impl_->render(out, frames); }

void Engine::getScope(float* out, int n) const {
    n = std::min(n, kScopeSize);
    uint32_t end = impl_->scopePos.load(std::memory_order_acquire);
    for (int i = 0; i < n; i++) out[i] = impl_->scope[(end - (uint32_t)n + (uint32_t)i) % kScopeSize];
}
float Engine::peakLevel() const { return impl_->peak.load(std::memory_order_relaxed); }
int Engine::activeVoices() const { return impl_->nVoices.load(std::memory_order_relaxed); }

void Engine::tableFrame(int table, float pos, float* out, int n) {
    WavetableBank& wb = WavetableBank::get();
    wb.init();
    table = clampi(table, 0, kNumTables - 1);
    pos = clampf(pos, 0.f, 1.f);
    for (int i = 0; i < n; i++) out[i] = wb.read(table, 0, pos, (float)i / (float)n);
}

}  // namespace sx
