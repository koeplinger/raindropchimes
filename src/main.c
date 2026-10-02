/* main.c -- the chimes command line and the presets.
 *
 *   chimes make   [options]            compose a tune and render it
 *   chimes score  [options]            compose only; print the score
 *   chimes render FILE [options]       render an existing score file
 *
 * This is the only file that knows all three parts: it hands the composer's
 * score to the synthesizer and the synthesizer's frames to an output.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compose/compose.h"
#include "output/sink.h"
#include "score/score_text.h"
#include "synth/render.h"

/* A preset bundles rules, patch, tempo, drift and cycles for one version. */
typedef struct {
    const char *name;
    const char *rules;
    const char *patch;
    double      bins;     /* tempo: a slot lasts (bins + 1) / 44100 seconds   */
    double      drift;    /* bins added to the slot length per cycle           */
    int         cycles;
} Preset;

static const Preset PRESETS[] = {
    { "v4",  "v4",  "v4",  1700.0, 0.0, 500 },   /* 14 February 2011: the favourite tune       */
    { "v5",  "v4",  "v4",  1700.0, 0.0, 400 },   /* 17 February 2011: the v4 rules, 400 cycles */
    { "v6",  "v6",  "v6",  3700.0, 1.0, 300 },   /* 18 February 2011: slows down as it goes    */
    { "v10", "v10", "v10", 3440.0, 0.0, 200 },   /* the last 2011 version (legacy/)            */
};

#define N_PRESETS      ((int) (sizeof PRESETS / sizeof PRESETS[0]))
#define DEFAULT_PRESET "v4"

/* Leaves a little room below full scale when a tune is turned down, so that
 * the loudest sample converts to 32767 and nothing saturates. */
#define FULL_SCALE_16_BIT (32767.0 / 32768.0)

typedef struct {
    const char *preset, *rules, *patch, *out;
    int have_seed, have_bins, have_drift, have_cycles, have_rate, have_gain;
    uint32_t seed;
    double bins, drift, rate, gain;
    int cycles;
} Options;

/* What a render needs, once presets, score notes and options are resolved. */
typedef struct {
    const Patch *patch;
    double bins, drift, rate;
    int have_gain;
    double gain;
} RenderPlan;

static void usage(FILE *file)
{
    fprintf(file,
        "usage:\n"
        "  chimes make   [options]         compose a tune and render it to a WAV file\n"
        "  chimes score  [options]         compose only; print the score\n"
        "  chimes render FILE [options]    render an existing score file\n"
        "\n"
        "options:\n"
        "  --seed N       the seed (default: the clock; it is printed)\n"
        "  --preset NAME  rules, patch, tempo, drift and cycles of one version (default: %s)\n"
        "  --rules NAME   the rule set\n"
        "  --patch NAME   the sound\n"
        "  --cycles T     length in cycles; this shapes the piece, so it changes the tune\n"
        "  --bins B       tempo, in the old unit: a slot lasts (B + 1) / 44100 seconds\n"
        "  --drift D      bins added to the slot length per cycle\n"
        "  --rate R       sample rate (default: 44100)\n"
        "  --gain G       output gain; 1 is the old scale (default: 1, turned down only\n"
        "                 if the tune would exceed full scale)\n"
        "  -o FILE        output file; '-' writes raw 32-bit float frames to standard output\n"
        "\n"
        "Without -o, make writes chimes_<seed>_<bins>_<cycles>.wav, and the score beside it.\n",
        DEFAULT_PRESET);
}

/* The fewest digits that read back as the same number. */
static const char *number_text(char *text, size_t len, double value)
{
    int precision;
    if (value == (double) (long) value && value > -1e15 && value < 1e15) {
        snprintf(text, len, "%ld", (long) value);   /* whole numbers without an exponent */
        return text;
    }
    for (precision = 1; precision <= 17; precision++) {
        snprintf(text, len, "%.*g", precision, value);
        if (strtod(text, NULL) == value) break;
    }
    return text;
}

