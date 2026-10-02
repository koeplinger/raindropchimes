/* synth_patches.c -- checks the built-in patches against the numbers of the
 * plan (section 9), typed in here a second time, and the v6 envelope against
 * the formula of the 18 February 2011 program, written out a second time.
 *
 * A patch that has made a tune is never changed. This check is what notices
 * if one is.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "synth/render.h"
#include "synth/reverb.h"
#include "synth/shapes.h"
#include "synth/soften.h"
#include "synth_common.h"

#define TWO_PI  6.283185307179586476925
#define RATE    44100.0
#define SLOT    1701              /* frames per slot at the v4 tempo */
#define CYCLE   (11 * SLOT)

#define FREQ    1299.5451597662952
#define PAN     0.26
#define REPS    3
#define LEVEL   0.7

typedef struct { double samples; double gain; int swap; } PlanTap;

/* Plan section 9: delay in samples at 44.1 kHz, gain, swapped or not. */
static const PlanTap V4_TAPS[] = {
    { 7000, 0.35, 0 }, { 9500, 0.10, 1 }, { 12000, 0.20, 0 }, { 17000, 0.08, 1 }, { 19000, 0.12, 0 },
};
static const PlanTap V6_TAPS[] = {
    { 7000, 0.28, 0 }, { 9500, 0.09, 1 }, { 12000, 0.17, 0 }, { 17000, 0.07, 1 }, { 19000, 0.11, 0 },
    { 30000, 0.05, 1 }, { 35000, 0.07, 0 },
};

static void check_patch(const char *name, const char *envelope, double depth,
                        const PlanTap *taps, int n_taps)
{
    const Patch *patch = patch_find(name);
    const ReverbConfig *reverb;
    int i;

    if (!patch) fail("there is no patch \"%s\"", name);
    if (strcmp(patch->wave, "cosine") != 0) fail("patch %s: the wave form is not the cosine", name);
    if (patch->phase_restarts != 0) fail("patch %s: the phase should run on from tone to tone", name);
    if (strcmp(patch->loudness, "inverse_freq") != 0 || patch->loudness_constant != 600000.0 / 32768.0)
        fail("patch %s: the loudness law is not 600000 / frequency in 16-bit units", name);
    if (strcmp(patch->strike_gain, "linear") != 0) fail("patch %s: the strike-gain law is not linear", name);
    if (strcmp(patch->pan, "linear") != 0) fail("patch %s: the pan law is not linear", name);
    if (strcmp(patch->envelope, envelope) != 0)
        fail("patch %s: its envelope is \"%s\", not \"%s\"", name, patch->envelope, envelope);
    if (patch->vibrato_depth != depth)
        fail("patch %s: its vibrato depth is %g, not %g", name, patch->vibrato_depth, depth);
    if (fabs(patch->vibrato_period_seconds * 44100.0 - 18000.0) > 1e-9 || patch->vibrato_spread != 0.001)
        fail("patch %s: the vibrato period is not 18000 samples * (1 + jitter / 1000)", name);

    if (patch->n_effects != 1 || strcmp(patch->effects[0].effect, "reverb") != 0)
        fail("patch %s: its one effect should be the reverb", name);
    reverb = patch->effects[0].config;
    if (reverb->n_taps != n_taps) fail("patch %s: %d reverb taps, not %d", name, reverb->n_taps, n_taps);
    for (i = 0; i < n_taps; i++) {
        const ReverbTap *tap = &reverb->taps[i];
        if (fabs(tap->delay_seconds * 44100.0 - taps[i].samples) > 1e-9 || tap->gain != taps[i].gain
                || (tap->swap != 0) != taps[i].swap)
            fail("patch %s: reverb tap %d is not %g samples, gain %g, %s", name, i, taps[i].samples,
                 taps[i].gain, taps[i].swap ? "swapped" : "not swapped");
    }
    printf("patch %-3s  envelope %s, vibrato depth %g, %d reverb taps: as in the plan\n",
           name, envelope, depth, n_taps);
}

/* The envelope of the 18 February 2011 program (and of the code in legacy/),
 * written out a second time, with S = 11:
 *
 *   3 * bin < slot:       3 * bin / slot                      the attack
 *   3 * bin < 2 * slot:   1.5 * (slot - bin) / slot           the "ding"
 *   bin > 10 * slot:      0.5 * 0.549 * (11 slot - bin) / slot
 *   otherwise:            0.5 * fourth root of ((11 slot - bin) / (11 slot))
 *
 * with 0.549 standing for 11 to the power -1/4, as in the v4 envelope. */
static double source_envelope_v6(double u)
{
    if (3.0 * u < 1.0) return 3.0 * u;
    if (3.0 * u < 2.0) return 1.5 * (1.0 - u);
    if (u > 10.0) return 0.5 * pow(11.0, -0.25) * (11.0 - u);
    return 0.5 * pow((11.0 - u) / 11.0, 0.25);
}

/* Renders one tone of level 0.7 without reverb, and compares every sample
 * with the voice formula written out again: cosine, vibrato of the given
 * depth, loudness law, level, strike gain, the v6 envelope, pan. */
