/* timing.c -- ticks to seconds, and to sample indexes. */
#include "synth/timing.h"

#include <math.h>

Timing timing_from_bins(double bins, double drift_bins)
{
    Timing timing;
    timing.slot_seconds = (bins + 1.0) / TIMING_BINS_RATE;
    timing.drift_seconds = drift_bins / TIMING_BINS_RATE;
    return timing;
}

double timing_tick_seconds(const Timing *timing, int slots, int64_t tick)
{
    double cycle = (double) (tick / slots);   /* whole cycles before this tick  */
    double slot = (double) (tick % slots);    /* slots into the current cycle   */

    /* Cycle k lasts slots * (slot_seconds + k * drift_seconds). Summed over
     * k = 0 .. cycle-1, the drift part is 0 + 1 + ... + (cycle-1). */
    double drift_steps = cycle * (cycle - 1.0) / 2.0;
    double whole_cycles = slots * (cycle * timing->slot_seconds + drift_steps * timing->drift_seconds);
    double this_cycle = slot * (timing->slot_seconds + cycle * timing->drift_seconds);

    return whole_cycles + this_cycle;
}

int64_t timing_tick_frame(const Timing *timing, int slots, int64_t tick, double sample_rate)
{
    return (int64_t) llround(timing_tick_seconds(timing, slots, tick) * sample_rate);
}
