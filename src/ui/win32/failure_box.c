#include "failure_box.h"

#include "folio_state.h"
#include "utf16_text.h"

#include <string.h>

void failure_box_show(HWND _Nullable owner, enum folio_state_outcome outcome)
{
    if (outcome == FOLIO_STATE_CANCELLED)
    {
        return;
    }
    const char *_Nonnull line = folio_state_failure_line(outcome);
    struct utf16_text *_Nullable text = nullptr;
    if (utf16_text_create(line, strlen(line), &text) != UTF16_TEXT_CONVERTED)
    {
        return;
    }
    MessageBoxW(owner, utf16_text_units(text), L"NeNe Folio", MB_OK | MB_ICONWARNING);
    utf16_text_destroy(text);
}