static void check_voice(const char *name, double depth)
{
    Patch dry = dry_patch(name);
    Timing timing = timing_from_bins(1700.0, 0.0);
    Score score;
    Capture capture;
    RenderStats stats;
    char err[256];
    double phase = 0.0, worst = 0.0;
    long v;

    one_tone_score(&score, 0, FREQ, PAN, REPS);
    score.tones[0].level = LEVEL;
    capture_init(&capture);
    if (render_score(&score, &timing, &dry, RATE, 1.0, &capture.sink, &stats, err, sizeof err) != 0)
        fail("render: %s", err);
    if (capture.frames < (long) REPS * CYCLE) fail("the render is too short");

    for (v = 0; v < (long) REPS * CYCLE; v++) {
        int    strike = (int) (v / CYCLE);
        double u = (double) (v % CYCLE) / SLOT;
        double value, diff;

        phase += TWO_PI * FREQ / 44100.0 * (1.0 + depth * sin(TWO_PI * (double) v / 18000.0));
        value = cos(phase) * (600000.0 / FREQ / 32768.0) * LEVEL * ((double) (REPS - strike) / REPS)
              * source_envelope_v6(u);

        diff = fabs(capture.samples[2 * v] - value * (1.0 - PAN));
        if (diff > worst) worst = diff;
        diff = fabs(capture.samples[2 * v + 1] - value * PAN);
        if (diff > worst) worst = diff;
    }
    printf("patch %-3s  one tone against the voice formula with the v6 envelope and vibrato depth %g: "
           "largest difference %.2e of full scale\n", name, depth, worst);
    if (worst > 1e-9) fail("patch %s does not follow the v6 voice formula", name);

    capture_free(&capture);
    score_free(&score);
}

int main(void)
{
    const Patch *v6 = patch_find("v6"), *v10 = patch_find("v10");

    check_patch("v4",  "v4", 0.0005, V4_TAPS, 5);
    check_patch("v6",  "v6", 0.0005, V6_TAPS, 7);
    check_patch("v10", "v6", 0.0010, V6_TAPS, 7);

    /* The shape of the program's v6 envelope at its corners, for 11 slots:
     * the attack ends at 1; the ding falls to one half; the decay starts a
     * little lower; the release ends at 0. */
    {
        EnvelopeFn v6_envelope = envelope_find("v6");
        if (!v6_envelope) fail("there is no envelope \"v6\"");
        if (v6_envelope(0.0, 11) != 0.0) fail("the v6 envelope should start at 0");
        if (fabs(v6_envelope(1.0 / 3.0, 11) - 1.0) > 1e-12) fail("the v6 attack should end at 1");
        if (fabs(v6_envelope(2.0 / 3.0 - 1e-12, 11) - 0.5) > 1e-9) fail("the v6 ding should fall to one half");
        if (fabs(v6_envelope(2.0 / 3.0 + 1e-12, 11) - 0.5 * pow(31.0 / 33.0, 0.25)) > 1e-9)
            fail("the v6 decay should start at one half of the fourth root of 31/33");
        if (fabs(v6_envelope(11.0 - 1e-9, 11)) > 1e-8) fail("the v6 release should end at 0");
        printf("envelope v6: 0, up to 1 at a third of a slot, down to one half at two thirds, 0 at the end\n");
    }

    check_voice("v6", 0.0005);
    check_voice("v10", 0.0010);

    /* v10 is the v6 sound with twice the vibrato, and nothing else. */
    if (!v6 || !v10 || v10->vibrato_depth != 2.0 * v6->vibrato_depth
            || v6->effects[0].config != v10->effects[0].config)
        fail("patch v10 should be patch v6 with twice the vibrato depth and the same reverb");
    printf("patch v10 is patch v6 with twice the vibrato depth\n");

    /* The worked example of docs/EXTENDING.md: the v4 sound with the bell
     * wave form, softened at 4000 Hz before the v4 reverb. */
    {
        const Patch *bell = patch_find("bell"), *v4 = patch_find("v4");
        const SoftenConfig *soften;

        if (!bell || !v4) fail("the patch \"bell\" or \"v4\" is missing");
        if (strcmp(bell->wave, "bell") != 0) fail("patch bell: its wave form is not \"bell\"");
        if (bell->phase_restarts != 0 || strcmp(bell->envelope, "v4") != 0
                || strcmp(bell->loudness, "inverse_freq") != 0
                || bell->loudness_constant != v4->loudness_constant
                || strcmp(bell->strike_gain, "linear") != 0 || strcmp(bell->pan, "linear") != 0
                || bell->vibrato_depth != v4->vibrato_depth
                || bell->vibrato_period_seconds != v4->vibrato_period_seconds
                || bell->vibrato_spread != v4->vibrato_spread)
            fail("patch bell: apart from its wave form and its effects it should be the v4 patch");
        if (bell->n_effects != 2 || strcmp(bell->effects[0].effect, "soften") != 0
                || strcmp(bell->effects[1].effect, "reverb") != 0
                || bell->effects[1].config != v4->effects[0].config)
            fail("patch bell: its effects should be soften, then the v4 reverb");
        soften = bell->effects[0].config;
        if (soften->cutoff_hz != 4000.0) fail("patch bell: soften's cut-off is %g Hz, not 4000", soften->cutoff_hz);
        printf("patch bell is the v4 sound with the bell wave form, softened at 4000 Hz before the v4 reverb\n");
    }

    printf("ok: patches\n");
    return 0;
}
