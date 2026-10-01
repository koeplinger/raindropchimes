# Raindrop Chimes: revamp plan

Status: draft plan, drafted 2026-09-30. Nothing of the new codebase is written yet.

Review status: a throwaway composer built from Appendix A alone reproduced the
reference tone list exactly, so the composition rules as written here are sufficient.
Three further checks were started and stopped unfinished on 2026-09-30 and should be
re-run before this plan is treated as final: a renderer built from Appendix B and
compared with the 2011 recording; a cell-by-cell check of the tables in section 9
against the old sources; and a read-through for gaps against the original request.

The goal is a clean, expandable codebase that keeps three things apart: **composition**
(the algorithm that decides which tones sound when), the **synthesizer** (wave form,
modulations, effects), and the **output file format**. The starting point is the
program as it was on 14 February 2011, because that version made the favourite tune.

## 1. Where things stand

- The old program and its examples are parked, untouched, in [legacy/](../legacy/).
- The seed of the favourite tune is recovered: **1297735820**
  (Monday 2011-02-14, 21:10:20 US Eastern). Details are in [section 10](#10-record-of-the-seed-recovery).
- The tone list of that tune, traced from the original code, is saved as
  [reference/tune4_seed1297735820.tones.txt](reference/tune4_seed1297735820.tones.txt).
  It is the main test for the new composer.
- A portable copy of the Linux random generator the 2011 tunes depend on is saved as
  [reference/glibc_rand.c](reference/glibc_rand.c).
- How these files were made, and how to make them again, is in
  [reference/README.md](reference/README.md).

## 2. Decisions already made

1. **Tones, not notes.** The music has no scale and no tuning. Every new frequency is the
   frequency of the tone in the previous slot times an exact small-integer ratio, so pitch
   is a lineage, not a position on a grid. A tone records its parent and its ratio.
   Nothing in the design uses pitch numbers or assumes a tone could be played on a piano.
2. **The composer must reproduce the old tone sequence.** With the 2011 rules and seed
   1297735820 it must produce the reference tone list exactly. This is checked on tone
   lists, not on audio.
3. **The synthesizer has to sound right, not match sample for sample.** There is no
   "legacy-exact" render mode.
4. **End-of-song bugs are not carried over.** The 2011 code ends the favourite tune by
   accident: an out-of-range table read produces a 0/0, which silences the reverb at
   200.34 s. The new code lets the reverb ring out.
5. **Synth order is wave form, then modulation, then effects.** The composer knows
   nothing about reverb.
6. **C (C99, standard library and maths library only).** It is portable and fast, the
   random-number arithmetic of the original carries over unchanged, and the old formulas
   transcribe one to one.

## 3. What the 14 February 2011 program does

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
  - its left/right position is random;
  - its repetition count comes from a table of 11 primes; the more tones are currently
    sounding, the further into the table the draw may reach;
  - whether it sounds or is only tracked silently is random, with a probability that
    follows a sin² curve over the whole piece (sparse, dense, sparse). A silent tone
    still occupies its slot and still passes its frequency on.
- The very first tone gets a random absolute frequency (about 50.7 to 14,720 Hz). It is
  the only absolute frequency in the piece.
- In the final 30 cycles no new tones are meant to start.

Only this part uses random numbers. The tone sequence depends on the rules, the random
generator, the seed, and the number of cycles (which shapes the sin² curve). It does not
depend on tempo or on anything in the synthesizer.

### Synthesizer

- **Wave form:** a cosine per slot.
- **Modulations:** a slight vibrato (0.05 % of the frequency, 2.2 to 2.45 cycles per
  second, with a slightly different rate per tone); loudness proportional to 1/frequency;
  the repetition decay; an envelope per strike (quick attack over half a slot, slow
  fourth-root decay over the cycle, linear release in the last slot); linear left/right
  panning.
- **Effect:** a feedback echo with five delays between 0.16 and 0.43 seconds, two of
  which swap left and right.

### Output

A 16-bit stereo WAV at 44.1 kHz, written sample by sample, with a header whose length
fields are left at zero.

### Where the three are tangled

| Tangle in the 2011 code | How the new design cuts it |
|---|---|
| The composer's "how many tones are sounding" count is computed inside the audio loop. | The composer counts from its own slot table. Traced runs confirm the two counts are always equal. |
| The slot clock is the audio sample counter. | The composer counts **ticks** (slot boundaries). Seconds and samples exist only in the synthesizer. |
| Tempo (`binsPerSlot`) is also the envelope's time unit, and every delay is a sample count at 44.1 kHz. | Tempo is a render setting in seconds. Envelope times are fractions of a slot; echo delays and vibrato periods are in seconds. |
| One random draw per tone sets the vibrato rate, a synthesizer matter, yet it is part of the composer's random sequence, even for silent tones. | The composer draws an anonymous number per tone (**jitter**, 0..99) and hands it over. The 2011 sound maps it to a vibrato period; other sounds may use or ignore it. |
| The repetition counter is both the composer's "slot is free" timer and the synth's loudness factor. | The tone carries its repetition count. Each side derives what it needs. |
| The piece ends when the composer's loop ends; the echo is cut off. | The synthesizer decides when the sound has died away. |
| Samples above 16-bit range wrap around; a 0/0 becomes silence. | The output stage saturates and reports it; an invalid number is an error, never written. |

## 4. Vocabulary

| Term | Meaning |
|---|---|
| **tick** | The composer's only clock: a counter of slot boundaries, 0, 1, 2, ... It has no duration. |
| **slot** | `tick mod 11`. A recurring position in the cycle that holds one tone at a time. |
| **cycle** | `tick div 11`. One pass over all slots. |
| **tone** | What the composer creates at a tick: a frequency given by its lineage, plus pan, repetition count, level and jitter. Its identity is the tick it was created at. |
| **lineage** | A tone's parent (the tone occupying the previous slot at that moment) and the exact ratio to it, as numerator/denominator. |
| **root tone** | The first tone. It has no parent and carries the only absolute frequency. |
| **strike** | One sounding of a tone. A tone with repetition count N has strikes 0..N, one per cycle. |
| **level** | 0 = tracked silently; above 0 up to 1 = audible. The 2011 rules only use 0 and 1. |
| **jitter** | An anonymous random number 0..99 per tone, drawn by the composer for the synthesizer's use. |
| **score** | The full tone list plus the settings that produced it. It is the whole composition. |
| **rules** | The tables and numbers that steer a composing algorithm. |
| **patch** | The description of a sound: wave form, modulations, effects. |
| **timing** | Tempo: seconds per slot, and how much that changes per cycle. |

## 5. Design

```
   rules + generator + seed + cycles          timing + patch + sample rate
                 |                                         |
                 v                                         v
          +-------------+        score         +----------------------+     frames     +--------+
          |  composer   | -------------------> |     synthesizer      | -------------> | output |
          +-------------+   (tone list, in     | wave -> modulation   |   (stereo,     +--------+
                             memory or as a    |      -> effects      |    floating       WAV,
                             text file)        +----------------------+    point)         raw, ...
```

There are three seams: random generator to composer, composer to synthesizer (the
score), and synthesizer to output. Everything else is plain functions and data tables.

### 5.1 The score

The score is the contract between composer and synthesizer. It exists in memory as a
struct and on disk as a text file that is a lossless print-out of that struct. The text
form uses the layout of the reference tone list, so checking the composer is a plain
file comparison.

```
# raindropchimes score 1
@ rules v4   @ generator glibc   @ seed 1297735820   @ cycles 500   @ slots 11
# tick cycle slot parent ratio freq_hz pan reps level jitter
0 0 0 - - 1299.5451597662952 0.26 13 1 -
1 0 1 0 5/16 406.10786242696724 0.70 3 0 49
2 0 2 1 1/2 203.05393121348362 0.28 3 1 57
```

```c
typedef struct {
    int32_t tick;      /* creation tick = the tone's identity                     */
    int32_t parent;    /* tick of the tone it derives from; -1 for the root       */
    int32_t num, den;  /* exact ratio to the parent                               */
    double  freq_hz;   /* root: drawn. Others: parent's freq_hz * num/den         */
    double  pan;       /* 0 = left .. 1 = right                                   */
    int32_t reps;      /* N >= 1: strikes 0..N, one per cycle                     */
    double  level;     /* 0 = silent, else up to 1                                */
    int32_t jitter;    /* 0..99, or -1 for none (root)                            */
} Tone;
```

- **Lineage is the truth.** `freq_hz` is a convenience: the composer needs it for its
  range test and the synthesizer needs it to make sound. When a score is read from a
  file, only the root's `freq_hz` is trusted; the rest are recomputed down the chain.
  Changing the root frequency therefore transposes the whole piece.
- **Silent tones are in the score**, because later tones descend from them. The
  synthesizer skips them.
- **Strikes are not listed.** The tone carries `reps`; the synthesizer expands the
  strikes, and how loud each strike is belongs to the patch.
- **The score has no tempo, no sample rate, and nothing about the sound.** Tempo is
  given to the synthesizer. The same score can be rendered slow or fast, at any sample
  rate, through any patch.

### 5.2 Composition

- A **composing algorithm** is a function that takes rules, a random generator, and a
  number of cycles, and fills a score. There is one to begin with: the slot algorithm of
  2011. A genuinely different algorithm later is a new file with a new function that
  fills the same score.
- **Rules are data.** The ratio table, the repetition table, the frequency range, and a
  dozen numbers form one block of constants per rule set. The historical versions
  differ only in such numbers, plus one choice between two ways of deciding a tone's
  level (see [section 9](#9-the-historical-versions-as-rule-sets-and-patches)).
- **The random generator is a named component** handed to the composer. Two are needed:
  `glibc` (Linux; all 2011 tunes) and `bsd` (Apple; the 2022 tunes). The system's own
  `rand()` is never called, so a seed gives the same tune on every computer.
- **End phase.** In the last 30 cycles the composer creates no tones at all. Tones
  already sounding finish their strikes. This is what the 2011 code intended, it is what
  the favourite tune audibly does (its last audible tone starts in cycle 468), and it
  removes the out-of-range read and the 0/0 by construction.

**What "same seed, same tune" means, precisely.** For a rule set, a generator, a seed
and a number of cycles T, every tone created before tick 11·(T−30) is identical to the
one the original program of that version created: same tick, parent, ratio, pan,
repetition count, level and jitter, and the same frequency to 17 digits. Tempo, sample
rate and patch cannot affect this, because the composer never sees them. The number of
cycles does affect it: T is part of a tune's identity, together with rules, generator
and seed.

### 5.3 Synthesizer

The synthesizer takes a score, a timing, a patch and a sample rate, and produces stereo
frames in floating point, where 1.0 is full scale.

- **Timing.** `slot_seconds` and `drift_seconds`: in cycle c a slot lasts
  `slot_seconds + c·drift_seconds`. The 2011 tempo is `1701/44100` s per slot (the old
  `binsPerSlot = 1700` plus one, because a slot was really 1701 samples). The slowdown
  of the 18 February version is a drift of `1/44100` s per cycle.
- **Voices.** One voice per slot, as in 2011: an oscillator whose phase runs on from one
  tone to the next. For each strike the voice computes, per sample,
  `wave(phase) × loudness(frequency) × level × strike_gain(j, N) × envelope(u) × pan`,
  where `u` is the strike's age in slot lengths. Vibrato modulates the phase increment.
- **Effects.** A chain that processes the mixed stereo signal. The 2011 echo is one
  effect: `out = in + Σ gainᵢ × out(delayed by dᵢ)`, with some taps reading the other
  channel. Delays are stored in seconds.
- **Ring-out.** After the last audible strike has ended, the synthesizer keeps feeding
  silence through the effects until the output stays below −96 dB of full scale for
  longer than the longest delay (with an upper limit of 60 s). The piece ends there.
- **A patch is data**: it names a wave form, an envelope, a loudness law, a strike-gain
  law and a pan law, gives the vibrato numbers, and lists the effects with their
  settings.

### 5.4 Output

- **The interface** is three functions: open, write a block of stereo frames, close. A
  format is one file that implements them.
- **First format:** 16-bit stereo WAV with correct header lengths. Second: raw samples
  to standard output, so that `ffmpeg` or `oggenc` can make OGG or FLAC without any
  codec library in this project.
- **Loudness.** Default: the 2011 level, unchanged (the favourite tune peaks at about
  half of full scale). If a tune would exceed full scale it is turned down just enough,
  and the program says so. This needs the peak before writing, so the synthesizer
  renders twice; rendering takes seconds.
- **Conversion to 16 bit** rounds to nearest and saturates. A value that is not a
  number stops the program with an error.

### 5.5 Files

```
src/
  score/     tone.h          Tone and Score
             score_text.c    write, read, check a score file
  compose/   rng.c           named generators: glibc, bsd
             rules.c         rule sets as data
             slotwalk.c      the 2011 slot algorithm
  synth/     timing.c        ticks to seconds
             voice.c         oscillator and modulations for one voice
             shapes.c        wave forms, envelopes, loudness/strike/pan laws, by name
             echo.c          the feedback echo
             effects.h       what an effect must provide
             patch.c         patches as data
             render.c        runs a score through voices and effects into an output
  output/    sink.h          what an output format must provide
             wav.c  raw.c
  main.c                     command line
tests/
docs/        this plan; reference/
legacy/      the 2011 program and all examples, untouched
Makefile
```

Three rules keep the parts apart, checked automatically by the tests:

- `compose/` includes nothing from `synth/` or `output/`.
- `synth/` and `output/` include nothing from `compose/` and never touch a random
  generator.
- `output/` sees only frames.

### 5.6 How to extend it

| To add | Do this |
|---|---|
| A rule set for the slot algorithm (new ratio table, new repetition table, other numbers) | Add one block of constants to `rules.c`. |
| A different composing algorithm | Add a file in `compose/` with a function that fills a score, and one line in the list of algorithms. |
| A random generator | Add two functions (seed, next) to `rng.c` and one line in its list. |
| A wave form, envelope, loudness law or pan law | Add one function to `shapes.c` and one line in its list. |
| A new kind of modulation (say, tremolo) | Add one factor in `voice.c` and one field in the patch. |
| An effect | Add a file that provides create / process / longest-delay / destroy, and one line in the list of effects. Patches list their effects in order. |
| A sound | Add one block of constants to `patch.c`. |
| An output format | Add a file in `output/` that provides open / write / close, and one line in the list of formats. |

The lists are plain name-to-function tables in the file they belong to. There is no
plugin system and no configuration language; the four historical versions differ in
about 25 numbers and two short branches, and that is the amount of flexibility the
design pays for.

## 6. Command line

```
chimes make --preset v4 --seed 1297735820       # the favourite tune
chimes make                                     # a new tune; the seed is the clock, and is printed
chimes make --seed 42 --bins 3400               # the same tune as seed 42, at half speed
chimes make --seed 42 --cycles 300              # a different tune (cycles shapes the piece); warns
chimes tones --preset v4 --seed 42              # the score only, no audio
chimes render my.tones --patch v6 -o my.wav     # any score through any patch
```

- A **preset** bundles rules, generator, patch, tempo, drift and cycles for one
  historical version. Each part can be overridden (`--rules`, `--generator`, `--patch`,
  `--bins`, `--drift`, `--cycles`, `--rate`, `--gain`, `-o`).
- `--bins B` is the old tempo unit, so numbers from old file names still work: a slot
  lasts (B+1)/44100 seconds.
- **The record of a tune is its score file**, written next to every audio file. Its
  header holds everything needed to make the tune again: rules, generator, seed, cycles,
  tempo, patch, gain, and the exact command. File names are a convenience, no longer the
  only record.

## 7. Verification

In order of importance.

1. **The favourite tune's tone list.** `chimes tones --preset v4 --seed 1297735820`
   must equal the reference list on all 625 lines, with no tone after tick 5169. This
   proves the generator, the order of the random draws, the sounding-tones count and the
   lineage in one go. On failure it shows the first differing line.
2. **Generator known answers.** glibc with seed 1 gives 1804289383, 846930886; bsd with
   seed 1 gives 16807, 282475249. This separates generator bugs from rule bugs.
3. **Separation.** The score is identical under different tempo, sample rate and patch,
   and different under a different number of cycles. The include rules of section 5.5
   hold.
4. **Score integrity.** Writing, reading and writing again gives the same file. Every
   frequency equals parent × ratio; every parent occupied the previous slot when its
   child was created; every repetition count is at least 1.
5. **Render sanity for the favourite tune.** Every sample is a valid number; nothing
   saturates; there is sound after 200.34 s; the end is below −96 dB; the WAV header is
   correct; two runs give identical files.
6. **Likeness to the 2011 recording** (a developer check; needs `ffmpeg` to decode the
   OGG). Because the voices keep their phase as in 2011, the new render should line up
   with the old recording wave for wave over cycles 0..469. The original code reaches a
   correlation of 0.999 against the OGG; the pass mark for the new synthesizer is to be
   set when first measured. If the wave forms turn out not to line up, the fallback is
   comparing loudness per cycle and per channel. Either way, your ears on the first
   half minute are the real test.
7. **Synthesizer unit checks.** The echo's response to a single click has the right
   delays, gains and channel swaps. A single tone has the right frequency, position and
   loudness. Renders at 44.1 and 48 kHz have the same duration and loudness per cycle.
8. **Composer properties over 1,000 seeds.** Frequencies stay in range, ratios come
   from the table, no tone is created in the end phase, the composer always finishes.
9. **Later versions.** Reference tone lists for the Feb 17, Feb 18 and current versions,
   traced from the legacy sources the same way, each checked like item 1.

## 8. Phases

Each phase ends with something that can be checked.

| Phase | Work | Check |
|---|---|---|
| 0. Freeze the evidence | Makefile; `rng.c` from `reference/glibc_rand.c`. Trace reference tone lists for the Feb 17, Feb 18 and current versions with `reference/later_versions_trace_prelude.h`, one per version, using seeds from the example file names. | Item 2. |
| 1. Composer | `tone.h`, the v4 rule set, `slotwalk.c`, score writer, `chimes tones`. | Items 1, 3, 4, 8. The tune exists as data. |
| 2. Dry synthesizer | Timing, voices with the v4 modulations, WAV output at the 2011 level. | The file plays. The first tone measures 1299.5 Hz at position 0.26. |
| 3. Echo and ring-out | The echo effect, ring-out, automatic turn-down. | Items 5, 6, 7, and you listen. **Phases 0–3 are the minimum that gives you your tune back.** |
| 4. Everyday use | Clock seed, overrides, score saved next to the audio, presets. | A command copied from a score header reproduces the same audio file. |
| 5. Later versions, one at a time | v5 (data only). v6 (the other level rule, its envelope, seven echo taps, drift). v10 (bsd generator, its numbers). The 2020 table variants. | Item 9, and item 6 against their OGG files. |
| 6. The seams, proven | Score reader and `chimes render`; raw output for piping; one new wave form and one new effect, written up as worked examples of section 5.6. | A score written, read and rendered gives the same audio as a direct render. |

## 9. The historical versions as rule sets and patches

Rules (composition):

| | v4 (Feb 14) and v5 (Feb 17) | v6 (Feb 18) | v10 (current) |
|---|---|---|---|
| Ratio table | 25 ratios | same | same |
| Frequency range | 50 .. 16000 Hz | same | 100 .. 3000 Hz |
| Repetition table | 1,2,3,5,7,11,13,17,23,29,31 | same | 1,2,3,5,7,11,13,17,19,23,29 |
| First tone's repetition draw | any of the 11 | same | one of the first 4 |
| Density curve | sin² | same | min(1, 2.5·sin)² |
| A tone counts as sounding if | level is 1 | level > 0 | level > 0.69 |
| Level rule | on or off, by chance | continuous volume | continuous volume |
| First tone's level; level when nothing else sounds | 1; 1 | 0.7; 0.7 | 0.7; 0.7 |

v4 and v5 differ only in the end phase, which the new composer replaces for all
versions, and in the default number of cycles. They are one rule set.

The two level rules need a few lines of code each, because they draw random numbers
differently: "on or off" always draws; "continuous volume" draws nothing when no other
tone is sounding.

Patches (sound):

| | v4 and v5 | v6 | v10 |
|---|---|---|---|
| Envelope | attack over ½ slot, fourth-root decay, linear release in the last slot | attack over ⅓ slot, a "ding" falling to half level by ⅔ slot, then the v4 shape at half level | as v6 |
| Vibrato depth | 0.0005 | 0.0005 | 0.0010 |
| Echo taps (delay in samples at 44.1 kHz, gain, s = swapped) | 7000 .35, 9500 .10 s, 12000 .20, 17000 .08 s, 19000 .12 | 7000 .28, 9500 .09 s, 12000 .17, 17000 .07 s, 19000 .11, 30000 .05 s, 35000 .07 | as v6 |

Preset defaults (neither rules nor sound):

| | v4 | v5 | v6 | v10 |
|---|---|---|---|---|
| Tempo (`bins`) | 1700 | 1700 | 3700 | 3440 |
| Drift per cycle | none | none | +1 sample | none |
| Cycles | 500 | 400 | 300 | 200 |
| Generator of the surviving examples | glibc | glibc | glibc | bsd (2022 files) |

Wave form (cosine), loudness law (600000/frequency in 16-bit units, i.e. about
18.3/frequency of full scale), strike gain ((N−j)/N), linear pan, and vibrato period
(18000 samples × (1 + jitter/1000)) never changed.

The 2020 variants (`pp_18`, `pfib_18`, `psq_18`) change only the repetition table and
three numbers. `pfib_11` reads past the end of its table in the old code; it is entered
with that fixed.

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
- **The file name is misleading:** `sndharmonics4_1700-400c.ogg` is 500 cycles long, not
  400, which matches the archived source.
- **The ending:** the last audible tone starts at tick 5154 (cycle 468). Cycles 470–472
  are echo decay only, until the bug described in section 2 cuts to silence at
  200.34 s; the remaining 11.8 s of the file are zeros.
- **Opening tones:** 1299.545 Hz at position 0.26 with 13 repetitions; then 203.05 Hz
  and 406.11 Hz.

## 11. Not being built

- Any sample-exact reproduction of the 2011 audio, including its end-of-song behaviour,
  its wrap-around on loud passages, and its zero-length WAV header.
- Tones in the end phase.
- MIDI or any other note-based export; it cannot express this music.
- Built-in OGG/FLAC/MP3 encoding, real-time playback, a graphical interface.
- A plugin system or a configuration language for rules and patches.

## 12. Risks

- **The sounding-tones count is easy to get subtly wrong.** It includes the tone about
  to be replaced and tones on their final, silent strike. Verification item 1 catches
  any slip at the first differing tone, which is why the composer is built before any
  audio.
- **A few composer decisions compare floating-point numbers** (the frequency range and
  the on/off threshold). For the favourite tune the closest calls are far above rounding
  differences between computers (1.4 % on the frequency range; 0.0135 on the 0..100
  scale of the on/off draw). For v10's `> 0.69` test the margins have not been measured.
- **Tunes other than the favourite can differ from 2011 in their last 30 cycles.** In
  the Feb 14 code a new tone could still sound there with 8 % probability per draw. The
  favourite has none, so it is unaffected.
- **"Sounds right" is a judgement.** Verification item 6 puts a number on it, but the
  final check is listening.
- **Loud seeds.** In a sample of 64 seeds the 2011 level peaked at up to 83 % of full
  scale, and one seed in a separate sample exceeded it. The automatic turn-down covers
  this.

## 13. Open questions

1. **Loudness.** Keep each tune at its 2011 level and turn it down only when it would
   exceed full scale (the plan's default), or bring every tune up to the same peak?
2. **A plain `chimes make`.** Should a new random tune use the Feb 14 rules and sound,
   or the current (v10) ones?
3. **The 2022 Mac tunes.** Are they part of the first round, or can the bsd generator
   and the v10 rules wait until the Feb 14 version is fully done (phase 5)?
4. **OGG.** Is piping into `ffmpeg` or `oggenc` enough, or should compressed output be
   built in (which adds a library dependency)?
5. **Editing scores by hand.** Do you want to change a ratio or the root frequency in a
   score file and render it? That decides whether phase 6 moves forward.
6. **The example's file name.** Rename `sndharmonics4_1700-400c.ogg` to carry 500 and
   the seed, or leave `legacy/` strictly as it was?

## Appendix A: the 14 February 2011 rules, exactly

`U(k)` means: take the next number from the generator, modulo k. S = 11 slots, T cycles.

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
        (this includes the tone being replaced, and tones on their final silent strike)
    density = sin(pi * c / (T - 31)) ^ 2
    parent  = the tone now in slot (s - 1) mod S
    repeat  k = U(25);  freq = parent.freq * RATIOS[k]   until 50 <= freq <= 16000
    pan     = U(101) / 100
    reps    = REPS[U(d + 2)]  if d < 10,  else REPS[U(11)]
    r       = U(100)
    level   = 1  if 150 * (density + 0.05) > r,  or if d == 0;  else 0
    jitter  = U(100)
```

- The five draws happen in this order for every new tone, silent ones included. The
  ratio draw is the only one that can repeat.
- A tone with repetition count N holds its slot for N+1 cycles, so its successor is
  created at tick n + S·(N+1).
- In cycle 0, slots 1..10 are filled one after another at ticks 1..10, each derived
  from the slot before it.
- Frequencies are multiplied in double precision as `parent.freq * (num / den)`, with
  the division done first, as in the original table.
- `pi` is written as 3.141592653589793116 in the original.

## Appendix B: the 14 February 2011 sound, exactly

At 44.1 kHz, with B = one slot and u = a strike's age in slots (0 to 11):

- **Phase**, per sample: `phase += 2π·freq/44100 × (1 + 0.0005·sin(2π·v/period))`, where
  `v` counts samples since the tone was created and `period = 18000·(1 + jitter/1000)`
  samples (18000 for the root). The sample value is `cos(phase)`.
- **Loudness:** `600000 / freq` in 16-bit units.
- **Strike gain:** `(N − j) / N` for strike j of a tone with repetition count N.
- **Envelope:** `2u` for u < ½; `((11 − u)/11)^¼` up to u = 10; `0.549·(11 − u)` for the
  last slot. (0.549 stands for 11^(−¼); the new code uses the exact value.)
- **Pan:** left gain `1 − pan`, right gain `pan`.
- **Echo**, per channel, applied to the sum of all voices:
  `out = in + 0.35·out[−7000] + 0.10·other[−9500] + 0.20·out[−12000] + 0.08·other[−17000] + 0.12·out[−19000]`,
  where `other` is the other channel's output. The gains sum to 0.85, so the echo is
  stable. Its tail fades at roughly 5.5 dB per second, so ring-out takes on the order
  of ten seconds.
- **Slot length:** 1701 samples; a cycle is 18711 samples (0.4243 s).

For the likeness check (verification item 6) the voice must advance its phase when the
2011 code did: on every sample of every strike of an audible tone, including the final
zero-gain strike, and not while its slot holds a silent tone. The phase is never reset.
The 2011 code also had one-sample irregularities at slot boundaries (its envelope clock
ran on 1700 while slots lasted 1701); these are not reproduced and do not matter to
the check.
