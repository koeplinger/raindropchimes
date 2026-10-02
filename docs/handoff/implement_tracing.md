**Tracing of v5, v6 and v10 is complete: every check passed.** Two things are left for you: a stray `docs/reference/__pycache__/` I could not remove, and the old prelude header I could not delete.

## Files written (all in `/home/jens/Projects/raindropchimes/docs/reference/`)

- **Patches:** `v5_trace.patch`, `v6_trace.patch`, `v10_trace.patch`. Each applies with `patch -p1` to a pristine source directory. The traced program runs as `./harmonics_trace [seed [cycles [binsPerSlot]]]` and writes `out.wav` and `events.txt`.
- **Tone lists** (ten columns, v4 layout, comment header saying how each was made, cut before tick 11·(T−30)):
  - `tune5_seed1297975154_cycles400.tones.txt`: 571 tones, last tick 4068
  - `tune5_seed1297981226_cycles200.tones.txt`: 257 tones, last tick 1869
  - `tune6_seed1298060492_cycles300.tones.txt`: 212 tones, last tick 2940
  - `tune6_seed1298060823_cycles300.tones.txt`: 239 tones, last tick 2947
  - `v10_seed1654607101_cycles200_glibc.tones.txt`: 187 tones, last tick 1861
  - `v10_seed1655110192_cycles200_glibc.tones.txt`: 174 tones, last tick 1866
  - `v10_seed1655110280_cycles200_glibc.tones.txt`: 201 tones, last tick 1852 (an addition, see judgement calls)
- **`trace_to_tones.py`** (extended): reads all four versions' logs, same usage `events.txt [cycles]`. It now prints the level with the same shortest-digits rule as `score_text.c`, and stops with an error if the log's own end-phase flag or parent frequency disagrees with what it derives.
- **`compare_with_recording.py`** (new, needs `ffmpeg` and `numpy`): frame counts and per-channel correlation of `out.wav` against an OGG.
- **`README.md`** (rewritten): a row per file, a recipe per version, the event-log fields, both proofs with result tables, and a section on what the traces show.

Unchanged: `tune4_seed1297735820.tones.txt`, `v4_trace.patch`, `glibc_rand.c`, `legacy/`, the plan, and everything outside `docs/reference/`.

## Checks and results

1. **Traced audio equals untraced audio.** For all seven runs, `cmp` finds the traced `out.wav` byte-identical to an unpatched build whose only edits are the seed, `binsPerSlot` and `totalSongTime` constants.

   | Version | Seeds | `out.wav` bytes |
   |---|---|---|
   | v5 | 1297975154 | 29,937,644 |
   | v5 | 1297981226 | 6,168,844 |
   | v6 | 1298060492, 1298060823 | 50,826,644 each |
   | v10 | 1654607101, 1655110192, 1655110280 | 30,280,844 each |

2. **v5 and v6 match the 2011 recordings.** Full length, identical frame counts, best time shift 0 in every case.

   | Recording | Frames | Left | Right |
   |---|---|---|---|
   | `sndharmonics5_1700-400-1297975154.ogg` | 7,484,400 | 0.99941 | 0.99938 |
   | `sndharmonics5_700-200-1297981226.ogg` | 1,542,200 | 0.99921 | 0.99908 |
   | `sndharmonics6_3700p1-300-1298060492.ogg` | 12,706,650 | 0.99933 | 0.99915 |
   | `sndharmonics6_3700p1-300-1298060823.ogg` | 12,706,650 | 0.99891 | 0.99888 |

3. **v5 is v4 before the end phase.** The v4 traced program with the two v5 seeds and cycle counts gives the two v5 lists exactly (571 and 257 lines). It also agreed on 100 further seed/cycle combinations (12,265 tones, 0 differences).
4. **The plan's rules reproduce every trace.** A throwaway Python composer written only from section 9 and Appendix A (v4 rules for v5) equals all seven reference lists. It also equals the traced programs on 420 runs per version (60 seeds at each of 32, 33, 60, 100, 200, 300, 400 cycles; 80,068 / 48,234 / 60,122 tones) with 0 mismatches.
5. **Tempo does not affect the tone list.** `binsPerSlot` 7 and 1700 give the same list in all three versions; the sweep ran at 7.
6. **The slot-table count of `d` equals the audio-loop count.** The patches abort if they ever differ; they never did.
7. **README recipes run literally.** Every code block was extracted and run with `sh -e` in a scratch copy of `docs/` and `legacy/`: all six pass. That includes the unchanged v4 recipe with the new `trace_to_tones.py` (625 lines reproduced).

