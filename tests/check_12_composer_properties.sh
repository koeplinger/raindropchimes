#!/bin/sh
# Composer properties over 1,000 seeds for every built-in rule set
# (docs/REVAMP_PLAN.md, section 7, item 8): every tune is a sound score; every
# parent held the previous slot when its child was created; frequencies stay
# in range (the first tone excepted); ratios come from the table; no tone is
# created in the end phase; another number of cycles gives another tune; the
# same rules, seed and cycles always give the same tune. Broken rule sets and
# too few or too many cycles are refused, and a refusal or a failure half way
# leaves nothing in the score. The longest piece allowed can be read back.

cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"

SOURCES="src/compose/rng.c src/compose/rules.c src/compose/slotwalk.c
         src/score/score.c src/score/score_text.c"
$CC $CFLAGS -Isrc -o tests/bin/compose_properties tests/compose_properties.c $SOURCES -lm || exit 1

tests/bin/compose_properties
