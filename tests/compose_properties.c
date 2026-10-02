/* compose_properties.c -- test helper: what must hold for every tune the
 * composer makes, tried over 1,000 seeds per built-in rule set, and what the
 * composer must refuse (docs/REVAMP_PLAN.md, section 7, item 8).
 *
 * Prints one line per failure and a summary. Exit status 0 = all good.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compose/compose.h"

#define SEEDS         1000
#define CYCLES        500
#define OTHER_CYCLES  400    /* must give a different tune */
#define VARIANT_SEEDS 100

/* Half the seeds are clock seeds from the evening of the favourite tune on;
 * the other half are spread over the whole 32-bit range. */
static uint32_t seed_number(int i)
{
    if (i < SEEDS / 2) return 1297735821u + (uint32_t) i;
    return 12345u + (uint32_t) (i - SEEDS / 2) * 8589934u;
}

/* Composes into an initialised, empty score. Returns 0, or -1 after a message. */
static int compose(const Rules *rules, uint32_t seed, int cycles, Score *score)
{
    Rng rng;
    char err[300];

    rng_seed(&rng, seed);
    if (slotwalk_compose(rules, &rng, cycles, score, err, sizeof err) != 0) {
        printf("FAIL rules %s, seed %lu, %d cycles: the composer did not finish: %s\n",
               rules->name, (unsigned long) seed, cycles, err);
        return -1;
    }
    return 0;
}

static int same_tone(const Tone *a, const Tone *b)
{
    return a->tick == b->tick && a->parent == b->parent && a->num == b->num && a->den == b->den
        && a->freq_hz == b->freq_hz && a->pan == b->pan && a->reps == b->reps
        && a->level == b->level && a->jitter == b->jitter;
}

static int same_tones(const Score *a, const Score *b)
{
    int32_t i;
    if (a->n_tones != b->n_tones) return 0;
    for (i = 0; i < a->n_tones; i++)
        if (!same_tone(&a->tones[i], &b->tones[i])) return 0;
    return 1;
}

static int ratio_in_table(const Rules *rules, const Tone *tone)
{
    int k;
    for (k = 0; k < rules->n_ratios; k++)
        if (rules->ratios[k].num == tone->num && rules->ratios[k].den == tone->den) return 1;
    return 0;
}

/* Is the repetition count one of the first `reach` entries of the table? */
static int reps_in_table(const Rules *rules, const Tone *tone, int reach)
{
    int k;
    for (k = 0; k < reach; k++)
        if (rules->reps[k] == tone->reps) return 1;
    return 0;
}

/* How many slots hold a counted tone, going by the latest tone of each slot. */
static int counted_now(const Rules *rules, const Score *score, const int32_t *latest)
{
    int32_t slot;
    int d = 0;
    for (slot = 0; slot < score->slots; slot++)
        if (latest[slot] >= 0 && score_tone_at(score, latest[slot])->level > rules->counted_above)
            d++;
    return d;
}

/* Checks one tone. latest[slot] is the tick of the latest tone created in
 * that slot before this one (-1: none yet), so that tone is the one holding
 * the slot now. Returns NULL if all is well, else what is wrong. */
static const char *tone_problem(const Rules *rules, const Score *score, const Tone *tone,
                                const int32_t *latest)
{
    int32_t slots = score->slots;
    int32_t slot = tone->tick % slots;
    int32_t end_phase_tick = slots * (score->cycles - rules->end_cycles);
    double pan_steps = tone->pan * 100.0;

    if (tone->tick >= end_phase_tick) return "it was created in the end phase";

    if (tone->parent < 0) {
        /* The root tone. Its frequency need not lie in the range for new tones. */
        if (tone->tick != 0) return "a root tone that is not at tick 0";
        if (tone->level != rules->first_level) return "the root's level is not first_level";
        if (tone->jitter != -1) return "the root has a jitter";
        if (!reps_in_table(rules, tone, rules->first_reach))
            return "the root's repetition count is not within first_reach of the table";
    } else {
        int32_t previous_slot = (slot + slots - 1) % slots;
        int d = counted_now(rules, score, latest);
        int reach = (d < rules->reach_threshold) ? d + 2 : rules->full_reach;

        if (latest[previous_slot] < 0 || tone->parent != latest[previous_slot])
            return "its parent was not the tone holding the previous slot";
        if (!ratio_in_table(rules, tone)) return "its ratio is not in the ratio table";
        if (tone->freq_hz < rules->freq_min || tone->freq_hz > rules->freq_max)
            return "its frequency is outside the range for new tones";
        if (tone->jitter < 0 || tone->jitter >= rules->jitter_range)
            return "its jitter is outside the jitter range";
        if (!reps_in_table(rules, tone, reach))
            return "its repetition count lies further into the table than the counted tones allow";
        if (d == 0 && tone->level != rules->lone_level
                && !(rules->level_rule == LEVEL_ON_OFF && tone->level == 1.0))
            return "nothing was counted, yet its level is not lone_level";
    }

    if (pan_steps < 0.0 || pan_steps > 100.0 || fabs(pan_steps - floor(pan_steps + 0.5)) > 1e-9)
        return "its pan is not one of 0.00, 0.01, .. 1.00";
    if (rules->level_rule == LEVEL_ON_OFF && tone->level != 0.0 && tone->level != 1.0)
        return "its level is neither 0 nor 1 (on/off level rule)";
    if (rules->level_rule == LEVEL_CONTINUOUS && !(tone->level >= 0.0749 && tone->level <= 1.0))
        return "its level is outside 0.075 .. 1 (continuous level rule)";

    /* A slot gets its first tone in cycle 0, and each later tone at the very
     * tick the tone before it has had its last strike. */
    if (latest[slot] < 0) {
        if (tone->tick != slot) return "the first tone of its slot was not created in cycle 0";
    } else {
        const Tone *before = score_tone_at(score, latest[slot]);
        if (tone->tick != score_tone_end(score, before))
            return "it was not created at the tick its slot became free";
    }
    return NULL;
}

