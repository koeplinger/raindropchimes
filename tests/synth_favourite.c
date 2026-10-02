/* synth_favourite.c -- render sanity for the favourite tune (plan section 7,
 * item 5): renders a score with the v4 preset into a WAV file, then reads
 * that file back and checks it.
 *
 *   synth_favourite favourite.score out.wav
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "synth/render.h"
#include "synth_common.h"

#define RATE          44100
#define LONGEST_DELAY 19000      /* frames: the longest v4 reverb tap           */
#define OLD_CUT       200.34     /* seconds: where the 2011 file fell silent    */

static unsigned long get_u16(const unsigned char *bytes)
{
    return (unsigned long) bytes[0] | ((unsigned long) bytes[1] << 8);
}

static unsigned long get_u32(const unsigned char *bytes)
{
    return get_u16(bytes) | (get_u16(bytes + 2) << 16);
}

static int sample_at(const unsigned char *audio, long frame, int channel)
{
    int value = (int) get_u16(audio + 4 * frame + 2 * channel);
    return value >= 32768 ? value - 65536 : value;
}

int main(int argc, char **argv)
{
    Score score;
    Timing timing = timing_from_bins(1700.0, 0.0);
    const Patch *patch = patch_find("v4");
    const SinkFormat *format = sink_format_find("wav");
    Sink *sink;
    RenderStats stats;
    char err[256];
    long saturated, file_bytes, frames, frame, loud_after_cut = 0;
    unsigned char *bytes, *audio;
    FILE *file;

    if (argc != 3) fail("usage: synth_favourite favourite.score out.wav");
    if (!patch) fail("there is no patch \"v4\"");
    if (!format) fail("there is no output format \"wav\"");
    load_score(argv[1], &score);

    /* Render. */
    sink = format->open(argv[2], RATE);
    if (!sink) fail("cannot open %s for writing", argv[2]);
    if (render_score(&score, &timing, patch, RATE, 1.0, sink, &stats, err, sizeof err) != 0)
        fail("render: %s", err);
    saturated = sink->saturated;
    if (sink->close(sink) != 0) fail("closing %s failed", argv[2]);

    printf("frames        %ld\n", (long) stats.frames);
    printf("duration      %.3f s   (expected about 208 s)\n", (double) stats.frames / RATE);
    printf("music ends at %.3f s   (expected about 199.4 s)\n", (double) stats.music_frames / RATE);
    printf("ring-out      %.3f s\n", (double) (stats.frames - stats.music_frames) / RATE);
    printf("peak          %.5f     (expected about 0.5085)\n", stats.peak);
    printf("saturated     %ld\n", saturated);

    /* Every sample was a valid number, or render_score would have failed. */
    if (saturated != 0) fail("%ld samples saturated", saturated);
    if (fabs(stats.peak - 0.5085) > 0.002) fail("the peak is not about 0.5085");
    if (fabs((double) stats.music_frames / RATE - 199.4) > 0.1) fail("the music does not end at about 199.4 s");
    if (fabs((double) stats.frames / RATE - 208.0) > 1.5) fail("the piece is not about 208 s long");

    /* Read the file back. */
    file = fopen(argv[2], "rb");
    if (!file) fail("cannot open %s for reading", argv[2]);
    fseek(file, 0L, SEEK_END);
    file_bytes = ftell(file);
    fseek(file, 0L, SEEK_SET);
    bytes = malloc((size_t) file_bytes);
    if (!bytes || fread(bytes, 1, (size_t) file_bytes, file) != (size_t) file_bytes)
        fail("cannot read %s", argv[2]);
    fclose(file);

    /* The header. */
    if (file_bytes != 44 + 4 * (long) stats.frames) fail("file size %ld is not 44 + 4 * frames", file_bytes);
    if (memcmp(bytes, "RIFF", 4) != 0) fail("header: no RIFF");
    if (get_u32(bytes + 4) != (unsigned long) file_bytes - 8) fail("header: wrong RIFF length");
    if (memcmp(bytes + 8, "WAVEfmt ", 8) != 0) fail("header: no WAVEfmt");
    if (get_u32(bytes + 16) != 16) fail("header: fmt length is not 16");
    if (get_u16(bytes + 20) != 1) fail("header: not PCM");
    if (get_u16(bytes + 22) != 2) fail("header: not stereo");
    if (get_u32(bytes + 24) != RATE) fail("header: wrong sample rate");
    if (get_u32(bytes + 28) != RATE * 4) fail("header: wrong byte rate");
    if (get_u16(bytes + 32) != 4) fail("header: wrong frame size");
    if (get_u16(bytes + 34) != 16) fail("header: not 16 bit");
    if (memcmp(bytes + 36, "data", 4) != 0) fail("header: no data part");
    if (get_u32(bytes + 40) != (unsigned long) file_bytes - 44) fail("header: wrong data length");
    printf("header        correct (%ld bytes of audio)\n", file_bytes - 44);

    audio = bytes + 44;
    frames = (long) stats.frames;

    /* There is reverb tail after the point where the 2011 file fell silent. */
    for (frame = (long) (OLD_CUT * RATE); frame < frames; frame++)
        if (sample_at(audio, frame, 0) != 0 || sample_at(audio, frame, 1) != 0) loud_after_cut++;
    printf("tail          %ld frames after %.2f s are not silent\n", loud_after_cut, OLD_CUT);
    if (loud_after_cut < RATE) fail("there is no reverb tail after %.2f s", OLD_CUT);

    /* The final stretch, one longest reverb delay, is digital silence ... */
    for (frame = frames - LONGEST_DELAY; frame < frames; frame++)
        if (sample_at(audio, frame, 0) != 0 || sample_at(audio, frame, 1) != 0)
            fail("frame %ld of %ld is not silent", frame, frames);
    /* ... and the piece ends as soon as that is so: the quiet stretch is one
     * frame longer than the longest delay, and the frame before it is not
     * quiet. */
    frame = frames - LONGEST_DELAY - 2;
    if (sample_at(audio, frame, 0) == 0 && sample_at(audio, frame, 1) == 0)
        fail("the piece runs on after the reverb has died away");
    printf("ending        the last %d frames are digital silence\n", LONGEST_DELAY);

    free(bytes);
    score_free(&score);
    printf("ok: render sanity for the favourite tune\n");
    return 0;
}
