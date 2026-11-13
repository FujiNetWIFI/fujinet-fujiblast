#!/usr/bin/env python3
"""sndview.py -- a MAME recording of the Channel F's sound as a line of
letters, a 50 ms window each: '.' quiet, H ~1 kHz, M ~500 Hz, B the buzz
(its waveform is pulses that decay: crossings of the window's own mean).

    sndview.py FILE.wav
"""
import struct
import sys
import wave

w = wave.open(sys.argv[1])
n, sr, ch = w.getnframes(), w.getframerate(), w.getnchannels()
mono = struct.unpack("<%dh" % (n * ch), w.readframes(n))[::ch]
win = sr // 20
out = []
for i in range(0, len(mono) - win, win):
    seg = mono[i:i + win]
    m = sum(seg) / len(seg)
    span = max(seg) - min(seg)
    if span < 1500:
        out.append(".")
        continue
    zc = sum(1 for a, b in zip(seg, seg[1:]) if (a < m) != (b < m))
    f = zc * 10                 # crossings a second / 2
    out.append("H" if f > 700 else "M" if f > 300 else "B")
s = "".join(out)
print("%.1f s, %d Hz" % (len(mono) / sr, sr))
for i in range(0, len(s), 80):
    print("%5.1f %s" % (i / 20, s[i:i + 80]))
