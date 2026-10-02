# Reference material

Evidence gathered on 2026-09-30 and 2026-10-01: first while recovering the seed of the
favourite tune (`legacy/examples/sndharmonics4_1700-400c.ogg`), then while tracing the
later versions of the old program. The revamp plan
([../REVAMP_PLAN.md](../REVAMP_PLAN.md)) uses these files as the known-right answers
that its checks compare against.

| File | What it is |
|---|---|
| `tune4_seed1297735820.tones.txt` | The tone list of the favourite tune for cycles 0..469, as produced by the original 2011-02-14 source (v4) with seed 1297735820, 500 cycles. A new composer must reproduce its tone lines. |
| `tune5_seed1297975154_cycles400.tones.txt` | The tone list of `sndharmonics5_1700-400-1297975154.ogg`, from the 2011-02-17 source (v5): seed 1297975154, 400 cycles. 571 tones. |
| `tune5_seed1297981226_cycles200.tones.txt` | The tone list of `sndharmonics5_700-200-1297981226.ogg`, from the v5 source: seed 1297981226, 200 cycles. 257 tones. |
| `tune6_seed1298060492_cycles300.tones.txt` | The tone list of `sndharmonics6_3700p1-300-1298060492.ogg`, from the 2011-02-18 source (v6): seed 1298060492, 300 cycles. 212 tones. |
| `tune6_seed1298060823_cycles300.tones.txt` | The tone list of `sndharmonics6_3700p1-300-1298060823.ogg`, from the v6 source: seed 1298060823, 300 cycles. 239 tones. |
| `v10_seed1654607101_cycles200_glibc.tones.txt` | The v10 rules (the code in `legacy/`) run with the Linux generator: seed 1654607101, 200 cycles. 187 tones. **Not** the 2022 tune of that seed, which was made with the Apple generator. |
| `v10_seed1655110192_cycles200_glibc.tones.txt` | The same for seed 1655110192, 200 cycles. 174 tones. Its first tone (56.5 Hz) lies outside 100..3000 Hz. |
| `v10_seed1655110280_cycles200_glibc.tones.txt` | The same for seed 1655110280, 200 cycles. 201 tones. Added because it takes the one branch the other two never take: at tick 24 no tone is counted, so that tone gets level 0.7 without a level draw. |
| `glibc_rand.c` | Portable re-implementation of the Linux (glibc) `srand()`/`rand()`. Checked against the system generator on 40,000 seeds. The 2011 tunes need this generator. |
| `v4_trace.patch` | The tracing changes that produced the v4 tone list. They add a seed argument and an `events.txt` log; the audio output is unchanged. |
| `v5_trace.patch`, `v6_trace.patch`, `v10_trace.patch` | The same kind of tracing changes for the later versions. The traced program takes seed, cycles and `binsPerSlot` from the command line. |
| `trace_to_tones.py` | Turns the `events.txt` of any of the four traced programs into tone lines in the layout of the reference lists. |
| `compare_with_recording.py` | Compares a traced program's `out.wav` with a 2011 recording (needs `ffmpeg` and `numpy`). |
| `.gitignore` | Keeps two kinds of scratch out of git: the recipes' working directory `work/` (below) and Python's `__pycache__/`. |
| `later_versions_trace_prelude.h` | Superseded by the three patches above; nothing uses it. An earlier tracing header that only logs every random draw. It also holds the Apple/BSD `rand()` in C, the generator of the 2022 `out_*.ogg` examples. |

All tone lists stop before the end phase (the final 30 cycles), because the new composer
creates no tones there. They all have the same ten columns, explained in each file's
comment header.

## Regenerating the tone lists

The recipes run from the repository root and need Linux: the old sources call the
system's `rand()`, which must be the glibc one. Each `diff` prints nothing when the list
is reproduced.

`make test` repeats this comparison on other seeds: `tests/check_20_original_program.sh`
builds the traced v4, v6 and v10 programs the same way and compares their tones with
the new composer's.

Every recipe on this page works inside `docs/reference/work/` and nowhere else. Git
ignores that directory (see `.gitignore` here), so nothing in it can be committed by
accident. Delete it when you are done: `rm -r docs/reference/work`.

