#include "storage_native_path.h"
#include "storage_path.h"
#include "utf16_text.h"
#include "utf8_text.h"
#include <string.h>
#include <wchar.h>

bool storage_native_path_fill(const struct storage_path *_Nonnull path, wchar_t *_Nonnull out,
                              size_t *_Nonnull length)
{
    wchar_t units[storage_native_capacity];
    size_t count = 0;
    if (utf16_text_fill(storage_path_text(path), units, storage_native_capacity, &count) !=
        UTF16_TEXT_FILL_READY)
        return false;
    bool unc = count >= 2 && units[0] == L'\\' && units[1] == L'\\';
    const wchar_t *_Nonnull prefix = unc ? L"\\\\?\\UNC\\" : L"\\\\?\\";
    size_t prefix_length = unc ? 8 : 4;
    size_t offset = unc ? 2 : 0;
    size_t size = prefix_length + count - offset;
    if (size >= storage_native_capacity)
        return false;
    memcpy(out, prefix, prefix_length * sizeof *out);
    memcpy(out + prefix_length, units + offset, (count - offset + 1) * sizeof *out);
    *length = size;
    return true;
}

static bool regular_units(const wchar_t *_Nonnull units, wchar_t *_Nonnull out)
{
    size_t length = wcslen(units);
    size_t skip = 0;
    size_t prefix = 0;
    if (length >= 8 && wmemcmp(units, L"\\\\?\\UNC\\", 8) == 0)
    {
        skip = 8;
        prefix = 2;
    }
    else if (length >= 4 && wmemcmp(units, L"\\\\?\\", 4) == 0)
        skip = 4;
    if (length - skip + prefix >= storage_native_capacity)
        return false;
    if (prefix != 0)
    {
        out[0] = L'\\';
        out[1] = L'\\';
    }
    memcpy(out + prefix, units + skip, (length - skip + 1) * sizeof *out);
    return true;
}

enum storage_outcome storage_native_path_from_units(const wchar_t *_Nonnull units,
                                                    struct storage_path *_Nullable *_Nonnull out)
{
    wchar_t regular[storage_native_capacity];
    if (!regular_units(units, regular))
        return STORAGE_INVALID_PATH;
    struct utf8_text *_Nullable text = nullptr;
    enum utf8_text_outcome converted = utf8_text_create(regular, wcslen(regular), &text);
    if (converted != UTF8_TEXT_CONVERTED)
        return converted == UTF8_TEXT_OUT_OF_MEMORY ? STORAGE_OUT_OF_MEMORY : STORAGE_INVALID_PATH;
    enum storage_path_outcome result =
        storage_path_create(utf8_text_bytes(text), utf8_text_length(text), out);
    utf8_text_destroy(text);
    switch (result)
    {
    case STORAGE_PATH_READY:
        return STORAGE_READY;
    case STORAGE_PATH_INVALID:
        return STORAGE_INVALID_PATH;
    case STORAGE_PATH_OUT_OF_MEMORY:
        return STORAGE_OUT_OF_MEMORY;
    }
    return STORAGE_INVALID_PATH;
}
