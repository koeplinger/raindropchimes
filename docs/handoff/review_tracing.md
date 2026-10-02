VERDICT: The tracing track holds up. Every substantive claim in the implementer's report reproduced under my own measurements; the four findings are minor (one stray directory, one check that proves nothing, two documentation points).

All work was done in /tmp/claude-1000/-home-jens-Projects-raindropchimes/4e199ce1-d2a0-47d9-bb6f-9e21b46c8d88/scratchpad/impl_tracing_review on a copy of the tree without .git; the repository was not touched.

**1. README recipes, run literally.** I extracted every code block of docs/reference/README.md and ran each with `sh -e` from the root of the scratch copy.
- v4, v5, v6 and v10 recipes all exit 0 with empty diffs; the v4 one gives 625 lines with the new trace_to_tones.py.
- The "untraced" recipe's `cmp` prints nothing.
- The compare recipe prints 12,706,650 frames for both, 0.99891 left, 0.99888 right, shift 0.
- A second run of the v5 and v10 recipes over existing directories also gives empty diffs.

**2. Patches and audio.**
- **Clean apply:** v5, v6 and v10 patches apply to pristine sources with `patch -p1 --fuzz=0`, no offsets. Compiling patched sources with `-Wall -Wextra` shows no warning from an added line.
- **Traced = untraced:** for all seven tunes the traced out.wav is byte-identical (`cmp`) to an unpatched build. I injected the seed by `sed` on the `seedTime = time(NULL);` line; the v5 700/200 tune also needed the two song constants.

  | Version | Seeds | out.wav bytes |
  |---|---|---|
  | v5 | 1297975154 | 29,937,644 |
  | v5 | 1297981226 | 6,168,844 |
  | v6 | 1298060492, 1298060823 | 50,826,644 each |
  | v10 | 1654607101, 1655110192, 1655110280 | 30,280,844 each |

- **Match to the 2011 recordings:** my own correlation script (mycorr.py, full length, shifts of ±50 searched) gives equal frame counts and best shift 0 in all four cases.

  | Recording | Left | Right |
  |---|---|---|
  | v5, seed 1297975154 | 0.99941 | 0.99938 |
  | v5, seed 1297981226 | 0.99921 | 0.99908 |
  | v6, seed 1298060492 | 0.99933 | 0.99915 |
  | v6, seed 1298060823 | 0.99891 | 0.99888 |

  Every tenth of every file is at or above 0.9975. Correlating one v6 tune against the other recording gives 0.0015.

**3. Independent re-derivation.** I wrote sim.c, a tick-level simulation with no audio loop, from the legacy sources and glibc_rand.c.
- It reproduces all seven new lists and the v4 list in full, not only 40 lines: 212, 239, 187, 174, 201, 571, 257 and 625 lines, 0 differences.
- On the implementer's 420 seed/cycle combinations per version it agrees with the traced programs run at binsPerSlot 7:

  | Version | Tones | Mismatches | d = 0 before the end phase |
  |---|---|---|---|
  | v5 | 80,068 | 0 | 977 |
  | v6 | 48,234 | 0 | 0 |
  | v10 | 60,122 | 0 | 217 |

**4. The lists themselves.**
- Each stops before tick 11·(T−30); last ticks are 4068, 1869, 2940, 2947, 1861, 1866 and 1852.
- Every line has ten columns, and ticks increase.
- Every level reads back to the exact double and is the text that score_text.c's `format_shortest` produces (checked with a C copy of that loop).
- Header statements are true: tone counts, frame counts, correlations and first-tone frequencies (324.9 Hz and 56.5 Hz) all match.
- The v6 header's statement that d runs 1..10 at ticks 1..10 and is 11 afterwards is true.
- In the v10 lists d = 0 occurs only at tick 24 of seed 1655110280; the README's sample event line is verbatim from that log.
- The v4 traced program reproduces both v5 lists, and matched the v5 program on 20 further combinations.

**5. Cross-check of plan section 9.** Each claim checked against the source is correct.
- **Density order:** the source multiplies first (v6 lib-harmonics-dingdong.h:310, v10 harmonics-dingdong.c:313). Dividing first changes 18+15 = 33 of 451 v6 levels and 7+6+16 = 29 of 562 v10 levels, and nothing else. The example in the README is tick 38 of seed 1298060492. src/compose/slotwalk.c:56 already multiplies first.
- **v6's 0.7 fallback** is unreachable before the end phase: every level is at least 0.075, and d never falls below 1.
- **v10 seeds 1654607101 and 1655110192** never reach d = 0.
- **The `> 0.69` margin:** the closest level is 0.69005802607769728 (seed 1298000400, 400 cycles, tick 140), a distance of 5.8e-5.
- **Smallest level** in the lists is 0.0758.
- **v10 first tone** lies outside 100..3000 Hz in 242 of 420 runs.
- **Rule table:** repetition tables, `rand() % 4`, the inclusive 100..3000 range, `> 0.` and `> 0.69`, and "no level draw when d = 0" all match the sources.

