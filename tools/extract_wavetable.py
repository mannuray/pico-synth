#!/usr/bin/env python3
"""
Wavetable extraction tool for the Pico Synth.

Extracts multi-frame single-cycle wavetables from WAV recordings of single notes.
Outputs C header arrays ready to embed in the firmware.

Usage:
  python3 extract_wavetable.py input.wav --name Piano --table-size 512 --frames 4
  python3 extract_wavetable.py --generate-all --table-size 512 --frames 4

The --generate-all mode creates synthetic wavetables using spectral models
derived from real instrument analysis (no WAV files needed).
"""

import argparse
import wave
import struct
import sys
import os
import numpy as np


def load_wav(path):
    """Load a WAV file and return (sample_rate, samples_as_float_array)."""
    with wave.open(path, 'rb') as wf:
        n_channels = wf.getnchannels()
        sample_width = wf.getsampwidth()
        sample_rate = wf.getframerate()
        n_frames = wf.getnframes()
        raw = wf.readframes(n_frames)

    if sample_width == 2:
        fmt = f'<{n_frames * n_channels}h'
        samples = np.array(struct.unpack(fmt, raw), dtype=np.float64)
        samples /= 32768.0
    elif sample_width == 1:
        samples = np.frombuffer(raw, dtype=np.uint8).astype(np.float64)
        samples = (samples - 128.0) / 128.0
    elif sample_width == 3:
        # 24-bit: unpack manually
        samples = []
        for i in range(0, len(raw), 3):
            val = int.from_bytes(raw[i:i+3], byteorder='little', signed=True)
            samples.append(val / 8388608.0)
        samples = np.array(samples)
    else:
        raise ValueError(f"Unsupported sample width: {sample_width}")

    # Convert to mono if stereo
    if n_channels > 1:
        samples = samples.reshape(-1, n_channels).mean(axis=1)

    return sample_rate, samples


def find_zero_crossings(samples):
    """Find positive-going zero crossings in the signal."""
    crossings = []
    for i in range(1, len(samples)):
        if samples[i - 1] <= 0 and samples[i] > 0:
            crossings.append(i)
    return crossings


def extract_single_cycle(samples, sample_rate, fundamental_hz, start_sample):
    """Extract one cycle of the waveform at the given position."""
    cycle_length = sample_rate / fundamental_hz
    start = int(start_sample)
    end = int(start + cycle_length)
    if end > len(samples):
        end = len(samples)
    return samples[start:end]


def resample_cycle(cycle, target_size):
    """Resample a single cycle to exactly target_size samples."""
    x_old = np.linspace(0, 1, len(cycle), endpoint=False)
    x_new = np.linspace(0, 1, target_size, endpoint=False)
    return np.interp(x_new, x_old, cycle)


def extract_frames(samples, sample_rate, fundamental_hz, num_frames, table_size):
    """Extract multiple frames at different time positions in the recording."""
    total_samples = len(samples)

    # Find the stable region (skip first 5% for attack noise, last 10% for tail)
    start_region = int(total_samples * 0.02)
    end_region = int(total_samples * 0.90)

    # Distribute frame extraction points across the recording
    positions = np.linspace(start_region, end_region, num_frames).astype(int)

    frames = []
    cycle_len = int(sample_rate / fundamental_hz)

    for pos in positions:
        # Find nearest zero crossing to get a clean cycle
        search_start = max(0, pos - cycle_len)
        search_end = min(total_samples - cycle_len, pos + cycle_len)
        region = samples[search_start:search_end]
        crossings = find_zero_crossings(region)

        if crossings:
            # Use the crossing closest to our target position
            best = min(crossings, key=lambda c: abs(c + search_start - pos))
            cycle_start = search_start + best
        else:
            cycle_start = pos

        cycle = extract_single_cycle(samples, sample_rate, fundamental_hz, cycle_start)
        resampled = resample_cycle(cycle, table_size)

        # Normalize to [-1, 1]
        peak = np.max(np.abs(resampled))
        if peak > 0:
            resampled /= peak

        frames.append(resampled)

    return frames