/* Checks a whole tune. Returns the number of failures (0 or 1). */
static int check_tune(const Rules *rules, const Score *score, uint32_t seed)
{
    int32_t slots = score->slots;
    int32_t end_phase_tick = slots * (score->cycles - rules->end_cycles);
    int32_t *latest = malloc((size_t) slots * sizeof *latest);
    const char *problem = NULL;
    int32_t problem_tick = -1;
    char err[300];
    int32_t i, slot;

    if (!latest) {
        printf("FAIL out of memory\n");
        return 1;
    }
    for (slot = 0; slot < slots; slot++) latest[slot] = -1;

    if (score_check(score, err, sizeof err) != 0) problem = err;
    if (!problem && (strcmp(score->rules, rules->name) != 0 || slots != rules->slots))
        problem = "the score does not record the rule set's name and slot count";

    for (i = 0; i < score->n_tones && !problem; i++) {
        const Tone *tone = &score->tones[i];
        problem = tone_problem(rules, score, tone, latest);
        problem_tick = tone->tick;
        latest[tone->tick % slots] = tone->tick;
    }

    /* Before the end phase no slot may stay free. */
    for (slot = 0; slot < slots && !problem; slot++) {
        problem_tick = latest[slot];
        if (latest[slot] < 0)
            problem = "a slot never got a tone";
        else if (score_tone_end(score, score_tone_at(score, latest[slot])) < end_phase_tick)
            problem = "its slot was left free before the end phase";
    }

    if (problem)
        printf("FAIL rules %s, seed %lu, %d cycles, tone at tick %d: %s\n", rules->name,
               (unsigned long) seed, (int) score->cycles, (int) problem_tick, problem);
    free(latest);
    return problem ? 1 : 0;
}

/* The same rules, seed and cycles must give the same tones, whatever else
 * has happened: here with the rules copied to another place, a generator and
 * a score that have been used before, and another tune composed in between. */
static int check_repeatable(const Rules *rules, uint32_t seed, const Score *first, Score *scratch)
{
    Rules copy = *rules;
    Rng rng;
    char err[300];
    int bad = 0;

    rng_seed(&rng, seed ^ 0x5a5a5a5au);
    if (slotwalk_compose(&copy, &rng, CYCLES - 7, scratch, err, sizeof err) != 0) bad = 1;
    score_free(scratch);

    rng_seed(&rng, seed);
    if (slotwalk_compose(&copy, &rng, CYCLES, scratch, err, sizeof err) != 0) bad = 1;
    if (!bad && !same_tones(first, scratch)) bad = 1;
    score_free(scratch);

    if (bad)
        printf("FAIL rules %s, seed %lu: composing the same tune again gave different tones\n",
               rules->name, (unsigned long) seed);
    return bad;
}