### v4 (14 February 2011)

```
mkdir -p docs/reference/work
tar -xzf legacy/examples/sndharmonics4.source.2011.02.14.tar.gz -C docs/reference/work
cd docs/reference/work/harmonics.2011.02.14
patch -p1 < ../../v4_trace.patch
gcc -O2 -w -o harmonics_trace harmonics.c -lm
./harmonics_trace 1297735820 500        # seed, cycles; writes out.wav and events.txt
python3 ../../trace_to_tones.py events.txt 500 > tones.txt
grep -v '^#' ../../tune4_seed1297735820.tones.txt | diff - tones.txt
cd ../../../..
```

`events.txt` has one line per new tone, with every random draw. The reference list is
its first 625 lines (ticks 0..5169) in a tidier layout, with each tone's parent added.
Lines after that belong to the end phase, where the 2011 code reads past the end of an
array; their content depends on the compiler and is not part of the reference.

### v5 (17 February 2011)

```
mkdir -p docs/reference/work
tar -xzf legacy/examples/sndharmonics5.source.tar.gz -C docs/reference/work
cd docs/reference/work/harmonics.2011.02.17
patch -p1 < ../../v5_trace.patch
gcc -O2 -w -o harmonics_trace harmonics.c -lm

./harmonics_trace 1297975154 400 1700 > /dev/null    # seed, cycles, binsPerSlot; writes out.wav and events.txt
python3 ../../trace_to_tones.py events.txt 400 > tones.txt
grep -v '^#' ../../tune5_seed1297975154_cycles400.tones.txt | diff - tones.txt

./harmonics_trace 1297981226 200 700 > /dev/null
python3 ../../trace_to_tones.py events.txt 200 > tones.txt
grep -v '^#' ../../tune5_seed1297981226_cycles200.tones.txt | diff - tones.txt
cd ../../../..
```

### v6 (18 February 2011)

```
mkdir -p docs/reference/work
tar -xzf legacy/examples/sndharmonics6.source.tar.gz -C docs/reference/work
cd docs/reference/work/harmonics.2011.02.18
patch -p1 < ../../v6_trace.patch
gcc -O2 -w -o harmonics_trace harmonics.c -lm

./harmonics_trace 1298060492 300 3700 > /dev/null
python3 ../../trace_to_tones.py events.txt 300 > tones.txt
grep -v '^#' ../../tune6_seed1298060492_cycles300.tones.txt | diff - tones.txt

./harmonics_trace 1298060823 300 3700 > /dev/null
python3 ../../trace_to_tones.py events.txt 300 > tones.txt
grep -v '^#' ../../tune6_seed1298060823_cycles300.tones.txt | diff - tones.txt
cd ../../../..
```

3700 is the `binsPerSlot` of the first cycle; the v6 program itself adds 1 per cycle
(the "p1" in the file names).

### v10 (the code in `legacy/`), with the Linux generator

```
mkdir -p docs/reference/work/v10_trace
cp legacy/harmonics.c legacy/harmonics-util.c legacy/harmonics-dingdong.c docs/reference/work/v10_trace/
cd docs/reference/work/v10_trace
patch -p1 < ../../v10_trace.patch
gcc -O2 -w -o harmonics_trace harmonics.c -lm

./harmonics_trace 1654607101 200 3440 > /dev/null
python3 ../../trace_to_tones.py events.txt 200 > tones.txt
grep -v '^#' ../../v10_seed1654607101_cycles200_glibc.tones.txt | diff - tones.txt

./harmonics_trace 1655110192 200 3440 > /dev/null
python3 ../../trace_to_tones.py events.txt 200 > tones.txt
grep -v '^#' ../../v10_seed1655110192_cycles200_glibc.tones.txt | diff - tones.txt

./harmonics_trace 1655110280 200 3440 > /dev/null
python3 ../../trace_to_tones.py events.txt 200 > tones.txt
grep -v '^#' ../../v10_seed1655110280_cycles200_glibc.tones.txt | diff - tones.txt
cd ../../../..
```

