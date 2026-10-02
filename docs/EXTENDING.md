# Extending Raindrop Chimes

How to add a wave form, an effect, a sound, a rule set, and the other things the design
leaves room for. Two additions are in the code as worked examples: the wave form `bell`
and the effect `soften`, used together by the patch `bell`. Try it:

```
./chimes make --seed 1297735820 --patch bell -o favourite_bell.wav    # the favourite tune through the bell sound
./chimes render samples/chimes_1297735820_1700_500.score --patch bell -o bell.wav
```

The words used here (tone, strike, slot, patch, rules, ...) are defined in section 4 of
[REVAMP_PLAN.md](REVAMP_PLAN.md). The parts of a sound (wave form, envelope, the
loudness, strike-gain and pan laws, which the code calls shapes, and effects) are
described in its section 5.3 and Appendix B.

## Two rules

1. **Run `make test` after every change.** It rebuilds the program and runs every check.
2. **A named rule set or patch is never changed once a tune has been made with it.** A
   score records the names `rules v4` and `patch v4`; if `v4` meant something else
   tomorrow, old scores and old seeds would silently give other tunes. To change a
   sound, copy its block under a new name and change the copy.

## Where things go

| To add | Open | And add |
|---|---|---|
| A wave form, an envelope, a loudness law, a strike-gain law, a pan law | `src/synth/shapes.c` | one function, and one line in that kind's list near the end of the file, under the heading "the lists" |
| An effect | a new file `src/synth/NAME.c`, and a second small file `NAME.h` that announces it to the rest of the program | four functions (create, process, longest delay, destroy); in `src/synth/effects.c` one `#include` line and one line in the list; and one `#include` line in `src/synth/patch.c` if a patch gives the effect settings |
| A sound (patch) | `src/synth/patch.c` | one block of constants in the list |
| A rule set for the slot algorithm | `src/compose/rules.c` | one block of constants in the list |
| A preset | `src/main.c` | one line in `PRESETS` |
| An output format | a new file `src/output/NAME.c` | three functions (open, write, close), one line in `src/output/formats.h` and one in the list in `src/output/sink.c` |

Nothing else has to change, and the `Makefile` picks up new files by itself.

## Worked example 1: a wave form

A wave form is a function that gives the value of the wave (between −1 and 1) at a
phase. The phase runs from 0 to 2π once per swing of the tone. The 2011 wave form is
the plain cosine. The bell adds three overtones, at 2, 3 and 5 times the tone's
frequency. This is the whole addition in `src/synth/shapes.c`:

```c
static double wave_bell(double phase, int highest)
{
    double value = cos(phase);

    if (highest >= 2) value += cos(2.0 * phase) / 2.0;
    if (highest >= 3) value += cos(3.0 * phase) / 3.0;
    if (highest >= 5) value += cos(5.0 * phase) / 5.0;
    return value / (1.0 + 1.0 / 2.0 + 1.0 / 3.0 + 1.0 / 5.0);
}
```

and one line in the list of wave forms near the end of the same file:

```c
static const struct { const char *name; WaveFn shape; } WAVES[] = {
    { "cosine", wave_cosine },
    { "bell",   wave_bell },
};
```

A patch can now say `.wave = "bell"`. Envelopes, loudness laws, strike-gain laws and pan
laws are added the same way; `shapes.h` says what each kind of function is given and
what it returns.

Two things to keep in mind for a wave form:

- Keep it between −1 and 1, and at 1 for phase 0, as the cosine is. Then no tone is
  louder at its peak than with the cosine. The bell divides by the sum of its four
  parts for that reason. (The sound as a whole may still come out quieter: the
  favourite tune peaks at 37.5 % of full scale through `bell`, at 50.9 % through `v4`.)
- `highest` is the highest whole multiple of the tone's frequency that still lies below
  half the sample rate. An overtone above half the sample rate cannot be carried: it
  would come out as another, wrong frequency (a tone of 8800 Hz at a sample rate of
  44100 would put its fifth multiple at 100 Hz). So a wave form adds an overtone only
  if `highest` allows it, as the three `if` lines of the bell do. For most tones every
  overtone fits; the bell's overtone at 5 times the frequency is left out for tones
  above about a tenth of the sample rate (4410 Hz at 44100). The plain cosine has no
  overtones and ignores `highest`. (A tone that itself lies at or above half the sample
  rate is not sounded at all; the voice sees to that, not the wave form.)

## Worked example 2: an effect

An effect works on the finished mix of all voices, a block of stereo frames at a time.
It provides four functions, which `src/synth/effects.h` describes:

| Function | What it does |
|---|---|
| create | builds the effect's memory from its settings and the sample rate |
| process | changes a block of frames in place |
| longest delay | says how long the effect holds on to sound, so that a piece is not cut off before its tail is over |
| destroy | frees the memory |

