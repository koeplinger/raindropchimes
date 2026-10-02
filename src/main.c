/* main.c -- the chimes command line and the presets.
 *
 *   chimes make   [options]            compose a tune and render it
 *   chimes score  [options]            compose only; print the score
 *   chimes render FILE [options]       render an existing score file
 *
 * This is the only file that knows all three parts: it hands the composer's
 * score to the synthesizer and the synthesizer's frames to an output.
 */
#include <ctype.h>
#include <errno.h>
#include <math.h>
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

/* The limits of the settings. They are the same on the command line and in
 * the "# render:" lines of a score file. */
#define MAX_BINS   1.0e7
#define MAX_DRIFT  1.0e7
#define MAX_CYCLES 1.0e7
#define MIN_RATE   1000.0
#define MAX_RATE   768000.0
#define MAX_GAIN   1.0e6

#define PATH_LEN 4096    /* the longest path of an output file, with its closing zero */

/* A 16-bit stereo WAV file holds at most this many frames: its header counts
 * bytes in 32 bits, 36 of them are taken, and a frame is 4 bytes, so it is
 * (4294967295 - 36) / 4. The same number is in output/wav.c, which refuses
 * to write more. */
#define WAV_MAX_FRAMES 1073741814.0

/* Leaves a little room below full scale when a tune is turned down, so that
 * the loudest sample converts to 32767 and nothing saturates. */
#define FULL_SCALE_16_BIT (32767.0 / 32768.0)

/* The score is written into a file of this ending first, and moved to its
 * place when it is complete. */
#define TEMP_ENDING ".chimes-tmp"

typedef struct {
    const char *preset, *rules, *patch, *out;
    int have_seed, have_bins, have_drift, have_cycles, have_rate, have_gain;
    uint32_t seed;
    double bins, drift, rate, gain;
    int cycles;
} Options;

/* What a render needs, once presets, the score's render lines and options are resolved. */
typedef struct {
    const Patch *patch;
    double bins, drift, rate;
    int have_gain;
    double gain;
} RenderPlan;

/* A short text put together piece by piece: the command that is recorded in
 * a score. It has to fit one render line. `unfit` says that it does not (it
 * is too long, or holds a line break); then it is not recorded at all. */
typedef struct {
    char   text[SCORE_RENDER_VALUE_LEN];
    size_t length;
    int    unfit;
} Text;

static void print_patch_names(FILE *file)
{
    int i;
    for (i = 0; i < patch_count(); i++) fprintf(file, " %s", patch_at(i)->name);
}

static void usage(FILE *file)
{
    int i;

    fprintf(file,
        "usage:\n"
        "  chimes make   [options]         compose a tune and render it to a WAV file\n"
        "  chimes score  [options]         compose only; print the score (or write it to -o FILE)\n"
        "  chimes render FILE [options]    render an existing score file\n"
        "  chimes help                     this text\n"
        "\n"
        "options:\n"
        "  --seed N       the seed (default: the clock; it is printed)\n"
        "  --preset NAME  rules, patch, tempo, drift and cycles of one version:");
    for (i = 0; i < N_PRESETS; i++) fprintf(file, " %s", PRESETS[i].name);
    fprintf(file, " (default: %s)\n  --rules NAME   the rule set:", DEFAULT_PRESET);
    for (i = 0; i < rules_count(); i++) fprintf(file, " %s", rules_at(i)->name);
    fprintf(file, "\n  --patch NAME   the sound:");
    print_patch_names(file);
    fprintf(file, "\n"
        "  --cycles T     length in cycles; this shapes the piece, so it changes the tune\n"
        "  --bins B       tempo, in the 2011 unit: a slot lasts (B + 1) / 44100 seconds\n"
        "  --drift D      bins added to the slot length per cycle (negative: the piece speeds up)\n"
        "  --rate R       sample rate (default: 44100); a tone above half of it is left out\n"
        "  --gain G       output gain; 1 is the loudness of the 2011 program (default: 1, turned\n"
        "                 down only if the tune would exceed full scale)\n"
        "  -o FILE        output file; with make and render, '-' writes raw 32-bit float\n"
        "                 frames to standard output\n"
        "\n"
        "make:    without -o, writes chimes_<seed>_<bins>_<cycles>.wav, and the score beside it.\n"
        "score:   takes --seed, --preset, --rules, --cycles and -o only.\n"
        "render:  takes --preset, --patch, --bins, --drift, --rate, --gain and -o. Patch, bins\n"
        "         and drift that are not given come from the score, if it records them and no\n"
        "         --preset is given, else from the preset. Without -o the audio goes next to\n"
        "         the score file. The score is written again beside the audio, with the\n"
        "         settings used.\n");
}

