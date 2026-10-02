#!/bin/sh
# Output formats: "wav" (16-bit conversion, saturation count, header) and
# "raw" (32-bit float frames, to a file and to standard output).
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
$CC $CFLAGS -Isrc -o tests/bin/synth_sinks tests/synth_sinks.c tests/synth_common.c \
    src/synth/*.c src/output/*.c src/score/*.c -lm || exit 1

tests/bin/synth_sinks files tests/tmp || exit 1

tests/bin/synth_sinks stdout > tests/tmp/sinks_stdout.raw || exit 1
if ! cmp tests/tmp/sinks.raw tests/tmp/sinks_stdout.raw; then
    echo "FAIL: raw frames to standard output differ from raw frames to a file"
    exit 1
fi
echo "raw: standard output gives the same bytes as a file"
