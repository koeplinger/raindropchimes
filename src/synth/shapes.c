/* shapes.c -- wave forms, envelopes, loudness / strike-gain / pan laws, by name. */
#include "synth/shapes.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/* ---------------------------------------------------------------- wave forms */

static double wave_cosine(double phase)
{
    return cos(phase);
}

/* ----------------------------------------------------------------- envelopes */

/* The 2011 envelope: a quick attack over half a slot, a slow fourth-root
 * decay over the cycle, and a linear release in the last slot. The attack
 * ends at 1.0 and the decay starts a little lower (0.988 for 11 slots); that
 * small step is in the original and is kept. */
static double envelope_v4(double u, int slots)
{
    double s = (double) slots;

    if (u < 0.5) return 2.0 * u;
    if (u > s - 1.0) return sqrt(sqrt(1.0 / s)) * (s - u);
    return sqrt(sqrt((s - u) / s));
}

/* ------------------------------------------------------------- loudness laws */

/* Loudness proportional to 1 / frequency. */
static double loudness_inverse_freq(double freq_hz, double constant)
{
    return constant / freq_hz;
}

/* ---------------------------------------------------------- strike-gain laws */

/* Each strike a step quieter: N/N, (N-1)/N, ... down to 0 on the last one. */
static double strike_gain_linear(int strike, int reps)
{
    return (double) (reps - strike) / (double) reps;
}

/* ------------------------------------------------------------------ pan laws */

static void pan_linear(double pan, double *left, double *right)
{
    *left = 1.0 - pan;
    *right = pan;
}

/* ----------------------------------------------------------------- the lists */

static const struct { const char *name; WaveFn shape; } WAVES[] = {
    { "cosine", wave_cosine },
};

static const struct { const char *name; EnvelopeFn shape; } ENVELOPES[] = {
    { "v4", envelope_v4 },
};

static const struct { const char *name; LoudnessFn shape; } LOUDNESS_LAWS[] = {
    { "inverse_freq", loudness_inverse_freq },
};

static const struct { const char *name; StrikeGainFn shape; } STRIKE_GAIN_LAWS[] = {
    { "linear", strike_gain_linear },
};

static const struct { const char *name; PanFn shape; } PAN_LAWS[] = {
    { "linear", pan_linear },
};

#define COUNT(list) (sizeof (list) / sizeof (list)[0])

WaveFn wave_find(const char *name)
{
    size_t i;
    for (i = 0; i < COUNT(WAVES); i++)
        if (strcmp(WAVES[i].name, name) == 0) return WAVES[i].shape;
    return NULL;
}

EnvelopeFn envelope_find(const char *name)
{
    size_t i;
    for (i = 0; i < COUNT(ENVELOPES); i++)
        if (strcmp(ENVELOPES[i].name, name) == 0) return ENVELOPES[i].shape;
    return NULL;
}

LoudnessFn loudness_find(const char *name)
{
    size_t i;
    for (i = 0; i < COUNT(LOUDNESS_LAWS); i++)
        if (strcmp(LOUDNESS_LAWS[i].name, name) == 0) return LOUDNESS_LAWS[i].shape;
    return NULL;
}

StrikeGainFn strike_gain_find(const char *name)
{
    size_t i;
    for (i = 0; i < COUNT(STRIKE_GAIN_LAWS); i++)
        if (strcmp(STRIKE_GAIN_LAWS[i].name, name) == 0) return STRIKE_GAIN_LAWS[i].shape;
    return NULL;
}

PanFn pan_find(const char *name)
{
    size_t i;
    for (i = 0; i < COUNT(PAN_LAWS); i++)
        if (strcmp(PAN_LAWS[i].name, name) == 0) return PAN_LAWS[i].shape;
    return NULL;
}
