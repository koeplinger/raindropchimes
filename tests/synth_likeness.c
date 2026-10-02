/* synth_likeness.c -- likeness to a 2011 recording (plan section 7, item 6):
 * renders a score into a WAV file and compares it, wave for wave, with the
 * decoded recording over the cycles that hold music.
 *
 *   synth_likeness SCORE RECORDING.s16le SCRATCH.wav [PATCH BINS DRIFT [ALL CYCLE QUIET]]
 *
 * RECORDING.s16le is the 2011 OGG decoded to 16-bit stereo at 44.1 kHz with
 * no header. PATCH, BINS and DRIFT say how the recording was made (default:
 * v4, 1700, 0, the favourite tune). Pass marks, per channel: the two line up
 * with no time shift; overall correlation at least ALL (default 0.999); every
 * single cycle at least CYCLE (default 0.99).
 *
 * QUIET (default 0: every cycle is judged) leaves out the cycles in which the
 * recording is quieter than that, as an average size of its 16-bit samples.
 * In a nearly silent cycle the noise of the OGG compression is as large as
 * the music, and no render could correlate with it.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "synth/render.h"
#include "synth_common.h"

#define RATE        44100
#define MAX_SHIFT   50        /* frames */

/* Reads 16-bit stereo frames (lowest byte first) after skipping a header. */
static short *read_frames(const char *path, long skip_bytes, long *n_frames)
{
    FILE *file = fopen(path, "rb");
    unsigned char *bytes;
    short *samples;
    long n_bytes, i;

    if (!file) fail("cannot open %s", path);
    fseek(file, 0L, SEEK_END);
    n_bytes = ftell(file) - skip_bytes;
    fseek(file, skip_bytes, SEEK_SET);
    if (n_bytes < 4) fail("%s holds no audio", path);

    bytes = malloc((size_t) n_bytes);
    samples = malloc((size_t) (n_bytes / 2) * sizeof *samples);
    if (!bytes || !samples) fail("out of memory");
    if (fread(bytes, 1, (size_t) n_bytes, file) != (size_t) n_bytes) fail("cannot read %s", path);
    fclose(file);

    for (i = 0; i < n_bytes / 2; i++) {
        int value = bytes[2 * i] | (bytes[2 * i + 1] << 8);
        samples[i] = (short) (value >= 32768 ? value - 65536 : value);
    }
    free(bytes);
    *n_frames = n_bytes / 4;
    return samples;
}

/* Correlation of one channel of a with the same channel of b, shifted by
 * `shift` frames, over frames from .. to-1 of a. 1 = the same wave. */
static double correlation(const short *a, const short *b, int channel, long from, long to, long shift)
{
    double ab = 0.0, aa = 0.0, bb = 0.0;
    long i;

    for (i = from; i < to; i++) {
        double x = a[2 * i + channel];
        double y = b[2 * (i + shift) + channel];
        ab += x * y;
        aa += x * x;
        bb += y * y;
    }
    if (aa == 0.0 || bb == 0.0) return 0.0;
    return ab / sqrt(aa * bb);
}

