#!/usr/bin/env python3
"""
Generate PCM drum samples for the Pico Synth rhythm engine.
Outputs a C header with const int16_t arrays stored in flash.

Uses proper drum synthesis models:
- Kick: layered sine body with pitch envelope + transient click + sub harmonics
- Snare: body tone + noise burst with bandpass character + snare wire
- Hi-hat: ring-modulated metallic tones + filtered noise
- Percussion: clave, cowbell using resonant tone pairs
"""

import math
import struct

SAMPLE_RATE = 44100  # Full rate: cymbals need the 6-16 kHz "sizzle" a 22 kHz render can't hold
OUTPUT_RATE = 44100  # Playback rate of the synth engine


def generate_sine(freq, duration_s, sample_rate, amplitude=1.0):
    n_samples = int(duration_s * sample_rate)
    return [amplitude * math.sin(2 * math.pi * freq * i / sample_rate) for i in range(n_samples)]


def generate_noise(duration_s, sample_rate, amplitude=1.0, seed=42):
    import random
    random.seed(seed)
    n_samples = int(duration_s * sample_rate)
    return [amplitude * (random.random() * 2.0 - 1.0) for _ in range(n_samples)]


def apply_envelope(samples, attack_ms, decay_ms, sample_rate):
    """Simple attack-decay envelope (no sustain)."""
    attack_samples = int(attack_ms * sample_rate / 1000)
    decay_samples = int(decay_ms * sample_rate / 1000)
    total = len(samples)
    result = list(samples)
    for i in range(total):
        if i < attack_samples:
            env = i / max(1, attack_samples)
        elif i < attack_samples + decay_samples:
            t = (i - attack_samples) / max(1, decay_samples)
            env = math.exp(-4.0 * t)  # Exponential decay
        else:
            env = 0.0
        result[i] *= env
    return result


def apply_pitch_envelope(samples, start_freq, end_freq, sweep_ms, sample_rate):
    """Apply pitch sweep to a sine wave."""
    n_samples = len(samples)
    sweep_samples = int(sweep_ms * sample_rate / 1000)
    phase = 0.0
    result = []
    for i in range(n_samples):
        if i < sweep_samples:
            t = i / max(1, sweep_samples)
            freq = start_freq * math.exp(math.log(end_freq / start_freq) * t)
        else:
            freq = end_freq
        phase += 2 * math.pi * freq / sample_rate
        result.append(math.sin(phase) * samples[i] if i < len(samples) else 0.0)
    return result


def low_pass(samples, cutoff_hz, sample_rate):
    """Simple one-pole low pass filter."""
    rc = 1.0 / (2 * math.pi * cutoff_hz)
    dt = 1.0 / sample_rate
    alpha = dt / (rc + dt)
    result = [0.0] * len(samples)
    result[0] = samples[0] * alpha
    for i in range(1, len(samples)):
        result[i] = result[i-1] + alpha * (samples[i] - result[i-1])
    return result


def high_pass(samples, cutoff_hz, sample_rate):
    """Simple one-pole high pass filter."""
    rc = 1.0 / (2 * math.pi * cutoff_hz)
    dt = 1.0 / sample_rate
    alpha = rc / (rc + dt)
    result = [0.0] * len(samples)
    result[0] = samples[0]
    for i in range(1, len(samples)):
        result[i] = alpha * (result[i-1] + samples[i] - samples[i-1])
    return result


def bandpass(samples, low_hz, high_hz, sample_rate):
    return high_pass(low_pass(samples, high_hz, sample_rate), low_hz, sample_rate)


def mix(a, b, ratio=0.5):
    length = max(len(a), len(b))
    result = [0.0] * length
    for i in range(length):
        va = a[i] if i < len(a) else 0.0
        vb = b[i] if i < len(b) else 0.0
        result[i] = va * ratio + vb * (1.0 - ratio)
    return result


def normalize(samples, peak=0.95):
    max_val = max(abs(s) for s in samples) or 1.0
    return [s * peak / max_val for s in samples]


def to_int16(samples):
    return [max(-32768, min(32767, int(s * 32767))) for s in samples]


