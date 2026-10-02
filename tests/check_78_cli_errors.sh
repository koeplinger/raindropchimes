#!/bin/sh
# The command line refuses what it should, with exit status 1 and a message,
# and a refusal never damages a file that is already there.
cd tests/tmp || exit 1
chimes=../../chimes
fail=0

# Files a refused command must not write. Removed first, so that what an
# earlier, broken build left behind cannot fail this run.
rm -f cli_same.score cli_tune.* cli_days.wav cli_line_out.wav "it's a.ogg"

refuses() {   # refuses "words expected in the message" args...
    words=$1; shift
    $chimes "$@" > cli_err.out 2> cli_err.err
    status=$?
    if [ $status -eq 0 ]; then echo "accepted: chimes $*"; fail=1
    elif [ $status -ne 1 ]; then echo "chimes $*: exit status $status, not 1"; fail=1
    elif ! grep -q -e "$words" cli_err.err; then echo "chimes $*: message lacks '$words':"; cat cli_err.err; fail=1; fi
}

# Commands and options.
refuses "usage"
refuses "unknown command" compose
refuses "unknown preset" score --preset v99
refuses "unknown rule set" score --rules nope
refuses "unknown patch" make --seed 1 --cycles 40 --patch nope
refuses "at least" score --seed 1 --cycles 31
refuses "unknown option" score --tempo 3
refuses "needs a value" score --seed
refuses "needs a number" score --cycles many
refuses "seed" score --seed -5
refuses "seed" score --seed 4294967296
refuses "cycles" score --cycles 40.5
refuses "cycles" score --cycles 1e30
refuses "cycles" score --cycles inf
refuses "bins" make --seed 1 --cycles 40 --bins 0
refuses "bins" make --seed 1 --cycles 40 --bins 1e11
refuses "drift" make --seed 1 --cycles 40 --drift 1e300
refuses "rate" make --seed 1 --cycles 40 --rate 1e30
refuses "rate" make --seed 1 --cycles 40 --rate 44100.5
refuses "gain" make --seed 1 --cycles 40 --gain 0
refuses "score file" render
refuses "cannot read" render no_such_file.score
refuses "belong to composing" render ../data/favourite.score --seed 3
refuses "belong to rendering" score --seed 3 --patch v6
refuses "belong to rendering" score --seed 3 --bins 300
printf 'not a score\n' > cli_bad.score
refuses "line 1" render cli_bad.score

# Output paths.
refuses "both be written" make --seed 1 --cycles 40 -o cli_same.score
for ending in ogg oga opus flac mp3 m4a aac mp4 wma webm aiff aif FLAC Ogg; do
    refuses "WAV files only" make --seed 1 --cycles 40 -o cli_tune.$ending
    [ -e cli_tune.$ending ] && { echo "a refused command wrote cli_tune.$ending"; fail=1; }
done
refuses "too long" make --seed 1 --cycles 40 -o "$(awk 'BEGIN { for (i = 0; i < 5000; i++) printf "x"; print ".wav" }')"
# A piece too long for a WAV file: 6.8 hours is what one holds at 44100 Hz.
refuses "holds 6.8 hours at most" make --seed 1 --cycles 40 --bins 9000000 -o cli_days.wav
[ -e cli_same.score ] || [ -e cli_days.wav ] && { echo "a refused command left a file behind"; fail=1; }
# The advice for a compressed file names the sample rate in use and quotes the
# name for the shell, and a path that is refused is refused before anything
# is composed.
refuses "ffmpeg -f f32le -ar 48000 " make --seed 1 --cycles 40 --rate 48000 -o cli_tune.ogg
grep -q "tones" cli_err.err && { echo "a refused output path was noticed only after composing"; fail=1; }
refuses "'it'\\\\''s a.ogg'" make --seed 1 --cycles 40 -o "it's a.ogg"

# A hidden file name is not an extension: the score goes beside it.
rm -f .cli_hidden .cli_hidden.score
$chimes make --seed 1 --cycles 40 --bins 100 -o .cli_hidden 2> /dev/null && [ -s .cli_hidden ] && [ -s .cli_hidden.score ] \
    || { echo "'-o .cli_hidden' did not write .cli_hidden and .cli_hidden.score"; fail=1; }

# '# render:' lines that cannot be used are refused, not guessed at.
$chimes make --seed 1 --cycles 40 --bins 300 -o cli_made.wav 2> /dev/null || { echo "could not make cli_made.wav"; exit 1; }
with_line() { sed "s/^# render: $1 .*/# render: $1 $2/" cli_made.score > "$3"; }
with_line bins abc cli_line_1.score;      refuses "cannot be used" render cli_line_1.score -o cli_line_out.wav
with_line bins 17OO cli_line_2.score;     refuses "cannot be used" render cli_line_2.score -o cli_line_out.wav
with_line bins 0 cli_line_3.score;        refuses "cannot be used" render cli_line_3.score -o cli_line_out.wav
with_line bins 1e300 cli_line_4.score;    refuses "cannot be used" render cli_line_4.score -o cli_line_out.wav
with_line drift nonsense cli_line_5.score; refuses "cannot be used" render cli_line_5.score -o cli_line_out.wav
with_line patch nope cli_line_6.score;    refuses "'# render: patch nope' cannot be used" render cli_line_6.score -o cli_line_out.wav
# Too many such lines, or one too long, are refused too: nothing is dropped or cut in silence.
awk '{ print } NR == 1 { for (i = 1; i <= 17; i++) print "# render: extra" i " x" }' cli_made.score > cli_line_7.score
refuses "not a usable" render cli_line_7.score -o cli_line_out.wav
awk '{ print } NR == 1 { printf "# render: long "; for (i = 0; i < 600; i++) printf "x"; print "" }' cli_made.score > cli_line_8.score
refuses "not a usable" render cli_line_8.score -o cli_line_out.wav
[ -e cli_line_out.wav ] && { echo "a refused render left audio behind"; fail=1; }
# ... but an option, or a preset, takes the place of the line that cannot be used.
$chimes render cli_line_1.score --bins 300 -o cli_line_ok.wav 2> /dev/null && cmp -s cli_line_ok.wav cli_made.wav \
    || { echo "--bins did not take the place of an unusable bins line"; fail=1; }
