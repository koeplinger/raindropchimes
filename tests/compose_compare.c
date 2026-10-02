/* compose_compare.c -- test helper: compares two tone lists line by line and
 * shows the first line that differs.
 *
 *   compose_compare exact EXPECTED COMPOSED    every line must be identical
 *   compose_compare loose EXPECTED COMPOSED    the frequency and level columns
 *                                              may differ in their last digits
 *
 * "loose" is for computers other than Linux, whose maths library may round
 * the very first frequency, and the levels of the continuous level rule,
 * differently in the last digit. Everything else must still be identical,
 * and frequencies and levels must agree to 12 digits.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_LEN     1024
#define FIELDS       10
#define FREQ_FIELD   5       /* tick cycle slot parent ratio FREQ_HZ pan reps LEVEL jitter */
#define LEVEL_FIELD  8
#define LAST_DIGITS  1e-12

/* Reads one line without its line end. Returns 0 at the end of the file. */
static int read_line(FILE *file, char *line)
{
    if (!fgets(line, LINE_LEN, file)) return 0;
    line[strcspn(line, "\r\n")] = '\0';
    return 1;
}

/* Splits a line at its spaces. Returns the number of fields (at most FIELDS + 1). */
static int split(char *line, char *field[FIELDS + 1])
{
    int n = 0;
    char *token;
    for (token = strtok(line, " "); token && n <= FIELDS; token = strtok(NULL, " "))
        field[n++] = token;
    return n;
}

/* Are the two tone lines the same apart from the last digits of the
 * frequency and of the level? */
static int same_but_last_digits(const char *expected, const char *composed)
{
    char a[LINE_LEN], b[LINE_LEN];
    char *fa[FIELDS + 1], *fb[FIELDS + 1];
    int i;

    snprintf(a, sizeof a, "%s", expected);
    snprintf(b, sizeof b, "%s", composed);
    if (split(a, fa) != FIELDS || split(b, fb) != FIELDS) return 0;

    for (i = 0; i < FIELDS; i++) {
        if (i == FREQ_FIELD || i == LEVEL_FIELD) {
            double x = strtod(fa[i], NULL), y = strtod(fb[i], NULL);
            if (!(fabs(x - y) <= LAST_DIGITS * fabs(x))) return 0;
        } else if (strcmp(fa[i], fb[i]) != 0) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    FILE *expected_file, *composed_file;
    char expected[LINE_LEN], composed[LINE_LEN];
    long line = 0, last_digit_lines = 0;
    int loose, status = 0;

    if (argc != 4 || (strcmp(argv[1], "exact") != 0 && strcmp(argv[1], "loose") != 0)) {
        fprintf(stderr, "usage: compose_compare exact|loose EXPECTED COMPOSED\n");
        return 2;
    }
    loose = strcmp(argv[1], "loose") == 0;
    expected_file = fopen(argv[2], "r");
    composed_file = fopen(argv[3], "r");
    if (!expected_file || !composed_file) {
        fprintf(stderr, "compose_compare: cannot open %s\n", expected_file ? argv[3] : argv[2]);
        return 2;
    }

    for (;;) {
        int has_expected = read_line(expected_file, expected);
        int has_composed = read_line(composed_file, composed);

        if (!has_expected && !has_composed) break;
        line++;
        if (has_expected && has_composed && strcmp(expected, composed) == 0) continue;
        if (loose && has_expected && has_composed && same_but_last_digits(expected, composed)) {
            last_digit_lines++;
            continue;
        }
        printf("first difference, at line %ld:\n", line);
        printf("  expected: %s\n", has_expected ? expected : "(the list has ended)");
        printf("  composed: %s\n", has_composed ? composed : "(the list has ended)");
        status = 1;
        break;
    }

    if (status == 0 && last_digit_lines > 0)
        printf("note: %ld of %ld lines differ only in the last digits of the frequency or "
               "the level (this computer's maths library rounds differently)\n",
               last_digit_lines, line);
    else if (status == 0)
        printf("%ld lines, all identical\n", line);

    fclose(expected_file);
    fclose(composed_file);
    return status;
}
