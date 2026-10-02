/* patch.c -- patches as data. To add a sound: add one block of constants
 * here. A patch that has made a tune is never changed; a changed one gets a
 * new name. */
#include "synth/patch.h"

#include <stddef.h>
#include <string.h>

#include "synth/reverb.h"

/* The old code gave every time as a count of samples at 44.1 kHz. */
#define SAMPLES(n) ((n) / 44100.0)

/* ------------------------------------------------- v4: 14 February 2011 */

static const ReverbConfig V4_REVERB = {
    .n_taps = 5,
    .taps = {
        /*  delay            gain  swap */
        { SAMPLES( 7000.0),  0.35, 0 },
        { SAMPLES( 9500.0),  0.10, 1 },
        { SAMPLES(12000.0),  0.20, 0 },
        { SAMPLES(17000.0),  0.08, 1 },
        { SAMPLES(19000.0),  0.12, 0 },
    }
};

/* ------------------------------------------------------------- the list */

static const Patch PATCHES[] = {
    {
        .name                   = "v4",
        .wave                   = "cosine",
        .phase_restarts         = 0,
        .loudness               = "inverse_freq",
        .loudness_constant      = 600000.0 / 32768.0,   /* 600000 / frequency in 16-bit units */
        .strike_gain            = "linear",
        .envelope               = "v4",
        .vibrato_depth          = 0.0005,
        .vibrato_period_seconds = SAMPLES(18000.0),
        .vibrato_spread         = 0.001,
        .pan                    = "linear",
        .n_effects              = 1,
        .effects                = { { "reverb", &V4_REVERB } },
    },
};

int patch_count(void)
{
    return (int) (sizeof PATCHES / sizeof PATCHES[0]);
}

const Patch *patch_at(int index)
{
    if (index < 0 || index >= patch_count()) return NULL;
    return &PATCHES[index];
}

const Patch *patch_find(const char *name)
{
    int i;
    for (i = 0; i < patch_count(); i++)
        if (strcmp(PATCHES[i].name, name) == 0) return &PATCHES[i];
    return NULL;
}
