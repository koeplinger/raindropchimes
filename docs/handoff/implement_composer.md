The composer track is finished: the favourite tune's 625 tone lines are reproduced digit for digit, and all four checks pass, both with the normal flags and under `-fsanitize=address,undefined`. Everything compiles without warnings under the required flags. No header changes are needed.

## Files written

All under `/home/jens/Projects/raindropchimes/`:

- `src/compose/rng.c` — the Linux generator, adapted from `docs/reference/glibc_rand.c`.
- `src/compose/rules.c` — the `v4` block of constants, `rules_find` / `rules_count` / `rules_at` / `rules_check`.
- `src/compose/slotwalk.c` — `slotwalk_compose()` per Appendix A, with both level rules.
- `tests/check_10_generator.sh` with `tests/compose_generator.c`
- `tests/check_11_favourite.sh` with `tests/compose_tones.c` and `tests/compose_compare.c`
- `tests/check_12_composer_properties.sh` with `tests/compose_properties.c`
- `tests/check_20_original_program.sh` (reuses the three helpers above)

## What each check proves

| Check | Result | Key numbers |
|---|---|---|
| `check_10_generator` | pass, 0.3 s | Seed 1 gives 1804289383, 846930886; re-seeding restarts; seed 0 equals seed 1. Identical to the host `rand()` on 4,012 seeds × 5,000 draws. |
| `check_11_favourite` | pass, 0.3 s | v4 / 1297735820 / 500 gives all 625 reference lines exactly. The score written, read back and written again is identical. Its non-comment lines equal `tests/data/favourite.score`. |
| `check_12_composer_properties` | pass, 1.1 s | 1,000 seeds at 500 and 400 cycles: 686,681 tones, 0 failures. 15 broken rule sets refused, each message naming the field. |
| `check_20_original_program` | pass, 7.5 s | The traced 2011 program and the new composer agree on 12345/500 (668 lines), 1297700000/500 (708), 42/200 (248), 2011/333 (433). |

The properties check covers, per tune:
- `score_check` passes, and the score records the rule set's name and slot count.
- Every parent held the previous slot when its child was created.
- Frequencies are in range (root excepted) and every ratio is in the table.
- No tone is created in the end phase.
- Each slot is refilled at exactly the tick it became free, and none is left free before the end phase.
- The repetition count lies within the reach that `d` allows.
- 400 cycles gives a different tone list from 500 for every seed.
- Composing again with copied rules, a reused generator and score, and another tune in between gives the same tones.

It also checks the refusals:
- cycles = `end_cycles + 1`, 0 and negative are refused; `end_cycles + 2` is accepted.
- A non-empty score is refused.
- An unreachable frequency range makes the ratio redraw give up after 10,000 tries.

Five test-only rule variants (100 seeds each) exercise the generic fields: the continuous level rule, a v10-like set, 7 slots, 1 slot, and `end_cycles` 0.

I also broke the composer eleven ways in scratch to see whether the checks notice. Nine were caught. A too-low count of `d` is invisible to the properties check and caught only by the favourite check. A truncated PI (3.14159265) is caught by neither, which is the wide margin section 12 of the plan describes.

## Evidence beyond the shipped checks (scratch only)

- 240 further seed/cycle combinations against the traced v4 original, including 32 and 33 cycles: all identical.
- The generic composer, given v6-like and v10-like rule sets built only from `Rules` fields, reproduces all seven reference lists another engineer added to `docs/reference/` during this session (two v5, two v6, three v10), digit for digit. So `rules.h` is sufficient for v6 and v10 as one more block of constants each.

## Judgement calls and deviations

- **`counted_above` for v4 is 0.0.** Levels are only 0 and 1 there, so "above 0" means "is 1".
- **On/off rule when the draw fails and nothing is counted:** I return `lone_level`, where the comment in `rules.h` says "level 1". Identical for v4 (`lone_level` is 1).
- **Extra limits in `rules_check`:** slots 1..1000 and each repetition count 1..1000, so tick arithmetic (including `score_tone_end`) cannot overflow 32 bits. It also checks:
  - the name is one word shorter than `SCORE_NAME_LEN`;
  - ratio numbers are at least 1;
  - `jitter_range` is at least 1 (otherwise a modulo by zero);
  - `end_cycles` and `reach_threshold` are not negative;
  - gain, scale and offset are finite and not negative;
  - the three levels are within 0..1.
- **Reach rule as implemented:** `first_reach` and `full_reach` must be within 1..`n_reps`, and `min(slots, reach_threshold − 1) + 2` must not exceed `n_reps`. This passes v4 and the `pp_18` shape, and refuses `pfib_11` (threshold 17 with 11 entries).
- **Extra refusals in `slotwalk_compose`:** `slots × cycles` above 2,000,000,000, and a score that already holds tones. On any error the tone list is left empty. A defensive error for "previous slot never held a tone" exists but cannot occur.
- **Fixed in the algorithm, not rule fields:** the root frequency law, pan as U(101)/100, and the level draw as U(100).
- **Favourite check is exact on Linux only.** Elsewhere (`uname -s`) it lets the frequency column differ beyond 12 digits and prints a note, per section 7 item 1. That path is tested only with doctored files; I have no non-Linux machine here.
- **Host comparison is decided at run time:** the generator check compares with the host only if the host `rand()` gives the known answers for seed 1, rather than testing `__GLIBC__`.
- **Cross-check skips (exit 77)** without `tar`, `patch` or `python3`, without the archive, when the host `rand()` is not the Linux one, or when the original does not build. It builds the original with `$CC -O2 -w`, not the project flags, and deletes the 37 MB `out.wav` after each run.

## Not finished

- Stale scratch from my first run is still in `tests/tmp/`: `favourite.{composed,diff,expected,kept,reread,written,written_lines}` and `original_v4/`. I renamed my scratch to `compose_favourite.*` and `compose_original_v4/` afterwards, and my `rm` of the old ones was denied. They are gitignored; `make clean` removes them.
- No compiler other than gcc 13 was tried (clang is not installed). The favourite is identical at `-O0`, `-O2`, `-O3` and `-march=native`.
- `make` and `make test` were not run, since `src/main.c` does not exist yet; each check was run on its own from the repo root.