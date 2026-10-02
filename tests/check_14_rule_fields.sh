#!/bin/sh
# Rule fields that no reference tone list can test, because every built-in
# rule set has the same number there: full_reach (11), level_scale (150),
# level_offset (0.05), and lone_level (the same wherever the level rule is
# the same). Each is tried with a test-only rule set whose tunes can be
# worked out by hand (see tests/compose_fields.c). Two details of the rules
# are tried the same way: the frequency range includes both its ends, and
# the density is held at 1 before it is squared.

cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"

SOURCES="src/compose/rng.c src/compose/rules.c src/compose/slotwalk.c
         src/score/score.c"
$CC $CFLAGS -Isrc -o tests/bin/compose_fields tests/compose_fields.c $SOURCES -lm || exit 1

tests/bin/compose_fields
