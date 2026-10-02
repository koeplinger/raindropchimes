#!/bin/sh
# Separation at the command line (plan section 7, item 3): the composition is
# identical under different tempo and sample rate, and different under a
# different number of cycles.
cd tests/tmp || exit 1
chimes=../../chimes
composition() { grep -v '^#' "$1"; }    # the '@' lines and the tone lines

$chimes make --seed 7 --cycles 40 --bins 300 -o sep_a.wav 2> /dev/null || exit 1
$chimes make --seed 7 --cycles 40 --bins 800 --drift 2 --rate 48000 -o sep_b.wav 2> /dev/null || exit 1
$chimes make --seed 7 --cycles 41 --bins 300 -o sep_c.wav 2> /dev/null || exit 1
$chimes make --seed 8 --cycles 40 --bins 300 -o sep_d.wav 2> /dev/null || exit 1

composition sep_a.score > sep_a.txt; composition sep_b.score > sep_b.txt
composition sep_c.score > sep_c.txt; composition sep_d.score > sep_d.txt
cmp sep_a.txt sep_b.txt || { echo "tempo, drift or sample rate changed the composition"; exit 1; }
cmp -s sep_a.wav sep_b.wav && { echo "tempo and sample rate did not change the audio"; exit 1; }
grep -v '^@' sep_a.txt > sep_a_tones.txt; grep -v '^@' sep_c.txt > sep_c_tones.txt
cmp -s sep_a_tones.txt sep_c_tones.txt && { echo "a different number of cycles gave the same tones"; exit 1; }
cmp -s sep_a.txt sep_d.txt && { echo "a different seed gave the same composition"; exit 1; }
echo "the composition does not depend on tempo, drift or sample rate; it does depend on cycles and seed"
