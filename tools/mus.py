"""mus.py -- FujiBlast's music (music/*.txt), the tune every port converts
for its own sound chip (tools/mkmus_<platform>.py), as gfx/*.png is the art.

A tune has two channels. ch1 is whole on its own (a drum, then the bass, on
one voice); ch2 is the robots' chatter, for a platform with a second voice.

    tempo N             jiffies (1/60 s) a row
    pattern NAME        rows "ch1 | ch2" up to "end"
    song NAME ...       the patterns in order (more song lines go on)
    loop N              the bar (0 = the first) the song goes back to

'#' after a space starts a comment.

ch1: [K|S|H] then a bass note, '.' (holds) or '-' (rests); a drum alone is
silence after it. K kick, S snare, H hat.
ch2: INSTR NOTE, '.' (holds) or '-' (silence). b blip, t tone, a minor
arpeggio, A major arpeggio (a note every other jiffy), w warble (up a
semitone and back, every other jiffy),
z sweep down, u sweep up.
Notes: C4 is middle C; E1, Bb1, F#5.

frames() turns a channel of a pattern into one event a jiffy, for the port to
voice: None (silence) or (kind, note, k): kind a drum or an instrument
('bass' for ch1's notes), note a MIDI number (the arpeggio's or the warble's
note this jiffy; None for drums), k the jiffies since it began.
"""
import re

DRUMS = {"K": 2, "S": 3, "H": 1}            # jiffies each
INSTR = {"b": "blip", "t": "tone", "a": "arp", "A": "arpmaj", "w": "warble",
         "z": "sweep", "u": "rise"}
ARP = {"arp": (0, 3, 7), "arpmaj": (0, 4, 7)}
PITCH = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def midi(tok, where):
    m = re.fullmatch(r"([A-G])(#|b)?(-?\d)", tok)
    if not m:
        raise SystemExit("%s: not a note: %r" % (where, tok))
    n = PITCH[m.group(1)] + (int(m.group(3)) + 1) * 12
    return n + {"#": 1, "b": -1, None: 0}[m.group(2)]


def hz(note):
    return 440.0 * 2 ** ((note - 69) / 12)


class Song:
    def __init__(self):
        self.tempo = 6
        self.patterns = {}      # name -> [(ch1 cell, ch2 cell, where)]
        self.order = []
        self.loop = 0


def parse(path):
    song, cur = Song(), None
    for i, line in enumerate(open(path), 1):
        where = "%s:%d" % (path, i)
        line = re.sub(r"(^|\s)#.*", "", line).strip()   # (not F#1's)
        if not line:
            continue
        if cur is not None:
            if line == "end":
                cur = None
                continue
            if "|" not in line:
                raise SystemExit("%s: a row is 'ch1 | ch2'" % where)
            a, b = line.split("|", 1)
            cur.append((a.split(), b.split(), where))
            continue
        w = line.split()
        if w[0] == "tempo":
            song.tempo = int(w[1])
        elif w[0] == "pattern":
            cur = song.patterns[w[1]] = []
        elif w[0] == "song":
            song.order += w[1:]
        elif w[0] == "loop":
            song.loop = int(w[1])
        else:
            raise SystemExit("%s: what is %r?" % (where, w[0]))
    for name in song.order:
        if name not in song.patterns:
            raise SystemExit("%s: no pattern %r" % (path, name))
    if not 0 <= song.loop < len(song.order):
        raise SystemExit("%s: loop %d is past the song" % (path, song.loop))
    return song


def frames(song, name, ch):
    """One event a jiffy for channel ch (1 or 2) of pattern name."""
    out, note, inst, k = [], None, None, 0
    for a, b, where in song.patterns[name]:
        cell = list(a if ch == 1 else b)
        row = []
        if ch == 1:
            if cell and cell[0] in DRUMS:
                d = cell.pop(0)
                row += [(d, None, j) for j in range(min(DRUMS[d], song.tempo))]
                if not cell:
                    cell = ["-"]
            if len(cell) != 1:
                raise SystemExit("%s: ch1 is [drum] note|.|-" % where)
            if cell[0] == "-":
                note = None
            elif cell[0] != ".":
                note, k = midi(cell[0], where), 0
            while len(row) < song.tempo:
                row.append(("bass", note, k) if note is not None else None)
                k += 1
        else:
            if cell == ["-"]:
                inst = None
            elif cell != ["."]:
                if len(cell) != 2 or cell[0] not in INSTR:
                    raise SystemExit("%s: ch2 is INSTR NOTE, . or -" % where)
                inst, note, k = INSTR[cell[0]], midi(cell[1], where), 0
            for _ in range(song.tempo):
                if inst is None:
                    row.append(None)
                    continue
                n = note
                if inst in ARP:                 # a note every other jiffy
                    n += ARP[inst][k // 2 % 3]
                elif inst == "warble":
                    n += k >> 1 & 1
                row.append((inst, n, k))
                k += 1
        out += row
    return out
