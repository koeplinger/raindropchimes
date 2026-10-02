/* render.c -- runs a score through voices and effects into an output. */
#include "synth/render.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "synth/effects.h"
#include "synth/voice.h"

#define BLOCK_FRAMES 1024               /* the piece is made in blocks of this many frames */
#define QUIET_LEVEL  (1.0 / 65536.0)    /* half a 16-bit step */

/* What comes after the voices: the patch's effects in order, then the output
 * gain, then the sink. */
typedef struct {
    int           n_effects;
    const Effect *effect[PATCH_MAX_EFFECTS];
    void         *state[PATCH_MAX_EFFECTS];
    double        sample_rate;
    double        gain;
    Sink         *sink;      /* NULL: measure only */
    int64_t       frames;    /* frames sent out so far */
    double        peak;      /* largest |sample| sent out so far */
} Chain;

static void chain_close(Chain *chain)
{
    int i;
    for (i = 0; i < chain->n_effects; i++) chain->effect[i]->destroy(chain->state[i]);
    chain->n_effects = 0;
}

/* Looks up and creates the patch's effects. */
static int chain_open(Chain *chain, const Patch *patch, double sample_rate, double gain,
                      Sink *sink, char *err, size_t errlen)
{
    int i;

    memset(chain, 0, sizeof *chain);
    chain->sample_rate = sample_rate;
    chain->gain = gain;
    chain->sink = sink;

    if (patch->n_effects < 0 || patch->n_effects > PATCH_MAX_EFFECTS) {
        snprintf(err, errlen, "patch %s: it lists %d effects; at most %d are allowed",
                 patch->name, patch->n_effects, PATCH_MAX_EFFECTS);
        return -1;
    }
    for (i = 0; i < patch->n_effects; i++) {
        const char *name = patch->effects[i].effect;
        const Effect *effect = effect_find(name);
        void *state;

        if (!effect) {
            snprintf(err, errlen, "patch %s: unknown effect \"%s\"", patch->name, name);
            chain_close(chain);
            return -1;
        }
        state = effect->create(patch->effects[i].config, sample_rate);
        if (!state) {
            snprintf(err, errlen, "patch %s: the effect \"%s\" could not be set up at %g Hz "
                     "(bad settings, or out of memory)", patch->name, name, sample_rate);
            chain_close(chain);
            return -1;
        }
        chain->effect[i] = effect;
        chain->state[i] = state;
        chain->n_effects = i + 1;
    }
    return 0;
}

/* The longest time any effect holds on to its input, in frames. */
static int64_t chain_longest_delay_frames(const Chain *chain)
{
    double longest = 0.0;
    int i;
    for (i = 0; i < chain->n_effects; i++) {
        double seconds = chain->effect[i]->longest_delay_seconds(chain->state[i]);
        if (seconds > longest) longest = seconds;
    }
    return (int64_t) llround(longest * chain->sample_rate);
}

/* Turns a block of mixed voices into the final signal, in place: effects,
 * then output gain. A sample that is not a finite number is an error. */
static int chain_process(Chain *chain, double *frames, int n_frames, char *err, size_t errlen)
{
    int i;

    for (i = 0; i < chain->n_effects; i++)
        chain->effect[i]->process(chain->state[i], frames, n_frames);

    for (i = 0; i < 2 * n_frames; i++) {
        frames[i] *= chain->gain;
        if (!isfinite(frames[i])) {
            snprintf(err, errlen, "the synthesizer produced a value that is not a number, "
                     "%.3f s into the piece; nothing more was written",
                     (double) (chain->frames + i / 2) / chain->sample_rate);
            return -1;
        }
    }
    return 0;
}

