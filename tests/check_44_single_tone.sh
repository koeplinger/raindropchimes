#!/bin/sh
# Single tone: the favourite's first tone has the right frequency, pan and
# loudness (plan section 7, item 7); the voice follows Appendix B sample by
# sample; the phase runs on, holds, or restarts as the plan says.
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
$CC $CFLAGS -Isrc -o tests/bin/synth_single_tone tests/synth_single_tone.c tests/synth_common.c \
    src/synth/*.c src/output/*.c src/score/*.c -lm || exit 1

tests/bin/synth_single_tone
