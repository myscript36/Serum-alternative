// Host-side test: renders audio offline and prints stats.  Usage:
//   host_render out.wav seconds notes=60,64,67 hold=1.5 key=value ...
#include "../app/src/main/cpp/engine.h"
#include "../app/src/main/cpp/params.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <chrono>

using namespace sx;
int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage\n"); return 1; }
    const char* path = argv[1];
    float seconds = (float)atof(argv[2]);
    std::vector<int> notes = {60};
    float hold = 1.0f;
    float sr = 48000.f;
    Engine eng(sr);
    for (int a = 3; a < argc; a++) {
        std::string s = argv[a];
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string k = s.substr(0, eq), v = s.substr(eq + 1);
        if (k == "notes") { notes.clear(); size_t p = 0; while (p < v.size()) { size_t c = v.find(',', p); if (c == std::string::npos) c = v.size(); notes.push_back(atoi(v.substr(p, c - p).c_str())); p = c + 1; } continue; }
        if (k == "hold") { hold = (float)atof(v.c_str()); continue; }
        int id = -1;
        for (int i = 0; i < P_COUNT; i++) if (k == kParams[i].key) id = i;
        if (id < 0) { fprintf(stderr, "unknown key %s\n", k.c_str()); return 2; }
        eng.setParam(id, (float)atof(v.c_str()));
    }
    int total = (int)(seconds * sr);
    std::vector<float> buf((size_t)total * 2);
    int block = 256, pos = 0; bool on = false;
    auto t0 = std::chrono::steady_clock::now();
    while (pos < total) {
        if (pos == 0 && !on) { for (int n : notes) eng.noteOn(n, 100); on = true; }
        if (on && pos >= (int)(hold * sr)) { for (int n : notes) eng.noteOff(n); on = false; hold = 1e9f; }
        int n = std::min(block, total - pos);
        eng.render(&buf[(size_t)pos * 2], n);
        pos += n;
    }
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    double peak = 0, sum = 0, tailSum = 0; long bad = 0, tailN = 0;
    for (int i = 0; i < total; i++) for (int c = 0; c < 2; c++) {
        float x = buf[(size_t)i * 2 + c];
        if (!std::isfinite(x)) { bad++; continue; }
        peak = std::max(peak, (double)std::fabs(x)); sum += (double)x * x;
        if (i > total - (int)(0.25 * sr)) { tailSum += (double)x * x; tailN++; }
    }
    double rms = std::sqrt(sum / (total * 2.0)), tail = tailN ? std::sqrt(tailSum / tailN) : 0;
    printf("peak=%.3f rms=%.4f tail=%.5f nan=%ld cpu=%.1f%%\n", peak, rms, tail, bad, ms / (seconds * 1000.0) * 100.0);
    FILE* f = fopen(path, "wb");
    int32_t dataLen = total * 4, v32; int16_t v16;
    fwrite("RIFF", 1, 4, f); v32 = 36 + dataLen; fwrite(&v32, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v32 = 16; fwrite(&v32, 4, 1, f); v16 = 1; fwrite(&v16, 2, 1, f); v16 = 2; fwrite(&v16, 2, 1, f);
    v32 = 48000; fwrite(&v32, 4, 1, f); v32 = 48000 * 4; fwrite(&v32, 4, 1, f); v16 = 4; fwrite(&v16, 2, 1, f); v16 = 16; fwrite(&v16, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dataLen, 4, 1, f);
    for (size_t i = 0; i < buf.size(); i++) { float x = buf[i]; if (!std::isfinite(x)) x = 0; int16_t s = (int16_t)(std::fmax(-1.f, std::fmin(1.f, x)) * 32767); fwrite(&s, 2, 1, f); }
    fclose(f);
    return bad ? 3 : 0;
}
