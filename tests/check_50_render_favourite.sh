#!/bin/sh
# Render sanity for the favourite tune (plan section 7, item 5): every sample
# valid; nothing saturates; peak 51 % of full scale; reverb tail after
# 200.34 s; the final stretch is digital silence; the WAV header is correct;
# two runs give identical files.
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
$CC $CFLAGS -Isrc -o tests/bin/synth_favourite tests/synth_favourite.c tests/synth_common.c \
    src/synth/*.c src/output/*.c src/score/*.c -lm || exit 1

tests/bin/synth_favourite tests/data/favourite.score tests/tmp/favourite_1.wav || exit 1
tests/bin/synth_favourite tests/data/favourite.score tests/tmp/favourite_2.wav > /dev/null || exit 1

if ! cmp tests/tmp/favourite_1.wav tests/tmp/favourite_2.wav; then
    echo "FAIL: two runs give different files"
    exit 1
fi
echo "two runs give identical files"
rm -f tests/tmp/favourite_1.wav tests/tmp/favourite_2.wav
