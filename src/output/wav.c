/* wav.c -- output format "wav": a 16-bit stereo WAV file. The two lengths in
 * its header are filled in when the file is closed. */
#include "output/formats.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_BYTES 44
#define FRAME_BYTES  4                                         /* two 16-bit samples       */
#define MAX_FRAMES   ((0xffffffffUL - 36UL) / FRAME_BYTES)     /* the header's 32-bit limit */
#define BLOCK_FRAMES 1024

typedef struct {
    Sink          sink;
    FILE         *file;
    unsigned long frames;   /* written so far */
} WavSink;

/* WAV numbers are stored lowest byte first, on any computer. */
static void put_u16(unsigned char *bytes, unsigned value)
{
    bytes[0] = (unsigned char) (value & 0xff);
    bytes[1] = (unsigned char) ((value >> 8) & 0xff);
}

static void put_u32(unsigned char *bytes, unsigned long value)
{
    put_u16(bytes, (unsigned) (value & 0xffff));
    put_u16(bytes + 2, (unsigned) ((value >> 16) & 0xffff));
}

/* The header, with both lengths still zero. */
static int write_header(FILE *file, int sample_rate)
{
    unsigned char header[HEADER_BYTES] = { 0 };

    memcpy(header, "RIFF", 4);                                       /* 4: file length, later   */
    memcpy(header + 8, "WAVEfmt ", 8);
    put_u32(header + 16, 16);                                        /* length of the fmt part  */
    put_u16(header + 20, 1);                                         /* plain PCM               */
    put_u16(header + 22, 2);                                         /* channels                */
    put_u32(header + 24, (unsigned long) sample_rate);
    put_u32(header + 28, (unsigned long) sample_rate * FRAME_BYTES); /* bytes per second        */
    put_u16(header + 32, FRAME_BYTES);                               /* bytes per frame         */
    put_u16(header + 34, 16);                                        /* bits per sample         */
    memcpy(header + 36, "data", 4);                                  /* 40: audio length, later */

    return fwrite(header, 1, HEADER_BYTES, file) == HEADER_BYTES ? 0 : -1;
}

/* Goes back into the header and fills in the two lengths. */
static int write_lengths(FILE *file, unsigned long frames)
{
    unsigned long data_bytes = frames * FRAME_BYTES;
    unsigned char field[4];

    put_u32(field, 36 + data_bytes);            /* bytes after this field */
    if (fseek(file, 4L, SEEK_SET) != 0 || fwrite(field, 1, 4, file) != 4) return -1;
    put_u32(field, data_bytes);                 /* bytes of audio         */
    if (fseek(file, 40L, SEEK_SET) != 0 || fwrite(field, 1, 4, file) != 4) return -1;
    return 0;
}

/* round(x * 32768), held at the 16-bit limits; *saturated counts the samples
 * that had to be held. */
static int to_16_bit(double x, long *saturated)
{
    double scaled = round(x * 32768.0);
    if (scaled > 32767.0) {
        (*saturated)++;
        return 32767;
    }
    if (scaled < -32768.0) {
        (*saturated)++;
        return -32768;
    }
    return (int) scaled;
}

static int wav_write(Sink *sink, const double *frames, int n_frames)
{
    WavSink *wav = sink->state;
    unsigned char bytes[BLOCK_FRAMES * FRAME_BYTES];
    int done = 0, i;

    /* Nothing of a block that holds an invalid number is written. */
    for (i = 0; i < 2 * n_frames; i++)
        if (!isfinite(frames[i])) return -1;
    if ((unsigned long) n_frames > MAX_FRAMES - wav->frames) return -1;

    while (done < n_frames) {
        int n = n_frames - done < BLOCK_FRAMES ? n_frames - done : BLOCK_FRAMES;

        for (i = 0; i < 2 * n; i++) {
            int sample = to_16_bit(frames[2 * done + i], &sink->saturated);
            put_u16(bytes + 2 * i, (unsigned) sample & 0xffff);
        }
        if (fwrite(bytes, FRAME_BYTES, (size_t) n, wav->file) != (size_t) n) return -1;
        wav->frames += (unsigned long) n;
        done += n;
    }
    return 0;
}

static int wav_close(Sink *sink)
{
    WavSink *wav = sink->state;
    int status = 0;

    if (write_lengths(wav->file, wav->frames) != 0) status = -1;
    if (fclose(wav->file) != 0) status = -1;
    free(wav);
    return status;
}

/* A WAV file cannot go to standard output: its header is completed at the
 * end, and a pipe cannot be rewound. */
static Sink *wav_open(const char *path, int sample_rate)
{
    WavSink *wav;

    if (path[0] == '-' && path[1] == '\0') return NULL;
    if (sample_rate < 1) return NULL;

    wav = calloc(1, sizeof *wav);
    if (!wav) return NULL;
    wav->file = fopen(path, "wb");
    if (!wav->file) {
        free(wav);
        return NULL;
    }
    if (write_header(wav->file, sample_rate) != 0) {
        fclose(wav->file);
        free(wav);
        return NULL;
    }
    wav->sink.write = wav_write;
    wav->sink.close = wav_close;
    wav->sink.saturated = 0;
    wav->sink.state = wav;
    return &wav->sink;
}

const SinkFormat WAV_FORMAT = { "wav", wav_open };
