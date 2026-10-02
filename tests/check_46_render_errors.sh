#!/bin/sh
# What the synthesizer refuses: unknown names in a patch, settings that cannot
# be rendered, a value that is not a number. Also the output gain and the
# upper limit of the ring-out.
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
$CC $CFLAGS -Isrc -o tests/bin/synth_render_errors tests/synth_render_errors.c tests/synth_common.c \
    src/synth/*.c src/output/*.c src/score/*.c -lm || exit 1

tests/bin/synth_render_errors
