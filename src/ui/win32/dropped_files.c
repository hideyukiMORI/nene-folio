#include "dropped_files.h"
#include "storage_path.h"
#include "utf8_text.h"
#include <stdlib.h>
#include <uchar.h>
struct dropped_files
{
    struct storage_path *_Nonnull *_Nonnull paths;
    size_t kept;  /* paths に入っている数 */
    size_t count; /* 落とされた数から lost を引いたもの */
    size_t lost;
};

/* storage_path が受ける長さ（UTF-8 で storage_path_byte_limit バイト）は、UTF-16 でもこの単位数を
 * 超えない。これより長いパスは、パスにできないものとして数える。 */
constexpr UINT path_unit_capacity = (UINT)storage_path_byte_limit + 1;

static bool path_at(HDROP _Nonnull drop, UINT index, struct storage_path *_Nullable *_Nonnull out)
{
    wchar_t path[path_unit_capacity];
    UINT needed = DragQueryFileW(drop, index, nullptr, 0);
    if (needed == 0 || needed >= path_unit_capacity)
        return false;
    UINT length = DragQueryFileW(drop, index, path, path_unit_capacity);
    struct utf8_text *_Nullable text = nullptr;
    if (length != needed ||
        utf8_text_create((const char16_t *)path, length, &text) != UTF8_TEXT_CONVERTED ||
        text == nullptr)
        return false;
    enum storage_path_outcome accepted =
        storage_path_create(utf8_text_bytes(text), utf8_text_length(text), out);
    utf8_text_destroy(text);
    return accepted == STORAGE_PATH_READY;
}

bool dropped_files_create(HDROP _Nonnull drop, size_t limit,
                          struct dropped_files *_Nullable *_Nonnull out)
{
    struct dropped_files *_Nullable files = calloc(1, sizeof *files);
    struct storage_path **_Nullable paths = calloc(limit == 0 ? 1 : limit, sizeof *paths);
    if (files == nullptr || paths == nullptr)
    {
        free(files);
        free(paths);
        return false;
    }
    files->paths = paths;
    UINT dropped = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    /* limit 個のパスがそろうまで読む: 途中にパスにできないものがあっても、渡す並びに穴を作らない
     * （application は、数と limit の小さいほうの個数を先頭から読む）。 */
    for (UINT index = 0; index < dropped && files->kept < limit; ++index)
    {
        struct storage_path *_Nullable path = nullptr;
        if (path_at(drop, index, &path) && path != nullptr)
            files->paths[files->kept++] = path;
        else
            ++files->lost;
    }
    files->count = dropped - files->lost;
    *out = files;
    return true;
}

size_t dropped_files_count(const struct dropped_files *_Nonnull files)
{
    return files->count;
}

size_t dropped_files_lost(const struct dropped_files *_Nonnull files)
{
    return files->lost;
}

struct storage_path *_Nonnull const *_Nonnull dropped_files_paths(
    const struct dropped_files *_Nonnull files)
{
    return (struct storage_path *_Nonnull const *)files->paths;
}

void dropped_files_destroy(struct dropped_files *_Nullable files)
{
    if (files == nullptr)
        return;
    for (size_t at = 0; at < files->kept; ++at)
        storage_path_destroy(files->paths[at]);
    free(files->paths);
    free(files);
}
