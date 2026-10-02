/* effects.h -- what an effect must provide. An effect processes the mixed
 * stereo signal in place. A patch lists its effects in order.
 */
#ifndef CHIMES_EFFECTS_H
#define CHIMES_EFFECTS_H

typedef struct {
    const char *name;
    /* Builds the effect's state from its settings. NULL on failure. */
    void  *(*create)(const void *config, double sample_rate);
    /* Processes n_frames interleaved left/right frames in place. */
    void   (*process)(void *state, double *frames, int n_frames);
    /* The longest time the effect holds on to its input; used for ring-out. */
    double (*longest_delay_seconds)(const void *state);
    void   (*destroy)(void *state);
} Effect;

/* The list of effects, by name. NULL for an unknown name. */
const Effect *effect_find(const char *name);

#endif
