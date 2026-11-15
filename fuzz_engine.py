#!/usr/bin/env python3
"""Random parameter fuzzing against the host engine: checks for NaN/inf and runaway levels."""
import random, re, subprocess, sys, os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/host_render"
N = int(sys.argv[2]) if len(sys.argv) > 2 else 60
params = []
for line in open(os.path.join(ROOT, "app/src/main/cpp/params.h")):
    m = re.match(r'\s*\{"(\w+)", \'(\w)\', ([-\d.]+)f, ([-\d.]+)f, ([-\d.]+)f\},', line)
    if m: params.append((m.group(1), m.group(2), float(m.group(3)), float(m.group(4))))
random.seed(1)
bad = 0; worst = 0; cpu_worst = 0
for it in range(N):
    args = []
    for k, kind, lo, hi in params:
        if k in ("master",): continue
        if random.random() < 0.6 and not k.endswith(("_on",)): continue
        if kind == "f": v = random.uniform(lo, hi)
        else: v = random.randint(int(lo), int(hi))
        args.append(f"{k}={v}")
    r = subprocess.run([BIN, "/tmp/f.wav", "2.5", "notes=36,48,60,72,84,95", "hold=1.5"] + args, capture_output=True, text=True)
    out = r.stdout.strip()
    if r.returncode != 0 or "nan=0" not in out:
        bad += 1; print("FAIL", out or r.stderr, args[:6]); continue
    pk = float(out.split("peak=")[1].split()[0]); worst = max(worst, pk)
    cpu_worst = max(cpu_worst, float(out.split("cpu=")[1].rstrip("%")))
print(f"fuzz {N} runs, failures={bad}, worst peak={worst:.3f}, worst cpu={cpu_worst:.1f}% realtime (host)")
sys.exit(1 if bad else 0)
