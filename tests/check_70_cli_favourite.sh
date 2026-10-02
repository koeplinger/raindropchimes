#!/bin/sh
# The favourite tune from the command line (plan section 7, item 1):
# chimes score --preset v4 --seed 1297735820 must give the reference tone lines.
ref=docs/reference/tune4_seed1297735820.tones.txt
out=tests/tmp/cli_favourite.score

./chimes score --preset v4 --seed 1297735820 > $out 2> tests/tmp/cli_favourite.err || { cat tests/tmp/cli_favourite.err; exit 1; }
grep -v '^#' $ref > tests/tmp/cli_reference_tones.txt
grep -v '^[#@]' $out > tests/tmp/cli_favourite_tones.txt
if ! cmp -s tests/tmp/cli_reference_tones.txt tests/tmp/cli_favourite_tones.txt; then
    echo "the composed favourite differs from the reference; first differing lines:"
    diff tests/tmp/cli_reference_tones.txt tests/tmp/cli_favourite_tones.txt | head -n 4
    exit 1
fi
echo "the favourite: $(wc -l < tests/tmp/cli_favourite_tones.txt | tr -d ' ') tone lines equal the reference"

# The whole file equals the stored score, and v4 is the default preset.
cmp $out tests/data/favourite.score || { echo "the score file differs from tests/data/favourite.score"; exit 1; }
./chimes score --seed 1297735820 2> /dev/null | cmp - $out || { echo "a plain 'chimes score' does not use the v4 preset"; exit 1; }
./chimes score --seed 1297735820 -o tests/tmp/cli_favourite_o.score 2> /dev/null && cmp tests/tmp/cli_favourite_o.score $out \
    || { echo "'chimes score -o FILE' does not write the same score"; exit 1; }
echo "the score file equals tests/data/favourite.score; v4 is the default preset"
