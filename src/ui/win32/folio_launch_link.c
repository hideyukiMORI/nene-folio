#include "folio_launch_link.h"
#include "folio_open_link.h"
#include "storage_path.h"
#include "utf8_text.h"
#include <stdlib.h>
#include <string.h>
#include <uchar.h>

static bool blank(wchar_t unit)
{
    return unit == L' ' || unit == L'\t';
}

/* `nenefolio:` で始まるか（ASCII の大小は問わない）。 */
static bool link_scheme(const wchar_t *_Nonnull text, size_t length)
{
    static const wchar_t scheme[] = L"nenefolio:";
    size_t count = sizeof scheme / sizeof scheme[0] - 1;
    if (length < count)
        return false;
    for (size_t index = 0; index < count; ++index)
    {
        wchar_t unit = text[index];
        if (unit >= L'A' && unit <= L'Z')
            unit = (wchar_t)(unit - L'A' + L'a');
        if (unit != scheme[index])
            return false;
    }
    return true;
}

struct utf8_text *_Nullable folio_launch_link_read(const wchar_t *_Nonnull arguments)
{
    const wchar_t *start = arguments;
    while (blank(*start))
        start += 1;
    size_t length = wcslen(start);
    while (length > 0 && blank(start[length - 1]))
        length -= 1;
    if (length >= 2 && start[0] == L'"' && start[length - 1] == L'"')
    {
        start += 1;
        length -= 2;
    }
    if (!link_scheme(start, length))
        return nullptr;
    struct utf8_text *_Nullable link = nullptr;
    if (utf8_text_create((const char16_t *)start, length, &link) != UTF8_TEXT_CONVERTED)
        return nullptr;
    return link;
}

/* 1 つの主窓へ渡す。受けたら true。 */
static bool hand_to(HWND _Nonnull window, const COPYDATASTRUCT *_Nonnull copy)
{
    DWORD process = 0;
    if (GetWindowThreadProcessId(window, &process) != 0 && process != 0)
        (void)AllowSetForegroundWindow(process); /* the taker comes to the front itself */
    DWORD_PTR taken = FALSE;
    LRESULT sent = SendMessageTimeoutW(window, WM_COPYDATA, 0, (LPARAM)copy,
                                       SMTO_ABORTIFHUNG | SMTO_BLOCK, 5000, &taken);
    return sent != 0 && taken == TRUE;
}

bool folio_launch_link_hand_over(const struct storage_path *_Nonnull place,
                                 const char *_Nonnull link, size_t length)
{
    size_t root = storage_path_length(place);
    size_t total = root + 1 + length;
    if (total > folio_open_link_limit)
        return false;
    char *data = malloc(total);
    if (data == nullptr)
        return false;
    memcpy(data, storage_path_text(place), root);
    data[root] = '\0';
    memcpy(data + root + 1, link, length);
    COPYDATASTRUCT copy = {.dwData = folio_open_link_data, .cbData = (DWORD)total, .lpData = data};
    bool taken = false;
    HWND window = nullptr;
    while (!taken &&
           (window = FindWindowExW(nullptr, window, folio_open_link_class, nullptr)) != nullptr)
        taken = hand_to(window, &copy);
    free(data);
    return taken;
}
