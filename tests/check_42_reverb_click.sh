#!/bin/sh
# Reverb: a single click comes back with the right delays, gains and channel
# swaps, for the first and second generation of echoes (plan section 7, item 7).
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
$CC $CFLAGS -Isrc -o tests/bin/synth_reverb_click tests/synth_reverb_click.c tests/synth_common.c \
    src/synth/*.c src/output/*.c src/score/*.c -lm || exit 1

tests/bin/synth_reverb_click
