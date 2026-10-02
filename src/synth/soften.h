/* soften.h -- the effect "soften": a gentle low-pass filter. It takes the
 * edge off high tones and overtones and leaves low ones almost as they are.
 */
#ifndef CHIMES_SOFTEN_H
#define CHIMES_SOFTEN_H

#include "synth/effects.h"

typedef struct {
    double cutoff_hz;    /* tones well below this frequency pass almost unchanged */
} SoftenConfig;

extern const Effect SOFTEN_EFFECT;

#endif
