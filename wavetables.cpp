#include "wavetables.h"
#include "params.h"
#include <cmath>
#include <complex>
#include <mutex>
#include <algorithm>

namespace sx {

namespace {
constexpr int N = kTableSize;
constexpr int H = N / 2;  // highest harmonic index + 1
constexpr double kPi = 3.14159265358979323846;

// In-place radix-2 forward FFT (e^{-i...}).
void fft(std::vector<std::complex<double>>& a) {
    const int n = (int)a.size();
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * kPi / len;
        std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (int j = 0; j < len / 2; j++) {
                auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Spectrum of one frame: x[n] = sum_h S[h] sin(2 pi h n/N) + C[h] cos(2 pi h n/N)
struct Spec {
    double S[H];
    double C[H];
    Spec() { for (int i = 0; i < H; i++) S[i] = C[i] = 0.0; }
};

void fromTime(const double* x, Spec& sp) {
    std::vector<std::complex<double>> a(N);
    for (int i = 0; i < N; i++) a[i] = x[i];
    fft(a);
    for (int h = 1; h < H; h++) {
        sp.C[h] = 2.0 * a[h].real() / N;
        sp.S[h] = -2.0 * a[h].imag() / N;
    }
}

// Rebuilds time signal from spectrum with harmonics limited to `cap` (cosine taper on the top 20%).
void toTime(const Spec& sp, int cap, double* out) {
    std::vector<std::complex<double>> a(N, std::complex<double>(0, 0));
    for (int h = 1; h < H; h++) {
        if (h >= cap && cap < H) continue;
        double w = 1.0;
        double start = 0.8 * cap;
        if (h > start) w = 0.5 * (1.0 + std::cos(kPi * (h - start) / (cap - start)));
        double re = sp.C[h] * 0.5 * w, im = -sp.S[h] * 0.5 * w;
        // conj(X) is fed to a forward FFT, then conjugated => real part is the signal
        a[h] = std::complex<double>(re, -im);
        a[N - h] = std::complex<double>(re, im);
    }
    fft(a);
    for (int i = 0; i < N; i++) out[i] = a[i].real();
}

double smoothstep(double t) { t = std::min(1.0, std::max(0.0, t)); return t * t * (3 - 2 * t); }

uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }
double rnd(uint32_t& s) { return ((lcg(s) >> 8) & 0xFFFF) / 32768.0 - 1.0; }

void buildFrame(int table, double x, Spec& sp) {
    switch (table) {
    case 0: {  // Analog morph: sine -> tri -> saw -> square
        double seg = x * 3.0;
        int s0 = std::min(2, (int)seg);
        double f = seg - s0;
        auto shape = [](int which, int h) -> double {
            switch (which) {
            case 0: return h == 1 ? 1.0 : 0.0;                                   // sine
            case 1: return (h & 1) ? ((((h - 1) / 2) & 1) ? -1.0 : 1.0) / (double)(h * h) : 0.0;  // tri
            case 2: return 1.0 / h;                                              // saw
            default: return (h & 1) ? 1.0 / h : 0.0;                             // square
            }
        };
        for (int h = 1; h < H; h++) sp.S[h] = shape(s0, h) * (1 - f) + shape(s0 + 1, h) * f;
        break;
    }
    case 1: {  // PWM pulse: width 0.5 -> 0.04
        double d = 0.5 - 0.46 * x;
        for (int h = 1; h < H; h++) sp.C[h] = std::sin(kPi * h * d) / h;
        break;
    }
    case 2: {  // Harmonic sweep: a bright bump travels up the harmonic series over a soft saw
        double hc = 1.0 + x * 31.0, bw = 0.7 + 0.12 * hc;
        for (int h = 1; h < 256; h++) {
            double g = std::exp(-0.5 * (h - hc) * (h - hc) / (bw * bw));
            sp.S[h] = 0.25 / h + g * (h == 1 ? 1.0 : 1.0);
        }
        break;
    }
    case 3: {  // Vowel formants A E I O U
        static const double F[5][3] = {{800, 1150, 2900}, {400, 1600, 2700}, {270, 2140, 2950},
                                       {450, 800, 2830}, {325, 700, 2700}};
        double seg = x * 4.0;
        int v0 = std::min(3, (int)seg);
        double f = seg - v0;
        double fm[3];
        for (int k = 0; k < 3; k++) fm[k] = F[v0][k] * (1 - f) + F[v0 + 1][k] * f;
        for (int h = 1; h < 120; h++) {
            double hf = 110.0 * h, g = 0.0;
            for (int k = 0; k < 3; k++) {
                double bw = 60.0 + 0.1 * fm[k];
                g += (k == 0 ? 1.0 : (k == 1 ? 0.7 : 0.4)) * std::exp(-0.5 * (hf - fm[k]) * (hf - fm[k]) / (bw * bw));
            }
            sp.S[h] = g / std::pow((double)h, 0.35);
        }
        break;
    }
    case 4: {  // Digital FM
        std::vector<double> t(N);
        double idx = x * 6.0;
        for (int n = 0; n < N; n++) {
            double th = 2 * kPi * n / N;
            t[n] = std::sin(th + idx * std::sin(2 * th));
        }
        fromTime(t.data(), sp);
        break;
    }
    case 5: {  // Spectral morph between four random spectra
        double seg = x * 3.0;
        int s0 = std::min(2, (int)seg);
        double f = smoothstep(seg - s0);
        for (int h = 1; h < 96; h++) {
            double a[2][2];
            for (int w = 0; w < 2; w++) {
                uint32_t st = 7919u * (uint32_t)(s0 + w + 1) + 104729u * (uint32_t)h;
                a[w][0] = rnd(st); a[w][1] = rnd(st);
            }
            double env = 1.0 / std::pow((double)h, 0.8);
            sp.S[h] = env * (a[0][0] * (1 - f) + a[1][0] * f);
            sp.C[h] = env * (a[0][1] * (1 - f) + a[1][1] * f);
        }
        break;
    }
    case 6: {  // Hard sync: saw slave with ratio 1..8
        std::vector<double> t(N);
        double ratio = 1.0 + x * 7.0;
        for (int n = 0; n < N; n++) {
            double ph = (double)n / N * ratio;
            ph -= std::floor(ph);
            t[n] = 1.0 - 2.0 * ph;
        }
        fromTime(t.data(), sp);
        break;
    }
    case 7: {  // Organ drawbars morph (harmonics 1,2,3,4,5,6,8)
        static const int hs[7] = {1, 2, 3, 4, 5, 6, 8};
        static const double pr[4][7] = {{1, .5, .3, 0, 0, 0, 0}, {1, 0, .7, 0, .4, 0, 0},
                                        {.6, .6, 1, .6, .3, .3, .2}, {.3, 1, .3, .8, .2, .5, .4}};
        double seg = x * 3.0;
        int s0 = std::min(2, (int)seg);
        double f = seg - s0;
        for (int k = 0; k < 7; k++) sp.S[hs[k]] = pr[s0][k] * (1 - f) + pr[s0 + 1][k] * f;
        break;
    }
    default: {  // Wavefold
        std::vector<double> t(N);
        double g = 0.5 + x * 9.0;
        for (int n = 0; n < N; n++) t[n] = std::sin(g * std::sin(2 * kPi * n / N));
        fromTime(t.data(), sp);
        break;
    }
    }
    sp.S[0] = sp.C[0] = 0.0;
}
}  // namespace

WavetableBank& WavetableBank::get() {
    static WavetableBank inst;
    return inst;
}

void WavetableBank::init() {
    static std::mutex m;
    std::lock_guard<std::mutex> lock(m);
    if (ready_) return;
    data_.assign((size_t)kNumTables * kMips * kFrames * kTableStride, 0.0f);
    std::vector<double> buf(N);
    for (int t = 0; t < kNumTables; t++) {
        for (int f = 0; f < kFrames; f++) {
            Spec sp;
            buildFrame(t, (double)f / (kFrames - 1), sp);
            double scale = 1.0;
            for (int m = 0; m < kMips; m++) {
                int cap = 512 >> m;
                toTime(sp, cap, buf.data());
                if (m == 0) {
                    double pk = 1e-6;
                    for (int i = 0; i < N; i++) pk = std::max(pk, std::fabs(buf[i]));
                    scale = 0.95 / pk;
                }
                float* dst = data_.data() + (((size_t)t * kMips + m) * kFrames + f) * kTableStride;
                for (int i = 0; i < N; i++) dst[i] = (float)(buf[i] * scale);
                dst[N] = dst[0];
            }
        }
    }
    ready_ = true;
}

}  // namespace sx
