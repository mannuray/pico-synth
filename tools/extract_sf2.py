#!/usr/bin/env python3
"""
Extract instrument and drum samples from a General MIDI SoundFont (SF2) into
C headers for the Pico Synth.

    python3 tools/extract_sf2.py /usr/share/sounds/sf2/FluidR3_GM.sf2

Writes:
  arduino/synth_v1/SampleData.h   melodic instruments (22.05 kHz mono, looped)
  arduino/synth_v1/DrumSamples.h  drum kit (44.1 kHz mono, one-shot)

The defaults target FluidR3_GM (MIT licence, Frank Wen); see
third_party/FluidR3_GM/LICENSE. Requires numpy.

SF2 rules applied (SoundFont 2.04): instrument global zones are defaults for
the other zones; preset-level generators add to instrument-level ones (ranges
intersect); address offsets have fine and coarse (x32768) parts; sampleModes
1 and 3 loop; root key = overridingRootKey or the sample's originalPitch;
tuning = pitchCorrection + coarseTune + fineTune. Only zones whose velocity
range covers 100 are used (the Pico always plays at velocity 100). Stereo
pairs are mixed to mono.
"""

import math
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(HERE, '..', 'arduino', 'synth_v1')

INSTRUMENT_RATE = 22050   # plenty for a small speaker (content up to ~10 kHz)
DRUM_RATE = 44100         # cymbals need the 6-16 kHz range
VELOCITY = 100

# (name, GM program 0-127, max zones, max seconds per zone). A zone whose own
# loop ends within the limit keeps it; a longer one (the natural decay of a
# plucked or struck note) is cut at the limit and given a loop of whole pitch
# periods there, and the preset's envelope carries on the decay.
INSTRUMENTS = [
    ('Piano',    0,   6, 1.0),
    ('Organ',    16,  1, 1.6),
    ('Violin',   40,  4, 1.6),
    ('Trumpet',  56,  1, 0.5),
    ('Banjo',    105, 5, 0.9),
    ('Bell',     14,  1, 1.5),
    ('MusicBox', 10,  4, 0.9),
    ('Guitar',   24,  5, 1.2),
    # Bank 2
    ('EPiano',     4,   2, 0.6),
    ('Strings',    48,  2, 0.6),
    ('Flute',      73,  2, 0.6),
    ('Sax',        65,  2, 0.6),
    ('Clarinet',   71,  3, 1.0),
    ('Harmonica',  22,  3, 1.0),
    ('Vibraphone', 11,  3, 1.0),
    ('SteelDrum',  114, 3, 1.0),
]
TAIL_LOOP_SECONDS = 0.08
# Retuning in cents. FluidR3's Drawbar Organ sounds two octaves below the key
# (a 16' stop: tune -2400), too low for a small speaker; play it at the key.
# The USB MIDI notes are unaffected, so a computer's FluidSynth plays it lower.
TUNE_CENTS = {'Organ': 2400}
KEY_LOW, KEY_HIGH = 24, 108   # every playable note maps to some zone

# DrumSampleId name -> (GM percussion note, max seconds)
DRUMS = [
    ('Kick',      36, 0.40),
    ('Snare',     38, 0.35),
    ('ClosedHat', 42, 0.20),
    ('OpenHat',   46, 0.50),
    ('Clave',     75, 0.15),
    ('Cowbell',   56, 0.30),
    ('Clap',      39, 0.35),
    ('Rimshot',   37, 0.15),
    ('LowTom',    45, 0.50),
    ('MidTom',    47, 0.45),
    ('HighTom',   50, 0.40),
    ('Crash',     49, 1.00),
]

# Generator numbers (SoundFont 2.04, section 8.1.2)
G_START, G_END, G_LOOP_START, G_LOOP_END = 0, 1, 2, 3
G_START_COARSE, G_END_COARSE = 4, 12
G_LOOP_START_COARSE, G_LOOP_END_COARSE = 45, 50
G_INSTRUMENT, G_KEY_RANGE, G_VEL_RANGE = 41, 43, 44
G_ATTENUATION = 48
G_COARSE_TUNE, G_FINE_TUNE, G_SAMPLE_ID, G_SAMPLE_MODES = 51, 52, 53, 54
G_SCALE_TUNING, G_ROOT_KEY = 56, 58
RANGE_GENS = (G_KEY_RANGE, G_VEL_RANGE)


