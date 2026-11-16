#!/usr/bin/env python3
"""psgdec.py -- check a psglog (smssim.py) against music/play.txt.

usage: psgdec.py music.psg [music/play.txt]

Rebuilds the SN76489's registers frame by frame from the log ("frame data"),
finds where the loop begins (the first bass note on tone 2), and compares
tone 2 / noise (ch1) and tone 0 (ch2) with what tools/mkmus_sms.py would
send for each jiffy. Frames where an effect has the noise or tone 1 are
counted apart (the drums yield). Prints the kick gap and mismatches."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import mus  # noqa: E402
import mkmus_sms as m  # noqa: E402


def regs_by_frame(path):
    reg = {"t": [0, 0, 0], "a": [15, 15, 15, 15], "n": 0}
    latch = None
    frames, last = {}, None
    for line in open(path):
        f, d = line.split()
        f, d = int(f), int(d, 16)
        if d & 0x80:
            latch = (d >> 5) & 3, d & 0x10
            ch, isvol = latch
            if isvol:
                reg["a"][ch] = d & 15
            elif ch == 3:
                reg["n"] = d & 7
            else:
                reg["t"][ch] = reg["t"][ch] & ~15 | d & 15
        elif latch and not latch[1] and latch[0] < 3:
            reg["t"][latch[0]] = reg["t"][latch[0]] & 15 | (d & 0x3F) << 4
        frames[f] = (list(reg["t"]), list(reg["a"]), reg["n"])
    return frames


def main():
    log = sys.argv[1]
    src = sys.argv[2] if len(sys.argv) > 2 else "music/play.txt"
    octaves = int(os.environ.get("OCTAVES", "2"))
    fr = regs_by_frame(log)
    ks = sorted(fr)
    song = mus.parse(src)
    e1, e2 = [], []
    for name in song.order:
        e1 += mus.frames(song, name, 1)
        e2 += mus.frames(song, name, 2)
    n = len(e1)
    loop_from = sum(len(song.patterns[x]) for x in song.order[:song.loop]) * song.tempo

    def exp(i):
        i = i if i < n else loop_from + (i - loop_from) % (n - loop_from)
        return m.ch1(e1[i], octaves), m.ch2(e2[i])

    def state(f):
        while f not in fr and f > ks[0]:
            f -= 1
        return fr[f]

    # the loop starts at the first frame tone 2 is audible with the bass's att
    start = next(f for f in ks if fr[f][1][2] == m.BASS_ATT)
    bad = fx = tot = 0
    kicks = []
    for f in range(start, ks[-1]):
        t, a, noi = state(f)
        (a1, b1), (a2, b2) = exp(f - start)
        tot += 1
        if a[1] != 15:              # tone 1: an effect
            fx += 1
            continue
        if b1 & 0x40:
            ok = t[2] == ((b1 & 0x3F) << 4 | a1 & 15) and a[2] == a1 >> 4
        elif b1 & 0x80:
            ok = a[3] == a1 >> 4 and noi == a1 & 7 or a[3] != 15
        else:
            ok = a[2] == 15
        if b2 & 0x40:
            ok = ok and t[0] == ((b2 & 0x3F) << 4 | a2 & 15) and a[0] == a2 >> 4
        else:
            ok = ok and a[0] == 15
        bad += not ok
        if not ok and os.environ.get("V"): print("bad", f, f - start, state(f), exp(f - start))
        if (e1[(f - start) % n] or (0, 0, 9))[0] == "K" and (e1[(f - start) % n][2] == 0):
            kicks.append(f)
    print("frames %d..%d: %d compared, %d with an effect on tone 1, %d mismatches"
          % (start, ks[-1], tot, fx, bad))
    print("first kicks", kicks[:6], "gaps", sorted({b - a for a, b in zip(kicks, kicks[1:])})[:6])


if __name__ == "__main__":
    main()
