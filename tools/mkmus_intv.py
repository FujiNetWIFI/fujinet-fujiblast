#!/usr/bin/env python3
"""mkmus_intv.py -- FujiBlast's in-play music (music/play.txt, tools/mus.py)
for the Intellivision's PSG (src/intv/sound.bas): ch1 on voice A, ch2 on voice
C. The Atari's converter (tools/mkmus_atari.py) is the model: each channel of
a pattern is a subroutine (the same data once), each channel's order a list
of calls.

A step is three decles (every value under 1024: a cartridge ROM is 10 bits):

    period  volume+noise  jiffies   jiffies 1-252
    lo      hi            253       call (one level): the step at lo + 256 hi
    0       0             254       jump (the same)
    0       0             0         return

  period   an index into the period tables (0 is silent)
  volume   bits 0-3 the PSG volume; bits 4-8 the noise period (0: no noise,
           a tone: 1-31, the voice is a noise drum, its tone off)

The PSG's tone is f = clock / (16 P): 111861 / P at an NTSC console's
1.79 MHz, 125000 / P at PAL's 2 MHz. Both tables are written, a pitch's
12-bit period split in a low and a high table (8 and 4 bits).

usage: mkmus_intv.py music/play.txt OUTDIR     (writes OUTDIR/music.bas)"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mus  # noqa: E402

NTSC, PAL = 111861.0, 125000.0
ACLOCK = 63921.0            # the Atari's, whose steps the sweeps and slides copy
CALL, JUMP, RET = 253, 254, 0
MAXN = 252

# noise periods of the drums (the PSG's, 1-31; lower is brighter)
NOISE = {"S": 10, "H": 3}


def cents(f, want):
    return 1200 * math.log2(f / want)


def pure(note):
    """the Atari's AUDF of a pure tone (the slides are shaped in them)"""
    return round(ACLOCK / (2 * mus.hz(note))) - 1


def vol(k, steps):
    """the volume k jiffies in: steps is ((until, volume), ...)"""
    for until, v in steps:
        if k < until:
            return v
    return steps[-1][1]


def psgvol(v):
    """the Atari's volume (0-15) as the PSG's, whose steps are 3 dB"""
    return 0 if v == 0 else min(15, v + 4)


def audf_hz(a):
    return ACLOCK / (2 * (a + 1))


