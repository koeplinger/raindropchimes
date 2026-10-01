/* glibc_rand.c -- self-contained, portable re-implementation of glibc's
 * srand()/rand() (random_r TYPE_3: additive feedback generator of degree 31,
 * separation 3, i.e. r[i] = r[i-31] + r[i-3] mod 2^32, output = r[i] >> 1).
 *
 * Bit-compatible with glibc srand(seed); rand(); on every platform that has
 * <stdint.h>. RAND_MAX is 2147483647.
 *
 * Build the self-test (compares against the host libc, so it only passes on
 * glibc hosts):
 *     cc -O2 -DGLIBC_RAND_TEST -o glibc_rand_test glibc_rand.c && ./glibc_rand_test
 * Use as a library: compile without -DGLIBC_RAND_TEST and declare the three
 * prototypes below (or cut them into a header).
 */
#include <stdint.h>

#define GLIBC_RAND_MAX 2147483647

typedef struct {
    uint32_t r[31];   /* state table                                  */
    int      f;       /* "front" index (starts at 3)                  */
    int      b;       /* "rear"  index (starts at 0)                  */
} glibc_rand_t;

void glibc_srand(glibc_rand_t *g, unsigned int seed);
int  glibc_rand(glibc_rand_t *g);          /* 0 .. GLIBC_RAND_MAX        */

int glibc_rand(glibc_rand_t *g)
{
    uint32_t v;
    g->r[g->f] += g->r[g->b];              /* wraps mod 2^32             */
    v = g->r[g->f] >> 1;                   /* drop least-random bit      */
    if (++g->f >= 31) g->f = 0;
    if (++g->b >= 31) g->b = 0;
    return (int) v;
}

void glibc_srand(glibc_rand_t *g, unsigned int seed)
{
    int i;
    int32_t word;

    if (seed == 0) seed = 1;               /* glibc maps seed 0 to 1     */
    g->r[0] = (uint32_t) seed;
    /* glibc does this arithmetic in *signed* 32 bit; seeds >= 2^31 are
     * therefore negative here. Do the conversion without relying on
     * implementation-defined narrowing. */
    word = (seed <= 0x7fffffffu) ? (int32_t) seed
                                 : (int32_t) (-(int64_t) (0x100000000ull - seed));
    for (i = 1; i < 31; i++) {
        /* word = (16807 * word) % 2147483647 without overflow (Schrage);
         * C99 division truncates toward zero, as glibc relies on. */
        int32_t hi = word / 127773;
        int32_t lo = word % 127773;
        word = 16807 * lo - 2836 * hi;
        if (word < 0) word += 2147483647;
        g->r[i] = (uint32_t) word;
    }
    g->f = 3;
    g->b = 0;
    for (i = 0; i < 310; i++) (void) glibc_rand(g);   /* discard 10*31 */
}

#ifdef GLIBC_RAND_TEST
#include <stdio.h>
#include <stdlib.h>

static int check_seed(unsigned int seed, int n)
{
    glibc_rand_t g;
    int i;
    srand(seed);
    glibc_srand(&g, seed);
    for (i = 0; i < n; i++) {
        int a = rand(), b = glibc_rand(&g);
        if (a != b) {
            printf("MISMATCH seed=%u output #%d: libc=%d mine=%d\n", seed, i, a, b);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    static const unsigned int special[] = {
        0u, 1u, 2u, 42u, 127773u, 2147483646u, 2147483647u, 2147483648u,
        2147483649u, 4294967295u, 4294967294u, 1297735820u, 1297735862u
    };
    unsigned long nseeds = argc > 1 ? strtoul(argv[1], 0, 10) : 2000;
    int nout = argc > 2 ? atoi(argv[2]) : 10000;
    unsigned long k, bad = 0, total = 0;
    uint64_t x = 0x9E3779B97F4A7C15ull;    /* splitmix64 for test seeds  */

    for (k = 0; k < sizeof special / sizeof special[0]; k++, total++)
        bad += check_seed(special[k], nout);
    for (k = 0; k < nseeds; k++, total++) {            /* random 32-bit  */
        uint64_t z = (x += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        bad += check_seed((unsigned int) z, nout);
    }
    for (k = 0; k < nseeds; k++, total++)              /* epoch seconds  */
        bad += check_seed(1297573200u + (unsigned int) k * 83u, nout);

    if (RAND_MAX != GLIBC_RAND_MAX) { printf("RAND_MAX differs\n"); bad++; }
    printf("%lu seeds x %d outputs each: %s (%lu mismatching seeds)\n",
           total, nout, bad ? "FAIL" : "all identical to libc rand()", bad);
    {   /* known-answer test, independent of the host libc */
        glibc_rand_t g; glibc_srand(&g, 1u);
        int a = glibc_rand(&g), b = glibc_rand(&g);
        printf("KAT seed 1: %d %d (expect 1804289383 846930886): %s\n", a, b,
               (a == 1804289383 && b == 846930886) ? "ok" : "FAIL");
        if (!(a == 1804289383 && b == 846930886)) bad++;
    }
    return bad ? 1 : 0;
}
#endif
