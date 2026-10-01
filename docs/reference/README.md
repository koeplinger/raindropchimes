# Reference material

Evidence gathered on 2026-09-30 and 2026-10-01 while recovering the seed of the
favourite tune (`legacy/examples/sndharmonics4_1700-400c.ogg`). The revamp plan
([../REVAMP_PLAN.md](../REVAMP_PLAN.md)) uses these files as its test oracles.

| File | What it is |
|---|---|
| `tune4_seed1297735820.tones.txt` | The tone list of the favourite tune for cycles 0..469, as produced by the original 2011-02-14 source with seed 1297735820. A new composer must reproduce its tone lines. |
| `glibc_rand.c` | Portable re-implementation of the Linux (glibc) `srand()`/`rand()`. Checked against the system generator on 40,000 seeds. The 2011 tunes need this generator. |
| `v4_trace.patch` | The tracing changes that produced the tone list. They add a seed argument and an `events.txt` log; the audio output is unchanged. |
| `trace_to_tones.py` | Turns that `events.txt` into tone lines in the layout of the reference list. |
| `later_versions_trace_prelude.h` | Tracing header used for the later versions (Feb 17, Feb 18, current). On its own it only logs every random draw (`rand.log`); it does not produce a tone list. It also contains the Apple/BSD `rand()` that the 2022 `out_*.ogg` examples need. |

## Regenerating the tone list

From the repository root:

```
tar -xzf legacy/examples/sndharmonics4.source.2011.02.14.tar.gz
cd harmonics.2011.02.14
patch -p1 < ../docs/reference/v4_trace.patch
gcc -O2 -w -o harmonics_trace harmonics.c -lm
./harmonics_trace 1297735820 500        # seed, cycles; writes out.wav and events.txt
python3 ../docs/reference/trace_to_tones.py events.txt 500 > tones.txt
grep -v '^#' ../docs/reference/tune4_seed1297735820.tones.txt | diff - tones.txt
```

The last command prints nothing when the list is reproduced.

`events.txt` has one line per new tone, with every random draw. The reference list is
its first 625 lines (ticks 0..5169) in a tidier layout, with each tone's parent added.
Lines after that belong to the end phase, where the 2011 code reads past the end of an
array; their content depends on the compiler and is not part of the reference.

## The later versions

No tone lists exist yet for the Feb 17, Feb 18 and current versions. Making them needs
event-log lines added to a copy of each source, as `v4_trace.patch` does for the Feb 14
version, with the seed, tempo and cycles taken from an example's file name. This is
phase 5 of the plan.

## The two random generators

- **Linux (glibc)**: all 2011 examples (`sndharmonics4/5/6_*`).
- **Apple/BSD**: all 2022 examples (`out_*.ogg`). The state `x` starts as the seed.
  Before each draw, an `x` of 0 is replaced by 123459876; then
  `x = 16807 * x mod 2147483647`, and `x` is the output.

Known answers: glibc with seed 1 gives 1804289383, 846930886. Apple/BSD with seed 1
gives 16807, 282475249, and with seed 2147483647 gives 0, 520932930, 28925691 (the
last follows Apple's published code and has not been checked on a Mac).

The same seed gives a different tune on each generator, so a tune's record must name
its generator.
