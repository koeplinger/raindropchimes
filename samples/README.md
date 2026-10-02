# Samples

Tunes made with `chimes`, each with its score beside it. The score is the tune's
record: every tone with its parent and exact ratio, and the settings and command that
made the audio.

The sound samples in this folder are © Jens Koeplinger, licensed under
[Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/)
(CC BY 4.0).

| File | What it is |
|---|---|
| `chimes_1297735820_1700_500.ogg`, `.score` | The tune of 14 February 2011: seed 1297735820, 500 cycles, the rules and sound of that day (preset `v4`). The same tune as `legacy/examples/sndharmonics4_1700-400c.ogg`, whose seed had been lost; here the reverb rings out at the end instead of being cut off. 208 seconds. |

## How a sample is made

`chimes` writes WAV files, which are large (37 MB for this tune). For a sample, the raw
frames are piped into an encoder instead. From this folder:

```
../chimes make --preset v4 --seed 1297735820 -o - \
  | ffmpeg -y -f f32le -ar 44100 -ac 2 -i - -c:a libvorbis -q:a 6 \
      -metadata title="Raindrop Chimes, seed 1297735820 (the tune of 14 February 2011)" \
      -metadata artist="Jens Koeplinger" \
      -metadata license="CC BY 4.0, https://creativecommons.org/licenses/by/4.0/" \
      -metadata comment="chimes make --preset v4 --seed 1297735820" \
      chimes_1297735820_1700_500.ogg
```

`-y` lets `ffmpeg` replace the file that is already there; without it `ffmpeg` stops.
The score is written into the current folder under its default name,
`chimes_<seed>_<bins>_<cycles>.score`. The command recorded in it ends in `-o -`: it is
the first half of the pipe above.

## From a score back to audio

From the top folder of the repository:

```
./chimes render samples/chimes_1297735820_1700_500.score -o tune.wav
```

This gives the uncompressed audio that the sample was encoded from, with the tempo and
sound recorded in the score, and writes `tune.score` beside it. Keep the `-o`: without
it the WAV is written into `samples/` and the sample's own score is written again.
Other settings give the same tune another way: `--bins` for the tempo, `--patch` for
the sound.