The 2022 recordings with these seeds (`legacy/examples/out_<seed>_*.ogg`) were made on
a Mac, with the Apple generator. These three lists are therefore not those tunes; they
are what the v10 rules give with the Linux generator, which is the only generator the
new program has. `binsPerSlot` (tempo) has no effect on a tone list, so 3440, the
source's own value, is used for all three.

## What the traced programs log

The v5, v6 and v10 traced programs write `events.txt` with one line per new tone, the
first tone included (`INIT`), and tones of the end phase included (`endphase=1`). The
label `NOTE` at the start of a line is the traced program's own word; it marks one new
tone.

```
NOTE tick=24 cycle=2 slot=2 endphase=0 dens=0.0086350773972429049 d=0 | r25all=17 rejects=0 r25=17 r101=6 rCnt=0 modCnt=2 r100play=- r100vib=11 | prevF=1663.4585852217506 ratio=0.66666666666666663 freq=1108.9723901478337 pos=0.06 cnt=1 play=0.69999999999999996 vibBins=18197.999999999996
```

| Field | Meaning |
|---|---|
| `tick`, `cycle`, `slot` | where the tone was created |
| `endphase` | 1 if the program was in its final 30 cycles |
| `dens` | density at that moment |
| `d` | the count of counted tones the program saw at that moment (before the end phase overwrites it) |
| `r25all` | every ratio draw (index into the ratio table), rejected ones first |
| `rejects`, `r25` | how many ratio draws were rejected, and the one that was accepted |
| `r101` | the pan draw |
| `rCnt`, `modCnt` | the repetition draw (index into the repetition table) and its modulus |
| `r100play` | the level draw, or `-` if none was made |
| `r100vib` | the jitter draw |
| `prevF`, `ratio`, `freq` | parent frequency, ratio and resulting frequency, 17 significant digits |
| `pos`, `cnt`, `play` | pan, repetition count, level (17 significant digits in v6 and v10) |
| `vibBins` | the vibrato period the old sound derives from the jitter |

The last line (`SUMMARY`) gives the number of samples, ticks, tones and `rand()` calls.

