/* synth_render_errors.c -- checks what render_score refuses and where it
 * stops: unknown names in a patch, settings that cannot be rendered, a value
 * that is not a number, the output gain, and the upper limit of the ring-out.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "synth/render.h"
#include "synth/reverb.h"
#include "synth_common.h"

#define RATE 44100.0

/* Renders and expects a refusal whose message contains `expect`. */
static void must_refuse(const char *what, const Score *score, const Timing *timing,
                        const Patch *patch, double rate, double gain, const char *expect)
{
    RenderStats stats;
    char err[256] = "";

    if (render_score(score, timing, patch, rate, gain, NULL, &stats, err, sizeof err) == 0)
        fail("%s: was rendered, should have been refused", what);
    if (!strstr(err, expect))
        fail("%s: the message \"%s\" does not mention \"%s\"", what, err, expect);
    printf("refused  %-28s \"%s\"\n", what, err);
}

int main(void)
{
    Score score;
    Timing timing = timing_from_bins(1700.0, 0.0);
    Timing bad_timing;
    const Patch *v4 = patch_find("v4");
    Patch patch;
    ReverbConfig echo;
    Capture capture;
    RenderStats stats, again;
    char err[256];
    int64_t i;

    if (!v4) fail("there is no patch \"v4\"");
    if (patch_find("no-such-patch")) fail("patch_find finds a patch that does not exist");
    if (patch_count() < 1 || patch_at(0) != v4 || patch_at(patch_count()) != NULL || patch_at(-1) != NULL)
        fail("patch_count / patch_at do not agree with patch_find");

    one_tone_score(&score, 0, 440.0, 0.5, 1);    /* one cycle of music */

    /* Every name in a patch is looked up. */
    patch = *v4; patch.wave = "square-ish";
    must_refuse("unknown wave form", &score, &timing, &patch, RATE, 1.0, "square-ish");
    patch = *v4; patch.envelope = "v99";
    must_refuse("unknown envelope", &score, &timing, &patch, RATE, 1.0, "v99");
    patch = *v4; patch.loudness = "flat-ish";
    must_refuse("unknown loudness law", &score, &timing, &patch, RATE, 1.0, "flat-ish");
    patch = *v4; patch.strike_gain = "steps";
    must_refuse("unknown strike-gain law", &score, &timing, &patch, RATE, 1.0, "steps");
    patch = *v4; patch.pan = "round";
    must_refuse("unknown pan law", &score, &timing, &patch, RATE, 1.0, "round");
    patch = *v4; patch.effects[0].effect = "chorus";
    must_refuse("unknown effect", &score, &timing, &patch, RATE, 1.0, "chorus");
    patch = *v4; patch.n_effects = PATCH_MAX_EFFECTS + 1;
    must_refuse("too many effects", &score, &timing, &patch, RATE, 1.0, "effects");
    patch = *v4; patch.vibrato_period_seconds = 0.0;
    must_refuse("no vibrato period", &score, &timing, &patch, RATE, 1.0, "vibrato");

    /* Settings that cannot be rendered. */
    must_refuse("sample rate 0", &score, &timing, v4, 0.0, 1.0, "sample rate");
    must_refuse("output gain not a number", &score, &timing, v4, RATE, NAN, "gain");
    bad_timing = timing_from_bins(-1.0, 0.0);
    must_refuse("slots of no length", &score, &bad_timing, v4, RATE, 1.0, "slot");
    bad_timing = timing_from_bins(1.0, -1.0);
    must_refuse("slots shrinking to nothing", &score, &bad_timing, v4, RATE, 1.0, "slot");

    /* A value that is not a number is an error and is never written. Here
     * the very first sample is infinity times zero. */
    patch = *v4; patch.loudness_constant = INFINITY;
    capture_init(&capture);
    if (render_score(&score, &timing, &patch, RATE, 1.0, &capture.sink, &stats, err, sizeof err) == 0)
        fail("a render full of invalid numbers was accepted");
    if (capture.frames != 0) fail("%ld frames were written before the error", (long) capture.frames);
    printf("refused  %-28s \"%s\"\n", "invalid number at the start", err);
    capture_free(&capture);

    /* A reverb that doubles with every echo runs away: an error part way
     * through; everything written before it is valid. */
    echo.n_taps = 1;
    echo.taps[0].delay_seconds = 0.001;
    echo.taps[0].gain = 2.0;
    echo.taps[0].swap = 0;
    patch = *v4; patch.effects[0].config = &echo;
    capture_init(&capture);
    if (render_score(&score, &timing, &patch, RATE, 1.0, &capture.sink, &stats, err, sizeof err) == 0)
        fail("a runaway reverb was accepted");
    if (capture.frames == 0) fail("the runaway reverb should have failed part way through");
    for (i = 0; i < 2 * capture.frames; i++)
        if (!isfinite(capture.samples[i])) fail("an invalid number was written");
    printf("refused  %-28s \"%s\" (after %ld valid frames)\n", "runaway reverb", err, (long) capture.frames);
    capture_free(&capture);

    /* A reverb that never fades: the ring-out stops at its upper limit. */
    echo.taps[0].delay_seconds = 0.01;
    echo.taps[0].gain = 1.0;
    if (render_score(&score, &timing, &patch, RATE, 1.0, NULL, &stats, err, sizeof err) != 0)
        fail("everlasting reverb: %s", err);
    printf("everlasting reverb: ring-out stopped after %.3f s\n",
           (double) (stats.frames - stats.music_frames) / RATE);
    if (stats.frames - stats.music_frames != (int64_t) (RENDER_RING_OUT_LIMIT * RATE))
        fail("the ring-out should stop at %g s", RENDER_RING_OUT_LIMIT);

    /* The output gain comes last: half the gain, half the peak, and a
     * shorter ring-out. Measuring (no sink) gives the same numbers as
     * writing. */
    if (render_score(&score, &timing, v4, RATE, 1.0, NULL, &stats, err, sizeof err) != 0)
        fail("render: %s", err);
    capture_init(&capture);
    if (render_score(&score, &timing, v4, RATE, 1.0, &capture.sink, &again, err, sizeof err) != 0)
        fail("render: %s", err);
    if (again.frames != stats.frames || again.music_frames != stats.music_frames || again.peak != stats.peak)
        fail("measuring and writing give different numbers");
    if (capture.frames != stats.frames) fail("the sink received %ld of %ld frames",
                                             (long) capture.frames, (long) stats.frames);
    capture_free(&capture);
    if (render_score(&score, &timing, v4, RATE, 0.5, NULL, &again, err, sizeof err) != 0)
        fail("render: %s", err);
    printf("gain 1:   peak %.6f, %ld frames\ngain 0.5: peak %.6f, %ld frames\n",
           stats.peak, (long) stats.frames, again.peak, (long) again.frames);
    if (fabs(again.peak - 0.5 * stats.peak) > 1e-12) fail("half the gain should give half the peak");
    if (again.music_frames != stats.music_frames || again.frames >= stats.frames)
        fail("half the gain should give the same music and a shorter ring-out");

    score_free(&score);
    printf("ok: render refusals and limits\n");
    return 0;
}
