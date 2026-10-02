/* formats.h -- the output formats that exist, one per file in this
 * directory. sink.c lists them by name.
 */
#ifndef CHIMES_FORMATS_H
#define CHIMES_FORMATS_H

#include "output/sink.h"

extern const SinkFormat WAV_FORMAT;   /* wav.c: 16-bit stereo WAV               */
extern const SinkFormat RAW_FORMAT;   /* raw.c: 32-bit float frames, no header  */

#endif
