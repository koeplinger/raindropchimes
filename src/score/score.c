/* score.c -- building, searching and checking a score in memory. */
#include "score/tone.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void score_init(Score *score)
{
    memset(score, 0, sizeof *score);
}

void score_free(Score *score)
{
    free(score->tones);
    memset(score, 0, sizeof *score);
}

int score_add(Score *score, const Tone *tone)
{
    if (score->n_tones == score->capacity) {
        int32_t capacity = score->capacity ? score->capacity * 2 : 1024;
        Tone *tones = realloc(score->tones, (size_t) capacity * sizeof *tones);
        if (!tones) return -1;
        score->tones = tones;
        score->capacity = capacity;
    }
    score->tones[score->n_tones++] = *tone;
    return 0;
}

const Tone *score_tone_at(const Score *score, int32_t tick)
{
    int32_t lo = 0, hi = score->n_tones - 1;
    while (lo <= hi) {
        int32_t mid = lo + (hi - lo) / 2;
        if (score->tones[mid].tick == tick) return &score->tones[mid];
        if (score->tones[mid].tick < tick) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

int32_t score_tone_end(const Score *score, const Tone *tone)
{
    return tone->tick + score->slots * (tone->reps + 1);
}

void score_set_render_note(Score *score, const char *key, const char *value)
{
    RenderNote *note = NULL;
    int32_t i;
    for (i = 0; i < score->n_render; i++)
        if (strcmp(score->render[i].key, key) == 0) note = &score->render[i];
    if (!note) {
        if (score->n_render == SCORE_RENDER_NOTES) return;
        note = &score->render[score->n_render++];
        snprintf(note->key, sizeof note->key, "%s", key);
    }
    snprintf(note->value, sizeof note->value, "%s", value);
}

const char *score_render_note(const Score *score, const char *key)
{
    int32_t i;
    for (i = 0; i < score->n_render; i++)
        if (strcmp(score->render[i].key, key) == 0) return score->render[i].value;
    return NULL;
}

void score_clear_render_notes(Score *score)
{
    score->n_render = 0;
}

int score_check(const Score *score, char *err, size_t errlen)
{
    int32_t *slot_free_at;   /* per slot: the tick from which it is free */
    int32_t i;
    int status = -1;

    if (score->slots < 1) {
        snprintf(err, errlen, "score has %d slots", (int) score->slots);
        return -1;
    }
    if (score->n_tones < 1) {
        snprintf(err, errlen, "score has no tones");
        return -1;
    }
    slot_free_at = calloc((size_t) score->slots, sizeof *slot_free_at);
    if (!slot_free_at) {
        snprintf(err, errlen, "out of memory");
        return -1;
    }

    for (i = 0; i < score->n_tones; i++) {
        const Tone *tone = &score->tones[i];
        int32_t slot;

        if (tone->tick < 0 || (i > 0 && tone->tick <= score->tones[i - 1].tick)) {
            snprintf(err, errlen, "tone %d: ticks must increase", (int) tone->tick);
            goto done;
        }
        if ((i == 0) != (tone->parent == -1)) {
            snprintf(err, errlen, "tone %d: exactly one root is allowed, and it must come first",
                     (int) tone->tick);
            goto done;
        }
        if (!(tone->freq_hz > 0.0) || !isfinite(tone->freq_hz)) {
            snprintf(err, errlen, "tone %d: frequency is not a positive number", (int) tone->tick);
            goto done;
        }
        if (i > 0) {
            const Tone *parent = score_tone_at(score, tone->parent);
            if (!parent || parent->tick >= tone->tick) {
                snprintf(err, errlen, "tone %d: its parent %d is not an earlier tone",
                         (int) tone->tick, (int) tone->parent);
                goto done;
            }
            if (tone->num < 1 || tone->den < 1) {
                snprintf(err, errlen, "tone %d: ratio %d/%d is not positive",
                         (int) tone->tick, (int) tone->num, (int) tone->den);
                goto done;
            }
            if (tone->freq_hz != parent->freq_hz * ((double) tone->num / (double) tone->den)) {
                snprintf(err, errlen, "tone %d: frequency is not parent * ratio", (int) tone->tick);
                goto done;
            }
        }
        if (tone->reps < 1) {
            snprintf(err, errlen, "tone %d: repetition count %d is below 1",
                     (int) tone->tick, (int) tone->reps);
            goto done;
        }
        if (!(tone->pan >= 0.0 && tone->pan <= 1.0)) {
            snprintf(err, errlen, "tone %d: pan is outside 0..1", (int) tone->tick);
            goto done;
        }
        if (!(tone->level >= 0.0 && tone->level <= 1.0)) {
            snprintf(err, errlen, "tone %d: level is outside 0..1", (int) tone->tick);
            goto done;
        }
        if (tone->jitter < -1) {
            snprintf(err, errlen, "tone %d: jitter is negative", (int) tone->tick);
            goto done;
        }
        slot = tone->tick % score->slots;
        if (tone->tick < slot_free_at[slot]) {
            snprintf(err, errlen, "tone %d: slot %d is still held by an earlier tone until tick %d",
                     (int) tone->tick, (int) slot, (int) slot_free_at[slot]);
            goto done;
        }
        slot_free_at[slot] = score_tone_end(score, tone);
    }
    status = 0;

done:
    free(slot_free_at);
    return status;
}
