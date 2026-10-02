/* voice.h -- the voice: the part of the synthesizer that plays one slot.
 *
 * For each strike a voice computes, per sample,
 *
 *   wave(phase) * loudness law(frequency) * level * strike gain * envelope(u) * pan
 *
 * where u is the strike's age in slots. Vibrato modulates the phase step.
 * A voice advances its phase on every sample of every strike of a tone with
 * level above 0, the zero-gain strike included, and holds it while its slot
 * holds a tracked tone or no tone.
 */
#ifndef CHIMES_VOICE_H
#define CHIMES_VOICE_H

#include <stddef.h>
#include <stdint.h>

#include "score/tone.h"
#include "synth/patch.h"
#include "synth/shapes.h"

/* A patch with its names looked up, plus the two render settings a voice
 * needs. Shared by all voices; never changes during a render. */
typedef struct {
    const Patch *patch;
    WaveFn       wave;
    EnvelopeFn   envelope;
    LoudnessFn   loudness;
    StrikeGainFn strike_gain;
    PanFn        pan;
    int          slots;         /* slots per cycle, from the score */
    double       sample_rate;
} VoiceSetup;

/* Looks up every shape the patch names. Returns 0, or -1 with a message in
 * err that says which name is unknown. */
int voice_setup(VoiceSetup *setup, const Patch *patch, int slots, double sample_rate,
                char *err, size_t errlen);

typedef struct {
    const Tone *tone;            /* the tone now holding the slot, or NULL             */
    int         strike;          /* which of its strikes is sounding: 0 .. reps        */
    int64_t     strike_tick;     /* the tick at which that strike began                */
    double      phase;           /* radians, kept within 0 .. 2 pi                     */
    double      phase_step;      /* phase per frame without vibrato                    */
    double      vibrato_frames;  /* this tone's vibrato period, in frames              */
    double      vibrato_count;   /* frames played since the tone was created           */
    double      left, right;     /* loudness * level * strike gain, split by the pan   */
} Voice;

/* An empty slot at phase 0, as at the beginning of the piece. */
void voice_init(Voice *voice);

/* The next two are called when the voice's slot comes round, at a tick.
 * frames_late says how far the frame on which that tick starts lies after
 * the tick's exact time, in frames (-0.5 .. 0.5; 0 when the tick falls
 * exactly on a frame). The phase follows the exact time, so that a piece
 * sounds the same at any sample rate. */

/* A new tone takes the slot, and its strike 0 begins (at the tone's tick). */
void voice_start_tone(Voice *voice, const VoiceSetup *setup, const Tone *tone, double frames_late);

/* The slot comes round again without a new tone: the next strike of its tone
 * begins at this tick. A tone that has had all its strikes leaves the slot
 * empty. */
void voice_next_strike(Voice *voice, const VoiceSetup *setup, int64_t tick, double frames_late);

/* Adds this voice's sound to n_frames interleaved left/right frames. They
 * lie in the slot that begins at `tick` and is slot_frames long, and start
 * `first` frames into it. */
void voice_add(Voice *voice, const VoiceSetup *setup, int64_t tick, int64_t first,
               int64_t slot_frames, double *frames, int n_frames);

#endif
