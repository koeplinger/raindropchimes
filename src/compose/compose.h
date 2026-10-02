/* compose.h -- composing algorithms. An algorithm takes rules, a seeded
 * generator and a number of cycles, and fills a score. It knows nothing about
 * seconds, samples or sound.
 */
#ifndef CHIMES_COMPOSE_H
#define CHIMES_COMPOSE_H

#include <stddef.h>

#include "compose/rng.h"
#include "compose/rules.h"
#include "score/tone.h"

#define COMPOSE_MIN_CYCLES_MARGIN 2   /* cycles must be at least end_cycles + 2 (v4: 32) */

/* The slot algorithm of 14 February 2011 (docs/REVAMP_PLAN.md, Appendix A).
 * Fills score->tones and sets score->rules, cycles and slots; the caller
 * records the seed and the generator's name. No tones are created in the end
 * phase. Returns 0, or -1 with a message in err. */
int slotwalk_compose(const Rules *rules, Rng *rng, int cycles, Score *score,
                     char *err, size_t errlen);

#endif
