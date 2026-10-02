/* compose_fields.c -- test helper: the rule fields that are the same in every
 * built-in rule set (full_reach, level_scale, level_offset), or the same in
 * every one that shares a level rule (lone_level). No reference tone list
 * can show whether the composer really uses them: a composer with 11, 150
 * and 0.05 written into its code would reproduce every list.
 *
 * So each field is tried here with a test-only rule set, made so that the
 * outcome can be worked out by hand, whatever the generator draws.
 *
 * Two details of the rules are tried the same way, because no reference list
 * happens to show them either: the frequency range includes both its ends,
 * and the density is held at 1 before it is squared.
 *
 * Prints one line per failure and a summary. Exit status 0 = all good.
 */
#include <stdio.h>

#include "compose/compose.h"

#define SEEDS 20

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

/* full_reach, part one. With first_reach 1, reach_threshold 0 and full_reach 1
 * every repetition draw reaches only the first entry of the table, which
 * is 1. So every tone has repetition count 1 and holds its slot for 2 cycles
 * (22 ticks), and the whole tune is known beforehand:
 *
 *   - with 40 cycles, tones are created up to cycle 40 - 31 = 9, so in cycles
 *     0, 2, 4, 6 and 8: 5 times 11 tones = 55 tones;
 *   - tone number i (counting from 0) is created at tick 22 * (i div 11) + (i mod 11);
 *   - its parent holds the slot before it: that is the tone created one tick
 *     earlier, except in slot 0, where the tone in slot 10 is 12 ticks old.
 *
 * A composer that ignored full_reach would draw from all 11 entries. */
static int check_full_reach_one(const Rules *v4)
{
    Rules variant = *v4;
    Score score;
    int bad = 0;
    uint32_t seed;

    variant.name = "test-reach-1";
    variant.first_reach = 1;
    variant.reach_threshold = 0;
    variant.full_reach = 1;

    score_init(&score);
    for (seed = 1; seed <= SEEDS; seed++) {
        const char *problem = NULL;
        int32_t i;

        if (compose(&variant, seed, 40, &score) != 0) { bad++; continue; }
        if (score.n_tones != 55) problem = "there are not exactly 55 tones";
        for (i = 0; i < score.n_tones && !problem; i++) {
            const Tone *tone = &score.tones[i];
            int32_t slot = i % 11;
            int32_t tick = 22 * (i / 11) + slot;
            int32_t parent = (i == 0) ? -1 : (slot == 0) ? tick - 12 : tick - 1;

            if (tone->reps != 1) problem = "a repetition count is not 1";
            else if (tone->tick != tick) problem = "a tone was not created at the expected tick";
            else if (tone->parent != parent) problem = "a tone does not have the expected parent";
        }
        if (problem) {
            printf("FAIL rules %s, seed %lu: %s\n", variant.name, (unsigned long) seed, problem);
            bad++;
        }
        score_free(&score);
    }
    printf("full_reach 1: %d seeds, every tune is the 55 tones worked out by hand; %d failures\n",
           SEEDS, bad);
    return bad;
}

/* full_reach, part two. With full_reach 2 the draw reaches the first two
 * entries, 1 and 2: every repetition count after the first tone's is 1 or 2,
 * and over a whole tune both must turn up. */
static int check_full_reach_two(const Rules *v4)
{
    Rules variant = *v4;
    Score score;
    int bad = 0;
    uint32_t seed;

    variant.name = "test-reach-2";
    variant.first_reach = 1;
    variant.reach_threshold = 0;
    variant.full_reach = 2;

    score_init(&score);
    for (seed = 1; seed <= SEEDS; seed++) {
        long ones = 0, twos = 0, others = 0;
        int32_t i;

        if (compose(&variant, seed, 100, &score) != 0) { bad++; continue; }
        for (i = 0; i < score.n_tones; i++) {
            if (score.tones[i].reps == 1) ones++;
            else if (score.tones[i].reps == 2) twos++;
            else others++;
        }
        if (others != 0 || ones == 0 || twos == 0) {
            printf("FAIL rules %s, seed %lu: repetition counts 1, 2, other: %ld, %ld, %ld tones "
                   "(expected: some, some, none)\n", variant.name, (unsigned long) seed,
                   ones, twos, others);
            bad++;
        }
        score_free(&score);
    }
    printf("full_reach 2: %d seeds, every repetition count is 1 or 2; %d failures\n", SEEDS, bad);
    return bad;
}

/* level_scale and level_offset. Both level rules compare or divide
 *     weight = level_scale * (density + level_offset)
 * with a draw r of 0..99, and density lies between 0 and 1:
 *
 *   on/off:      level 1 if weight > r
 *   continuous:  level = weight / (1 + r), held at 1
 *
 * If the weight is never below 150, the on/off rule always says 1 (150 > 99)
 * and the continuous rule always gives at least 150 / 100, held at 1. So
 * with the first tone's level at 1 as well, EVERY level is exactly 1.
 *
 *   scale 3000, offset 0.05:  weight is at least 3000 * 0.05 = 150
 *   scale 1,    offset 200:   weight is at least 1 * 200     = 200
 *
 * With the built-in 150 and 0.05 instead of either number, the weight starts
 * at 7.5 or below, and most levels come out below 1. */