def generate_synthetic_instrument(name, table_size, num_frames):
    """Generate synthetic wavetable frames using spectral models of real instruments."""
    t = np.linspace(0, 2 * np.pi, table_size, endpoint=False)

    # Per-frame brightness rolloff (frame 0 = bright attack, frame N-1 = dark sustain)
    rolloffs = np.linspace(1.0, 0.15, num_frames)

    # Instrument-specific spectral models based on acoustic research
    instruments = {
        'GrandPno': {
            'harmonics': [
                (1, 1.00, 0.0),   # fundamental
                (2, 0.62, 0.08),  # 2nd with slight phase
                (3, 0.38, 0.15),  # 3rd
                (4, 0.22, 0.05),  # 4th (piano inharmonicity: slight sharp)
                (5, 0.14, 0.0),
                (6, 0.09, 0.12),
                (7, 0.05, 0.0),
                (8, 0.03, 0.0),
            ],
            'noise': 0.02,  # hammer noise
            'inharmonicity': 0.0003,
        },
        'Violin': {
            'harmonics': [
                (1, 0.80, 0.0),
                (2, 1.00, 0.0),   # violin 2nd harmonic is often strongest
                (3, 0.72, 0.0),
                (4, 0.48, 0.0),
                (5, 0.36, 0.0),
                (6, 0.20, 0.0),
                (7, 0.14, 0.0),
                (8, 0.08, 0.0),
                (9, 0.05, 0.0),
                (10, 0.03, 0.0),
            ],
            'noise': 0.015,  # bow noise
            'inharmonicity': 0.0,
            'formant_hz': [300, 1200, 2700],  # violin body resonances
            'formant_amp': [0.08, 0.05, 0.03],
        },
        'NylonGtr': {
            'harmonics': [
                (1, 1.00, 0.0),
                (2, 0.58, 0.1),
                (3, 0.30, 0.0),
                (4, 0.18, 0.05),
                (5, 0.12, 0.0),
                (6, 0.06, 0.0),
            ],
            'noise': 0.03,  # finger/nail noise
            'inharmonicity': 0.0001,
        },
        'Flute': {
            'harmonics': [
                (1, 1.00, 0.0),
                (2, 0.25, 0.0),   # flute has weak upper harmonics
                (3, 0.08, 0.0),
                (4, 0.03, 0.0),
            ],
            'noise': 0.06,  # breath noise (significant for flute)
            'inharmonicity': 0.0,
        },
        'Trumpet': {
            'harmonics': [
                (1, 0.85, 0.0),
                (2, 1.00, 0.0),
                (3, 0.90, 0.0),   # trumpet has strong mid harmonics
                (4, 0.70, 0.0),
                (5, 0.50, 0.0),
                (6, 0.30, 0.0),
                (7, 0.18, 0.0),
                (8, 0.10, 0.0),
            ],
            'noise': 0.01,
            'inharmonicity': 0.0,
            'formant_hz': [1200, 2500],  # mute resonance
            'formant_amp': [0.06, 0.04],
        },
        'Harmonica': {
            'harmonics': [
                (1, 1.00, 0.0),
                (2, 0.45, 0.0),
                (3, 0.60, 0.0),   # odd harmonics stronger (reed instrument)
                (4, 0.20, 0.0),
                (5, 0.40, 0.0),
                (6, 0.10, 0.0),
                (7, 0.25, 0.0),
            ],
            'noise': 0.04,  # breath/reed noise
            'inharmonicity': 0.0,
        },
        'Cello': {
            'harmonics': [
                (1, 1.00, 0.0),
                (2, 0.80, 0.0),
                (3, 0.55, 0.0),
                (4, 0.35, 0.0),
                (5, 0.22, 0.0),
                (6, 0.14, 0.0),
                (7, 0.08, 0.0),
                (8, 0.05, 0.0),
            ],
            'noise': 0.02,
            'inharmonicity': 0.0,
            'formant_hz': [250, 600],  # cello body resonances
            'formant_amp': [0.06, 0.04],
        },
        'Organ': {
            'harmonics': [
                (1, 1.00, 0.0),
                (2, 0.80, 0.0),
                (3, 0.60, 0.0),
                (4, 0.40, 0.0),
                (5, 0.30, 0.0),
                (6, 0.20, 0.0),
            ],
            'noise': 0.0,
            'inharmonicity': 0.0,
        },
        'Choir': {
            'harmonics': [
                (1, 0.95, 0.0),
                (2, 0.42, 0.25),
                (3, 0.33, 0.0),
                (4, 0.10, 0.0),
                (5, 0.18, 0.15),
            ],
            'noise': 0.025,
            'inharmonicity': 0.0,
            'formant_hz': [500, 1500, 2500],  # vocal formants (ah vowel)
            'formant_amp': [0.10, 0.06, 0.03],
        },
        'SoftStr': {
            'harmonics': [
                (1, 0.72, 0.0),
                (2, 0.46, 0.0),
                (3, 0.23, 0.0),
                (4, 0.11, 0.0),
            ],
            'noise': 0.01,
            'inharmonicity': 0.0,
        },
        'Reed': {
            'harmonics': [
                (1, 1.00, 0.0),
                (3, 0.58, 0.0),
                (5, 0.30, 0.0),
                (7, 0.16, 0.0),
                (9, 0.08, 0.0),
            ],
            'noise': 0.02,
            'inharmonicity': 0.0,
        },
        'AccordionReed': {
            'harmonics': [
                (1, 1.00, 0.0),
                (2, 0.50, 0.0),
                (3, 0.33, 0.0),
                (4, 0.25, 0.0),
                (5, 0.20, 0.0),
                (6, 0.15, 0.0),
                (7, 0.10, 0.0),
            ],
            'noise': 0.03,
            'inharmonicity': 0.0,
        },
        'PipeOrgan': {
            'harmonics': [
                (1, 0.82, 0.0),
                (2, 0.52, 0.0),
                (3, 0.28, 0.0),
                (4, 0.34, 0.0),
                (5, 0.14, 0.0),
                (6, 0.18, 0.0),
                (7, 0.08, 0.0),
                (8, 0.09, 0.0),
            ],
            'noise': 0.005,
            'inharmonicity': 0.0,
        },
        'SoftLead': {
            'harmonics': [
                (1, 0.55, 0.0),
                (2, 0.18, 0.0),
            ],
            'noise': 0.0,
            'inharmonicity': 0.0,
            'pulse_width': 0.38,  # pulse-saw hybrid
        },
    }

    if name not in instruments:
        print(f"Unknown instrument: {name}. Available: {', '.join(instruments.keys())}")
        return None

    spec = instruments[name]
    frames = []

    for frame_idx in range(num_frames):
        r = rolloffs[frame_idx]
        wave_data = np.zeros(table_size)

        # Additive harmonics with per-frame rolloff and inharmonicity
        for h_num, amp, phase_offset in spec['harmonics']:
            # Inharmonicity: upper partials slightly sharper
            inharm = 1.0 + spec.get('inharmonicity', 0) * h_num * h_num
            rolloff_factor = r ** (h_num - 1)  # fundamental unaffected
            wave_data += amp * rolloff_factor * np.sin(t * h_num * inharm + phase_offset)

        # Pulse-saw hybrid (for lead sounds)
        if 'pulse_width' in spec:
            pw = spec['pulse_width']
            pulse = np.where(np.linspace(0, 1, table_size, endpoint=False) < pw, 1.0, -1.0)
            wave_data = 0.55 * r * pulse + 0.45 * wave_data

        # Formant resonances (body/vocal character)
        if 'formant_hz' in spec:
            for f_hz, f_amp in zip(spec['formant_hz'], spec['formant_amp']):
                # Approximate formant as a narrow harmonic boost around the frequency
                # Map formant frequency to harmonic number (assuming ~260Hz fundamental)
                h_approx = f_hz / 260.0
                formant_factor = r ** max(0, h_approx - 1)
                wave_data += f_amp * formant_factor * np.sin(t * h_approx)

        # Noise component (bow scratch, breath, hammer) — stronger in bright frames
        if spec.get('noise', 0) > 0:
            np.random.seed(42 + frame_idx)  # reproducible per frame
            noise = np.random.randn(table_size) * spec['noise'] * (0.3 + 0.7 * r)
            # Low-pass the noise slightly so it doesn't dominate
            noise = np.convolve(noise, np.ones(3) / 3, mode='same')
            wave_data += noise

        # Normalize
        peak = np.max(np.abs(wave_data))
        if peak > 0:
            wave_data /= peak

        frames.append(wave_data)

    return frames