## Plan section 9 against the traces

Every row of the rules table is confirmed for v6 and v10: frequency range (100..3000 inclusive in v10), repetition tables, first-tone draw (`rand() % 4` in v10), counted-if (`> 0.` in v6, `> 0.69` in v10), first level 0.7, and the continuous level rule drawn after the repetition draw and before jitter.

**When a level draw is made, and `d` at that moment:**

| | v4 / v5 | v6 | v10 |
|---|---|---|---|
| `d` > 0 | draw | draw | draw |
| `d` = 0 | draw; level 1 regardless (977 cases in the sweep) | cannot happen | no draw; level 0.7 (217 of 60,122 tones) |

`d` includes the tone being replaced and tones on their zero-gain strike. The repetition modulus is `d + 2` while `d` < 10, else 11, with 0 violations in every version.

**What the plan leaves out or states imprecisely:**

- **Arithmetic order of the density.** Appendix A writes `density = sin(pi * c / (T - 31)) ^ 2` and lists "`c/(T−31)`" among the divisions, which invites dividing first. The source (v6 `lib-harmonics-dingdong.h:310`, v10 `harmonics-dingdong.c:313`) is `density = sin(PI * ((double) songTime) / ((double) (totalSongTime - 31)));`, which is `(PI*c)/(T-31)`. Dividing first changes the last digits of 33 of 451 v6 levels and 29 of 562 v10 levels (e.g. 0.1052610484206006 instead of 0.10526104842060062), and nothing else. To match bit for bit, follow the source's order:
  - `s = sin((PI*c)/(T-31))`
  - v10 only: `s = 2.5*s; if (s > 1) s = 1`
  - `density = s*s`
  - `level = (150.*(density+0.05))/(1.+r); if (level > 1) level = 1`
- **v6's "level when nothing is counted: 0.7" is unreachable before the end phase.** Every v6 level is above 0, so `d` is 1..10 at ticks 1..10 and 11 from then on. No v6 list can test that branch.
- **The requested v10 seeds do not test it either.** Neither 1654607101 nor 1655110192 ever reaches `d` = 0 before the end phase.
- **The `> 0.69` margin** (sections 5.2 and 12 say "still has to be measured"): the closest level over 60,122 tones was 0.690058, a distance of 5.8e-5.
- **"Never below 0.075"** holds; the smallest level in the reference lists is 0.0758.
- **The v10 first tone** lay outside 100..3000 Hz in 242 of 420 runs. The redraw always terminates, since ×1/5 or ×2 reaches the range from anywhere in 50.7..14,720.
- **Section 12's sentence** "for v5, v6 and v10 there is only a header that logs every random draw" is now out of date; I may not edit the plan.

## Deviations and judgement calls

- **Third v10 list added (seed 1655110280).** It is the seed of `out_1655110280_5500_200.ogg` and reaches `d` = 0 at tick 24 (no level draw, level 0.7, repetition draw modulo 2). Drop it if unwanted.
- **v5 also takes `binsPerSlot`**, because its second example was made at 700; the brief named only v6 and v10.
- **v10 lists were traced at `binsPerSlot` 3440** (the source's value), although two of the 2022 file names say 5500.
- **Event-log field names differ slightly from v4's**: `d=`, `modCnt=`, `r25all=` (every ratio draw, rejects first), `endphase=`, and `r100play=-` when no draw was made. The converter reads both layouts.
- **Recipes leave their working directories in the repo root**, as the v4 recipe already did; the README says to delete them. I added a closing `cd ..` to the v4 recipe.
- **The margin, sweep and model figures come from scratch scripts**, not repo files. The README records them as measurements.

## Not finished

- **`later_versions_trace_prelude.h` is still there.** `rm` inside the repo was denied by the permission system. The README now marks it "superseded; nothing uses it". Delete it and its README row if you want it gone.
- **`docs/reference/__pycache__/` is a stray untracked directory** (two `.pyc` files) created by my own `py_compile` check. Removal was denied too; please run `rm -r docs/reference/__pycache__`.

Scratch tools are in `/tmp/claude-1000/-home-jens-Projects-raindropchimes/4e199ce1-d2a0-47d9-bb6f-9e21b46c8d88/scratchpad/impl_tracing/`: `make_patches.py`, `run_one.sh`, `inject_seed.py`, `plan_model.py`, `sweep.py`, `v4_vs_v5.sh`, `pi_order.py`, `write_lists.py`, `test_recipes.py`.

Unrelated to this track: the claude.ai Google Drive connector is not authorized in this session. It is unavailable until the user authorizes it in their claude.ai connector settings.