static int parse_number(const char *text, double *value)
{
    char *end;
    errno = 0;
    *value = strtod(text, &end);
    return (end == text || *end != '\0' || errno != 0 || *value != *value) ? -1 : 0;
}

static int parse_seed(const char *text, uint32_t *seed)
{
    char *end;
    unsigned long parsed;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || errno != 0 || text[0] == '-' || parsed > 0xfffffffful)
        return -1;
    *seed = (uint32_t) parsed;
    return 0;
}

/* Parses the options from argv[first..]; a bare argument is stored in *file
 * if file is not NULL. Returns 0, or -1 after printing a message. */
static int parse_options(int argc, char **argv, int first, Options *opt, const char **file)
{
    int i;
    memset(opt, 0, sizeof *opt);

    for (i = first; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = (i + 1 < argc) ? argv[i + 1] : NULL;
        double number;

        if (arg[0] != '-' || strcmp(arg, "-") == 0) {
            if (!file || *file) {
                fprintf(stderr, "chimes: unexpected argument '%s'\n", arg);
                return -1;
            }
            *file = arg;
            continue;
        }
        if (!value) {
            fprintf(stderr, "chimes: option '%s' needs a value\n", arg);
            return -1;
        }
        i++;

        if (strcmp(arg, "--preset") == 0) opt->preset = value;
        else if (strcmp(arg, "--rules") == 0) opt->rules = value;
        else if (strcmp(arg, "--patch") == 0) opt->patch = value;
        else if (strcmp(arg, "-o") == 0) opt->out = value;
        else if (strcmp(arg, "--seed") == 0) {
            if (parse_seed(value, &opt->seed) != 0) {
                fprintf(stderr, "chimes: --seed must be a whole number from 0 to 4294967295\n");
                return -1;
            }
            opt->have_seed = 1;
        } else if (parse_number(value, &number) != 0) {
            fprintf(stderr, "chimes: '%s' is not a number or '%s' is not an option\n", value, arg);
            return -1;
        } else if (strcmp(arg, "--bins") == 0) {
            if (!(number > 0.0 && number <= 1e7)) {
                fprintf(stderr, "chimes: --bins must be above 0\n");
                return -1;
            }
            opt->bins = number; opt->have_bins = 1;
        } else if (strcmp(arg, "--drift") == 0) {
            if (!(number >= 0.0 && number <= 1e7)) {
                fprintf(stderr, "chimes: --drift must be 0 or more\n");
                return -1;
            }
            opt->drift = number; opt->have_drift = 1;
        } else if (strcmp(arg, "--cycles") == 0) {
            if (number != (double) (int) number || number < 1.0 || number > 1e7) {
                fprintf(stderr, "chimes: --cycles must be a whole number\n");
                return -1;
            }
            opt->cycles = (int) number; opt->have_cycles = 1;
        } else if (strcmp(arg, "--rate") == 0) {
            if (number != (double) (int) number || number < 1000.0 || number > 768000.0) {
                fprintf(stderr, "chimes: --rate must be a whole number from 1000 to 768000\n");
                return -1;
            }
            opt->rate = number; opt->have_rate = 1;
        } else if (strcmp(arg, "--gain") == 0) {
            if (!(number > 0.0 && number <= 1e6)) {
                fprintf(stderr, "chimes: --gain must be above 0\n");
                return -1;
            }
            opt->gain = number; opt->have_gain = 1;
        } else {
            fprintf(stderr, "chimes: unknown option '%s'\n", arg);
            return -1;
        }
    }
    return 0;
}

static const Preset *find_preset(const char *name)
{
    int i;
    if (!name) name = DEFAULT_PRESET;
    for (i = 0; i < N_PRESETS; i++)
        if (strcmp(PRESETS[i].name, name) == 0) return &PRESETS[i];
    fprintf(stderr, "chimes: unknown preset '%s'; known presets:", name);
    for (i = 0; i < N_PRESETS; i++) fprintf(stderr, " %s", PRESETS[i].name);
    fprintf(stderr, "\n");
    return NULL;
}

