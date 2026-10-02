/* soften.c -- the effect "soften": a one-pole low-pass filter.
 *
 *   out = previous out + share * (in - previous out)
 *
 * Each output sample moves a share of the way from the previous output to
 * the new input: share = 1 - exp(-2 pi * cutoff / sample rate). A slow wave
 * is followed closely; a fast one is smoothed away.
 */
#include "synth/soften.h"

#include <math.h>
#include <stdlib.h>

#define TWO_PI 6.283185307179586476925

typedef struct {
    double share;          /* how far each sample moves towards the input */
    double left, right;    /* the previous output frame                   */
} Soften;

static void *soften_create(const void *config, double sample_rate)
{
    const SoftenConfig *settings = config;
    Soften *soften;

    if (!settings || !(settings->cutoff_hz > 0.0) || !isfinite(settings->cutoff_hz)) return NULL;
    if (!(sample_rate > 0.0) || !isfinite(sample_rate)) return NULL;

    soften = calloc(1, sizeof *soften);
    if (!soften) return NULL;
    soften->share = 1.0 - exp(-TWO_PI * settings->cutoff_hz / sample_rate);
    return soften;
}

static void soften_process(void *state, double *frames, int n_frames)
{
    Soften *soften = state;
    int i;

    for (i = 0; i < n_frames; i++) {
        soften->left += soften->share * (frames[2 * i] - soften->left);
        soften->right += soften->share * (frames[2 * i + 1] - soften->right);
        frames[2 * i] = soften->left;
        frames[2 * i + 1] = soften->right;
    }
}

/* It remembers only the last frame, not a stretch of the past: there is
 * nothing to wait for at the end of a piece. */
static double soften_longest_delay_seconds(const void *state)
{
    (void) state;
    return 0.0;
}

static void soften_destroy(void *state)
{
    free(state);
}

const Effect SOFTEN_EFFECT = {
    "soften",
    soften_create,
    soften_process,
    soften_longest_delay_seconds,
    soften_destroy
};
