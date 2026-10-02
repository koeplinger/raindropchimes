#!/bin/sh
# The two worked examples of docs/EXTENDING.md (plan phase 6): the wave form
# "bell" holds exactly the overtones it was given; the effect "soften" takes
# away what its formula says; and the patch "bell", which uses both, renders
# the favourite tune without invalid numbers or saturation, with a ring-out,
# and the same way twice.
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
$CC $CFLAGS -Isrc -o tests/bin/synth_extension tests/synth_extension.c tests/synth_common.c \
    src/synth/*.c src/output/*.c src/score/*.c -lm || exit 1

tests/bin/synth_extension || exit 1

tests/bin/synth_extension tests/data/favourite.score tests/tmp/bell_1.wav || exit 1
tests/bin/synth_extension tests/data/favourite.score tests/tmp/bell_2.wav > /dev/null || exit 1
if ! cmp tests/tmp/bell_1.wav tests/tmp/bell_2.wav; then
    echo "FAIL: two runs through the patch bell give different files"
    exit 1
fi
echo "patch bell: two runs give identical files"
rm -f tests/tmp/bell_1.wav tests/tmp/bell_2.wav
