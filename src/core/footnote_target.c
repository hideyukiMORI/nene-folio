#include "footnote_target.h"
#include "markdown_references.h"
#include "rtf_text.h"
#include "text_buffer.h"
#include <string.h>

static const char note_scheme[] = "nenefolio-note:";
static const char reference_scheme[] = "nenefolio-noteref:";
constexpr size_t digit_limit = 4;
static_assert(markdown_references_limit < 10000, "a footnote number is at most four digits");

void footnote_target_append(struct text_buffer *_Nonnull out, bool back, size_t number)
{
    text_buffer_append_text(out, back ? reference_scheme : note_scheme);
    rtf_text_number(out, number);
}

/* The digits after a scheme: canonical decimal, 1..limit. */
static bool parse_number(const char *_Nonnull bytes, size_t length, size_t *_Nonnull number)
{
    if (length == 0 || length > digit_limit || bytes[0] == '0')
        return false;
    size_t value = 0;
    for (size_t index = 0; index < length; ++index)
    {
        if (bytes[index] < '0' || bytes[index] > '9')
            return false;
        value = value * 10 + (size_t)(bytes[index] - '0');
    }
    if (value > markdown_references_limit)
        return false;
    *number = value;
    return true;
}

static bool with_scheme(const char *_Nonnull bytes, size_t length, const char *_Nonnull scheme,
                        size_t *_Nonnull number)
{
    size_t prefix = strlen(scheme);
    return length > prefix && memcmp(bytes, scheme, prefix) == 0 &&
           parse_number(bytes + prefix, length - prefix, number);
}

enum footnote_target_outcome footnote_target_parse(const char *_Nonnull bytes, size_t length,
                                                   size_t *_Nonnull number)
{
    if (with_scheme(bytes, length, note_scheme, number))
        return FOOTNOTE_TARGET_NOTE;
    if (with_scheme(bytes, length, reference_scheme, number))
        return FOOTNOTE_TARGET_REFERENCE;
    return FOOTNOTE_TARGET_OTHER;
}
