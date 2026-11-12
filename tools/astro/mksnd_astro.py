#!/usr/bin/env python3
"""mksnd_astro.py -- FujiBlast's sounds for the Astrocade's custom I/O chip
(src/astrocade/src/p_snd.inc): all three tones and the noise.

    mksnd_astro.py src/apple2/sndtab.s OUT.inc

The chip: tones A, B and C are square waves dividing one master
oscillator, f = 1789773 / (2 (MO+1) (T+1)); the noise is a 15-bit LFSR at
about 28 kHz, mixed in at its own volume. MO is fixed here (MO below: D3
to C6 within about 12 cents), and nothing modulates the master (vibrato,
noise), so an effect never retunes another voice: warbles are written
into the notes.

The effects are the Atari's own (src/atari/sound.c, as sndtab.s has them
for the Mockingboard: steps of AUDF, AUDC, jiffies): a pure step (AUDC
$Ax) becomes a tone at the same pitch (POKEY: 31960 / (AUDF+1) Hz), a
noise or buzz step's volume the noise's. Each effect has channels of its
own (EFFECTS), so a blast on the noise never cuts a footstep on A.

The jingles keep FujiBlast's melodies (sndtab.s, its notes at their true
pitches: POKEY's dividers are up to 30 cents off, which a harmony would
beat against) on A and add two parts, new here: a harmony on B (chord tones under the tune) and a bass on C
(HARMONY, BASS below). A note sounds with a decay, then a short release.

A channel's list: steps of T (0: a rest; the noise's: unused), volume,
frames (0 ends), decay (the volume drops 1 every that many frames, to 1;
0 holds it).
"""
import re
import sys

CLOCK = 1789773
MO = 25
FX = ["drop", "boom", "pickup", "kick", "throw", "fizzle", "die", "beep", "go", "select"]
MUS = ["title", "round", "win", "game"]
CH_A, CH_B, CH_C, CH_N = 0, 1, 2, 3

# which channel each effect's tone steps take (its noise steps go to N)
EFFECTS = {
    "drop": CH_A, "boom": CH_C, "pickup": CH_A, "kick": CH_A, "throw": CH_A,
    "fizzle": CH_A, "die": CH_B, "beep": CH_A, "go": CH_A, "select": CH_A,
}

# the new parts: (note, frames); "R" a rest. They line up with the melody.
HARMONY = {
    "title": [("E4", 8), ("G4", 8), ("C5", 8), ("E5", 16), ("C5", 8), ("E5", 24), ("R", 8),
              ("F5", 8), ("E5", 8), ("C5", 8), ("E5", 32)],
    "round": [("G4", 6), ("G4", 6), ("E5", 18)],
    "win": [("C5", 8), ("E5", 8), ("E5", 24), ("R", 4), ("E5", 8), ("G5", 32)],
    "game": [("E4", 8), ("G4", 8), ("C5", 8), ("E5", 16), ("F5", 8), ("E5", 16),
             ("E5", 8), ("C5", 8), ("E5", 8), ("G5", 40)],
}
BASS = {
    "title": [("C4", 24), ("G3", 16), ("C4", 8), ("E3", 24), ("R", 8), ("F3", 8), ("G3", 16),
              ("C4", 32)],
    "round": [("C4", 6), ("C4", 6), ("C4", 18)],
    "win": [("C4", 16), ("C4", 24), ("R", 4), ("G3", 8), ("C4", 32)],
    "game": [("C4", 24), ("C4", 16), ("F3", 8), ("C4", 16), ("G3", 24), ("C4", 40)],
}
NOTES = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def hz(name):
    n = NOTES[name[0]] + 12 * (int(name[-1]) + 1)
    return 440.0 * 2 ** ((n - 69) / 12.0)


def period(f):
    """The tone register for f Hz (clamped to the chip's range)."""
    t = round(CLOCK / (2 * (MO + 1) * f)) - 1
    return max(1, min(255, t))


def audf_hz(audf):
    return 31960.5 / (audf + 1)


def parse(src):
    consts, sounds, cur = {}, {}, None

    def val(t):
        t = t.strip()
        if t in consts:
            return consts[t]
        return int(t[1:], 16) if t.startswith("$") else int(t)

    for line in open(src):
        line = line.split(";", 1)[0].rstrip()
        m = re.match(r"^(\w+)\s*=\s*(\S+)$", line.strip())
        if m:
            consts[m.group(1)] = val(m.group(2))
            continue
        m = re.match(r"^_([sm]_\w+):\s*(.*)$", line)
        if m:
            cur = m.group(1)
            sounds[cur] = []
            line = m.group(2)
        if cur is None:
            continue
        s = line.strip()
        if not s:
            continue
        op, _, args = s.partition(" ")
        a = [x for x in args.split(",") if x.strip()]
        if op == ".byte":
            sounds[cur] += [val(x) for x in a]
        elif op == "N":                 # (the note's name: its true pitch)
            sounds[cur].append(("N", a[0].strip(), val(a[1])))
        elif op == "R":
            sounds[cur].append(("R", 0, val(a[0])))
        elif op == "END":
            cur = None
    return sounds


