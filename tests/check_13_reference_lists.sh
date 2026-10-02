#!/bin/sh
# Every reference tone list (docs/REVAMP_PLAN.md, section 7, items 1 and 9):
# for each list in docs/reference, composing with its rule set, seed and
# cycles must give exactly its tone lines. The lists were traced from the old
# programs of 14, 17 and 18 February 2011 and from the code in legacy/.
# Between them they pin down what the favourite alone does not: the density
# curve's denominator, its gain, and the continuous level rule.
#
# The tunes of 17 February (v5) use the rule set v4: v5 composes by the v4
# rules, with another number of cycles.

cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"

SOURCES="src/compose/rng.c src/compose/rules.c src/compose/slotwalk.c
         src/score/score.c src/score/score_text.c"
$CC $CFLAGS -Isrc -o tests/bin/compose_tones tests/compose_tones.c $SOURCES -lm || exit 1
$CC $CFLAGS -Isrc -o tests/bin/compose_compare tests/compose_compare.c -lm || exit 1

# On Linux every digit must match. Elsewhere the maths library may round the
# first frequency, and the levels of v6 and v10, differently in the last
# digit; that passes, with a remark.
case "$(uname -s)" in
    Linux) mode=exact ;;
    *)     mode=loose ;;
esac

status=0
compared=""

# compare RULES SEED CYCLES LIST
compare() {
    list=docs/reference/$4
    tmp=tests/tmp/compose_reference_$4
    compared="$compared $4"

    echo "rules $1, seed $2, $3 cycles, against $list:"
    if [ ! -f "$list" ]; then
        echo "FAIL the reference list is not there"
        status=1
        return
    fi
    grep -v '^#' "$list" > "$tmp.expected"
    if ! tests/bin/compose_tones tones "$1" "$2" "$3" > "$tmp.composed"; then
        echo "FAIL the composer did not finish"
        status=1
        return
    fi
    tests/bin/compose_compare $mode "$tmp.expected" "$tmp.composed" || status=1
}

compare v4  1297735820 500 tune4_seed1297735820.tones.txt
compare v4  1297975154 400 tune5_seed1297975154_cycles400.tones.txt
compare v4  1297981226 200 tune5_seed1297981226_cycles200.tones.txt
compare v6  1298060492 300 tune6_seed1298060492_cycles300.tones.txt
compare v6  1298060823 300 tune6_seed1298060823_cycles300.tones.txt
compare v10 1654607101 200 v10_seed1654607101_cycles200_glibc.tones.txt
compare v10 1655110192 200 v10_seed1655110192_cycles200_glibc.tones.txt
compare v10 1655110280 200 v10_seed1655110280_cycles200_glibc.tones.txt

# A reference list that is added later must be added above as well.
for list in docs/reference/*.tones.txt; do
    name=$(basename "$list")
    case " $compared " in
        *" $name "*) ;;
        *) echo "FAIL $list is not compared by this check; add a line for it"
           status=1 ;;
    esac
done

exit $status