$chimes render cli_line_1.score --preset v4 -o cli_line_preset.wav 2> /dev/null \
    || { echo "--preset did not take the place of an unusable bins line"; fail=1; }
$chimes render cli_line_6.score --patch v4 -o cli_line_patch.wav 2> /dev/null && cmp -s cli_line_patch.wav cli_made.wav \
    || { echo "--patch did not take the place of an unusable patch line"; fail=1; }
# The advice given by render names the sample rate of the render, too.
refuses "ffmpeg -f f32le -ar 48000 " render cli_made.score --rate 48000 -o cli_tune.ogg

# Render must not write over its own input, and a refusal must not touch a
# file that is already there.
cp cli_made.score cli_in.score
refuses "both be written" render cli_in.score -o cli_in.score
cmp -s cli_in.score cli_made.score || { echo "a refused render changed its input score"; fail=1; }
cp cli_made.score cli_named.wav          # a score under the name of an audio file
refuses "overwrite the score" render cli_named.wav
cmp -s cli_named.wav cli_made.score || { echo "a refused render overwrote its input"; fail=1; }
cp cli_made.wav cli_keep.wav
refuses "slot must last" make --seed 1 --cycles 40 --bins 10 --drift -5 --gain 1 -o cli_keep.wav
cmp -s cli_keep.wav cli_made.wav || { echo "a refused render with --gain destroyed the file that was there"; fail=1; }
# A score that cannot be rendered (its only tone is so low that the loudness
# law overflows) is refused before the audio file is touched, with --gain as
# without.
printf '# raindropchimes score 1\n@ rules none\n@ generator none\n@ seed 0\n@ cycles 32\n@ slots 11\n0 0 0 - - 1e-310 0.50 2 1 -\n' > cli_tiny.score
refuses "not a number" render cli_tiny.score --gain 1 -o cli_keep.wav
cmp -s cli_keep.wav cli_made.wav || { echo "a failing render with --gain destroyed the file that was there"; fail=1; }
refuses "not a number" render cli_tiny.score -o cli_keep.wav
cmp -s cli_keep.wav cli_made.wav || { echo "a failing render destroyed the file that was there"; fail=1; }
# A score file that may not be written is noticed before the audio is made:
# the audio and its record always belong together.
cp cli_made.wav cli_ro.wav; cp cli_made.score cli_ro.score; chmod a-w cli_ro.score
if [ ! -w cli_ro.score ]; then    # (for the superuser every file is writable)
    refuses "cannot write 'cli_ro.score'" make --seed 2 --cycles 40 --bins 300 -o cli_ro.wav
    cmp -s cli_ro.wav cli_made.wav || { echo "the audio was replaced although its score could not be written"; fail=1; }
fi
chmod u+w cli_ro.score
ls | grep -q 'chimes-tmp$' && { echo "a temporary score file was left behind"; fail=1; }

# A full disk shows as an error, even for a short score. And when the audio
# cannot be written, no score is written for it either.
if [ -w /dev/full ]; then
    $chimes score --seed 5 --cycles 40 > /dev/full 2> /dev/null && { echo "writing the score to a full disk was not noticed"; fail=1; }
    rm -f cli_full.wav cli_full.score chimes_5_100_40.score
    ln -s /dev/full cli_full.wav
    refuses "is incomplete" make --seed 5 --cycles 40 --bins 100 -o cli_full.wav
    [ -e cli_full.score ] && { echo "a score was written although its audio could not be"; fail=1; }
    $chimes make --seed 5 --cycles 40 --bins 100 -o - > /dev/full 2> cli_err.err
    [ $? -eq 1 ] && grep -q "could not write" cli_err.err || { echo "raw output to a full disk was not noticed"; fail=1; }
    [ -e chimes_5_100_40.score ] && { echo "a score was written although the raw output could not be"; fail=1; }
    rm -f cli_full.wav
fi

$chimes help > cli_help.out 2> cli_help.err && grep -q "chimes make" cli_help.out || { echo "'chimes help' failed"; fail=1; }
grep -q -- "--preset NAME .* v4 v5 v6 v10" cli_help.out && grep -q -- "--patch NAME .* v4 v6 v10 bell" cli_help.out \
    || { echo "'chimes help' does not list the presets and patches"; fail=1; }
for command in make score render; do
    $chimes $command --help > cli_help_2.out 2> /dev/null && cmp -s cli_help.out cli_help_2.out \
        || { echo "'chimes $command --help' does not print the help text"; fail=1; }
done
$chimes score --seed 4294967295 --cycles 32 > /dev/null 2>&1 || { echo "the largest seed and the smallest cycles were refused"; fail=1; }
$chimes make --seed 1 --cycles 40 --bins 300 --drift -2 -o cli_faster.wav 2> /dev/null || { echo "a negative drift was refused"; fail=1; }
[ $fail -eq 0 ] && echo "the command line refuses bad input with a message, and leaves existing files alone"
exit $fail
