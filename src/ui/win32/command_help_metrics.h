#ifndef NENEFOLIO_COMMAND_HELP_METRICS_H
#define NENEFOLIO_COMMAND_HELP_METRICS_H

#include "command_help_placement.h"
#include "folio_language.h"
#include "key_guide.h"
#include <stddef.h>
#include <windows.h>

/* 実件数は既存の説明表から導く。これは確保を増やさない固定bufferの容量。 */
constexpr size_t command_help_capacity = key_guide_capacity;

struct command_help_metrics
{
    bool valid;
    int width;
    UINT dpi;
    HFONT _Nullable font;
    enum folio_language language;
    size_t count;
    int maximum_line_height;
    int offsets[command_help_capacity + 1];
    struct command_help_placement rows[command_help_capacity];
};

#endif
