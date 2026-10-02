/* synth_extension.c -- checks the two worked examples of docs/EXTENDING.md:
 * the wave form "bell", the effect "soften", and the patch "bell" that uses
 * both.
 *
 *   synth_extension                      the wave form and the effect
 *   synth_extension SCORE OUT.wav        renders the score through patch "bell"
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "synth/effects.h"
#include "synth/render.h"
#include "synth/shapes.h"
#include "synth/soften.h"
#include "synth_common.h"

#define TWO_PI 6.283185307179586476925
#define RATE   44100L

/* The bell wave must hold exactly the parts it was given, at the gains it was
 * given: the tone itself and its multiples 2, 3 and 5, at 1, 1/2, 1/3 and
 * 1/5, divided by their sum; and of those, only the multiples up to
 * `highest`, the highest one the sample rate can carry. Measured by adding
 * up one whole swing. */
static void check_bell_wave(int highest)
{
    WaveFn bell = wave_find("bell");
    double sum = 1.0 + 1.0 / 2.0 + 1.0 / 3.0 + 1.0 / 5.0;
    double largest = 0.0;
    int n, k, points = 4096;

    if (!bell) fail("there is no wave form \"bell\"");

    printf("bell wave, highest multiple %d:", highest);
    for (n = 0; n <= 8; n++) {
        double cosine_part = 0.0, sine_part = 0.0, expected = 0.0;

        for (k = 0; k < points; k++) {
            double phase = TWO_PI * (double) k / points;
            double value = bell(phase, highest);
            cosine_part += value * cos(n * phase);
            sine_part += value * sin(n * phase);
            if (fabs(value) > largest) largest = fabs(value);
        }
        cosine_part *= (n == 0 ? 1.0 : 2.0) / points;
        sine_part *= 2.0 / points;

        /* The tone itself (n = 1) is always there. */
        if (n == 1 || ((n == 2 || n == 3 || n == 5) && n <= highest)) expected = (1.0 / n) / sum;
        printf("  %d: %.4f", n, fabs(cosine_part) < 5e-5 ? 0.0 : cosine_part);
        if (fabs(cosine_part - expected) > 1e-12 || fabs(sine_part) > 1e-12)
            fail("with highest multiple %d, part %d of the bell wave is %.6f, not %.6f",
                 highest, n, cosine_part, expected);
    }
    printf("\n");
    if (largest > 1.0 + 1e-12) fail("the bell wave is larger than 1 somewhere (%.6f)", largest);
    if (highest >= 5 && fabs(bell(0.0, highest) - 1.0) > 1e-12)
        fail("the whole bell wave should be 1 at phase 0");
}

/* How much of a wave of the given frequency is in the left channel of the
 * captured frames from..to-1 (its size, as the peak of a steady wave). */
static double part_at(const Capture *capture, long from, long to, double freq)
{
    double cosine_part = 0.0, sine_part = 0.0;
    long i;

    for (i = from; i < to; i++) {
        double turn = TWO_PI * freq * (double) i / RATE;
        cosine_part += capture->samples[2 * i] * cos(turn);
        sine_part += capture->samples[2 * i] * sin(turn);
    }
    return 2.0 * sqrt(cosine_part * cosine_part + sine_part * sine_part) / (double) (to - from);
}

/* Renders one tone through the bell wave without reverb, and measures one
 * second in the slow decay of its first strike: the size of the wave at
 * `where` Hz, as a share of the tone's own size. */
static double bell_part(double freq, double vibrato_depth, double where)
{
    Patch dry = dry_patch("bell");
    Timing timing = timing_from_bins(8819.0, 0.0);    /* a slot of 0.2 s: a strike of 2.2 s */
    Score score;
    Capture capture;
    RenderStats stats;
    char err[256];
    double share;
    long from = RATE / 2, to = RATE / 2 + RATE;

    dry.vibrato_depth = vibrato_depth;
    one_tone_score(&score, 0, freq, 0.0, 1);
    capture_init(&capture);
    if (render_score(&score, &timing, &dry, RATE, 1.0, &capture.sink, &stats, err, sizeof err) != 0)
        fail("render: %s", err);
    if (capture.frames < to) fail("the render is too short");
    share = part_at(&capture, from, to, where) / part_at(&capture, from, to, freq);
    capture_free(&capture);
    score_free(&score);
    return share;
}

