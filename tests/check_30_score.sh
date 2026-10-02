#!/bin/sh
# The score in memory and as a text file (plan section 7, item 4): round
# trips, and everything score_check and score_read must refuse.
$CC $CFLAGS -Isrc -o tests/bin/score_selftest tests/score_selftest.c src/score/*.c -lm || exit 1
tests/bin/score_selftest tests/data/favourite.score tests/tmp/favourite_rewritten.score || exit 1
cmp tests/data/favourite.score tests/tmp/favourite_rewritten.score || { echo "the favourite's score file changed on write -> read -> write"; exit 1; }
echo "the favourite's score file survives read -> write unchanged (625 tones)"