def melody_steps(items):
    """A jingle's melody (N f, l / R l) on A: a decaying note, a release."""
    out = []
    for kind, f, ln in items:
        if kind == "R":
            out.append((0, 0, ln, 0))
        else:
            t = period(hz(f))
            out.append((t, 11, ln - 2, 3))
            out.append((t, 3, 2, 0))
    return out


def part_steps(part, vol, decay):
    out = []
    for name, ln in part:
        if name == "R":
            out.append((0, 0, ln, 0))
        else:
            t = period(hz(name))
            out.append((t, vol, ln - 2, decay))
            out.append((t, 2, 2, 0))
    return out


def effect_steps(raw, tone_ch):
    """The Atari's steps (AUDF, AUDC, jiffies): per channel lists."""
    tone, noise = [], []
    for i in range(0, len(raw), 3):
        f, c, n = raw[i:i + 3]
        if n == 0:
            break
        vol = c & 15
        if c >> 4 == 0xA:               # pure: a tone at the pitch
            tone.append((period(audf_hz(f)), vol, n, 0))
            noise.append((0, 0, n, 0))
        elif c >> 4 == 0xC:             # buzz: a tone, quieter, a fifth down
            tone.append((period(audf_hz(f) * 2 / 3), vol, n, 0))
            noise.append((0, 0, n, 0))
        else:                           # noise
            tone.append((0, 0, n, 0))
            noise.append((0, vol, n, 0))
    chans = {}
    if any(v for _, v, _, _ in tone):
        chans[tone_ch] = tone
    if any(v for _, v, _, _ in noise):
        chans[CH_N] = noise
    return chans


def boom_body():
    """The blast's body under its noise: a low tone sinking, fading."""
    out, t, v = [], 150, 9
    while v > 0:
        out.append((min(255, t), v, 4, 0))
        t += 14
        v -= 1
    return out


def main(src, out):
    s = parse(src)
    lists = {}                           # label -> steps
    table = []                           # per id: [(channel, label)]
    for name in FX:
        chans = effect_steps(s["s_" + name], EFFECTS[name])
        if name == "boom":
            chans[CH_C] = boom_body()
        entry = []
        for ch, steps in sorted(chans.items()):
            lab = "SN_%s_%s" % (name.upper(), "ABCN"[ch])
            lists[lab] = steps
            entry.append((ch, lab))
        table.append(entry)
    for name in MUS:
        mel = melody_steps(s["m_" + name])
        lists["SN_%s_A" % name.upper()] = mel
        lists["SN_%s_B" % name.upper()] = part_steps(HARMONY[name], 7, 0)
        lists["SN_%s_C" % name.upper()] = part_steps(BASS[name], 9, 4)
        table.append([(ch, "SN_%s_%s" % (name.upper(), "ABC"[ch])) for ch in (CH_A, CH_B, CH_C)])
    for name in MUS:                     # the parts line up
        lens = [sum(st[2] for st in lists["SN_%s_%s" % (name.upper(), c)]) for c in "ABC"]
        assert len(set(lens)) == 1, (name, lens)
    lines = ["; GENERATED by tools/astro/mksnd_astro.py from src/apple2/sndtab.s -- do not edit.",
             "SNDMO   EQU     %d               ; the master oscillator" % MO,
             "; per sound: its channels (0-3: A B C N) and lists, 0FFH ends", "SNDTAB:"]
    for i, entry in enumerate(table):
        lines.append("        DW      SNDE%d" % i)
    for i, entry in enumerate(table):
        lines.append("SNDE%d:" % i)
        for ch, l in entry:
            lines.append("        DB      %d" % ch)
            lines.append("        DW      %s" % l)
        lines.append("        DB      0FFH")
    lines.append("; the lists: T, volume, frames (0 ends), decay")
    for lab, steps in lists.items():
        flat = []
        for st in steps:
            flat += list(st)
        flat += [0, 0, 0, 0]
        lines.append(lab + ":")
        for i in range(0, len(flat), 16):
            lines.append("        DB      " + ",".join("%d" % v for v in flat[i:i + 16]))
    open(out, "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main(*sys.argv[1:3])
