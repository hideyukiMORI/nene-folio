#include "code_copy_target.h"
#include "markdown_media.h"
#include "rtf_text.h"
#include "text_buffer.h"
#include <string.h>

static const char scheme[] = "nenefolio-code:";
constexpr size_t scheme_length = sizeof scheme - 1;
constexpr size_t digit_limit = 3;
static_assert(markdown_media_display_limit <= 1000, "a slot is written in at most three digits");

void code_copy_target_append(struct text_buffer *_Nonnull out, size_t slot)
{
    text_buffer_append_text(out, scheme);
    rtf_text_number(out, slot);
}

enum code_copy_target_outcome code_copy_target_parse(const char *_Nonnull bytes, size_t length,
                                                     size_t *_Nonnull slot)
{
    if (length <= scheme_length || length > scheme_length + digit_limit ||
        memcmp(bytes, scheme, scheme_length) != 0)
        return CODE_COPY_TARGET_OTHER;
    size_t value = 0;
    for (size_t index = scheme_length; index < length; ++index)
    {
        if (bytes[index] < '0' || bytes[index] > '9')
            return CODE_COPY_TARGET_OTHER;
        value = value * 10 + (size_t)(bytes[index] - '0');
    }
    bool padded = bytes[scheme_length] == '0' && length > scheme_length + 1;
    if (padded || value >= markdown_media_display_limit)
        return CODE_COPY_TARGET_OTHER;
    *slot = value;
    return CODE_COPY_TARGET_SLOT;
}
