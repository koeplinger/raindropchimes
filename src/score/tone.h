/* tone.h -- the score: the contract between composer and synthesizer.
 *
 * A score is the full list of tones plus the settings the composer used.
 * It has no tempo, no sample rate, and nothing about the sound.
 */
#ifndef CHIMES_TONE_H
#define CHIMES_TONE_H

#include <stddef.h>
#include <stdint.h>

/* One tone: what the composer creates at a tick. */
typedef struct {
    int32_t tick;      /* creation tick = the tone's identity                     */
    int32_t parent;    /* tick of the tone it derives from; -1 for the root       */
    int32_t num, den;  /* exact ratio to the parent (0, 0 for the root)           */
    double  freq_hz;   /* root: drawn. Others: parent's freq_hz * (num / den)     */
    double  pan;       /* 0 = left .. 1 = right                                   */
    int32_t reps;      /* N >= 1: strikes 0..N, one per cycle                     */
    double  level;     /* 0 = tracked, else up to 1                               */
    int32_t jitter;    /* 0..99, or -1 for none (root)                            */
} Tone;

#define SCORE_NAME_LEN     32
#define SCORE_RENDER_NOTES 16

/* One "# render: key value" line: a record of how audio was made from the
 * score. Never part of the composition. */
typedef struct {
    char key[16];
    char value[512];
} RenderNote;

typedef struct {
    char     rules[SCORE_NAME_LEN];      /* name of the rule set               */
    char     generator[SCORE_NAME_LEN];  /* name of the random generator       */
    uint32_t seed;
    int32_t  cycles;                     /* T, as given to the composer        */
    int32_t  slots;                      /* slots per cycle                    */
    Tone    *tones;                      /* in order of tick                   */
    int32_t  n_tones;
    int32_t  capacity;
    RenderNote render[SCORE_RENDER_NOTES];
    int32_t  n_render;
} Score;

void score_init(Score *score);
void score_free(Score *score);

/* Appends a tone. Returns 0, or -1 if out of memory. */
int score_add(Score *score, const Tone *tone);

/* The tone created at the given tick, or NULL. */
const Tone *score_tone_at(const Score *score, int32_t tick);

/* The tick at which a tone stops holding its slot: its successor may be
 * created there. */
int32_t score_tone_end(const Score *score, const Tone *tone);

/* Sets (or replaces) a render note; score_render_note returns NULL if absent. */
void        score_set_render_note(Score *score, const char *key, const char *value);
const char *score_render_note(const Score *score, const char *key);
void        score_clear_render_notes(Score *score);

/* Checks what the score format requires of any composing algorithm:
 * ticks increase; exactly one root, and it comes first; every parent is an
 * earlier tone; every frequency equals parent * (num / den); every repetition
 * count is at least 1; no two tones hold the same slot at the same time.
 * Returns 0 if the score is sound, else -1 with a message in err. */
int score_check(const Score *score, char *err, size_t errlen);

#endif
