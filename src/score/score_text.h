/* score_text.h -- the score as a text file.
 *
 *   # raindropchimes score 1
 *   @ rules v4
 *   @ generator glibc
 *   @ seed 1297735820
 *   @ cycles 500
 *   @ slots 11
 *   # render: bins 1700
 *   # tick cycle slot parent ratio freq_hz pan reps level jitter
 *   0 0 0 - - 1299.5451597662952 0.26 13 1 -
 *   1 0 1 0 5/16 406.10786242696724 0.70 3 0 49
 *
 * Lines starting with '#' are comments, with two exceptions: the first line
 * names the format version and is checked, and "# render: key value" lines
 * are remembered as render notes. '@' lines hold what the composer used.
 * Every other line is one tone.
 */
#ifndef CHIMES_SCORE_TEXT_H
#define CHIMES_SCORE_TEXT_H

#include <stdio.h>

#include "score/tone.h"

/* Writes the whole score. Returns 0, or -1 on a write error. */
int score_write(FILE *file, const Score *score);

/* Writes one tone line (no header). Returns 0, or -1 on a write error. */
int score_write_tone(FILE *file, const Score *score, const Tone *tone);

/* Reads a score written by score_write into an initialised, empty Score.
 * Only the root's frequency is taken from the file; all others are
 * recomputed down the lineage. The result has passed score_check.
 * Returns 0, or -1 with a message in err. */
int score_read(FILE *file, Score *score, char *err, size_t errlen);

#endif
