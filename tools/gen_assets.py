#!/usr/bin/env python3
"""
Generate original, IP-clean visual and audio assets for Bard Beats:
- romfs/audio/lute_tbl.bin  (Woody Renaissance Bard's Lute wavetable, 32728.5 Hz PCM16)
- romfs/audio/harp_tbl.bin  (Lush Celtic Minstrel Harp wavetable, 32728.5 Hz PCM16)
- romfs/audio/bass_tbl.bin  (Warm Upright Acoustic Bass wavetable, 32728.5 Hz PCM16)
- gfx/banner.wav            (2.0s D-Dorian lute arpeggio jingle, 22050 Hz PCM16)
"""

import argparse
import math
import os
import random
import struct
import wave

SAMPLE_RATE_3DS = 32728.498
BANNER_SR = 22050


def biquad_filter(samples: list[float], sr: float, mode: str, freq: float, q: float, gain_db: float = 0.0) -> list[float]:
    w0 = 2.0 * math.pi * freq / sr
    cos_w0 = math.cos(w0)
    sin_w0 = math.sin(w0)
    alpha = sin_w0 / (2.0 * q)
    A = 10.0 ** (gain_db / 40.0)

    if mode == "lowpass":
        b0 = (1.0 - cos_w0) * 0.5
        b1 = 1.0 - cos_w0
        b2 = (1.0 - cos_w0) * 0.5
        a0 = 1.0 + alpha
        a1 = -2.0 * cos_w0
        a2 = 1.0 - alpha
    elif mode == "peaking":
        b0 = 1.0 + alpha * A
        b1 = -2.0 * cos_w0
        b2 = 1.0 - alpha * A
        a0 = 1.0 + alpha / A
        a1 = -2.0 * cos_w0
        a2 = 1.0 - alpha / A
    else:  # highpass
        b0 = (1.0 + cos_w0) * 0.5
        b1 = -(1.0 + cos_w0)
        b2 = (1.0 + cos_w0) * 0.5
        a0 = 1.0 + alpha
        a1 = -2.0 * cos_w0
        a2 = 1.0 - alpha

    b0 /= a0; b1 /= a0; b2 /= a0
    a1 /= a0; a2 /= a0

    out = [0.0] * len(samples)
    x1 = x2 = y1 = y2 = 0.0
    for i, x0 in enumerate(samples):
        y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        out[i] = y0
        x2, x1 = x1, x0
        y2, y1 = y1, y0
    return out


