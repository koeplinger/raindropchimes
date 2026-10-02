#!/bin/sh
# Likeness to the 2011 recording (plan section 7, item 6): the new render of
# the favourite lines up with the recording wave for wave over cycles 0..469.
# Needs ffmpeg to decode the OGG; skipped without it.
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"

RECORDING=legacy/examples/sndharmonics4_1700-400c.ogg

if ! command -v ffmpeg > /dev/null 2>&1; then
    echo "ffmpeg is not installed"
    exit 77
fi

$CC $CFLAGS -Isrc -o tests/bin/synth_likeness tests/synth_likeness.c tests/synth_common.c \
    src/synth/*.c src/output/*.c src/score/*.c -lm || exit 1

ffmpeg -nostdin -v error -y -i "$RECORDING" -f s16le -acodec pcm_s16le -ac 2 -ar 44100 \
    tests/tmp/recording_2011.s16le || exit 1

tests/bin/synth_likeness tests/data/favourite.score tests/tmp/recording_2011.s16le \
    tests/tmp/likeness.wav || exit 1
rm -f tests/tmp/recording_2011.s16le tests/tmp/likeness.wav
