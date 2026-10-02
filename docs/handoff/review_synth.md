VERDICT: The synthesizer and the output formats hold up: I found nothing wrong in src/synth or src/output. All the problems I can demonstrate are in src/main.c (path handling, the recorded command, render notes), plus five regressions the checks would miss.

**What I ran**
- **The checks:** check_44 to check_80 pass in the repo, run one by one. All 13 (check_40 to check_80) pass on a snapshot copy, also under -fsanitize=address,undefined, with no leak reports. check_82 (not in my scope) failed in the repo because the binary predated the v6/v10 rule sets.
- **Key numbers reproduced:** favourite 9,168,440 frames = 207.901 s, music ends 199.376 s, peak 0.50852, 0 saturated; likeness 0.99909 / 0.99921 at shift 0, worst cycle 0.9935.
- **Snapshot:** other engineers changed files during the review (main.c gained presets v5/v6/v10), so the experiments ran on a copy taken at 09:22. main.c, src/synth and src/output were unchanged in the repo when I finished.

**Synthesizer and output: sound**
- **Plan comparison:** voice, envelope, reverb, ring-out and 16-bit conversion match sections 5.3, 5.4 and Appendix B line by line.
- **frames_late is correct.** I recovered the phase of every frame through a probe wave form and compared it with the exact-time integral.
  - Score used: tones following at once, a gap, a tracked tone, the same frequency twice.
  - Settings: five tempos including drift and fractional bins, seven sample rates from 1000 to 96000 Hz, with and without phase_restarts.
  - Worst error 6.4e-11 rad; the phase always stayed within 0..2π.
  - At 44.1 kHz with whole-sample tempos the WAV is byte-identical with the correction removed (favourite, 3700 bins with drift 1, and two others). It differs, as it should, at 48000, 22050 and with fractional bins.
- **Drift and other rates:** with 3700 bins and drift 1 at 44100, 48000, 22050 and 96000 Hz, u stays in [0, 10.9999], the envelope is never negative, and the music ends on the frame timing_tick_frame gives. Single tones at 10 ticks, 3 tempos and 4 rates start and end on exactly the expected frame.
- **Edge scores behave:**
  - last tone tracked; a tracked root only (19,001 silent frames); level 0.3 (exactly 0.3 times level 1); jitter -1; reps 1;
  - frequencies from 0.001 Hz to 1e300 Hz give finite output; 1e308 and 1e-310 Hz are refused with nothing written;
  - two reverbs in a row equal applying the effect twice by hand.
- **Ring-out rule:** for 13 settings the length equals last loud frame + longest delay + 1. That covers gains 0, 1e-9, 0.5, 1000, 1e13 and -1, sound on one channel only, and 48 and 22.05 kHz.
- **Sinks:** the saturation count matches an independent count (469,998 at gain 5000). WAV headers are correct at 1000, 48000 and 768000 Hz and for an empty file. /dev/full, a closed standard output and an unwritable path each give exit 1 with a message.
- **Automatic turn-down:** seed 1297700025 gets gain 0.97239594005535; the samples span -32767..30785 with none held at the limit, and the recorded command reproduces the WAV and the score byte for byte.
- **Separation and state:** no global mutable state (nm shows only const tables), no includes of compose/, no generator.

**Mutations**
- 30 of 35 mutations on scratch copies were caught. These included cosine before increment, phase reset per tone, wrong tap gain, envelope off by a slot, threshold doubled, WAV size fields and turn-down removed.
- The five that were missed are in the test-gap findings.

No header change is requested.

--- [1] BUG @ src/main.c:325 and :493 (render_and_record, command_render)
PROBLEM: The recorded command is not shell-quoted, so a path containing a space (or any shell character) gives a command that does not recreate the file. The plan requires 'a complete command that recreates the file'.
EVIDENCE: chimes make --seed 5 --cycles 40 --bins 300 -o "my tune.wav" records '# render: command chimes make ... --gain 1 -o my tune.wav'. Running it with sh -c gives "chimes: unexpected argument 'tune.wav'", exit 1. 'chimes render "my tune.score"' records 'chimes render my tune.score ... -o my tune.wav', broken the same way.
FIX: Quote each path when building the command: wrap it in single quotes (writing ' as '\'') unless it consists only of [A-Za-z0-9_./+-]. Add a path with a space to tests/check_75_cli_record.sh.