def resample(samples, from_rate, to_rate):
    """Simple linear interpolation resampling."""
    ratio = from_rate / to_rate
    out_len = int(len(samples) / ratio)
    result = []
    for i in range(out_len):
        src_pos = i * ratio
        idx = int(src_pos)
        frac = src_pos - idx
        if idx + 1 < len(samples):
            val = samples[idx] * (1.0 - frac) + samples[idx + 1] * frac
        elif idx < len(samples):
            val = samples[idx]
        else:
            val = 0.0
        result.append(val)
    return result


# ── Drum synthesis models ──────────────────────────────────────────────────

# Six detuned square waves, the classic analog drum-machine recipe for a
# metallic, inharmonic cymbal tone (frequencies in the spirit of the TR-808).
METAL_FREQS = [205.3, 304.4, 369.6, 522.7, 540.0, 800.0]


def metal_bank(dur, sample_rate, freq_scale=1.0):
    n = int(dur * sample_rate)
    out = [0.0] * n
    for f in METAL_FREQS:
        f *= freq_scale
        for i in range(n):
            out[i] += 1.0 if math.sin(2 * math.pi * f * i / sample_rate) > 0 else -1.0
    return [v / len(METAL_FREQS) for v in out]


def make_hat(dur, decay_ms, seed):
    """Hi-hat: metal bank + noise, high-passed so only the 'chick' and sizzle remain."""
    metal = metal_bank(dur, SAMPLE_RATE)
    noise = generate_noise(dur, SAMPLE_RATE, 1.0, seed)
    result = mix(metal, noise, 0.55)
    for _ in range(3):  # steeper high-pass: keep roughly 6 kHz and up
        result = high_pass(result, 6000, SAMPLE_RATE)
    for _ in range(2):  # roll off the very top so the sizzle sits around 6-12 kHz
        result = low_pass(result, 11000, SAMPLE_RATE)
    result = apply_envelope(result, 0.2, decay_ms, SAMPLE_RATE)
    return normalize(result)

def make_kick():
    """808-style kick: pitched sine body with sweep + click transient."""
    dur = 0.25  # 250ms
    n = int(dur * SAMPLE_RATE)

    # Body: sine with pitch sweep 220Hz → 45Hz over 80ms
    envelope = [1.0] * n
    envelope = apply_envelope(envelope, 0.5, 220, SAMPLE_RATE)
    body = apply_pitch_envelope(envelope, 220, 45, 80, SAMPLE_RATE)

    # Sub body: add second harmonic for small speaker presence
    sub_env = apply_envelope([1.0] * n, 0.5, 180, SAMPLE_RATE)
    sub = apply_pitch_envelope(sub_env, 440, 90, 60, SAMPLE_RATE)
    body = mix(body, sub, 0.7)

    # Click transient: short noise burst, bandpassed
    click = generate_noise(0.008, SAMPLE_RATE, 1.0)
    click = apply_envelope(click, 0.1, 6, SAMPLE_RATE)
    click = bandpass(click, 1000, 6000, SAMPLE_RATE)
    click += [0.0] * (n - len(click))

    result = mix(body, click, 0.75)
    result = low_pass(result, 5000, SAMPLE_RATE)
    return normalize(result)


def make_snare():
    """Snare: body tone + noise burst + snare wire tail."""
    dur = 0.3  # 300ms
    n = int(dur * SAMPLE_RATE)

    # Body: two drum-head modes (~185 Hz and ~330 Hz), fast decay
    body_env = apply_envelope([1.0] * n, 0.3, 60, SAMPLE_RATE)
    body = [body_env[i] * (math.sin(2 * math.pi * 185 * i / SAMPLE_RATE) +
                           0.5 * math.sin(2 * math.pi * 330 * i / SAMPLE_RATE)) for i in range(n)]

    # Noise crack: broadband, sharp attack
    noise = generate_noise(dur, SAMPLE_RATE, 1.0)
    noise = low_pass(low_pass(noise, 7000, SAMPLE_RATE), 7000, SAMPLE_RATE)
    noise = apply_envelope(noise, 0.2, 120, SAMPLE_RATE)

    # Snare wire: bandpassed noise, longer tail
    wire = generate_noise(dur, SAMPLE_RATE, 0.6)
    wire = bandpass(wire, 2000, 7000, SAMPLE_RATE)
    wire = apply_envelope(wire, 5, 260, SAMPLE_RATE)

    # Mix: body for punch, noise for crack, wire for sizzle
    result = [0.0] * n
    for i in range(n):
        result[i] = body[i] * 0.35 + noise[i] * 0.40 + wire[i] * 0.25

    return normalize(result)


