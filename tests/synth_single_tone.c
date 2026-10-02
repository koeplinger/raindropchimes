/* synth_single_tone.c -- checks the voice on scores of one or two tones,
 * rendered without reverb:
 *
 *   1. the favourite's first tone measures 1299.5 Hz at pan 0.26, as loud as
 *      the loudness law says, each strike a step quieter;
 *   2. the whole tone equals Appendix B of the plan, written out here a
 *      second time, sample by sample;
 *   3. the phase runs on from tone to tone, through the zero-gain strike,
 *      and holds while the slot holds a tracked tone or no tone; a patch
 *      with phase_restarts set starts every tone at phase zero;
 *   4. a tone at or above half the sample rate is not sounded.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "synth/render.h"
#include "synth_common.h"

#define TWO_PI      6.283185307179586476925
#define RATE        44100.0
#define SLOT        1701           /* frames per slot at the v4 tempo */
#define CYCLE       (11 * SLOT)

#define FREQ        1299.5451597662952
#define PAN         0.26
#define REPS        13

static void render_dry(const Score *score, const Patch *patch, double rate, Capture *capture,
                       RenderStats *stats)
{
    Timing timing = timing_from_bins(1700.0, 0.0);
    char err[256];

    capture_init(capture);
    if (render_score(score, &timing, patch, rate, 1.0, &capture->sink, stats, err, sizeof err) != 0)
        fail("render: %s", err);
}

/* The frame at which a cycle starts, at the v4 tempo. */
static long cycle_start(int cycle, double rate)
{
    Timing timing = timing_from_bins(1700.0, 0.0);
    return (long) timing_tick_frame(&timing, 11, (int64_t) cycle * 11, rate);
}

/* The frequency of one channel between two frames, from the times at which
 * the wave crosses zero going up. */
static double measure_frequency(const Capture *capture, long from, long to, double rate)
{
    double first = 0.0, last = 0.0;
    long   crossings = 0, i;

    for (i = from + 1; i < to; i++) {
        double before = capture->samples[2 * (i - 1)];
        double after = capture->samples[2 * i];
        if (before < 0.0 && after >= 0.0) {
            double when = (double) (i - 1) + before / (before - after);   /* between the two */
            if (crossings == 0) first = when;
            last = when;
            crossings++;
        }
    }
    if (crossings < 2) fail("the tone does not swing");
    return (double) (crossings - 1) * rate / (last - first);
}

/* Appendix B, written out a second time: the envelope with S = 11. */
static double plan_envelope(double u)
{
    if (u < 0.5) return 2.0 * u;
    if (u > 10.0) return pow(11.0, -0.25) * (11.0 - u);
    return pow((11.0 - u) / 11.0, 0.25);
}

/* ---------------------------------------------------- 1. measurements */

static void check_measurements(const Patch *dry, double rate)
{
    Score score;
    Capture capture;
    RenderStats stats;
    double loudness = 600000.0 / FREQ / 32768.0;     /* of full scale */
    double freq, left = 0.0, right = 0.0, peak = 0.0, first_energy = 0.0;
    long   i;
    int    strike;

    one_tone_score(&score, 0, FREQ, PAN, REPS);
    render_dry(&score, dry, rate, &capture, &stats);
    printf("at %g Hz:\n", rate);

    /* Strikes 0..12 sound; strike 13 has gain 0. Without effects the
     * ring-out is the single quiet frame that ends the piece. */
    if (stats.music_frames != cycle_start(REPS, rate) || stats.frames != stats.music_frames + 1)
        fail("%ld frames, of which %ld music; expected %ld + 1",
             (long) stats.frames, (long) stats.music_frames, cycle_start(REPS, rate));

    /* Frequency, over one whole swing of the vibrato inside strike 0. */
    freq = measure_frequency(&capture, 500, 500 + (long) (18000.0 * rate / RATE), rate);
    printf("  frequency %.4f Hz   (expected %.4f)\n", freq, FREQ);
    if (fabs(freq - FREQ) > 0.02) fail("the frequency is off");

    /* Pan: the share of the sound that is on the right. */
    for (i = 0; i < capture.frames; i++) {
        left += fabs(capture.samples[2 * i]);
        right += fabs(capture.samples[2 * i + 1]);
    }
    printf("  pan       %.6f      (expected %.2f)\n", right / (left + right), PAN);
    if (fabs(right / (left + right) - PAN) > 1e-9) fail("the pan is off");

    /* Loudness: the envelope's maximum is 1, reached for an instant at the
     * end of the attack; the wave need not be at its crest just then. */
    for (i = 0; i < cycle_start(1, rate); i++) {
        double size = fabs(capture.samples[2 * i]) + fabs(capture.samples[2 * i + 1]);
        if (size > peak) peak = size;
    }
    printf("  peak      %.6f      (loudness law: %.6f)\n", peak, loudness);
    if (peak > loudness * 1.0000001 || peak < loudness * 0.985) fail("the loudness is off");

    /* Strike gain: strike j is (13 - j) / 13 as loud as strike 0. */
    for (strike = 0; strike < REPS; strike++) {
        double energy = 0.0, ratio;
        for (i = cycle_start(strike, rate); i < cycle_start(strike + 1, rate); i++)
            energy += capture.samples[2 * i] * capture.samples[2 * i];
        if (strike == 0) first_energy = energy;
        ratio = sqrt(energy / first_energy);
        if (fabs(ratio - (double) (REPS - strike) / REPS) > 0.003)
            fail("strike %d is %.4f as loud as strike 0; expected %.4f",
                 strike, ratio, (double) (REPS - strike) / REPS);
    }
    printf("  strikes   each a thirteenth quieter than the one before\n");

    capture_free(&capture);
    score_free(&score);
}

