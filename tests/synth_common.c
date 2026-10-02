/* synth_common.c -- shared by the synthesizer's test helpers. */
#include "synth_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "score/score_text.h"

void fail(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printf("FAIL: ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
    exit(1);
}

void load_score(const char *path, Score *score)
{
    char err[256];
    FILE *file = fopen(path, "r");

    if (!file) fail("cannot open %s", path);
    score_init(score);
    if (score_read(file, score, err, sizeof err) != 0) fail("%s: %s", path, err);
    fclose(file);
}

void one_tone_score(Score *score, int32_t tick, double freq_hz, double pan, int32_t reps)
{
    Tone tone;

    score_init(score);
    snprintf(score->rules, sizeof score->rules, "test");
    snprintf(score->generator, sizeof score->generator, "none");
    score->cycles = 32;
    score->slots = 11;

    tone.tick = tick;
    tone.parent = -1;
    tone.num = tone.den = 0;
    tone.freq_hz = freq_hz;
    tone.pan = pan;
    tone.reps = reps;
    tone.level = 1.0;
    tone.jitter = -1;
    if (score_add(score, &tone) != 0) fail("out of memory");
}

Patch dry_patch(const char *name)
{
    const Patch *patch = patch_find(name);
    Patch dry;

    if (!patch) fail("there is no patch \"%s\"", name);
    dry = *patch;
    dry.n_effects = 0;
    return dry;
}

static int capture_write(Sink *sink, const double *frames, int n_frames)
{
    Capture *capture = sink->state;

    if (capture->frames + n_frames > capture->capacity) {
        int64_t capacity = capture->capacity ? capture->capacity * 2 : 65536;
        double *samples;
        while (capacity < capture->frames + n_frames) capacity *= 2;
        samples = realloc(capture->samples, (size_t) capacity * 2 * sizeof *samples);
        if (!samples) return -1;
        capture->samples = samples;
        capture->capacity = capacity;
    }
    memcpy(capture->samples + 2 * capture->frames, frames, (size_t) n_frames * 2 * sizeof *frames);
    capture->frames += n_frames;
    return 0;
}

static int capture_close(Sink *sink)
{
    (void) sink;
    return 0;
}

void capture_init(Capture *capture)
{
    memset(capture, 0, sizeof *capture);
    capture->sink.write = capture_write;
    capture->sink.close = capture_close;
    capture->sink.state = capture;
}

void capture_free(Capture *capture)
{
    free(capture->samples);
    capture->samples = NULL;
    capture->frames = capture->capacity = 0;
}
