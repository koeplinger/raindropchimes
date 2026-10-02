/* patch.h -- a patch: the description of a sound. A patch is data; it names
 * its shapes (see shapes.h) and lists its effects in order. Once a tune has
 * been made with a named patch, that patch is never changed.
 */
#ifndef CHIMES_PATCH_H
#define CHIMES_PATCH_H

#define PATCH_MAX_EFFECTS 4

typedef struct {
    const char *effect;      /* name in the list of effects */
    const void *config;      /* that effect's settings      */
} EffectSpec;

typedef struct {
    const char *name;

    const char *wave;                    /* wave form                                        */
    int         phase_restarts;          /* 0: phase runs on from tone to tone (v4); 1: restarts at zero */
    const char *loudness;                /* loudness law ...                                 */
    double      loudness_constant;       /* ... and its constant                             */
    const char *strike_gain;             /* strike-gain law                                  */
    const char *envelope;                /* envelope of one strike                           */
    double      vibrato_depth;           /* as a fraction of the frequency                   */
    double      vibrato_period_seconds;  /* base period                                      */
    double      vibrato_spread;          /* period = base * (1 + spread * jitter)            */
    const char *pan;                     /* pan law                                          */

    int         n_effects;
    EffectSpec  effects[PATCH_MAX_EFFECTS];
} Patch;

/* The built-in patches. patch_find returns NULL for an unknown name. */
const Patch *patch_find(const char *name);
int          patch_count(void);
const Patch *patch_at(int index);

#endif
