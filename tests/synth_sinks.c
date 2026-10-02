/* synth_sinks.c -- checks the output formats: the 16-bit conversion of "wav"
 * with its saturation count and header, and the 32-bit float frames of "raw".
 *
 *   synth_sinks files <scratch directory>     writes and reads back both formats
 *   synth_sinks stdout                        writes the raw test frames to standard output
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "output/sink.h"
#include "synth_common.h"

#define N_FRAMES 6

/* Test frames, with what each sample must become in 16 bits. */
static const double FRAMES[2 * N_FRAMES] = {
    0.0,                 0.5,
    -0.5,                1.0,                 /* 1.0 is one step above the largest value */
    -1.0,                1.5,
    -1.5,                0.25 / 32768.0,
    0.5 / 32768.0,       -0.5 / 32768.0,      /* halves round away from zero */
    32767.4 / 32768.0,   -32768.4 / 32768.0,  /* just inside the limits      */
};
static const int EXPECTED[2 * N_FRAMES] = {
    0,       16384,
    -16384,  32767,     /* held */
    -32768,  32767,     /* held */
    -32768,  0,         /* held */
    1,       -1,
    32767,   -32768,
};
#define EXPECTED_SATURATED 3

static long read_file(const char *path, unsigned char *bytes, long max)
{
    FILE *file = fopen(path, "rb");
    long n;
    if (!file) fail("cannot open %s", path);
    n = (long) fread(bytes, 1, (size_t) max, file);
    fclose(file);
    return n;
}

static unsigned long get_u32(const unsigned char *b)
{
    return (unsigned long) b[0] | ((unsigned long) b[1] << 8) | ((unsigned long) b[2] << 16)
         | ((unsigned long) b[3] << 24);
}

static void check_wav(const char *dir)
{
    const SinkFormat *format = sink_format_find("wav");
    static const double invalid[2] = { 0.25, NAN };
    unsigned char bytes[256];
    char path[512];
    Sink *sink;
    long n, saturated;
    int i;

    if (!format) fail("there is no output format \"wav\"");
    if (format->open("-", 44100) != NULL) fail("wav: standard output should be refused");
    snprintf(path, sizeof path, "%s/no-such-directory/x.wav", dir);
    if (format->open(path, 44100) != NULL) fail("wav: a path that cannot be opened was accepted");

    snprintf(path, sizeof path, "%s/sinks.wav", dir);
    sink = format->open(path, 48000);
    if (!sink) fail("cannot open %s", path);
    if (sink->write(sink, FRAMES, 2) != 0) fail("wav: write failed");
    if (sink->write(sink, invalid, 1) == 0) fail("wav: a value that is not a number was accepted");
    if (sink->write(sink, FRAMES + 4, N_FRAMES - 2) != 0) fail("wav: write failed");
    saturated = sink->saturated;
    if (sink->close(sink) != 0) fail("wav: close failed");

    printf("wav: %ld samples saturated (expected %d)\n", saturated, EXPECTED_SATURATED);
    if (saturated != EXPECTED_SATURATED) fail("wav: wrong saturation count");

    n = read_file(path, bytes, (long) sizeof bytes);
    if (n != 44 + 4 * N_FRAMES) fail("wav: the file is %ld bytes, expected %d", n, 44 + 4 * N_FRAMES);
    if (memcmp(bytes, "RIFF", 4) != 0 || memcmp(bytes + 8, "WAVEfmt ", 8) != 0
        || memcmp(bytes + 36, "data", 4) != 0) fail("wav: header tags are wrong");
    if (get_u32(bytes + 4) != 36 + 4 * N_FRAMES) fail("wav: wrong RIFF length");
    if (get_u32(bytes + 16) != 16 || get_u32(bytes + 20) != 0x00020001UL) fail("wav: not 2-channel PCM");
    if (get_u32(bytes + 24) != 48000 || get_u32(bytes + 28) != 192000) fail("wav: wrong sample rate");
    if (get_u32(bytes + 32) != 0x00100004UL) fail("wav: not 4 bytes per frame at 16 bits");
    if (get_u32(bytes + 40) != 4 * N_FRAMES) fail("wav: wrong data length");
    for (i = 0; i < 2 * N_FRAMES; i++) {
        int value = bytes[44 + 2 * i] | (bytes[45 + 2 * i] << 8);
        if (value >= 32768) value -= 65536;
        if (value != EXPECTED[i]) fail("wav: sample %d is %d, expected %d", i, value, EXPECTED[i]);
    }
    printf("wav: header and all %d samples as expected\n", 2 * N_FRAMES);
}

static void check_raw(const char *dir)
{
    const SinkFormat *format = sink_format_find("raw");
    static const double invalid[2] = { INFINITY, 0.0 };
    unsigned char bytes[256];
    char path[512];
    Sink *sink;
    long n, saturated;
    int i;

    if (!format) fail("there is no output format \"raw\"");
    snprintf(path, sizeof path, "%s/sinks.raw", dir);
    sink = format->open(path, 44100);
    if (!sink) fail("cannot open %s", path);
    if (sink->write(sink, FRAMES, N_FRAMES) != 0) fail("raw: write failed");
    if (sink->write(sink, invalid, 1) == 0) fail("raw: a value that is not a number was accepted");
    saturated = sink->saturated;
    if (sink->close(sink) != 0) fail("raw: close failed");
    if (saturated != 0) fail("raw: floating-point frames never saturate");

    n = read_file(path, bytes, (long) sizeof bytes);
    if (n != 8 * N_FRAMES) fail("raw: the file is %ld bytes, expected %d", n, 8 * N_FRAMES);
    for (i = 0; i < 2 * N_FRAMES; i++) {
        uint32_t bits = (uint32_t) get_u32(bytes + 4 * i);
        float value;
        memcpy(&value, &bits, sizeof value);
        if (value != (float) FRAMES[i]) fail("raw: sample %d is %g, expected %g", i, value, FRAMES[i]);
    }
    /* 0.5 as a 32-bit float is 3f000000; the lowest byte comes first. */
    if (bytes[4] != 0x00 || bytes[5] != 0x00 || bytes[6] != 0x00 || bytes[7] != 0x3f)
        fail("raw: the bytes are not in lowest-first order");
    printf("raw: all %d samples as expected, 8 bytes per frame, no header\n", 2 * N_FRAMES);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "stdout") == 0) {
        const SinkFormat *format = sink_format_find("raw");
        Sink *sink = format ? format->open("-", 44100) : NULL;
        if (!sink || sink->write(sink, FRAMES, N_FRAMES) != 0 || sink->close(sink) != 0) return 1;
        return 0;
    }
    if (argc != 3 || strcmp(argv[1], "files") != 0)
        fail("usage: synth_sinks files <scratch directory> | synth_sinks stdout");

    if (sink_format_find("ogg") != NULL) fail("sink_format_find finds a format that does not exist");
    check_wav(argv[2]);
    check_raw(argv[2]);
    printf("ok: output formats\n");
    return 0;
}
