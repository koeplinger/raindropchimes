/* rules.c -- the built-in rule sets as data, and their start-up check. */
#include "compose/rules.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "score/tone.h"   /* SCORE_NAME_LEN: a score records the rule set's name */

/* Sanity limits. They keep tick and cycle numbers far inside the range of a
 * 32-bit whole number: a tone holds its slot for slots * (reps + 1) ticks,
 * and a piece is at least end_cycles + 2 cycles long. */
#define RULES_MAX_SLOTS      1000
#define RULES_MAX_REP_COUNT  1000
#define RULES_MAX_END_CYCLES 1000000

/* To add a rule set, add one more block of constants to this list. */
static const Rules BUILT_IN[] = {

    /* v4: the program as archived on 14 February 2011. It made the favourite
     * tune. The 17 February version (v5) composes by the same rules. */
    {
        .name = "v4",

        .slots      = 11,
        .end_cycles = 30,

        .n_ratios = 25,
        .ratios = {
            {5, 16}, {5, 8}, {5, 4}, {5, 2}, {5, 1},     /* 5, shifted by powers of two */
            {3, 8},  {3, 4}, {3, 2}, {3, 1}, {6, 1},     /* 3                           */
            {1, 4},  {1, 2}, {1, 1}, {2, 1}, {4, 1},     /* 1                           */
            {1, 6},  {1, 3}, {2, 3}, {4, 3}, {8, 3},     /* 1/3                         */
            {1, 5},  {2, 5}, {4, 5}, {8, 5}, {16, 5}     /* 1/5                         */
        },
        .freq_min = 50.0,
        .freq_max = 16000.0,

        .n_reps = 11,
        .reps   = {1, 2, 3, 5, 7, 11, 13, 17, 23, 29, 31},
        .first_reach     = 11,
        .reach_threshold = 10,
        .full_reach      = 11,

        .density_gain = 1.0,

        .level_rule    = LEVEL_ON_OFF,
        .level_scale   = 150.0,
        .level_offset  = 0.05,
        .counted_above = 0.0,   /* levels are only 0 and 1 here: counted means level 1 */
        .first_level   = 1.0,
        .lone_level    = 1.0,

        .jitter_range = 100
    },

    /* v6: the program as archived on 18 February 2011. The v4 tables, but
     * the other level rule: every tone sounds, at a level of its own. So
     * there are no tracked tones, and every tone is counted. */
    {
        .name = "v6",

        .slots      = 11,
        .end_cycles = 30,

        .n_ratios = 25,
        .ratios = {
            {5, 16}, {5, 8}, {5, 4}, {5, 2}, {5, 1},
            {3, 8},  {3, 4}, {3, 2}, {3, 1}, {6, 1},
            {1, 4},  {1, 2}, {1, 1}, {2, 1}, {4, 1},
            {1, 6},  {1, 3}, {2, 3}, {4, 3}, {8, 3},
            {1, 5},  {2, 5}, {4, 5}, {8, 5}, {16, 5}
        },
        .freq_min = 50.0,
        .freq_max = 16000.0,

        .n_reps = 11,
        .reps   = {1, 2, 3, 5, 7, 11, 13, 17, 23, 29, 31},
        .first_reach     = 11,
        .reach_threshold = 10,
        .full_reach      = 11,

        .density_gain = 1.0,

        .level_rule    = LEVEL_CONTINUOUS,
        .level_scale   = 150.0,
        .level_offset  = 0.05,
        .counted_above = 0.0,
        .first_level   = 0.7,
        .lone_level    = 0.7,

        .jitter_range = 100
    },

    /* v10: the code in legacy/ (its banner says 4 March 2011). As v6, but
     * with a narrower frequency range (the first tone may still lie outside
     * it), another end of the repetition table, a first tone that draws
     * from the first 4 repetition counts only, a density curve that rises
     * 2.5 times as fast and then stays at its top, and only tones with a
     * level above 0.69 counted. */
    {
        .name = "v10",

        .slots      = 11,
        .end_cycles = 30,

        .n_ratios = 25,
        .ratios = {
            {5, 16}, {5, 8}, {5, 4}, {5, 2}, {5, 1},
            {3, 8},  {3, 4}, {3, 2}, {3, 1}, {6, 1},
            {1, 4},  {1, 2}, {1, 1}, {2, 1}, {4, 1},
            {1, 6},  {1, 3}, {2, 3}, {4, 3}, {8, 3},
            {1, 5},  {2, 5}, {4, 5}, {8, 5}, {16, 5}
        },
        .freq_min = 100.0,
        .freq_max = 3000.0,

        .n_reps = 11,
        .reps   = {1, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29},
        .first_reach     = 4,
        .reach_threshold = 10,
        .full_reach      = 11,

        .density_gain = 2.5,

        .level_rule    = LEVEL_CONTINUOUS,
        .level_scale   = 150.0,
        .level_offset  = 0.05,
        .counted_above = 0.69,
        .first_level   = 0.7,
        .lone_level    = 0.7,

        .jitter_range = 100
    }
};

int rules_count(void)
{
    return (int) (sizeof BUILT_IN / sizeof BUILT_IN[0]);
}

const Rules *rules_at(int index)
{
    if (index < 0 || index >= rules_count()) return NULL;
    return &BUILT_IN[index];
}

const Rules *rules_find(const char *name)
{
    int i;
    if (!name) return NULL;
    for (i = 0; i < rules_count(); i++)
        if (strcmp(BUILT_IN[i].name, name) == 0) return &BUILT_IN[i];
    return NULL;
}

