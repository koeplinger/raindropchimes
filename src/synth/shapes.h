/* shapes.h -- the shapes a patch can name: wave forms, envelopes, loudness
 * laws, strike-gain laws and pan laws. Each kind is a plain list of names and
 * functions in shapes.c. To add one: write the function there and add one
 * line to its list.
 */
#ifndef CHIMES_SHAPES_H
#define CHIMES_SHAPES_H

#define SHAPES_TWO_PI 6.283185307179586476925

/* Wave form: the value (-1 .. 1) at a phase in radians. The voice keeps the
 * phase within 0 .. 2 pi.
 *
 * `highest` is the highest whole multiple of the tone's frequency that still
 * lies below half the sample rate. A wave form with overtones leaves out
 * those above it: the sample rate cannot carry them, and they would come out
 * as other, wrong frequencies. The plain cosine has no overtones and ignores
 * it. (A tone that itself lies at or above half the sample rate is not
 * sounded at all; the voice sees to that, so highest is at least 1 here.) */
typedef double (*WaveFn)(double phase, int highest);

/* Envelope of one strike: the gain at age u, in slots (0 <= u < slots). */
typedef double (*EnvelopeFn)(double u, int slots);

/* Loudness law: how loud a tone of this frequency is; 1.0 = full scale. */
typedef double (*LoudnessFn)(double freq_hz, double constant);

/* Strike-gain law: the gain of strike 0 .. reps of a tone with that
 * repetition count. */
typedef double (*StrikeGainFn)(int strike, int reps);

/* Pan law: the left and right gain for a pan of 0 (left) .. 1 (right). */
typedef void (*PanFn)(double pan, double *left, double *right);

/* The lists, by name. Each returns NULL for an unknown name. */
WaveFn       wave_find(const char *name);
EnvelopeFn   envelope_find(const char *name);
LoudnessFn   loudness_find(const char *name);
StrikeGainFn strike_gain_find(const char *name);
PanFn        pan_find(const char *name);

#endif
