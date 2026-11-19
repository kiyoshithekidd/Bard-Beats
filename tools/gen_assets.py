#!/usr/bin/env python3
"""
Generate original, IP-clean visual and audio assets for Bard Beats:
- icon.png (48x48)
- gfx/banner.png (256x128)
- gfx/banner.wav (2.0s D-Dorian lute chord jingle, 22050 Hz PCM16)
- romfs/audio/lute_tbl.bin (Karplus-Strong plucked lute wavetable, 32728.5 Hz PCM16)
- romfs/audio/harp_tbl.bin (Karplus-Strong Celtic harp wavetable, 32728.5 Hz PCM16)
- romfs/audio/bass_tbl.bin (Warm upright acoustic bass wavetable, 32728.5 Hz PCM16)
"""

import argparse
import math
import os
import random
import struct
import wave

SAMPLE_RATE_3DS = 32728.498
BANNER_SR = 22050


def karplus_strong(freq_hz: float, duration_sec: float, sr: float,
                   decay: float = 0.996, brightness: float = 0.5, seed: int = 42) -> list[float]:
    rng = random.Random(seed)
    period = max(2, int(round(sr / freq_hz)))
    # Plucked excitation: filtered noise burst + warm fundamental impulse
    buf = []
    for i in range(period):
        noise = rng.uniform(-1.0, 1.0)
        sine = 0.35 * math.sin(2.0 * math.pi * i / period)
        buf.append(0.65 * noise + sine)

    total_samples = int(round(duration_sec * sr))
    out = []
    idx = 0
    prev = 0.0
    for _ in range(total_samples):
        cur = buf[idx]
        nxt = buf[(idx + 1) % period]
        filtered = decay * (brightness * cur + (1.0 - brightness) * nxt)
        buf[idx] = 0.85 * filtered + 0.15 * prev
        prev = buf[idx]
        out.append(cur)
        idx = (idx + 1) % period

    # Normalize to 0.92 peak and apply gentle 10ms fade-out at end
    peak = max(abs(x) for x in out) or 1.0
    fade_len = min(int(0.01 * sr), total_samples // 4)
    for i in range(total_samples):
        out[i] = (out[i] / peak) * 0.92
        if i >= total_samples - fade_len:
            out[i] *= (total_samples - i) / float(fade_len)
    return out


def write_pcm16_bin(path: str, samples: list[float]) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        # 4-byte uint32 sample count header followed by int16 PCM samples
        f.write(struct.pack("<I", len(samples)))
        for s in samples:
            clamped = max(-1.0, min(1.0, s))
            f.write(struct.pack("<h", int(round(clamped * 32767.0))))


def generate_audio_tables() -> None:
    # Base pitch D3 (146.83 Hz) for Lute, D4 (293.66 Hz) for Harp, D2 (73.42 Hz) for Upright Bass
    # Length = 1.2s for Lute/Harp, 0.9s for Bass -> ~210 KB total in RomFS
    lute = karplus_strong(146.832, 1.1, SAMPLE_RATE_3DS, decay=0.9965, brightness=0.58, seed=101)
    harp = karplus_strong(293.665, 1.2, SAMPLE_RATE_3DS, decay=0.9978, brightness=0.45, seed=202)
    bass = karplus_strong(73.416, 0.85, SAMPLE_RATE_3DS, decay=0.9955, brightness=0.28, seed=303)

    write_pcm16_bin("romfs/audio/lute_tbl.bin", lute)
    write_pcm16_bin("romfs/audio/harp_tbl.bin", harp)
    write_pcm16_bin("romfs/audio/bass_tbl.bin", bass)


def generate_banner_wav() -> None:
    os.makedirs("gfx", exist_ok=True)
    # 1.8 second D minor / Dorian lute strum: D3, A3, F4, A4
    notes = [
        (146.83, 0.00),
        (220.00, 0.07),
        (349.23, 0.14),
        (440.00, 0.21),
    ]
    total_len = int(1.8 * BANNER_SR)
    mix = [0.0] * total_len
    for idx, (freq, delay) in enumerate(notes):
        tone = karplus_strong(freq, 1.6, BANNER_SR, decay=0.996, brightness=0.55, seed=500 + idx)
        offset = int(delay * BANNER_SR)
        for i, val in enumerate(tone):
            if offset + i < total_len:
                mix[offset + i] += val * 0.35

    peak = max(abs(x) for x in mix) or 1.0
    with wave.open("gfx/banner.wav", "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(BANNER_SR)
        frames = bytearray()
        for s in mix:
            v = max(-1.0, min(1.0, (s / peak) * 0.85))
            frames.extend(struct.pack("<h", int(round(v * 32767.0))))
        wf.writeframes(bytes(frames))


def draw_lute_emblem(draw, cx: int, cy: int, scale: float) -> None:
    # Warm wooden lute body (teardrop / pear shape)
    bw = int(14 * scale)
    bh = int(16 * scale)
    draw.ellipse([cx - bw, cy - bh + int(5 * scale), cx + bw, cy + bh + int(5 * scale)],
                 fill=(181, 101, 45), outline=(240, 200, 120), width=max(1, int(scale)))
    # Soundhole
    sr = int(4 * scale)
    draw.ellipse([cx - sr, cy + int(2 * scale) - sr, cx + sr, cy + int(2 * scale) + sr],
                 fill=(42, 24, 16), outline=(230, 190, 110), width=1)
    # Neck
    nw = max(2, int(2.5 * scale))
    draw.rectangle([cx - nw, cy - int(20 * scale), cx + nw, cy - int(6 * scale)],
                   fill=(120, 68, 32), outline=(230, 190, 110))
    # Peghead
    draw.rectangle([cx - nw - 2, cy - int(24 * scale), cx + nw + 2, cy - int(19 * scale)],
                   fill=(90, 48, 22), outline=(240, 200, 120))
    # 4 Strings
    for s in (-1.5, -0.5, 0.5, 1.5):
        sx = int(cx + s * max(1, scale * 1.1))
        draw.line([(sx, cy - int(19 * scale)), (sx, cy + int(14 * scale))],
                  fill=(245, 228, 175), width=1)


def generate_images() -> None:
    from PIL import Image, ImageDraw
    os.makedirs("gfx", exist_ok=True)

    # 1. icon.png (48x48)
    icon = Image.new("RGBA", (48, 48), (28, 20, 16, 255))
    d = ImageDraw.Draw(icon)
    d.rounded_rectangle([2, 2, 45, 45], radius=8, fill=(38, 26, 20, 255), outline=(224, 178, 92, 255), width=2)
    draw_lute_emblem(d, 24, 26, 0.95)
    icon.save("icon.png")

    # 2. gfx/banner.png (256x128)
    banner = Image.new("RGBA", (256, 128), (24, 17, 14, 255))
    bd = ImageDraw.Draw(banner)
    bd.rounded_rectangle([4, 4, 251, 123], radius=12, fill=(36, 24, 18, 255), outline=(224, 178, 92, 255), width=3)
    # Subtle musical staff lines
    for y in (42, 52, 62, 72, 82):
        bd.line([(16, y), (240, y)], fill=(75, 55, 40, 255), width=1)
    # Step sequencer blocks at bottom of banner
    colors = [(217, 83, 79), (240, 173, 78), (92, 184, 92), (91, 192, 222)]
    for step in range(12):
        x = 92 + step * 12
        y = 94
        c = colors[step % 4] if step % 3 == 0 else (68, 50, 38)
        bd.rounded_rectangle([x, y, x + 9, y + 14], radius=2, fill=c)
    draw_lute_emblem(bd, 52, 64, 1.9)
    # Pixel-crisp title text blocks
    bd.text((94, 26), "BARD BEATS", fill=(244, 214, 138, 255))
    bd.text((94, 46), "3DS LOOP MAKER & SYNTH", fill=(195, 170, 128, 255))
    banner.save("gfx/banner.png")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--audio-only", action="store_true")
    args = parser.parse_args()

    generate_audio_tables()
    if not args.audio_only:
        generate_banner_wav()
        generate_images()
    print("Generated Bard Beats assets successfully.")


if __name__ == "__main__":
    main()
