#include "image_source.h"
#include "ascii_fold.h"
#include "utf8_text.h"
#include "web_uri.h"
#include <stdlib.h>
#include <string.h>

struct image_source
{
    char *_Nonnull target;
    struct note_search_span span;
    enum image_source_kind kind;
    bool recommended;
};

static bool prefix(const char *_Nonnull text, const char *_Nonnull expected)
{
    for (size_t index = 0; expected[index] != '\0'; ++index)
        if (ascii_fold_byte(text[index]) != expected[index])
            return false;
    return true;
}

static size_t colon_offset(const char *_Nonnull text)
{
    for (size_t index = 0; text[index] != '\0'; ++index)
        if (text[index] == ':')
            return index;
    return SIZE_MAX;
}
static enum image_source_kind classify(const char *_Nonnull text)
{
    if (prefix(text, "https://"))
        return IMAGE_SOURCE_HTTPS;
    size_t colon = colon_offset(text);
    if (colon != SIZE_MAX &&
        (colon != 1 ||
         !((text[0] >= 'A' && text[0] <= 'Z') || (text[0] >= 'a' && text[0] <= 'z')) ||
         (text[2] != '/' && text[2] != '\\')))
        return IMAGE_SOURCE_UNSUPPORTED;
    if (text[0] == '/' || text[0] == '\\')
        return IMAGE_SOURCE_UNSUPPORTED;
    return IMAGE_SOURCE_LOCAL;
}

static bool valid_local(const char *_Nonnull text, size_t length)
{
    size_t cursor = 0;
    while (cursor < length)
    {
        uint32_t point = 0;
        size_t used = utf8_text_decode(text + cursor, length - cursor, &point);
        if (used == 0 || point < 32 || point == 127 || point == '"' || point == '<' ||
            point == '>' || point == '|' || point == '*' || point == '?' ||
            (point == ':' && cursor != 1))
            return false;
        cursor += used;
    }
    return true;
}

static bool has_parent(const char *_Nonnull text)
{
    for (size_t index = 0; text[index] != '\0'; ++index)
        if (strncmp(text + index, "/..", 3) == 0 &&
            (text[index + 3] == '/' || text[index + 3] == '\0'))
            return true;
    return false;
}
static bool recommended_path(const char *_Nonnull text)
{
    while (text[0] == '.' && text[1] == '/')
        text += 2;
    if (!prefix(text, "images/") || text[7] == '\0')
        return false;
    return !has_parent(text);
}

static enum image_outcome validate_https(const char *_Nonnull text, size_t length)
{
    struct web_uri *_Nullable uri = nullptr;
    enum web_uri_outcome valid = web_uri_create(text, length, &uri);
    web_uri_destroy(uri);
    switch (valid)
    {
    case WEB_URI_ACCEPTED:
        return IMAGE_READY;
    case WEB_URI_REJECTED:
        return IMAGE_UNSUPPORTED;
    case WEB_URI_OUT_OF_MEMORY:
        return IMAGE_OUT_OF_MEMORY;
    }
    return IMAGE_UNSUPPORTED;
}

static void destroy_prepared(struct image_source *_Nullable source, char *_Nullable text)
{
    free(source);
    free(text);
}
static int hex_digit(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    value = ascii_fold_byte(value);
    return value >= 'a' && value <= 'f' ? value - 'a' + 10 : -1;
}
static size_t decode_percent(char *_Nonnull target, size_t index, unsigned char *_Nonnull value)
{
    if (target[index] != '%' || target[index + 1] == '\0' || target[index + 2] == '\0')
        return 1;
    int high = hex_digit(target[index + 1]);
    int low = hex_digit(target[index + 2]);
    if (high < 0 || low < 0)
        return 1;
    *value = (unsigned char)(high * 16 + low);
    return 3;
}
static bool normalize_local(char *_Nonnull target)
{
    size_t written = 0;
    for (size_t index = 0; target[index] != '\0';)
    {
        unsigned char value = (unsigned char)target[index];
        index += decode_percent(target, index, &value);
        if (value == 0)
            return false;
        target[written++] = value == '\\' ? '/' : (char)value;
    }
    target[written] = '\0';
    return classify(target) == IMAGE_SOURCE_LOCAL && valid_local(target, written);
}
static enum image_outcome validate(const char *_Nonnull destination, size_t length,
                                   enum image_source_kind *_Nonnull kind)
{
    *kind = classify(destination);
    if (*kind == IMAGE_SOURCE_HTTPS)
        return validate_https(destination, length);
    if (*kind == IMAGE_SOURCE_LOCAL && !valid_local(destination, length))
        *kind = IMAGE_SOURCE_UNSUPPORTED;
    return IMAGE_READY;
}

static enum image_outcome copy_owned(const char *_Nonnull destination,
                                     struct note_search_span source, enum image_source_kind kind,
                                     struct image_source *_Nullable *_Nonnull out)
{
    size_t length = strlen(destination);
    struct image_source *_Nullable image = malloc(sizeof *image);
    char *_Nullable target = malloc(length + 1);
    if (image == nullptr || target == nullptr)
    {
        destroy_prepared(image, target);
        return IMAGE_OUT_OF_MEMORY;
    }
    memcpy(target, destination, length + 1);
    *image = (struct image_source){.target = target,
                                   .span = source,
                                   .kind = kind,
                                   .recommended =
                                       kind == IMAGE_SOURCE_LOCAL && recommended_path(target)};
    *out = image;
    return IMAGE_READY;
}
enum image_outcome image_source_create(const char *_Nonnull destination,
                                       struct note_search_span span,
                                       struct image_source *_Nullable *_Nonnull out)
{
    size_t length = strlen(destination);
    if (length == 0 || length > image_source_byte_limit || span.start > span.end)
        return IMAGE_UNSUPPORTED;
    enum image_source_kind kind;
    enum image_outcome valid = validate(destination, length, &kind);
    if (valid != IMAGE_READY)
        return valid;
    struct image_source *_Nullable source = nullptr;
    enum image_outcome copied = copy_owned(destination, span, kind, &source);
    if (copied != IMAGE_READY)
        return copied;
    if (kind == IMAGE_SOURCE_LOCAL && !normalize_local(source->target))
        source->kind = IMAGE_SOURCE_UNSUPPORTED;
    source->recommended = source->kind == IMAGE_SOURCE_LOCAL && recommended_path(source->target);
    *out = source;
    return IMAGE_READY;
}
enum image_outcome image_source_copy(const struct image_source *_Nonnull source,
                                     struct image_source *_Nullable *_Nonnull out)
{
    return copy_owned(source->target, source->span, source->kind, out);
}

const char *_Nonnull image_source_target(const struct image_source *_Nonnull source)
{
    return source->target;
}
enum image_source_kind image_source_kind(const struct image_source *_Nonnull source)
{
    return source->kind;
}
bool image_source_recommended(const struct image_source *_Nonnull source)
{
    return source->recommended;
}
struct note_search_span image_source_span(const struct image_source *_Nonnull source)
{
    return source->span;
}
void image_source_destroy(struct image_source *_Nullable source)
{
    if (source != nullptr)
        free(source->target);
    free(source);
}
