/* rules.h -- rule sets for the slot algorithm: the tables and numbers that
 * steer it. A rule set is data. Once a tune has been made with a named rule
 * set, that rule set is never changed; a changed one gets a new name.
 */
#ifndef CHIMES_RULES_H
#define CHIMES_RULES_H

#include <stddef.h>

#define RULES_MAX_RATIOS 32
#define RULES_MAX_REPS   32

typedef enum {
    LEVEL_ON_OFF,      /* always draws: level 1 if scale * (density + offset) > r, or if nothing is counted; else 0 */
    LEVEL_CONTINUOUS   /* nothing counted: lone_level, no draw; else min(1, scale * (density + offset) / (1 + r))   */
} LevelRule;

typedef struct {
    const char *name;

    int slots;                    /* slots per cycle                                          */
    int end_cycles;               /* the end phase: no tones are created in the final cycles  */

    int n_ratios;                 /* the ratio table; its order matters                       */
    struct { int num, den; } ratios[RULES_MAX_RATIOS];
    double freq_min, freq_max;    /* range for new tones, both ends included                  */

    int n_reps;                   /* the repetition table                                     */
    int reps[RULES_MAX_REPS];
    int first_reach;              /* first tone: its draw reaches this many entries           */
    int reach_threshold;          /* later tones: d + 2 entries while d < reach_threshold ... */
    int full_reach;               /* ... else this many                                       */

    double density_gain;          /* g in density = min(1, g * sin(pi * c / (T - end_cycles - 1)))^2 */

    LevelRule level_rule;
    double level_scale;           /* 150  */
    double level_offset;          /* 0.05 */
    double counted_above;         /* a tone is counted (in d) if its level is above this      */
    double first_level;           /* level of the first tone                                  */
    double lone_level;            /* level of a new tone when nothing is counted              */

    int jitter_range;             /* jitter is drawn from 0 .. jitter_range - 1               */
} Rules;

/* The built-in rule sets. rules_find returns NULL for an unknown name. */
const Rules *rules_find(const char *name);
int          rules_count(void);
const Rules *rules_at(int index);

/* The start-up check: every repetition count is at least 1; the repetition
 * draw can never reach past the end of its table; the frequency range is not
 * empty; the tables fit. Returns 0, or -1 with a message naming the field. */
int rules_check(const Rules *rules, char *err, size_t errlen);

#endif
