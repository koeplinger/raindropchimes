/* score_selftest.c -- the score in memory and as text: round trips, and what
 * score_check and score_read must refuse (plan section 7, item 4). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "score/score_text.h"

static int failures = 0;

static void expect(int condition, const char *what)
{
    if (!condition) {
        printf("FAILED: %s\n", what);
        failures++;
    }
}

static Tone tone(int tick, int parent, int num, int den, double freq, int reps, double level)
{
    Tone t;
    t.tick = tick; t.parent = parent; t.num = num; t.den = den; t.freq_hz = freq;
    t.pan = 0.5; t.reps = reps; t.level = level; t.jitter = parent < 0 ? -1 : 7;
    return t;
}

/* A small sound score: root in slot 0, two children. */
static void build(Score *score)
{
    Tone t;
    score_init(score);
    snprintf(score->rules, sizeof score->rules, "test");
    snprintf(score->generator, sizeof score->generator, "glibc");
    score->seed = 4294967295u; score->cycles = 40; score->slots = 11;
    t = tone(0, -1, 0, 0, 440.0, 2, 1.0); score_add(score, &t);
    t = tone(1, 0, 3, 2, 440.0 * (3.0 / 2.0), 1, 0.223691167760108); score_add(score, &t);
    t = tone(2, 1, 1, 3, 440.0 * (3.0 / 2.0) * (1.0 / 3.0), 3, 0.0); score_add(score, &t);
}

static int check_fails(Score *score, const char *expected_word, const char *what)
{
    char err[256] = "";
    int refused = score_check(score, err, sizeof err) != 0;
    expect(refused, what);
    if (refused && !strstr(err, expected_word)) {
        printf("FAILED: %s: message was \"%s\"\n", what, err);
        failures++;
    }
    return refused;
}

/* Writes text to a temporary file and tries to read it as a score. */
static int reads(const char *text)
{
    Score score;
    char err[256];
    FILE *file = tmpfile();
    int status;
    if (!file) { printf("FAILED: tmpfile\n"); failures++; return 0; }
    fputs(text, file);
    rewind(file);
    score_init(&score);
    status = score_read(file, &score, err, sizeof err);
    fclose(file);
    score_free(&score);
    return status == 0;
}

#define HEADER "# raindropchimes score 1\n@ rules t\n@ generator glibc\n@ seed 1\n@ cycles 40\n@ slots 11\n"

