# Raindrop Chimes

Music from simple mathematical rules. Every tone is derived from an earlier tone by an
exact small-integer ratio (the families 5, 3, 1, 1/3 and 1/5, shifted by powers of two),
so the music has no scale and no tuning: pitch is a lineage, not a position on a grid.

For the idea and some history see http://www.jenskoeplinger.com/RC/raindrops.html

## Build

A C compiler and `make` are all it needs.

```
make          # builds ./chimes
make test     # runs every check
```

## Make a tune

```
./chimes make                                  # a new tune; the seed is the clock, and is printed
./chimes make --seed 1297735820                # the tune of 14 February 2011 (see samples/)
./chimes make --seed 42 --bins 3401            # the same tune as seed 42, at half speed
./chimes make --seed 42 --cycles 300           # a different tune: the length shapes the piece
./chimes make --seed 42 --preset v6            # the rules and sound of 18 February 2011
./chimes make --seed 42 --patch bell           # the same tones through another sound
```

`make` writes two files: the audio, `chimes_<seed>_<bins>_<cycles>.wav`, and the
**score** beside it, `chimes_<seed>_<bins>_<cycles>.score`. The score is the tune's
record: a text file that lists every tone with its parent tone and exact ratio, and that
ends its header with the complete command that recreates the audio. Keep the score and
you can always make the tune again.

```
./chimes score --seed 42                       # the score only, no audio
./chimes render my.score --patch v6 -o my.wav  # an existing score through any sound
./chimes help                                  # all options
```

To get OGG or FLAC, pipe the raw output into an encoder:

```
./chimes make --seed 42 -o - | ffmpeg -f f32le -ar 44100 -ac 2 -i - tune.ogg
```

## What decides a tune

| Setting | Effect |
|---|---|
| `--seed` | Which tune. The same seed gives the same tune on any computer. |
| `--cycles` | The length, in cycles of 11 slots. It also shapes the piece (sparse, dense, sparse), so another length is another tune. |
| `--preset` | The rules and sound of one historical version: `v4` (14 Feb 2011, the default), `v5`, `v6`, `v10`. |
| `--bins`, `--drift` | Tempo and slowdown. They change how the tune is played, never which tones it has. |
| `--patch` | The sound: `v4`, `v6`, `v10`, `bell`. |
| `--rate`, `--gain` | Sample rate and output gain. |

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

## Licences

- The code is under the MIT licence: see [LICENSE](LICENSE).
- The sound samples in `samples/` are under
  [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).
- `legacy/` keeps its own notice (Creative Commons Attribution-ShareAlike 3.0).
