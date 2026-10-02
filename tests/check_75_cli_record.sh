#!/bin/sh
# The record of a tune is its score file (plan section 6 and phase 4):
#   the command recorded in a score reproduces the same audio file;
#   a score written, read and rendered gives the same audio as a direct render;
#   '-o -' writes the same frames, raw, to standard output.
cd tests/tmp || exit 1
PATH="$(cd ../.. && pwd):$PATH"; export PATH     # so that the recorded 'chimes ...' command runs

recorded_command() { sed -n 's/^# render: command //p' "$1"; }

rm -f rec_a.wav rec_a.score rec_b.wav rec_b.score
chimes make --seed 20110214 --cycles 45 --bins 500 -o rec_a.wav 2> rec_make.err || { cat rec_make.err; exit 1; }
[ -s rec_a.wav ] && [ -s rec_a.score ] || { echo "make did not write the audio and its score"; exit 1; }
[ -e rec_a.score.chimes-tmp ] && { echo "the temporary score file was left behind"; exit 1; }
cp rec_a.wav rec_first.wav; cp rec_a.score rec_first.score

# 1. The recorded command recreates the same audio and the same score.
command=$(recorded_command rec_a.score)
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
command=$(recorded_command rec_b.score)
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

# 5. Odd names: the recorded command must still recreate the file when a
#    shell runs it. With a space and a quote; with characters a shell would
#    otherwise act on ($, backtick, *, ~, ;); with a leading dash.
for odd in "rec odd name's.wav" 'rec $HOME `id` *;~.wav' '-rec dash.wav'; do
    odd_score="${odd%.wav}.score"
    rm -f -- "$odd" "$odd_score"
    chimes make --seed 20110214 --cycles 45 --bins 500 -o "$odd" 2> /dev/null || { echo "make failed for the name '$odd'"; exit 1; }
    cmp rec_first.wav "./$odd" || { echo "the audio under the name '$odd' differs"; exit 1; }
    command=$(recorded_command "./$odd_score")
    rm -f -- "$odd"
    sh -c "$command" 2> /dev/null && cmp rec_first.wav "./$odd" \
        || { echo "the recorded command does not survive the name '$odd': $command"; exit 1; }
    # ... and the same through render, whose command names the score file too.
    rm -f -- "$odd"
    chimes render "./$odd_score" -o "$odd" 2> /dev/null || { echo "render failed for the name '$odd'"; exit 1; }
    command=$(recorded_command "./$odd_score")
    rm -f -- "$odd"
    sh -c "$command" 2> /dev/null && cmp rec_first.wav "./$odd" \
        || { echo "the command recorded by render does not survive the name '$odd': $command"; exit 1; }
done
echo "names with spaces, quotes, \$, backticks, * and a leading dash are recorded so that a shell reads them back"

# 6. A command too long for one line of the score is left out, and the program
#    says so; it is never recorded cut short. The audio and the settings are
#    still written. An older command in the score does not stay behind either.
part=rec_long_0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
long=$part/$part/$part/$part/$part
mkdir -p "$long" || exit 1
chimes make --seed 20110214 --cycles 45 --bins 500 -o "$long/tune.wav" 2> rec_long.err || { cat rec_long.err; exit 1; }
cmp rec_first.wav "$long/tune.wav" || { echo "the audio under a very long path differs"; exit 1; }
[ -z "$(recorded_command "$long/tune.score")" ] || { echo "a command too long for one line of the score was recorded"; exit 1; }
grep -q "not recorded" rec_long.err || { echo "the program did not say that the command was not recorded:"; cat rec_long.err; exit 1; }
grep -q '^# render: bins 500$' "$long/tune.score" || { echo "the render settings are missing under a very long path"; exit 1; }
chimes render rec_a.score -o "$long/again.wav" 2> /dev/null || { echo "render to a very long path failed"; exit 1; }
[ -z "$(recorded_command "$long/again.score")" ] || { echo "an older command stayed in the score when the new one could not be recorded"; exit 1; }
echo "a command too long to record is left out, with a message; the settings are still recorded"

# 7. The limit is one line of the score: a command of 511 characters is
#    recorded, one of 512 is not.
prefix="chimes make --rules v4 --seed 20110214 --cycles 45 --patch v4 --bins 500 --drift 0 --rate 44100 --gain 1 -o "
room=$(( 511 - ${#prefix} ))
dir=$(awk 'BEGIN { for (i = 0; i < 200; i++) printf "d" }')
mkdir -p "$dir" || exit 1
name=$(awk -v n=$(( room - 200 - 1 - 4 )) 'BEGIN { for (i = 0; i < n; i++) printf "f" }')
chimes make --seed 20110214 --cycles 45 --bins 500 -o "$dir/$name.wav" 2> /dev/null || exit 1
command=$(recorded_command "$dir/$name.score")
[ ${#command} -eq 511 ] || { echo "a command of 511 characters was not recorded (got ${#command})"; exit 1; }
chimes make --seed 20110214 --cycles 45 --bins 500 -o "$dir/f$name.wav" 2> /dev/null || exit 1
[ -z "$(recorded_command "$dir/f$name.score")" ] || { echo "a command of 512 characters was recorded"; exit 1; }
echo "a command of 511 characters is recorded, one of 512 is not"

# 8. A line break in a name cannot be recorded on one line: the command is
#    left out, and the score can still be read.
broken=$(printf 'rec line\nbreak.wav')
chimes make --seed 20110214 --cycles 45 --bins 500 -o "$broken" 2> rec_break.err || { cat rec_break.err; exit 1; }
grep -q "not recorded" rec_break.err || { echo "a name with a line break: the program did not say that the command was not recorded"; exit 1; }
chimes render "$(printf 'rec line\nbreak.score')" -o rec_break_again.wav 2> /dev/null \
    && cmp rec_first.wav rec_break_again.wav || { echo "the score written beside a name with a line break cannot be read back"; exit 1; }
echo "a name with a line break: the command is left out, and the score still reads"
