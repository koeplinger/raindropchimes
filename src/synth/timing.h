/* timing.h -- tempo: how ticks become seconds. In cycle c a slot lasts
 * slot_seconds + c * drift_seconds.
 */
#ifndef CHIMES_TIMING_H
#define CHIMES_TIMING_H

#include <stdint.h>

#define TIMING_BINS_RATE 44100.0   /* the old tempo unit counts samples at 44.1 kHz */

typedef struct {
    double slot_seconds;
    double drift_seconds;
} Timing;

/* The old tempo unit: a slot lasts (bins + 1) / 44100 seconds, and grows by
 * drift_bins / 44100 seconds per cycle. */
Timing timing_from_bins(double bins, double drift_bins);

/* The time at which a tick starts, in seconds from the start of the piece. */
double timing_tick_seconds(const Timing *timing, int slots, int64_t tick);

/* The same as a sample index: round(seconds * sample_rate). Slot lengths are
 * never rounded one by one, so every tick falls at the same time at any
 * sample rate. */
int64_t timing_tick_frame(const Timing *timing, int slots, int64_t tick, double sample_rate);

#endif
