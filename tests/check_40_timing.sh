#!/bin/sh
# Timing: the closed form for the start of a tick against a slot-by-slot sum,
# with and without drift; tick n at 44.1 kHz with the v4 tempo is frame 1701 n;
# 300 cycles at the v6 tempo last 12,706,650 frames.
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
$CC $CFLAGS -Isrc -o tests/bin/synth_timing tests/synth_timing.c tests/synth_common.c \
    src/synth/*.c src/output/*.c src/score/*.c -lm || exit 1

tests/bin/synth_timing
