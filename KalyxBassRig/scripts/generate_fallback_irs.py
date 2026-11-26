#!/usr/bin/env python3
"""
Fallback Impulse Response generator for Kalyx Bass Rig.
Generates minimal realistic bass cabinet IRs (8x10 Fridge, 4x10 Modern, 1x15 Vintage, DI Flat)
if Resources/IRs/ contains no .wav files.
Uses only Python standard library so it works everywhere without external deps.
"""

import math
import os
import struct
import wave


def write_wav_mono(path: str, samples: list[float], sample_rate: int = 48000) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    peak = max((abs(s) for s in samples), default=1.0)
    scale = 0.85 / max(peak, 1e-9)
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        frames = bytearray()
        for s in samples:
            v = max(-1.0, min(1.0, s * scale))
            frames.extend(struct.pack("<h", int(round(v * 32767.0))))
        wf.writeframes(bytes(frames))


def make_cab_ir(
    sample_rate: int,
    length_samples: int,
    f_res: float,
    q_res: float,
    f_lp: float,
    decay_ms: float,
) -> list[float]:
    ir = [0.0] * length_samples
    tau = max(1e-4, decay_ms * 0.001)
    for n in range(length_samples):
        t = n / float(sample_rate)
        env = math.exp(-t / tau)
        # Resonant speaker cone + cabinet air load + high-frequency rolloff
        body = math.sin(2.0 * math.pi * f_res * t) * math.exp(-t * math.pi * f_res / max(q_res, 0.5))
        punch = math.exp(-t * 2.0 * math.pi * f_lp) * (1.0 if n == 0 else 0.35)
        ir[n] = (0.65 * body + 0.35 * punch) * env
    # Ensure DC-free and strong initial transient
    ir[0] += 0.45
    return ir


def main() -> None:
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ir_dir = os.path.join(repo_root, "Resources", "IRs")
    os.makedirs(ir_dir, exist_ok=True)

    existing = [f for f in os.listdir(ir_dir) if f.lower().endswith(".wav")]
    if existing:
        print(f"[generate_fallback_irs] Found {len(existing)} existing .wav IR(s) in {ir_dir}; skipping generation.")
        return

    sr = 48000
    n_samples = 2048
    specs = [
        ("Fallback_8x10_Fridge_RE20.wav", 95.0, 1.8, 3600.0, 18.0),
        ("Fallback_4x10_Modern_Beta52.wav", 110.0, 2.1, 4800.0, 15.0),
        ("Fallback_1x15_Vintage_U47.wav", 72.0, 1.5, 2800.0, 22.0),
        ("Fallback_SVT_DI_Out.wav", 55.0, 0.8, 12000.0, 6.0),
    ]
    for name, f_res, q_res, f_lp, dec in specs:
        ir = make_cab_ir(sr, n_samples, f_res, q_res, f_lp, dec)
        write_wav_mono(os.path.join(ir_dir, name), ir, sr)
        print(f"[generate_fallback_irs] Generated {name}")


if __name__ == "__main__":
    main()
