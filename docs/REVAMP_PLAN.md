# Raindrop Chimes: revamp plan

Status: implemented, 2026-10-02. All phases of [section 8](#8-phases) are built, and
`make test` passes. How to use the program is in [../README.md](../README.md); how to
extend it, in [EXTENDING.md](EXTENDING.md). This plan stays as the record of the
design: the exact rules and formulas, the reasons, and how the favourite tune's seed
was recovered. Passages that were corrected from what the implementation and the
tracing of the later versions showed are marked "(measured)" or "(as built)".

The goal is a clean, expandable codebase that keeps three things apart: **composition**
(the algorithm that decides which tones sound when), the **synthesizer** (wave form,
modulations, effects), and the **output file format**. The starting point is the
program as it was on 14 February 2011, because that version made your favourite tune
(your 4th tune, `legacy/examples/sndharmonics4_1700-400c.ogg`).

## 1. Where things stand

- **The new program is built**: `make` gives `./chimes`. The favourite tune, made with
  it, is in [samples/](../samples/) with its score.
- The old program and its examples are parked, untouched, in [legacy/](../legacy/).
- The seed of the favourite tune is recovered: **1297735820**
  (Monday 2011-02-14, 21:10:20 US Eastern). Details are in
  [section 10](#10-record-of-the-seed-recovery).
- The tone list of that tune, traced from the original code, is saved as
  [reference/tune4_seed1297735820.tones.txt](reference/tune4_seed1297735820.tones.txt).
  It is the main test for the new composer.
- A portable copy of the Linux random generator that the 2011 tunes depend on is saved
  as [reference/glibc_rand.c](reference/glibc_rand.c).
- How these files were made, and how to make them again, is in
  [reference/README.md](reference/README.md).
- **The written rules have been tested.** Two throwaway prototypes were built from
  this plan alone, without looking at the old code:
  - a composer built from Appendix A reproduced the reference tone list exactly, and
    matched the traced original on 369 further combinations of seed and length;
  - a synthesizer built from Appendix B, fed the reference tone list, lines up with your
    recording wave for wave (correlation 0.999, the same as the original code reaches;
    the remainder is the OGG compression).

This plan names four versions of the old program:

| Name | What it is |
|---|---|
| **v4** | The program as archived on 14 February 2011 (`sndharmonics4`). It made the favourite tune. |
| **v5** | As archived on 17 February 2011 (`sndharmonics5`). |
| **v6** | As archived on 18 February 2011 (`sndharmonics6`). |
| **v10** | The code now in `legacy/` (its README calls it v10; its banner says 4 March 2011). |

## 2. Decisions

Yours:

1. **Tones, not notes.** The music has no scale and no tuning. Every new frequency is the
   frequency of the tone in the previous slot times an exact small-integer ratio, so pitch
   is a lineage, not a position on a grid. A tone records its parent and its ratio.
   Nothing in the design uses pitch numbers or assumes a tone could be played on a piano.
2. **The composer must reproduce the old tone sequence.** With the v4 rules, the Linux
   generator, 500 cycles and seed 1297735820 it must produce the reference tone list.
   This is checked on tone lists, not on audio.
3. **The synthesizer has to sound right, not match sample for sample.** There is no
   "legacy-exact" render mode.
4. **End-of-tune bugs are not carried over.** The v4 code ends the favourite tune by
   accident: an out-of-range table read produces a 0/0, which silences the reverb at
   200.34 s. The new code lets the reverb ring out.
5. **Synth order is wave form, then modulation, then reverb.** The composer knows
   nothing about reverb.

Proposed (you called the language secondary):

6. **C (C99, standard library and maths library only).** It is portable and fast, the
   random-number arithmetic of the original carries over unchanged, and the old formulas
   transcribe one to one.

## 3. What the v4 program does

One function does everything, one audio sample at a time. Taken apart, it contains three
separate jobs.

### Composition

- Time is divided into **slots**; 11 slots make a **cycle**. Each slot holds one tone at a
  time.
- A tone is struck once per cycle in its slot. A tone with repetition count N is struck
  N+1 times, each time quieter: N/N, (N−1)/N, ... down to 0. Then the slot is free.
- When a slot is free, a new tone is drawn:
  - its frequency is the frequency of the tone currently in the previous slot, times one
    of 25 ratios (the families 5, 3, 1, 1/3 and 1/5, each shifted by powers of two),
    redrawn until it lies between 50 and 16000 Hz;
  - its pan (left/right position) is random;
  - its repetition count comes from a table of 11 numbers (1 and ten primes); the more
    tones are currently switched on, the further into the table the draw may reach;
  - whether it is switched on or only tracked silently is random, with a probability
    that follows a sin² curve over the whole piece (sparse, dense, sparse). A tracked
    tone still occupies its slot and still passes its frequency on.
- The very first tone gets a random absolute frequency (about 50.7 to 14,720 Hz). It is
  the only absolute frequency in the piece.
- In the final 30 cycles no new tones are meant to start.

Only this part uses random numbers. The tone sequence depends on the rules, the random
generator, the seed, and the number of cycles (which shapes the sin² curve). It does not
depend on tempo or on anything in the synthesizer. This was confirmed by rebuilding the
original at a different tempo: the tone list is unchanged.

### Synthesizer

- **Wave form:** a cosine per slot.
- **Modulations:** a slight vibrato (0.05 % of the frequency, 2.2 to 2.45 swings per
  second, with a slightly different rate per tone); loudness proportional to
  1/frequency; the repetition decay; an envelope per strike (quick attack over half a
  slot, slow fourth-root decay over the cycle, linear release in the last slot); linear
  panning.
- **Reverb:** five delayed copies of the output, between 0.16 and 0.43 seconds late, fed
  back into it; two of them swap left and right.

### Output

A 16-bit stereo WAV at 44.1 kHz, written sample by sample, with a header that declares
zero bytes of audio (the length is never filled in).

### Where the three are tangled

| Tangle in the v4 code | How the new design cuts it |
|---|---|
| The composer's count of switched-on tones is computed inside the audio loop. | The composer counts from its own slot table. Traced runs (193 of them) confirm the two counts are always equal. |
| The slot clock is the audio sample counter. | The composer counts **ticks** (slot boundaries). Seconds and samples exist only in the synthesizer. |
| Tempo (`binsPerSlot`) is also the envelope's time unit, and every delay is a sample count at 44.1 kHz. | Tempo is a render setting in seconds. Envelope times are fractions of a slot; reverb delays and vibrato periods are in seconds. |
| One random draw per tone sets the vibrato rate, a synthesizer matter, yet it is part of the composer's random sequence, even for tracked tones. | The composer draws an anonymous number per tone (**jitter**, 0..99) and hands it over. The v4 sound maps it to a vibrato period; other sounds may use or ignore it. |
| The repetition counter is both the composer's "slot is free" timer and the synthesizer's gain factor. | The tone carries its repetition count. Each side derives what it needs. |
| The piece ends when the composer's loop ends; the reverb is cut off. | The synthesizer decides when the sound has died away. |
| Samples above 16-bit range wrap around; a 0/0 becomes silence. | The output stage saturates and reports it; an invalid number is an error, never written. |

## 4. Vocabulary

| Term | Meaning |
|---|---|
| **tick** | The composer's only clock: a counter of slot boundaries, 0, 1, 2, ... It has no duration. |
| **slot** | `tick mod 11`. A recurring position in the cycle that holds one tone at a time. In the synthesizer, "a slot" also means the length of time from one tick to the next (as in "half a slot"). |
| **cycle** | `tick div 11`. One pass over all slots. |
| **tone** | What the composer creates at a tick: a frequency given by its lineage, plus pan, repetition count, level and jitter. Its identity is the tick it was created at. |
| **lineage** | A tone's parent (the tone occupying the previous slot at that moment) and the exact ratio to it, as numerator/denominator. |
| **root tone** | The first tone. It has no parent and carries the only absolute frequency. |
| **strike** | One sounding of a tone. A tone with repetition count N has strikes 0..N, one per cycle. |
| **zero-gain strike** | The last strike (strike N) of a tone. Its gain is 0, but the tone still holds its slot. |
| **level** | A tone's own volume, 0 to 1. The v4 and v5 rules only use 0 and 1. |
| **tracked tone** | A tone with level 0. It makes no sound, but holds its slot and passes its frequency on. |
| **counted tone** | A tone that counts towards "how many tones are switched on" (called `d` in Appendix A). v4 and v5: level 1. v6: level above 0. v10: level above 0.69. It counts on every one of its strikes, including the zero-gain one. |
| **pan** | Left/right position: 0 = left, 1 = right. |
| **jitter** | An anonymous random number 0..99 per tone, drawn by the composer for the synthesizer's use. |
| **density** | The sin² curve over the piece that steers how likely a new tone is to be switched on. |
| **end phase** | The final 30 cycles, in which no tones are created. |
| **score** | The full list of tones plus the settings the composer used. It is the whole composition. |
| **rules** | The tables and numbers that steer a composing algorithm. |
| **generator** | The random number generator. The new program has one: the Linux one (`glibc`), built in. |
| **voice** | The part of the synthesizer that plays one slot. |
| **patch** | The description of a sound: wave form, modulations, reverb. |
| **reverb** | Your word for the effect. Technically it is a feedback echo with a handful of delays. |
| **tap** | One delayed copy that the reverb feeds back. |
| **timing** | Tempo: seconds per slot, and how much that changes per cycle (the **drift**). |
| **preset** | A bundle of rules, patch, timing and cycles for one version (v4, v6, ...). Every preset uses the one built-in generator. |
| **frame** | One left sample plus one right sample. |
| **output gain** | One factor applied to the whole piece before it is written. 1 = the loudness the old program had. |
| **saturate** | Hold at the largest value instead of wrapping round. |
| **ring-out** | The reverb tail after the last strike. |

Your 2011 names and what they become:

| In the old code | In the new design |
|---|---|
| `slotFrequency` | a tone's `freq_hz` |
| `slotPlay` | a tone's `level` |
| `slotPos` | a tone's `pan` |
| `slotAmplitudeCnt` | a tone's `reps` |
| `slotAmplitudeCur` | strikes left (derived, on each side separately) |
| `vibratoBins` | computed by the patch from a tone's `jitter` |
| `densityCnt` | the count of counted tones, `d` |
| `density` | density |
| `prettyTone` | the ratio table (in the rules) |
| `prettyPrime` | the repetition table (in the rules) |
| `binsPerSlot` | `--bins` (tempo) |
| `totalSongTime` | cycles |
| `reverbBins`, `reverbStrength`, `reverbType` | a tap's delay, gain, and swap flag (in the patch) |

## 5. Design

```
   rules + generator + seed + cycles          timing + patch + sample rate
                 |                                         |
                 v                                         v
          +-------------+        score         +----------------------+     frames     +--------+
          |  composer   | -------------------> |     synthesizer      | -------------> | output |
          +-------------+   (in memory, or     | wave -> modulation   |   (stereo,     +--------+
                             as a text file)   |      -> reverb       |    floating       WAV,
                                               +----------------------+    point)         raw, ...
```

There are three seams: generator to composer, composer to synthesizer (the score), and
synthesizer to output. Everything else is plain functions and data tables.

### 5.1 The score

The score is the contract between composer and synthesizer. It exists in memory as a
record (a C struct) and on disk as a text file that prints that record.

```
# raindropchimes score 1
@ rules v4
@ generator glibc
@ seed 1297735820
@ cycles 500
@ slots 11
# tick cycle slot parent ratio freq_hz pan reps level jitter
0 0 0 - - 1299.5451597662952 0.26 13 1 -
1 0 1 0 5/16 406.10786242696724 0.70 3 0 49
2 0 2 1 1/2 203.05393121348362 0.28 3 1 57
```

```c
typedef struct {
    int32_t tick;      /* creation tick = the tone's identity                     */
    int32_t parent;    /* tick of the tone it derives from; -1 for the root       */
    int32_t num, den;  /* exact ratio to the parent (0, 0 for the root)           */
    double  freq_hz;   /* root: drawn. Others: parent's freq_hz * (num / den)     */
    double  pan;       /* 0 = left .. 1 = right                                   */
    int32_t reps;      /* N >= 1: strikes 0..N, one per cycle                     */
    double  level;     /* 0 = tracked, else up to 1                               */
    int32_t jitter;    /* 0..99, or -1 for none (root)                            */
} Tone;
```

The file format:

- Lines starting with `#` are comments: they are never part of the composition. The
  reader skips them, with two exceptions. The first line (`# raindropchimes score 1`)
  names the format version and is checked. Lines of the form `# render: key value` are
  remembered, so that `chimes render` can use the patch, bins and drift recorded there
  as its default settings (section 6).
- Lines starting with `@` hold what the composer used, one `@ key value` per line:
  rules, generator, seed, cycles, slots.
- Every other line is one tone. Fields are separated by one space. `tick`, `cycle`,
  `slot`, `parent`, `reps` and `jitter` are whole numbers. `ratio` is written `num/den`
  as it stands in the rule table. `freq_hz` is written with 17 significant digits (C
  format `%.17g`), `pan` with two decimals, `level` with the fewest digits that read back as the same
  number (so 0, 1 and 0.7 print as written). The
  root has `-` for parent, ratio and jitter.
- When audio is made from a score, the render settings (bins, drift, patch, sample
  rate, output gain, and a complete command) are written as `# render:` lines,
  replacing any that were there. They are a record for you, not part of the
  composition.
- Two scores are the same when their `@` lines and tone lines are the same. The
  reference file was written before this format was settled, so it has only comments
  and tone lines; checks against it compare tone lines.

What the score means:

- **Lineage is the truth.** `freq_hz` is a convenience: the composer needs it for its
  range test and the synthesizer needs it to make sound. When a score is read from a
  file, only the root's `freq_hz` is trusted; the rest are recomputed down the chain
  (this reproduces every frequency in the reference list bit for bit).
  (As built.) The reader also compares every printed frequency with the recomputed one
  and refuses the file if they differ by more than one part in a billion. A root
  frequency changed by hand is therefore refused, not followed; transposing a piece
  belongs to the later work on editing scores (section 13, answer 6).
- **Tracked tones are in the score**, because later tones descend from them. The
  synthesizer skips them.
- **Strikes are not listed.** The tone carries `reps`; the synthesizer expands the
  strikes, and how loud each strike is belongs to the patch.
- **The score has no tempo, no sample rate, and nothing about the sound.** Tempo is
  given to the synthesizer. The same score can be rendered slow or fast, at any sample
  rate, through any patch.

What the score format assumes of any composing algorithm: at most one tone is created
per tick; a tone's parent is an earlier tone; a tone sounds once per cycle in slot
`tick mod slots`, `reps + 1` times, and so holds its slot for ticks
`tick .. tick + slots·(reps+1) − 1`; two tones may not hold the same slot at the same
time. An algorithm that needs a different time structure needs a new score version.

### 5.2 Composition

- A **composing algorithm** is a function that takes rules, a generator, and a number of
  cycles, and fills a score. There is one to begin with: the slot algorithm of v4. A
  genuinely different algorithm later is a new file with a new function that fills the
  same score.
- **Rules are data.** A rule set for the slot algorithm has these fields (v4 values):

  | Field | v4 value |
  |---|---|
  | slots | 11 |
  | end-phase cycles | 30 |
  | ratio table (its order matters) | the 25 ratios of Appendix A |
  | frequency range for new tones | 50 .. 16000 Hz |
  | repetition table | 1, 2, 3, 5, 7, 11, 13, 17, 23, 29, 31 |
  | first tone: how far into the repetition table its draw may reach | all 11 |
  | later tones: how far the draw may reach | `d + 2` entries while `d < 10`, else all 11 (the 10 and 11 belong to the table's length, not to the slot count) |
  | density gain g, in density = min(1, g·sin(π·c/(T−31)))² (c = the current cycle, T = the number of cycles) | 1 (v10: 2.5) |
  | level rule | on/off, with scale 150 and offset 0.05 |
  | a tone is counted if | its level is 1 |
  | first tone's level; level of a new tone when nothing is counted | 1; 1 |
  | jitter range | 0..99 |

  The first tone's frequency law is part of the algorithm, not a field. The historical
  versions differ only in such fields, plus a choice between two level rules (see
  [section 9](#9-the-historical-versions-as-rule-sets-and-patches)).
- **Rule sets are checked** (as built: whenever one is used, and `make test` checks
  every built-in one): every repetition count is at least
  1; the repetition draw can never reach past the end of its table; the frequency range
  is not empty. The ratio redraw gives up with an error after 10,000 tries. A rule set
  that fails stops the program with a message naming the field. (A table shorter than
  its draw can reach is exactly the kind of out-of-range read that ended the favourite
  tune.)
- **A named rule set or patch is never changed once a tune has been made with it.** A
  changed one gets a new name. Otherwise old seeds would silently give new tunes.
- **The generator is an explicit component** handed to the composer: the Linux
  generator (`glibc`), built into the program from `reference/glibc_rand.c`. The
  system's own `rand()` is never called, so a seed gives the same tune on any computer.
  The Apple generator, which made the 2022 tunes, is not carried forward; its rule is
  kept in `reference/README.md` as a record only.
- **End phase.** In the final 30 cycles the composer creates no tones at all. Tones
  already sounding finish their strikes. This is what the v4 code intended, it is what
  the favourite tune audibly does (its last audible tone starts in cycle 468), and it
  removes the out-of-range read and the 0/0 by construction.
- **Cycles must be at least 32.** Smaller values are refused with a message (with 31
  the density formula divides by zero).

**What "same seed, same tune" means, precisely.** For a rule set, a generator, a seed
and a number of cycles T, every tone created before tick 11·(T−30) is the one the
original program of that version created.

- Exactly equal: tick, parent, ratio, pan, repetition count, jitter, and on/off level.
- Equal to 12 digits on any computer, and to all 17 on Linux: frequencies, and the
  continuous levels of v6 and v10. These pass through the maths library's `exp` and
  `sin`, which can differ in the last digit between systems. For the v4 rules such a
  difference is far too small to change which tones are created (margins in
  section 12); the same holds for v10's `> 0.69` test (measured: section 12).
- Tempo, sample rate and patch cannot affect any of this, because the composer never
  sees them.
- The number of cycles does affect it: T is part of a tune's identity, together with
  rules, generator and seed. (For the favourite, 499 cycles instead of 500 gives a
  different piece from tick 4508 on; 400 instead of 500, from tick 138 on.)

### 5.3 Synthesizer

The synthesizer takes a score, a timing, a patch and a sample rate (default 44100), and
produces stereo frames in floating point, where 1.0 is full scale.

- **Timing.** `slot_seconds` and `drift_seconds`: in cycle c a slot lasts
  `slot_seconds + c·drift_seconds`. The v4 tempo is `1701/44100` s per slot (the old
  `binsPerSlot = 1700` plus one, because a slot was really 1701 samples). The slowdown
  of v6 is a drift of `1/44100` s per cycle. Tick n starts at sample
  `round(start time of tick n × sample rate)`; slot lengths are never rounded one by
  one, so every tick falls at the same time at any sample rate. At 44100 with the v4
  tempo, tick n starts at sample 1701·n, as in 2011.
- **Voices.** One voice per slot. For each strike the voice computes, per sample,
  `wave(phase) × loudness law(frequency) × level × strike gain(j, N) × envelope(u) × pan`.
  - (As built.) A wave form is also told the highest multiple of the tone's frequency
    that the sample rate can carry, so that one with overtones can leave out those
    that would come out as wrong frequencies. The cosine has none and ignores it. A
    tone that itself lies at or above half the sample rate is not sounded; at 44100 Hz
    no tone of the built-in rule sets lies that high.
  - `u` is the strike's age in slots: whole slots elapsed since the strike began, plus
    the fraction of the current slot elapsed. Every strike is therefore exactly 11
    units long, whatever the tempo and drift.
  - Vibrato modulates the phase increment.
  - A voice advances its phase on every sample of every strike of a tone with level
    above 0, including the zero-gain strike, and holds it while its slot holds a tracked
    tone or no tone.
  - In the v4 sound the phase runs on from one tone to the next, as the old code
    happened to do. You cannot hear this; it is kept because it costs nothing and lets
    the new render be compared with your recording wave for wave (verification item 6).
    It is a property of that patch, not a requirement: a later sound may start each
    tone at phase zero.
  - (As built.) When a slot's tick does not fall exactly on a sample, the voice shifts
    its phase by the fraction of a sample in between. At 44.1 kHz with whole-sample
    tempos this does nothing. At other sample rates it keeps tones that are almost in
    unison beating against each other the same way as at 44.1 kHz.
- **A patch is data.** Its fields: wave form; phase at a new tone (runs on, as in v4, or
  restarts at zero); loudness law and its constant; strike-gain law; envelope; vibrato depth, base period and spread; pan law; the list of effects
  with their settings (for the reverb: each tap's delay in seconds, gain, and swap
  flag).
- **Effects.** A chain that processes the mixed stereo signal. The reverb is one
  effect: `out = in + Σ gainᵢ × out(delayed by dᵢ)`, with some taps reading the other
  channel.
- **Output gain** is applied here, after the effects, before frames go to the output.
- **Ring-out.** After the last strike with non-zero gain has ended, the synthesizer
  keeps feeding silence through the effects until both channels of the final signal
  have stayed below half a 16-bit step (1/65536 of full scale) for longer than the
  longest reverb delay. The piece ends at the end of that quiet stretch. Upper limit:
  60 s. For the favourite tune the music ends at 199.4 s and the reverb takes about
  8.5 s to die away, so the new file is 207.9 s long. (The 2011 file was
  212.1 s, with the cut to silence at 200.34 s.)

### 5.4 Output

- **The interface** is three functions: open, write a block of stereo frames, close. A
  format is one file that implements them. The output only converts and writes.
- **First format:** 16-bit stereo WAV with correct header lengths. Second: raw 32-bit
  floating-point frames to standard output, so that `ffmpeg` can make OGG or FLAC
  without any codec library in this project. (As built: `oggenc` cannot read these
  frames, it takes whole-number samples only. It can encode the finished WAV file:
  `oggenc file.wav`.)
- **Conversion to 16 bit** is `round(x × 32768)`, saturated to −32768..32767. The output
  counts saturated samples and reports them. A value that is not a number stops the
  program with an error.
- **Default output gain: 1**, the loudness of the old program (the favourite tune peaks at 51 % of full
  scale). If a tune would exceed full scale it is turned down just enough, and the
  program says so. This needs the peak before writing; a render pass takes about two
  seconds, so the synthesizer simply renders twice. (As built: also when `--gain` is
  given, so that a render that cannot work shows before any file is touched.)

### 5.5 Files

```
src/
  score/     tone.h          Tone and Score
             score.c         building, searching and checking a score in memory
             score_text.c    write and read a score file
  compose/   rng.c           the Linux generator (glibc)
             rules.c         rule sets as data, and their check
             slotwalk.c      the slot algorithm
  synth/     timing.c        ticks to seconds
             voice.c         oscillator and modulations for one voice
             shapes.c        wave forms, envelopes, loudness / strike-gain / pan laws, by name
             reverb.c        the reverb
             soften.c        a second effect, the worked example of EXTENDING.md
             effects.c       the list of effects (effects.h: what an effect must provide)
             patch.c         patches as data
             render.c        runs a score through voices and effects into an output
  output/    sink.c          the list of formats (sink.h: what a format must provide)
             wav.c  raw.c
  main.c                     command line and presets
tests/       the checks of section 7: run.sh runs every check_*.sh
docs/        this plan; EXTENDING.md; reference/
samples/     tunes made with the new program, with their scores (CC BY 4.0)
legacy/      the old program and all examples, untouched
LICENSE      MIT, for the code
Makefile
```

Three rules keep the parts apart, checked automatically by `make test`:

- `compose/` includes nothing from `synth/` or `output/`. (A C file includes another
  when it uses what that file defines.)
- `synth/` and `output/` include nothing from `compose/` and never touch a generator.
- `output/` sees only frames.

`make test` runs every check of section 7 and some more, 25 in all. It needs a C
compiler and the usual shell tools. Three checks need more and are skipped where that
is missing: the two likeness checks need `ffmpeg`; the comparison with the rebuilt 2011
programs needs `tar`, `patch`, `python3` and a Linux computer.

Each `.c` file has a `.h` file beside it that says what it offers to the others;
`compose/compose.h` announces the composing algorithm and `output/formats.h` the
formats. `README.md` at the top says how to build and use the program.

### 5.6 How to extend it

| To add | Do this |
|---|---|
| A rule set for the slot algorithm (new ratio table, new repetition table, other numbers) | Add one block of constants to `rules.c`. |
| A different composing algorithm | Add a file in `compose/` with a function that fills a score. (As built: there is no list of algorithms yet; `main.c` calls the one there is directly.) |
| Another generator | Add two functions (seed, next) to `rng.c` and a way to choose it. Not planned: the built-in Linux generator is the only one. |
| A wave form, envelope, loudness law, strike-gain law or pan law | Add one function to `shapes.c` and one line in its list. |
| A new kind of modulation (say, tremolo) | Add one factor in `voice.c` and one field in the patch. |
| An effect | Add a file that provides create / process / longest-delay / destroy, and one line in the list of effects. Patches list their effects in order. |
| A sound | Add one block of constants to `patch.c`. |
| An output format | Add a file in `output/` that provides open / write / close, one line in `formats.h` and one in the list of formats. (As built: the command line chooses only between WAV and raw frames.) |

[EXTENDING.md](EXTENDING.md) shows the common ones step by step (wave form, effect,
sound, rule set), with two worked examples that are in the code, the wave form `bell`
and the effect `soften`, and says what the others take.

The lists are plain name-to-function tables in the file they belong to. There is no
plugin system and no configuration language; the four historical versions differ in
about 25 numbers and two short branches, and that is the amount of flexibility the
design pays for.

## 6. Command line

```
chimes make --preset v4 --seed 1297735820       # the favourite tune
chimes make                                     # a new tune; the seed is the clock, and is printed
chimes make --seed 42 --bins 3401               # the same tune as seed 42, at half speed
chimes make --seed 42 --cycles 300              # a different tune (cycles shapes the piece); warns
chimes score --preset v4 --seed 42              # the score only, no audio
chimes render my.score --patch v6 -o my.wav     # any score through any patch
```

- Without `--preset`, the v4 preset is used: a plain `chimes make` composes with the
  14 February rules and sound.
- Each part of a preset can be overridden: `--rules`, `--patch`, `--bins`, `--drift`,
  `--cycles`. `--rate`, `--gain` and `-o` are set apart from any preset.
- `--bins B` is the old tempo unit, so numbers from old file names still work: a slot
  lasts (B+1)/44100 seconds. `--drift D` is in the same unit: D samples at 44.1 kHz
  added to the slot length per cycle (v6: 1). `--gain` is the output gain, a factor.
- Without `-o`, files are named the way you named them by hand:
  `chimes_<seed>_<bins>_<cycles>.wav`, with the score beside it as
  `chimes_<seed>_<bins>_<cycles>.score`.
- **The record of a tune is its score file**, written next to every audio file. Its `@`
  lines say how it was composed and its `# render:` lines how it was rendered,
  including a complete command that recreates the file: the seed and every setting are
  written out, even if you typed only `chimes make`. File names are a convenience, no
  longer the only record.
- `chimes render` takes patch, tempo and drift from the command line. What is not given
  there comes from the score's `# render:` lines, or, if the score has none or
  `--preset` is given, from the preset (`--preset`, else the default `v4`). Sample rate
  and output gain are not taken from the score: without `--rate` and `--gain` they are
  44100 and the automatic gain.
- `-o -` writes raw floating-point frames to standard output instead of a WAV file
  (phase 6), for piping into `ffmpeg`. The score is then written under its default
  name.
- (As built.) Further details of the command line:
  - `--drift` may be negative: the piece then speeds up.
  - The recorded command names the rule set and the patch (`--rules`, `--patch`)
    rather than a preset, so that it stays valid if a preset's defaults ever change.
    Paths in it are quoted where a shell needs it. A command too long for one line of
    the score (511 characters) is left out, with a message; the settings are still
    recorded.
  - A render that cannot work is refused before any file is touched: settings out of
    range, a `# render:` line in a score that cannot be used, an audio file that would
    land on its own score, a name ending in `.ogg`, `.mp3` or the like (the program
    writes WAV only), a score file that may not be written, or a piece too long for a
    WAV file (about 6.8 hours at 44.1 kHz).
  - The score is written beside its place first and moved there when it is complete,
    so that a score that is already there is never left half written. If the audio
    cannot be written to the end (a full disk), the audio file is left as it is, the
    program says that it is incomplete, and no score is written for it.
  - `chimes score` composes only: it refuses the options that belong to rendering,
    as `chimes render` refuses those that belong to composing.
  - `chimes help`, or `--help` after any command, prints the options, with the names
    of the presets, rule sets and patches there are.

## 7. Verification

In order of importance.

1. **The favourite tune's tone list.** `chimes score --preset v4 --seed 1297735820`
   must equal the reference on all 625 tone lines, with no tone after tick 5169. This
   proves the generator, the order of the random draws, the count of counted tones and
   the lineage in one go; the favourite exercises every branch of the rules. On failure
   it shows the first differing line. On a system other than Linux, a difference only
   in the last digits of the frequency column passes, with a remark.
2. **Generator known answers.** With seed 1 the generator gives 1804289383, 846930886.
   This separates generator bugs from rule bugs.
3. **Separation.** The tone lines are identical under different tempo, sample rate and
   patch, and different under a different number of cycles. The include rules of
   section 5.5 hold.
4. **Score integrity.** Writing, reading and writing again gives the same file. Every
   tone's parent is an earlier tone; every frequency equals parent × ratio; every
   repetition count is at least 1; ticks increase; there is exactly one root; no two
   tones hold the same slot at the same time.
5. **Render sanity for the favourite tune.** Every sample is a valid number; nothing
   saturates; the peak is 51 % of full scale; there is reverb tail after 200.34 s; the
   final stretch of the file (one longest reverb delay) is digital silence; the WAV
   header is correct; two runs give identical files.
6. **Likeness to the 2011 recording** (needs `ffmpeg` to decode the OGG). With the v4
   patch, the new render is compared with your recording over cycles 0..469, per
   channel. Pass mark: the two line up with no time shift; overall correlation at least
   0.999; every single cycle at least 0.99. The prototype built from this plan reaches
   0.99909 (left) and 0.99921 (right), with the worst cycle at 0.9935, which is also
   what the original code reaches. This is a check of the v4 patch, not a render mode.
   Your ears on the first half minute remain the real test.
   (Measured.) The built program reaches these same numbers. The v5 and v6 sounds are
   checked the same way against their own recordings, v6 with an overall pass mark of
   0.9985 instead of 0.999: 0.9991 to 0.9994 for the two v5
   tunes and 0.9989 to 0.9993 for the two v6 tunes, each exactly what the original
   program of that version reaches. For those four the per-cycle mark leaves out the
   cycles in which the recording is nearly silent, where the noise of the OGG
   compression is as large as the music.
7. **Synthesizer unit checks.** The reverb's response to a single click has the right
   delays, gains and channel swaps. A single tone has the right frequency, pan and
   loudness. Renders at 44.1 and 48 kHz start every tick at the same time (to within
   one sample) and end within one second of each other. Without reverb their loudness
   per cycle agrees to within 1 % (measured: 0.04 %). With reverb it agrees to within
   1 % over the whole piece and 10 % in any one cycle (measured: 0.1 % and 6 %), because
   the reverb delays round to whole samples and its resonances shift slightly.
8. **Composer properties over 1,000 seeds.** Every parent held the previous slot when
   its child was created; frequencies stay in range (the first tone excepted: under the
   v10 rules it may lie outside); ratios come from the table; no tone is created in the
   end phase; the composer always finishes. Every built-in rule set passes the rule-set
   check of section 5.2, and a deliberately broken one is refused.
9. **Later versions.** Reference tone lists for v5, v6 and v10, traced from the old
   sources with the Linux generator, each checked like item 1.

## 8. Phases

Each phase ends with something that can be checked.

| Phase | Work | Check |
|---|---|---|
| 0. Foundations | Makefile with `make test`; `rng.c` with the Linux generator, from `reference/glibc_rand.c`; the `LICENSE` file (MIT). | Item 2. |
| 1. Composer | `tone.h`, the v4 rule set with its check, `slotwalk.c`, the score writer, `chimes score --seed N` (v4 is the only preset until phase 4). | Items 1 and 8. The tune exists as data. |
| 2. Dry synthesizer | Timing, voices with the v4 modulations, WAV output at output gain 1. `chimes make --seed N` with the v4 preset fixed. | The file plays. With seed 1297735820 the first tone measures 1299.5 Hz at pan 0.26 (item 7, single tone). |
| 3. Reverb and ring-out | The reverb, ring-out. The peak is printed. The favourite tune goes into `samples/` as the first sample, with its score. | Items 5, 6 and 7 (the 48 kHz part of item 7 is run by the test program directly; `--rate` comes in phase 4), and you listen. **Phases 0–3 are the minimum that gives you your tune back.** |
| 4. Everyday use | Clock seed; overrides; presets; the score saved next to the audio; the score reader and `chimes render`; automatic turn-down; the top-level README updated with how to build, run, and recreate a tune. | Item 3 for tempo, sample rate and cycles (its patch part waits for the v6 patch in phase 5), and item 4. A score written, read and rendered gives the same audio as a direct render (at the default sample rate and output gain). The command recorded in a score reproduces the same audio file. |
| 5. Later versions, one at a time | For each of v5, v6 and v10: add lines to a copy of its source that log every new tone (keep these changes as a diff file next to `v4_trace.patch`); run it with the Linux generator and the seed, tempo and cycles from an example's file name; for v5 and v6, confirm the traced program's own audio matches that example (v10 has no Linux recording to compare with); convert and commit the tone list. Then add the rule set and the sound patch: v5 is data only; v6 brings the other level rule, its envelope, seven reverb taps and drift; v10 brings its numbers. | Item 3, patch part. Item 9. Item 6 for v5 and v6 against their own recordings. |
| 6. Extension, proven | Raw output for piping into an encoder; one new wave form and one new effect, written up as worked examples of section 5.6. | The worked examples build and play, and the include rules still hold. |

(As built, 2026-10-02.) All seven phases are done. The first sample was made after
phase 6 rather than phase 3, as an OGG file through `ffmpeg`. The worked examples of
phase 6 are the wave form `bell`, the effect `soften` and the patch `bell`.

## 9. The historical versions as rule sets and patches

Rules (composition):

| | v4 and v5 | v6 | v10 |
|---|---|---|---|
| First tone's frequency | exp law, about 50.7 .. 14,720 Hz | same | same (so it may lie outside 100 .. 3000) |
| Ratio table | 25 ratios | same | same |
| Frequency range for new tones | 50 .. 16000 Hz | same | 100 .. 3000 Hz |
| Repetition table | 1,2,3,5,7,11,13,17,23,29,31 | same | 1,2,3,5,7,11,13,17,19,23,29 |
| First tone's repetition draw | any of the 11 | same | one of the first 4 |
| Density curve | sin² | same | min(1, 2.5·sin)² |
| A tone is counted if | level is 1 | level > 0 | level > 0.69 |
| Level rule | on/off | continuous | continuous |
| First tone's level; level when nothing is counted | 1; 1 | 0.7; 0.7 | 0.7; 0.7 |

v4 and v5 differ only in the end phase, which the new composer replaces for all
versions, and in the default number of cycles. They are one rule set. (As built: the
rule sets are named `v4`, `v6` and `v10`, the patches `v4`, `v6`, `v10` and `bell`;
the preset `v5` is the rule set `v4` and the patch `v4` with 400 cycles.)

The two level rules need a few lines of code each, because they use the generator
differently. Both draw at the same place: after the repetition count, before the jitter.
`U(100)` means the next number from the generator, modulo 100 (as in Appendix A).

- **On/off** (Appendix A): always draws `r = U(100)`; level 1 if
  `150·(density + 0.05) > r` or nothing is counted, else 0.
- **Continuous:** if nothing is counted, level 0.7 and no draw. Otherwise `r = U(100)`
  and level = `min(1, 150·(density + 0.05) / (1 + r))`. The level is never below 0.075,
  so these versions have no tracked tones before the end phase.

Patches (sound):

| | v4 and v5 | v6 | v10 |
|---|---|---|---|
| Envelope | attack over ½ slot, fourth-root decay, linear release in the last slot | attack over ⅓ slot, a "ding" falling to half height by ⅔ slot, then the v4 shape at half height | as v6 |
| Vibrato depth | 0.0005 | 0.0005 | 0.0010 |
| Reverb taps (delay in samples at 44.1 kHz, gain, s = swapped) | 7000 .35, 9500 .10 s, 12000 .20, 17000 .08 s, 19000 .12 | 7000 .28, 9500 .09 s, 12000 .17, 17000 .07 s, 19000 .11, 30000 .05 s, 35000 .07 | as v6 |

Wave form (cosine), loudness law (600000/frequency in 16-bit units, about
18.3/frequency of full scale), strike gain ((N−j)/N), linear pan, and vibrato period
(18000 samples × (1 + jitter/1000)) never changed.

Preset defaults (neither rules nor sound):

| | v4 | v5 | v6 | v10 |
|---|---|---|---|---|
| Tempo (`bins`) | 1700 | 1700 | 3700 | 3440 |
| Drift per cycle | none | none | +1 sample | none |
| Cycles | 500 | 400 | 300 | 200 |
| Generator of the surviving examples | Linux | Linux | Linux | Apple (2022 files) |

All presets use the Linux generator. The 2022 files (`out_*.ogg`) were made with the
Apple generator, so the new program does not reproduce them; they stay in `legacy/` as
they are, and the Apple rule is recorded in `reference/README.md`.

**The 2020 variants** are not part of this plan's phases (section 13, answer 4); this
is a record of what they are. They are the folders `pp_18`, `pfib_18` and `psq_18`, which you
removed in December 2022 (commit 372c8da, "make your own!"), and `pfib_11`, which you
had already removed in October 2020 (commit 03e5882). All four sources and two `pp_18`
recordings are still in the git history.

- `pp_18`, `pfib_18` and `psq_18` are v10 with an 18-entry repetition table and three
  numbers changed: the first tone may draw any of the 18 (instead of the first 4); the
  draw reaches `d + 2` entries while `d < 17` (instead of 10); otherwise all 18. Since
  `d` is at most 11, the draw always reaches `d + 2` entries. Their defaults: `pp_18`
  1700 bins and 800 cycles; the other two 3100 and 2000.
- `pfib_11` is v10 with its own 11-entry table and two numbers changed: the first tone
  may draw any of the 11 (instead of the first 4), and the threshold is 17 (instead of
  10), so the draw reads past the end of the table. Its defaults were 3100 bins and 2000
  cycles. If it is wanted, it is entered with the threshold lowered to 10; that is a new
  rule set, not a reproduction.

## 10. Record of the seed recovery

- **Seed:** 1297735820, i.e. 2011-02-14 21:10:20 US Eastern (02:10:20 UTC on the 15th).
  Linux generator, `binsPerSlot = 1700`, `totalSongTime = 500`.
- **How it was narrowed down:** the OGG encoder numbered its stream with the first
  random number after seeding with the clock. That number pins the encoding to
  21:11:02 the same evening, 42 seconds after the tune's seed. The four other 2011
  examples, whose seeds are in their file names, show the same pattern.
- **How it was confirmed:** two independent searches (one matching predicted tones
  against the recording's spectrum over 1.2 million candidate seconds, one correlating
  rendered wave forms over 249,000) found the same seed. Re-rendering the original
  source with it correlates at 0.999 with the OGG overall, and above 0.99 in every
  audible cycle. The best of 295 control seeds reaches 0.065.
- **The source:** the archived files are dated 20:19 to 20:33 that evening, 37 minutes
  before the seed. You confirmed the source was not edited in between, and the match
  bears that out.
- **The file name is misleading:** `sndharmonics4_1700-400c.ogg` is 500 cycles long, not
  400, which matches the archived source.
- **The ending:** the last audible tone starts at tick 5154 (cycle 468), and the last
  strike with non-zero gain ends at 199.4 s. After that there is reverb decay only,
  until the 0/0 cuts to silence at 200.34 s; the remaining 11.8 s of the file are
  zeros. The tone that causes the cut is created at tick 5194, in the end phase.
- **Opening tones:** 1299.545 Hz at pan 0.26 with 13 repetitions; then 203.05 Hz and
  406.11 Hz.

## 11. Not being built

- Any sample-exact reproduction of the old audio, including its end-of-tune behaviour,
  its wrap-around on loud passages, and its WAV header without a length.
- Tones in the end phase.
- MIDI or any other note-based export; it cannot express this music.
- Built-in OGG/FLAC/MP3 encoding, real-time playback, a graphical interface.
- A plugin system or a configuration language for rules and patches.

## 12. Risks

- **The count of counted tones is easy to get subtly wrong.** It includes the tone about
  to be replaced and tones on their zero-gain strike. Verification item 1 catches any
  slip at the first differing tone, which is why the composer is built before any audio.
- **A few composer decisions compare floating-point numbers** (the frequency range and
  the on/off threshold). The closest calls are far above rounding differences between
  computers: for the favourite, 1.4 % on the frequency range and 0.0135 on the 0..100
  scale of the on/off draw; over 1,000 other seeds, 3·10⁻⁷ and 0.004. For v10's
  `> 0.69` test (measured) the closest level over 60,122 traced tones was 0.690058,
  a margin of 5.8·10⁻⁵.
- **The maths library can differ in the last digit between systems.** The first tone's
  frequency goes through `exp`, and every later frequency is a multiple of it. For the
  v4 rules this cannot change which tones are made (see the margins above), but it can
  change the last printed digits of the frequency column, so comparisons allow for it
  (section 5.2). This shows up only when the tests are run on a system other than
  Linux, such as a Mac.
- **Tunes other than the favourite can differ from 2011 in their final 30 cycles.** In
  the v4 code a new tone could still be switched on there, with 8 % probability per
  draw; this happened in 43 of 49 traced runs, and 129 of 192 traced seeds ended in the
  same 0/0 silence (the exact count depends on the compiler). In the favourite the first such tone (tick 5194) is
  the one that hit the 0/0 and silenced the output, so no end-phase tone is ever heard
  and the tune is unaffected.
- **"Sounds right" is a judgement.** Verification item 6 puts a number on it for the v4
  patch, but the final check is listening. (Measured.) For v6 the number did not come
  out lower than the original program's own: a strike's age is counted in slots, which
  follows the slowdown the way the old code did.
- **Loud seeds.** Over 192 sampled seeds the v4 scale peaked at a median of 55 % of full
  scale; three seeds exceeded 90 %, and one (1297700025) exceeded full scale and
  wrapped. The automatic turn-down covers this.
- **Tracing the later versions (done).** `docs/reference/` now holds tracing changes
  for v4, v5, v6 and v10 (`v4_trace.patch` and so on: files of source-code changes, not
  patches in the sense of section 4), the script that turns their logs into tone
  lists, and eight reference tone lists. The traced v5 and v6 programs match their
  2011 recordings (correlation 0.9989 to 0.9994). The v10 lists were made with the
  Linux generator, so they are not the 2022 tunes.
- **Other sample rates.** Reverb delays round to whole samples, which shifts the
  reverb's resonances by a fraction of a hertz away from 44.1 kHz.

## 13. Your answers to the open questions

Answered 2026-10-01.

1. **Output gain.** The plan's default: each tune keeps the old loudness and is turned
   down only when it would exceed full scale.
2. **A plain `chimes make`** uses the 14 February (v4) rules and sound.
3. **The 2022 Mac tunes are not part of the new program.** They remain a legacy
   reference only, and their behaviour is not carried forward: the new program always
   calls its own built-in Linux generator. Which computer made the old README tune
   (seed 1635773128) therefore no longer matters.
4. **The 2020 variants: not now.** To be dealt with later. Keep the code reasonably
   modular for a future refactor, but not over-abstracted.
5. **OGG:** piping into `ffmpeg` or `oggenc` is enough. No built-in encoder. (As built: the
   pipe works with `ffmpeg`; `oggenc` needs the finished WAV file.)
6. **Editing scores by hand: not yet.** To be dealt with later, with the same guidance
   as answer 4. Until then the score reader only has to accept files the program wrote
   itself.
7. **Licence.** MIT for the code. CC BY 4.0 for the sound samples. (`legacy/` keeps its
   own Creative Commons BY-SA 3.0 notice.) Whether the program prints a licence line
   when it runs was not answered; the plan assumes it does not.
8. **Name:** `chimes`.
9. **`legacy/` stays untouched**, including the misleading file name. New samples go
   into a new `samples/` directory. The first is the reconstructed favourite: seed
   1297735820, 500 cycles, made with the new program, with its score beside it.

## Appendix A: the v4 rules, exactly

`U(k)` means: take the next number from the generator, modulo k. S = 11 slots, T cycles
(at least 32).

```
RATIOS[25] = 5/16 5/8 5/4 5/2 5/1   3/8 3/4 3/2 3/1 6/1   1/4 1/2 1/1 2/1 4/1
             1/6 1/3 2/3 4/3 8/3   1/5 2/5 4/5 8/5 16/5          (this order)
REPS[11]   = 1 2 3 5 7 11 13 17 23 29 31

tick 0, slot 0 (the root tone):
    freq   = exp(U(10000) / 1000) / 1.5 + 50
    pan    = U(101) / 100
    reps   = REPS[U(11)]
    level  = 1;  jitter = none

for tick n = 1, 2, ... while cycle c = n div S <= T - 31:
    s = n mod S
    if the tone in slot s still has strikes left: continue
    d = number of slots whose tone has level 1
        (this includes the tone being replaced, and tones on their zero-gain strike;
         a slot that has never held a tone counts 0)
    density = sin((pi * c) / (T - 31)) ^ 2
    parent  = the tone now in slot (s - 1) mod S
    repeat  k = U(25);  freq = parent.freq * RATIOS[k]   until 50 <= freq <= 16000
    pan     = U(101) / 100
    reps    = REPS[U(d + 2)]  if d < 10,  else REPS[U(11)]
    r       = U(100)
    level   = 1  if 150 * (density + 0.05) > r,  or if d == 0;  else 0
    jitter  = U(100)
```

- The five draws happen in this order for every new tone, tracked ones included. The
  ratio draw is the only one that can repeat.
- A tone with repetition count N holds its slot for N+1 cycles, so its successor is
  created at tick n + S·(N+1).
- In cycle 0, slots 1..10 are filled one after another at ticks 1..10, each derived
  from the slot before it.
- All divisions are real-number divisions in double precision: `U(10000)/1000.0`,
  `U(101)/100.0`, `(pi·c)/(T−31)`, and each ratio `num/den`. The range test accepts both
  end points.
- In the density, `pi` is multiplied by `c` first and the product is then divided, as
  in the original. Dividing first changes the last digit of some continuous levels in
  v6 and v10 (measured: 33 of 451 and 29 of 562 in the reference lists); for v4 it
  makes no difference to any tone.
- Frequencies are multiplied as `parent.freq * (num / den)`, with the division done
  first, as in the original table. (Multiplying first and dividing after changes the
  last digit of roughly one frequency in eight.)
- `pi` is written as 3.141592653589793116 in the original.
- The loop condition means: tones are created up to and including cycle T−31; the
  final 30 cycles (T−30 to T−1) are the end phase.

## Appendix B: the v4 sound, as formulas

At 44.1 kHz, with S = 11 slots and u = a strike's age in slots (0 to S):

- **Phase.** Every voice starts at phase 0 at the beginning of the piece. Per sample:
  first `phase += 2π·freq/44100 × (1 + 0.0005·sin(2π·v/period))`, then the sample value
  is `cos(phase)` of the updated phase. `v` counts samples since the tone was created
  (0 on its first sample, used before it is incremented), and
  `period = 18000·(1 + jitter/1000)` samples (18000 for the root).
- **Loudness law:** `600000 / freq` in 16-bit units, that is `(600000 / freq) / 32768`
  of full scale.
- **Strike gain:** `(N − j) / N` for strike j of a tone with repetition count N.
- **Envelope:** `2u` for u < ½; `((S − u)/S)^¼` from u = ½ to u = S−1 inclusive;
  `S^(−¼)·(S − u)` for the last slot. The attack ends at 1.0 and the decay starts at
  0.988; this 1.2 % step is in the original and is kept. (The original wrote 0.549 for
  S^(−¼) = 0.54910.)
- **Pan:** left gain `1 − pan`, right gain `pan`.
- **Reverb**, per channel, applied to the sum of all voices:
  `out = in + 0.35·out[−7000] + 0.10·other[−9500] + 0.20·out[−12000] + 0.08·other[−17000] + 0.12·out[−19000]`,
  where `other` is the other channel's output. The gains sum to 0.85, so the reverb is
  stable. Its tail fades at about 5.5 dB per second.
- **Slot length:** 1701 samples; a cycle is 18711 samples (0.4243 s).

The old code also had one-sample irregularities at slot boundaries (its envelope clock
ran on 1700 while slots lasted 1701). These are not reproduced; the prototype shows
they do not matter to verification item 6.