# ── SF2 parsing ─────────────────────────────────────────────────────────────

class SoundFont:
    def __init__(self, path):
        data = open(path, 'rb').read()
        assert data[0:4] == b'RIFF' and data[8:12] == b'sfbk', 'not an SF2 file'
        self.chunks = {}
        self._walk(data, 12, len(data))
        self.samples = np.frombuffer(self.chunks[b'smpl'], dtype='<i2')
        self.phdr = self._records(b'phdr', '<20sHHHIII')
        self.pbag = self._records(b'pbag', '<HH')
        self.pgen = self._gens(b'pgen')
        self.inst = self._records(b'inst', '<20sH')
        self.ibag = self._records(b'ibag', '<HH')
        self.igen = self._gens(b'igen')
        self.shdr = self._records(b'shdr', '<20sIIIIIBbHH')

    def _walk(self, data, pos, end):
        while pos + 8 <= end:
            cid, size = data[pos:pos + 4], struct.unpack('<I', data[pos + 4:pos + 8])[0]
            body = pos + 8
            if cid == b'LIST':
                self._walk(data, body + 4, body + size)
            else:
                self.chunks[cid] = data[body:body + size]
            pos = body + size + (size & 1)

    def _records(self, cid, fmt):
        raw, size = self.chunks[cid], struct.calcsize(fmt)
        return [struct.unpack(fmt, raw[i:i + size]) for i in range(0, len(raw), size)]

    def _gens(self, cid):
        raw = self.chunks[cid]
        out = []
        for i in range(0, len(raw), 4):
            oper = struct.unpack('<H', raw[i:i + 2])[0]
            if oper in RANGE_GENS:
                out.append((oper, (raw[i + 2], raw[i + 3])))
            else:
                out.append((oper, struct.unpack('<h', raw[i + 2:i + 4])[0]))
        return out

    def _zones(self, bags, gens, first, last):
        zones = []
        for b in range(first, last):
            g0, g1 = bags[b][0], bags[b + 1][0]
            zones.append(dict(gens[g0:g1]))
        return zones

    def preset_zones(self, bank, program):
        for i, h in enumerate(self.phdr[:-1]):
            if h[1] == program and h[2] == bank:
                return self._zones(self.pbag, self.pgen, h[3], self.phdr[i + 1][3])
        raise KeyError(f'no preset bank {bank} program {program}')

    def instrument_zones(self, index):
        return self._zones(self.ibag, self.igen, self.inst[index][1], self.inst[index + 1][1])

    def sample_name(self, sid):
        return self.shdr[sid][0].split(b'\0')[0].decode('latin1')


def intersect(a, b):
    return (max(a[0], b[0]), min(a[1], b[1]))


def resolve(sf, bank, program):
    """All instrument zones of a preset, with preset and instrument generators combined."""
    pzones = sf.preset_zones(bank, program)
    pglobal = {}
    if pzones and G_INSTRUMENT not in pzones[0]:
        pglobal = pzones.pop(0)
    result = []
    for pz in pzones:
        pgen = {**pglobal, **pz}
        izones = sf.instrument_zones(pz[G_INSTRUMENT])
        iglobal = {}
        if izones and G_SAMPLE_ID not in izones[0]:
            iglobal = izones.pop(0)
        for iz in izones:
            gen = {**iglobal, **iz}
            if G_SAMPLE_ID not in gen:
                continue
            keys = gen.get(G_KEY_RANGE, (0, 127))
            vels = gen.get(G_VEL_RANGE, (0, 127))
            if G_KEY_RANGE in pgen:
                keys = intersect(keys, pgen[G_KEY_RANGE])
            if G_VEL_RANGE in pgen:
                vels = intersect(vels, pgen[G_VEL_RANGE])
            if keys[0] > keys[1] or not (vels[0] <= VELOCITY <= vels[1]):
                continue
            # Preset generators are relative: they add to the instrument's.
            for op in (G_COARSE_TUNE, G_FINE_TUNE, G_ATTENUATION):
                if op in pgen:
                    gen[op] = gen.get(op, 0) + pgen[op]
            gen[G_KEY_RANGE] = keys
            result.append(gen)
    return result


