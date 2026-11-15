#include "metadata_scalar.h"
#include "text_buffer.h"
#include "utf8_text.h"
#include <stdlib.h>
#include <string.h>

struct metadata_scalar
{
    struct text_buffer *_Nonnull decoded;
    size_t consumed;
    bool string;
};

static bool space(char byte)
{
    return byte == ' ' || byte == '\t';
}

static bool digit(char byte)
{
    return byte >= '0' && byte <= '9';
}

static int hex(char byte)
{
    if (byte >= '0' && byte <= '9')
        return byte - '0';
    if (byte >= 'a' && byte <= 'f')
        return byte - 'a' + 10;
    if (byte >= 'A' && byte <= 'F')
        return byte - 'A' + 10;
    return -1;
}

static bool radix_number(const char *_Nonnull bytes, size_t length)
{
    if (length < 3 || bytes[0] != '0')
        return false;
    unsigned base = bytes[1] == 'o' ? 8 : 16;
    if (bytes[1] != 'o' && bytes[1] != 'x')
        return false;
    for (size_t at = 2; at < length; ++at)
    {
        int value = hex(bytes[at]);
        if (value < 0 || (unsigned)value >= base)
            return false;
    }
    return true;
}

static size_t digits(const char *_Nonnull bytes, size_t length, size_t at)
{
    while (at < length && digit(bytes[at]))
        ++at;
    return at;
}

static size_t signed_start(const char *_Nonnull bytes, size_t length, size_t at)
{
    return at < length && (bytes[at] == '+' || bytes[at] == '-') ? at + 1 : at;
}

static bool exponent(const char *_Nonnull bytes, size_t length, size_t at)
{
    if (at == length)
        return true;
    if (bytes[at] != 'e' && bytes[at] != 'E')
        return false;
    size_t first = signed_start(bytes, length, at + 1);
    return digits(bytes, length, first) == length && first < length;
}

static bool decimal_number(const char *_Nonnull bytes, size_t length)
{
    size_t at = signed_start(bytes, length, 0);
    size_t first = at;
    at = digits(bytes, length, at);
    size_t whole = at - first;
    size_t fraction = 0;
    if (at < length && bytes[at] == '.')
    {
        first = ++at;
        at = digits(bytes, length, at);
        fraction = at - first;
    }
    return (whole > 0 || fraction > 0) && exponent(bytes, length, at);
}

static bool resolved_string(const char *_Nonnull bytes, size_t length)
{
    static const char *const nonstrings[] = {"null",  "Null",  "NULL",  "~",     "true",  "True",
                                             "TRUE",  "false", "False", "FALSE", ".inf",  ".Inf",
                                             ".INF",  "+.inf", "+.Inf", "+.INF", "-.inf", "-.Inf",
                                             "-.INF", ".nan",  ".NaN",  ".NAN"};
    for (size_t index = 0; index < sizeof nonstrings / sizeof nonstrings[0]; ++index)
        if (length == strlen(nonstrings[index]) && memcmp(bytes, nonstrings[index], length) == 0)
            return false;
    return !radix_number(bytes, length) && !decimal_number(bytes, length);
}

static bool printable(uint32_t point)
{
    return point == 9 || (point >= 0x20 && point <= 0x7e) || point == 0x85 ||
           (point >= 0xa0 && point <= 0xd7ff) || (point >= 0xe000 && point <= 0xfffd) ||
           (point >= 0x10000 && point <= 0x10ffff);
}

bool metadata_scalar_printable(const char *_Nonnull bytes, size_t length)
{
    for (size_t at = 0; at < length;)
    {
        uint32_t point = 0;
        size_t step = utf8_text_decode(bytes + at, length - at, &point);
        if (step == 0 || !printable(point))
            return false;
        at += step;
    }
    return true;
}

static bool append_point(struct metadata_scalar *_Nonnull scalar, uint32_t point)
{
    char encoded[utf8_text_max_encoded];
    size_t count = utf8_text_encode(point, encoded);
    if (count == 0)
        return false;
    text_buffer_append(scalar->decoded, encoded, count);
    return true;
}