/* Writes "rule set NAME: message" into err and returns -1. */
static int refuse(const Rules *rules, char *err, size_t errlen, const char *format, ...)
{
    char message[200];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof message, format, args);
    va_end(args);
    snprintf(err, errlen, "rule set %s: %s", rules->name ? rules->name : "(no name)", message);
    return -1;
}

static int is_fraction(double value)   /* a number from 0 to 1 */
{
    return value >= 0.0 && value <= 1.0;
}

int rules_check(const Rules *rules, char *err, size_t errlen)
{
    int i, largest_d;

    /* The name goes into the score file as one word. */
    if (!rules->name || rules->name[0] == '\0' || strlen(rules->name) >= SCORE_NAME_LEN
            || strpbrk(rules->name, " \t\r\n"))
        return refuse(rules, err, errlen, "name must be one word of 1 to %d characters",
                      SCORE_NAME_LEN - 1);

    if (rules->slots < 1 || rules->slots > RULES_MAX_SLOTS)
        return refuse(rules, err, errlen, "slots is %d; it must be 1 to %d",
                      rules->slots, RULES_MAX_SLOTS);
    if (rules->end_cycles < 0 || rules->end_cycles > RULES_MAX_END_CYCLES)
        return refuse(rules, err, errlen, "end_cycles is %d; it must be 0 to %d",
                      rules->end_cycles, RULES_MAX_END_CYCLES);

    /* The ratio table and the frequency range. */
    if (rules->n_ratios < 1 || rules->n_ratios > RULES_MAX_RATIOS)
        return refuse(rules, err, errlen, "n_ratios is %d; the ratio table holds 1 to %d ratios",
                      rules->n_ratios, RULES_MAX_RATIOS);
    for (i = 0; i < rules->n_ratios; i++)
        if (rules->ratios[i].num < 1 || rules->ratios[i].den < 1)
            return refuse(rules, err, errlen, "ratios[%d] is %d/%d; both numbers must be at least 1",
                          i, rules->ratios[i].num, rules->ratios[i].den);
    if (!isfinite(rules->freq_min) || !isfinite(rules->freq_max) || !(rules->freq_min > 0.0))
        return refuse(rules, err, errlen, "freq_min and freq_max must be positive numbers");
    if (rules->freq_min > rules->freq_max)
        return refuse(rules, err, errlen, "the frequency range is empty: freq_min %g is above "
                      "freq_max %g", rules->freq_min, rules->freq_max);

    /* The repetition table. */
    if (rules->n_reps < 1 || rules->n_reps > RULES_MAX_REPS)
        return refuse(rules, err, errlen, "n_reps is %d; the repetition table holds 1 to %d counts",
                      rules->n_reps, RULES_MAX_REPS);
    for (i = 0; i < rules->n_reps; i++)
        if (rules->reps[i] < 1 || rules->reps[i] > RULES_MAX_REP_COUNT)
            return refuse(rules, err, errlen, "reps[%d] is %d; every repetition count must be 1 to %d",
                          i, rules->reps[i], RULES_MAX_REP_COUNT);

    /* The repetition draw must never reach past the end of its table. (Such
     * an out-of-range read is what ended the favourite tune in 2011.) */
    if (rules->first_reach < 1 || rules->first_reach > rules->n_reps)
        return refuse(rules, err, errlen, "first_reach is %d; it must be 1 to n_reps (%d)",
                      rules->first_reach, rules->n_reps);
    if (rules->full_reach < 1 || rules->full_reach > rules->n_reps)
        return refuse(rules, err, errlen, "full_reach is %d; it must be 1 to n_reps (%d)",
                      rules->full_reach, rules->n_reps);
    if (rules->reach_threshold < 0)
        return refuse(rules, err, errlen, "reach_threshold is %d; it must not be negative",
                      rules->reach_threshold);
    /* Later tones draw from the first d + 2 entries while d is below
     * reach_threshold. d counts slots, so it is at most the slot count. */
    largest_d = rules->reach_threshold - 1;
    if (largest_d > rules->slots) largest_d = rules->slots;
    if (largest_d + 2 > rules->n_reps)
        return refuse(rules, err, errlen, "reach_threshold is %d, so the repetition draw can reach "
                      "%d entries, but the table has only %d (n_reps)",
                      rules->reach_threshold, largest_d + 2, rules->n_reps);

    /* Density and level. */
    if (!isfinite(rules->density_gain) || rules->density_gain < 0.0)
        return refuse(rules, err, errlen, "density_gain must be a number, and not negative");
    if (rules->level_rule != LEVEL_ON_OFF && rules->level_rule != LEVEL_CONTINUOUS)
        return refuse(rules, err, errlen, "level_rule is not one of the known level rules");
    if (!isfinite(rules->level_scale) || rules->level_scale < 0.0)
        return refuse(rules, err, errlen, "level_scale must be a number, and not negative");
    if (!isfinite(rules->level_offset) || rules->level_offset < 0.0)
        return refuse(rules, err, errlen, "level_offset must be a number, and not negative");
    if (!(rules->counted_above >= 0.0 && rules->counted_above < 1.0))
        return refuse(rules, err, errlen, "counted_above must be at least 0 and below 1");
    if (!is_fraction(rules->first_level))
        return refuse(rules, err, errlen, "first_level must be 0 to 1");
    if (!is_fraction(rules->lone_level))
        return refuse(rules, err, errlen, "lone_level must be 0 to 1");

    if (rules->jitter_range < 1)
        return refuse(rules, err, errlen, "jitter_range is %d; it must be at least 1",
                      rules->jitter_range);
    return 0;
}
