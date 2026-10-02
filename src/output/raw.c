/* raw.c -- output format "raw": interleaved left/right 32-bit floating-point
 * frames with no header, lowest byte first (ffmpeg calls this f32le). The
 * path "-" means standard output, for piping into an encoder:
 *
 *   ... | ffmpeg -f f32le -ar 44100 -ac 2 -i - tune.ogg
 */
#include "output/formats.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_BYTES  8      /* two 32-bit samples */
#define BLOCK_FRAMES 1024

typedef struct {
    Sink  sink;
    FILE *file;
    int   is_stdout;   /* standard output is flushed, not closed */
} RawSink;

static int raw_write(Sink *sink, const double *frames, int n_frames)
{
    RawSink *raw = sink->state;
    unsigned char bytes[BLOCK_FRAMES * FRAME_BYTES];
    int done = 0, i;

    /* Nothing of a block that holds an invalid number is written. */
    for (i = 0; i < 2 * n_frames; i++)
        if (!isfinite(frames[i])) return -1;

    while (done < n_frames) {
        int n = n_frames - done < BLOCK_FRAMES ? n_frames - done : BLOCK_FRAMES;

        for (i = 0; i < 2 * n; i++) {
            float    sample = (float) frames[2 * done + i];
            uint32_t bits;

            /* Take the float's bit pattern and store it lowest byte first,
             * so the file is the same on any computer. */
            memcpy(&bits, &sample, sizeof bits);
            bytes[4 * i] = (unsigned char) (bits & 0xff);
            bytes[4 * i + 1] = (unsigned char) ((bits >> 8) & 0xff);
            bytes[4 * i + 2] = (unsigned char) ((bits >> 16) & 0xff);
            bytes[4 * i + 3] = (unsigned char) ((bits >> 24) & 0xff);
        }
        if (fwrite(bytes, FRAME_BYTES, (size_t) n, raw->file) != (size_t) n) return -1;
        done += n;
    }
    return 0;
}

static int raw_close(Sink *sink)
{
    RawSink *raw = sink->state;
    int status = 0;

    if (raw->is_stdout) {
        if (fflush(raw->file) != 0) status = -1;
    } else {
        if (fclose(raw->file) != 0) status = -1;
    }
    free(raw);
    return status;
}

/* Raw frames carry no sample rate; whoever reads them has to be told. */
static Sink *raw_open(const char *path, int sample_rate)
{
    RawSink *raw;

    (void) sample_rate;

    raw = calloc(1, sizeof *raw);
    if (!raw) return NULL;
    raw->is_stdout = (path[0] == '-' && path[1] == '\0');
    raw->file = raw->is_stdout ? stdout : fopen(path, "wb");
    if (!raw->file) {
        free(raw);
        return NULL;
    }
    raw->sink.write = raw_write;
    raw->sink.close = raw_close;
    raw->sink.saturated = 0;
    raw->sink.state = raw;
    return &raw->sink;
}

const SinkFormat RAW_FORMAT = { "raw", raw_open };
