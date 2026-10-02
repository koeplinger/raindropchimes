#!/bin/sh
# The built-in patches v4, v6 and v10 against the numbers of the plan
# (section 9), typed in a second time; and the v6 envelope against the formula
# of the 18 February 2011 program, written out a second time.
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
$CC $CFLAGS -Isrc -o tests/bin/synth_patches tests/synth_patches.c tests/synth_common.c \
    src/synth/*.c src/output/*.c src/score/*.c -lm || exit 1
tests/bin/synth_patches