static int check_level_fields(const Rules *v4, LevelRule level_rule, double scale, double offset,
                              const char *name)
{
    Rules variant = *v4;
    Score score;
    long tones = 0;
    int bad = 0;
    uint32_t seed;

    variant.name = name;
    variant.level_rule = level_rule;
    variant.level_scale = scale;
    variant.level_offset = offset;
    variant.first_level = 1.0;
    variant.lone_level = 1.0;

    score_init(&score);
    for (seed = 1; seed <= SEEDS; seed++) {
        long not_one = 0;
        int32_t i;

        if (compose(&variant, seed, 100, &score) != 0) { bad++; continue; }
        for (i = 0; i < score.n_tones; i++)
            if (score.tones[i].level != 1.0) not_one++;
        if (not_one != 0) {
            printf("FAIL rules %s, seed %lu: %ld of %d tones have a level other than 1\n",
                   variant.name, (unsigned long) seed, not_one, (int) score.n_tones);
            bad++;
        }
        tones += score.n_tones;
        score_free(&score);
    }
    printf("%s (level_scale %g, level_offset %g): %d seeds, %ld tones, every level is 1; "
           "%d failures\n", name, scale, offset, SEEDS, tones, bad);
    return bad;
}

/* lone_level, and level_scale once more. With level_scale 0 the weight is
 * always 0: the on/off rule never finds it above a draw, and the continuous
 * rule gives 0 / (1 + r) = 0. So a new tone is tracked (level 0) whenever
 * some tone is counted, and gets lone_level, here 0.5, when none is:
 *
 *   - every level is exactly 0 or 0.5;
 *   - the first tone has level 0.5 and is counted, so the ten tones that
 *     follow it at ticks 1..10 all have level 0;
 *   - the first tone's successor still finds it counted and gets level 0.
 *     After that nothing is counted, so the next tone gets 0.5. With 100
 *     cycles there is always a next tone: at least one later tone has 0.5.
 *
 * The built-in rule sets have lone_level 1 (on/off) and 0.7 (continuous). */
static int check_lone_level(const Rules *v4, LevelRule level_rule, const char *name)
{
    Rules variant = *v4;
    Score score;
    long tones = 0, lone = 0;
    int bad = 0;
    uint32_t seed;

    variant.name = name;
    variant.level_rule = level_rule;
    variant.level_scale = 0.0;
    variant.first_level = 0.5;
    variant.lone_level = 0.5;

    score_init(&score);
    for (seed = 1; seed <= SEEDS; seed++) {
        const char *problem = NULL;
        long lone_here = 0;
        int32_t i;

        if (compose(&variant, seed, 100, &score) != 0) { bad++; continue; }
        for (i = 0; i < score.n_tones && !problem; i++) {
            const Tone *tone = &score.tones[i];

            if (tone->level != 0.0 && tone->level != 0.5)
                problem = "a level is neither 0 nor 0.5";
            else if (tone->tick == 0 && tone->level != 0.5)
                problem = "the first tone's level is not 0.5";
            else if (tone->tick >= 1 && tone->tick <= 10 && tone->level != 0.0)
                problem = "a tone at ticks 1..10 is not tracked";
            else if (tone->tick > 10 && tone->level == 0.5)
                lone_here++;
        }
        if (!problem && lone_here == 0) problem = "no later tone has level 0.5";
        if (problem) {
            printf("FAIL rules %s, seed %lu: %s\n", variant.name, (unsigned long) seed, problem);
            bad++;
        }
        tones += score.n_tones;
        lone += lone_here;
        score_free(&score);
    }
    printf("%s (level_scale 0, lone_level 0.5): %d seeds, %ld tones, every level is 0 or 0.5 "
           "(%ld later tones at 0.5); %d failures\n", name, SEEDS, tones, lone, bad);
    return bad;
}

/* The ends of the frequency range. A new tone's frequency must lie in the
 * range "both ends included" (Appendix A of the plan). With a ratio table
 * that holds only 1/1, every tone has exactly the first tone's frequency.
 * So if the range is set to start and end at that very frequency, every new
 * tone lands on both ends at once:
 *
 *   - ends included: the tune is composed, and all its frequencies are equal;
 *   - ends left out: no ratio is ever accepted, and the composer gives up.
 *
 * The first tone's frequency is learnt by composing once with the v4 rules;
 * it is the first draw, so it is the same for the variant. */
