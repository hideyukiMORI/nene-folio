#include "web_uri.h"
#include "ascii_fold.h"
#include "utf8_text.h"
#include <stdlib.h>
#include <string.h>

struct web_uri
{
    char *_Nonnull bytes;
    size_t length;
};

static size_t authority_start(const char *_Nonnull bytes, size_t length)
{
    if (length < 8)
        return 0;
    const char scheme[] = "http";
    for (size_t index = 0; index < sizeof scheme - 1; ++index)
    {
        if (ascii_fold_byte(bytes[index]) != scheme[index])
            return 0;
    }
    size_t end = ascii_fold_byte(bytes[4]) == 's' ? 5 : 4;
    return bytes[end] == ':' && bytes[end + 1] == '/' && bytes[end + 2] == '/' ? end + 3 : 0;
}

static bool forbidden_space(uint32_t point)
{
    if (point <= 32 || (point >= 127 && point <= 160))
        return true;
    if (point >= 0x2000 && point <= 0x200A)
        return true;
    return point == 0x1680 || point == 0x2028 || point == 0x2029 || point == 0x202F ||
           point == 0x205F || point == 0x3000;
}

static bool valid_characters(const char *_Nonnull bytes, size_t length)
{
    size_t at = 0;
    while (at < length)
    {
        uint32_t point = 0;
        size_t used = utf8_text_decode(bytes + at, length - at, &point);
        if (used == 0 || forbidden_space(point) || point == '\\' || point == '"' || point == '<' ||
            point == '>')
            return false;
        at += used;
    }
    return true;
}

static bool valid_authority(const char *_Nonnull bytes, size_t length, size_t start)
{
    size_t end = start;
    while (end < length && bytes[end] != '/' && bytes[end] != '?' && bytes[end] != '#')
    {
        if (bytes[end] == '@')
            return false;
        end += 1;
    }
    return end > start;
}

static void destroy_prepared(struct web_uri *_Nullable uri, char *_Nullable bytes)
{
    free(bytes);
    free(uri);
}

enum web_uri_outcome web_uri_create(const char *_Nonnull bytes, size_t length,
                                    struct web_uri *_Nullable *_Nonnull out)
{
    if (length > web_uri_byte_limit)
        return WEB_URI_REJECTED;
    size_t start = authority_start(bytes, length);
    if (start == 0 || !valid_authority(bytes, length, start) || !valid_characters(bytes, length))
        return WEB_URI_REJECTED;
    struct web_uri *_Nullable uri = malloc(sizeof *uri);
    char *_Nullable owned = malloc(length + 1);
    if (uri == nullptr || owned == nullptr)
    {
        destroy_prepared(uri, owned);
        return WEB_URI_OUT_OF_MEMORY;
    }
    memcpy(owned, bytes, length);
    owned[length] = '\0';
    *uri = (struct web_uri){.bytes = owned, .length = length};
    *out = uri;
    return WEB_URI_ACCEPTED;
}

const char *_Nonnull web_uri_bytes(const struct web_uri *_Nonnull uri)
{
    return uri->bytes;
}

size_t web_uri_length(const struct web_uri *_Nonnull uri)
{
    return uri->length;
}

void web_uri_destroy(struct web_uri *_Nullable uri)
{
    if (uri != nullptr)
        free(uri->bytes);
    free(uri);
}