def frames_to_int16(frames):
    """Convert float frames to int16 arrays."""
    result = []
    for frame in frames:
        int16_data = np.clip(frame * 32767.0, -32768, 32767).astype(np.int16)
        result.append(int16_data)
    return result


def generate_c_header(tables, table_names, table_size, num_frames):
    """Generate a C header file with the wavetable data."""
    lines = [
        "// Auto-generated wavetable data — do not edit manually.",
        f"// {len(tables)} instruments, {num_frames} frames each, {table_size} samples per frame.",
        f"// Generated by tools/extract_wavetable.py",
        "",
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"static constexpr size_t kGenWaveTableSize = {table_size};",
        f"static constexpr uint8_t kGenFrameCount = {num_frames};",
        f"static constexpr uint8_t kGenTableCount = {len(tables)};",
        "",
    ]

    for idx, (name, frames) in enumerate(zip(table_names, tables)):
        lines.append(f"// WT {idx}: {name}")
        for f_idx, frame in enumerate(frames):
            arr_name = f"kWT_{name}_f{f_idx}"
            values = ', '.join(str(s) for s in frame)
            lines.append(f"static const int16_t {arr_name}[{table_size}] = {{{values}}};")
        lines.append("")

    # Index table
    lines.append("// Frame pointers indexed as kWaveTableData[table][frame]")
    lines.append(f"static const int16_t* const kWaveTableData[{len(tables)}][{num_frames}] = {{")
    for idx, (name, frames) in enumerate(zip(table_names, tables)):
        ptrs = ', '.join(f"kWT_{name}_f{f}" for f in range(num_frames))
        lines.append(f"  {{{ptrs}}},  // {idx}: {name}")
    lines.append("};")
    lines.append("")

    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description='Wavetable extraction tool for Pico Synth')
    parser.add_argument('input', nargs='?', help='Input WAV file')
    parser.add_argument('--name', default='Instrument', help='Instrument name for the output')
    parser.add_argument('--fundamental', type=float, default=0,
                        help='Fundamental frequency in Hz (auto-detected if 0)')
    parser.add_argument('--table-size', type=int, default=512, help='Samples per wavetable frame')
    parser.add_argument('--frames', type=int, default=4, help='Number of frames to extract')
    parser.add_argument('--output', '-o', help='Output C header file')
    parser.add_argument('--generate-all', action='store_true',
                        help='Generate all synthetic instrument tables')
    args = parser.parse_args()

    if args.generate_all:
        # Generate synthetic wavetables for all instruments
        instrument_names = [
            'GrandPno', 'Violin', 'NylonGtr', 'Flute',
            'Trumpet', 'Harmonica', 'Cello', 'Organ',
            'Choir', 'SoftStr', 'Reed', 'AccordionReed',
            'PipeOrgan', 'SoftLead',
        ]

        all_tables = []
        valid_names = []

        for name in instrument_names:
            frames = generate_synthetic_instrument(name, args.table_size, args.frames)
            if frames is not None:
                int16_frames = frames_to_int16(frames)
                all_tables.append(int16_frames)
                valid_names.append(name)
                print(f"  Generated {name}: {args.frames} frames x {args.table_size} samples")

        header = generate_c_header(all_tables, valid_names, args.table_size, args.frames)
        out_path = args.output or os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            '..', 'arduino', 'synth_v1', 'WaveTableData.h'
        )
        with open(out_path, 'w') as f:
            f.write(header)
        print(f"\nWrote {len(all_tables)} tables to {out_path}")
        total_bytes = len(all_tables) * args.frames * args.table_size * 2
        print(f"Total data: {total_bytes:,} bytes ({total_bytes/1024:.1f} KB)")

    elif args.input:
        print(f"Loading {args.input}...")
        sample_rate, samples = load_wav(args.input)
        print(f"  {sample_rate} Hz, {len(samples)} samples ({len(samples)/sample_rate:.2f}s)")

        # Auto-detect fundamental if not specified
        fundamental = args.fundamental
        if fundamental <= 0:
            # Simple autocorrelation pitch detection
            chunk = samples[int(len(samples)*0.1):int(len(samples)*0.4)]
            corr = np.correlate(chunk[:4096], chunk[:4096], mode='full')
            corr = corr[len(corr)//2:]
            # Find first peak after zero crossing
            d = np.diff(corr)
            peaks = []
            for i in range(20, len(d)-1):
                if d[i-1] > 0 and d[i] <= 0:
                    peaks.append(i)
                    break
            if peaks:
                fundamental = sample_rate / peaks[0]
            else:
                fundamental = 261.63  # default to middle C
            print(f"  Detected fundamental: {fundamental:.1f} Hz")

        frames = extract_frames(samples, sample_rate, fundamental, args.frames, args.table_size)
        int16_frames = frames_to_int16(frames)

        header = generate_c_header([int16_frames], [args.name], args.table_size, args.frames)
        out_path = args.output or f"{args.name}_wavetable.h"
        with open(out_path, 'w') as f:
            f.write(header)
        print(f"Wrote to {out_path}")
    else:
        parser.print_help()


if __name__ == '__main__':
    main()