/* Tries a rule set over many seeds. Returns the number of failures. */
static int check_rule_set(const Rules *rules, int seeds, int repeat_too)
{
    Score score, other;
    long tones = 0, tracked = 0;
    int bad = 0, i;

    score_init(&score);
    score_init(&other);
    for (i = 0; i < seeds; i++) {
        uint32_t seed = seed_number(i);
        int32_t k;

        if (compose(rules, seed, CYCLES, &score) != 0) { bad++; continue; }
        bad += check_tune(rules, &score, seed);
        tones += score.n_tones;
        for (k = 0; k < score.n_tones; k++)
            if (score.tones[k].level == 0.0) tracked++;

        /* Another number of cycles gives another tune. */
        if (compose(rules, seed, OTHER_CYCLES, &other) != 0) {
            bad++;
        } else {
            bad += check_tune(rules, &other, seed);
            if (same_tones(&score, &other)) {
                printf("FAIL rules %s, seed %lu: %d and %d cycles give the same tones\n",
                       rules->name, (unsigned long) seed, CYCLES, OTHER_CYCLES);
                bad++;
            }
        }
        score_free(&other);

        if (repeat_too) bad += check_repeatable(rules, seed, &score, &other);
        score_free(&score);
    }
    printf("rules %s: %d seeds, %ld tones (%ld of them tracked), %d failures\n",
           rules->name, seeds, tones, tracked, bad);
    return bad;
}

/* The list of built-in rule sets must be in order, and every one of them
 * must pass the start-up check and the properties above. */
static int check_built_in_rule_sets(void)
{
    char err[300];
    int bad = 0, i;

    if (rules_count() < 1 || !rules_find("v4")) {
        printf("FAIL the rule set v4 is not built in\n");
        bad++;
    }
    if (rules_at(-1) || rules_at(rules_count()) || rules_find("no-such-rules") || rules_find(NULL)) {
        printf("FAIL rules_at or rules_find returns a rule set that does not exist\n");
        bad++;
    }
    for (i = 0; i < rules_count(); i++) {
        const Rules *rules = rules_at(i);
        if (rules_find(rules->name) != rules) {
            printf("FAIL rule set %d (%s) is not found by its name\n", i, rules->name);
            bad++;
        }
        if (rules_check(rules, err, sizeof err) != 0) {
            printf("FAIL the built-in rule set fails the start-up check: %s\n", err);
            bad++;
            continue;
        }
        bad += check_rule_set(rules, SEEDS, 1);
    }
    return bad;
}

/* A broken rule set must be refused by the start-up check, with a message
 * that names the field, and by the composer, which must leave the score empty. */
static int expect_refused(const Rules *broken, const char *field, const char *what)
{
    Score score;
    Rng rng;
    char err[300] = "";
    int bad = 0;

    if (rules_check(broken, err, sizeof err) == 0) {
        printf("FAIL the start-up check accepts %s\n", what);
        bad = 1;
    } else if (!strstr(err, field)) {
        printf("FAIL %s: the message does not name %s: %s\n", what, field, err);
        bad = 1;
    }

    score_init(&score);
    rng_seed(&rng, 1);
    if (slotwalk_compose(broken, &rng, CYCLES, &score, err, sizeof err) == 0 || score.n_tones != 0) {
        printf("FAIL the composer accepts %s\n", what);
        bad = 1;
    }
    score_free(&score);
    return bad;
}

static int check_broken_rule_sets(const Rules *good)
{
    Rules broken;
    int bad = 0;

    broken = *good;  broken.n_reps = good->n_reps - 1;
    bad += expect_refused(&broken, "first_reach", "a repetition table shorter than the first tone's draw reaches");

    broken = *good;  broken.reach_threshold = good->n_reps + 6;
    bad += expect_refused(&broken, "reach_threshold", "a repetition table shorter than the d + 2 draw reaches");

    broken = *good;  broken.full_reach = good->n_reps + 1;
    bad += expect_refused(&broken, "full_reach", "a full reach past the end of the repetition table");

    broken = *good;  broken.first_reach = 0;
    bad += expect_refused(&broken, "first_reach", "a first reach of zero");

    broken = *good;  broken.reps[4] = 0;
    bad += expect_refused(&broken, "reps[4]", "a repetition count of zero");

    broken = *good;  broken.n_reps = RULES_MAX_REPS + 1;
    bad += expect_refused(&broken, "n_reps", "a repetition table larger than its limit");

    broken = *good;  broken.freq_min = good->freq_max;  broken.freq_max = good->freq_min;
    bad += expect_refused(&broken, "freq_min", "an empty frequency range");

    broken = *good;  broken.n_ratios = 0;
    bad += expect_refused(&broken, "n_ratios", "an empty ratio table");

    broken = *good;  broken.n_ratios = RULES_MAX_RATIOS + 1;
    bad += expect_refused(&broken, "n_ratios", "a ratio table larger than its limit");

    broken = *good;  broken.ratios[7].den = 0;
    bad += expect_refused(&broken, "ratios[7]", "a ratio with a zero in it");

    broken = *good;  broken.slots = 0;
    bad += expect_refused(&broken, "slots", "a cycle of zero slots");

    broken = *good;  broken.end_cycles = -1;
    bad += expect_refused(&broken, "end_cycles", "a negative end phase");

    broken = *good;  broken.jitter_range = 0;
    bad += expect_refused(&broken, "jitter_range", "an empty jitter range");

    broken = *good;  broken.first_level = 1.5;
    bad += expect_refused(&broken, "first_level", "a first level above 1");

    broken = *good;  broken.name = "two words";
    bad += expect_refused(&broken, "name", "a name that is not one word");

    printf("broken rule sets: %d failures\n", bad);
    return bad;
}