/* -------------------------------------------- 2. Appendix B, a second time */

static void check_against_plan(const Patch *dry)
{
    Score score;
    Capture capture;
    RenderStats stats;
    double phase = 0.0, worst = 0.0;
    long   v;

    one_tone_score(&score, 0, FREQ, PAN, REPS);
    render_dry(&score, dry, RATE, &capture, &stats);

    for (v = 0; v < (long) REPS * CYCLE; v++) {
        int    strike = (int) (v / CYCLE);
        double u = (double) (v % CYCLE) / SLOT;
        double value, diff;

        phase += TWO_PI * FREQ / 44100.0 * (1.0 + 0.0005 * sin(TWO_PI * (double) v / 18000.0));
        value = cos(phase) * (600000.0 / FREQ / 32768.0) * ((double) (REPS - strike) / REPS)
              * plan_envelope(u);

        diff = fabs(capture.samples[2 * v] - value * (1.0 - PAN));
        if (diff > worst) worst = diff;
        diff = fabs(capture.samples[2 * v + 1] - value * PAN);
        if (diff > worst) worst = diff;
    }
    printf("against Appendix B written out again: largest difference %.2e of full scale\n", worst);
    if (worst > 1e-9) fail("the voice does not follow Appendix B");

    capture_free(&capture);
    score_free(&score);
}

/* ------------------------------------------------- 3. the phase between tones */

/* Adds a second tone to a one-tone score, in the same slot. */
static void add_tone(Score *score, int32_t tick, int32_t parent, double level)
{
    Tone tone;

    tone.tick = tick;
    tone.parent = parent;
    tone.num = tone.den = 1;
    tone.freq_hz = FREQ;
    tone.pan = PAN;
    tone.reps = 1;
    tone.level = level;
    tone.jitter = 0;
    if (score_add(score, &tone) != 0) fail("out of memory");
}

/* Renders the score and returns the largest difference between the first
 * strike of the tone created at `tick` and the given reference samples. */
static double difference(const Score *score, const Patch *patch, int32_t tick, const double *reference)
{
    Capture capture;
    RenderStats stats;
    double worst = 0.0;
    long   start = (long) tick * SLOT, i;

    render_dry(score, patch, RATE, &capture, &stats);
    if (capture.frames < start + CYCLE) fail("the render is too short");
    for (i = 0; i < 2L * CYCLE; i++) {
        double diff = fabs(capture.samples[2 * start + i] - reference[i]);
        if (diff > worst) worst = diff;
    }
    capture_free(&capture);
    return worst;
}

