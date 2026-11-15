#!/usr/bin/env python3
"""Renders every factory preset on the host engine and checks level / NaN / silence."""
import json, subprocess, sys, os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/host_render"
pre = json.load(open(os.path.join(ROOT, "app/src/main/assets/presets/factory.json")))
bad = 0
for p in pre:
    args = [f"{k}={v}" for k, v in p["params"].items()]
    r = subprocess.run([BIN, "/tmp/p.wav", "4", "notes=48,55,60,64", "hold=2"] + args, capture_output=True, text=True)
    line = r.stdout.strip() or r.stderr.strip()
    peak = float(line.split("peak=")[1].split()[0]) if "peak=" in line else 9
    rms = float(line.split("rms=")[1].split()[0]) if "rms=" in line else 0
    ok = r.returncode == 0 and 0.05 < peak <= 1.0 and rms > 0.01
    if not ok: bad += 1
    print(("OK  " if ok else "FAIL"), f"{p['name']:<16}", line)
sys.exit(1 if bad else 0)
