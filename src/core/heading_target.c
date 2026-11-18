#include "heading_target.h"
#include "text_buffer.h"
#include <string.h>

static const char prefix[] = "<nenefolio:heading:";
static_assert(sizeof prefix - 1 + 2 == heading_target_length, "prefix, one digit and the bracket");

void heading_target_append(struct text_buffer *_Nonnull out, size_t level)
{
    const char digit = (char)('0' + level);
    text_buffer_append_text(out, "{\\v ");
    text_buffer_append_text(out, prefix);
    text_buffer_append(out, &digit, 1);
    text_buffer_append_text(out, ">}");
}

size_t heading_target_parse(const char *_Nonnull bytes, size_t length)
{
    size_t head = sizeof prefix - 1;
    if (length != heading_target_length || memcmp(bytes, prefix, head) != 0 ||
        bytes[head + 1] != '>')
        return 0;
    char digit = bytes[head];
    return digit >= '1' && digit <= '0' + (char)heading_target_levels ? (size_t)(digit - '0') : 0;
}
