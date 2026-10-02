/* synth_timing.c -- checks timing.c: the closed form for the start of a tick
 * against a slot-by-slot sum, with and without drift, and the known sample
 * positions of the 2011 tunes.
 */
#include <math.h>
#include <stdio.h>

#include "synth/timing.h"
#include "synth_common.h"

/* Walks `ticks` ticks, adding up slot lengths one by one in whole samples at
 * 44.1 kHz, and compares every tick with the closed form. */
static void check_against_sum(const char *label, int bins, int drift_bins, int slots, int ticks)
{
    Timing timing = timing_from_bins(bins, drift_bins);
    int64_t sum = 0;     /* samples at 44.1 kHz up to this tick */
    int64_t tick;

    for (tick = 0; tick <= ticks; tick++) {
        int64_t cycle = tick / slots;
        double  seconds = timing_tick_seconds(&timing, slots, tick);
        int64_t frame = timing_tick_frame(&timing, slots, tick, 44100.0);
        int64_t frame_48k = timing_tick_frame(&timing, slots, tick, 48000.0);

        if (fabs(seconds - (double) sum / 44100.0) > 1e-9)
            fail("%s: tick %ld starts at %.12f s, the sum says %.12f s",
                 label, (long) tick, seconds, (double) sum / 44100.0);
        if (frame != sum)
            fail("%s: tick %ld starts at frame %ld, the sum says %ld",
                 label, (long) tick, (long) frame, (long) sum);
        /* At another sample rate the tick falls on the nearest frame. */
        if (fabs((double) frame_48k / 48000.0 - seconds) > 0.5 / 48000.0 + 1e-12)
            fail("%s: tick %ld at 48 kHz is more than half a frame off", label, (long) tick);

        sum += bins + 1 + cycle * drift_bins;    /* this slot's length */
    }
    printf("%-28s %d ticks agree with the slot-by-slot sum\n", label, ticks);
}

int main(void)
{
    Timing v4 = timing_from_bins(1700.0, 0.0);
    Timing v6 = timing_from_bins(3700.0, 1.0);
    int64_t tick, frame;

    if (v4.slot_seconds != 1701.0 / 44100.0 || v4.drift_seconds != 0.0)
        fail("timing_from_bins(1700, 0) is not 1701/44100 s per slot");
    if (v6.slot_seconds != 3701.0 / 44100.0 || v6.drift_seconds != 1.0 / 44100.0)
        fail("timing_from_bins(3700, 1) is not 3701/44100 s per slot, plus 1/44100 s per cycle");

    check_against_sum("v4 tempo (1700, no drift)", 1700, 0, 11, 5500);
    check_against_sum("v6 tempo (3700, drift +1)", 3700, 1, 11, 3300);
    check_against_sum("speeding up (2000, drift -3)", 2000, -3, 11, 3300);
    check_against_sum("7 slots (500, drift +2)", 500, 2, 7, 2000);

    /* The 2011 positions: with the v4 tempo tick n starts at sample 1701 n. */
    for (tick = 0; tick <= 5500; tick++) {
        frame = timing_tick_frame(&v4, 11, tick, 44100.0);
        if (frame != 1701 * tick) fail("v4 tempo: tick %ld starts at frame %ld", (long) tick, (long) frame);
    }
    printf("v4 tempo: tick n starts at frame 1701 n, for n = 0..5500\n");

    /* The v6 example: 3700 bins, +1 per cycle, 300 cycles. */
    frame = timing_tick_frame(&v6, 11, 300 * 11, 44100.0);
    printf("v6 tempo: 300 cycles last %ld frames (expected 12706650)\n", (long) frame);
    if (frame != 12706650) fail("v6 tempo: 300 cycles should last 12706650 frames");

    printf("ok: timing\n");
    return 0;
}
