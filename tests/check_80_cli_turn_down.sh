#!/bin/sh
# Output gain (plan section 5.4): the default is 1, the loudness of the 2011
# program; a tune that would exceed full scale is turned down just enough, and
# the program says so.
# Seed 1297700025 with the v4 preset peaks at about 103 % of full scale.
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
$CC $CFLAGS -o tests/bin/cli_wav_peak tests/cli_wav_peak.c || exit 1

cd tests/tmp || exit 1
PATH="$(cd ../.. && pwd):$PATH"; export PATH     # so that the recorded 'chimes ...' command runs

chimes make --preset v4 --seed 1297700025 -o loud_auto.wav 2> loud_auto.err || { cat loud_auto.err; exit 1; }
grep -q "turned down" loud_auto.err || { echo "a loud tune was not turned down:"; cat loud_auto.err; exit 1; }
grep -q "warning" loud_auto.err && { echo "samples saturated despite the turn-down:"; cat loud_auto.err; exit 1; }
gain=$(sed -n 's/^# render: gain //p' loud_auto.score)
case $gain in 0.9*) ;; *) echo "expected a recorded gain just below 1, got '$gain'"; exit 1;; esac
echo "seed 1297700025 is turned down to output gain $gain; nothing saturates"

# "Just enough": the loudest sample is exactly 32767 in size, the largest a
# 16-bit sample can be on both sides of zero. Not 32768 (that fits only on
# the negative side), and not less.
peaks=$(../bin/cli_wav_peak loud_auto.wav) || exit 1
smallest=${peaks% *}; largest=${peaks#* }
loudest=$(( -smallest > largest ? -smallest : largest ))
[ "$loudest" -eq 32767 ] || { echo "after the turn-down the samples run from $smallest to $largest; the loudest should be exactly 32767 in size"; exit 1; }
echo "after the turn-down the samples run from $smallest to $largest: the loudest is exactly 32767 in size"

# The gain that is recorded is exactly the gain that was applied: the
# recorded command gives the same audio and the same score, byte for byte.
cp loud_auto.wav loud_first.wav; cp loud_auto.score loud_first.score
command=$(sed -n 's/^# render: command //p' loud_auto.score)
rm -f loud_auto.wav loud_auto.score
sh -c "$command" 2> /dev/null || { echo "the recorded command failed: $command"; exit 1; }
cmp loud_first.wav loud_auto.wav || { echo "the recorded command of a turned-down tune gave different audio: $command"; exit 1; }
cmp loud_first.score loud_auto.score || { echo "the recorded command of a turned-down tune gave a different score"; exit 1; }
echo "the recorded command reproduces the turned-down audio exactly"

chimes make --preset v4 --seed 1297700025 --gain 1 -o loud_fixed.wav 2> loud_fixed.err || { cat loud_fixed.err; exit 1; }
grep -q "warning: .* samples exceeded full scale" loud_fixed.err || { echo "a fixed gain of 1 did not report saturated samples:"; cat loud_fixed.err; exit 1; }
echo "with --gain 1 the saturated samples are reported: $(grep warning loud_fixed.err)"

# The favourite keeps output gain 1.
chimes make --preset v4 --seed 1297735820 -o loud_favourite.wav 2> loud_favourite.err || exit 1
grep -q "turned down" loud_favourite.err && { echo "the favourite was turned down"; exit 1; }
[ "$(sed -n 's/^# render: gain //p' loud_favourite.score)" = "1" ] || { echo "the favourite's recorded gain is not 1"; exit 1; }
echo "the favourite keeps output gain 1"