static bool escaped_point(char code, uint32_t *_Nonnull point)
{
    static const char codes[] = {'0', 'a', 'b', 't',  'n', 'v', 'f', 'r', 'e',
                                 ' ', '"', '/', '\\', 'N', '_', 'L', 'P', '\t'};
    static const uint32_t values[] = {0,  7,  8,  9,  10,   11,   12,     13,     27,
                                      32, 34, 47, 92, 0x85, 0xa0, 0x2028, 0x2029, 9};
    for (size_t index = 0; index < sizeof codes; ++index)
        if (codes[index] == code)
        {
            *point = values[index];
            return true;
        }
    return false;
}

static bool escape(struct metadata_scalar *_Nonnull scalar, const char *_Nonnull bytes,
                   size_t length, size_t *_Nonnull at)
{
    if (++*at == length)
        return false;
    char code = bytes[(*at)++];
    uint32_t point = 0;
    if (escaped_point(code, &point))
        return append_point(scalar, point);
    size_t count = code == 'x' ? 2 : 0;
    count = code == 'u' ? 4 : count;
    count = code == 'U' ? 8 : count;
    if (count == 0 || length - *at < count)
        return false;
    for (size_t index = 0; index < count; ++index)
    {
        int value = hex(bytes[(*at)++]);
        if (value < 0)
            return false;
        point = (point << 4) | (uint32_t)value;
    }
    return append_point(scalar, point);
}

static bool literal(struct metadata_scalar *_Nonnull scalar, const char *_Nonnull bytes,
                    size_t length, size_t *_Nonnull at)
{
    uint32_t point = 0;
    size_t count = utf8_text_decode(bytes + *at, length - *at, &point);
    if (count == 0 || !printable(point))
        return false;
    text_buffer_append(scalar->decoded, bytes + *at, count);
    *at += count;
    return true;
}

static bool quote_closed(const char *_Nonnull bytes, size_t length, size_t *_Nonnull at)
{
    if (bytes[*at] != bytes[0])
        return false;
    ++*at;
    return bytes[0] != '\'' || *at == length || bytes[*at] != '\'';
}

static bool quoted_character(struct metadata_scalar *_Nonnull scalar, const char *_Nonnull bytes,
                             size_t length, size_t *_Nonnull at)
{
    return bytes[*at] == '\\' && bytes[0] == '"' ? escape(scalar, bytes, length, at)
                                                 : literal(scalar, bytes, length, at);
}

static enum metadata_scalar_outcome quoted(struct metadata_scalar *_Nonnull scalar,
                                           const char *_Nonnull bytes, size_t length)
{
    size_t at = 1;
    while (at < length)
    {
        if (quote_closed(bytes, length, &at))
        {
            scalar->consumed = at;
            scalar->string = true;
            return METADATA_SCALAR_READY;
        }
        if (!quoted_character(scalar, bytes, length, &at))
            return METADATA_SCALAR_MALFORMED;
    }
    return METADATA_SCALAR_MALFORMED;
}

static bool reserved_start(const char *_Nonnull bytes, size_t length)
{
    if (strchr("!&*{}[],#|>@`%", bytes[0]) != nullptr)
        return true;
    return strchr("-?:", bytes[0]) != nullptr && (length == 1 || space(bytes[1]));
}

static bool plain_invalid(const char *_Nonnull bytes, size_t length, size_t at, bool flow)
{
    if (flow && strchr("{}[", bytes[at]) != nullptr)
        return true;
    if (bytes[at] != ':')
        return false;
    return at + 1 == length || space(bytes[at + 1]) ||
           (flow && strchr(",[]{}", bytes[at + 1]) != nullptr);
}

static bool plain_stop(const char *_Nonnull bytes, size_t at, bool flow)
{
    bool comment = bytes[at] == '#' && at > 0 && space(bytes[at - 1]);
    bool delimiter = bytes[at] == ',' || bytes[at] == ']';
    return comment || (flow && delimiter);
}

