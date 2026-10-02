#!/bin/sh
# The built-in generator is the Linux one (docs/REVAMP_PLAN.md, section 7, item 2):
# seed 1 gives 1804289383, 846930886. On a Linux computer it is also compared
# with the system's own rand() on a few thousand seeds.

cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"

$CC $CFLAGS -Isrc -o tests/bin/compose_generator tests/compose_generator.c \
    src/compose/rng.c || exit 1

tests/bin/compose_generator
