/* sink.h -- what an output format must provide: open, write a block of
 * stereo frames, close. The output only converts and writes; it sees frames
 * and nothing else.
 */
#ifndef CHIMES_SINK_H
#define CHIMES_SINK_H

typedef struct Sink Sink;

struct Sink {
    /* Writes n_frames interleaved left/right frames; 1.0 is full scale.
     * A value that is not a number is an error. Returns 0, or -1. */
    int  (*write)(Sink *sink, const double *frames, int n_frames);
    /* Finishes the file and frees the sink. Returns 0, or -1. */
    int  (*close)(Sink *sink);
    long  saturated;     /* samples held at the limit so far */
    void *state;
};

typedef struct {
    const char *name;
    /* Opens the output; path "-" means standard output where the format
     * allows it. NULL on failure. */
    Sink *(*open)(const char *path, int sample_rate);
} SinkFormat;

/* The list of formats, by name. NULL for an unknown name. */
const SinkFormat *sink_format_find(const char *name);

#endif