int main(int argc, char **argv)
{
    static const char *const CHANNEL[2] = { "left", "right" };
    Score score;
    Timing timing;
    const Patch *patch;
    const SinkFormat *format = sink_format_find("wav");
    Sink *sink;
    RenderStats stats;
    char err[256];
    short *ours, *theirs;
    long our_frames, their_frames, music_frames;
    double pass_all = 0.999, pass_cycle = 0.99, quiet = 0.0;
    int cycles, channel, passed = 1;

    if (argc != 4 && argc != 7 && argc != 10)
        fail("usage: synth_likeness SCORE RECORDING.s16le SCRATCH.wav "
             "[PATCH BINS DRIFT [ALL CYCLE QUIET]]");
    patch = patch_find(argc > 4 ? argv[4] : "v4");
    timing = argc > 4 ? timing_from_bins(atof(argv[5]), atof(argv[6])) : timing_from_bins(1700.0, 0.0);
    if (argc > 7) {
        pass_all = atof(argv[7]);
        pass_cycle = atof(argv[8]);
        quiet = atof(argv[9]);
    }
    if (!patch || !format) fail("the patch or the wav format is missing");
    load_score(argv[1], &score);

    sink = format->open(argv[3], RATE);
    if (!sink) fail("cannot open %s for writing", argv[3]);
    if (render_score(&score, &timing, patch, RATE, 1.0, sink, &stats, err, sizeof err) != 0)
        fail("render: %s", err);
    if (sink->close(sink) != 0) fail("closing %s failed", argv[3]);

    ours = read_frames(argv[3], 44L, &our_frames);
    theirs = read_frames(argv[2], 0L, &their_frames);

    /* The cycles that hold music: every cycle that begins before the music
     * ends, as far as the recording reaches. */
    cycles = 0;
    while (timing_tick_frame(&timing, score.slots, (int64_t) cycles * score.slots, RATE) < stats.music_frames)
        cycles++;
    while (cycles > 0 && timing_tick_frame(&timing, score.slots, (int64_t) cycles * score.slots, RATE)
                         + MAX_SHIFT > their_frames)
        cycles--;
    music_frames = (long) timing_tick_frame(&timing, score.slots, (int64_t) cycles * score.slots, RATE);
    printf("comparing cycles 0..%d: frames 0..%ld (%.2f s); the recording has %ld frames\n",
           cycles - 1, music_frames, (double) music_frames / RATE, their_frames);
    if (cycles < 1) fail("there is nothing to compare");
    if (our_frames < music_frames) fail("the render is too short (%ld frames)", our_frames);

    for (channel = 0; channel < 2; channel++) {
        long   shift, best_shift = 0;
        double best = -2.0, overall, worst = 2.0;
        int    cycle, worst_cycle = 0, too_quiet = 0;

        /* Which time shift lines the two up best? It must be none. */
        for (shift = -MAX_SHIFT; shift <= MAX_SHIFT; shift++) {
            double c = correlation(ours, theirs, channel, MAX_SHIFT, music_frames, shift);
            if (c > best) {
                best = c;
                best_shift = shift;
            }
        }

        overall = correlation(ours, theirs, channel, 0, music_frames, 0);

        for (cycle = 0; cycle < cycles; cycle++) {
            long from = (long) timing_tick_frame(&timing, score.slots, (int64_t) cycle * score.slots, RATE);
            long to = (long) timing_tick_frame(&timing, score.slots, (int64_t) (cycle + 1) * score.slots, RATE);
            double c, size = 0.0;
            long   i;

            /* How loud is the recording in this cycle? */
            for (i = from; i < to; i++)
                size += (double) theirs[2 * i + channel] * (double) theirs[2 * i + channel];
            size = sqrt(size / (double) (to - from));
            if (size < quiet) {
                too_quiet++;
                continue;
            }

            c = correlation(ours, theirs, channel, from, to, 0);
            if (c < worst) {
                worst = c;
                worst_cycle = cycle;
            }
        }

        printf("%-5s  best shift %ld frames   overall %.5f   worst cycle %.4f (cycle %d)",
               CHANNEL[channel], best_shift, overall, worst, worst_cycle);
        if (quiet > 0.0) printf("   %d of %d cycles too quiet to judge", too_quiet, cycles);
        printf("\n");
        if (too_quiet == cycles) fail("every cycle of the recording is too quiet to judge");
        if (best_shift != 0 || overall < pass_all || worst < pass_cycle) passed = 0;
    }

    free(ours);
    free(theirs);
    score_free(&score);
    if (!passed)
        fail("pass marks: shift 0, overall >= %g, every judged cycle >= %g", pass_all, pass_cycle);
    printf("ok: the render is like the 2011 recording\n");
    return 0;
}
