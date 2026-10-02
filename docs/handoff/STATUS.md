# Where the implementation stands

Last updated 2026-10-02. To resume after a stop, tell Claude: "resume the
implementation; read docs/handoff/STATUS.md". The plan itself is
[../REVAMP_PLAN.md](../REVAMP_PLAN.md); this file only records progress against it.

Nothing of the implementation is committed yet. `git status` shows new `src/`, `tests/`,
`Makefile`, `LICENSE`, new files in `docs/reference/`, and this folder.

## State on 2026-10-02, start of session

`make clean && make && make test`: **19 checks, all pass.**

- `./chimes score --preset v4 --seed 1297735820` gives the reference tone list exactly.
- `./chimes make --preset v4 --seed 1297735820` renders the favourite in about 3 s:
  207.9 s long, music ends at 199.4 s, peak 50.9 % of full scale.
- Likeness to the 2011 OGG: 0.99909 left, 0.99921 right, no time shift, worst cycle
  0.9935 (the plan's pass mark is met).

## In progress (second workflow, started 2026-10-02)

Four agents; if the session stops, each must be re-checked rather than assumed done.

| Agent | Work | Owns |
|---|---|---|
| Composer, part two | Apply composer review findings 3, 4, 5; add rule sets `v6` and `v10`; check against all eight reference lists (closes findings 1 and 2) | `src/compose/*.c`, `tests/check_1*`, `check_2*`, `tests/compose_*.c` |
| Synthesizer review | Independent review of `src/synth/`, `src/output/`, `src/main.c` and their checks (read-only) | nothing |
| Synthesizer fixes and later sounds | Apply the review's findings; envelope `v6`, patches `v6` and `v10`; likeness to the v5 and v6 recordings | `src/synth/*.c`, `src/output/*.c`, `tests/check_4*`..`check_6*`, `tests/synth_*` |
| Extension examples | Wave form `bell`, effect `soften`, patch `bell`, and `docs/EXTENDING.md` | same files as the row above, plus the guide |

The workflow script with the full briefs:
`~/.claude/projects/-home-jens-Projects-raindropchimes/4e199ce1-d2a0-47d9-bb6f-9e21b46c8d88/workflows/scripts/chimes-finish-core-and-later-versions-wf_ec972b9d-13d.js`
Its agent results land in
`~/.claude/projects/-home-jens-Projects-raindropchimes/4e199ce1-d2a0-47d9-bb6f-9e21b46c8d88/subagents/workflows/wf_ec972b9d-13d/journal.jsonl`.

## Still to do after that workflow (the lead's own work)

1. Read the four agent reports; apply the review findings that concern `src/main.c`
   and the lead's headers and checks.
2. Add presets `v5`, `v6`, `v10` to `src/main.c` (v5 = rules v4, patch v4, 1700 bins,
   400 cycles; v6 = 3700 bins, drift 1, 300 cycles; v10 = 3440 bins, 200 cycles), with
   command-line checks for them.
3. `make clean && make test`; everything must pass.
4. Render the favourite into `samples/` (seed 1297735820, 500 cycles) with its score,
   marked CC BY 4.0. Stored as OGG via `ffmpeg` (assumption recorded on 2026-10-01; the
   WAV is about 37 MB and `.gitignore` excludes `*.wav`).
5. Update the top-level `README.md`: how to build, run, and recreate a tune; licences.
6. A final independent review of the whole codebase against the plan; fix; re-test.
7. Delete this `docs/handoff/` folder and the resume memory when everything is done.

## Done so far

| Phase | State |
|---|---|
| 0. Foundations | Done. |
| 1. Composer | Implemented, reviewed; findings being applied (see above). |
| 2–3. Synthesizer, reverb, ring-out | Implemented; review running (see above). |
| 4. Everyday use | `src/main.c` and command-line checks written and passing; under review. README not yet updated. |
| 5. Later versions | Evidence done and reviewed (trace patches and 8 reference lists in `docs/reference/`). Rule sets and patches in progress (see above). |
| 6. Extension, proven | Raw output exists. Worked examples in progress (see above). |

Plan passages corrected on 2026-10-02 from the tracing review (finding 4) and the
synthesizer implementer's report: the density's multiply-then-divide order (Appendix A),
the measured `> 0.69` margin, the tracing bullet in section 12, the sample-rate pass
marks in verification item 7, and the phase alignment note in section 5.3.

## Decisions made during implementation

- `src/score/score.c` was added beside `score_text.c` (the plan's file list named only
  `tone.h` and `score_text.c`).
- There is no "list of algorithms": `main.c` calls the one composing function,
  `slotwalk_compose`, directly. A list can be added when there is a second algorithm.
- The recorded command uses `--rules` and `--patch` rather than `--preset`, so that it
  stays valid if a preset's defaults ever change.
- `chimes render` takes patch, bins and drift from the score's `# render:` lines unless
  `--preset` is given; sample rate and output gain come from the command line or the
  defaults.
- The automatic turn-down scales the peak to 32767/32768 of full scale, so the loudest
  sample converts without saturating.
- The voice shifts its phase by the fraction of a sample between a tick's exact time
  and its frame (added by the synthesizer implementer; kept). It does nothing at
  44.1 kHz with whole-sample tempos.
- `docs/reference/later_versions_trace_prelude.h` is kept: it is the only place the
  Apple generator is written down as C, which is the "legacy reference" of plan
  section 13, answer 3.
- Check numbering in `tests/`: 05 separation, 10–29 composer, 30–39 score, 40–69
  synthesizer and output, 70–89 command line.

## Where the earlier records are

- First workflow's reports and review findings: the other files in this folder.
- Full agent transcripts:
  `~/.claude/projects/-home-jens-Projects-raindropchimes/4e199ce1-d2a0-47d9-bb6f-9e21b46c8d88/subagents/workflows/`
- The scratch folder under `/tmp` does not survive; nothing needed is kept there.

This folder is working material. Delete it when the implementation is finished.
