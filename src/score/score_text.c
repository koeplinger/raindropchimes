/* score_text.c -- write and read a score file. */
#include "score/score_text.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SCORE_FIRST_LINE "# raindropchimes score 1"
#define RENDER_PREFIX    "# render: "

/* The fewest digits that read back as the same number: 0, 1 and 0.7 print as
 * written. */
static void format_shortest(char *text, size_t len, double value)
{
    int precision;
    for (precision = 1; precision <= 17; precision++) {
        snprintf(text, len, "%.*g", precision, value);
        if (strtod(text, NULL) == value) return;
    }
}

int score_write_tone(FILE *file, const Score *score, const Tone *tone)
{
    char level[32];
    format_shortest(level, sizeof level, tone->level);

    fprintf(file, "%d %d %d ", (int) tone->tick, (int) (tone->tick / score->slots),
            (int) (tone->tick % score->slots));
    if (tone->parent < 0) fprintf(file, "- - ");
    else fprintf(file, "%d %d/%d ", (int) tone->parent, (int) tone->num, (int) tone->den);
    fprintf(file, "%.17g %.2f %d %s ", tone->freq_hz, tone->pan, (int) tone->reps, level);
    if (tone->jitter < 0) fprintf(file, "-\n");
    else fprintf(file, "%d\n", (int) tone->jitter);
    return ferror(file) ? -1 : 0;
}

int score_write(FILE *file, const Score *score)
{
    int32_t i;

    fprintf(file, "%s\n", SCORE_FIRST_LINE);
    fprintf(file, "@ rules %s\n", score->rules);
    fprintf(file, "@ generator %s\n", score->generator);
    fprintf(file, "@ seed %lu\n", (unsigned long) score->seed);
    fprintf(file, "@ cycles %d\n", (int) score->cycles);
    fprintf(file, "@ slots %d\n", (int) score->slots);
    for (i = 0; i < score->n_render; i++)
        fprintf(file, "%s%s %s\n", RENDER_PREFIX, score->render[i].key, score->render[i].value);
    fprintf(file, "# tick cycle slot parent ratio freq_hz pan reps level jitter\n");
    for (i = 0; i < score->n_tones; i++)
        if (score_write_tone(file, score, &score->tones[i]) != 0) return -1;
    return ferror(file) ? -1 : 0;
}

/* ------------------------------------------------------------------ reading */

static void strip_line_end(char *line)
{
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
}

/* Parses a whole number that fills the whole field. */
static int parse_int(const char *field, long min, long max, int32_t *value)
{
    char *end;
    long parsed;
    errno = 0;
    parsed = strtol(field, &end, 10);
    if (end == field || *end != '\0' || errno != 0 || parsed < min || parsed > max) return -1;
    *value = (int32_t) parsed;
    return 0;
}

static int parse_double(const char *field, double *value)
{
    char *end;
    *value = strtod(field, &end);
    if (end == field || *end != '\0' || !isfinite(*value)) return -1;
    return 0;
}

/* "@ key value" */
static int read_setting(Score *score, char *line)
{
    char *key = strtok(line + 1, " ");
    char *value = strtok(NULL, " ");
    int32_t number;

    if (!key || !value || strtok(NULL, " ")) return -1;
    if (strcmp(key, "rules") == 0) {
        snprintf(score->rules, sizeof score->rules, "%s", value);
    } else if (strcmp(key, "generator") == 0) {
        snprintf(score->generator, sizeof score->generator, "%s", value);
    } else if (strcmp(key, "seed") == 0) {
        char *end;
        unsigned long seed;
        errno = 0;
        seed = strtoul(value, &end, 10);
        if (end == value || *end != '\0' || errno != 0 || seed > 0xfffffffful) return -1;
        score->seed = (uint32_t) seed;
    } else if (strcmp(key, "cycles") == 0) {
        if (parse_int(value, 1, 100000000, &number) != 0) return -1;
        score->cycles = number;
    } else if (strcmp(key, "slots") == 0) {
        if (parse_int(value, 1, 100000, &number) != 0) return -1;
        score->slots = number;
    } else {
        return -1;
    }
    return 0;
}

/* "# render: key value with spaces". Returns 0, or -1 if the line cannot be
 * kept: no value, too long, or too many such lines. */
static int read_render_line(Score *score, char *line)
{
    char *key = line + strlen(RENDER_PREFIX);
    char *space = strchr(key, ' ');
    if (!space) return -1;
    *space = '\0';
    return score_set_render_line(score, key, space + 1);
}

