# Reference material

Evidence gathered on 2026-09-30 while recovering the seed of the favourite tune
(`legacy/examples/sndharmonics4_1700-400c.ogg`). The revamp plan
([../REVAMP_PLAN.md](../REVAMP_PLAN.md)) uses these files as its test oracles.

| File | What it is |
|---|---|
| `tune4_seed1297735820.tones.txt` | The tone list of the favourite tune for cycles 0..469, as produced by the original 2011-02-14 source with seed 1297735820. A new composer must reproduce it exactly. |
| `glibc_rand.c` | Portable re-implementation of the Linux (glibc) `srand()`/`rand()`. Checked against the system generator on 40,000 seeds. The 2011 tunes need this generator. |
| `v4_trace.patch` | The tracing changes that produced the tone list. They add a seed argument and an `events.txt` log; the audio output is unchanged. |
| `later_versions_trace_prelude.h` | Tracing header used for the later versions (Feb 17, Feb 18, current). It also contains the Apple/BSD `rand()` that the 2022 `out_*.ogg` examples need. |

## Regenerating the tone list

```
tar -xzf legacy/examples/sndharmonics4.source.2011.02.14.tar.gz
cd harmonics.2011.02.14
patch -p1 < ../docs/reference/v4_trace.patch
gcc -O2 -w -o harmonics_trace harmonics.c -lm
./harmonics_trace 1297735820        # writes out.wav and events.txt
```

`events.txt` has one line per new tone, with every random draw. The reference list is
its first 625 lines (ticks 0..5169) in a tidier layout, with each tone's parent added.
Lines after that belong to the end phase, where the 2011 code reads past the end of an
array; their content depends on the compiler and is not part of the reference.

## The two random generators

- **Linux (glibc)**: all 2011 examples (`sndharmonics4/5/6_*`).
- **Apple/BSD**: all 2022 examples (`out_*.ogg`). The rule is
  `x = 16807 * x mod 2147483647`, with the seed as the first `x` (a seed of 0 is
  replaced by 123459876), and each new `x` is the output.

The same seed gives a different tune on each, so a tune's record must name its generator.
