"""psgwav.py -- the AY-3-8914's registers ($1F0-$1FD, one set a frame) as a
WAV, for the music's previews (intvsim.py's `psg NAME.wav`). Tones are
squares at clock / (16 P), the noise a 17-bit LFSR at clock / (16 N), the
volume 3 dB a step, the three voices summed. Mono 44.1 kHz, 16 bit.

usage: psgwav.py REGS.txt OUT.wav [pal]     (REGS.txt: 14 hex words a line)"""
import struct
import sys
import wave

RATE = 44100


def synth(frames, pal=False):
    clk = (2000000.0 if pal else 1789773.0) / 16
    fps = 50.0 if pal else 60.0
    amp = [0.0] + [0.25 * 0.707 ** (15 - v) for v in range(1, 16)]
    tphase = [0.0, 0.0, 0.0]
    nphase, lfsr, nbit = 0.0, 1, 1
    out = bytearray()
    spf = RATE / fps
    t = 0.0
    for regs in frames:
        per = [max(1, (regs[i] & 255) | (regs[4 + i] & 15) << 8) for i in range(3)]
        mix = regs[8] & 63
        np = max(1, regs[9] & 31)
        vol = [regs[11 + i] & 15 for i in range(3)]
        t += spf
        while len(out) // 2 < int(t):
            s = 0.0
            for v in range(3):
                tphase[v] += clk / per[v] / RATE
                tphase[v] -= int(tphase[v])
                tone = tphase[v] < 0.5 or (mix >> v) & 1
                noise = nbit or (mix >> (v + 3)) & 1
                if tone and noise:
                    s += amp[vol[v]]
                else:
                    s -= amp[vol[v]]
            nphase += clk / np / RATE
            while nphase >= 1:
                nphase -= 1
                lfsr = (lfsr >> 1) | (((lfsr ^ (lfsr >> 3)) & 1) << 16)
                nbit = lfsr & 1
            out += struct.pack("<h", int(max(-1, min(1, s)) * 20000))
    return bytes(out)


def write(path, frames, pal=False):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(synth(frames, pal))


if __name__ == "__main__":
    fr = [[int(x, 16) for x in ln.split()] for ln in open(sys.argv[1]) if ln.strip()]
    write(sys.argv[2], fr, len(sys.argv) > 3)
