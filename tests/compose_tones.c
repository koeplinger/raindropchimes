/* compose_tones.c -- test helper: runs the composer and prints what it made.
 *
 *   compose_tones tones  RULES SEED CYCLES    the tone lines only
 *   compose_tones score  RULES SEED CYCLES    the whole score file
 *   compose_tones reread FILE                 reads a score file, writes it again
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compose/compose.h"
#include "score/score_text.h"

static int usage(void)
{
    fprintf(stderr, "usage: compose_tones tones|score RULES SEED CYCLES\n"
                    "       compose_tones reread FILE\n");
    return 2;
}

/* Composes into an initialised, empty score, the way the program will. */
static int compose(const char *rules_name, const char *seed_text, const char *cycles_text,
                   Score *score)
{
    const Rules *rules = rules_find(rules_name);
    uint32_t seed = (uint32_t) strtoul(seed_text, NULL, 10);
    int cycles = atoi(cycles_text);
    Rng rng;
    char err[300];

    if (!rules) {
        fprintf(stderr, "compose_tones: no rule set named %s\n", rules_name);
        return -1;
    }
    rng_seed(&rng, seed);
    if (slotwalk_compose(rules, &rng, cycles, score, err, sizeof err) != 0) {
        fprintf(stderr, "compose_tones: %s\n", err);
        return -1;
    }
    score->seed = seed;
    snprintf(score->generator, sizeof score->generator, "%s", RNG_NAME);
    return 0;
}

static int reread(const char *path, Score *score)
{
    FILE *file = fopen(path, "r");
    char err[300];
    int status;

    if (!file) {
        fprintf(stderr, "compose_tones: cannot open %s\n", path);
        return -1;
    }
    status = score_read(file, score, err, sizeof err);
    fclose(file);
    if (status != 0) fprintf(stderr, "compose_tones: %s: %s\n", path, err);
    return status;
}

int main(int argc, char **argv)
{
    Score score;
    int status = 1;
    int32_t i;

    if (argc < 2) return usage();
    score_init(&score);

    if (strcmp(argv[1], "tones") == 0 && argc == 5) {
        if (compose(argv[2], argv[3], argv[4], &score) == 0) {
            status = 0;
            for (i = 0; i < score.n_tones; i++)
                if (score_write_tone(stdout, &score, &score.tones[i]) != 0) status = 1;
        }
    } else if (strcmp(argv[1], "score") == 0 && argc == 5) {
        if (compose(argv[2], argv[3], argv[4], &score) == 0)
            status = score_write(stdout, &score) == 0 ? 0 : 1;
    } else if (strcmp(argv[1], "reread") == 0 && argc == 3) {
        if (reread(argv[2], &score) == 0)
            status = score_write(stdout, &score) == 0 ? 0 : 1;
    } else {
        status = usage();
    }

    score_free(&score);
    return status;
}
