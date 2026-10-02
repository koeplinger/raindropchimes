/* reverb.h -- the reverb: delayed copies of the output fed back into it.
 *
 *   out = in + sum of gain_i * out(delayed by delay_i)
 *
 * A tap with swap set reads the other channel's output.
 */
#ifndef CHIMES_REVERB_H
#define CHIMES_REVERB_H

#include "synth/effects.h"

#define REVERB_MAX_TAPS 8

typedef struct {
    double delay_seconds;
    double gain;
    int    swap;
} ReverbTap;

typedef struct {
    int       n_taps;
    ReverbTap taps[REVERB_MAX_TAPS];
} ReverbConfig;

extern const Effect REVERB_EFFECT;

#endif