def make_closed_hat():
    """Closed hi-hat: short and tight."""
    return make_hat(0.09, 45, 7)


def make_open_hat():
    """Open hi-hat: same metal, a much longer ring (open hats decay several times longer)."""
    return make_hat(0.6, 450, 8)


def make_clave():
    """Clave: short resonant woodblock tone."""
    dur = 0.05  # 50ms
    n = int(dur * SAMPLE_RATE)

    # Two resonant tones
    tone1 = generate_sine(2500, dur, SAMPLE_RATE, 1.0)
    tone2 = generate_sine(3200, dur, SAMPLE_RATE, 0.5)
    tone1 = apply_envelope(tone1, 0.1, 30, SAMPLE_RATE)
    tone2 = apply_envelope(tone2, 0.1, 25, SAMPLE_RATE)

    result = mix(tone1, tone2, 0.6)
    result = bandpass(result, 1500, 6000, SAMPLE_RATE)
    return normalize(result)


def make_cowbell():
    """Cowbell: two detuned tones (minor third)."""
    dur = 0.1  # 100ms
    n = int(dur * SAMPLE_RATE)

    tone1 = generate_sine(540, dur, SAMPLE_RATE, 1.0)
    tone2 = generate_sine(680, dur, SAMPLE_RATE, 0.8)
    tone1 = apply_envelope(tone1, 0.2, 70, SAMPLE_RATE)
    tone2 = apply_envelope(tone2, 0.2, 60, SAMPLE_RATE)

    # Square wave harmonics for brightness
    sq = [0.0] * n
    for i in range(n):
        sq[i] = 0.3 * (1.0 if math.sin(2 * math.pi * 540 * i / SAMPLE_RATE) > 0 else -1.0)
    sq = apply_envelope(sq, 0.2, 50, SAMPLE_RATE)

    result = [tone1[i] * 0.4 + tone2[i] * 0.35 + sq[i] * 0.25 for i in range(n)]
    result = bandpass(result, 400, 4000, SAMPLE_RATE)
    return normalize(result)


def make_clap():
    """Handclap: multiple layered noise bursts for the classic clap sound."""
    dur = 0.25  # 250ms
    n = int(dur * SAMPLE_RATE)
    import random
    random.seed(99)

    result = [0.0] * n

    # 3-4 rapid noise bursts spaced ~8ms apart (the "multiple hands" effect)
    burst_spacing = int(0.008 * SAMPLE_RATE)  # 8ms
    burst_length = int(0.012 * SAMPLE_RATE)    # 12ms each
    for burst in range(4):
        offset = burst * burst_spacing
        for i in range(burst_length):
            if offset + i < n:
                noise = random.random() * 2.0 - 1.0
                env = math.exp(-3.0 * i / burst_length)
                result[offset + i] += noise * env * 0.7

    # Reverb tail: longer filtered noise
    tail = generate_noise(dur, SAMPLE_RATE, 0.5)
    tail = bandpass(tail, 800, 5000, SAMPLE_RATE)
    tail = apply_envelope(tail, 15, 200, SAMPLE_RATE)

    result = [result[i] + tail[i] for i in range(n)]
    result = bandpass(bandpass(result, 700, 4500, SAMPLE_RATE), 700, 4500, SAMPLE_RATE)
    return normalize(result)


def make_rimshot():
    """Rimshot: sharp attack click + short resonant tone."""
    dur = 0.06  # 60ms
    n = int(dur * SAMPLE_RATE)

    # Sharp noise click
    click = generate_noise(0.004, SAMPLE_RATE, 1.0)
    click = apply_envelope(click, 0.05, 3, SAMPLE_RATE)
    click = bandpass(click, 2000, 9000, SAMPLE_RATE)
    click += [0.0] * (n - len(click))

    # Resonant ring at ~1700Hz
    tone = generate_sine(1700, dur, SAMPLE_RATE, 0.8)
    tone = apply_envelope(tone, 0.1, 35, SAMPLE_RATE)

    result = mix(click, tone, 0.55)
    return normalize(result)