static const Patch *find_patch(const char *name)
{
    const Patch *patch = patch_find(name);
    int i;
    if (patch) return patch;
    fprintf(stderr, "chimes: unknown patch '%s'; known patches:", name);
    for (i = 0; i < patch_count(); i++) fprintf(stderr, " %s", patch_at(i)->name);
    fprintf(stderr, "\n");
    return NULL;
}

/* Composes a tune into score. Returns 0, or -1 after printing a message. */
static int compose(const Options *opt, const Preset *preset, Score *score)
{
    const char *rules_name = opt->rules ? opt->rules : preset->rules;
    const Rules *rules = rules_find(rules_name);
    int cycles = opt->have_cycles ? opt->cycles : preset->cycles;
    uint32_t seed = opt->have_seed ? opt->seed : (uint32_t) time(NULL);
    char err[256];
    Rng rng;
    int i;

    if (!rules) {
        fprintf(stderr, "chimes: unknown rule set '%s'; known rule sets:", rules_name);
        for (i = 0; i < rules_count(); i++) fprintf(stderr, " %s", rules_at(i)->name);
        fprintf(stderr, "\n");
        return -1;
    }
    if (opt->have_cycles && cycles != preset->cycles)
        fprintf(stderr, "note: the number of cycles shapes the piece, so --cycles %d gives a "
                "different tune than the preset's %d cycles, not a shorter or longer one\n",
                cycles, preset->cycles);

    rng_seed(&rng, seed);
    if (slotwalk_compose(rules, &rng, cycles, score, err, sizeof err) != 0) {
        fprintf(stderr, "chimes: %s\n", err);
        return -1;
    }
    score->seed = seed;
    snprintf(score->generator, sizeof score->generator, "%s", RNG_NAME);
    fprintf(stderr, "seed %lu, rules %s, %d cycles: %d tones\n",
            (unsigned long) seed, score->rules, cycles, (int) score->n_tones);
    return 0;
}

/* Replaces the extension of path (or appends one). */
static void with_extension(char *out, size_t len, const char *path, const char *extension)
{
    const char *slash = strrchr(path, '/');
    const char *dot = strrchr(path, '.');
    size_t stem = (dot && (!slash || dot > slash) && dot != path) ? (size_t) (dot - path)
                                                                  : strlen(path);
    snprintf(out, len, "%.*s%s", (int) stem, path, extension);
}

/* Renders score to audio_path ("-": raw frames to standard output) and writes
 * the score, with its render notes, to score_path. command_start is the
 * beginning of the command that recreates the file; the render settings are
 * appended to it. Returns 0, or -1 after printing a message. */
