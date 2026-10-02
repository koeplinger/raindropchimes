# Raindrop Chimes

Music from simple mathematical rules. Every tone is derived from an earlier tone by an
exact small-integer ratio (the families 5, 3, 1, 1/3 and 1/5, shifted by powers of two),
so the music has no scale and no tuning: pitch is a lineage, not a position on a grid.

For the idea and some history see http://jenskoeplinger.com/raindropchimes

## Build

A C compiler and `make` are all it needs.

```
make          # builds ./chimes
make test     # runs every check
```

A few checks compare against the 2011 recordings and the 2011 programs; they need
`ffmpeg`, `patch` and `python3`, and are skipped where those are missing.

## Make a tune

```
./chimes make                                  # a new tune; the seed is the clock, and is printed
./chimes make --seed 1297735820                # the tune of 14 February 2011 (see samples/)
./chimes make --seed 42 --bins 3401            # the same tune as seed 42, at half speed
./chimes make --seed 42 --cycles 300           # a different tune: the length shapes the piece
./chimes make --seed 42 --preset v6            # the rules and sound of 18 February 2011
./chimes make --seed 42 --patch bell           # the same tones through another sound
```

`chimes make` writes two files into the current folder: the audio,
`chimes_<seed>_<bins>_<cycles>.wav`, and the **score** beside it,
`chimes_<seed>_<bins>_<cycles>.score`. The score is the tune's record: a text file that
lists every tone with its parent tone and exact ratio, and whose header holds the
settings and a complete command that recreates the audio. Keep the score and you can
always make the tune again: `./chimes render chimes_42_1700_500.score`, or the command
in its `# render: command` line (it is written as `chimes ...`; type `./chimes ...`
unless the program is on your PATH).

The file name does not say which sound or rules were used: the same seed made again
with another `--patch`, `--rules`, `--drift` or `--rate` replaces the earlier files.
Give `-o NAME.wav` to keep both.

To listen, use any audio player, for example `ffplay -nodisp -autoexit FILE.wav`
(it comes with `ffmpeg`), `aplay FILE.wav` on Linux, or `afplay FILE.wav` on a Mac.

```
./chimes score --seed 42 -o my.score           # the score only, no audio (without -o it is printed)
./chimes render my.score --patch v6 -o my.wav  # an existing score through any sound
./chimes help                                  # all options
```

`render` leaves the tones as they are. It writes the audio and, beside it, the score
again with the settings it used (above: `my.wav`, and `my.score` once more). Without
`-o` the audio goes next to the score file, under the same name with `.wav`. A score
made by `chimes score` records no tempo and no sound; `render` then uses those of the
`v4` preset unless you give `--preset`, `--patch`, `--bins` or `--drift`.

To get OGG or FLAC, pipe the raw output into an encoder:

```
./chimes make --seed 42 -o - | ffmpeg -f f32le -ar 44100 -ac 2 -i - tune.ogg
```

The score is then written into the current folder under its default name (here
`chimes_42_1700_500.score`). The command recorded in it ends in `-o -` and makes the
raw output only; add the `| ffmpeg ...` part again when you use it. The number after
`-ar` must be the sample rate (`--rate`; 44100 unless you say otherwise).

## What decides a tune

| Setting | Effect |
|---|---|
| `--seed` | Which tune. The same seed gives the same tune on any computer. |
| `--cycles` | The length, in cycles of 11 slots. It also shapes the piece (sparse, dense, sparse), so another length is another tune. |
| `--preset` | The rules, sound, tempo and length of one historical version: `v4` (14 Feb 2011, the default), `v5`, `v6`, `v10`. `v5` is the `v4` rules and sound with 400 cycles. |
| `--rules` | The rule set alone: `v4`, `v6`, `v10`. |
| `--bins`, `--drift` | Tempo: a slot lasts (bins + 1)/44100 seconds, the 2011 unit (default 1700, so 3401 is half speed). Drift: bins added per cycle (`v6`: 1; a negative drift speeds the piece up). They change how the tune is played, never which tones it has. |
| `--patch` | The sound: `v4`, `v6`, `v10`, `bell`. It never changes the tones either. |
| `--rate`, `--gain` | Sample rate (default 44100; a tone above half the sample rate cannot be carried and is left out) and output gain (1 is the loudness of the 2011 program; a tune that would exceed full scale is turned down just enough). |

## How it is put together

Three parts, kept apart:

- **Composition** (`src/compose/`) decides which tones sound when. It is the only part
  that uses random numbers, and it knows nothing about seconds, samples or sound.
- **Synthesizer** (`src/synth/`) turns a score into sound: wave form, then modulations,
  then reverb.
- **Output** (`src/output/`) writes the frames to a file.

The score (`src/score/`) is what passes from the composer to the synthesizer.

- [docs/REVAMP_PLAN.md](docs/REVAMP_PLAN.md): the design, the exact rules and formulas,
  and how the tune of 14 February 2011 was recovered.
- [docs/EXTENDING.md](docs/EXTENDING.md): how to add a wave form, an effect, a sound or
  a rule set.
- [docs/reference/](docs/reference/): the tone lists traced from the 2011 programs,
  which the checks compare against.

## Samples and the old program

- [samples/](samples/): tunes made with this program, each with its score.
- [legacy/](legacy/): the original 2011 single-file program and the tunes it made over
  the years, kept as they were.

The five 2011 tunes in `legacy/examples/` can be made again; `make test` compares them
with the recordings wave for wave. (The new files end when the reverb has died away,
not at a fixed length; the plan, section 12, says what else can differ in the final 30
cycles.)

```
./chimes make --seed 1297735820                                      # sndharmonics4_1700-400c
./chimes make --preset v5 --seed 1297975154                          # sndharmonics5_1700-400-1297975154
./chimes make --preset v5 --seed 1297981226 --cycles 200 --bins 700  # sndharmonics5_700-200-1297981226
./chimes make --preset v6 --seed 1298060492                          # sndharmonics6_3700p1-300-1298060492
./chimes make --preset v6 --seed 1298060823                          # sndharmonics6_3700p1-300-1298060823
```

The 2022 tunes (`out_<seed>_<bins>_<cycles>.ogg`) cannot: they were made on a Mac,
whose random generator is a different one, and `chimes` has only the Linux generator.
`--preset v10` with one of their seeds gives another tune, under a file name that looks
the same.

## Licences

- The code is under the MIT licence: see [LICENSE](LICENSE).
- The sound samples in `samples/` are under
  [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).
- `legacy/` keeps its own notice (Creative Commons Attribution-ShareAlike 3.0).
