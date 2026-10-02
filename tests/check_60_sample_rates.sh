#!/bin/sh
# Sample rates (plan section 7, item 7): renders at 44.1 and 48 kHz start
# every tick at the same time, are equally loud cycle by cycle, and end
# within one second of each other.
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
$CC $CFLAGS -Isrc -o tests/bin/synth_rates tests/synth_rates.c tests/synth_common.c \
    src/synth/*.c src/output/*.c src/score/*.c -lm || exit 1

tests/bin/synth_rates tests/data/favourite.score
