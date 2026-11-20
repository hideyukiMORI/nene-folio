#include "image_link.h"
#include "text_buffer.h"
#include <string.h>

/* 最後の区切り `/` の次の位置（名前の始まり）。区切りが無ければ 0。 */
static size_t name_start(const char *_Nonnull leaf, size_t length)
{
    for (size_t at = length; at > 0; --at)
    {
        if (leaf[at - 1] == '/')
            return at;
    }
    return 0;
}

/* 最後の `.` の位置（拡張子の始まり）。名前の先頭の `.` と、`.` が無い名前では length。 */
static size_t extension_start(const char *_Nonnull leaf, size_t length)
{
    size_t name = name_start(leaf, length);
    for (size_t at = length; at > name + 1; --at)
    {
        if (leaf[at - 1] == '.')
            return at - 1;
    }
    return length;
}

/* 代替の文: 記法を閉じてしまう文字を除いて写す。 */
static void append_alternative(struct text_buffer *_Nonnull out, const char *_Nonnull stem,
                               size_t length)
{
    for (size_t at = 0; at < length; ++at)
    {
        if (stem[at] != '[' && stem[at] != ']' && stem[at] != '\\')
            text_buffer_append(out, stem + at, 1);
    }
}

static bool needs_brackets(const char *_Nonnull leaf, size_t length)
{
    for (size_t at = 0; at < length; ++at)
    {
        if (leaf[at] == ' ' || leaf[at] == '(' || leaf[at] == ')')
            return true;
    }
    return false;
}

/* パス: `%` だけを `%25` に替えて写す。 */
static void append_path(struct text_buffer *_Nonnull out, const char *_Nonnull leaf, size_t length)
{
    text_buffer_append_text(out, image_link_folder);
    text_buffer_append_text(out, "/");
    for (size_t at = 0; at < length; ++at)
    {
        if (leaf[at] == '%')
            text_buffer_append_text(out, "%25");
        else
            text_buffer_append(out, leaf + at, 1);
    }
}

void image_link_append(struct text_buffer *_Nonnull out, const char *_Nonnull leaf, size_t length)
{
    bool bracketed = needs_brackets(leaf, length);
    text_buffer_append_text(out, "![");
    size_t name = name_start(leaf, length);
    append_alternative(out, leaf + name, extension_start(leaf, length) - name);
    text_buffer_append_text(out, bracketed ? "](<" : "](");
    append_path(out, leaf, length);
    text_buffer_append_text(out, bracketed ? ">)" : ")");
}

/* 10 進の数を足す。 */
static void append_number(struct text_buffer *_Nonnull out, size_t number)
{
    char digits[24];
    size_t used = 0;
    do
    {
        digits[used++] = (char)('0' + number % 10);
        number /= 10;
    } while (number != 0);
    while (used > 0)
        text_buffer_append(out, &digits[--used], 1);
}

void image_link_numbered(struct text_buffer *_Nonnull out, const char *_Nonnull leaf, size_t length,
                         size_t number)
{
    size_t extension = extension_start(leaf, length);
    text_buffer_append(out, leaf, extension);
    text_buffer_append_text(out, "-");
    append_number(out, number);
    text_buffer_append(out, leaf + extension, length - extension);
}