--- [2] BUG @ src/main.c:357, :437, :275 (audio_path[512], score_path[512], command[512])
PROBLEM: Paths and the recorded command are cut silently to fit fixed buffers. With an output path over 511 characters the audio is lost and the program reports success. With shorter but long paths the recorded command is cut and, when run, writes to the wrong files.
EVIDENCE: (a) -o with a 548-character path (4 nested directories of 120 characters, file of 60 u's + .wav): exit 0, and exactly one file exists, 513 characters long, starting '# raindropchimes score 1'. Audio and score were both cut to the same name and the score overwrote the audio. (b) A 327-character -o path, then 'chimes render' of its score: the recorded command is 511 characters and ends '-o ddd.../ddddddddd'. Running it exits 0 and writes audio and a new score under that cut name, one directory up.
FIX: Check every snprintf result against the buffer size and stop with 'path too long' rather than continue. If the complete command does not fit in a render note (512 characters, src/score/tone.h), say so and do not record a cut one.

--- [3] BUG @ src/main.c:381 and :487 (score path derived from -o)
PROBLEM: When -o ends in .score, the audio path and the score path are the same file. The audio is written, then overwritten by the score, with exit 0. With 'render in.score -o in.score' the input score is first overwritten with audio.
EVIDENCE: chimes make --seed 5 --cycles 40 --bins 300 -o tune.score prints 'tune.score: 13.3 s ...' and exits 0; tune.score is 1522 bytes of score text and no audio exists. 'chimes render in.score -o in.score' behaves the same. Related oddity: -o sub/.hidden writes the score to sub/.score.
FIX: After working out both paths, refuse with a message if they are equal. In with_extension, treat a dot directly after the last '/' like a leading dot.

--- [4] BUG @ src/main.c:101-105 (parse_number), :423-427 (noted_number), :476-477
PROBLEM: parse_number stores into *value before it knows the text is valid, so a '# render: bins' or 'drift' note that is not a number overwrites the preset default instead of leaving it. Noted values also skip the range checks that --bins and --drift get.
EVIDENCE: Each case is a 40-cycle score with one note line added. (a) '# render: bins abc': exit 0, 'music 0.0 s', score rewritten with 'bins 0'. (b) 'bins 17OO': rendered with bins 17. (c) 'bins 0' or 'drift -2': accepted, but the recorded command then contains --bins 0 / --drift -2, which the command line refuses ('--drift must be 0 or more'). (d) 'bins 1e11': no output, killed by timeout after 20 s. (e) 'bins 1e300': exit 0 with a 0.4 s silent WAV; a build with -fsanitize=float-cast-overflow reports main.c:90 '1e+300 is outside the range of representable values of type long'.
FIX: In parse_number, parse into a local and assign only on success. Give noted bins and drift the same limits as the options (one shared function) and stop with a message naming the note when one is invalid.

--- [5] ROBUSTNESS @ src/main.c:415-417 (command_score)
PROBLEM: A write error on standard output goes unnoticed for a small score: it is still in the stdio buffer when ferror is checked, and stdout is never flushed.
EVIDENCE: 'chimes score --seed 5 --cycles 40 > /dev/full; echo $?' prints 0. The favourite (33 KB) and '-o /dev/full' both give 1.
FIX: When file is stdout, call fflush(stdout) and count a failure as a write error before returning.

--- [6] ROBUSTNESS @ src/main.c:303-312 (render_and_record)
PROBLEM: With --gain given, the output file is opened (and so emptied) before render_score has checked the settings. A refused render destroys an existing audio file, and a render that fails part-way leaves a partial WAV behind. Without --gain the measuring pass catches the refusal first.
EVIDENCE: keep.wav exists with 3,010,116 bytes. 'chimes render neg.score --gain 1 -o keep.wav' (neg.score carries '# render: bins -5') exits 1 with the timing message and leaves keep.wav at 44 bytes. The same command without --gain leaves keep.wav untouched.
FIX: Remove the audio file when render_score fails after the sink was opened. That also covers the partial-file case.

--- [7] ROBUSTNESS @ src/main.c:173, :179 (and :90)
PROBLEM: The option value is cast to int before its range is checked. For a value outside int range this is undefined behaviour in C99. It happens to give the right refusal on x86.
EVIDENCE: Built with -fsanitize=address,undefined,float-cast-overflow: 'chimes score --cycles 1e30' gives 'main.c:173:13: runtime error: 1e+30 is outside the range of representable values'; '--cycles inf' the same; 'chimes make --rate 1e30' and '--rate -inf' at main.c:179.
FIX: Test the range first, then the whole-number test: number < 1.0 || number > 1e7 || number != floor(number). Same for --rate, and in number_text put the ±1e15 test before the cast.

--- [8] TEST-GAP @ tests/check_80_cli_turn_down.sh, tests/check_75_cli_record.sh
PROBLEM: No check runs the recorded command of a turned-down tune, so nothing proves the recorded gain is exactly the gain applied. No check reads the sample rate in the WAV header of a file made with --rate.
EVIDENCE: Mutation M19 (the gain note and command written with "%.6g", giving 0.972396 instead of 0.97239594005535) passes all 13 checks. Mutation M29 (main.c:303 opening the sink with 44100 instead of plan->rate) also passes all 13. Unmutated, I confirmed by hand that the recorded command for seed 1297700025 reproduces loud.wav and loud.score byte for byte.
FIX: In check_80, run the recorded command of loud_auto.score and cmp the WAV and the score with the first run. In check_72 or check_75, read bytes 24-27 of the --rate 48000 WAV and compare with 48000.

--- [9] TEST-GAP @ tests/synth_render_errors.c:108-109; tests/synth_favourite.c:110-119
PROBLEM: Three regressions of the ring-out rule pass every check. The 60 s limit is tested against the code's own constant, so changing the constant changes the expectation with it. No check has a tail that is louder on one channel. No check pins the quiet stretch to exactly longest delay + 1 frames.
EVIDENCE: Mutations on a scratch copy, all 13 checks pass: M18 (RENDER_RING_OUT_LIMIT 30.0 in render.h; check_46 then prints 'stopped after 30.000 s' and passes), M11 (quiet test on the left channel only), M15 ('quiet >= longest' instead of '>'). The rule itself is correct in the current code (see verdict).
FIX: In synth_render_errors.c compare against the literal 60 s. Add one case with pan 1.0 and a single tap without swap (0.25 s, gain 0.7), and require frames == last frame with |sample| >= 1/65536 on either channel + longest + 2, which is 279334 for that case at 44.1 kHz.
