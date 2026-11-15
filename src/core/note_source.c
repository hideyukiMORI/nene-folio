#include "note_source.h"
#include "utf8_text.h"

uint32_t note_source_next(const char *_Nonnull bytes, size_t length, size_t *_Nonnull at)
{
    uint32_t point = 0;
    *at += utf8_text_decode(bytes + *at, length - *at, &point);
    if (point == '\r' && *at < length && bytes[*at] == '\n')
        ++*at;
    return point == '\n' ? '\r' : point;
}

size_t note_source_point_units(uint32_t point, enum note_source_projection projection)
{
    if (point == '\r')
    {
        switch (projection)
        {
        case NOTE_SOURCE_PARAGRAPH:
            return 1;
        case NOTE_SOURCE_CRLF:
            return 2;
        }
    }
    return point > 0xffff ? 2 : 1;
}

size_t note_source_units(const char *_Nonnull bytes, size_t length,
                         enum note_source_projection projection)
{
    size_t units = 0;
    for (size_t at = 0; at < length;)
        units += note_source_point_units(note_source_next(bytes, length, &at), projection);
    return units;
}