/* "tick cycle slot parent ratio freq_hz pan reps level jitter" */
static int read_tone(Score *score, char *line, double *printed_freq, Tone *tone)
{
    char *field[10];
    int n = 0;
    int32_t cycle, slot;
    char *token;

    for (token = strtok(line, " "); token; token = strtok(NULL, " ")) {
        if (n == 10) return -1;
        field[n++] = token;
    }
    if (n != 10) return -1;

    if (parse_int(field[0], 0, INT32_MAX, &tone->tick) != 0) return -1;
    if (parse_int(field[1], 0, INT32_MAX, &cycle) != 0) return -1;
    if (parse_int(field[2], 0, INT32_MAX, &slot) != 0) return -1;
    if (cycle != tone->tick / score->slots || slot != tone->tick % score->slots) return -1;

    if (strcmp(field[3], "-") == 0) {
        if (strcmp(field[4], "-") != 0) return -1;
        tone->parent = -1;
        tone->num = tone->den = 0;
    } else {
        char *slash = strchr(field[4], '/');
        if (!slash) return -1;
        *slash = '\0';
        if (parse_int(field[3], 0, INT32_MAX, &tone->parent) != 0) return -1;
        if (parse_int(field[4], 1, INT32_MAX, &tone->num) != 0) return -1;
        if (parse_int(slash + 1, 1, INT32_MAX, &tone->den) != 0) return -1;
    }
    if (parse_double(field[5], printed_freq) != 0) return -1;
    if (parse_double(field[6], &tone->pan) != 0) return -1;
    if (parse_int(field[7], 1, INT32_MAX, &tone->reps) != 0) return -1;
    if (parse_double(field[8], &tone->level) != 0) return -1;
    if (strcmp(field[9], "-") == 0) tone->jitter = -1;
    else if (parse_int(field[9], 0, INT32_MAX, &tone->jitter) != 0) return -1;
    return 0;
}

int score_read(FILE *file, Score *score, char *err, size_t errlen)
{
    char line[1024];
    long line_number = 0;

    while (fgets(line, sizeof line, file)) {
        line_number++;
        if (!strchr(line, '\n') && !feof(file)) {
            snprintf(err, errlen, "line %ld: too long", line_number);
            return -1;
        }
        strip_line_end(line);

        if (line_number == 1) {
            if (strcmp(line, SCORE_FIRST_LINE) != 0) {
                snprintf(err, errlen, "line 1: expected \"%s\"", SCORE_FIRST_LINE);
                return -1;
            }
        } else if (strncmp(line, RENDER_PREFIX, strlen(RENDER_PREFIX)) == 0) {
            if (read_render_line(score, line) != 0) {
                snprintf(err, errlen, "line %ld: not a usable \"# render: key value\" line (no "
                         "value, a key or value that is too long, or more than %d such lines)",
                         line_number, SCORE_RENDER_LINES);
                return -1;
            }
        } else if (line[0] == '#' || line[0] == '\0') {
            continue;
        } else if (line[0] == '@') {
            if (read_setting(score, line) != 0) {
                snprintf(err, errlen, "line %ld: not a valid \"@ key value\" setting", line_number);
                return -1;
            }
        } else {
            Tone tone;
            double printed_freq;

            if (score->slots < 1) {
                snprintf(err, errlen, "line %ld: \"@ slots\" must come before the first tone",
                         line_number);
                return -1;
            }
            if (read_tone(score, line, &printed_freq, &tone) != 0) {
                snprintf(err, errlen, "line %ld: not a valid tone line", line_number);
                return -1;
            }
            /* Lineage is the truth: only the root's frequency is trusted. */
            if (tone.parent < 0) {
                tone.freq_hz = printed_freq;
            } else {
                const Tone *parent = score_tone_at(score, tone.parent);
                if (!parent) {
                    snprintf(err, errlen, "line %ld: parent %d is not an earlier tone",
                             line_number, (int) tone.parent);
                    return -1;
                }
                tone.freq_hz = parent->freq_hz * ((double) tone.num / (double) tone.den);
                if (fabs(printed_freq - tone.freq_hz) > 1e-9 * tone.freq_hz) {
                    snprintf(err, errlen, "line %ld: frequency %.17g does not follow from its "
                             "lineage (%.17g)", line_number, printed_freq, tone.freq_hz);
                    return -1;
                }
            }
            if (score_add(score, &tone) != 0) {
                snprintf(err, errlen, "out of memory");
                return -1;
            }
        }
    }
    if (ferror(file)) {
        snprintf(err, errlen, "read error");
        return -1;
    }
    if (line_number == 0) {
        snprintf(err, errlen, "empty file");
        return -1;
    }
    return score_check(score, err, errlen);
}
