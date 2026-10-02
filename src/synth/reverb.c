/* reverb.c -- the reverb: delayed copies of the output fed back into it. */
#include "synth/reverb.h"

#include <math.h>
#include <stdlib.h>

typedef struct {
    int     n_taps;
    int     delay[REVERB_MAX_TAPS];   /* in frames */
    double  gain[REVERB_MAX_TAPS];
    int     swap[REVERB_MAX_TAPS];
    int     longest;                  /* the longest delay, in frames                  */
    double  sample_rate;
    double *past;                     /* the last `longest` output frames, as a ring   */
    int     next;                     /* where in the ring the next output frame goes  */
} Reverb;

static void reverb_destroy(void *state)
{
    Reverb *reverb = state;
    if (!reverb) return;
    free(reverb->past);
    free(reverb);
}

static void *reverb_create(const void *config, double sample_rate)
{
    const ReverbConfig *settings = config;
    Reverb *reverb;
    int i;

    if (!settings || settings->n_taps < 0 || settings->n_taps > REVERB_MAX_TAPS) return NULL;
    if (!(sample_rate > 0.0) || !isfinite(sample_rate)) return NULL;

    reverb = calloc(1, sizeof *reverb);
    if (!reverb) return NULL;
    reverb->n_taps = settings->n_taps;
    reverb->sample_rate = sample_rate;

    for (i = 0; i < settings->n_taps; i++) {
        const ReverbTap *tap = &settings->taps[i];
        double frames = round(tap->delay_seconds * sample_rate);

        /* A tap must look back at least one frame, or the output would
         * depend on itself. */
        if (!(frames >= 1.0 && frames <= 1.0e9) || !isfinite(tap->gain)) {
            reverb_destroy(reverb);
            return NULL;
        }
        reverb->delay[i] = (int) frames;
        reverb->gain[i] = tap->gain;
        reverb->swap[i] = tap->swap ? 1 : 0;
        if (reverb->delay[i] > reverb->longest) reverb->longest = reverb->delay[i];
    }

    if (reverb->longest > 0) {
        reverb->past = calloc((size_t) reverb->longest * 2, sizeof *reverb->past);
        if (!reverb->past) {
            reverb_destroy(reverb);
            return NULL;
        }
    }
    return reverb;
}

static void reverb_process(void *state, double *frames, int n_frames)
{
    Reverb *reverb = state;
    int i, t;

    if (reverb->longest == 0) return;   /* no taps: nothing to add */

    for (i = 0; i < n_frames; i++) {
        double left = frames[2 * i];
        double right = frames[2 * i + 1];

        for (t = 0; t < reverb->n_taps; t++) {
            /* The output frame from delay[t] frames ago. */
            int at = reverb->next - reverb->delay[t];
            const double *old;
            if (at < 0) at += reverb->longest;
            old = &reverb->past[2 * at];

            if (reverb->swap[t]) {
                left += reverb->gain[t] * old[1];
                right += reverb->gain[t] * old[0];
            } else {
                left += reverb->gain[t] * old[0];
                right += reverb->gain[t] * old[1];
            }
        }

        reverb->past[2 * reverb->next] = left;
        reverb->past[2 * reverb->next + 1] = right;
        reverb->next = (reverb->next + 1) % reverb->longest;

        frames[2 * i] = left;
        frames[2 * i + 1] = right;
    }
}

static double reverb_longest_delay_seconds(const void *state)
{
    const Reverb *reverb = state;
    return (double) reverb->longest / reverb->sample_rate;
}

const Effect REVERB_EFFECT = {
    "reverb",
    reverb_create,
    reverb_process,
    reverb_longest_delay_seconds,
    reverb_destroy
};
