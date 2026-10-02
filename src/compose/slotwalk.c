/* slotwalk.c -- the slot algorithm of 14 February 2011: walk through the
 * ticks, and whenever a slot is free, draw a new tone from the tone in the
 * slot before it (docs/REVAMP_PLAN.md, Appendix A).
 *
 * The arithmetic and the order of the random draws follow the 2011 program
 * exactly: the same rules, seed and cycles must give the same tones, digit
 * for digit. Please do not "simplify" a formula here.
 */
#include "compose/compose.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define PI 3.141592653589793116    /* as written in the 2011 program */

#define MAX_RATIO_TRIES  10000       /* then the ratio redraw gives up */
#define MAX_CYCLES       100000000   /* the longest piece; the score reader accepts no more */
#define MAX_TICKS        2000000000  /* tick numbers must fit a 32-bit whole number */
#define LEVEL_DRAW_RANGE 100         /* the level draw is a number 0..99 */

/* U(k) of Appendix A: the next number from the generator, modulo k. */
static int draw(Rng *rng, int k)
{
    return (int) (rng_next(rng) % k);
}

/* The root tone: the only one with an absolute frequency (about 50.7 to
 * 14,720 Hz). This law is part of the algorithm, not of the rules. */
static void draw_root(const Rules *rules, Rng *rng, Tone *tone)
{
    tone->tick    = 0;
    tone->parent  = -1;
    tone->num     = 0;
    tone->den     = 0;
    tone->freq_hz = exp((double) draw(rng, 10000) / 1000.0) / 1.5 + 50.0;
    tone->pan     = (double) draw(rng, 101) / 100.0;
    tone->reps    = rules->reps[draw(rng, rules->first_reach)];
    tone->level   = rules->first_level;
    tone->jitter  = -1;
}

/* d of Appendix A: how many slots hold a counted tone right now. This
 * includes the tone about to be replaced and tones on their zero-gain
 * strike. A slot that has never held a tone counts 0. */
static int count_counted(const Rules *rules, const Score *score, const int32_t *holder)
{
    int d = 0, slot;
    for (slot = 0; slot < rules->slots; slot++)
        if (holder[slot] >= 0 && score->tones[holder[slot]].level > rules->counted_above) d++;
    return d;
}

/* The density curve over the piece: sparse, dense, sparse.
 * The order matters to the last digit of a level: multiply PI by the cycle
 * first, then divide; then the gain; then hold at 1; then square. */
static double density_at(const Rules *rules, int cycle, int last_cycle)
{
    double density = rules->density_gain * sin((PI * (double) cycle) / (double) last_cycle);
    if (density > 1.0) density = 1.0;
    return density * density;
}

/* Draws a ratio again and again until parent * ratio lies in the allowed
 * range (both ends included). Returns 0, or -1 if it gives up. */
static int draw_ratio(const Rules *rules, Rng *rng, double parent_freq, Tone *tone)
{
    int tries;
    for (tries = 0; tries < MAX_RATIO_TRIES; tries++) {
        int k = draw(rng, rules->n_ratios);
        int num = rules->ratios[k].num;
        int den = rules->ratios[k].den;
        /* The division comes first, as in the original table of ratios. */
        double freq = parent_freq * ((double) num / (double) den);
        if (freq >= rules->freq_min && freq <= rules->freq_max) {
            tone->num = num;
            tone->den = den;
            tone->freq_hz = freq;
            return 0;
        }
    }
    return -1;
}

/* The more tones are counted, the further into the repetition table the
 * draw may reach. */
static int draw_reps(const Rules *rules, Rng *rng, int d)
{
    int reach = (d < rules->reach_threshold) ? d + 2 : rules->full_reach;
    return rules->reps[draw(rng, reach)];
}

/* The two level rules of section 9 of the plan. They use the generator
 * differently: on/off always draws; continuous does not draw when nothing
 * is counted. */
static double draw_level(const Rules *rules, Rng *rng, int d, double density)
{
    double weight = rules->level_scale * (density + rules->level_offset);
    double level;
    int r;

    if (rules->level_rule == LEVEL_ON_OFF) {
        r = draw(rng, LEVEL_DRAW_RANGE);
        if (weight > (double) r) return 1.0;
        if (d == 0) return rules->lone_level;
        return 0.0;
    }

    /* LEVEL_CONTINUOUS */
    if (d == 0) return rules->lone_level;
    r = draw(rng, LEVEL_DRAW_RANGE);
    level = weight / (1.0 + (double) r);
    if (level > 1.0) level = 1.0;
    return level;
}