def synth_plucked_string(freq_hz: float, duration_sec: float, sr: float,
                         pick_pos: float, brightness: float, decay_sec: float,
                         body_freq1: float, body_freq2: float, body_gain: float,
                         inharmonicity: float = 0.00012, seed: int = 42) -> list[float]:
    """
    Physically-inspired modal + harmonic plucked string with pick-position comb,
    frequency-dependent damping (higher harmonics decay faster like real gut/nylon strings),
    and wooden body resonance formants. Zero aliasing, rich acoustic warmth.
    """
    rng = random.Random(seed)
    n_samples = int(round(duration_sec * sr))
    out = [0.0] * n_samples

    # 1. Harmonic modal bank (up to Nyquist * 0.42 so pitch-shifting up 1 octave stays clean!)
    max_harm = min(22, int((sr * 0.40) / freq_hz))
    for k in range(1, max_harm + 1):
        # Real plucked string harmonic amplitude modulated by pick position sin(k * pi * pick_pos)
        pick_amp = abs(math.sin(k * math.pi * pick_pos))
        amp = (pick_amp + 0.12) / (k ** (1.05 + (1.0 - brightness) * 0.75))
        # Subtle string stiffness (inharmonicity) gives authentic acoustic shimmer
        fk = freq_hz * k * math.sqrt(1.0 + inharmonicity * k * k)
        # Higher harmonics decay much faster than the fundamental (nylon/gut warmth)
        tau = decay_sec / (1.0 + 0.32 * (k - 1) ** 1.35)
        phase0 = rng.uniform(0.0, 0.25)
        w = 2.0 * math.pi * fk / sr
        inv_tau = 1.0 / (tau * sr)
        for i in range(n_samples):
            # Fast 2.5ms pluck onset + natural exponential decay
            env = math.exp(-i * inv_tau)
            if i < 80:
                env *= (i / 80.0)
            out[i] += amp * env * math.sin(w * i + phase0)

    # 2. Short woody finger/plectrum transient (first 12ms)
    pluck_len = min(n_samples, int(0.014 * sr))
    transient = [0.0] * pluck_len
    for i in range(pluck_len):
        t = i / float(pluck_len)
        transient[i] = rng.uniform(-1.0, 1.0) * math.exp(-t * 6.5) * 0.28
    transient = biquad_filter(transient, sr, "lowpass", 2400.0 * (0.5 + brightness), 0.9)
    for i in range(pluck_len):
        out[i] += transient[i]

    # 3. Wooden soundbox body resonances (gives lute/harp/upright bass acoustic character)
    out = biquad_filter(out, sr, "peaking", body_freq1, 1.8, body_gain)
    out = biquad_filter(out, sr, "peaking", body_freq2, 2.2, body_gain * 0.7)
    out = biquad_filter(out, sr, "highpass", 45.0, 0.707)

    # Normalize to 0.92 peak and apply 15ms tail fade-out
    peak = max(abs(x) for x in out) or 1.0
    fade_len = min(int(0.015 * sr), n_samples // 4)
    for i in range(n_samples):
        v = (out[i] / peak) * 0.92
        if i >= n_samples - fade_len:
            v *= (n_samples - i) / float(fade_len)
        out[i] = v
    return out


def write_pcm16_bin(path: str, samples: list[float]) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(struct.pack("<I", len(samples)))
        for s in samples:
            clamped = max(-1.0, min(1.0, s))
            f.write(struct.pack("<h", int(round(clamped * 32767.0))))


def generate_audio_tables() -> None:
    # 1. Bard's Lute (base D3 = 146.83 Hz): warm gut strings, woody rosette resonance
    lute = synth_plucked_string(
        freq_hz=146.832, duration_sec=1.15, sr=SAMPLE_RATE_3DS,
        pick_pos=0.22, brightness=0.62, decay_sec=0.72,
        body_freq1=280.0, body_freq2=780.0, body_gain=4.5,
        inharmonicity=0.00018, seed=101
    )
    # 2. Minstrel Harp (base D4 = 293.66 Hz): sweet bell-clear sustain, plucked near middle
    harp = synth_plucked_string(
        freq_hz=293.665, duration_sec=1.35, sr=SAMPLE_RATE_3DS,
        pick_pos=0.38, brightness=0.52, decay_sec=1.10,
        body_freq1=340.0, body_freq2=1100.0, body_gain=3.5,
        inharmonicity=0.00008, seed=202
    )
    # 3. Upright Acoustic Bass (base D2 = 73.42 Hz): punchy finger pluck + 2nd/3rd harmonics audible on 3DS speakers
    bass = synth_plucked_string(
        freq_hz=73.416, duration_sec=0.95, sr=SAMPLE_RATE_3DS,
        pick_pos=0.28, brightness=0.34, decay_sec=0.58,
        body_freq1=165.0, body_freq2=420.0, body_gain=6.0,
        inharmonicity=0.00006, seed=303
    )

    write_pcm16_bin("romfs/audio/lute_tbl.bin", lute)
    write_pcm16_bin("romfs/audio/harp_tbl.bin", harp)
    write_pcm16_bin("romfs/audio/bass_tbl.bin", bass)


def generate_banner_wav() -> None:
    os.makedirs("gfx", exist_ok=True)
    # 1.4 second warm D-Dorian lute chord strum (D3, A3, D4, F4, A4)
    # Padded to exact multiple of 14 samples (DSP-ADPCM frame size) with trailing silence
    notes = [
        (146.83, 0.03),
        (220.00, 0.09),
        (293.66, 0.15),
        (349.23, 0.21),
        (440.00, 0.27),
    ]
    raw_len = int(1.40 * BANNER_SR)
    total_len = ((raw_len + 13) // 14) * 14
    active_len = total_len - int(0.08 * BANNER_SR)
    mix = [0.0] * total_len
    for idx, (freq, delay) in enumerate(notes):
        tone = synth_plucked_string(
            freq_hz=freq, duration_sec=1.25, sr=BANNER_SR,
            pick_pos=0.24, brightness=0.55, decay_sec=0.65,
            body_freq1=280.0, body_freq2=780.0, body_gain=4.0,
            seed=500 + idx
        )
        offset = int(delay * BANNER_SR)
        for i, val in enumerate(tone):
            if offset + i < active_len:
                mix[offset + i] += val * 0.30

    # Smooth 25ms fade-in and 120ms fade-out before trailing silence
    fade_in = int(0.025 * BANNER_SR)
    fade_out = int(0.120 * BANNER_SR)
    for i in range(fade_in):
        mix[i] *= (i / float(fade_in))
    for i in range(fade_out):
        idx = active_len - fade_out + i
        if 0 <= idx < active_len:
            mix[idx] *= (1.0 - (i / float(fade_out)))

    peak = max(abs(x) for x in mix) or 1.0
    with wave.open("gfx/banner.wav", "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(BANNER_SR)
        frames = bytearray()
        for s in mix:
            v = max(-1.0, min(1.0, (s / peak) * 0.72))
            frames.extend(struct.pack("<h", int(round(v * 32767.0))))
        wf.writeframes(bytes(frames))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--audio-only", action="store_true")
    args = parser.parse_args()

    generate_audio_tables()
    if not args.audio_only:
        generate_banner_wav()
    print("Generated upgraded Bard Beats audio assets successfully.")


if __name__ == "__main__":
    main()