def stereo_partner(sf, sid):
    """The other half of a stereo sample, or None. FluidR3 leaves every
    sampleLink at 0 (the "Gun" sample), so fall back to the (L)/(R) name."""
    name, start, end, *_, link, stype = sf.shdr[sid]
    if stype not in (2, 4):
        return None
    length = end - start
    if link < len(sf.shdr) and link != sid:
        other = sf.shdr[link]
        if other[8] == sid and other[2] - other[1] == length:
            return other
    me = sf.sample_name(sid)
    want = me.replace('(L)', '(R)') if '(L)' in me else me.replace('(R)', '(L)')
    for i, other in enumerate(sf.shdr):
        if i != sid and sf.sample_name(i) == want and other[2] - other[1] == length:
            return other
    return None


def zone_audio(sf, gen):
    """Mono float audio plus loop points and pitch info for one zone."""
    sid = gen[G_SAMPLE_ID]
    name, start, end, lstart, lend, rate, pitch, correction, link, stype = sf.shdr[sid]

    def off(fine, coarse):
        return gen.get(fine, 0) + 32768 * gen.get(coarse, 0)

    start += off(G_START, G_START_COARSE)
    end += off(G_END, G_END_COARSE)
    lstart += off(G_LOOP_START, G_LOOP_START_COARSE)
    lend += off(G_LOOP_END, G_LOOP_END_COARSE)
    audio = sf.samples[start:end].astype(np.float64) / 32768.0

    # Stereo pair (1 mono, 2 right, 4 left): mix the other side in.
    other = stereo_partner(sf, sid)
    if other is not None:
        shift = start - sf.shdr[sid][1]   # the zone's sample offsets apply to both sides
        o = sf.samples[other[1] + shift:other[1] + shift + len(audio)].astype(np.float64) / 32768.0
        if len(o) == len(audio):
            audio = (audio + o) * 0.5

    root = gen.get(G_ROOT_KEY, -1)
    if root < 0:
        root = pitch
    tune = correction + 100 * gen.get(G_COARSE_TUNE, 0) + gen.get(G_FINE_TUNE, 0)
    looped = gen.get(G_SAMPLE_MODES, 0) in (1, 3)
    gain = 10 ** (-gen.get(G_ATTENUATION, 0) / 200.0)   # attenuation is in centibels
    return dict(name=sf.sample_name(sid), audio=audio * gain, rate=rate, root=root, tune=tune,
                looped=looped, loop=(lstart - start, lend - start), keys=gen[G_KEY_RANGE],
                scale_tuning=gen.get(G_SCALE_TUNING, 100))


# ── DSP helpers ─────────────────────────────────────────────────────────────

def resample(x, ratio):
    """Band-limited resample by `ratio` (out = in * ratio samples)."""
    if abs(ratio - 1.0) < 1e-9:
        return x.copy()
    cutoff = min(1.0, ratio) * 0.45            # fraction of the input rate
    taps = 63
    n = np.arange(taps) - (taps - 1) / 2
    h = np.sinc(2 * cutoff * n) * np.hamming(taps)
    h /= h.sum()
    filtered = np.convolve(x, h, mode='same')
    out_len = int(math.floor(len(x) * ratio))
    pos = np.arange(out_len) / ratio
    return np.interp(pos, np.arange(len(x)), filtered)


def crossfade_loop(x, loop_start, loop_end, n=64):
    """Blend the end of the loop toward the audio before its start, so the
    jump from loop_end back to loop_start is seamless."""
    n = min(n, loop_start, loop_end - loop_start)
    if n < 4:
        return x
    w = np.linspace(0, 1, n)
    x[loop_end - n:loop_end] = x[loop_end - n:loop_end] * (1 - w) + x[loop_start - n:loop_start] * w
    return x


def fade_out(x, fraction=0.15):
    n = max(1, int(len(x) * fraction))
    x[-n:] *= np.linspace(1, 0, n)
    return x


def to_int16(x):
    return np.clip(np.round(x * 32767), -32768, 32767).astype(np.int16)


# ── Instruments ─────────────────────────────────────────────────────────────

