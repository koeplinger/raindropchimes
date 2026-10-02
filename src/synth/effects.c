/* effects.c -- the list of effects. To add one: write a file that provides
 * create / process / longest-delay / destroy, and add one line here. */
#include "synth/effects.h"

#include <stddef.h>
#include <string.h>

#include "synth/reverb.h"
#include "synth/soften.h"

static const Effect *const EFFECTS[] = {
    &REVERB_EFFECT,
    &SOFTEN_EFFECT,
};

const Effect *effect_find(const char *name)
{
    size_t i;
    for (i = 0; i < sizeof EFFECTS / sizeof EFFECTS[0]; i++)
        if (strcmp(EFFECTS[i]->name, name) == 0) return EFFECTS[i];
    return NULL;
}
