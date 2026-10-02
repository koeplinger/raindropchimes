#!/bin/sh
# Runs every check in tests/check_*.sh, in name order. The numbers group them:
# 05 the parts stay apart; 10-29 the composer; 30-39 the score; 40-69 the
# synthesizer and the output; 70-89 the command line.
# A check exits 0 (pass), 77 (skipped, e.g. a tool it needs is missing) or anything else (fail).
# Checks may build helper programs into tests/bin and write scratch files into tests/tmp.

cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/bin tests/tmp

CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off}"
export CC CFLAGS

pass=0; fail=0; skip=0
for check in tests/check_*.sh; do
    [ -f "$check" ] || continue
    name=$(basename "$check" .sh)
    sh "$check" > "tests/tmp/$name.log" 2>&1
    status=$?
    if [ $status -eq 0 ]; then
        echo "pass  $name"; pass=$((pass + 1))
    elif [ $status -eq 77 ]; then
        echo "skip  $name  ($(tail -n 1 "tests/tmp/$name.log"))"; skip=$((skip + 1))
    else
        echo "FAIL  $name  (see tests/tmp/$name.log)"; fail=$((fail + 1))
        sed 's/^/      /' "tests/tmp/$name.log" | tail -n 15
    fi
done

echo "$pass passed, $fail failed, $skip skipped"
[ $fail -eq 0 ]
