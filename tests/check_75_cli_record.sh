#!/bin/sh
# The record of a tune is its score file (plan section 6 and phase 4):
#   the command recorded in a score reproduces the same audio file;
#   a score written, read and rendered gives the same audio as a direct render;
#   '-o -' writes the same frames, raw, to standard output.
cd tests/tmp || exit 1
PATH="$(cd ../.. && pwd):$PATH"; export PATH     # so that the recorded 'chimes ...' command runs

rm -f rec_a.wav rec_a.score rec_b.wav rec_b.score
chimes make --seed 20110214 --cycles 45 --bins 500 -o rec_a.wav 2> rec_make.err || { cat rec_make.err; exit 1; }
[ -s rec_a.wav ] && [ -s rec_a.score ] || { echo "make did not write the audio and its score"; exit 1; }
cp rec_a.wav rec_first.wav; cp rec_a.score rec_first.score

# 1. The recorded command recreates the same audio and the same score.
command=$(sed -n 's/^# render: command //p' rec_a.score)
[ -n "$command" ] || { echo "the score has no recorded command"; exit 1; }
rm -f rec_a.wav rec_a.score
sh -c "$command" 2> /dev/null || { echo "the recorded command failed: $command"; exit 1; }
cmp rec_first.wav rec_a.wav || { echo "the recorded command gave different audio: $command"; exit 1; }
cmp rec_first.score rec_a.score || { echo "the recorded command gave a different score file"; exit 1; }
echo "recorded command reproduces the audio: $command"

# 2. Rendering the score file gives the same audio as the direct render.
chimes render rec_a.score -o rec_b.wav 2> rec_render.err || { cat rec_render.err; exit 1; }
cmp rec_first.wav rec_b.wav || { echo "'chimes render' of the score gave different audio"; exit 1; }
grep -v '^#' rec_first.score > rec_first.txt; grep -v '^#' rec_b.score > rec_b.txt
cmp rec_first.txt rec_b.txt || { echo "'chimes render' changed the composition"; exit 1; }
command=$(sed -n 's/^# render: command //p' rec_b.score)
cp rec_b.wav rec_b_first.wav; rm -f rec_b.wav
sh -c "$command" 2> /dev/null && cmp rec_b_first.wav rec_b.wav || { echo "the command recorded by render failed: $command"; exit 1; }
echo "a score written, read and rendered gives the same audio as a direct render"

# 3. A render can change the sound without touching the composition.
chimes render rec_a.score --bins 250 -o rec_fast.wav 2> /dev/null || exit 1
cmp -s rec_first.wav rec_fast.wav && { echo "--bins did not change the render"; exit 1; }
grep -v '^#' rec_fast.score | cmp - rec_first.txt || { echo "render --bins changed the composition"; exit 1; }

# 4. Raw output: the same number of frames as the WAV, as 32-bit floats.
rm -f chimes_20110214_500_45.score
chimes make --seed 20110214 --cycles 45 --bins 500 -o - > rec_raw.f32 2> /dev/null || exit 1
wav_bytes=$(wc -c < rec_first.wav | tr -d ' '); raw_bytes=$(wc -c < rec_raw.f32 | tr -d ' ')
[ $(( (wav_bytes - 44) * 2 )) -eq "$raw_bytes" ] || { echo "raw output has $raw_bytes bytes, WAV has $wav_bytes"; exit 1; }
[ -s chimes_20110214_500_45.score ] || { echo "'-o -' did not write the score under its default name"; exit 1; }
echo "raw output to standard output: $((raw_bytes / 8)) frames, as in the WAV"