**Ground rules.** Only docs/reference files changed in this track; legacy/ and the plan are untouched, nothing is staged, and HEAD is still 1b1db58. Vocabulary and style are fine. The patches add three globals to the legacy program, exactly as the existing v4_trace.patch does; that is throwaway instrumentation, not new code.

**Left for the lead.** `later_versions_trace_prelude.h` is still present and marked "superseded" in the README; whether to delete it is your call.

--- [1] STYLE @ /home/jens/Projects/raindropchimes/docs/reference/__pycache__/
PROBLEM: A stray, untracked and un-ignored directory with two .pyc files sits beside the reference material. The implementer reported it; it is still there. An owner who commits with `git add -A` would commit it.
EVIDENCE: `ls -la docs/reference/__pycache__` shows compare_with_recording.cpython-312.pyc and trace_to_tones.cpython-312.pyc, dated Oct 1 16:34. `git status --short` lists `?? docs/reference/__pycache__/`. .gitignore has no `__pycache__` entry.
FIX: Run `rm -r docs/reference/__pycache__`. Optionally add `__pycache__/` to .gitignore.

--- [2] TEST-GAP @ docs/reference/v5_trace.patch, v6_trace.patch, v10_trace.patch (hunk 'TRACE: d is the count the audio loop made...'); docs/reference/README.md:149-150
PROBLEM: The patches' "d mismatch" abort can never fire, so "It never did" is not evidence. The README and the implementer's check 6 present it as confirmation that the slot-table count equals the audio-loop count. The conclusion is still true; this check is just not what shows it.
EVIDENCE: In the v6 source (lib-harmonics-dingdong.h:177-186) the audio loop sets `densityCnt` by counting slots with `slotFrequency[i] > 0 && slotPlay[i] > 0.`. The patch computes `dTable` with the identical predicate on the same arrays in the same loop iteration; nothing writes `slotFrequency` or `slotPlay` between line 186 and the patched line after 307. v5 (`== 1`) and v10 (`> 0.69`) are the same. The real evidence is a tick-level composer with no audio loop: my sim.c matches the traced programs on 1,260 runs with 0 mismatches.
FIX: Reword README lines 149-150 to say the equality is shown by the tick-level model comparison (1,260 lists agree). Keep the abort only as a guard, or drop the "It never did" sentence.

--- [3] ROBUSTNESS @ docs/reference/README.md:31-35 and :173-182
PROBLEM: The recipes leave five directories in the repository root, none of them git-ignored. The README's delete-when-done list names only four: `untraced/`, created by the identity recipe, is missing.
EVIDENCE: After running all README blocks in a scratch copy, the root holds harmonics.2011.02.14, harmonics.2011.02.17, harmonics.2011.02.18, v10_trace and untraced. README lines 33-35 list the first four only. .gitignore covers `*.wav`, `tests/bin/` and `tests/tmp/` but none of these directories, so their source files show up as untracked.
FIX: Add `untraced` to the list in the README. Better, have the recipes work under `tests/tmp/` (already ignored) or add the five directory names to .gitignore.

--- [4] SPEC-DEVIATION @ docs/REVAMP_PLAN.md section 12 (lines 663-667), Appendix A (lines 718 and 734-736), sections 5.2 and 12 (lines 322, 643-644)
PROBLEM: Three plan passages are now stale or imprecise; I verified each. The implementer could not edit the plan.
- Section 12 still says only a draw-logging header exists for v5, v6 and v10.
- Appendix A lists `c/(T−31)` among the divisions, which suggests dividing before multiplying by pi.
- Sections 5.2 and 12 say the v10 `> 0.69` margin is not yet measured.
EVIDENCE: - Three patches and seven lists now exist in docs/reference/.
- The source computes `sin(PI * ((double) songTime) / ((double) (totalSongTime - 31)))`, which multiplies first. My sim.c with the division done first differs in 33 of 451 v6 levels and 29 of 562 v10 levels (for example tick 38 of seed 1298060492: ...60061 instead of ...60062). src/compose/slotwalk.c:56 already multiplies first.
- The closest level to 0.69 over 60,122 v10 tones is 0.69005802607769728, a distance of 5.8e-5.
FIX: Lead to edit the plan: update the section 12 tracing bullet; write the Appendix A density as `sin((pi * c) / (T - 31))^2` and remove `c/(T−31)` from the division list or state the order; record the measured 0.69 margin of 5.8e-5.
