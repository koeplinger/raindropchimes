/* sink.c -- the list of output formats. To add one: write a file that
 * provides open / write / close, declare it in formats.h, and add one line
 * here. */
#include "output/sink.h"

#include <stddef.h>
#include <string.h>

#include "output/formats.h"

static const SinkFormat *const FORMATS[] = {
    &WAV_FORMAT,
    &RAW_FORMAT,
};

const SinkFormat *sink_format_find(const char *name)
{
    size_t i;
    for (i = 0; i < sizeof FORMATS / sizeof FORMATS[0]; i++)
        if (strcmp(FORMATS[i]->name, name) == 0) return FORMATS[i];
    return NULL;
}
