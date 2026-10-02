/* synth_rates.c -- sample rates (plan section 7, item 7): renders at 44.1
 * and 48 kHz start every tick at the same time (to within one sample), have
 * the same loudness per cycle (to within 1 % for the voices alone; see main
 * for the pass mark with reverb), and end within one second of each other.
 *
 *   synth_rates favourite.score
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "synth/render.h"
#include "synth_common.h"

#define CYCLES 470     /* cycles 0..469 of the favourite hold the music */

static const double RATES[2] = { 44100.0, 48000.0 };

/* ------------------------------------------------------- tick starts */

/* A sink that only records the first frame that is not digital silence. */
typedef struct {
    Sink    sink;
    int64_t frame;        /* frames received so far                 */
    int64_t first_sound;  /* the first frame that is not zero, or -1 */
} FirstSound;

static int first_sound_write(Sink *sink, const double *frames, int n_frames)
{
    FirstSound *found = sink->state;
    int i;

    for (i = 0; i < n_frames; i++, found->frame++)
        if (found->first_sound < 0 && (frames[2 * i] != 0.0 || frames[2 * i + 1] != 0.0))
            found->first_sound = found->frame;
    return 0;
}

/* Renders a single tone created at `tick`, without reverb, and returns the
 * time of its first frame as found in the audio. Everything before the tone
 * is digital silence, and a strike starts at gain 0, so the tone's first
 * frame is the one just before the first sample that is not zero. */
static double measured_start(const Timing *timing, int32_t tick, double rate)
{
    Patch dry = dry_patch("v4");
    Score score;
    FirstSound found;
    RenderStats stats;
    char err[256];

    found.sink.write = first_sound_write;
    found.sink.state = &found;
    found.frame = 0;
    found.first_sound = -1;

    one_tone_score(&score, tick, 440.0, 0.5, 1);
    if (render_score(&score, timing, &dry, rate, 1.0, &found.sink, &stats, err, sizeof err) != 0)
        fail("render: %s", err);
    if (found.first_sound < 1) fail("the tone at tick %d cannot be found in the audio", (int) tick);
    score_free(&score);
    return (double) (found.first_sound - 1) / rate;
}

static void check_tick_starts(const char *label, const Timing *timing)
{
    static const int32_t TICKS[] = { 1, 2, 7, 11, 100, 501, 1234, 3299 };
    size_t i;

    for (i = 0; i < sizeof TICKS / sizeof TICKS[0]; i++) {
        double exact = timing_tick_seconds(timing, 11, TICKS[i]);
        double at_44k = measured_start(timing, TICKS[i], RATES[0]);
        double at_48k = measured_start(timing, TICKS[i], RATES[1]);

        printf("%s  tick %4d: starts at %.6f s (44.1 kHz), %.6f s (48 kHz), exactly %.6f s\n",
               label, (int) TICKS[i], at_44k, at_48k, exact);
        if (fabs(at_44k - exact) > 0.5 / RATES[0] + 1e-9 || fabs(at_48k - exact) > 0.5 / RATES[1] + 1e-9)
            fail("tick %d does not start on the frame nearest to its time", (int) TICKS[i]);
        if (fabs(at_44k - at_48k) > 1.0 / RATES[0])
            fail("tick %d starts more than one sample apart at the two rates", (int) TICKS[i]);
    }
}

/* -------------------------------------------- loudness per cycle, and length */

/* A sink that only adds up the energy of each cycle, per channel. */
typedef struct {
    Sink     sink;
    int64_t  frame;                  /* frames received so far           */
    int      cycle;                  /* the cycle that frame lies in     */
    int64_t  cycle_end[CYCLES];      /* the frame at which a cycle ends  */
    double   energy[CYCLES][2];
} Loudness;

static int loudness_write(Sink *sink, const double *frames, int n_frames)
{
    Loudness *loudness = sink->state;
    int i;

    for (i = 0; i < n_frames; i++, loudness->frame++) {
        while (loudness->cycle < CYCLES && loudness->frame >= loudness->cycle_end[loudness->cycle])
            loudness->cycle++;
        if (loudness->cycle == CYCLES) continue;    /* the ring-out */
        loudness->energy[loudness->cycle][0] += frames[2 * i] * frames[2 * i];
        loudness->energy[loudness->cycle][1] += frames[2 * i + 1] * frames[2 * i + 1];
    }
    return 0;
}

/* Renders the favourite at both rates through the given patch and compares
 * the loudness (root of the mean square) of every cycle, per channel, the
 * loudness of the whole piece, and the lengths. */
