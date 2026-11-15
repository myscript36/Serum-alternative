# WaveForge – wavetable szintetizátor Androidra

Eredeti, Serum-stílusú wavetable szintetizátor (C++ hangmotor + Kotlin/Jetpack Compose felület).
Nem a Xfer Serum 2 másolata, és nem használ semmilyen Xfer kódot, wavetable-t vagy presetet.
Androidon nincs VST-formátum, ezért ez önálló app, amely MIDI-vel játszható.

## APK építése (GitHub)
1. Hozz létre egy új GitHub repót, és töltsd fel ennek a mappának a tartalmát (a `.github` mappával együtt).
2. A push után a **Actions → Build APK** workflow lefut. Futás közben saját tesztet is csinál a hangmotoron.
3. A futás végén az **Artifacts → WaveForge-apk** alatt letölthető az APK (debug kulccsal aláírt, telepíthető).
4. Telepítés: engedélyezd az ismeretlen forrásból való telepítést.

## Funkciók
- **2 wavetable oszcillátor** (A/B): 9 beépített, sávkorlátozott wavetable (64 frame, mip-mapelt), WT Pos, 8 warp mód
  (Sync, Bend+/-, Mirror, Asym, Self-FM, Quantize), oktáv/félhang/finomhangolás, fázis/random fázis, pan, szint.
- **Unison** oszcillátoronként 1–16 hang: detune, blend, stereo spread.
- **Sub oszcillátor** (sine/tri/square/saw) és **zaj** (fehér/rózsaszín).
- **Szűrő**: LP12, LP24, HP12, HP24, Band, Notch, Peak, Comb; cutoff, rezonancia, drive, mix, key tracking, routing (A/B/Sub/Noise).
- **3 envelope** (Env1 = amp; Env2/3 = mod forrás), **4 LFO** (8 alak, Hz vagy BPM-szinkron, trigger/free).
- **Mod mátrix**: 8 slot, 17 forrás (Env, LFO, Velocity, Key, ModWheel, Aftertouch, PitchBend, 4 Macro, Random) → 19 cél.
- **FX**: Distortion (4 típus), Chorus, ping-pong Delay (BPM-szinkron), Reverb.
- **Arpeggiator** (5 minta, 1–4 oktáv, gate, BPM).
- 16 hang polifónia, Mono/Legato/Glide, pitch bend tartomány, szusztain pedál (CC64).
- **15 gyári preset**, saját presetek mentése/betöltése.
- **MIDI**: USB/Bluetooth MIDI-vezérlő automatikusan, ÉS virtuális MIDI-eszköz ("WaveForge Synth"):
  más Android-appok (DAW-k, szekvenszerek) kiválaszthatják MIDI-kimenetként.
  CC1 modwheel, CC20-23 makrók, CC74 cutoff, CC71 rezonancia, CC123 minden hang le.
- Alacsony késleltetésű AAudio kimenet, előtérszolgáltatás, hogy háttérben is szóljon.

## Felépítés
- `app/src/main/cpp/` – hangmotor (`engine.cpp`), wavetable-generátor (`wavetables.cpp`), AAudio + JNI.
- `app/src/main/java/com/waveforge/synth/` – Kotlin: állapot, MIDI, presetek, Compose UI.
- `tools/gen_params.py` – **egyetlen forrás** a paraméterekhez: ebből készül a `params.h`, a `Params.kt` és a gyári presetek.
  Új paraméter vagy preset: szerkeszd ezt, majd `python3 tools/gen_params.py`.
- `test/host_render.cpp` + `tools/test_presets.py`, `tools/fuzz_engine.py` – a hangmotor gépen futtatható tesztjei.

## Tesztelés helyben (Linux/Mac)
```
python3 tools/gen_params.py
g++ -O2 -std=c++17 -o host_render test/host_render.cpp app/src/main/cpp/engine.cpp app/src/main/cpp/wavetables.cpp
python3 tools/test_presets.py ./host_render
python3 tools/fuzz_engine.py ./host_render 60
```
