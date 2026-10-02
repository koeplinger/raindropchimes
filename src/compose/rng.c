/* rng.c -- the Linux (glibc) srand()/rand(), rebuilt so that it gives the same
 * numbers on any computer. Adapted from docs/reference/glibc_rand.c.
 *
 * The generator keeps a table of 31 numbers. Each new number is the sum of
 * two table entries that lie 3 apart (wrapping at 2^32); the output is that
 * sum without its lowest bit.
 */
#include "compose/rng.h"

#define RNG_TABLE_SIZE 31
#define RNG_WARM_UP    310   /* glibc throws away the first 10 * 31 numbers */

int32_t rng_next(Rng *rng)
{
    uint32_t value;

    rng->r[rng->f] += rng->r[rng->b];      /* wraps at 2^32 */
    value = rng->r[rng->f] >> 1;           /* drop the lowest bit */
    if (++rng->f >= RNG_TABLE_SIZE) rng->f = 0;
    if (++rng->b >= RNG_TABLE_SIZE) rng->b = 0;
    return (int32_t) value;
}

void rng_seed(Rng *rng, uint32_t seed)
{
    int32_t word;
    int i;

    if (seed == 0) seed = 1;               /* glibc treats seed 0 as seed 1 */
    rng->r[0] = seed;

    /* glibc fills the table with signed 32-bit arithmetic, so a seed of 2^31
     * or more counts as a negative number here. */
    if (seed <= 0x7fffffffu) word = (int32_t) seed;
    else word = (int32_t) -(int64_t) (0x100000000ull - seed);

    for (i = 1; i < RNG_TABLE_SIZE; i++) {
        /* word = (16807 * word) mod 2147483647, done in two halves so that
         * nothing overflows. C99 division rounds toward zero, as glibc
         * relies on. */
        int32_t hi = word / 127773;
        int32_t lo = word % 127773;
        word = 16807 * lo - 2836 * hi;
        if (word < 0) word += 2147483647;
        rng->r[i] = (uint32_t) word;
    }

    rng->f = 3;
    rng->b = 0;
    for (i = 0; i < RNG_WARM_UP; i++) (void) rng_next(rng);
}