static enum metadata_scalar_outcome scan_plain(const char *_Nonnull bytes, size_t length, bool flow,
                                               size_t *_Nonnull consumed)
{
    if (flow && strchr("-?", bytes[0]) != nullptr && length > 1 &&
        strchr(",[]{}", bytes[1]) != nullptr)
        return METADATA_SCALAR_MALFORMED;
    size_t end = 0;
    while (end < length)
    {
        if (plain_stop(bytes, end, flow))
            break;
        if (plain_invalid(bytes, length, end, flow))
            return METADATA_SCALAR_MALFORMED;
        ++end;
    }
    while (end > 0 && space(bytes[end - 1]))
        --end;
    *consumed = end;
    return METADATA_SCALAR_READY;
}

static enum metadata_scalar_outcome plain(struct metadata_scalar *_Nonnull scalar,
                                          const char *_Nonnull bytes, size_t length, bool flow)
{
    if (reserved_start(bytes, length))
        return METADATA_SCALAR_UNSUPPORTED;
    size_t end = 0;
    enum metadata_scalar_outcome scanned = scan_plain(bytes, length, flow, &end);
    if (scanned != METADATA_SCALAR_READY)
        return scanned;
    if (end == 0 || !metadata_scalar_printable(bytes, end))
        return METADATA_SCALAR_MALFORMED;
    text_buffer_append(scalar->decoded, bytes, end);
    scalar->consumed = end;
    scalar->string = resolved_string(bytes, end);
    return METADATA_SCALAR_READY;
}

enum metadata_scalar_outcome metadata_scalar_create(const char *_Nonnull bytes, size_t length,
                                                    enum metadata_scalar_context context,
                                                    struct metadata_scalar *_Nullable *_Nonnull out)
{
    bool flow = false;
    switch (context)
    {
    case METADATA_SCALAR_BLOCK:
        break;
    case METADATA_SCALAR_FLOW:
        flow = true;
        break;
    }
    if (length == 0)
        return METADATA_SCALAR_MALFORMED;
    struct metadata_scalar *_Nullable scalar = calloc(1, sizeof *scalar);
    if (scalar == nullptr)
        return METADATA_SCALAR_OUT_OF_MEMORY;
    struct text_buffer *_Nullable decoded = nullptr;
    if (text_buffer_create(&decoded) != TEXT_BUFFER_ACCEPTED)
    {
        free(scalar);
        return METADATA_SCALAR_OUT_OF_MEMORY;
    }
    scalar->decoded = decoded;
    enum metadata_scalar_outcome outcome = bytes[0] == '"' || bytes[0] == '\''
                                               ? quoted(scalar, bytes, length)
                                               : plain(scalar, bytes, length, flow);
    if (text_buffer_finish(decoded) != TEXT_BUFFER_ACCEPTED)
        outcome = METADATA_SCALAR_OUT_OF_MEMORY;
    if (outcome != METADATA_SCALAR_READY)
    {
        metadata_scalar_destroy(scalar);
        return outcome;
    }
    *out = scalar;
    return METADATA_SCALAR_READY;
}

const char *_Nonnull metadata_scalar_bytes(const struct metadata_scalar *_Nonnull scalar)
{
    return text_buffer_bytes(scalar->decoded);
}

size_t metadata_scalar_length(const struct metadata_scalar *_Nonnull scalar)
{
    return text_buffer_length(scalar->decoded);
}

size_t metadata_scalar_consumed(const struct metadata_scalar *_Nonnull scalar)
{
    return scalar->consumed;
}

bool metadata_scalar_is_string(const struct metadata_scalar *_Nonnull scalar)
{
    return scalar->string;
}

void metadata_scalar_destroy(struct metadata_scalar *_Nullable scalar)
{
    if (scalar == nullptr)
        return;
    text_buffer_destroy(scalar->decoded);
    free(scalar);
}
