#ifndef NENEFOLIO_NAME_PROMPT_METRICS_REQUEST_H
#define NENEFOLIO_NAME_PROMPT_METRICS_REQUEST_H

#include "folio_language.h"
#include <windows.h>

struct name_prompt_metrics_request
{
    HWND _Nonnull dialog;
    HFONT _Nonnull font;
    int width;
    UINT dpi;
    enum folio_language language;
};

#endif