static int render_and_record(Score *score, const RenderPlan *plan, const char *command_start,
                             const char *audio_path, const char *score_path)
{
    Timing timing = timing_from_bins(plan->bins, plan->drift);
    int to_stdout = strcmp(audio_path, "-") == 0;
    const SinkFormat *format = sink_format_find(to_stdout ? "raw" : "wav");
    char err[256], bins[32], drift[32], rate[32], gain_text[32], command[512];
    double gain = plan->have_gain ? plan->gain : 1.0;
    RenderStats stats;
    Sink *sink;
    FILE *score_file;
    long saturated;

    if (!format) {
        fprintf(stderr, "chimes: no such output format\n");
        return -1;
    }

    /* The default output gain is 1, the old scale. A tune that would exceed
     * full scale is turned down just enough; that needs the peak first. */
    if (!plan->have_gain) {
        if (render_score(score, &timing, plan->patch, plan->rate, 1.0, NULL, &stats,
                         err, sizeof err) != 0) {
            fprintf(stderr, "chimes: %s\n", err);
            return -1;
        }
        if (stats.peak > FULL_SCALE_16_BIT) {
            gain = FULL_SCALE_16_BIT / stats.peak;
            fprintf(stderr, "this tune would peak at %.1f %% of full scale; turned down to "
                    "output gain %s\n", 100.0 * stats.peak,
                    number_text(gain_text, sizeof gain_text, gain));
        }
    }

    sink = format->open(audio_path, (int) plan->rate);
    if (!sink) {
        fprintf(stderr, "chimes: cannot write '%s'\n", audio_path);
        return -1;
    }
    if (render_score(score, &timing, plan->patch, plan->rate, gain, sink, &stats,
                     err, sizeof err) != 0) {
        fprintf(stderr, "chimes: %s\n", err);
        sink->close(sink);
        return -1;
    }
    saturated = sink->saturated;
    if (sink->close(sink) != 0) {
        fprintf(stderr, "chimes: error writing '%s'\n", audio_path);
        return -1;
    }

    /* The record: how this audio was made, including a complete command. */
    number_text(bins, sizeof bins, plan->bins);
    number_text(drift, sizeof drift, plan->drift);
    number_text(rate, sizeof rate, plan->rate);
    number_text(gain_text, sizeof gain_text, gain);
    snprintf(command, sizeof command, "%s --patch %s --bins %s --drift %s --rate %s --gain %s -o %s",
             command_start, plan->patch->name, bins, drift, rate, gain_text, audio_path);
    score_clear_render_notes(score);
    score_set_render_note(score, "patch", plan->patch->name);
    score_set_render_note(score, "bins", bins);
    score_set_render_note(score, "drift", drift);
    score_set_render_note(score, "rate", rate);
    score_set_render_note(score, "gain", gain_text);
    score_set_render_note(score, "command", command);

    score_file = fopen(score_path, "w");
    if (!score_file || score_write(score_file, score) != 0 || fclose(score_file) != 0) {
        fprintf(stderr, "chimes: cannot write '%s'\n", score_path);
        return -1;
    }

    fprintf(stderr, "%s: %.1f s (music %.1f s, then reverb tail), peak %.1f %% of full scale\n",
            to_stdout ? "standard output" : audio_path, (double) stats.frames / plan->rate,
            (double) stats.music_frames / plan->rate, 100.0 * stats.peak);
    if (saturated > 0)
        fprintf(stderr, "warning: %ld samples exceeded full scale and were held at the limit; "
                "use a lower --gain\n", saturated);
    fprintf(stderr, "%s: the score, with the command that recreates this audio\n", score_path);
    return 0;
}

static int command_make(int argc, char **argv)
{
    Options opt;
    const Preset *preset;
    RenderPlan plan;
    Score score;
    char name[128], bins[32], audio_path[512], score_path[512], command_start[256];
    int status;

    if (parse_options(argc, argv, 2, &opt, NULL) != 0) return 1;
    if (!(preset = find_preset(opt.preset))) return 1;

    plan.patch = find_patch(opt.patch ? opt.patch : preset->patch);
    if (!plan.patch) return 1;
    plan.bins = opt.have_bins ? opt.bins : preset->bins;
    plan.drift = opt.have_drift ? opt.drift : preset->drift;
    plan.rate = opt.have_rate ? opt.rate : RENDER_DEFAULT_RATE;
    plan.have_gain = opt.have_gain;
    plan.gain = opt.gain;

    score_init(&score);
    if (compose(&opt, preset, &score) != 0) {
        score_free(&score);
        return 1;
    }

    snprintf(name, sizeof name, "chimes_%lu_%s_%d", (unsigned long) score.seed,
             number_text(bins, sizeof bins, plan.bins), (int) score.cycles);
    if (opt.out && strcmp(opt.out, "-") != 0) {
        snprintf(audio_path, sizeof audio_path, "%s", opt.out);
        with_extension(score_path, sizeof score_path, opt.out, ".score");
    } else {
        snprintf(audio_path, sizeof audio_path, "%s%s", opt.out ? "-" : name, opt.out ? "" : ".wav");
        snprintf(score_path, sizeof score_path, "%s.score", name);
    }
    snprintf(command_start, sizeof command_start, "chimes make --rules %s --seed %lu --cycles %d",
             score.rules, (unsigned long) score.seed, (int) score.cycles);

    status = render_and_record(&score, &plan, command_start, audio_path, score_path);
    score_free(&score);
    return status == 0 ? 0 : 1;
}

