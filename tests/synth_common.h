/* synth_common.h -- shared by the synthesizer's test helpers (tests/synth_*.c). */
#ifndef CHIMES_TEST_SYNTH_COMMON_H
#define CHIMES_TEST_SYNTH_COMMON_H

#include <stdint.h>

#include "output/sink.h"
#include "score/tone.h"
#include "synth/patch.h"

/* Prints "FAIL: ..." and stops the helper with exit status 1. */
void fail(const char *format, ...);

/* Reads a score file, or stops the helper. */
void load_score(const char *path, Score *score);

/* A score that holds a single root tone, created at the given tick. */
void one_tone_score(Score *score, int32_t tick, double freq_hz, double pan, int32_t reps);

/* A copy of the built-in patch without its effects: the voices alone. */
Patch dry_patch(const char *name);

/* A sink that keeps every frame in memory. */
typedef struct {
    Sink    sink;
    double *samples;    /* interleaved left/right */
    int64_t frames;
    int64_t capacity;
} Capture;

void capture_init(Capture *capture);
void capture_free(Capture *capture);

#endif
