#!/bin/sh
# The presets of the later versions (plan section 9 and section 7, item 9):
# each composes the tone lines that the old program of that version made,
# as traced in docs/reference/.
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
$CC $CFLAGS -Isrc -o tests/bin/compose_compare tests/compose_compare.c -lm || exit 1

# On Linux every digit must match. Elsewhere the maths library may round the
# first frequency, and the levels of v6 and v10, differently in the last
# digit; that passes, with a remark.
case "$(uname -s)" in
    Linux) mode=exact ;;
    *)     mode=loose ;;
esac

ref=docs/reference
fail=0

same_tones() {   # same_tones REFERENCE-LIST chimes-score-options...
    list=$1; shift
    ./chimes score "$@" 2> tests/tmp/preset.err | grep -v '^[#@]' > tests/tmp/preset_tones.txt
    grep -v '^#' "$ref/$list" > tests/tmp/preset_reference.txt
    echo "chimes score $* against $list:"
    if ! tests/bin/compose_compare $mode tests/tmp/preset_reference.txt tests/tmp/preset_tones.txt; then
        cat tests/tmp/preset.err
        fail=1
    fi
}

same_tones tune4_seed1297735820.tones.txt           --preset v4  --seed 1297735820
same_tones tune5_seed1297975154_cycles400.tones.txt --preset v5  --seed 1297975154
same_tones tune5_seed1297981226_cycles200.tones.txt --preset v5  --seed 1297981226 --cycles 200
same_tones tune6_seed1298060492_cycles300.tones.txt --preset v6  --seed 1298060492
same_tones tune6_seed1298060823_cycles300.tones.txt --preset v6  --seed 1298060823
same_tones v10_seed1654607101_cycles200_glibc.tones.txt --preset v10 --seed 1654607101
same_tones v10_seed1655110192_cycles200_glibc.tones.txt --preset v10 --seed 1655110192
same_tones v10_seed1655110280_cycles200_glibc.tones.txt --preset v10 --seed 1655110280
[ $fail -eq 0 ] || exit 1

# Each preset renders, and records its own tempo, drift and patch.
cd tests/tmp || exit 1
for preset in v5 v6 v10; do
    ../../chimes make --preset $preset --seed 5 --cycles 36 -o preset_$preset.wav 2> preset_$preset.err \
        || { echo "chimes make --preset $preset failed:"; cat preset_$preset.err; exit 1; }
    [ -s preset_$preset.wav ] || { echo "--preset $preset wrote no audio"; exit 1; }
done
recorded() { sed -n "s/^# render: $2 //p" "preset_$1.score"; }
[ "$(recorded v5 patch)" = v4 ]   && [ "$(recorded v5 bins)" = 1700 ]  && [ "$(recorded v5 drift)" = 0 ]  || { echo "preset v5 records the wrong settings"; exit 1; }
[ "$(recorded v6 patch)" = v6 ]   && [ "$(recorded v6 bins)" = 3700 ]  && [ "$(recorded v6 drift)" = 1 ]  || { echo "preset v6 records the wrong settings"; exit 1; }
[ "$(recorded v10 patch)" = v10 ] && [ "$(recorded v10 bins)" = 3440 ] && [ "$(recorded v10 drift)" = 0 ] || { echo "preset v10 records the wrong settings"; exit 1; }
grep -q '^@ rules v4$' preset_v5.score && grep -q '^@ rules v6$' preset_v6.score && grep -q '^@ rules v10$' preset_v10.score \
    || { echo "a preset composed with the wrong rule set"; exit 1; }
echo "presets v5, v6 and v10 render and record their own rules, patch, tempo and drift"

# The recorded command carries every setting, not only the default ones: a
# tune made with other rules, another patch, drift and sample rate is made
# again exactly by its recorded command.
PATH="$(cd ../.. && pwd):$PATH"; export PATH     # so that the recorded 'chimes ...' command runs
chimes make --preset v6 --seed 5 --cycles 36 --patch bell --drift 3 --rate 48000 -o preset_other.wav 2> /dev/null || exit 1
cp preset_other.wav preset_other_first.wav; cp preset_other.score preset_other_first.score
command=$(sed -n 's/^# render: command //p' preset_other.score)
rm -f preset_other.wav preset_other.score
sh -c "$command" 2> /dev/null || { echo "the recorded command failed: $command"; exit 1; }
cmp preset_other_first.wav preset_other.wav && cmp preset_other_first.score preset_other.score \
    || { echo "the recorded command did not recreate a tune with other rules, patch, drift and rate: $command"; exit 1; }
for setting in "rules v6" "patch bell" "drift 3" "rate 48000" "bins 3700" "cycles 36" "seed 5"; do
    case " $command " in *" --$setting "*) ;; *) echo "the recorded command lacks --$setting: $command"; exit 1;; esac
done
[ "$(recorded other rate)" = 48000 ] || { echo "the score does not record the sample rate"; exit 1; }
echo "a tune with other rules, patch, drift and sample rate is recreated exactly by its recorded command"