/* Sends a block of the final signal to the sink and keeps the peak. */
static int chain_send(Chain *chain, const double *frames, int n_frames, char *err, size_t errlen)
{
    int i;

    for (i = 0; i < 2 * n_frames; i++) {
        double size = fabs(frames[i]);
        if (size > chain->peak) chain->peak = size;
    }
    if (chain->sink && chain->sink->write(chain->sink, frames, n_frames) != 0) {
        snprintf(err, errlen, "could not write to the output");
        return -1;
    }
    chain->frames += n_frames;
    return 0;
}

/* The tick at which the last strike with non-zero gain ends: the end of the
 * music. Tracked tones and zero-gain strikes make no sound. */
static int64_t music_end_tick(const Score *score, const VoiceSetup *setup)
{
    int64_t end = 0;
    int32_t i;

    for (i = 0; i < score->n_tones; i++) {
        const Tone *tone = &score->tones[i];
        int strike;

        if (!(tone->level > 0.0)) continue;
        for (strike = tone->reps; strike >= 0; strike--) {
            if (setup->strike_gain(strike, tone->reps) != 0.0) {
                int64_t strike_end = (int64_t) tone->tick + (int64_t) score->slots * (strike + 1);
                if (strike_end > end) end = strike_end;
                break;
            }
        }
    }
    return end;
}

/* A tick's exact time need not fall on a frame. This says by what fraction
 * of a frame (-0.5 .. 0.5) the frame at which the tick starts is late; the
 * voices use it to keep their phase on the exact time. Less than a millionth
 * of a frame is rounding in the arithmetic, not an offset: the tick is on
 * the frame, as it always is at 44.1 kHz with the old tempo unit. */
static double tick_frames_late(const Timing *timing, int slots, int64_t tick, double sample_rate)
{
    double exact = timing_tick_seconds(timing, slots, tick) * sample_rate;
    double late = (double) timing_tick_frame(timing, slots, tick, sample_rate) - exact;
    return fabs(late) < 1.0e-6 ? 0.0 : late;
}

/* The music: every tick from 0 to the end of the last strike with non-zero
 * gain. At each tick one slot comes round; all voices sound until the next
 * tick. */
static int render_music(const Score *score, const Timing *timing, const VoiceSetup *setup,
                        Voice *voices, Chain *chain, char *err, size_t errlen)
{
    double  block[2 * BLOCK_FRAMES];
    int64_t end_tick = music_end_tick(score, setup);
    int64_t tick;
    int32_t next_tone = 0;   /* index of the next tone to be created */

    for (tick = 0; tick < end_tick; tick++) {
        Voice  *voice = &voices[tick % score->slots];
        int64_t slot_start = timing_tick_frame(timing, score->slots, tick, setup->sample_rate);
        int64_t slot_end = timing_tick_frame(timing, score->slots, tick + 1, setup->sample_rate);
        int64_t slot_frames = slot_end - slot_start;
        int64_t first;
        double  frames_late = tick_frames_late(timing, score->slots, tick, setup->sample_rate);

        /* Either a new tone takes this slot, or the tone in it is struck again. */
        if (next_tone < score->n_tones && score->tones[next_tone].tick == tick)
            voice_start_tone(voice, setup, &score->tones[next_tone++], frames_late);
        else
            voice_next_strike(voice, setup, tick, frames_late);

        for (first = 0; first < slot_frames; first += BLOCK_FRAMES) {
            int n = (int) (slot_frames - first < BLOCK_FRAMES ? slot_frames - first : BLOCK_FRAMES);
            int s;

            memset(block, 0, (size_t) n * 2 * sizeof block[0]);
            for (s = 0; s < score->slots; s++)
                voice_add(&voices[s], setup, tick, first, slot_frames, block, n);

            if (chain_process(chain, block, n, err, errlen) != 0) return -1;
            if (chain_send(chain, block, n, err, errlen) != 0) return -1;
        }
    }
    return 0;
}

/* The ring-out: silence goes through the effects until both channels of the
 * final signal have stayed below QUIET_LEVEL for longer than the longest
 * effect delay. The piece ends at the end of that quiet stretch. */
