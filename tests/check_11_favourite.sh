#!/bin/sh
# The favourite tune (docs/REVAMP_PLAN.md, section 7, item 1): composing with
# the v4 rules, seed 1297735820 and 500 cycles must give exactly the 625 tone
# lines of the reference list, which was traced from the 2011 program.
# Also: the score, written to a file and read back, is the same score.

cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"

SOURCES="src/compose/rng.c src/compose/rules.c src/compose/slotwalk.c
         src/score/score.c src/score/score_text.c"
$CC $CFLAGS -Isrc -o tests/bin/compose_tones tests/compose_tones.c $SOURCES -lm || exit 1
$CC $CFLAGS -Isrc -o tests/bin/compose_compare tests/compose_compare.c -lm || exit 1

# On Linux every digit must match. Elsewhere the maths library may round the
# first frequency differently in its last digit; that passes, with a note.
case "$(uname -s)" in
    Linux) mode=exact ;;
    *)     mode=loose ;;
esac

reference=docs/reference/tune4_seed1297735820.tones.txt
kept_score=tests/data/favourite.score
tmp=tests/tmp/compose_favourite

# 1. The composer against the reference tone list.
grep -v '^#' "$reference" > "$tmp.expected"
tests/bin/compose_tones tones v4 1297735820 500 > "$tmp.composed" || exit 1
echo "composer against $reference:"
tests/bin/compose_compare $mode "$tmp.expected" "$tmp.composed" || exit 1

# 2. Written with score_write, read back with score_read (which also checks
#    the score), written again: the two files must be the same.
tests/bin/compose_tones score v4 1297735820 500 > "$tmp.written" || exit 1
tests/bin/compose_tones reread "$tmp.written" > "$tmp.reread" || exit 1
if ! diff "$tmp.written" "$tmp.reread" > "$tmp.diff"; then
    echo "the score is not the same after writing and reading it back:"
    head -n 10 "$tmp.diff"
    exit 1
fi
echo "score written, read back and written again: identical"

# 3. The composed score against the score file kept in tests/data
#    (comment lines aside).
grep -v '^#' "$kept_score" > "$tmp.kept"
grep -v '^#' "$tmp.written" > "$tmp.written_lines"
echo "composed score against $kept_score:"
tests/bin/compose_compare $mode "$tmp.kept" "$tmp.written_lines" || exit 1