`soften` is the simplest useful effect: each output sample moves a share of the way from
the previous output to the new input. Slow waves are followed closely, fast ones are
smoothed away. The easiest start for an effect of your own is to copy `soften.c` and
`soften.h` under the new name and replace `soften`, `Soften` and `SOFTEN` throughout.

Its settings, in `src/synth/soften.h`:

```c
typedef struct {
    double cutoff_hz;    /* tones well below this frequency pass almost unchanged */
} SoftenConfig;

extern const Effect SOFTEN_EFFECT;
```

Its heart, in `src/synth/soften.c`:

```c
static void soften_process(void *state, double *frames, int n_frames)
{
    Soften *soften = state;
    int i;

    for (i = 0; i < n_frames; i++) {
        soften->left += soften->share * (frames[2 * i] - soften->left);
        soften->right += soften->share * (frames[2 * i + 1] - soften->right);
        frames[2 * i] = soften->left;
        frames[2 * i + 1] = soften->right;
    }
}
```

The rest of that file is the three other functions and, at the end, the effect's entry:

```c
const Effect SOFTEN_EFFECT = {
    "soften",
    soften_create,
    soften_process,
    soften_longest_delay_seconds,
    soften_destroy
};
```

Then two lines in `src/synth/effects.c`: `#include "synth/soften.h"` near the top, and
the effect's line in the list:

```c
static const Effect *const EFFECTS[] = {
    &REVERB_EFFECT,
    &SOFTEN_EFFECT,
};
```

An effect must never produce a value that is not a number: the program stops with a
message as soon as one appears, and writes nothing more. And it must not call a random
generator: `make test` refuses that.

## A sound that uses them

A patch names its shapes and lists its effects in the order the sound passes through
them. This is the whole patch `bell` in `src/synth/patch.c`: the v4 sound with the bell
wave form, softened before the reverb. It is three pieces. Near the top of the file,
one line that makes the settings of `soften` known there:

```c
#include "synth/soften.h"
```

then the settings, and the patch's block in the list:

```c
static const SoftenConfig BELL_SOFTEN = {
    .cutoff_hz = 4000.0
};
```

```c
    {
        /* The v4 sound with the bell wave form, softened before the reverb. */
        .name                   = "bell",
        .wave                   = "bell",
        .phase_restarts         = 0,
        .loudness               = "inverse_freq",
        .loudness_constant      = 600000.0 / 32768.0,
        .strike_gain            = "linear",
        .envelope               = "v4",
        .vibrato_depth          = 0.0005,
        .vibrato_period_seconds = SAMPLES(18000.0),
        .vibrato_spread         = 0.001,
        .pan                    = "linear",
        .n_effects              = 2,
        .effects                = { { "soften", &BELL_SOFTEN }, { "reverb", &V4_REVERB } },
    },
```

The fields are explained in `src/synth/patch.h`. A patch can list up to four effects.
A new sound never changes a tune: the same score renders through any patch.

## A rule set

A rule set is one block of constants in `src/compose/rules.c`: the ratio table, the
frequency range, the repetition table, and the numbers explained in
`src/compose/rules.h` and in section 5.2 of the plan. The blocks `v4`, `v6` and `v10`
show how little differs between the historical versions. Copy one, give it a new name,
change what you like, and try it:

```
./chimes make --rules NAME --seed 42
```

If you make a table longer or shorter, change its count as well (`n_ratios`, `n_reps`):
C does not count the entries for you, and an entry beyond the count is never used,
without a message. `first_reach` and `full_reach` say how many entries of the
repetition table a draw may reach; raise them too if new entries are to be drawn. A
table holds at most 32 entries.

Every rule set is checked when it is used. One that could misbehave is refused with a
message that names the field: a repetition table shorter than its draw can reach (the
kind of mistake that cut off the favourite tune in 2011), a repetition count below 1,
an empty frequency range.

A new rule set gives new tunes for old seeds; that is its point. The seeds of the
existing rule sets keep their tunes, because those blocks are not touched.

## What takes more than this

- **A new kind of modulation**, tremolo for example, is one more factor in the voice
  formula in `src/synth/voice.c` and one more field in `Patch` (`src/synth/patch.h`),
  filled in for every existing patch so that their sound stays as it is.
- **A different composing algorithm** is a new file in `src/compose/` with a function
  that fills a score, as `slotwalk_compose` does. `src/main.c` calls the one algorithm
  there is directly; with a second one it would need a way to choose. What a score
  requires of any algorithm is listed at the end of section 5.1 of the plan.
- **An output format** can be added as the table says, but the command line only
  chooses between WAV (a file) and raw frames (`-o -`); a third format would need an
  option to ask for it.
- **The checks.** `tests/check_45_patches.sh` holds the numbers of the built-in patches
  a second time, and `tests/check_13_reference_lists.sh` composes the eight traced
  tunes again. That is what notices an accidental change. A new patch or rule set does
  not need to be added there, but a check of its own is worth having;
  `tests/check_47_extension.sh` is the one for the two examples above.
