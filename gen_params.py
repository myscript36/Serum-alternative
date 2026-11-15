#!/usr/bin/env python3
"""Single source of truth for all synth parameters.

Generates:
  app/src/main/cpp/params.h                         (C++ enum + table)
  app/src/main/java/com/waveforge/synth/Params.kt   (Kotlin table + label lists)
  app/src/main/assets/presets/factory.json          (factory presets, by key)
Run:  python3 tools/gen_params.py
"""
import json, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------- labels
TABLES = ["Analog Morph", "PWM Pulse", "Harmonic Sweep", "Vowel Formant", "Digital FM",
          "Spectral Morph", "Hard Sync", "Organ Drawbars", "Wavefold"]
WARP = ["Off", "Sync", "Bend +", "Bend -", "Mirror", "Asym", "Self FM", "Quantize"]
FILTER = ["LP 12", "LP 24", "HP 12", "HP 24", "Band", "Notch", "Peak", "Comb"]
LFO_SHAPES = ["Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample&Hold", "Smooth Rnd", "Pluck"]
MOD_SRC = ["None", "Env 2", "Env 3", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Velocity", "Key",
           "Mod Wheel", "Aftertouch", "Pitch Bend", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Random"]
MOD_DST = ["None", "A Pos", "B Pos", "A Warp", "B Warp", "A Level", "B Level", "A Pitch", "B Pitch",
           "A Detune", "B Detune", "Cutoff", "Resonance", "Drive", "Amp", "Sub Level", "Noise Level",
           "A Pan", "B Pan", "Pitch All"]
DIVS = ["1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "2 bars", "1/4 dot", "1/8 dot", "1/8 trip", "1/4 trip"]
ARP_RATE = ["1/4", "1/8", "1/16", "1/32", "1/8 trip", "1/16 trip"]
ARP_PAT = ["Up", "Down", "Up/Down", "Random", "As played"]
DIST = ["Soft", "Hard", "Fold", "Crush"]
NOISE = ["White", "Pink"]
SUB = ["Sine", "Triangle", "Square", "Saw"]

P = []
def add(k, kind, lo, hi, d):
    assert all(p["key"] != k for p in P), k
    P.append(dict(key=k, kind=kind, lo=float(lo), hi=float(hi), d=float(d)))
def F(k, lo, hi, d): add(k, "f", lo, hi, d)
def I(k, lo, hi, d): add(k, "i", lo, hi, d)
def B(k, d): add(k, "b", 0, 1, d)

# global
F("master", 0, 1, 0.7); F("bpm", 40, 240, 120); B("mono", 0); B("legato", 0)
F("glide", 0, 1, 0); I("bend", 1, 24, 2); I("voices", 1, 16, 16)
for n in range(1, 5): F(f"macro{n}", 0, 1, 0)

# oscillators (identical layout!)
OSC_KEYS = ["on", "table", "pos", "oct", "semi", "fine", "phase", "rand", "uni", "det",
            "blend", "spread", "wmode", "warp", "level", "pan"]
for o in "ab":
    B(f"{o}_on", 1 if o == "a" else 0)
    I(f"{o}_table", 0, len(TABLES) - 1, 0)
    F(f"{o}_pos", 0, 1, 2 / 3 if o == "a" else 0.0)
    I(f"{o}_oct", -3, 3, 0); I(f"{o}_semi", -12, 12, 0); F(f"{o}_fine", -1, 1, 0)
    F(f"{o}_phase", 0, 1, 0); F(f"{o}_rand", 0, 1, 1)
    I(f"{o}_uni", 1, 16, 1); F(f"{o}_det", 0, 1, 0.25); F(f"{o}_blend", 0, 1, 0.75); F(f"{o}_spread", 0, 1, 0.5)
    I(f"{o}_wmode", 0, len(WARP) - 1, 0); F(f"{o}_warp", 0, 1, 0)
    F(f"{o}_level", 0, 1, 0.75); F(f"{o}_pan", -1, 1, 0)

B("sub_on", 0); I("sub_shape", 0, 3, 0); I("sub_oct", -3, 0, -1); F("sub_level", 0, 1, 0.5)
B("noise_on", 0); F("noise_level", 0, 1, 0.25); I("noise_type", 0, 1, 0)

B("f_on", 1); I("f_type", 0, len(FILTER) - 1, 0); F("f_cutoff", 0, 1, 0.75); F("f_res", 0, 1, 0.15)
F("f_drive", 0, 1, 0); F("f_mix", 0, 1, 1); F("f_key", 0, 1, 0)
B("f_ra", 1); B("f_rb", 1); B("f_rs", 1); B("f_rn", 1)

ENV_DEF = {1: (0.05, 0.5, 0.8, 0.55), 2: (0.3, 0.6, 0.0, 0.5), 3: (0.3, 0.6, 0.0, 0.5)}
for n in (1, 2, 3):
    a, d, s, r = ENV_DEF[n]
    F(f"e{n}_a", 0, 1, a); F(f"e{n}_d", 0, 1, d); F(f"e{n}_s", 0, 1, s); F(f"e{n}_r", 0, 1, r)

for n in range(1, 5):
    I(f"l{n}_shape", 0, len(LFO_SHAPES) - 1, 0); F(f"l{n}_rate", 0, 1, 0.4); B(f"l{n}_sync", 0)
    I(f"l{n}_div", 0, len(DIVS) - 1, 3); B(f"l{n}_retrig", 1); F(f"l{n}_phase", 0, 1, 0)

for n in range(1, 9):
    I(f"m{n}_src", 0, len(MOD_SRC) - 1, 0); I(f"m{n}_dst", 0, len(MOD_DST) - 1, 0); F(f"m{n}_amt", -1, 1, 0)

B("dist_on", 0); I("dist_type", 0, 3, 0); F("dist_amt", 0, 1, 0.3); F("dist_mix", 0, 1, 1)
B("ch_on", 0); F("ch_rate", 0, 1, 0.3); F("ch_depth", 0, 1, 0.5); F("ch_mix", 0, 1, 0.4)
B("dl_on", 0); F("dl_time", 0, 1, 0.4); B("dl_sync", 1); I("dl_div", 0, len(DIVS) - 1, 3)
F("dl_fb", 0, 0.95, 0.4); F("dl_mix", 0, 1, 0.25); F("dl_tone", 0, 1, 0.7)
B("rv_on", 0); F("rv_size", 0, 1, 0.6); F("rv_damp", 0, 1, 0.5); F("rv_mix", 0, 1, 0.2)

B("arp_on", 0); I("arp_rate", 0, 5, 2); I("arp_pat", 0, 4, 0); I("arp_oct", 1, 4, 1); F("arp_gate", 0.05, 1, 0.6)

IDX = {p["key"]: i for i, p in enumerate(P)}

# ---------------------------------------------------------------- presets
def t(sec):  # envelope time -> normalized
    import math
    return round(math.log10(max(sec, 0.001) / 0.001) / 4, 3)

def hz_to_rate(hz):
    import math
    return round(math.log(hz / 0.05) / math.log(600), 3)

def cutoff(hz):
    import math
    return round(math.log10(hz / 20) / 3, 3)

PRESETS = {
 "Init": {},
 "Super Saw Lead": {
   "a_uni": 7, "a_det": 0.35, "a_blend": 0.8, "a_spread": 0.8, "a_level": 0.6,
   "b_on": 1, "b_pos": 2/3, "b_uni": 5, "b_det": 0.3, "b_spread": 0.8, "b_level": 0.5, "b_oct": 0, "b_fine": 0.05,
   "f_cutoff": cutoff(5000), "f_res": 0.2, "e1_a": t(0.003), "e1_d": t(0.4), "e1_s": 0.8, "e1_r": t(0.25),
   "ch_on": 1, "ch_mix": 0.3, "dl_on": 1, "dl_mix": 0.2, "rv_on": 1, "rv_mix": 0.2, "master": 0.55},
 "Sub Bass": {
   "a_pos": 0.0, "a_level": 0.7, "sub_on": 1, "sub_shape": 0, "sub_oct": -1, "sub_level": 0.5,
   "mono": 1, "glide": 0.15, "f_cutoff": cutoff(900), "f_res": 0.1, "e1_a": t(0.002), "e1_d": t(0.3), "e1_s": 1.0, "e1_r": t(0.1),
   "master": 0.8},
 "Acid Pluck": {
   "mono": 1, "legato": 1, "glide": 0.2, "a_pos": 2/3, "a_level": 0.8,
   "f_type": 1, "f_cutoff": cutoff(300), "f_res": 0.75, "f_drive": 0.2,
   "e1_a": t(0.002), "e1_d": t(0.35), "e1_s": 0.5, "e1_r": t(0.1),
   "e2_a": t(0.001), "e2_d": t(0.25), "e2_s": 0.0,
   "m1_src": 1, "m1_dst": 11, "m1_amt": 0.55,
   "dist_on": 1, "dist_type": 0, "dist_amt": 0.3, "dist_mix": 0.6, "dl_on": 1, "dl_mix": 0.2, "master": 0.6},
 "Warm Pad": {
   "a_pos": 0.55, "a_uni": 5, "a_det": 0.3, "a_spread": 0.9, "a_level": 0.6,
   "b_on": 1, "b_pos": 0.3, "b_fine": 0.08, "b_uni": 3, "b_det": 0.2, "b_level": 0.5,
   "f_cutoff": cutoff(1800), "f_res": 0.1, "f_key": 0.4,
   "e1_a": t(0.6), "e1_d": t(0.8), "e1_s": 0.8, "e1_r": t(1.2),
   "l1_rate": hz_to_rate(0.25), "m1_src": 3, "m1_dst": 1, "m1_amt": 0.25,
   "ch_on": 1, "ch_mix": 0.35, "rv_on": 1, "rv_size": 0.85, "rv_mix": 0.35, "master": 0.55},
 "Vowel Pad": {
   "a_table": 3, "a_pos": 0.0, "a_uni": 3, "a_det": 0.2, "a_spread": 0.8, "a_level": 0.8, "f_on": 0,
   "e1_a": t(0.4), "e1_d": t(0.5), "e1_s": 0.9, "e1_r": t(1.0),
   "l1_rate": hz_to_rate(0.2), "l1_retrig": 0, "m1_src": 3, "m1_dst": 1, "m1_amt": 0.5,
   "rv_on": 1, "rv_size": 0.8, "rv_mix": 0.3, "master": 0.55},
 "FM Bell": {
   "a_table": 4, "a_pos": 0.5, "a_oct": 1, "a_level": 0.6, "b_on": 1, "b_table": 4, "b_pos": 0.2, "b_oct": 2, "b_level": 0.3,
   "f_on": 0, "e1_a": t(0.001), "e1_d": t(1.5), "e1_s": 0.0, "e1_r": t(1.5),
   "e2_a": t(0.001), "e2_d": t(1.0), "e2_s": 0.0, "m1_src": 1, "m1_dst": 1, "m1_amt": -0.4,
   "dl_on": 1, "dl_mix": 0.25, "rv_on": 1, "rv_size": 0.8, "rv_mix": 0.3, "master": 0.55},
 "Wobble Bass": {
   "mono": 1, "a_uni": 3, "a_det": 0.2, "a_level": 0.7, "sub_on": 1, "sub_level": 0.5,
   "f_type": 1, "f_cutoff": cutoff(400), "f_res": 0.5, "f_drive": 0.2,
   "l1_shape": 1, "l1_sync": 1, "l1_div": 3, "l1_retrig": 1, "m1_src": 3, "m1_dst": 11, "m1_amt": 0.45,
   "e1_a": t(0.002), "e1_s": 1.0, "e1_r": t(0.15),
   "dist_on": 1, "dist_type": 1, "dist_amt": 0.35, "dist_mix": 0.5, "master": 0.6},
 "Schranz Stab": {
   "a_uni": 5, "a_det": 0.45, "a_blend": 0.9, "a_spread": 0.7, "a_level": 0.55,
   "b_on": 1, "b_pos": 2/3, "b_semi": 7, "b_uni": 3, "b_det": 0.3, "b_level": 0.4,
   "f_type": 1, "f_cutoff": cutoff(700), "f_res": 0.35, "f_drive": 0.2,
   "e1_a": t(0.001), "e1_d": t(0.25), "e1_s": 0.0, "e1_r": t(0.2),
   "e2_a": t(0.001), "e2_d": t(0.3), "e2_s": 0.0, "m1_src": 1, "m1_dst": 11, "m1_amt": 0.4,
   "dist_on": 1, "dist_type": 1, "dist_amt": 0.5, "dist_mix": 0.7,
   "dl_on": 1, "dl_div": 4, "dl_mix": 0.2, "rv_on": 1, "rv_mix": 0.12, "master": 0.5},
 "Drawbar Organ": {
   "a_table": 7, "a_pos": 0.1, "a_level": 0.6, "b_on": 1, "b_table": 7, "b_pos": 0.4, "b_oct": 1, "b_level": 0.3,
   "f_cutoff": cutoff(6000), "e1_a": t(0.003), "e1_d": t(0.1), "e1_s": 1.0, "e1_r": t(0.08),
   "ch_on": 1, "ch_mix": 0.4, "ch_rate": 0.5, "master": 0.6},
 "PWM Strings": {
   "a_table": 1, "a_pos": 0.1, "a_uni": 5, "a_det": 0.2, "a_spread": 0.8, "a_level": 0.6,
   "l1_rate": hz_to_rate(0.6), "l1_retrig": 0, "m1_src": 3, "m1_dst": 1, "m1_amt": 0.35,
   "f_cutoff": cutoff(3500), "e1_a": t(0.25), "e1_d": t(0.5), "e1_s": 0.85, "e1_r": t(0.6),
   "ch_on": 1, "ch_mix": 0.4, "rv_on": 1, "rv_mix": 0.25, "master": 0.55},
 "Sync Lead": {
   "mono": 1, "glide": 0.1, "a_table": 6, "a_pos": 0.3, "a_uni": 2, "a_det": 0.1, "a_level": 0.7,
   "f_cutoff": cutoff(7000), "f_res": 0.15, "e1_a": t(0.002), "e1_s": 0.9, "e1_r": t(0.2),
   "e2_a": t(0.15), "e2_d": t(0.5), "e2_s": 0.5, "m1_src": 1, "m1_dst": 1, "m1_amt": 0.4,
   "m2_src": 9, "m2_dst": 1, "m2_amt": 0.3,
   "dl_on": 1, "dl_mix": 0.25, "master": 0.55},
 "Arp Pluck": {
   "arp_on": 1, "arp_rate": 2, "arp_pat": 0, "arp_oct": 2, "arp_gate": 0.5, "a_uni": 3, "a_det": 0.2, "a_level": 0.7,
   "f_type": 1, "f_cutoff": cutoff(1500), "f_res": 0.3,
   "e1_a": t(0.001), "e1_d": t(0.25), "e1_s": 0.0, "e1_r": t(0.2),
   "e2_a": t(0.001), "e2_d": t(0.2), "e2_s": 0.0, "m1_src": 1, "m1_dst": 11, "m1_amt": 0.3,
   "dl_on": 1, "dl_mix": 0.3, "rv_on": 1, "rv_mix": 0.2, "master": 0.55},
 "Fold Growl": {
   "mono": 1, "a_table": 8, "a_pos": 0.4, "a_level": 0.7, "sub_on": 1, "sub_level": 0.4,
   "l1_rate": hz_to_rate(3), "l1_shape": 1, "m1_src": 3, "m1_dst": 1, "m1_amt": 0.35,
   "f_cutoff": cutoff(2500), "f_res": 0.25, "dist_on": 1, "dist_type": 2, "dist_amt": 0.4, "dist_mix": 0.6,
   "e1_a": t(0.002), "e1_s": 1.0, "e1_r": t(0.15), "master": 0.55},
 "Ambient Wash": {
   "a_table": 2, "a_pos": 0.3, "a_uni": 4, "a_det": 0.4, "a_spread": 1.0, "a_level": 0.6,
   "b_on": 1, "b_table": 5, "b_pos": 0.2, "b_oct": 1, "b_level": 0.3,
   "f_cutoff": cutoff(3000), "e1_a": t(1.5), "e1_d": t(1.0), "e1_s": 0.9, "e1_r": t(3.0),
   "l1_rate": hz_to_rate(0.1), "l1_retrig": 0, "m1_src": 3, "m1_dst": 1, "m1_amt": 0.5,
   "l2_rate": hz_to_rate(0.15), "l2_retrig": 0, "m2_src": 4, "m2_dst": 2, "m2_amt": 0.5,
   "ch_on": 1, "ch_mix": 0.3, "dl_on": 1, "dl_div": 2, "dl_fb": 0.5, "dl_mix": 0.3,
   "rv_on": 1, "rv_size": 0.95, "rv_damp": 0.4, "rv_mix": 0.45, "master": 0.5},
}

# ---------------------------------------------------------------- emit C++
def cname(k): return "P_" + k.upper()

def emit_cpp():
    L = ["// GENERATED by tools/gen_params.py - do not edit", "#pragma once", "#include <cstddef>", "namespace sx {", ""]
    L.append("enum ParamId : int {")
    for i, p in enumerate(P):
        L.append(f"  {cname(p['key'])} = {i},")
    L.append("  P_COUNT")
    L.append("};")
    L.append("struct ParamInfo { const char* key; char kind; float lo, hi, def; };")
    L.append("inline constexpr ParamInfo kParams[P_COUNT] = {")
    for p in P:
        L.append(f'  {{"{p["key"]}", \'{p["kind"]}\', {p["lo"]}f, {p["hi"]}f, {p["d"]:.6f}f}},')
    L.append("};")
    L.append("enum OscOff : int { " + ", ".join("O_" + k.upper() for k in OSC_KEYS) + ", O_STRIDE };")
    L.append("static_assert(P_B_ON - P_A_ON == O_STRIDE, \"osc layout\");")
    for k in OSC_KEYS:
        L.append(f"static_assert(P_A_{k.upper()} - P_A_ON == O_{k.upper()}, \"osc layout {k}\");")
        L.append(f"static_assert(P_B_{k.upper()} - P_B_ON == O_{k.upper()}, \"osc layout {k}\");")
    L.append("static_assert(P_E2_A - P_E1_A == 4 && P_E3_A - P_E2_A == 4, \"env layout\");")
    L.append("static_assert(P_L2_SHAPE - P_L1_SHAPE == 6 && P_L4_SHAPE - P_L3_SHAPE == 6, \"lfo layout\");")
    L.append("static_assert(P_M2_SRC - P_M1_SRC == 3 && P_M8_SRC - P_M7_SRC == 3, \"mod layout\");")
    L.append(f"constexpr int kNumTables = {len(TABLES)};")
    L.append("}  // namespace sx")
    path = os.path.join(ROOT, "app/src/main/cpp/params.h")
    open(path, "w").write("\n".join(L) + "\n")

def kl(lst):
    return "listOf(" + ", ".join('"' + s + '"' for s in lst) + ")"

def emit_kt():
    L = ["// GENERATED by tools/gen_params.py - do not edit", "package com.waveforge.synth", "",
         'class ParamDef(val key: String, val kind: Char, val lo: Float, val hi: Float, val def: Float)', "",
         "object Labels {"]
    for name, lst in [("tables", TABLES), ("warp", WARP), ("filter", FILTER), ("lfoShapes", LFO_SHAPES),
                      ("modSrc", MOD_SRC), ("modDst", MOD_DST), ("divs", DIVS), ("arpRate", ARP_RATE),
                      ("arpPat", ARP_PAT), ("dist", DIST), ("noise", NOISE), ("sub", SUB)]:
        L.append(f"    val {name} = {kl(lst)}")
    L.append("}")
    L.append("")
    L.append("object P {")
    L.append("    val defs: List<ParamDef> = listOf(")
    for p in P:
        L.append(f'        ParamDef("{p["key"]}", \'{p["kind"]}\', {p["lo"]}f, {p["hi"]}f, {p["d"]:.6f}f),')
    L.append("    )")
    L.append("    val count: Int = defs.size")
    L.append("    private val index: Map<String, Int> = defs.mapIndexed { i, d -> d.key to i }.toMap()")
    L.append("    fun id(key: String): Int = index[key] ?: throw IllegalArgumentException(\"unknown param $key\")")
    L.append("    fun idOrNull(key: String): Int? = index[key]")
    L.append("}")
    path = os.path.join(ROOT, "app/src/main/java/com/waveforge/synth/Params.kt")
    open(path, "w").write("\n".join(L) + "\n")

def emit_presets():
    out = []
    for name, d in PRESETS.items():
        for k in d:
            assert k in IDX, f"{name}: unknown key {k}"
        out.append({"name": name, "params": d})
    path = os.path.join(ROOT, "app/src/main/assets/presets/factory.json")
    json.dump(out, open(path, "w"), indent=1)

if __name__ == "__main__":
    assert len(TABLES) == 9
    emit_cpp(); emit_kt(); emit_presets()
    print("params:", len(P), "presets:", len(PRESETS))
