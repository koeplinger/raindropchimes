#!/bin/sh
# Separation at the command line (plan section 7, item 3): the composition is
# identical under different tempo, drift, sample rate and patch, and different
# under a different number of cycles or another seed.
cd tests/tmp || exit 1
chimes=../../chimes
composition() { grep -v '^#' "$1"; }    # the '@' lines and the tone lines

$chimes make --seed 7 --cycles 40 --bins 300 -o sep_a.wav 2> /dev/null || exit 1
$chimes make --seed 7 --cycles 40 --bins 800 --drift 2 --rate 48000 -o sep_b.wav 2> /dev/null || exit 1
$chimes make --seed 7 --cycles 41 --bins 300 -o sep_c.wav 2> /dev/null || exit 1
$chimes make --seed 7 --cycles 40 --bins 300 --patch bell -o sep_e.wav 2> /dev/null || exit 1
$chimes make --seed 8 --cycles 40 --bins 300 -o sep_d.wav 2> /dev/null || exit 1

composition sep_a.score > sep_a.txt; composition sep_b.score > sep_b.txt
composition sep_c.score > sep_c.txt; composition sep_d.score > sep_d.txt
cmp sep_a.txt sep_b.txt || { echo "tempo, drift or sample rate changed the composition"; exit 1; }
cmp -s sep_a.wav sep_b.wav && { echo "tempo and sample rate did not change the audio"; exit 1; }
composition sep_e.score | cmp - sep_a.txt || { echo "another patch changed the composition"; exit 1; }
cmp -s sep_a.wav sep_e.wav && { echo "another patch did not change the audio"; exit 1; }
grep -v '^@' sep_a.txt > sep_a_tones.txt; grep -v '^@' sep_c.txt > sep_c_tones.txt
cmp -s sep_a_tones.txt sep_c_tones.txt && { echo "a different number of cycles gave the same tones"; exit 1; }
cmp -s sep_a.txt sep_d.txt && { echo "a different seed gave the same composition"; exit 1; }
echo "the composition does not depend on tempo, drift, sample rate or patch; it does depend on cycles and seed"

# The sample rate asked for is the one written into the WAV header
# (bytes 24..27, lowest byte first).
rate_of() { od -An -tu1 -j24 -N4 "$1" | awk '{ print $1 + 256 * ($2 + 256 * ($3 + 256 * $4)) }'; }
[ "$(rate_of sep_a.wav)" = 44100 ] || { echo "the default sample rate in the WAV header is $(rate_of sep_a.wav), not 44100"; exit 1; }
[ "$(rate_of sep_b.wav)" = 48000 ] || { echo "--rate 48000 wrote $(rate_of sep_b.wav) into the WAV header"; exit 1; }
echo "the WAV header carries the sample rate asked for (44100 by default, 48000 with --rate 48000)"
