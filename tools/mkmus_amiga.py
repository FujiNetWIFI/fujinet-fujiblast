#!/usr/bin/env python3
"""Compile the shared two-voice score to Paula periods and 60 Hz steps."""
import sys
from pathlib import Path
import mus

CLOCKS = (3546895, 3579545)


def voice(event):
    if event is None:
        return 0, 0, False
    kind, note, age = event
    hz = mus.hz(note) if note is not None else 0
    if kind == 'K':
        return (220, 110)[min(age, 1)], 40, False
    if kind == 'S':
        return 7000, (40, 25, 16)[min(age, 2)], True
    if kind == 'H':
        return 14000, 16, True
    if kind == 'bass':
        return hz, 32, False
    if kind == 'blip':
        return hz, 28 if age < 2 else 14 if age < 4 else 0, False
    if kind == 'tone':
        return hz, 28 if age < 2 else 20, False
    if kind in ('arp', 'arpmaj', 'warble'):
        return hz, 24, False
    age &= ~1
    if kind == 'sweep':
        return max(125, hz / 1.08 ** age), max(5, 28 - age * 2), False
    if kind == 'rise':
        return min(3500, hz / 0.96 ** age), 24 if age < 24 else 28, False
    raise ValueError(kind)


def steps(events):
    result = []
    for event in events:
        hz, volume, noise = voice(event)
        periods = tuple(max(124, min(65535, round(clock / (hz * (1 if noise else 8)))))
                        if hz else 124 for clock in CLOCKS)
        value = (*periods, volume, int(noise))
        if result and result[-1][0] == value and result[-1][1] < 255:
            result[-1][1] += 1
        else:
            result.append([value, 1])
    return result


def generate(source, target):
    song = mus.parse(source)
    patterns, order = {}, []
    for name in song.order:
        pair = []
        for channel in (1, 2):
            data = tuple((value, count) for value, count in steps(mus.frames(song, name, channel)))
            if data not in patterns:
                patterns[data] = 'pattern_%d' % len(patterns)
            pair.append(patterns[data])
        order.append(pair)
    lines = ['/* Generated from music/play.txt by tools/mkmus_amiga.py. */', '#include "music.h"']
    for data, label in patterns.items():
        lines.append('static const FbMusicStep %s[] = {' % label)
        for (pal, ntsc, volume, noise), count in data:
            lines.append('    {%d, %d, %d, %d, %d},' % (pal, ntsc, count, volume, noise))
        lines.append('    {0, 0, 0, 0, 0}\n};')
    lines.append('const FbMusicStep *const fb_music_order[][2] = {')
    lines += ['    {%s, %s},' % tuple(pair) for pair in order]
    lines += ['};', 'const uint8_t fb_music_count = %d, fb_music_loopat = %d;' % (len(order), song.loop)]
    Path(target).parent.mkdir(parents=True, exist_ok=True)
    Path(target).write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    generate(sys.argv[1], sys.argv[2])
