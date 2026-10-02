#!/bin/sh
# Likeness of the later sounds to their 2011 recordings (plan section 7,
# item 6, for v5 and v6; phase 5): the two tunes of 17 February 2011 through
# the v4 patch, and the two of 18 February 2011 through the v6 patch with its
# slowdown, each compared wave for wave with its recording.
# Needs ffmpeg to decode the OGG files; skipped without it.
#
# Pass marks: no time shift; overall correlation at least 0.999 for v5 and
# 0.9985 for v6; every cycle at least 0.99, leaving out cycles in which the
# recording is nearly silent (average sample size below 50 of 32768), where
# the noise of the OGG compression is as large as the music.
# The original programs of 17 and 18 February, rebuilt from source, reach the
# same numbers against these recordings (docs/reference/README.md): what is
# missing to 1 is the OGG compression.
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"

if ! command -v ffmpeg > /dev/null 2>&1; then
    echo "ffmpeg is not installed"
    exit 77
fi

$CC $CFLAGS -Isrc -o tests/bin/synth_likeness tests/synth_likeness.c tests/synth_common.c \
    src/synth/*.c src/output/*.c src/score/*.c -lm || exit 1

status=0

# compare RECORDING SCORE PATCH BINS DRIFT OVERALL-PASS-MARK
compare() {
    echo "legacy/examples/$1.ogg against tests/data/$2.score through patch $3, $4 bins, drift $5:"
    ffmpeg -nostdin -v error -y -i "legacy/examples/$1.ogg" -f s16le -acodec pcm_s16le -ac 2 -ar 44100 \
        tests/tmp/recording_later.s16le || { status=1; return; }
    tests/bin/synth_likeness "tests/data/$2.score" tests/tmp/recording_later.s16le \
        tests/tmp/likeness_later.wav "$3" "$4" "$5" "$6" 0.99 50 || status=1
}

compare sndharmonics5_1700-400-1297975154   tune5_seed1297975154_cycles400 v4 1700 0 0.999
compare sndharmonics5_700-200-1297981226    tune5_seed1297981226_cycles200 v4  700 0 0.999
compare sndharmonics6_3700p1-300-1298060492 tune6_seed1298060492_cycles300 v6 3700 1 0.9985
compare sndharmonics6_3700p1-300-1298060823 tune6_seed1298060823_cycles300 v6 3700 1 0.9985

rm -f tests/tmp/recording_later.s16le tests/tmp/likeness_later.wav
exit $status