static void check_favourite(const char *label, const char *score_path, const Patch *patch,
                            double cycle_limit)
{
    Score score;
    Timing timing = timing_from_bins(1700.0, 0.0);
    Loudness *loudness = calloc(2, sizeof *loudness);
    RenderStats stats[2];
    char err[256];
    double worst = 0.0, seconds[2], whole[2] = { 0.0, 0.0 }, whole_apart;
    int r, cycle, channel, worst_cycle = 0;

    if (!loudness) fail("out of memory");
    load_score(score_path, &score);
    printf("%s:\n", label);

    for (r = 0; r < 2; r++) {
        loudness[r].sink.write = loudness_write;
        loudness[r].sink.state = &loudness[r];
        for (cycle = 0; cycle < CYCLES; cycle++)
            loudness[r].cycle_end[cycle] =
                timing_tick_frame(&timing, score.slots, (int64_t) (cycle + 1) * score.slots, RATES[r]);
        if (render_score(&score, &timing, patch, RATES[r], 1.0, &loudness[r].sink, &stats[r],
                         err, sizeof err) != 0)
            fail("render at %g Hz: %s", RATES[r], err);
        seconds[r] = (double) stats[r].frames / RATES[r];
        printf("  %g Hz: %ld frames, %.3f s; music ends at %.4f s; peak %.5f\n", RATES[r],
               (long) stats[r].frames, seconds[r], (double) stats[r].music_frames / RATES[r], stats[r].peak);
    }

    for (cycle = 0; cycle < CYCLES; cycle++) {
        int64_t frames_44k = loudness[0].cycle_end[cycle] - (cycle ? loudness[0].cycle_end[cycle - 1] : 0);
        int64_t frames_48k = loudness[1].cycle_end[cycle] - (cycle ? loudness[1].cycle_end[cycle - 1] : 0);

        for (channel = 0; channel < 2; channel++) {
            double at_44k = sqrt(loudness[0].energy[cycle][channel] / (double) frames_44k);
            double at_48k = sqrt(loudness[1].energy[cycle][channel] / (double) frames_48k);
            double apart;

            whole[0] += loudness[0].energy[cycle][channel] / RATES[0];
            whole[1] += loudness[1].energy[cycle][channel] / RATES[1];

            /* Without reverb a cycle can be silent on one side: then on both. */
            if (at_44k == 0.0 && at_48k == 0.0) continue;
            if (at_44k == 0.0 || at_48k == 0.0)
                fail("cycle %d is silent at one sample rate only", cycle);

            apart = fabs(at_48k / at_44k - 1.0);
            if (apart > worst) {
                worst = apart;
                worst_cycle = cycle;
            }
        }
    }
    printf("  loudness per cycle: at most %.3f %% apart (cycle %d); pass mark %g %%\n",
           100.0 * worst, worst_cycle, 100.0 * cycle_limit);
    if (worst > cycle_limit) fail("the loudness of a cycle differs by more than %g %%", 100.0 * cycle_limit);

    whole_apart = fabs(sqrt(whole[1] / whole[0]) - 1.0);
    printf("  loudness of the whole piece: %.4f %% apart; pass mark 1 %%\n", 100.0 * whole_apart);
    if (whole_apart > 0.01) fail("the loudness of the whole piece differs by more than 1 %%");

    printf("  lengths: %.3f s apart; pass mark 1 s\n", fabs(seconds[0] - seconds[1]));
    if (fabs(seconds[0] - seconds[1]) > 1.0) fail("the two renders end more than one second apart");
    if (fabs((double) stats[0].music_frames / RATES[0] - (double) stats[1].music_frames / RATES[1])
        > 1.0 / RATES[0])
        fail("the music ends more than one sample apart");

    free(loudness);
    score_free(&score);
}

int main(int argc, char **argv)
{
    Timing v4 = timing_from_bins(1700.0, 0.0);
    Timing v6 = timing_from_bins(3700.0, 1.0);
    const Patch *patch = patch_find("v4");
    Patch dry = dry_patch("v4");

    if (argc != 2) fail("usage: synth_rates favourite.score");
    if (!patch) fail("there is no patch \"v4\"");

    check_tick_starts("v4 tempo", &v4);
    check_tick_starts("v6 tempo", &v6);

    /* The voices alone must be as loud at either rate, cycle by cycle. */
    check_favourite("the favourite, voices only (no reverb)", argv[1], &dry, 0.01);

    /* Through the reverb the pass mark is wider. Its delays are whole frames,
     * so at 48 kHz they are up to 8 millionths of a second off, which moves
     * its resonances a little: single cycles come out up to 6 % louder or
     * quieter. The piece as a whole must still agree to 1 %. */
    check_favourite("the favourite, with reverb", argv[1], patch, 0.10);

    printf("ok: sample rates\n");
    return 0;
}