def voice(ev, ch):
    """(Hz or None, volume, noise period) for one jiffy's event"""
    if ev is None:
        return (None, 0, 0)
    kind, note, k = ev
    if kind == "K":                     # a falling thump
        return ((150.0, 15, 0), (70.0, 13, 0))[k]
    if kind == "S":
        return (None, psgvol((0xC, 0x6, 0x6)[k]), NOISE["S"])
    if kind == "H":
        return (None, psgvol(6), NOISE["H"])
    if kind == "bass":
        return (mus.hz(note), psgvol(vol(k, ((2, 10), (999, 6)))), 0)
    if kind == "blip":
        v = vol(k, ((2, 6), (4, 3), (999, 0)))
        return (mus.hz(note), psgvol(v), 0)
    if kind == "tone":
        return (mus.hz(note), psgvol(vol(k, ((2, 6), (999, 4)))), 0)
    if kind in ("arp", "arpmaj", "warble"):
        return (mus.hz(note), psgvol(5), 0)
    k &= ~1                             # slides: a step every other jiffy
    if kind == "sweep":                 # down, fading
        a = min(255, round((pure(note) + 1) * 1.08 ** k) - 1)
        return (audf_hz(a), psgvol(max(1, 6 - k // 4)), 0)
    if kind == "rise":
        a = max(8, round((pure(note) + 1) * 0.96 ** k) - 1)
        return (audf_hz(a), psgvol(vol(k, ((24, 5), (999, 6)))), 0)
    raise SystemExit("no PSG voice for %r" % kind)


def main():
    src, outdir = sys.argv[1], sys.argv[2]
    song = mus.parse(src)
    pitches = {None: 0}                 # Hz -> table index
    subs = [{}, {}]                     # per channel: steps -> label
    order = [[], []]

    def steps(evs, ch):
        out = []
        for hz, v, nz in (voice(e, ch) for e in evs):
            if hz not in pitches:
                pitches[hz] = len(pitches)
            fc = [pitches[hz], v | nz << 4]
            if out and out[-1][:2] == fc and out[-1][2] < MAXN:
                out[-1][2] += 1
            else:
                out.append(fc + [1])
        return out

    for name in song.order:
        lens = set()
        for ch in (0, 1):
            evs = mus.frames(song, name, ch + 1)
            lens.add(len(evs))
            key = tuple(map(tuple, steps(evs, ch)))
            if key not in subs[ch]:
                subs[ch][key] = "m%d_%d" % (ch + 1, len(subs[ch]))
            order[ch].append(subs[ch][key])
        if len(lens) != 1:
            raise SystemExit("%s: pattern %s's channels differ in length" % (src, name))
    if len(pitches) > 255:
        raise SystemExit("%d pitches: the index is a byte" % len(pitches))

    # lay out: loop 1's list, loop 2's, then the subroutines
    data, at = [], {}
    pos = 0
    tops = []
    for ch in (0, 1):
        tops.append(pos)
        pos += 3 * (len(order[ch]) + 1)
    for ch in (0, 1):
        for key, lab in subs[ch].items():
            at[lab] = pos
            pos += 3 * (len(key) + 1)
    for ch in (0, 1):
        for lab in order[ch]:
            data += [at[lab] & 255, at[lab] >> 8, CALL]
        tgt = tops[ch] + 3 * song.loop
        data += [tgt & 255, tgt >> 8, JUMP]
    for ch in (0, 1):
        for key in subs[ch]:
            for p, v, n in key:
                data += [p, v, n]
            data += [0, 0, RET]
    assert len(data) == pos and max(data) < 1024

    inv = sorted(pitches, key=lambda h: pitches[h])
    lo = {"n": [], "p": []}
    hi = {"n": [], "p": []}
    for hz in inv:
        for r, clk in (("n", NTSC), ("p", PAL)):
            p = 1 if hz is None else max(1, min(4095, round(clk / hz)))
            lo[r].append(p & 255)
            hi[r].append(p >> 8)

    def dump(f, name, vals, per=16):
        f.write("%s:\n" % name)
        for i in range(0, len(vals), per):
            f.write("    DATA %s\n" % ", ".join(map(str, vals[i:i + per])))

    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "music.bas"), "w") as f:
        f.write("' GENERATED by tools/mkmus_intv.py from %s: edit that, not this.\n" % src)
        f.write("' Steps of three decles (period index, volume + noise << 4, jiffies;\n"
                "' 253 call, 254 jump, 0 return): src/intv/sound.bas plays them.\n")
        f.write("    CONST MUS_LOOP1 = %d\n    CONST MUS_LOOP2 = %d\n" % (tops[0], tops[1]))
        dump(f, "mus_nlo", lo["n"])
        dump(f, "mus_nhi", hi["n"])
        dump(f, "mus_plo", lo["p"])
        dump(f, "mus_phi", hi["p"])
        dump(f, "mus_d", data, 15)

    jiffies = sum(len(song.patterns[n]) for n in song.order) * song.tempo
    print("music: %d bars, %.1f s a loop at 60 Hz, %d decles, %d pitches, %d+%d subroutines"
          % (len(song.order), jiffies / 60, len(data) + 4 * len(inv), len(inv),
             len(subs[0]), len(subs[1])))
    used = sorted({ev[1] for n in song.order for ev in mus.frames(song, n, 1)
                   if ev and ev[0] == "bass"})
    names = "C C# D Eb E F F# G G# A Bb B".split()
    out = []
    for n in used:
        p = round(NTSC / mus.hz(n))
        out.append("%s%d %d %+.0fc" % (names[n % 12], n // 12 - 1, p, cents(NTSC / p, mus.hz(n))))
    print("bass (NTSC): " + ", ".join(out))


if __name__ == "__main__":
    main()
