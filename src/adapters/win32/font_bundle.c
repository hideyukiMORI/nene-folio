#include "font_bundle.h"

#include "font_bundle_verdict.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* パスの上限（UTF-16 単位・終端込み）。persistence_adapter と同じ値。 */
constexpr size_t path_capacity = 1024;
/* 拡張子 `.otf` / `.ttf` の長さ（終端を除く）。 */
constexpr int extension_length = 4;

struct font_bundle
{
    /* 登録できたファイルの絶対パス（それぞれ終端付き）。destroy が同じパスで解除する */
    wchar_t *_Nonnull *_Nullable paths;
    size_t count;
    size_t capacity;
};

static const wchar_t fonts_folder[] = L"fonts\\";

/* <exe のディレクトリ>\fonts\ を out に書き、その長さを返す。data\ を決める persistence_adapter と
 * 同じ GetModuleFileNameW の流儀だが、GDI へ渡すので `\\?\` の接頭辞は付けない。
 * 後ろに葉を足す余地が無ければ false。 */
static bool fonts_directory(wchar_t *_Nonnull out, size_t *_Nonnull length)
{
    DWORD count = GetModuleFileNameW(nullptr, out, (DWORD)path_capacity);
    if (count == 0 || count >= path_capacity)
    {
        return false;
    }
    size_t directory = count;
    while (directory > 0 && out[directory - 1] != L'\\')
    {
        directory -= 1;
    }
    size_t folder = sizeof fonts_folder / sizeof fonts_folder[0] - 1;
    if (directory == 0 || directory + folder + 2 > path_capacity)
    {
        return false;
    }
    memcpy(out + directory, fonts_folder, sizeof fonts_folder);
    *length = directory + folder;
    return true;
}

/* 大小を無視した拡張子の一致。CompareStringOrdinal は既定ロケールを読まない（ARC-007）。 */
static bool has_extension(const wchar_t *_Nonnull name, size_t length,
                          const wchar_t *_Nonnull extension)
{
    return length > (size_t)extension_length &&
           CompareStringOrdinal(name + length - extension_length, extension_length, extension,
                                extension_length, TRUE) == CSTR_EQUAL;
}

static bool font_file(const WIN32_FIND_DATAW *_Nonnull entry)
{
    if ((entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return false;
    }
    size_t length = wcslen(entry->cFileName);
    return has_extension(entry->cFileName, length, L".otf") ||
           has_extension(entry->cFileName, length, L".ttf");
}

static bool reserve(struct font_bundle *_Nonnull bundle)
{
    if (bundle->count < bundle->capacity)
    {
        return true;
    }
    size_t capacity = bundle->capacity == 0 ? 8 : bundle->capacity * 2;
    wchar_t *_Nonnull *_Nullable grown = realloc(bundle->paths, capacity * sizeof *grown);
    if (grown == nullptr)
    {
        return false;
    }
    bundle->paths = grown;
    bundle->capacity = capacity;
    return true;
}

/* path の directory の後ろへ leaf を足して登録し、覚える。登録できなかった（名前が長すぎる・
 * GDI が断った）ファイルは覚えない（数の差が PARTIAL になる）。覚える入れ物を確保できなければ
 * 登録する前に false を返す（それ以外は true）。 */
static bool register_one(struct font_bundle *_Nonnull bundle, wchar_t *_Nonnull path,
                         size_t directory, const wchar_t *_Nonnull leaf)
{
    size_t leaf_length = wcslen(leaf);
    if (directory + leaf_length + 1 > path_capacity)
    {
        return true;
    }
    memcpy(path + directory, leaf, (leaf_length + 1) * sizeof *path);
    size_t length = directory + leaf_length + 1;
    wchar_t *_Nullable copy = malloc(length * sizeof *copy);
    if (copy == nullptr || !reserve(bundle))
    {
        free(copy);
        return false;
    }
    memcpy(copy, path, length * sizeof *copy);
    if (AddFontResourceExW(copy, FR_PRIVATE, nullptr) == 0)
    {
        free(copy);
        return true;
    }
    bundle->paths[bundle->count] = copy;
    bundle->count += 1;
    return true;
}

enum font_bundle_outcome font_bundle_register(struct font_bundle *_Nullable *_Nonnull out)
{
    *out = nullptr;
    struct font_bundle *_Nullable bundle = calloc(1, sizeof *bundle);
    if (bundle == nullptr)
    {
        return FONT_BUNDLE_NO_MEMORY;
    }
    wchar_t path[path_capacity];
    size_t directory = 0;
    HANDLE search = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW entry;
    if (fonts_directory(path, &directory))
    {
        path[directory] = L'*';
        path[directory + 1] = L'\0';
        search = FindFirstFileW(path, &entry);
    }
    size_t found = 0;
    bool starved = false;
    for (bool more = search != INVALID_HANDLE_VALUE; more && !starved;
         more = FindNextFileW(search, &entry) != 0)
    {
        if (font_file(&entry))
        {
            found += 1;
            starved = !register_one(bundle, path, directory, entry.cFileName);
        }
    }
    if (search != INVALID_HANDLE_VALUE)
    {
        FindClose(search);
    }
    if (starved)
    {
        font_bundle_destroy(bundle);
        return FONT_BUNDLE_NO_MEMORY;
    }
    *out = bundle;
    return font_bundle_verdict_of(found, bundle->count);
}

void font_bundle_destroy(struct font_bundle *_Nullable bundle)
{
    if (bundle == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < bundle->count; ++index)
    {
        RemoveFontResourceExW(bundle->paths[index], FR_PRIVATE, nullptr);
        free(bundle->paths[index]);
    }
    free(bundle->paths);
    free(bundle);
}
