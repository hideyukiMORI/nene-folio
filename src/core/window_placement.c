#include "window_placement.h"
#include "json_reader.h"
#include "json_writer.h"
#include <stdint.h>
#include <string.h>

constexpr uint32_t placement_version = 1;
constexpr uint32_t coordinate_limit = 2 * 1000000;
constexpr uint32_t size_limit = 1000000;
constexpr uint32_t dpi_limit = 96 * 16;

struct window_placement window_placement_default(void)
{
    return (struct window_placement){.restore = true, .dpi = 96};
}

static bool key(struct json_reader *_Nonnull reader, const char *_Nonnull name)
{
    return json_reader_next(reader) == JSON_TOKEN_KEY &&
           json_reader_text_length(reader) == strlen(name) &&
           strcmp(json_reader_text(reader), name) == 0;
}

static bool flag(struct json_reader *_Nonnull reader, const char *_Nonnull name,
                 bool *_Nonnull value)
{
    if (!key(reader, name))
        return false;
    enum json_token token = json_reader_next(reader);
    *value = token == JSON_TOKEN_TRUE;
    return token == JSON_TOKEN_TRUE || token == JSON_TOKEN_FALSE;
}

/* A non-negative integer no larger than `limit`. */
static bool number(struct json_reader *_Nonnull reader, const char *_Nonnull name, uint32_t limit,
                   uint32_t *_Nonnull value)
{
    if (!key(reader, name) || json_reader_next(reader) != JSON_TOKEN_UNSIGNED)
        return false;
    *value = json_reader_unsigned(reader);
    return *value <= limit;
}

static bool rectangle(struct json_reader *_Nonnull reader, struct window_placement *_Nonnull out)
{
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    if (!number(reader, "x", coordinate_limit, &x) || !number(reader, "y", coordinate_limit, &y) ||
        !number(reader, "width", size_limit, &width) ||
        !number(reader, "height", size_limit, &height))
        return false;
    out->x = (int)x - window_placement_bias;
    out->y = (int)y - window_placement_bias;
    out->width = (int)width;
    out->height = (int)height;
    return true;
}

static enum window_placement_outcome document(struct json_reader *_Nonnull reader,
                                              struct window_placement *_Nonnull out)
{
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_BEGIN || !key(reader, "version") ||
        json_reader_next(reader) != JSON_TOKEN_UNSIGNED)
        return WINDOW_PLACEMENT_MALFORMED;
    if (json_reader_unsigned(reader) != placement_version)
        return WINDOW_PLACEMENT_UNSUPPORTED_VERSION;
    uint32_t dpi = 0;
    if (!flag(reader, "restore", &out->restore) || !flag(reader, "placed", &out->placed) ||
        !rectangle(reader, out) || !number(reader, "dpi", dpi_limit, &dpi) || dpi == 0 ||
        !flag(reader, "maximized", &out->maximized))
        return WINDOW_PLACEMENT_MALFORMED;
    out->dpi = dpi;
    return json_reader_next(reader) == JSON_TOKEN_OBJECT_END &&
                   json_reader_next(reader) == JSON_TOKEN_END
               ? WINDOW_PLACEMENT_READY
               : WINDOW_PLACEMENT_MALFORMED;
}

enum window_placement_outcome window_placement_parse(const char *_Nonnull text, size_t length,
                                                     struct window_placement *_Nonnull out)
{
    struct json_reader *_Nullable reader = nullptr;
    if (json_reader_create(text, length, &reader) != JSON_READER_CREATED)
        return WINDOW_PLACEMENT_OUT_OF_MEMORY;
    struct window_placement read = window_placement_default();
    enum window_placement_outcome result = document(reader, &read);
    if (result == WINDOW_PLACEMENT_MALFORMED &&
        json_reader_next(reader) == JSON_TOKEN_OUT_OF_MEMORY)
        result = WINDOW_PLACEMENT_OUT_OF_MEMORY;
    json_reader_destroy(reader);
    if (result == WINDOW_PLACEMENT_READY)
        *out = read;
    return result;
}

/* Values outside what the reader accepts are written as "nothing recorded". */
static bool writable(const struct window_placement *_Nonnull placement)
{
    return placement->x >= -window_placement_bias && placement->x <= window_placement_bias &&
           placement->y >= -window_placement_bias && placement->y <= window_placement_bias &&
           placement->width >= 0 && placement->width <= (int)size_limit && placement->height >= 0 &&
           placement->height <= (int)size_limit && placement->dpi != 0 &&
           placement->dpi <= dpi_limit;
}

void window_placement_write(const struct window_placement *_Nonnull placement,
                            struct json_writer *_Nonnull writer)
{
    struct window_placement value = *placement;
    if (!writable(&value))
    {
        bool restore = value.restore;
        value = window_placement_default();
        value.restore = restore;
    }
    json_writer_object_begin(writer);
    json_writer_key(writer, "version");
    json_writer_unsigned(writer, placement_version);
    json_writer_key(writer, "restore");
    json_writer_boolean(writer, value.restore);
    json_writer_key(writer, "placed");
    json_writer_boolean(writer, value.placed);
    json_writer_key(writer, "x");
    json_writer_unsigned(writer, (uint32_t)(value.x + window_placement_bias));
    json_writer_key(writer, "y");
    json_writer_unsigned(writer, (uint32_t)(value.y + window_placement_bias));
    json_writer_key(writer, "width");
    json_writer_unsigned(writer, (uint32_t)value.width);
    json_writer_key(writer, "height");
    json_writer_unsigned(writer, (uint32_t)value.height);
    json_writer_key(writer, "dpi");
    json_writer_unsigned(writer, value.dpi);
    json_writer_key(writer, "maximized");
    json_writer_boolean(writer, value.maximized);
    json_writer_object_end(writer);
}