static void check_phase(const Patch *dry)
{
    double *run_on = malloc(2 * CYCLE * sizeof *run_on);
    double *from_zero = malloc(2 * CYCLE * sizeof *from_zero);
    Patch  restarting = *dry;
    Score  score;
    double phase = 0.0, gain = 600000.0 / FREQ / 32768.0, diff;
    long   v;

    if (!run_on || !from_zero) fail("out of memory");
    restarting.phase_restarts = 1;

    /* What the second tone's first strike must look like. The first tone
     * (repetition count 1) has two strikes, the second with gain 0; the phase
     * moves through both. Then the second tone begins, and its vibrato
     * counter starts again at 0. */
    for (v = 0; v < 2L * CYCLE; v++)
        phase += TWO_PI * FREQ / 44100.0 * (1.0 + 0.0005 * sin(TWO_PI * (double) v / 18000.0));
    for (v = 0; v < CYCLE; v++) {
        double value;
        phase += TWO_PI * FREQ / 44100.0 * (1.0 + 0.0005 * sin(TWO_PI * (double) v / 18000.0));
        value = cos(phase) * gain * plan_envelope((double) v / SLOT);
        run_on[2 * v] = value * (1.0 - PAN);
        run_on[2 * v + 1] = value * PAN;
    }
    /* The same strike started at phase zero. */
    phase = 0.0;
    for (v = 0; v < CYCLE; v++) {
        double value;
        phase += TWO_PI * FREQ / 44100.0 * (1.0 + 0.0005 * sin(TWO_PI * (double) v / 18000.0));
        value = cos(phase) * gain * plan_envelope((double) v / SLOT);
        from_zero[2 * v] = value * (1.0 - PAN);
        from_zero[2 * v + 1] = value * PAN;
    }

    /* The second tone follows at once: the phase ran through the zero-gain strike. */
    one_tone_score(&score, 0, FREQ, PAN, 1);
    add_tone(&score, 22, 0, 1.0);
    diff = difference(&score, dry, 22, run_on);
    printf("phase runs on through the zero-gain strike:   difference %.2e\n", diff);
    if (diff > 1e-9) fail("the phase did not run on through the zero-gain strike");
    diff = difference(&score, dry, 22, from_zero);
    if (diff < 1e-4) fail("this check cannot tell a running phase from a restarted one");
    diff = difference(&score, &restarting, 22, from_zero);
    printf("phase_restarts starts the tone at phase zero: difference %.2e\n", diff);
    if (diff > 1e-9) fail("phase_restarts did not start the tone at phase zero");
    score_free(&score);

    /* The slot is empty for two cycles in between: the phase holds. */
    one_tone_score(&score, 0, FREQ, PAN, 1);
    add_tone(&score, 44, 0, 1.0);
    diff = difference(&score, dry, 44, run_on);
    printf("phase holds while the slot is empty:          difference %.2e\n", diff);
    if (diff > 1e-9) fail("the phase moved while the slot was empty");
    score_free(&score);

    /* A tracked tone holds the slot for two cycles in between: the phase holds. */
    one_tone_score(&score, 0, FREQ, PAN, 1);
    add_tone(&score, 22, 0, 0.0);
    add_tone(&score, 44, 22, 1.0);
    diff = difference(&score, dry, 44, run_on);
    printf("phase holds under a tracked tone:             difference %.2e\n", diff);
    if (diff > 1e-9) fail("the phase moved under a tracked tone");
    score_free(&score);

    free(run_on);
    free(from_zero);
}

/* --------------------------------------- 4. tones the sample rate cannot carry */

/* The loudest sample of a one-tone score at the given sample rate. */
static double loudest(const Patch *dry, double freq, double rate)
{
    Score score;
    Capture capture;
    RenderStats stats;
    double peak = 0.0;
    long i;

    one_tone_score(&score, 0, freq, PAN, 1);
    render_dry(&score, dry, rate, &capture, &stats);
    for (i = 0; i < 2 * capture.frames; i++)
        if (fabs(capture.samples[i]) > peak) peak = fabs(capture.samples[i]);
    capture_free(&capture);
    score_free(&score);
    return peak;
}

/* A tone at or above half the sample rate would come out as another, wrong
 * frequency (5000 Hz at a sample rate of 8000 Hz would sound as 3000 Hz).
 * It is left out instead. Just below half the sample rate it still sounds. */
static void check_too_high(const Patch *dry)
{
    if (loudest(dry, 5000.0, 8000.0) != 0.0) fail("a tone of 5000 Hz was sounded at a sample rate of 8000 Hz");
    if (loudest(dry, 4000.0, 8000.0) != 0.0) fail("a tone at exactly half the sample rate was sounded");
    if (loudest(dry, 3990.0, 8000.0) == 0.0) fail("a tone of 3990 Hz was left out at a sample rate of 8000 Hz");
    if (loudest(dry, 5000.0, 44100.0) == 0.0) fail("a tone of 5000 Hz was left out at 44100 Hz");
    if (loudest(dry, 16000.0, 44100.0) == 0.0) fail("the highest tone of the v4 rules was left out at 44100 Hz");
    printf("a tone at or above half the sample rate is left out; one just below it sounds\n");
}

int main(void)
{
    Patch dry = dry_patch("v4");

    check_measurements(&dry, 44100.0);
    check_measurements(&dry, 48000.0);
    check_against_plan(&dry);
    check_phase(&dry);
    check_too_high(&dry);

    printf("ok: single tone\n");
    return 0;
}
