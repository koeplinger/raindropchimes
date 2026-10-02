All eight deliverables are written and my eight checks pass, also under `-fsanitize=address,undefined`. One pass mark is not met as the brief states it: with reverb, loudness per cycle at 48 kHz differs from 44.1 kHz by up to 6.3 %, not 1 %. That and one addition beyond the plan are the two things to decide on.

The code compiles without warnings under the required flags. `src/main.c` also builds against my modules without warnings (compiled into scratch only; I did not run `make`).

## Decisions for you

**1. Loudness per cycle with reverb misses 1 % at 48 kHz.** The cause is the whole-frame rounding of the reverb delays, which the brief prescribes: at 48 kHz the taps land up to 8 µs off and the reverb's resonances shift.
- Evidence: at 88.2 kHz, where every delay is an exact multiple, the same comparison gives 0.009 %.
- A scratch experiment with interpolated (fractional) delays brought the worst cycle to 1.5 %, so even that does not strictly reach 1 %. I did not put it in.
- What the check does instead: 1 % per cycle for the voices alone (measured 0.038 %), and with reverb 10 % per cycle (measured 6.3 %, cycle 95) plus 1 % for the whole piece (measured 0.11 %).
- Plan section 7 item 7 needs either this wording or a different reverb; say which.

**2. I added exact-time phase alignment to the voice, which the plan does not have.** When a voice's phase changes speed at a tick, it is corrected by the fraction of a frame between the tick's exact time and its frame (`frames_late`, passed from `render.c` to `voice_start_tone` / `voice_next_strike`).
- Why: without it the run-on phase depends on frame rounding. Near-unison tones (52.23 and 51.92 Hz in cycles 370–375 of the favourite) then beat differently at 48 kHz, and loudness per cycle differed by up to 60 % even without reverb.
- With it: 0.038 % per cycle, and identical peak (0.47627) at both rates.
- At 44.1 kHz with whole-sample tempos it does nothing: the favourite's WAV is byte-identical with and without it.
- It is about 15 lines in `voice.c` and one small function in `render.c`, easy to remove if you do not want it.

## Check results

| Check | What it proves | Key numbers |
|---|---|---|
| `check_40_timing` | Closed form equals a slot-by-slot sum for four tempos (with, without and negative drift; 7 slots) | Tick n is frame 1701·n for n = 0..5500; v6 tempo, 300 cycles = 12,706,650 frames |
| `check_42_reverb_click` | Delays, gains and swaps for first and second echoes, against taps typed in from plan section 9 | Hand values match (e.g. frame 19000 left = 0.27); block size does not matter; 48 kHz delays 7619, 10340, 13061, 18503, 20680 |
| `check_44_single_tone` | The favourite's first tone, dry; the voice against Appendix B written out a second time; phase rules | 1299.5448 Hz (expected 1299.5452); pan 0.260000; peak 0.014009 against loudness law 0.014090; Appendix B difference 3.9e-12; phase runs on / holds / restarts to under 1e-12 |
| `check_46_render_errors` | Unknown names and unusable settings are refused with a clear message; invalid numbers are never written | Runaway reverb fails after 45,335 valid frames; everlasting reverb stops at exactly 60.000 s; half gain gives half peak |
| `check_48_sinks` | WAV rounding, saturation count, header; raw frames to file and to standard output | 3 of 12 test samples saturate as expected; raw to `-` gives the same bytes as to a file |
| `check_50_render_favourite` | Plan section 7 item 5 | 9,168,440 frames = 207.901 s; music ends 199.376 s; ring-out 8.525 s; peak 0.50852; 0 saturated; 261,069 non-silent frames after 200.34 s; last 19000 frames silent; header correct; two runs identical |
| `check_55_likeness` | Plan section 7 item 6 (exits 77 without ffmpeg) | Best shift 0 on both channels; overall 0.99909 left, 0.99921 right; worst cycle 0.9935 (left, cycle 447) and 0.9936 (right, cycle 400) |
| `check_60_sample_rates` | Tick starts measured from audio at 44.1 and 48 kHz; loudness; length | All 16 measured ticks on the nearest frame and within one sample of each other; lengths 0.000 s apart; loudness as in decision 1 |

One pass over the favourite takes about 1.5–1.7 s.

## Other judgement calls

- **Strike age `u`:** the fraction of the slot is frames into the slot divided by the slot's frame count, not exact time. So `u` runs exactly 0..S and is never negative.
- **Ring-out:** the quiet stretch is counted from the end of the music only, and must be strictly longer than the longest delay. The piece therefore ends with longest + 1 quiet frames, and a patch without effects gets one frame of ring-out. Hitting the 60 s limit cuts the piece without an error.
- **End of music:** derived from the patch's strike-gain law (the last strike with non-zero gain of any tone with level above 0), not hard-wired to strike N−1.
- **Extra refusals in `render_score`:** it runs `score_check` first, and refuses a sample rate below 1, a non-finite gain, and a timing whose slot length reaches zero in any cycle (negative drift).
- **Vibrato period:** a patch with a period of zero or less is refused, even if its depth is 0.
- **Reverb:** `create` returns NULL if a tap rounds to less than one frame or there are too many taps. Stability (gains summing below 1) is not checked; a runaway is caught by the invalid-number check.
- **WAV:** path `-` is refused, since the header needs a seek. A sample of exactly 1.0 becomes 32767 and counts as saturated. A block containing an invalid number is rejected whole.
- **Raw:** always lowest byte first (ffmpeg's `f32le`) on any machine; it never saturates.
- **Tests:** `fail()` stops a helper at its first failure, so helpers hold no state. The two favourite checks delete their large scratch files on success.

## Interface notes (no header change requested)

- `Effect.create` and `SinkFormat.open` return NULL without a reason, so the messages are generic ("could not be set up", and whatever `main.c` prints).
- `main.c` passes `(int) plan->rate` to the sink while the render uses the double; a non-integer `--rate` would give a WAV header that disagrees with the audio.

## Files

All under `/home/jens/Projects/raindropchimes/`. Nothing outside my track was touched and no git state was changed.

- `src/synth/`: `timing.c`, `shapes.h`, `shapes.c`, `voice.h`, `voice.c`, `reverb.c`, `effects.c`, `patch.c`, `render.c`
- `src/output/`: `formats.h`, `sink.c`, `wav.c`, `raw.c`
- `tests/`: `synth_common.h`, `synth_common.c`, `synth_timing.c`, `synth_reverb_click.c`, `synth_single_tone.c`, `synth_render_errors.c`, `synth_sinks.c`, `synth_favourite.c`, `synth_likeness.c`, `synth_rates.c`
- `tests/`: `check_40_timing.sh`, `check_42_reverb_click.sh`, `check_44_single_tone.sh`, `check_46_render_errors.sh`, `check_48_sinks.sh`, `check_50_render_favourite.sh`, `check_55_likeness.sh`, `check_60_sample_rates.sh`

New headers of my own: `shapes.h`, `voice.h`, `formats.h` (declares `WAV_FORMAT` and `RAW_FORMAT`), and `tests/synth_common.h`.

Nothing is unfinished.