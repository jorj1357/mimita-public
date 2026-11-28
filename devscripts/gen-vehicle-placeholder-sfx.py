"""Generate placeholder Rocket League vehicle sounds.

These are synthesized so the mode has immediate feedback; replace any .wav in
assets/sound/vehicle/ with a real recording later. The game resolves a sound
name `vehicle/boost` to `assets/sound/vehicle/boost.wav`.

Run: python devscripts/gen-vehicle-placeholder-sfx.py
"""
from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path

RATE = 48000
OUT = Path(__file__).resolve().parents[1] / "assets" / "sound" / "vehicle"


def write(name: str, samples: list[float]) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    peak = max(1e-6, max(abs(s) for s in samples))
    scale = 0.85 / peak
    data = b"".join(
        struct.pack("<h", int(max(-1.0, min(1.0, s * scale)) * 32767))
        for s in samples
    )
    with wave.open(str(OUT / name), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(data)


def env(t: float, attack: float, decay: float) -> float:
    if t < attack:
        return t / max(attack, 1e-6)
    return max(0.0, 1.0 - (t - attack) / max(decay, 1e-6))


def tone(t: float, f0: float, f1: float, dur: float) -> float:
    k = min(1.0, t / max(dur, 1e-6))
    freq = f0 + (f1 - f0) * k
    return math.sin(2.0 * math.pi * freq * t)


def noise(rng: random.Random) -> float:
    return rng.uniform(-1.0, 1.0)


def boost() -> list[float]:
    rng = random.Random(11)
    n = int(0.95 * RATE)
    out: list[float] = []
    lp = 0.0
    for i in range(n):
        t = i / RATE
        lp = lp * 0.86 + noise(rng) * 0.14
        rumble = math.sin(2 * math.pi * (150 + 90 * min(1.0, t / 0.5)) * t)
        a = min(1.0, t / 0.05) * max(0.0, 1.0 - t / 0.95)
        out.append(a * (0.55 * lp + 0.35 * rumble))
    return out


def blip(f0: float, f1: float, dur: float, gap: float = 0.0) -> list[float]:
    total = int((dur + gap) * RATE)
    out = []
    for i in range(total):
        t = i / RATE
        if t < gap:
            out.append(0.0)
        else:
            tt = t - gap
            out.append(env(tt, 0.005, dur) * tone(tt, f0, f1, dur) * 0.9)
    return out


def thump(f0: float, dur: float, noise_amt: float, seed: int) -> list[float]:
    rng = random.Random(seed)
    n = int(dur * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        e = env(t, 0.002, dur)
        body = math.sin(2 * math.pi * f0 * t) * (1.0 - min(1.0, t / dur))
        out.append(e * (body + noise(rng) * noise_amt))
    return out


def demolish() -> list[float]:
    rng = random.Random(23)
    n = int(1.05 * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        e = env(t, 0.003, 1.0)
        boom = math.sin(2 * math.pi * (60 - 20 * min(1.0, t)) * t)
        metal = math.sin(2 * math.pi * 1180 * t) * math.exp(-t * 5.0)
        out.append(e * (0.7 * boom + 0.4 * noise(rng) + 0.12 * metal))
    return out


def goal() -> list[float]:
    rng = random.Random(31)
    n = int(1.7 * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        swell = min(1.0, t / 0.12) * max(0.0, 1.0 - (t - 0.12) / 1.55)
        chord = (
            math.sin(2 * math.pi * 440 * t)
            + math.sin(2 * math.pi * 554 * t)
            + math.sin(2 * math.pi * 659 * t)
        ) / 3.0
        boom = math.sin(2 * math.pi * 70 * t) * math.exp(-t * 2.5)
        out.append(swell * (0.55 * chord + 0.45 * boom + noise(rng) * 0.05))
    return out


def ghost() -> list[float]:
    n = int(0.5 * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        trem = 0.5 + 0.5 * math.sin(2 * math.pi * 7.0 * t)
        out.append(env(t, 0.01, 0.5) * trem * math.sin(2 * math.pi * 620 * t) * 0.7)
    return out


def main() -> None:
    write("boost.wav", boost())
    write("jump.wav", blip(210, 520, 0.14))
    write("airjump.wav", blip(300, 700, 0.09, 0.05) + blip(360, 820, 0.10))
    write("dodge.wav", thump(180, 0.20, 0.5, 7))
    write("wall_bump.wav", thump(95, 0.28, 0.65, 13))
    write("land.wav", thump(120, 0.14, 0.4, 17))
    write("demolish.wav", demolish())
    write("ball_hit.wav", thump(165, 0.14, 0.25, 19))
    write("goal.wav", goal())
    write("ghost.wav", ghost())
    print(f"wrote {len(list(OUT.glob('*.wav')))} files to {OUT}")


if __name__ == "__main__":
    main()
