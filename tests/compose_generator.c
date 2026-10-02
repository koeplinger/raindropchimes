/* compose_generator.c -- test helper: is the built-in generator the Linux one?
 *
 *   compose_generator         known answer: seed 1 gives 1804289383, 846930886.
 *                             Where this computer's own rand() is the Linux
 *                             (glibc) one, the two are also compared on a few
 *                             thousand seeds. Exit status 0 = all good.
 *   compose_generator host    exit status 0 if this computer's own rand() is
 *                             the Linux one, else 1. Prints nothing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compose/rng.h"

#define DRAWS_PER_SEED 5000   /* a 500-cycle tune uses about 3,500 */
#define RANDOM_SEEDS   2000
#define CLOCK_SEEDS    2000

static int known_answers(void)
{
    Rng rng;
    long first, second;
    int i;

    rng_seed(&rng, 1);
    first = rng_next(&rng);
    second = rng_next(&rng);
    if (first != 1804289383L || second != 846930886L) {
        printf("FAIL seed 1 gives %ld, %ld; expected 1804289383, 846930886\n", first, second);
        return 1;
    }

    /* Seeding again must start the same sequence again. */
    for (i = 0; i < 1000; i++) (void) rng_next(&rng);
    rng_seed(&rng, 1);
    if (rng_next(&rng) != 1804289383L) {
        printf("FAIL seeding a used generator again does not restart it\n");
        return 1;
    }

    /* The Linux generator treats seed 0 as seed 1. */
    rng_seed(&rng, 0);
    if (rng_next(&rng) != 1804289383L) {
        printf("FAIL seed 0 does not give the sequence of seed 1\n");
        return 1;
    }

    printf("known answers: ok\n");
    return 0;
}

/* Does this computer's own rand() give the Linux generator's known answers? */
static int host_is_linux(void)
{
    srand(1);
    return rand() == 1804289383L && rand() == 846930886L;
}

/* Returns 1 (after a message) if the two generators disagree for this seed. */
static int differs_from_host(unsigned int seed)
{
    Rng rng;
    int i;

    srand(seed);
    rng_seed(&rng, seed);
    for (i = 0; i < DRAWS_PER_SEED; i++) {
        long host = rand();
        long mine = rng_next(&rng);
        if (host != mine) {
            printf("FAIL seed %u, draw %d: this computer's rand() gives %ld, the built-in "
                   "generator %ld\n", seed, i, host, mine);
            return 1;
        }
    }
    return 0;
}

static int compare_with_host(void)
{
    static const unsigned int special[] = {
        0u, 1u, 2u, 42u, 127773u, 2147483646u, 2147483647u, 2147483648u, 2147483649u,
        4294967294u, 4294967295u, 1297735820u
    };
    unsigned int scattered = 2463534242u;
    int bad = 0, seeds = 0;
    size_t k;
    int i;

    if (!host_is_linux()) {
        printf("this computer's rand() is not the Linux one: comparison left out\n");
        return 0;
    }

    for (k = 0; k < sizeof special / sizeof special[0]; k++, seeds++)
        bad += differs_from_host(special[k]);

    /* Seeds scattered over the whole 32-bit range. */
    for (i = 0; i < RANDOM_SEEDS; i++, seeds++) {
        scattered = scattered * 1664525u + 1013904223u;
        bad += differs_from_host(scattered);
    }

    /* Clock seeds, as the program uses them: seconds from February 2011 on. */
    for (i = 0; i < CLOCK_SEEDS; i++, seeds++)
        bad += differs_from_host(1297573200u + (unsigned int) i * 83u);

    if (bad == 0)
        printf("compared with this computer's rand(): identical on %d seeds, %d draws each\n",
               seeds, DRAWS_PER_SEED);
    return bad;
}

int main(int argc, char **argv)
{
    int bad;

    if (argc == 2 && strcmp(argv[1], "host") == 0) return host_is_linux() ? 0 : 1;

    bad = known_answers() + compare_with_host();
    return bad ? 1 : 0;
}
