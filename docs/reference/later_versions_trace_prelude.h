// instrumentation prelude: force-included before the original harmonics.c
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
static FILE *rlog;
static long nrand_calls;
static unsigned long pm_state; static int rng_mode = -1;
static void my_srand(unsigned seed) { if (rng_mode < 0) rng_mode = (getenv("RNG") && getenv("RNG")[0]=='b') ? 1 : 0; pm_state = seed; srand(seed); }
static int raw_rand(void) {
    if (rng_mode == 1) { /* Apple/BSD libc rand(): Park-Miller minimal standard */
        long hi, lo, x; if (pm_state == 0) pm_state = 123459876;
        hi = pm_state / 127773; lo = pm_state % 127773; x = 16807 * lo - 2836 * hi; if (x < 0) x += 0x7fffffff;
        pm_state = x; return (int)(x % 0x80000000UL); }
    return rand();
}
static int logrand(int line, int songTime, int curSlot, int densityCnt, int curBin) {
    int r = raw_rand();
    if (!rlog) rlog = fopen(getenv("RLOG") ? getenv("RLOG") : "rand.log", "w");
    fprintf(rlog, "%ld line=%d songTime=%d curSlot=%d densityCnt=%d curBin=%d r=%d\n", nrand_calls++, line, songTime, curSlot, densityCnt, curBin, r);
    return r;
}
static time_t mytime(void) { return getenv("SEED") ? (time_t)atol(getenv("SEED")) : time(NULL); }
#define rand() logrand(__LINE__, songTime, curSlot, densityCnt, curBin)
#define time(x) mytime()
#define srand(x) my_srand(x)
#ifdef BPS
#define OVERRIDE_PARAMS 1
#endif
#ifdef BPS
#define BPS_OR(x) BPS
#else
#define BPS_OR(x) x
#endif
#ifdef TST
#define TST_OR(x) TST
#else
#define TST_OR(x) x
#endif
static FILE *elog;
static long total_samples_written;
#define EVT(tag) do { if (!elog) elog = fopen(getenv("ELOG") ? getenv("ELOG") : "events.log", "w"); \
  fprintf(elog, "%s sample=%ld songTime=%d slot=%d f=%.6f pos=%.2f cnt=%d play=%g vib=%.1f dens=%d bps=%d\n", tag, total_samples_written, songTime, curSlot, slotFrequency[curSlot], slotPos[curSlot], slotAmplitudeCnt[curSlot], (double)slotPlay[curSlot], vibratoBins[curSlot], densityCnt, binsPerSlot); } while (0)
