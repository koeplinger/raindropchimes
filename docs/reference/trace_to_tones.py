#!/usr/bin/env python3
"""Turn the event log of a traced 2011 program into tone lines.

    python3 trace_to_tones.py events.txt [cycles]

events.txt is written by a program built with v4_trace.patch, v5_trace.patch,
v6_trace.patch or v10_trace.patch (see README.md); all four logs are read the
same way. cycles is the totalSongTime the program was run with (default 500).
Output: one line per tone created before the end phase, in the layout of the
reference lists (without their comment header):

    tick cycle slot parent ratio freq_hz pan reps level jitter
"""
import re
import sys

SLOTS = 11
END_PHASE_CYCLES = 30
RATIOS = ("5/16 5/8 5/4 5/2 5/1 3/8 3/4 3/2 3/1 6/1 1/4 1/2 1/1 2/1 4/1 "
          "1/6 1/3 2/3 4/3 8/3 1/5 2/5 4/5 8/5 16/5").split()


def shortest(text):
    """The fewest digits that read back as the same number: 0, 1 and 0.7 print
    as written. The same rule as the score writer (src/score/score_text.c)."""
    value = float(text)
    for precision in range(1, 18):
        short = '%.*g' % (precision, value)
        if float(short) == value:
            return short
    return text


def main():
    path = sys.argv[1]
    cycles = int(sys.argv[2]) if len(sys.argv) > 2 else 500
    end_phase_tick = SLOTS * (cycles - END_PHASE_CYCLES)
    occupant = {}  # slot -> tick at which its current tone was created
    freq_of = {}   # tick -> that tone's frequency, as logged
    for line in open(path):
        fields = dict(re.findall(r'(\w+)=(\S+)', line))
        if line.startswith('INIT'):
            print(f"0 0 0 - - {fields['freq']} {fields['pos']} {fields['cnt']} "
                  f"{shortest(fields['play'])} -")
            occupant[0] = 0
            freq_of[0] = fields['freq']
        elif line.startswith('NOTE'):
            tick = int(fields['tick'])
            # The v5, v6 and v10 logs say themselves whether the program was in
            # its end phase; that must agree with the cut made here.
            if 'endphase' in fields and (fields['endphase'] == '1') != (tick >= end_phase_tick):
                sys.exit(f"tick {tick}: the log's end phase does not match cycles = {cycles}")
            if tick >= end_phase_tick:
                break
            slot = tick % SLOTS
            parent = occupant[(slot - 1) % SLOTS]
            if fields['prevF'] != freq_of[parent]:
                sys.exit(f"tick {tick}: parent frequency {fields['prevF']} is not that of tone {parent}")
            ratio = RATIOS[int(fields['r25'])]
            print(f"{tick} {tick // SLOTS} {slot} {parent} {ratio} {fields['freq']} "
                  f"{fields['pos']} {fields['cnt']} {shortest(fields['play'])} {fields['r100vib']}")
            occupant[slot] = tick
            freq_of[tick] = fields['freq']


if __name__ == '__main__':
    main()