/* A high tone through the bell wave must not fold back, and must keep every
 * overtone that fits. At 44100 Hz, half the sample rate is 22050 Hz. */
static void check_no_fold_back(void)
{
    double second, third, fifth, folded;

    /* 5000 Hz: its multiples 2 and 3 (10000 and 15000 Hz) fit and must be
     * there; its multiple 5 (25000 Hz) does not fit. Were it sounded all the
     * same, it would come out at 44100 - 25000 = 19100 Hz. */
    second = bell_part(5000.0, 0.0, 10000.0);
    third = bell_part(5000.0, 0.0, 15000.0);
    folded = bell_part(5000.0, 0.0, 19100.0);
    printf("a tone of 5000 Hz through the bell wave: multiples 2 and 3 at %.3f and %.3f of the "
           "tone; at 19100 Hz, where multiple 5 would fold back to: %.6f\n", second, third, folded);
    if (fabs(second - 0.5) > 0.01 || fabs(third - 1.0 / 3.0) > 0.01)
        fail("the overtones that fit are not at 1/2 and 1/3 of the tone");
    if (folded > 0.001) fail("an overtone above half the sample rate was sounded and folded back");

    /* 4000 Hz: multiple 5 is 20000 Hz. It fits, and must not be left out. */
    fifth = bell_part(4000.0, 0.0, 20000.0);
    printf("a tone of 4000 Hz: multiple 5, at 20000 Hz, at %.3f of the tone\n", fifth);
    if (fabs(fifth - 0.2) > 0.01) fail("an overtone that fits below half the sample rate was left out");

    /* 4409 Hz: multiple 5 is 22045 Hz, just below 22050. But the vibrato of
     * the patch (0.05 %) swings it up to 22056 Hz, so it must be left out. */
    fifth = bell_part(4409.0, 0.0005, 22045.0);
    printf("a tone of 4409 Hz with vibrato: at 22045 Hz, where multiple 5 would swing across half "
           "the sample rate: %.6f\n", fifth);
    if (fifth > 0.001) fail("an overtone that the vibrato carries past half the sample rate was sounded");
}

/* Sends one second of a steady wave through the effect, after one second to
 * settle, and returns how much of its size is left. */
static double soften_passes(const Effect *soften, double cutoff, double freq, long rate)
{
    SoftenConfig config;
    void *state;
    double in = 0.0, out_left = 0.0, out_right = 0.0;
    long i;

    config.cutoff_hz = cutoff;
    state = soften->create(&config, (double) rate);
    if (!state) fail("the effect \"soften\" could not be set up");

    for (i = 0; i < 2 * rate; i++) {
        double frame[2];
        frame[0] = cos(TWO_PI * freq * (double) i / (double) rate);
        frame[1] = 0.5 * frame[0];            /* the right channel at half the size */
        if (i >= rate) in += frame[0] * frame[0];
        soften->process(state, frame, 1);
        if (i >= rate) {
            out_left += frame[0] * frame[0];
            out_right += frame[1] * frame[1];
        }
    }
    if (soften->longest_delay_seconds(state) != 0.0) fail("\"soften\" should report no delay");
    soften->destroy(state);

    if (fabs(sqrt(out_right / out_left) - 0.5) > 1e-9)
        fail("\"soften\" does not treat the two channels alike");
    return sqrt(out_left / in);
}

/* What the one-pole formula predicts for a wave of that frequency. */
static double soften_predicted(double cutoff, double freq, long rate)
{
    double share = 1.0 - exp(-TWO_PI * cutoff / (double) rate);
    double keep = 1.0 - share;
    double turn = TWO_PI * freq / (double) rate;
    return share / sqrt(1.0 - 2.0 * keep * cos(turn) + keep * keep);
}