def make_tom(start_hz, end_hz, dur, decay_ms):
    """Tom: pitched body with a slight downward bend, an overtone, and a stick click."""
    n = int(dur * SAMPLE_RATE)
    body = apply_pitch_envelope(apply_envelope([1.0] * n, 0.5, decay_ms, SAMPLE_RATE),
                                start_hz, end_hz, 50, SAMPLE_RATE)
    harmonics = apply_pitch_envelope(apply_envelope([1.0] * n, 0.5, decay_ms * 0.6, SAMPLE_RATE),
                                     start_hz * 2, end_hz * 2, 40, SAMPLE_RATE)
    click = generate_noise(0.005, SAMPLE_RATE, 0.6)
    click = apply_envelope(click, 0.1, 4, SAMPLE_RATE)
    click += [0.0] * (n - len(click))
    result = [body[i] * 0.5 + harmonics[i] * 0.3 + click[i] * 0.2 for i in range(n)]
    result = low_pass(result, 4000, SAMPLE_RATE)
    return normalize(result)


def make_low_tom():
    return make_tom(120, 82, 0.35, 280)


def make_mid_tom():
    return make_tom(165, 115, 0.3, 240)


def make_high_tom():
    return make_tom(225, 160, 0.25, 200)


def make_crash():
    """Crash cymbal: metal bank at a few pitches plus noise, bright, long decay."""
    dur = 1.2
    metal = [0.0] * int(dur * SAMPLE_RATE)
    for scale in (1.0, 1.37, 1.83):
        layer = metal_bank(dur, SAMPLE_RATE, scale)
        metal = [a + b for a, b in zip(metal, layer)]
    noise = generate_noise(dur, SAMPLE_RATE, 1.5, 11)
    result = mix(metal, noise, 0.4)
    for _ in range(2):
        result = high_pass(result, 4000, SAMPLE_RATE)
    result = low_pass(result, 12000, SAMPLE_RATE)
    result = apply_envelope(result, 1.0, 1100, SAMPLE_RATE)
    return normalize(result)


def generate_header():
    print("Generating drum samples...")

    drums = {
        'Kick': make_kick(),
        'Snare': make_snare(),
        'ClosedHat': make_closed_hat(),
        'OpenHat': make_open_hat(),
        'Clave': make_clave(),
        'Cowbell': make_cowbell(),
        'Clap': make_clap(),
        'Rimshot': make_rimshot(),
        'LowTom': make_low_tom(),
        'MidTom': make_mid_tom(),
        'HighTom': make_high_tom(),
        'Crash': make_crash(),
    }

    for name in drums:
        if SAMPLE_RATE != OUTPUT_RATE:
            drums[name] = resample(drums[name], SAMPLE_RATE, OUTPUT_RATE)
        print(f"  {name}: {len(drums[name])} samples ({len(drums[name])*2} bytes)")

    total_bytes = sum(len(s) * 2 for s in drums.values())
    print(f"  Total: {total_bytes:,} bytes ({total_bytes/1024:.1f} KB)")

    lines = [
        "// Auto-generated PCM drum samples — do not edit manually.",
        f"// Generated by tools/generate_drums.py",
        f"// {len(drums)} samples at {OUTPUT_RATE}Hz, 16-bit mono.",
        "",
        "#pragma once",
        "#include <stdint.h>",
        "",
    ]

    for name, samples in drums.items():
        int16_data = to_int16(samples)
        values = ', '.join(str(s) for s in int16_data)
        lines.append(f"static const int16_t kDrum_{name}[] = {{{values}}};")
        lines.append(f"static constexpr size_t kDrum_{name}_len = {len(int16_data)};")
        lines.append("")

    # Index structure
    lines.append("struct DrumSample {")
    lines.append("  const int16_t* data;")
    lines.append("  size_t length;")
    lines.append("};")
    lines.append("")
    lines.append(f"static constexpr size_t kDrumSampleCount = {len(drums)};")
    lines.append(f"static const DrumSample kDrumSamples[{len(drums)}] = {{")
    for name in drums:
        lines.append(f"  {{kDrum_{name}, kDrum_{name}_len}},  // {name}")
    lines.append("};")
    lines.append("")

    # Enum for easy access
    lines.append("enum DrumSampleId : uint8_t {")
    for i, name in enumerate(drums):
        lines.append(f"  kDrumId_{name} = {i},")
    lines.append("};")

    import os
    out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            '..', 'arduino', 'synth_v1', 'DrumSamples.h')
    with open(out_path, 'w') as f:
        f.write('\n'.join(lines))
    print(f"\nWrote to {out_path}")


if __name__ == '__main__':
    generate_header()