static int render_ring_out(Chain *chain, char *err, size_t errlen)
{
    double  block[2 * BLOCK_FRAMES];
    int64_t longest = chain_longest_delay_frames(chain);
    int64_t limit = (int64_t) llround(RENDER_RING_OUT_LIMIT * chain->sample_rate);
    int64_t fed = 0;      /* frames of silence fed in so far            */
    int64_t quiet = 0;    /* length of the quiet stretch so far         */
    int     done = 0;

    while (!done && fed < limit) {
        int n = (int) (limit - fed < BLOCK_FRAMES ? limit - fed : BLOCK_FRAMES);
        int i;

        memset(block, 0, (size_t) n * 2 * sizeof block[0]);
        if (chain_process(chain, block, n, err, errlen) != 0) return -1;

        for (i = 0; i < n && !done; i++) {
            if (fabs(block[2 * i]) < QUIET_LEVEL && fabs(block[2 * i + 1]) < QUIET_LEVEL) quiet++;
            else quiet = 0;
            if (quiet > longest) {
                n = i + 1;    /* the piece ends with this frame */
                done = 1;
            }
        }
        if (chain_send(chain, block, n, err, errlen) != 0) return -1;
        fed += n;
    }
    return 0;
}

/* Refuses settings that cannot be rendered, with a message. */
static int check_settings(const Score *score, const Timing *timing, double sample_rate,
                          double gain, char *err, size_t errlen)
{
    int32_t last_tick = 0, i;
    double  first_slot, last_slot;

    if (score_check(score, err, errlen) != 0) return -1;

    if (!(sample_rate >= 1.0) || !isfinite(sample_rate)) {
        snprintf(err, errlen, "the sample rate must be at least 1 Hz");
        return -1;
    }
    if (!isfinite(gain)) {
        snprintf(err, errlen, "the output gain is not a number");
        return -1;
    }

    /* With a negative drift the slots get shorter; none may shrink to nothing. */
    for (i = 0; i < score->n_tones; i++) {
        int32_t end = score_tone_end(score, &score->tones[i]);
        if (end > last_tick) last_tick = end;
    }
    first_slot = timing->slot_seconds;
    last_slot = timing->slot_seconds + (double) (last_tick / score->slots) * timing->drift_seconds;
    if (!(first_slot > 0.0) || !(last_slot > 0.0) || !isfinite(first_slot) || !isfinite(last_slot)) {
        snprintf(err, errlen, "timing: a slot must last longer than zero seconds in every cycle "
                 "(first cycle %g s, last cycle %g s)", first_slot, last_slot);
        return -1;
    }
    return 0;
}

int render_score(const Score *score, const Timing *timing, const Patch *patch,
                 double sample_rate, double gain, Sink *sink,
                 RenderStats *stats, char *err, size_t errlen)
{
    VoiceSetup setup;
    Chain  chain;
    Voice *voices;
    int    status = -1;
    int    s;

    if (check_settings(score, timing, sample_rate, gain, err, errlen) != 0) return -1;
    if (voice_setup(&setup, patch, score->slots, sample_rate, err, errlen) != 0) return -1;
    if (chain_open(&chain, patch, sample_rate, gain, sink, err, errlen) != 0) return -1;

    voices = malloc((size_t) score->slots * sizeof *voices);
    if (!voices) {
        snprintf(err, errlen, "out of memory");
        chain_close(&chain);
        return -1;
    }
    for (s = 0; s < score->slots; s++) voice_init(&voices[s]);

    if (render_music(score, timing, &setup, voices, &chain, err, errlen) == 0) {
        int64_t music_frames = chain.frames;

        if (render_ring_out(&chain, err, errlen) == 0) {
            if (stats) {
                stats->frames = chain.frames;
                stats->music_frames = music_frames;
                stats->peak = chain.peak;
            }
            status = 0;
        }
    }

    free(voices);
    chain_close(&chain);
    return status;
}