static void check_soften(void)
{
    static const double FREQS[] = { 100.0, 1000.0, 4000.0, 10000.0 };
    static const long RATES[] = { 44100, 48000 };
    const Effect *soften = effect_find("soften");
    SoftenConfig bad;
    size_t i, r;

    if (!soften) fail("there is no effect \"soften\"");
    for (r = 0; r < sizeof RATES / sizeof RATES[0]; r++) {
        for (i = 0; i < sizeof FREQS / sizeof FREQS[0]; i++) {
            double passes = soften_passes(soften, 4000.0, FREQS[i], RATES[r]);
            double predicted = soften_predicted(4000.0, FREQS[i], RATES[r]);
            printf("soften at 4000 Hz, sample rate %ld: a wave of %5.0f Hz keeps %.4f of its size "
                   "(formula: %.4f)\n", RATES[r], FREQS[i], passes, predicted);
            if (fabs(passes - predicted) > 1e-6) fail("\"soften\" does not follow the one-pole formula");
        }
    }
    /* The cut-off is given in Hz: at another sample rate the same tones pass.
     * (Only close to half the sample rate do the two differ.) */
    if (fabs(soften_passes(soften, 4000.0, 1000.0, 44100) - soften_passes(soften, 4000.0, 1000.0, 48000)) > 0.005)
        fail("\"soften\" does not take the sample rate into account");
    if (soften_passes(soften, 4000.0, 100.0, 44100) < 0.999) fail("\"soften\" should leave a low tone nearly alone");
    if (soften_passes(soften, 4000.0, 10000.0, 44100) > 0.5) fail("\"soften\" should take away most of a high tone");

    bad.cutoff_hz = 0.0;
    if (soften->create(&bad, RATE)) fail("\"soften\" accepted a cut-off of 0 Hz");
    if (soften->create(NULL, RATE)) fail("\"soften\" accepted missing settings");
}

/* The favourite through patch "bell": finite, unsaturated, with a ring-out. */
static void render_bell(const char *score_path, const char *wav_path)
{
    Score score;
    Timing timing = timing_from_bins(1700.0, 0.0);
    const Patch *bell = patch_find("bell");
    const SinkFormat *format = sink_format_find("wav");
    Sink *sink;
    RenderStats stats;
    char err[256];
    long saturated;

    if (!bell || !format) fail("the patch \"bell\" or the wav format is missing");
    load_score(score_path, &score);
    sink = format->open(wav_path, RATE);
    if (!sink) fail("cannot open %s for writing", wav_path);
    if (render_score(&score, &timing, bell, RATE, 1.0, sink, &stats, err, sizeof err) != 0)
        fail("render: %s", err);            /* a value that is not a number would fail here */
    saturated = sink->saturated;
    if (sink->close(sink) != 0) fail("closing %s failed", wav_path);

    printf("patch bell: %.3f s, music ends %.3f s, peak %.5f, %ld samples saturated\n",
           (double) stats.frames / RATE, (double) stats.music_frames / RATE, stats.peak, saturated);
    if (saturated != 0 || stats.peak >= 1.0) fail("the bell patch saturates on the favourite tune");
    if (stats.peak < 0.05) fail("the bell patch is nearly silent");
    /* The reverb's longest delay is 19000 frames: the tail must outlast it. */
    if (stats.frames - stats.music_frames <= 19000) fail("the bell patch has no ring-out");
    score_free(&score);
}

int main(int argc, char **argv)
{
    if (argc == 3) {
        render_bell(argv[1], argv[2]);
        return 0;
    }
    if (argc != 1) fail("usage: synth_extension [SCORE OUT.wav]");

    check_bell_wave(1000);    /* a low tone: every overtone fits */
    check_bell_wave(5);
    check_bell_wave(4);       /* multiple 5 does not fit */
    check_bell_wave(2);
    check_bell_wave(1);       /* only the tone itself */
    check_bell_wave(0);       /* the voice never asks for this; the wave form answers with the tone alone */
    check_no_fold_back();
    check_soften();
    printf("ok: the wave form \"bell\" and the effect \"soften\"\n");
    return 0;
}