int main(int argc, char **argv)
{
    Score score, copy;
    char err[256];
    FILE *file;
    Tone t;

    /* 1. A sound score passes, and survives write -> read -> write unchanged. */
    build(&score);
    expect(score_check(&score, err, sizeof err) == 0, "a sound score passes score_check");
    score_set_render_note(&score, "bins", "1700");
    score_set_render_note(&score, "command", "chimes make --seed 1 -o a b.wav");
    score_set_render_note(&score, "bins", "3401");
    expect(strcmp(score_render_note(&score, "bins"), "3401") == 0, "a render note can be replaced");
    {
        char first[4096], second[4096];
        size_t n1, n2;
        file = tmpfile();
        score_write(file, &score);
        rewind(file);
        n1 = fread(first, 1, sizeof first - 1, file); first[n1] = '\0';
        rewind(file);
        score_init(&copy);
        expect(score_read(file, &copy, err, sizeof err) == 0, "a written score reads back");
        fclose(file);
        file = tmpfile();
        score_write(file, &copy);
        rewind(file);
        n2 = fread(second, 1, sizeof second - 1, file); second[n2] = '\0';
        fclose(file);
        expect(n1 == n2 && strcmp(first, second) == 0, "write -> read -> write gives the same text");
        expect(copy.n_tones == 3 && copy.seed == 4294967295u && copy.cycles == 40, "settings survive");
        expect(copy.tones[1].level == 0.223691167760108, "a continuous level reads back exactly");
        expect(copy.tones[2].freq_hz == score.tones[2].freq_hz, "frequencies are recomputed exactly");
        expect(strcmp(score_render_note(&copy, "command"), "chimes make --seed 1 -o a b.wav") == 0,
               "render notes survive, spaces included");
        expect(strstr(first, " 0.223691167760108 ") != NULL, "levels print with the fewest digits");
        score_free(&copy);
    }
    score_free(&score);

    /* 2. What score_check must refuse. */
    build(&score); score.tones[2].tick = 1;  check_fails(&score, "increase", "ticks must increase"); score_free(&score);
    build(&score); score.tones[1].parent = -1; check_fails(&score, "root", "only one root"); score_free(&score);
    build(&score); score.tones[0].parent = 0; check_fails(&score, "root", "the first tone must be the root"); score_free(&score);
    build(&score); score.tones[2].parent = 2; check_fails(&score, "earlier", "a parent must be an earlier tone"); score_free(&score);
    build(&score); score.tones[2].parent = 5; check_fails(&score, "earlier", "a parent must exist"); score_free(&score);
    build(&score); score.tones[1].freq_hz *= 1.0000001; check_fails(&score, "ratio", "frequency must be parent * ratio"); score_free(&score);
    build(&score); score.tones[1].reps = 0; check_fails(&score, "repetition", "repetition count below 1"); score_free(&score);
    build(&score); score.tones[1].den = 0; check_fails(&score, "ratio", "zero denominator"); score_free(&score);
    build(&score); score.tones[1].level = 1.5; check_fails(&score, "level", "level above 1"); score_free(&score);
    build(&score); score.tones[1].pan = -0.1; check_fails(&score, "pan", "pan below 0"); score_free(&score);
    build(&score); score.slots = 0; check_fails(&score, "slots", "no slots"); score_free(&score);
    build(&score);   /* a second tone in slot 0 while the root (reps 2) still holds it until tick 33 */
    t = tone(22, 2, 2, 1, score.tones[2].freq_hz * 2.0, 1, 1.0); score_add(&score, &t);
    check_fails(&score, "held", "two tones may not hold the same slot at the same time"); score_free(&score);
    build(&score);   /* ... but at tick 33 the slot is free */
    t = tone(33, 2, 2, 1, score.tones[2].freq_hz * 2.0, 1, 1.0); score_add(&score, &t);
    expect(score_check(&score, err, sizeof err) == 0, "a slot is free once its tone's strikes are done"); score_free(&score);

    /* 3. What score_read must refuse. */
    expect(reads(HEADER "0 0 0 - - 440 0.50 2 1 -\n1 0 1 0 3/2 660 0.50 1 1 7\n"), "a plain file reads");
    expect(!reads(""), "an empty file is refused");
    expect(!reads("# some other file\n" "0 0 0 - - 440 0.50 2 1 -\n"), "a wrong first line is refused");
    expect(!reads("# raindropchimes score 2\n@ slots 11\n0 0 0 - - 440 0.50 2 1 -\n"), "another format version is refused");
    expect(!reads("# raindropchimes score 1\n0 0 0 - - 440 0.50 2 1 -\n"), "tones before '@ slots' are refused");
    expect(!reads(HEADER "0 0 0 - - 440 0.50 2 1\n"), "a short tone line is refused");
    expect(!reads(HEADER "0 0 0 - - 440 0.50 2 1 - 9\n"), "a long tone line is refused");
    expect(!reads(HEADER "0 0 1 - - 440 0.50 2 1 -\n"), "a wrong slot column is refused");
    expect(!reads(HEADER "0 0 0 - - 440 0.50 2 1 -\n1 0 1 0 3/2 700 0.50 1 1 7\n"), "a frequency that does not follow from its lineage is refused");
    expect(!reads(HEADER "0 0 0 - - 440 0.50 2 1 -\n1 0 1 4 3/2 660 0.50 1 1 7\n"), "an unknown parent is refused");
    expect(!reads(HEADER "0 0 0 - - 440 0.50 2 1 -\n1 0 1 0 3/0 660 0.50 1 1 7\n"), "a zero denominator is refused");
    expect(!reads(HEADER "0 0 0 - - 440x 0.50 2 1 -\n"), "trailing junk in a number is refused");
    expect(!reads(HEADER "@ tempo 3\n0 0 0 - - 440 0.50 2 1 -\n"), "an unknown setting is refused");
    expect(!reads(HEADER), "a score without tones is refused");

    /* 4. A given file: read, write to standard output (the check script compares). */
    if (argc > 1) {
        file = fopen(argv[1], "r");
        score_init(&score);
        if (!file || score_read(file, &score, err, sizeof err) != 0) {
            printf("FAILED: reading %s: %s\n", argv[1], file ? err : "cannot open");
            failures++;
        } else {
            FILE *out = fopen(argv[2], "w");
            score_write(out, &score);
            fclose(out);
        }
        if (file) fclose(file);
        score_free(&score);
    }

    if (failures == 0) printf("score self-test: all passed\n");
    return failures == 0 ? 0 : 1;
}
