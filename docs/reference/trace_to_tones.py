#!/usr/bin/env python3
"""Turn the event log of the traced 2011-02-14 program into tone lines.

    python3 trace_to_tones.py events.txt [cycles]

events.txt is written by the program built with v4_trace.patch (see README.md).
cycles is the totalSongTime the program was run with (default 500). Output: one
line per tone created before the end phase, in the layout of
tune4_seed1297735820.tones.txt (without its comment header):

    tick cycle slot parent ratio freq_hz pan reps level jitter
"""
import re
import sys

SLOTS = 11
RATIOS = ("5/16 5/8 5/4 5/2 5/1 3/8 3/4 3/2 3/1 6/1 1/4 1/2 1/1 2/1 4/1 "
          "1/6 1/3 2/3 4/3 8/3 1/5 2/5 4/5 8/5 16/5").split()


def main():
    path = sys.argv[1]
    cycles = int(sys.argv[2]) if len(sys.argv) > 2 else 500
    end_phase_tick = SLOTS * (cycles - 30)
    occupant = {}  # slot -> tick at which its current tone was created
    for line in open(path):
        fields = dict(re.findall(r'(\w+)=(\S+)', line))
        if line.startswith('INIT'):
            print(f"0 0 0 - - {fields['freq']} {fields['pos']} {fields['cnt']} 1 -")
            occupant[0] = 0
        elif line.startswith('NOTE'):
            tick = int(fields['tick'])
            if tick >= end_phase_tick:
                break
            slot = tick % SLOTS
            parent = occupant[(slot - 1) % SLOTS]
            ratio = RATIOS[int(fields['r25'])]
            print(f"{tick} {tick // SLOTS} {slot} {parent} {ratio} {fields['freq']} "
                  f"{fields['pos']} {fields['cnt']} {fields['play']} {fields['r100vib']}")
            occupant[slot] = tick


if __name__ == '__main__':
    main()
