#!/bin/sh
# Output gain (plan section 5.4): the default is 1, the old scale; a tune that
# would exceed full scale is turned down just enough, and the program says so.
# Seed 1297700025 with the v4 preset peaks at about 103 % of full scale.
cd tests/tmp || exit 1
chimes=../../chimes

$chimes make --preset v4 --seed 1297700025 -o loud_auto.wav 2> loud_auto.err || { cat loud_auto.err; exit 1; }
grep -q "turned down" loud_auto.err || { echo "a loud tune was not turned down:"; cat loud_auto.err; exit 1; }
grep -q "warning" loud_auto.err && { echo "samples saturated despite the turn-down:"; cat loud_auto.err; exit 1; }
gain=$(sed -n 's/^# render: gain //p' loud_auto.score)
case $gain in 0.9*) ;; *) echo "expected a recorded gain just below 1, got '$gain'"; exit 1;; esac
echo "seed 1297700025 is turned down to output gain $gain; nothing saturates"

$chimes make --preset v4 --seed 1297700025 --gain 1 -o loud_fixed.wav 2> loud_fixed.err || { cat loud_fixed.err; exit 1; }
grep -q "warning: .* samples exceeded full scale" loud_fixed.err || { echo "a fixed gain of 1 did not report saturated samples:"; cat loud_fixed.err; exit 1; }
echo "with --gain 1 the saturated samples are reported: $(grep warning loud_fixed.err)"

# The favourite keeps output gain 1.
$chimes make --preset v4 --seed 1297735820 -o loud_favourite.wav 2> loud_favourite.err || exit 1
grep -q "turned down" loud_favourite.err && { echo "the favourite was turned down"; exit 1; }
[ "$(sed -n 's/^# render: gain //p' loud_favourite.score)" = "1" ] || { echo "the favourite's recorded gain is not 1"; exit 1; }
echo "the favourite keeps output gain 1"