The traced program also stops with an error ("d mismatch") if `d`, which the old code
counts inside its audio loop, differs from a count the tracing lines make from the slot
table at the moment the tone is created. This is only a guard against a misplaced
tracing line. Both counts apply the same test to the same table, and nothing changes
the table in between, so the guard cannot fire and its silence proves nothing. That a
composer with no audio loop arrives at the same `d` is shown differently: by the
comparison with tick-by-tick composers under
[What the traces show about the rules](#what-the-traces-show-about-the-rules).

## Checking that a traced program is the real thing

**The tracing changes do not alter the audio.** For every tune listed below, the
unpatched source was given the same seed (and, where they differ from the source's own
values, the same cycles and `binsPerSlot`) by editing those constants, built the same
way, and its `out.wav` compared with the traced program's using `cmp`: identical, byte
for byte.

| Version | Seed | Cycles | `binsPerSlot` | `out.wav` bytes | Traced = unpatched |
|---|---|---|---|---|---|
| v5 | 1297975154 | 400 | 1700 | 29,937,644 | identical |
| v5 | 1297981226 | 200 | 700 | 6,168,844 | identical |
| v6 | 1298060492 | 300 | 3700 (+1 per cycle) | 50,826,644 | identical |
| v6 | 1298060823 | 300 | 3700 (+1 per cycle) | 50,826,644 | identical |
| v10 | 1654607101 | 200 | 3440 | 30,280,844 | identical |
| v10 | 1655110192 | 200 | 3440 | 30,280,844 | identical |
| v10 | 1655110280 | 200 | 3440 | 30,280,844 | identical |

To repeat it for one tune, after the v6 recipe above (whose last run was seed
1298060823):

```
mkdir -p docs/reference/work/untraced
tar -xzf legacy/examples/sndharmonics6.source.tar.gz -C docs/reference/work/untraced
cd docs/reference/work/untraced/harmonics.2011.02.18
sed -i 's/seedTime = time(NULL);/seedTime = 1298060823;/' lib-harmonics-dingdong.h
gcc -O2 -w -o harmonics harmonics.c -lm
./harmonics > /dev/null
cmp out.wav ../../harmonics.2011.02.18/out.wav        # prints nothing: identical
cd ../../../../..
```

**The v5 and v6 traced programs reproduce the 2011 recordings.** The traced program's
`out.wav` was compared with the example OGG (decoded with `ffmpeg`) over the full
length, per channel, with no time shift. The remainder is the OGG compression.

| Recording | Frames (both) | Correlation left | Correlation right |
|---|---|---|---|
| `sndharmonics5_1700-400-1297975154.ogg` | 7,484,400 | 0.99941 | 0.99938 |
| `sndharmonics5_700-200-1297981226.ogg` | 1,542,200 | 0.99921 | 0.99908 |
| `sndharmonics6_3700p1-300-1298060492.ogg` | 12,706,650 | 0.99933 | 0.99915 |
| `sndharmonics6_3700p1-300-1298060823.ogg` | 12,706,650 | 0.99891 | 0.99888 |

To repeat it for one tune, after the v6 recipe above:

```
python3 docs/reference/compare_with_recording.py docs/reference/work/harmonics.2011.02.18/out.wav \
    legacy/examples/sndharmonics6_3700p1-300-1298060823.ogg
```

For v10 there is no recording made with the Linux generator, so the identity check
above is all there is.

## What the traces show about the rules

Measured on 2026-10-01. Besides the reference runs, the traced programs were run on 60
seeds at each of 32, 33, 60, 100, 200, 300 and 400 cycles (420 runs per version), and
their tone lists compared with a throwaway composer written from the plan's section 9
and Appendix A. All 1,260 lists agree.

- **v5 is v4 before the end phase.** The v4 traced program, given the v5 seeds and
  cycles, produces the two v5 lists exactly, and did so on 100 further combinations of
  seed and cycles.
- **When a level draw is made** (always after the repetition draw, before the jitter
  draw):

  | | v4 and v5 | v6 | v10 |
  |---|---|---|---|
  | Some tone is counted (`d` > 0) | draw | draw | draw |
  | No tone is counted (`d` = 0) | draw; the level is 1 whatever was drawn | cannot happen | **no draw**; the level is 0.7 |

  In v6 every tone has a level above 0 and so is counted: `d` is 1, 2, .. 10 at ticks
  1..10 and 11 from then on. In v10, `d` = 0 happened for 217 of 60,122 tones.
- **`d` at that moment** is the number of slots whose tone is counted, including the
  tone about to be replaced and tones on their zero-gain strike, exactly as for v4. The
  repetition draw is modulo `d + 2` while `d` < 10, else modulo 11, in every version.
- **The order of the arithmetic matters for the last digit of a level.** The old code
  computes `sin((PI * c) / (T - 31))`, multiplying before dividing; in v10 this is
  then multiplied by 2.5 and held at 1; then it is squared. The level is
  `(150 * (density + 0.05)) / (1 + r)`, held at 1. Dividing `c / (T - 31)` first
  changes the last digits of 33 of the 451 levels in the two v6 lists and of 29 of the
  562 levels in the three v10 lists (for example 0.1052610484206006 instead of
  0.10526104842060062). It changes nothing else in them.
- **v10's `> 0.69` test is not a close call.** Over the 60,122 tones the closest level
  to 0.69 was 0.690058, a distance of 0.00006. Rounding differences between computers
  sit in the sixteenth digit.
- **The first tone under the v10 rules** lay outside 100..3000 Hz in 242 of 420 runs.
  The redraw of the next tone's ratio always finds a ratio that brings it into range.

## The two random generators

- **Linux (glibc)**: all 2011 examples (`sndharmonics4/5/6_*`).
- **Apple/BSD**: all 2022 examples (`out_*.ogg`). The state `x` starts as the seed.
  Before each draw, an `x` of 0 is replaced by 123459876; then
  `x = 16807 * x mod 2147483647`, and `x` is the output.

Known answers: glibc with seed 1 gives 1804289383, 846930886. Apple/BSD with seed 1
gives 16807, 282475249, and with seed 2147483647 gives 0, 520932930, 28925691 (the
last follows Apple's published code and has not been checked on a Mac).

The same seed gives a different tune on each generator, so a tune's record must name
its generator. The new program has only the Linux generator; the Apple rule is kept
here as a record.