static int check_range_ends(const Rules *v4)
{
    Score score;
    long tones = 0;
    int bad = 0;
    uint32_t seed;

    score_init(&score);
    for (seed = 1; seed <= SEEDS; seed++) {
        Rules variant = *v4;
        double first_freq;
        long others = 0;
        int32_t i;

        if (compose(v4, seed, 40, &score) != 0) { bad++; continue; }
        first_freq = score.tones[0].freq_hz;
        score_free(&score);

        variant.name = "test-range-ends";
        variant.n_ratios = 1;
        variant.ratios[0].num = 1;
        variant.ratios[0].den = 1;
        variant.freq_min = first_freq;
        variant.freq_max = first_freq;

        if (compose(&variant, seed, 40, &score) != 0) { bad++; continue; }
        for (i = 0; i < score.n_tones; i++)
            if (score.tones[i].freq_hz != first_freq) others++;
        if (others != 0 || score.n_tones < 11) {
            printf("FAIL rules %s, seed %lu: %d tones, %ld of them not at the first tone's "
                   "frequency\n", variant.name, (unsigned long) seed, (int) score.n_tones, others);
            bad++;
        }
        tones += score.n_tones;
        score_free(&score);
    }
    printf("frequency range of one single frequency: %d seeds, %ld tones, all composed at that "
           "frequency; %d failures\n", SEEDS, tones, bad);
    return bad;
}

/* The density is held at 1. density = min(1, gain * sin(...)) squared. With a
 * density gain of 1000 and 100 cycles, gain * sin(pi * c / 69) is far above 1
 * in every cycle c from 1 to 68 (its smallest value there is 1000 * sin(pi / 69)
 * = 45.5), so the density is exactly 1 in all of them. With the continuous
 * level rule, level_scale 0.5 and level_offset 0, the weight is then 0.5, and
 *
 *     level = 0.5 / (1 + r),   r = 0..99:   at most 0.5, at least 0.005
 *
 * or lone_level, set to 0.25 here, when nothing is counted. So every tone
 * created in cycles 1 to 68 has a level above 0 and at most 0.5.
 *
 * Without the hold, the density would be 45.5 squared or more, the weight at
 * least 1000, and every such level would come out as 1. */
static int check_density_hold(const Rules *v4)
{
    Rules variant = *v4;
    Score score;
    long tones = 0;
    int bad = 0;
    uint32_t seed;

    variant.name = "test-density-hold";
    variant.level_rule = LEVEL_CONTINUOUS;
    variant.density_gain = 1000.0;
    variant.level_scale = 0.5;
    variant.level_offset = 0.0;
    variant.lone_level = 0.25;

    score_init(&score);
    for (seed = 1; seed <= SEEDS; seed++) {
        long wrong = 0, checked = 0;
        int32_t i;

        if (compose(&variant, seed, 100, &score) != 0) { bad++; continue; }
        for (i = 0; i < score.n_tones; i++) {
            const Tone *tone = &score.tones[i];
            int cycle = tone->tick / score.slots;

            if (cycle < 1 || cycle > 68) continue;
            checked++;
            if (!(tone->level > 0.0 && tone->level <= 0.5)) wrong++;
        }
        if (wrong != 0 || checked == 0) {
            printf("FAIL rules %s, seed %lu: %ld of %ld tones created in cycles 1..68 have a "
                   "level outside 0..0.5\n", variant.name, (unsigned long) seed, wrong, checked);
            bad++;
        }
        tones += checked;
        score_free(&score);
    }
    printf("%s (density_gain 1000, level_scale 0.5): %d seeds, %ld tones in cycles 1..68, every "
           "level is above 0 and at most 0.5; %d failures\n", variant.name, SEEDS, tones, bad);
    return bad;
}

int main(void)
{
    const Rules *v4 = rules_find("v4");
    int bad = 0;

    /* Print line by line, so that the messages survive if a later step crashes. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (!v4) {
        printf("FAIL the rule set v4 is not built in\n");
        return 1;
    }
    bad += check_full_reach_one(v4);
    bad += check_full_reach_two(v4);
    bad += check_level_fields(v4, LEVEL_ON_OFF,     3000.0, 0.05,  "test-on-off-scale");
    bad += check_level_fields(v4, LEVEL_ON_OFF,     1.0,    200.0, "test-on-off-offset");
    bad += check_level_fields(v4, LEVEL_CONTINUOUS, 3000.0, 0.05,  "test-continuous-scale");
    bad += check_level_fields(v4, LEVEL_CONTINUOUS, 1.0,    200.0, "test-continuous-offset");
    bad += check_lone_level(v4, LEVEL_ON_OFF,     "test-on-off-lone");
    bad += check_lone_level(v4, LEVEL_CONTINUOUS, "test-continuous-lone");
    bad += check_range_ends(v4);
    bad += check_density_hold(v4);

    printf("%s\n", bad ? "rule fields: FAILED" : "rule fields: all as worked out by hand");
    return bad ? 1 : 0;
}
