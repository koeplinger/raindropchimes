#!/bin/sh
# The favourite tune from the command line (plan section 7, item 1):
# chimes score --preset v4 --seed 1297735820 must give the reference tone lines.
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
$CC $CFLAGS -Isrc -o tests/bin/compose_compare tests/compose_compare.c -lm || exit 1

# On Linux every digit must match. Elsewhere the maths library may round the
# first frequency differently in its last digit; that passes, with a remark.
case "$(uname -s)" in
    Linux) mode=exact ;;
    *)     mode=loose ;;
esac

ref=docs/reference/tune4_seed1297735820.tones.txt
out=tests/tmp/cli_favourite.score

./chimes score --preset v4 --seed 1297735820 > $out 2> tests/tmp/cli_favourite.err || { cat tests/tmp/cli_favourite.err; exit 1; }
grep -v '^#' $ref > tests/tmp/cli_reference_tones.txt
grep -v '^[#@]' $out > tests/tmp/cli_favourite_tones.txt
echo "chimes score --preset v4 --seed 1297735820 against $ref:"
tests/bin/compose_compare $mode tests/tmp/cli_reference_tones.txt tests/tmp/cli_favourite_tones.txt || exit 1

# The composition equals that of the stored score, and v4 is the default preset.
grep -v '^#' tests/data/favourite.score > tests/tmp/cli_stored.txt
grep -v '^#' $out > tests/tmp/cli_favourite.txt
echo "the same against tests/data/favourite.score:"
tests/bin/compose_compare $mode tests/tmp/cli_stored.txt tests/tmp/cli_favourite.txt || exit 1
./chimes score --seed 1297735820 2> /dev/null | cmp - $out || { echo "a plain 'chimes score' does not use the v4 preset"; exit 1; }
./chimes score --seed 1297735820 -o tests/tmp/cli_favourite_o.score 2> /dev/null && cmp tests/tmp/cli_favourite_o.score $out \
    || { echo "'chimes score -o FILE' does not write the same score"; exit 1; }
echo "v4 is the default preset; 'chimes score -o FILE' writes the same score into a file"
