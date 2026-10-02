/* render.h -- runs a score through voices and effects into an output.
 * The synthesizer never touches a random generator and knows nothing about
 * rules.
 */
#ifndef CHIMES_RENDER_H
#define CHIMES_RENDER_H

#include <stddef.h>
#include <stdint.h>

#include "output/sink.h"
#include "score/tone.h"
#include "synth/patch.h"
#include "synth/timing.h"

#define RENDER_DEFAULT_RATE   44100.0
#define RENDER_RING_OUT_LIMIT 60.0     /* seconds */

typedef struct {
    int64_t frames;         /* frames produced, ring-out included                        */
    int64_t music_frames;   /* frames up to the end of the last strike with non-zero gain */
    double  peak;           /* largest |sample| after gain; 1.0 = full scale              */
} RenderStats;

/* Renders the score: voices (wave form, then modulations), then the patch's
 * effects, then the output gain. After the last strike with non-zero gain it
 * keeps feeding silence through the effects until both channels have stayed
 * below half a 16-bit step for longer than the longest effect delay (at most
 * RENDER_RING_OUT_LIMIT seconds). sink may be NULL, to measure only.
 * Returns 0, or -1 with a message in err. */
int render_score(const Score *score, const Timing *timing, const Patch *patch,
                 double sample_rate, double gain, Sink *sink,
                 RenderStats *stats, char *err, size_t errlen);

#endif