static int command_score(int argc, char **argv)
{
    Options opt;
    const Preset *preset;
    Score score;
    FILE *file = stdout;
    int status = 0;

    if (parse_options(argc, argv, 2, &opt, NULL) != 0) return 1;
    if (!(preset = find_preset(opt.preset))) return 1;

    score_init(&score);
    if (compose(&opt, preset, &score) != 0) {
        score_free(&score);
        return 1;
    }
    if (opt.out && strcmp(opt.out, "-") != 0 && !(file = fopen(opt.out, "w"))) {
        fprintf(stderr, "chimes: cannot write '%s'\n", opt.out);
        score_free(&score);
        return 1;
    }
    if (score_write(file, &score) != 0) status = 1;
    if (file != stdout && fclose(file) != 0) status = 1;
    if (status != 0) fprintf(stderr, "chimes: error writing the score\n");
    score_free(&score);
    return status;
}

/* A render setting recorded in the score, as a number. */
static int noted_number(const Score *score, const char *key, double *value)
{
    const char *text = score_render_note(score, key);
    return (text && parse_number(text, value) == 0) ? 0 : -1;
}

static int command_render(int argc, char **argv)
{
    Options opt;
    const char *path = NULL, *noted_patch;
    const Preset *preset;
    RenderPlan plan;
    Score score;
    FILE *file;
    char err[256], audio_path[512], score_path[512], command_start[600];
    int status;

    if (parse_options(argc, argv, 2, &opt, &path) != 0) return 1;
    if (!path) {
        fprintf(stderr, "chimes: render needs a score file\n");
        return 1;
    }
    if (opt.have_seed || opt.have_cycles || opt.rules) {
        fprintf(stderr, "chimes: --seed, --cycles and --rules belong to composing; "
                "render takes the score as it is\n");
        return 1;
    }
    if (!(preset = find_preset(opt.preset))) return 1;

    if (!(file = fopen(path, "r"))) {
        fprintf(stderr, "chimes: cannot read '%s'\n", path);
        return 1;
    }
    score_init(&score);
    status = score_read(file, &score, err, sizeof err);
    fclose(file);
    if (status != 0) {
        fprintf(stderr, "chimes: %s: %s\n", path, err);
        score_free(&score);
        return 1;
    }

    /* Settings come from the command line, else from the score's render notes
     * (unless a preset was asked for), else from the preset. */
    noted_patch = opt.preset ? NULL : score_render_note(&score, "patch");
    plan.patch = find_patch(opt.patch ? opt.patch : noted_patch ? noted_patch : preset->patch);
    if (!plan.patch) {
        score_free(&score);
        return 1;
    }
    plan.bins = preset->bins;
    plan.drift = preset->drift;
    if (!opt.preset) {
        noted_number(&score, "bins", &plan.bins);
        noted_number(&score, "drift", &plan.drift);
    }
    if (opt.have_bins) plan.bins = opt.bins;
    if (opt.have_drift) plan.drift = opt.drift;
    plan.rate = opt.have_rate ? opt.rate : RENDER_DEFAULT_RATE;
    plan.have_gain = opt.have_gain;
    plan.gain = opt.gain;

    if (opt.out && strcmp(opt.out, "-") != 0) {
        snprintf(audio_path, sizeof audio_path, "%s", opt.out);
        with_extension(score_path, sizeof score_path, opt.out, ".score");
    } else {
        if (opt.out) snprintf(audio_path, sizeof audio_path, "-");
        else with_extension(audio_path, sizeof audio_path, path, ".wav");
        with_extension(score_path, sizeof score_path, path, ".score");
    }
    snprintf(command_start, sizeof command_start, "chimes render %s", score_path);

    status = render_and_record(&score, &plan, command_start, audio_path, score_path);
    score_free(&score);
    return status == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(stderr);
        return 1;
    }
    if (strcmp(argv[1], "make") == 0) return command_make(argc, argv);
    if (strcmp(argv[1], "score") == 0) return command_score(argc, argv);
    if (strcmp(argv[1], "render") == 0) return command_render(argc, argv);
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(stdout);
        return 0;
    }
    fprintf(stderr, "chimes: unknown command '%s'\n", argv[1]);
    usage(stderr);
    return 1;
}