def pick_zones(zones, max_zones):
    """Up to max_zones zones, by root key. With many zones, spread evenly
    across all of them; with few (3 or less), the ones nearest evenly spaced
    points over the keyboard's range (C2-F#6 inner points), so two zones don't end
    up at the extremes (e.g. Rhodes C2 and C8)."""
    unique = {}
    for z in zones:
        unique.setdefault(z['root'], z)
    by_root = sorted(unique.values(), key=lambda z: z['root'])
    if len(by_root) <= max_zones:
        return by_root
    if max_zones > 3:
        idx = np.round(np.linspace(0, len(by_root) - 1, max_zones)).astype(int)
        return [by_root[i] for i in sorted(set(idx))]
    picked = []
    for target in np.linspace(36, 90, max_zones + 2)[1:-1]:
        rest = [z for z in by_root if z not in picked]
        picked.append(min(rest, key=lambda z: abs(z['root'] - target)))
    return sorted(picked, key=lambda z: z['root'])


def prepare_instrument(sf, program, max_zones, max_seconds, tune_cents=0):
    zones = pick_zones([zone_audio(sf, g) for g in resolve(sf, 0, program)], max_zones)
    out = []
    for z in zones:
        z['tune'] += tune_cents
        ratio = INSTRUMENT_RATE / z['rate']
        audio = resample(z['audio'], ratio)
        ls, le = int(round(z['loop'][0] * ratio)), int(round(z['loop'][1] * ratio))
        max_len = int(max_seconds * INSTRUMENT_RATE)
        if z['looped'] and 0 < ls < le <= min(len(audio), max_len) and le - ls >= 16:
            # The recording's own loop fits: keep it.
            audio = crossfade_loop(audio, ls, le)[:le]
        else:
            # Cut at the limit and loop the last whole pitch periods there.
            cut = min(len(audio), max_len)
            period = INSTRUMENT_RATE / (440.0 * 2 ** ((z['root'] - z['tune'] / 100.0 - 69) / 12.0))
            periods = max(1, round(TAIL_LOOP_SECONDS * INSTRUMENT_RATE / period))
            length = int(round(periods * period))
            le, ls = cut, cut - length
            audio = crossfade_loop(audio[:cut].copy(), ls, le, n=length // 2)
        out.append(dict(z, audio=audio, loop=(ls, le), looped=True))
    # Key ranges: each zone covers from the midpoint to its neighbours.
    for i, z in enumerate(out):
        lo = KEY_LOW if i == 0 else (out[i - 1]['root'] + z['root']) // 2 + 1
        hi = KEY_HIGH if i == len(out) - 1 else (z['root'] + out[i + 1]['root']) // 2
        z['keys'] = (lo, hi)
    # Normalise the instrument as a whole (zones keep their relative levels).
    peak = max(np.max(np.abs(z['audio'])) for z in out) or 1.0
    for z in out:
        z['audio'] = z['audio'] * (0.95 / peak)
    return out


# ── Drums ───────────────────────────────────────────────────────────────────

def prepare_drum(sf, note, max_seconds):
    zones = [g for g in resolve(sf, 128, 0) if g[G_KEY_RANGE][0] <= note <= g[G_KEY_RANGE][1]]
    if not zones:
        raise KeyError(f'no drum zone for note {note}')
    z = zone_audio(sf, zones[0])
    # Drum zones usually don't track pitch (scaleTuning 0); otherwise transpose.
    semis = (note - z['root']) * z['scale_tuning'] / 100.0 + z['tune'] / 100.0
    ratio = DRUM_RATE / z['rate'] / (2 ** (semis / 12.0))
    audio = resample(z['audio'], ratio)[:int(max_seconds * DRUM_RATE)]
    audio = fade_out(audio, 0.2)
    peak = np.max(np.abs(audio)) or 1.0
    return z['name'], audio * (0.95 / peak)


# ── Output ──────────────────────────────────────────────────────────────────

NOTICE = '''// Samples from the FluidR3_GM SoundFont, Copyright (c) 2000-2002, 2008 Frank Wen,
// released under the MIT licence (full text: third_party/FluidR3_GM/LICENSE).'''


def c_array(name, data):
    values = ', '.join(str(int(v)) for v in data)
    return f'static const int16_t {name}[] = {{{values}}};'


def write_instruments(instruments):
    lines = [
        '// Auto-generated by tools/extract_sf2.py -- do not edit manually.',
        NOTICE,
        f'// {len(instruments)} instruments, mono 16-bit at {INSTRUMENT_RATE} Hz.',
        '',
        '#pragma once',
        '#include <stdint.h>',
        '',
        f'static constexpr uint32_t kSampleDataRate = {INSTRUMENT_RATE};',
        '',
    ]
    total = 0
    for name, zones in instruments:
        for i, z in enumerate(zones):
            # One extra sample after the end so interpolation can read idx + 1.
            data = to_int16(np.append(z['audio'], z['audio'][z['loop'][0]] if z['looped'] else 0.0))
            total += len(data) * 2
            lines.append(f'// {name} zone {i}: "{z["name"]}" keys {z["keys"][0]}-{z["keys"][1]}, root {z["root"]}')
            lines.append(c_array(f'kSmp_{name}_{i}', data))
        lines.append(f'static const SampleZone kZones_{name}[] = {{')
        for i, z in enumerate(zones):
            lines.append(f'  {{kSmp_{name}_{i}, {len(z["audio"])}, {z["loop"][0]}, {z["loop"][1]}, '
                         f'{z["keys"][0]}, {z["keys"][1]}, {z["root"]}, {z["tune"]}, '
                         f'{"true" if z["looped"] else "false"}}},')
        lines.append('};')
        lines.append('')
    lines.append(f'static const SampleSet kSampleSets[{len(instruments)}] = {{')
    for name, zones in instruments:
        lines.append(f'  {{kZones_{name}, {len(zones)}}},')
    lines.append('};')
    path = os.path.join(OUT_DIR, 'SampleData.h')
    open(path, 'w').write('\n'.join(lines) + '\n')
    print(f'Wrote {path} ({total / 1024:.0f} KB of samples)')


def write_drums(drums):
    lines = [
        '// Auto-generated by tools/extract_sf2.py -- do not edit manually.',
        NOTICE,
        f'// {len(drums)} drum samples, mono 16-bit at {DRUM_RATE} Hz.',
        '// (tools/generate_drums.py can still write a synthesized kit instead.)',
        '',
        '#pragma once',
        '#include <stdint.h>',
        '#include <stddef.h>',
        '',
    ]
    total = 0
    for name, (sample_name, audio) in drums:
        data = to_int16(audio)
        total += len(data) * 2
        lines.append(f'// {name}: "{sample_name}"')
        lines.append(c_array(f'kDrum_{name}', data))
        lines.append(f'static constexpr size_t kDrum_{name}_len = {len(data)};')
        lines.append('')
    lines += ['struct DrumSample {', '  const int16_t* data;', '  size_t length;', '};', '',
              f'static constexpr size_t kDrumSampleCount = {len(drums)};',
              f'static const DrumSample kDrumSamples[{len(drums)}] = {{']
    for name, _ in drums:
        lines.append(f'  {{kDrum_{name}, kDrum_{name}_len}},  // {name}')
    lines += ['};', '', 'enum DrumSampleId : uint8_t {']
    for i, (name, _) in enumerate(drums):
        lines.append(f'  kDrumId_{name} = {i},')
    lines.append('};')
    path = os.path.join(OUT_DIR, 'DrumSamples.h')
    open(path, 'w').write('\n'.join(lines) + '\n')
    print(f'Wrote {path} ({total / 1024:.0f} KB of samples)')


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sf = SoundFont(sys.argv[1])
    instruments = []
    for name, program, max_zones, max_seconds in INSTRUMENTS:
        zones = prepare_instrument(sf, program, max_zones, max_seconds, TUNE_CENTS.get(name, 0))
        instruments.append((name, zones))
        print(f'{name:9} program {program:3}: ' + ', '.join(
            f'"{z["name"]}" {z["keys"][0]}-{z["keys"][1]} root {z["root"]} '
            f'{"loop" if z["looped"] else "one-shot"} {len(z["audio"]) / INSTRUMENT_RATE:.2f}s' for z in zones))
    drums = []
    for name, note, max_seconds in DRUMS:
        sample_name, audio = prepare_drum(sf, note, max_seconds)
        drums.append((name, (sample_name, audio)))
        print(f'drum {name:9} note {note}: "{sample_name}" {len(audio) / DRUM_RATE:.2f}s')
    write_instruments(instruments)
    write_drums(drums)


if __name__ == '__main__':
    main()
