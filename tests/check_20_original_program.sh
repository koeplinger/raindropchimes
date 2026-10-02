#!/bin/sh
# Cross-check against the original program: builds the traced 2011-02-14
# source (see docs/reference/README.md), runs it for a few seeds and cycle
# counts other than the favourite's, and compares its tones with the new
# composer's, line by line.
#
# Skipped (exit 77) where it cannot run: without tar, patch or python3, or on
# a computer whose own rand() is not the Linux one (the original program uses
# the system's rand(), so elsewhere it composes other tunes).
# Each run of the original takes a second or two and writes a WAV file of up
# to 37 MB into tests/tmp, which is deleted again at once.

cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"

RUNS="12345/500 1297700000/500 42/200 2011/333"    # seed/cycles

SOURCES="src/compose/rng.c src/compose/rules.c src/compose/slotwalk.c
         src/score/score.c src/score/score_text.c"
$CC $CFLAGS -Isrc -o tests/bin/compose_tones tests/compose_tones.c $SOURCES -lm || exit 1
$CC $CFLAGS -Isrc -o tests/bin/compose_compare tests/compose_compare.c -lm || exit 1
$CC $CFLAGS -Isrc -o tests/bin/compose_generator tests/compose_generator.c \
    src/compose/rng.c || exit 1

archive=legacy/examples/sndharmonics4.source.2011.02.14.tar.gz
for tool in tar patch python3; do
    if ! command -v $tool > /dev/null 2>&1; then
        echo "skipped: $tool is not installed"
        exit 77
    fi
done
if [ ! -f "$archive" ]; then
    echo "skipped: $archive is not there"
    exit 77
fi
if ! tests/bin/compose_generator host; then
    echo "skipped: this computer's rand() is not the Linux one"
    exit 77
fi

# Build the traced original in tests/tmp. It is built the way the reference
# was made, not with this project's compiler flags.
work=tests/tmp/compose_original_v4
original=$work/harmonics.2011.02.14
rm -rf "$work"
mkdir -p "$work"
if ! tar -xzf "$archive" -C "$work" \
    || ! (cd "$original" && patch -p1 < ../../../../docs/reference/v4_trace.patch \
          && $CC -O2 -w -o harmonics_trace harmonics.c -lm) > "$work/build.log" 2>&1; then
    echo "skipped: the traced original program does not build here (see $work/build.log)"
    exit 77
fi

status=0
for run in $RUNS; do
    seed=${run%/*}
    cycles=${run#*/}

    (cd "$original" && rm -f events.txt && ./harmonics_trace "$seed" "$cycles" > run.log 2>&1)
    rm -f "$original/out.wav"
    if [ ! -s "$original/events.txt" ]; then
        echo "FAIL seed $seed, $cycles cycles: the original program left no event log"
        exit 1
    fi
    python3 docs/reference/trace_to_tones.py "$original/events.txt" "$cycles" \
        > "$work/$seed.original" || exit 1
    tests/bin/compose_tones tones v4 "$seed" "$cycles" > "$work/$seed.composed" || exit 1

    echo "seed $seed, $cycles cycles, original against new composer:"
    tests/bin/compose_compare exact "$work/$seed.original" "$work/$seed.composed" || status=1
done
exit $status
