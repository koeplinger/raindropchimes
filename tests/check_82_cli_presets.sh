#!/bin/sh
# The presets of the later versions (plan section 9 and section 7, item 9):
# each composes the tone lines that the old program of that version made,
# as traced in docs/reference/.
ref=docs/reference
fail=0

same_tones() {   # same_tones REFERENCE-LIST chimes-score-options...
    list=$1; shift
    ./chimes score "$@" 2> tests/tmp/preset.err | grep -v '^[#@]' > tests/tmp/preset_tones.txt
    grep -v '^#' "$ref/$list" > tests/tmp/preset_reference.txt
    if cmp -s tests/tmp/preset_reference.txt tests/tmp/preset_tones.txt; then
        echo "chimes score $*: $(wc -l < tests/tmp/preset_tones.txt | tr -d ' ') tone lines equal $list"
    else
        echo "chimes score $*: differs from $list; first differing lines:"
        cat tests/tmp/preset.err
        diff tests/tmp/preset_reference.txt tests/tmp/preset_tones.txt | head -n 4
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
note() { sed -n "s/^# render: $2 //p" "preset_$1.score"; }
[ "$(note v5 patch)" = v4 ]   && [ "$(note v5 bins)" = 1700 ]  && [ "$(note v5 drift)" = 0 ]  || { echo "preset v5 records the wrong settings"; exit 1; }
[ "$(note v6 patch)" = v6 ]   && [ "$(note v6 bins)" = 3700 ]  && [ "$(note v6 drift)" = 1 ]  || { echo "preset v6 records the wrong settings"; exit 1; }
[ "$(note v10 patch)" = v10 ] && [ "$(note v10 bins)" = 3440 ] && [ "$(note v10 drift)" = 0 ] || { echo "preset v10 records the wrong settings"; exit 1; }
grep -q '^@ rules v4$' preset_v5.score && grep -q '^@ rules v6$' preset_v6.score && grep -q '^@ rules v10$' preset_v10.score \
    || { echo "a preset composed with the wrong rule set"; exit 1; }
echo "presets v5, v6 and v10 render and record their own rules, patch, tempo and drift"
