#include "clipboard_adapter.h"
#include "utf16_text.h"
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

/* ほかのプログラムが短く開いているだけのことが多いので、少しだけ待って開き直す。 */
constexpr int open_attempts = 5;
constexpr DWORD open_wait_milliseconds = 10;

struct clipboard_adapter
{
    HWND _Nullable owner; /* クリップボードの所有者になる表示しない窓。最初の書き込みで作る。 */
};

enum clipboard_adapter_outcome
clipboard_adapter_create(struct clipboard_adapter *_Nullable *_Nonnull out)
{
    struct clipboard_adapter *_Nullable adapter = calloc(1, sizeof *adapter);
    if (adapter == nullptr)
        return CLIPBOARD_ADAPTER_OUT_OF_MEMORY;
    *out = adapter;
    return CLIPBOARD_ADAPTER_CREATED;
}

void clipboard_adapter_destroy(struct clipboard_adapter *_Nullable adapter)
{
    if (adapter != nullptr && adapter->owner != nullptr)
        DestroyWindow(adapter->owner);
    free(adapter);
}

/* 所有者が NULL のまま EmptyClipboard すると SetClipboardData が失敗し得るので、窓を持つ。 */
static bool owner_ready(struct clipboard_adapter *_Nonnull adapter)
{
    if (adapter->owner == nullptr)
        adapter->owner = CreateWindowExW(0, L"STATIC", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE,
                                         nullptr, GetModuleHandleW(nullptr), nullptr);
    return adapter->owner != nullptr;
}

/* 行の区切りを CRLF にしたときの単位数（終端を除く）。既に CRLF の箇所は増やさない。 */
static size_t native_length(const char16_t *_Nonnull units, size_t length)
{
    size_t count = length;
    for (size_t index = 0; index < length; ++index)
        if (units[index] == u'\n' && (index == 0 || units[index - 1] != u'\r'))
            ++count;
    return count;
}

static void fill_native(wchar_t *_Nonnull out, const char16_t *_Nonnull units, size_t length)
{
    size_t written = 0;
    for (size_t index = 0; index < length; ++index)
    {
        if (units[index] == u'\n' && (index == 0 || units[index - 1] != u'\r'))
            out[written++] = L'\r';
        out[written++] = (wchar_t)units[index];
    }
    out[written] = L'\0';
}

/* 終端付きの UTF-16・CRLF を、クリップボードへ渡せる移動可能な領域に作る。
 * 成功したら領域の所有は呼び出し側にある。 */
static enum clipboard_outcome native_text(const struct utf16_text *_Nonnull text,
                                          HGLOBAL _Nullable *_Nonnull out)
{
    const char16_t *units = utf16_text_units(text);
    size_t length = utf16_text_length(text);
    size_t count = native_length(units, length);
    if (count >= SIZE_MAX / sizeof(wchar_t))
        return CLIPBOARD_OUT_OF_MEMORY;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (count + 1) * sizeof(wchar_t));
    if (memory == nullptr)
        return CLIPBOARD_OUT_OF_MEMORY;
    wchar_t *_Nullable locked = GlobalLock(memory);
    if (locked == nullptr)
    {
        GlobalFree(memory);
        return CLIPBOARD_OUT_OF_MEMORY;
    }
    fill_native(locked, units, length);
    GlobalUnlock(memory);
    *out = memory;
    return CLIPBOARD_WRITTEN;
}

static enum clipboard_outcome open_clipboard(HWND _Nonnull owner)
{
    for (int attempt = 0; attempt < open_attempts; ++attempt)
    {
        if (OpenClipboard(owner))
            return CLIPBOARD_WRITTEN;
        if (GetLastError() != ERROR_ACCESS_DENIED)
            return CLIPBOARD_UNAVAILABLE;
        if (attempt + 1 < open_attempts)
            Sleep(open_wait_milliseconds);
    }
    return CLIPBOARD_BUSY;
}

/* 成功したら memory の所有は OS へ移る。失敗したら呼び出し側に残る。 */
static enum clipboard_outcome place(struct clipboard_adapter *_Nonnull adapter,
                                    HGLOBAL _Nonnull memory)
{
    if (!owner_ready(adapter))
        return CLIPBOARD_UNAVAILABLE;
    enum clipboard_outcome opened = open_clipboard(adapter->owner);
    if (opened != CLIPBOARD_WRITTEN)
        return opened;
    bool placed = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
    CloseClipboard();
    return placed ? CLIPBOARD_WRITTEN : CLIPBOARD_UNAVAILABLE;
}

static enum clipboard_outcome write_native(struct clipboard_adapter *_Nonnull adapter,
                                           const struct utf16_text *_Nonnull text)
{
    HGLOBAL _Nullable memory = nullptr;
    enum clipboard_outcome outcome = native_text(text, &memory);
    if (outcome != CLIPBOARD_WRITTEN || memory == nullptr)
        return outcome;
    outcome = place(adapter, memory);
    if (outcome != CLIPBOARD_WRITTEN)
        GlobalFree(memory);
    return outcome;
}

static enum clipboard_outcome write_text(struct clipboard_adapter *_Nonnull adapter,
                                         const char *_Nonnull bytes, size_t length)
{
    struct utf16_text *_Nullable text = nullptr;
    switch (utf16_text_create(bytes, length, &text))
    {
    case UTF16_TEXT_CONVERTED:
        break;
    case UTF16_TEXT_INVALID_UTF8:
        return CLIPBOARD_UNAVAILABLE;
    case UTF16_TEXT_OUT_OF_MEMORY:
        return CLIPBOARD_OUT_OF_MEMORY;
    }
    enum clipboard_outcome outcome = write_native(adapter, text);
    utf16_text_destroy(text);
    return outcome;
}

struct clipboard_port clipboard_adapter_port(struct clipboard_adapter *_Nonnull adapter)
{
    return (struct clipboard_port){.adapter = adapter, .write = write_text};
}
