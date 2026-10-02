/* voice.c -- oscillator and modulations for one voice. */
#include "synth/voice.h"

#include <math.h>
#include <stdio.h>

int voice_setup(VoiceSetup *setup, const Patch *patch, int slots, double sample_rate,
                char *err, size_t errlen)
{
    setup->patch = patch;
    setup->slots = slots;
    setup->sample_rate = sample_rate;

    setup->wave = wave_find(patch->wave);
    if (!setup->wave) {
        snprintf(err, errlen, "patch %s: unknown wave form \"%s\"", patch->name, patch->wave);
        return -1;
    }
    setup->envelope = envelope_find(patch->envelope);
    if (!setup->envelope) {
        snprintf(err, errlen, "patch %s: unknown envelope \"%s\"", patch->name, patch->envelope);
        return -1;
    }
    setup->loudness = loudness_find(patch->loudness);
    if (!setup->loudness) {
        snprintf(err, errlen, "patch %s: unknown loudness law \"%s\"", patch->name, patch->loudness);
        return -1;
    }
    setup->strike_gain = strike_gain_find(patch->strike_gain);
    if (!setup->strike_gain) {
        snprintf(err, errlen, "patch %s: unknown strike-gain law \"%s\"",
                 patch->name, patch->strike_gain);
        return -1;
    }
    setup->pan = pan_find(patch->pan);
    if (!setup->pan) {
        snprintf(err, errlen, "patch %s: unknown pan law \"%s\"", patch->name, patch->pan);
        return -1;
    }
    if (!(patch->vibrato_period_seconds > 0.0)) {
        snprintf(err, errlen, "patch %s: the vibrato period must be above zero", patch->name);
        return -1;
    }
    return 0;
}

void voice_init(Voice *voice)
{
    voice->tone = NULL;
    voice->strike = 0;
    voice->strike_tick = 0;
    voice->phase = 0.0;
    voice->phase_step = 0.0;
    voice->vibrato_frames = 1.0;
    voice->vibrato_count = 0.0;
    voice->left = 0.0;
    voice->right = 0.0;
}

/* Works out the gain of the strike that is beginning. */
static void begin_strike(Voice *voice, const VoiceSetup *setup, int strike, int64_t tick)
{
    const Tone *tone = voice->tone;
    double gain, left, right;

    voice->strike = strike;
    voice->strike_tick = tick;

    gain = setup->loudness(tone->freq_hz, setup->patch->loudness_constant)
         * tone->level
         * setup->strike_gain(strike, tone->reps);
    setup->pan(tone->pan, &left, &right);
    voice->left = gain * left;
    voice->right = gain * right;
}

/* How fast the phase is moving: its step per frame, or 0 while it holds. */
static double phase_speed(const Voice *voice)
{
    if (!voice->tone || !(voice->tone->level > 0.0)) return 0.0;
    return voice->phase_step;
}

/* The phase changes speed at ticks. A tick's exact time need not fall on a
 * frame (at 44.1 kHz with the old tempo unit it always does): the tick's
 * frame is a fraction of a frame late or early. This puts the phase where it
 * would be had the speed changed at the exact time. Without it, two tones a
 * fraction of a hertz apart would beat differently at each sample rate. */
static void change_speed_on_time(Voice *voice, double speed_before, double frames_late)
{
    voice->phase += (phase_speed(voice) - speed_before) * frames_late;
    if (voice->phase < 0.0) voice->phase += SHAPES_TWO_PI;
    if (voice->phase > SHAPES_TWO_PI) voice->phase -= SHAPES_TWO_PI;
}

void voice_start_tone(Voice *voice, const VoiceSetup *setup, const Tone *tone, double frames_late)
{
    const Patch *patch = setup->patch;
    int    jitter = tone->jitter < 0 ? 0 : tone->jitter;   /* the root has none */
    double speed_before = phase_speed(voice);

    voice->tone = tone;
    voice->phase_step = SHAPES_TWO_PI * tone->freq_hz / setup->sample_rate;
    voice->vibrato_frames = patch->vibrato_period_seconds * (1.0 + patch->vibrato_spread * jitter)
                          * setup->sample_rate;
    voice->vibrato_count = 0.0;
    if (patch->phase_restarts) {
        voice->phase = 0.0;
        speed_before = 0.0;
    }
    change_speed_on_time(voice, speed_before, frames_late);

    begin_strike(voice, setup, 0, tone->tick);
}

void voice_next_strike(Voice *voice, const VoiceSetup *setup, int64_t tick, double frames_late)
{
    if (!voice->tone) return;

    if (voice->strike < voice->tone->reps) {
        begin_strike(voice, setup, voice->strike + 1, tick);
    } else {
        double speed_before = phase_speed(voice);
        voice->tone = NULL;
        change_speed_on_time(voice, speed_before, frames_late);
    }
}

void voice_add(Voice *voice, const VoiceSetup *setup, int64_t tick, int64_t first,
               int64_t slot_frames, double *frames, int n_frames)
{
    double depth = setup->patch->vibrato_depth;
    double whole_slots, frame_in_slots;
    int silent, i;

    /* An empty slot or a tracked tone: no sound, and the phase holds. */
    if (!voice->tone || !(voice->tone->level > 0.0)) return;

    whole_slots = (double) (tick - voice->strike_tick);   /* since the strike began */
    frame_in_slots = 1.0 / (double) slot_frames;
    silent = (voice->left == 0.0 && voice->right == 0.0);

    for (i = 0; i < n_frames; i++) {
        double vibrato, u, value;

        /* The phase moves first; the sample is the wave at the new phase. */
        vibrato = sin(SHAPES_TWO_PI * voice->vibrato_count / voice->vibrato_frames);
        voice->phase += voice->phase_step * (1.0 + depth * vibrato);
        if (voice->phase > SHAPES_TWO_PI) voice->phase = fmod(voice->phase, SHAPES_TWO_PI);
        voice->vibrato_count += 1.0;

        /* The zero-gain strike only keeps the phase running. */
        if (silent) continue;

        u = whole_slots + (double) (first + i) * frame_in_slots;
        value = setup->wave(voice->phase) * setup->envelope(u, setup->slots);
        frames[2 * i] += value * voice->left;
        frames[2 * i + 1] += value * voice->right;
    }
}