int slotwalk_compose(const Rules *rules, Rng *rng, int cycles, Score *score,
                     char *err, size_t errlen)
{
    int32_t *holder;     /* per slot: which tone it holds, as an index into
                            score->tones; -1 while it has never held one */
    int slots, most_cycles, last_cycle, slot;
    int32_t tick;
    Tone tone;
    int status = -1;

    if (rules_check(rules, err, errlen) != 0) return -1;
    slots = rules->slots;

    if (cycles < COMPOSE_MIN_CYCLES_MARGIN
            || cycles - COMPOSE_MIN_CYCLES_MARGIN < rules->end_cycles) {
        snprintf(err, errlen, "cycles is %d; with the rule set %s it must be at least %d "
                 "(the final %d cycles are the end phase)", cycles, rules->name,
                 rules->end_cycles + COMPOSE_MIN_CYCLES_MARGIN, rules->end_cycles);
        return -1;
    }
    /* The longest piece is MAX_CYCLES cycles. (Only a rule set with more
     * than 20 slots gets less: its tick numbers would outgrow a 32-bit
     * whole number sooner.) */
    most_cycles = MAX_CYCLES;
    if (most_cycles > MAX_TICKS / slots) most_cycles = MAX_TICKS / slots;
    if (cycles > most_cycles) {
        snprintf(err, errlen, "cycles is %d; with the rule set %s it must be at most %d",
                 cycles, rules->name, most_cycles);
        return -1;
    }
    if (score->n_tones != 0) {
        snprintf(err, errlen, "the score to fill is not empty");
        return -1;
    }

    holder = malloc((size_t) slots * sizeof *holder);
    if (!holder) {
        snprintf(err, errlen, "out of memory");
        return -1;
    }
    for (slot = 0; slot < slots; slot++) holder[slot] = -1;

    snprintf(score->rules, sizeof score->rules, "%s", rules->name);
    score->cycles = cycles;
    score->slots = slots;

    /* Tick 0, slot 0: the root tone. */
    draw_root(rules, rng, &tone);
    if (score_add(score, &tone) != 0) {
        snprintf(err, errlen, "out of memory");
        goto done;
    }
    holder[0] = 0;

    /* Tones are created up to and including this cycle (v4: T - 31). The
     * cycles after it are the end phase. */
    last_cycle = cycles - rules->end_cycles - 1;

    for (tick = 1; tick / slots <= last_cycle; tick++) {
        int cycle = tick / slots;
        int parent_slot;
        const Tone *parent;
        int d;
        double density;

        slot = tick % slots;

        /* Does the tone in this slot still have strikes left? */
        if (holder[slot] >= 0 && tick < score_tone_end(score, &score->tones[holder[slot]]))
            continue;

        /* The slot is free: a new tone, derived from the tone now in the
         * previous slot. */
        d = count_counted(rules, score, holder);
        density = density_at(rules, cycle, last_cycle);

        parent_slot = (slot + slots - 1) % slots;
        if (holder[parent_slot] < 0) {
            snprintf(err, errlen, "tick %d: the previous slot has never held a tone", (int) tick);
            goto done;
        }
        parent = &score->tones[holder[parent_slot]];

        /* The five draws, in the order of the 2011 program. */
        tone.tick = tick;
        tone.parent = parent->tick;
        if (draw_ratio(rules, rng, parent->freq_hz, &tone) != 0) {
            snprintf(err, errlen, "tick %d: gave up after %d tries to find a ratio that takes "
                     "%.17g Hz into the range %g .. %g Hz of the rule set %s", (int) tick,
                     MAX_RATIO_TRIES, parent->freq_hz, rules->freq_min, rules->freq_max,
                     rules->name);
            goto done;
        }
        tone.pan    = (double) draw(rng, 101) / 100.0;
        tone.reps   = draw_reps(rules, rng, d);
        tone.level  = draw_level(rules, rng, d, density);
        tone.jitter = draw(rng, rules->jitter_range);

        if (score_add(score, &tone) != 0) {
            snprintf(err, errlen, "out of memory");
            goto done;
        }
        holder[slot] = score->n_tones - 1;
    }
    status = 0;

done:
    if (status != 0) {
        /* Never hand back half a tune: no tones, and no settings either. */
        score->n_tones = 0;
        score->rules[0] = '\0';
        score->cycles = 0;
        score->slots = 0;
    }
    free(holder);
    return status;
}
