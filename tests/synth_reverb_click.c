/* synth_reverb_click.c -- the reverb's response to a single click: the right
 * delays, gains and channel swaps, for the first and second generation of
 * echoes. The expected taps are typed in here from the plan (section 9), so
 * this also checks the v4 patch's reverb settings.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "synth/patch.h"
#include "synth/reverb.h"
#include "synth_common.h"

#define N_TAPS 5

typedef struct {
    int    samples;    /* delay, in samples at 44.1 kHz */
    double gain;
    int    swap;
} PlanTap;

static const PlanTap PLAN[N_TAPS] = {
    { 7000, 0.35, 0 }, { 9500, 0.10, 1 }, { 12000, 0.20, 0 }, { 17000, 0.08, 1 }, { 19000, 0.12, 0 }
};

/* Sends one click (1.0 on the left, at frame 0) through the reverb at the
 * given sample rate and compares the answer with echoes worked out by hand:
 * the click itself, each tap once, and each pair of taps one after the
 * other. Third echoes start at three times the shortest delay; the
 * comparison stops just before that. */
static void check_click(const ReverbConfig *config, double sample_rate, int block)
{
    int     delay[N_TAPS];
    int     n_frames, i, j, done;
    double *expected, *got;
    void   *reverb;

    for (i = 0; i < N_TAPS; i++)
        delay[i] = (int) floor(PLAN[i].samples * sample_rate / 44100.0 + 0.5);
    n_frames = 3 * delay[0];

    expected = calloc((size_t) n_frames * 2, sizeof *expected);
    got = calloc((size_t) n_frames * 2, sizeof *got);
    if (!expected || !got) fail("out of memory");

    expected[0] = 1.0;                                 /* the click, left          */
    for (i = 0; i < N_TAPS; i++) {
        int at = delay[i];
        int channel = PLAN[i].swap;                    /* a swap moves it right    */
        if (at < n_frames) expected[2 * at + channel] += PLAN[i].gain;
        for (j = 0; j < N_TAPS; j++) {
            int at2 = at + delay[j];
            int channel2 = channel ^ PLAN[j].swap;     /* two swaps: left again    */
            if (at2 < n_frames) expected[2 * at2 + channel2] += PLAN[i].gain * PLAN[j].gain;
        }
    }

    reverb = REVERB_EFFECT.create(config, sample_rate);
    if (!reverb) fail("the reverb could not be created at %g Hz", sample_rate);
    got[0] = 1.0;
    for (done = 0; done < n_frames; done += block) {
        int n = n_frames - done < block ? n_frames - done : block;
        REVERB_EFFECT.process(reverb, got + 2 * done, n);
    }

    for (i = 0; i < 2 * n_frames; i++)
        if (fabs(got[i] - expected[i]) > 1e-12)
            fail("at %g Hz, frame %d %s: got %.6f, expected %.6f", sample_rate, i / 2,
                 i % 2 ? "right" : "left", got[i], expected[i]);

    if (fabs(REVERB_EFFECT.longest_delay_seconds(reverb) - delay[N_TAPS - 1] / sample_rate) > 1e-12)
        fail("at %g Hz the longest delay is reported as %.6f s", sample_rate,
             REVERB_EFFECT.longest_delay_seconds(reverb));
    REVERB_EFFECT.destroy(reverb);

    printf("%g Hz, blocks of %d: %d frames as expected; first echoes at", sample_rate, block, n_frames);
    for (i = 0; i < N_TAPS; i++) printf(" %d%s", delay[i], PLAN[i].swap ? "s" : "");
    printf("\n");
    free(expected);
    free(got);
}

/* A few values typed in by hand, for a click of 1.0 on the left at 44.1 kHz. */
static void check_by_hand(const ReverbConfig *config)
{
    static const struct { int frame; int channel; double value; const char *why; } SPOT[] = {
        {     0, 0, 1.0,    "the click itself" },
        {  7000, 0, 0.35,   "tap 1" },
        {  9500, 1, 0.10,   "tap 2, swapped to the right" },
        { 12000, 0, 0.20,   "tap 3" },
        { 14000, 0, 0.1225, "tap 1 twice" },
        { 16500, 1, 0.07,   "taps 1 and 2, either way round" },
        { 17000, 1, 0.08,   "tap 4, swapped to the right" },
        { 19000, 0, 0.27,   "tap 5, taps 1 and 3 either way round, and tap 2 twice (back on the left)" },
        { 19000, 1, 0.0,    "nothing" },
        {  9500, 0, 0.0,    "nothing" },
    };
    enum { FRAMES = 20000 };
    double *frames = calloc(2 * FRAMES, sizeof *frames);
    void   *reverb = REVERB_EFFECT.create(config, 44100.0);
    size_t  i;

    if (!frames || !reverb) fail("the reverb could not be created");
    frames[0] = 1.0;
    REVERB_EFFECT.process(reverb, frames, FRAMES);
    for (i = 0; i < sizeof SPOT / sizeof SPOT[0]; i++) {
        double got = frames[2 * SPOT[i].frame + SPOT[i].channel];
        printf("frame %5d %-5s %.4f   %s\n", SPOT[i].frame, SPOT[i].channel ? "right" : "left",
               got, SPOT[i].why);
        if (fabs(got - SPOT[i].value) > 1e-12)
            fail("frame %d: expected %.4f", SPOT[i].frame, SPOT[i].value);
    }
    REVERB_EFFECT.destroy(reverb);
    free(frames);
}

int main(void)
{
    const Patch *patch = patch_find("v4");
    const ReverbConfig *config;
    ReverbConfig bad;

    if (!patch) fail("there is no patch \"v4\"");
    if (patch->n_effects != 1 || strcmp(patch->effects[0].effect, "reverb") != 0)
        fail("the v4 patch should have exactly one effect, the reverb");
    if (effect_find("reverb") != &REVERB_EFFECT) fail("effect_find(\"reverb\") does not find the reverb");
    if (effect_find("no-such-effect") != NULL) fail("effect_find finds an effect that does not exist");
    config = patch->effects[0].config;

    check_by_hand(config);

    check_click(config, 44100.0, 21000);
    check_click(config, 44100.0, 1024);    /* the block size must not matter */
    check_click(config, 44100.0, 777);
    check_click(config, 48000.0, 1024);    /* delays round to whole frames   */

    /* Settings that cannot work are refused. */
    bad = *config;
    bad.taps[0].delay_seconds = 0.0;
    if (REVERB_EFFECT.create(&bad, 44100.0) != NULL) fail("a tap with no delay was accepted");
    bad = *config;
    bad.n_taps = REVERB_MAX_TAPS + 1;
    if (REVERB_EFFECT.create(&bad, 44100.0) != NULL) fail("too many taps were accepted");

    printf("ok: reverb click response\n");
    return 0;
}
