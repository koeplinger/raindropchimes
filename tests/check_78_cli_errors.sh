#!/bin/sh
# The command line refuses what it should, with exit status 1 and a message.
cd tests/tmp || exit 1
chimes=../../chimes
fail=0
refuses() {   # refuses "words expected in the message" args...
    words=$1; shift
    if $chimes "$@" > cli_err.out 2> cli_err.err; then echo "accepted: chimes $*"; fail=1
    elif ! grep -q "$words" cli_err.err; then echo "chimes $*: message lacks '$words':"; cat cli_err.err; fail=1; fi
}
refuses "usage"
refuses "unknown command" compose
refuses "unknown preset" score --preset v99
refuses "unknown rule set" score --rules nope
refuses "unknown patch" make --seed 1 --cycles 40 --patch nope
refuses "at least" score --seed 1 --cycles 31
refuses "unknown option" score --tempo 3
refuses "needs a value" score --seed
refuses "seed" score --seed -5
refuses "seed" score --seed 4294967296
refuses "cycles" score --cycles 40.5
refuses "bins" make --seed 1 --cycles 40 --bins 0
refuses "gain" make --seed 1 --cycles 40 --gain 0
refuses "score file" render
refuses "cannot read" render no_such_file.score
refuses "belong to composing" render ../data/favourite.score --seed 3
printf 'not a score\n' > cli_bad.score
refuses "line 1" render cli_bad.score
$chimes help > cli_help.out 2>&1 && grep -q "chimes make" cli_help.out || { echo "'chimes help' failed"; fail=1; }
$chimes score --seed 4294967295 --cycles 32 > /dev/null 2>&1 || { echo "the largest seed and the smallest cycles were refused"; fail=1; }
[ $fail -eq 0 ] && echo "the command line refuses bad input with a message"
exit $fail
