#!/bin/sh
# Cross-check against the original programs: builds the traced sources of
# 14 February 2011 (v4), 18 February 2011 (v6) and of legacy/ (v10) the way
# docs/reference/README.md describes, runs each for a few seeds and cycle
# counts other than those of the reference lists, and compares its tones with
# the new composer's, line by line.
#
# Skipped (exit 77) where it cannot run: without tar, patch or python3, or on
# a computer whose own rand() is not the Linux one (the original programs use
# the system's rand(), so elsewhere they compose other tunes).
# Everything is built and run in tests/tmp. Each run of the v4 original takes
# a second or two and writes a WAV file of up to 37 MB, which is deleted again
# at once. The v6 and v10 originals take their tempo from the command line;
# they are run very fast (8 samples per slot), which keeps them short and has
# no effect on the tones.

cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"

# seed/cycles. 32 cycles is the shortest piece there is.
RUNS_V4="12345/500 1297700000/500 42/200 2011/333"
RUNS_V6="12345/300 1298000000/300 42/200 2011/333 7/32 99/33"
RUNS_V10="12345/200 1655000000/200 42/200 2011/333 12/200 7/32 99/33"
FAST=7    # binsPerSlot for the v6 and v10 originals

SOURCES="src/compose/rng.c src/compose/rules.c src/compose/slotwalk.c
         src/score/score.c src/score/score_text.c"
$CC $CFLAGS -Isrc -o tests/bin/compose_tones tests/compose_tones.c $SOURCES -lm || exit 1
$CC $CFLAGS -Isrc -o tests/bin/compose_compare tests/compose_compare.c -lm || exit 1
$CC $CFLAGS -Isrc -o tests/bin/compose_generator tests/compose_generator.c \
    src/compose/rng.c || exit 1

archive_v4=legacy/examples/sndharmonics4.source.2011.02.14.tar.gz
archive_v6=legacy/examples/sndharmonics6.source.tar.gz
sources_v10="legacy/harmonics.c legacy/harmonics-util.c legacy/harmonics-dingdong.c"

for tool in tar patch python3; do
    if ! command -v $tool > /dev/null 2>&1; then
        echo "skipped: $tool is not installed"
        exit 77
    fi
done
for file in $archive_v4 $archive_v6 $sources_v10; do
    if [ ! -f "$file" ]; then
        echo "skipped: $file is not there"
        exit 77
    fi
done
if ! tests/bin/compose_generator host; then
    echo "skipped: this computer's rand() is not the Linux one"
    exit 77
fi

root=$(pwd)
status=0

# build_traced DIRECTORY PATCH
# Applies the tracing changes to the original source in DIRECTORY and builds
# it there. It is built the way the reference lists were made, not with this
# project's compiler flags.
build_traced() {
    if ! (cd "$1" && patch -p1 < "$root/docs/reference/$2" \
          && $CC -O2 -w -o harmonics_trace harmonics.c -lm) > "$1/build.log" 2>&1; then
        echo "skipped: the traced original program does not build here (see $1/build.log)"
        exit 77
    fi
}

# compare_runs RULES DIRECTORY BINS RUN...
# Runs the traced original in DIRECTORY for each seed/cycles and compares
# its tones with the new composer's. BINS is "" for the v4 original, whose
# tempo is fixed.
compare_runs() {
    rules=$1
    original=$2
    bins=$3
    shift 3

    for run in "$@"; do
        seed=${run%/*}
        cycles=${run#*/}

        (cd "$original" && rm -f events.txt \
            && ./harmonics_trace "$seed" "$cycles" $bins > run.log 2>&1)
        rm -f "$original/out.wav"
        if [ ! -s "$original/events.txt" ]; then
            echo "FAIL $rules, seed $seed, $cycles cycles: the original program left no event log"
            exit 1
        fi
        python3 docs/reference/trace_to_tones.py "$original/events.txt" "$cycles" \
            > "$original/$seed.original" || exit 1
        tests/bin/compose_tones tones "$rules" "$seed" "$cycles" \
            > "$original/$seed.composed" || exit 1

        echo "$rules, seed $seed, $cycles cycles, original against new composer:"
        tests/bin/compose_compare exact "$original/$seed.original" "$original/$seed.composed" \
            || status=1
    done
}

# v4: the source of 14 February 2011.
work=tests/tmp/compose_original_v4
rm -rf "$work"
mkdir -p "$work"
if ! tar -xzf "$archive_v4" -C "$work"; then
    echo "skipped: $archive_v4 cannot be unpacked"
    exit 77
fi
build_traced "$work/harmonics.2011.02.14" v4_trace.patch
compare_runs v4 "$work/harmonics.2011.02.14" "" $RUNS_V4

# v6: the source of 18 February 2011.
work=tests/tmp/compose_original_v6
rm -rf "$work"
mkdir -p "$work"
if ! tar -xzf "$archive_v6" -C "$work"; then
    echo "skipped: $archive_v6 cannot be unpacked"
    exit 77
fi
build_traced "$work/harmonics.2011.02.18" v6_trace.patch
compare_runs v6 "$work/harmonics.2011.02.18" $FAST $RUNS_V6

# v10: the code in legacy/. Copied, never changed in place.
work=tests/tmp/compose_original_v10
rm -rf "$work"
mkdir -p "$work"
cp $sources_v10 "$work" || exit 1
build_traced "$work" v10_trace.patch
compare_runs v10 "$work" $FAST $RUNS_V10

exit $status