/* --------------------------------------------------------------- numbers */

/* Writes a number with the fewest digits that read back as the same number;
 * whole numbers without an exponent. */
static const char *number_text(char *text, size_t len, double value)
{
    int precision;
    if (value > -1.0e15 && value < 1.0e15 && value == floor(value)) {
        snprintf(text, len, "%.0f", value);
        return text;
    }
    for (precision = 1; precision <= 17; precision++) {
        snprintf(text, len, "%.*g", precision, value);
        if (strtod(text, NULL) == value) break;
    }
    return text;
}

/* Reads a number that fills the whole text. *value is set only on success. */
static int parse_number(const char *text, double *value)
{
    char *end;
    double parsed;
    errno = 0;
    parsed = strtod(text, &end);
    if (end == text || *end != '\0' || errno != 0 || !isfinite(parsed)) return -1;
    *value = parsed;
    return 0;
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

/* Says what is wrong with a setting (the words are written into text), or
 * returns NULL if it is fine. The same tests serve the options and the
 * render lines of a score file. */
typedef const char *(*ProblemFn)(double value, char *text, size_t len);

static const char *bins_problem(double bins, char *text, size_t len)
{
    if (bins > 0.0 && bins <= MAX_BINS) return NULL;
    snprintf(text, len, "bins must be above 0 and at most %.0f", MAX_BINS);
    return text;
}

static const char *drift_problem(double drift, char *text, size_t len)
{
    if (drift >= -MAX_DRIFT && drift <= MAX_DRIFT) return NULL;
    snprintf(text, len, "drift must be from %.0f to %.0f", -MAX_DRIFT, MAX_DRIFT);
    return text;
}

/* --------------------------------------------------------------- options */

static int is_number_option(const char *arg)
{
    static const char *const names[] = { "--bins", "--drift", "--cycles", "--rate", "--gain" };
    size_t i;
    for (i = 0; i < sizeof names / sizeof names[0]; i++)
        if (strcmp(arg, names[i]) == 0) return 1;
    return 0;
}

/* Parses the options from argv[first..]; a bare argument is stored in *file
 * if file is not NULL. Returns 0; or -1 after printing a message; or 1 after
 * printing the usage text, when help was asked for. */
static int parse_options(int argc, char **argv, int first, Options *opt, const char **file)
{
    int i;
    memset(opt, 0, sizeof *opt);

    for (i = first; i < argc; i++) {
        const char *arg = argv[i];
        const char *value, *problem;
        char text[64];
        double number;

        /* A bare argument: the score file of `render`. */
        if (arg[0] != '-' || strcmp(arg, "-") == 0) {
            if (!file || *file) {
                fprintf(stderr, "chimes: unexpected argument '%s'\n", arg);
                return -1;
            }
            *file = arg;
            continue;
        }
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            usage(stdout);
            return 1;
        }

        /* Every option takes one value: the next argument. */
        if (i + 1 >= argc) {
            fprintf(stderr, "chimes: option '%s' needs a value\n", arg);
            return -1;
        }
        value = argv[++i];

        /* Options whose value is a name or a path. */
        if (strcmp(arg, "--preset") == 0) { opt->preset = value; continue; }
        if (strcmp(arg, "--rules") == 0)  { opt->rules = value;  continue; }
        if (strcmp(arg, "--patch") == 0)  { opt->patch = value;  continue; }
        if (strcmp(arg, "-o") == 0)       { opt->out = value;    continue; }
        if (strcmp(arg, "--seed") == 0) {
            if (parse_seed(value, &opt->seed) != 0) {
                fprintf(stderr, "chimes: --seed must be a whole number from 0 to 4294967295\n");
                return -1;
            }
            opt->have_seed = 1;
            continue;
        }

        /* Options whose value is a number. */
        if (!is_number_option(arg)) {
            fprintf(stderr, "chimes: unknown option '%s'\n", arg);
            return -1;
        }
        if (parse_number(value, &number) != 0) {
            fprintf(stderr, "chimes: %s needs a number, not '%s'\n", arg, value);
            return -1;
        }
        if (strcmp(arg, "--bins") == 0) {
            if ((problem = bins_problem(number, text, sizeof text)) != NULL) {
                fprintf(stderr, "chimes: --%s\n", problem);
                return -1;
            }
            opt->bins = number;
            opt->have_bins = 1;
        } else if (strcmp(arg, "--drift") == 0) {
            if ((problem = drift_problem(number, text, sizeof text)) != NULL) {
                fprintf(stderr, "chimes: --%s\n", problem);
                return -1;
            }
            opt->drift = number;
            opt->have_drift = 1;
        } else if (strcmp(arg, "--cycles") == 0) {
            /* The range is tested first: only then is it safe to take the
             * number as a whole number. */
            if (number < 1.0 || number > MAX_CYCLES || number != floor(number)) {
                fprintf(stderr, "chimes: --cycles must be a whole number from 1 to %.0f\n", MAX_CYCLES);
                return -1;
            }
            opt->cycles = (int) number;
            opt->have_cycles = 1;
        } else if (strcmp(arg, "--rate") == 0) {
            if (number < MIN_RATE || number > MAX_RATE || number != floor(number)) {
                fprintf(stderr, "chimes: --rate must be a whole number from %.0f to %.0f\n",
                        MIN_RATE, MAX_RATE);
                return -1;
            }
            opt->rate = number;
            opt->have_rate = 1;
        } else {   /* --gain */
            if (!(number > 0.0 && number <= MAX_GAIN)) {
                fprintf(stderr, "chimes: --gain must be above 0 and at most %.0f\n", MAX_GAIN);
                return -1;
            }
            opt->gain = number;
            opt->have_gain = 1;
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
    if (patch) return patch;
    fprintf(stderr, "chimes: unknown patch '%s'; known patches:", name);
    print_patch_names(stderr);
    fprintf(stderr, "\n");
    return NULL;
}

/* -------------------------------------------------------------- composing */

/* The seed and the number of cycles of a tune: as asked for, else the clock
 * and the preset's length. */
static uint32_t chosen_seed(const Options *opt)
{
    return opt->have_seed ? opt->seed : (uint32_t) time(NULL);
}

static int chosen_cycles(const Options *opt, const Preset *preset)
{
    return opt->have_cycles ? opt->cycles : preset->cycles;
}

/* Composes a tune into score. Returns 0, or -1 after printing a message. */
static int compose(const Options *opt, const Preset *preset, uint32_t seed, int cycles, Score *score)
{
    const char *rules_name = opt->rules ? opt->rules : preset->rules;
    const Rules *rules = rules_find(rules_name);
    char err[256];
    Rng rng;
    int i;

    if (!rules) {
        fprintf(stderr, "chimes: unknown rule set '%s'; known rule sets:", rules_name);
        for (i = 0; i < rules_count(); i++) fprintf(stderr, " %s", rules_at(i)->name);
        fprintf(stderr, "\n");
        return -1;
    }
    /* Said only where a preset's own length is being changed; a command that
     * names its rule set and cycles itself knows what it asks for. */
    if (opt->have_cycles && !opt->rules && cycles != preset->cycles)
        fprintf(stderr, "the number of cycles shapes the piece: --cycles %d gives a different "
                "tune than the preset's %d cycles, not a shorter or longer one\n",
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

/* --------------------------------------------------- the recorded command */

/* Can a shell take this word as it stands? If not, it needs quotes. */
static int is_plain_word(const char *word)
{
    static const char plain[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "0123456789_./+-,:=@%";
    return word[0] != '\0' && strspn(word, plain) == strlen(word);
}

static void text_add(Text *text, const char *piece)
{
    size_t n = strlen(piece);
    if (text->unfit || text->length + n >= sizeof text->text) {
        text->unfit = 1;
        return;
    }
    memcpy(text->text + text->length, piece, n + 1);
    text->length += n;
}

/* Adds a space and one word of a command, written the way a shell needs it:
 * as it stands if it holds only plain characters, else between single quotes
 * (a path with a space in it, for example). Between single quotes a shell
 * takes every character as it is, except the single quote itself; that one
 * is written as '\'' (end the quotes, a quote on its own, begin them again). */
static void text_add_word(Text *text, const char *word)
{
    const char *c;

    text_add(text, " ");
    if (strpbrk(word, "\r\n")) {    /* the command is recorded on one line of the score */
        text->unfit = 1;
        return;
    }
    if (is_plain_word(word)) {
        text_add(text, word);
        return;
    }
    text_add(text, "'");
    for (c = word; *c; c++) {
        char one[2];
        one[0] = *c;
        one[1] = '\0';
        text_add(text, *c == '\'' ? "'\\''" : one);
    }
    text_add(text, "'");
}

/* The same for a message: prints one word the way a shell needs it. */
static void print_shell_word(FILE *file, const char *word)
{
    const char *c;

    if (is_plain_word(word)) {
        fputs(word, file);
        return;
    }
    fputc('\'', file);
    for (c = word; *c; c++) {
        if (*c == '\'') fputs("'\\''", file);
        else fputc(*c, file);
    }
    fputc('\'', file);
}

/* ------------------------------------------------------------ output paths */

/* Writes a followed by b into out, which is PATH_LEN long. Returns 0, or -1
 * if the two do not fit. */
static int join(char *out, const char *a, const char *b)
{
    int n = snprintf(out, PATH_LEN, "%s%s", a, b);
    return (n < 0 || n >= PATH_LEN) ? -1 : 0;
}

/* Replaces the extension of a path, or appends one if it has none:
 * tune.wav -> tune.score. The leading dot of a file name (.hidden) is not an
 * extension. out is PATH_LEN long. Returns 0, or -1 if the result does not
 * fit. */
static int with_extension(char *out, const char *path, const char *extension)
{
    const char *slash = strrchr(path, '/');
    const char *name = slash ? slash + 1 : path;
    const char *dot = strrchr(name, '.');
    size_t kept = (dot && dot != name) ? (size_t) (dot - path) : strlen(path);
    int n;

    if (kept >= PATH_LEN) return -1;
    /* "%.*s" prints the first `kept` characters of the path. */
    n = snprintf(out, PATH_LEN, "%.*s%s", (int) kept, path, extension);
    return (n < 0 || n >= PATH_LEN) ? -1 : 0;
}

/* Does the file name end like an audio file of another kind? chimes cannot
 * write those; a WAV file under such a name would only mislead. */
static int looks_like_another_format(const char *path)
{
    static const char *const endings[] = {
        ".ogg", ".oga", ".opus", ".flac", ".mp3", ".m4a", ".aac", ".mp4", ".wma", ".webm",
        ".aiff", ".aif"
    };
    size_t length = strlen(path), i, j;

    for (i = 0; i < sizeof endings / sizeof endings[0]; i++) {
        size_t n = strlen(endings[i]);
        if (length <= n) continue;
        for (j = 0; j < n; j++)
            if (tolower((unsigned char) path[length - n + j]) != endings[i][j]) break;
        if (j == n) return 1;
    }
    return 0;
}

/* Works out where the audio and the score go:
 *   -o FILE   the audio is FILE; the score is FILE with the extension .score
 *   -o -      the audio goes to standard output; the score is STEM.score
 *   no -o     STEM.wav and STEM.score
 * Both paths are PATH_LEN long. rate is the sample rate, for the advice given
 * when FILE looks like an audio file of another kind. Returns 0, or -1 after
 * printing a message. */
static int output_paths(const char *out, const char *stem, double rate,
                        char *audio_path, char *score_path)
{
    char rate_text[32];
    int too_long;

    if (out && strcmp(out, "-") != 0) {
        too_long = join(audio_path, out, "") != 0
                || with_extension(score_path, out, ".score") != 0;
    } else if (out) {
        too_long = join(audio_path, "-", "") != 0
                || join(score_path, stem, ".score") != 0;
    } else {
        too_long = join(audio_path, stem, ".wav") != 0
                || join(score_path, stem, ".score") != 0;
    }
    if (too_long) {
        fprintf(stderr, "chimes: the path of the output file is too long\n");
        return -1;
    }
    if (strcmp(audio_path, score_path) == 0) {
        fprintf(stderr, "chimes: the audio and its score would both be written to '%s'; give -o "
                "another name (the score gets the same name with the extension .score)\n",
                audio_path);
        return -1;
    }
    if (looks_like_another_format(audio_path)) {
        fprintf(stderr, "chimes: '%s': chimes writes WAV files only. For a compressed file, send "
                "the raw frames to an encoder, for example:\n"
                "  chimes ... -o - | ffmpeg -f f32le -ar %s -ac 2 -i - ",
                audio_path, number_text(rate_text, sizeof rate_text, rate));
        print_shell_word(stderr, audio_path);
        fprintf(stderr, "\n");
        return -1;
    }
    return 0;
}

/* ----------------------------------------------------------------- render */

/* An upper limit for the length of the piece, in frames: the end of the last
 * strike, plus the longest ring-out there can be. */
static double longest_piece_frames(const Score *score, const Timing *timing, double rate)
{
    int32_t last_tick = 0, i;

    for (i = 0; i < score->n_tones; i++) {
        int32_t end = score_tone_end(score, &score->tones[i]);
        if (end > last_tick) last_tick = end;
    }
    return (timing_tick_seconds(timing, score->slots, last_tick) + RENDER_RING_OUT_LIMIT) * rate;
}

/* Can the score be written at this path? A file that is there must be open
 * to writing; one that is not there will be created. */
static int score_path_writable(const char *score_path)
{
    FILE *file = fopen(score_path, "r");
    if (!file) return 1;
    fclose(file);
    file = fopen(score_path, "r+");
    if (!file) return 0;
    fclose(file);
    return 1;
}

/* Moves the finished temporary file to its place. Returns 0, or -1. */
static int move_into_place(const char *temp_path, const char *path)
{
#ifdef _WIN32
    remove(path);    /* there, a file cannot be moved onto one that exists */
#endif
    return rename(temp_path, path) == 0 ? 0 : -1;
}

/* Renders score to audio_path ("-": raw frames to standard output) and writes
 * the score, with its render lines, to score_path. command holds the
 * beginning of the command that recreates the file; the render settings are
 * added to it. Returns 0, or -1 after printing a message. */
static int render_and_record(Score *score, const RenderPlan *plan, Text *command,
                             const char *audio_path, const char *score_path)
{
    Timing timing = timing_from_bins(plan->bins, plan->drift);
    int to_stdout = strcmp(audio_path, "-") == 0;
    const char *audio_name = to_stdout ? "standard output" : audio_path;
    const SinkFormat *format = sink_format_find(to_stdout ? "raw" : "wav");
    char err[256], bins[32], drift[32], rate[32], gain_text[32];
    char temp_path[PATH_LEN + sizeof TEMP_ENDING];
    double gain = plan->have_gain ? plan->gain : 1.0;
    double most_frames;
    RenderStats stats;
    Sink *sink;
    FILE *score_file;
    long saturated;
    int status;

    if (!format) {
        fprintf(stderr, "chimes: no such output format\n");
        return -1;
    }
    most_frames = longest_piece_frames(score, &timing, plan->rate);
    if (!to_stdout && most_frames > WAV_MAX_FRAMES) {
        fprintf(stderr, "chimes: this piece would last up to %.1f hours; a WAV file at %s Hz holds "
                "%.1f hours at most. Use a smaller --bins or --drift, or -o - for raw frames "
                "without that limit\n", most_frames / plan->rate / 3600.0,
                number_text(rate, sizeof rate, plan->rate), WAV_MAX_FRAMES / plan->rate / 3600.0);
        return -1;
    }

    /* The piece is first rendered without any output. That gives its peak,
     * and whatever cannot be rendered shows before a file is touched. */
    if (render_score(score, &timing, plan->patch, plan->rate, gain, NULL, &stats,
                     err, sizeof err) != 0) {
        fprintf(stderr, "chimes: %s\n", err);
        return -1;
    }
    /* The default output gain is 1, the loudness of the 2011 program. A tune
     * that would exceed full scale is turned down just enough. */
    if (!plan->have_gain && stats.peak > FULL_SCALE_16_BIT) {
        gain = FULL_SCALE_16_BIT / stats.peak;
        fprintf(stderr, "this tune would peak at %.1f %% of full scale; turned down to "
                "output gain %s\n", 100.0 * stats.peak,
                number_text(gain_text, sizeof gain_text, gain));
    }

    /* The score goes into a temporary file beside its place and is moved
     * there only when it is complete: a score that is already there (render
     * rewrites its own input) is never left half written. The temporary file
     * is opened before the audio is made, so that a score that cannot be
     * written shows first, and a tune is not left without its record. */
    snprintf(temp_path, sizeof temp_path, "%s%s", score_path, TEMP_ENDING);
    score_file = score_path_writable(score_path) ? fopen(temp_path, "w") : NULL;
    if (!score_file) {
        fprintf(stderr, "chimes: cannot write '%s'\n", score_path);
        return -1;
    }

    /* The audio. */
    sink = format->open(audio_path, (int) plan->rate);
    if (!sink) {
        fprintf(stderr, "chimes: cannot write '%s'\n", audio_name);
        fclose(score_file);
        remove(temp_path);
        return -1;
    }
    status = render_score(score, &timing, plan->patch, plan->rate, gain, sink, &stats,
                          err, sizeof err);
    saturated = sink->saturated;
    if (sink->close(sink) != 0 && status == 0) {
        snprintf(err, sizeof err, "error writing '%s'", audio_name);
        status = -1;
    }
    if (status != 0) {
        /* The audio file is left as it is, not removed: -o may name
         * something that must not be deleted, a device for example. */
        if (to_stdout) fprintf(stderr, "chimes: %s\n", err);
        else fprintf(stderr, "chimes: %s; '%s' is incomplete\n", err, audio_path);
        fclose(score_file);
        remove(temp_path);
        return -1;
    }

    /* The record: how this audio was made, including a complete command. */
    number_text(bins, sizeof bins, plan->bins);
    number_text(drift, sizeof drift, plan->drift);
    number_text(rate, sizeof rate, plan->rate);
    number_text(gain_text, sizeof gain_text, gain);
    text_add_word(command, "--patch"); text_add_word(command, plan->patch->name);
    text_add_word(command, "--bins");  text_add_word(command, bins);
    text_add_word(command, "--drift"); text_add_word(command, drift);
    text_add_word(command, "--rate");  text_add_word(command, rate);
    text_add_word(command, "--gain");  text_add_word(command, gain_text);
    text_add_word(command, "-o");      text_add_word(command, audio_path);

    score_clear_render_lines(score);
    status = score_set_render_line(score, "patch", plan->patch->name);
    if (status == 0) status = score_set_render_line(score, "bins", bins);
    if (status == 0) status = score_set_render_line(score, "drift", drift);
    if (status == 0) status = score_set_render_line(score, "rate", rate);
    if (status == 0) status = score_set_render_line(score, "gain", gain_text);
    if (status == 0 && !command->unfit) status = score_set_render_line(score, "command", command->text);

    if (status == 0) status = score_write(score_file, score);
    if (fclose(score_file) != 0) status = -1;
    if (status == 0) status = move_into_place(temp_path, score_path);
    if (status != 0) {
        fprintf(stderr, "chimes: error writing '%s'\n", score_path);
        remove(temp_path);
        return -1;
    }

    fprintf(stderr, "%s: %.1f s (music %.1f s, then reverb tail), peak %.1f %% of full scale\n",
            audio_name, (double) stats.frames / plan->rate,
            (double) stats.music_frames / plan->rate, 100.0 * stats.peak);
    if (saturated > 0)
        fprintf(stderr, "warning: %ld samples exceeded full scale and were held at the limit; "
                "use a lower --gain\n", saturated);
    if (command->unfit)
        fprintf(stderr, "%s: the score, with the render settings. The command that recreates "
                "this audio is not recorded in it: it is longer than %d characters, or a path "
                "holds a line break\n", score_path, SCORE_RENDER_VALUE_LEN - 1);
    else
        fprintf(stderr, "%s: the score, with the command that recreates this audio\n", score_path);
    return 0;
}

/* --------------------------------------------------------------- commands */

static int command_make(int argc, char **argv)
{
    Options opt;
    const Preset *preset;
    RenderPlan plan;
    Score score;
    Text command = { "", 0, 0 };
    char stem[128], number[32];
    char audio_path[PATH_LEN], score_path[PATH_LEN];
    uint32_t seed;
    int cycles, status;

    status = parse_options(argc, argv, 2, &opt, NULL);
    if (status != 0) return status < 0 ? 1 : 0;
    if (!(preset = find_preset(opt.preset))) return 1;

    plan.patch = find_patch(opt.patch ? opt.patch : preset->patch);
    if (!plan.patch) return 1;
    plan.bins = opt.have_bins ? opt.bins : preset->bins;
    plan.drift = opt.have_drift ? opt.drift : preset->drift;
    plan.rate = opt.have_rate ? opt.rate : RENDER_DEFAULT_RATE;
    plan.have_gain = opt.have_gain;
    plan.gain = opt.gain;

    /* Where the files go is settled, and refused if need be, before any work. */
    seed = chosen_seed(&opt);
    cycles = chosen_cycles(&opt, preset);
    snprintf(stem, sizeof stem, "chimes_%lu_%s_%d", (unsigned long) seed,
             number_text(number, sizeof number, plan.bins), cycles);
    if (output_paths(opt.out, stem, plan.rate, audio_path, score_path) != 0) return 1;

    status = -1;
    score_init(&score);
    if (compose(&opt, preset, seed, cycles, &score) == 0) {
        text_add(&command, "chimes make");
        text_add_word(&command, "--rules");
        text_add_word(&command, score.rules);
        text_add_word(&command, "--seed");
        text_add_word(&command, number_text(number, sizeof number, (double) score.seed));
        text_add_word(&command, "--cycles");
        text_add_word(&command, number_text(number, sizeof number, (double) score.cycles));
        status = render_and_record(&score, &plan, &command, audio_path, score_path);
    }
    score_free(&score);
    return status == 0 ? 0 : 1;
}

static int command_score(int argc, char **argv)
{
    Options opt;
    const Preset *preset;
    Score score;
    FILE *file = stdout;
    int status;

    status = parse_options(argc, argv, 2, &opt, NULL);
    if (status != 0) return status < 0 ? 1 : 0;
    if (opt.patch || opt.have_bins || opt.have_drift || opt.have_rate || opt.have_gain) {
        fprintf(stderr, "chimes: --patch, --bins, --drift, --rate and --gain belong to rendering; "
                "score only composes\n");
        return 1;
    }
    if (!(preset = find_preset(opt.preset))) return 1;

    score_init(&score);
    if (compose(&opt, preset, chosen_seed(&opt), chosen_cycles(&opt, preset), &score) != 0) {
        score_free(&score);
        return 1;
    }
    if (opt.out && strcmp(opt.out, "-") != 0 && !(file = fopen(opt.out, "w"))) {
        fprintf(stderr, "chimes: cannot write '%s'\n", opt.out);
        score_free(&score);
        return 1;
    }
    if (score_write(file, &score) != 0) status = 1;
    /* A short score may still sit in the buffer: only now does a full disk
     * or a closed pipe show. */
    if (file == stdout) {
        if (fflush(stdout) != 0) status = 1;
    } else if (fclose(file) != 0) {
        status = 1;
    }
    if (status != 0) fprintf(stderr, "chimes: error writing the score\n");
    score_free(&score);
    return status;
}

/* Takes a tempo setting from the score's render lines, if it is recorded
 * there. Returns 0 (*value set, or left alone if there is no such line), or
 * -1 after printing a message: a line that cannot be used is not passed over
 * in silence. */
static int recorded_setting(const Score *score, const char *path, const char *key,
                            ProblemFn problem_with, double *value)
{
    const char *recorded = score_render_line(score, key);
    const char *problem;
    char text[64];
    double number;

    if (!recorded) return 0;
    if (parse_number(recorded, &number) != 0) problem = "it is not a number";
    else problem = problem_with(number, text, sizeof text);
    if (problem) {
        fprintf(stderr, "chimes: %s: the line '# render: %s %s' cannot be used: %s. Correct or "
                "remove that line, or give --%s\n", path, key, recorded, problem, key);
        return -1;
    }
    *value = number;
    return 0;
}

/* Works out the settings of a render. Patch, tempo and drift come from the
 * command line; what is not given there, from the score's render lines; and
 * if the score has none, or a preset was asked for, from the preset. Sample
 * rate and output gain come from the command line only. Returns 0, or -1
 * after printing a message. */
static int render_settings(const Options *opt, const Preset *preset, const Score *score,
                           const char *path, RenderPlan *plan)
{
    int use_score_lines = !opt->preset;
    const char *recorded_patch = use_score_lines ? score_render_line(score, "patch") : NULL;

    plan->bins = preset->bins;
    plan->drift = preset->drift;
    if (opt->have_bins) {
        plan->bins = opt->bins;
    } else if (use_score_lines) {
        if (recorded_setting(score, path, "bins", bins_problem, &plan->bins) != 0) return -1;
    }
    if (opt->have_drift) {
        plan->drift = opt->drift;
    } else if (use_score_lines) {
        if (recorded_setting(score, path, "drift", drift_problem, &plan->drift) != 0) return -1;
    }

    if (opt->patch) {
        plan->patch = find_patch(opt->patch);
    } else if (recorded_patch) {
        plan->patch = patch_find(recorded_patch);
        if (!plan->patch) {
            fprintf(stderr, "chimes: %s: the line '# render: patch %s' cannot be used: there is no "
                    "such patch. Correct or remove that line, or give --patch; known patches:",
                    path, recorded_patch);
            print_patch_names(stderr);
            fprintf(stderr, "\n");
        }
    } else {
        plan->patch = find_patch(preset->patch);
    }
    if (!plan->patch) return -1;

    plan->rate = opt->have_rate ? opt->rate : RENDER_DEFAULT_RATE;
    plan->have_gain = opt->have_gain;
    plan->gain = opt->gain;
    return 0;
}

static int command_render(int argc, char **argv)
{
    Options opt;
    const char *path = NULL;
    const Preset *preset;
    RenderPlan plan;
    Score score;
    Text command = { "", 0, 0 };
    FILE *file;
    char err[256];
    char stem[PATH_LEN], audio_path[PATH_LEN], score_path[PATH_LEN], shown_path[PATH_LEN + 2];
    int status;

    status = parse_options(argc, argv, 2, &opt, &path);
    if (status != 0) return status < 0 ? 1 : 0;
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

    /* Where the files go: beside the score file, unless -o says otherwise. */
    if (with_extension(stem, path, "") != 0) {
        fprintf(stderr, "chimes: the path of the score file is too long\n");
        return 1;
    }
    if (output_paths(opt.out, stem, opt.have_rate ? opt.rate : RENDER_DEFAULT_RATE,
                     audio_path, score_path) != 0)
        return 1;
    if (strcmp(audio_path, path) == 0) {
        fprintf(stderr, "chimes: the audio would overwrite the score file it is made from; "
                "give -o another name\n");
        return 1;
    }

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

    status = render_settings(&opt, preset, &score, path, &plan);
    if (status == 0) {
        /* A score whose name begins with a dash is recorded as ./-name; the
         * command line would take the bare name for an option. */
        snprintf(shown_path, sizeof shown_path, "%s%s", score_path[0] == '-' ? "./" : "", score_path);
        text_add(&command, "chimes render");
        text_add_word(&command, shown_path);
        status = render_and_record(&score, &plan, &command, audio_path, score_path);
    }
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