/* The composer must refuse what it cannot do, and say why. */
static int check_refusals(const Rules *good)
{
    Rules unreachable = *good;
    Score score;
    Rng rng;
    char err[300];
    int bad = 0;

    score_init(&score);
    rng_seed(&rng, 1);

    /* Too few cycles: with one cycle less than end_cycles + 2 the density
     * formula would divide by zero. */
    err[0] = '\0';
    if (slotwalk_compose(good, &rng, good->end_cycles + 1, &score, err, sizeof err) == 0
            || !strstr(err, "cycles") || score.n_tones != 0) {
        printf("FAIL %d cycles are not refused properly: %s\n", good->end_cycles + 1, err);
        bad++;
    }
    if (slotwalk_compose(good, &rng, 0, &score, err, sizeof err) == 0
            || slotwalk_compose(good, &rng, -5, &score, err, sizeof err) == 0) {
        printf("FAIL zero or negative cycles are not refused\n");
        bad++;
    }

    /* The smallest number of cycles is accepted and gives a sound tune. */
    if (compose(good, 1, good->end_cycles + COMPOSE_MIN_CYCLES_MARGIN, &score) != 0) bad++;
    else bad += check_tune(good, &score, 1);

    /* A score that already holds tones is not composed into. */
    if (slotwalk_compose(good, &rng, CYCLES, &score, err, sizeof err) == 0) {
        printf("FAIL the composer fills a score that is not empty\n");
        bad++;
    }
    score_free(&score);

    /* A frequency range that no tone can ever reach passes the start-up
     * check, but the ratio redraw must give up instead of running for ever. */
    unreachable.name = "unreachable";
    unreachable.freq_min = 1e6;
    unreachable.freq_max = 2e6;
    err[0] = '\0';
    rng_seed(&rng, 1);
    if (rules_check(&unreachable, err, sizeof err) != 0
            || slotwalk_compose(&unreachable, &rng, CYCLES, &score, err, sizeof err) == 0
            || !strstr(err, "10000") || score.n_tones != 0) {
        printf("FAIL the ratio redraw does not give up properly: %s\n", err);
        bad++;
    }
    score_free(&score);

    printf("refusals: %d failures\n", bad);
    return bad;
}

/* The algorithm is generic over the fields of a rule set. These variants are
 * not built in (they only exist here); each must still give sound tunes. */
static int check_variants(const Rules *good)
{
    static const int other_reps[11] = {1, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
    Rules variant;
    int bad = 0;

    /* The other level rule, with the numbers the 18 February version used. */
    variant = *good;
    variant.name = "test-continuous";
    variant.level_rule = LEVEL_CONTINUOUS;
    variant.first_level = 0.7;
    variant.lone_level = 0.7;
    bad += check_rule_set(&variant, VARIANT_SEEDS, 1);

    /* On top of that: a narrower range (the root may lie outside it), another
     * repetition table, a shorter first reach, a density gain, and a
     * threshold for counting. */
    variant.name = "test-narrow";
    variant.freq_min = 100.0;
    variant.freq_max = 3000.0;
    memcpy(variant.reps, other_reps, sizeof other_reps);
    variant.first_reach = 4;
    variant.density_gain = 2.5;
    variant.counted_above = 0.69;
    bad += check_rule_set(&variant, VARIANT_SEEDS, 1);

    /* Other slot counts, down to a single slot (each tone then derives from
     * the tone it replaces). */
    variant = *good;
    variant.name = "test-7-slots";
    variant.slots = 7;
    bad += check_rule_set(&variant, VARIANT_SEEDS, 0);

    variant.name = "test-1-slot";
    variant.slots = 1;
    bad += check_rule_set(&variant, VARIANT_SEEDS, 0);

    variant = *good;
    variant.name = "test-short-end";
    variant.end_cycles = 0;
    variant.jitter_range = 7;
    bad += check_rule_set(&variant, VARIANT_SEEDS, 0);

    return bad;
}

int main(void)
{
    const Rules *v4 = rules_find("v4");
    int bad;

    /* Print line by line, so that the messages survive if a later step crashes. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    bad = check_built_in_rule_sets();

    if (v4) {
        bad += check_broken_rule_sets(v4);
        bad += check_refusals(v4);
        bad += check_variants(v4);
    }
    printf("%s\n", bad ? "composer properties: FAILED" : "composer properties: all hold");
    return bad ? 1 : 0;
}
