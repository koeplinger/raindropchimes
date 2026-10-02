/* rng.h -- the random generator: the Linux (glibc) srand()/rand(), built in.
 *
 * The 2011 tunes were made with this generator, so a seed gives the same tune
 * on any computer. The system's own rand() is never called.
 */
#ifndef CHIMES_RNG_H
#define CHIMES_RNG_H

#include <stdint.h>

#define RNG_NAME "glibc"

typedef struct {
    uint32_t r[31];   /* state table              */
    int      f, b;    /* front and rear positions */
} Rng;

void    rng_seed(Rng *rng, uint32_t seed);
int32_t rng_next(Rng *rng);              /* 0 .. 2147483647 */

#endif
