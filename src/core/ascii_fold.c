#include "ascii_fold.h"
#include <string.h>

char ascii_fold_byte(char value)
{
    return value >= 'A' && value <= 'Z' ? (char)(value + ('a' - 'A')) : value;
}

char16_t ascii_fold_unit(char16_t unit)
{
    return unit >= u'A' && unit <= u'Z' ? (char16_t)(unit + (u'a' - u'A')) : unit;
}

static bool same_folded_span(const char *_Nonnull candidate, const char *_Nonnull text,
                             size_t length)
{
    for (size_t index = 0; index < length; ++index)
    {
        if (ascii_fold_byte(candidate[index]) != ascii_fold_byte(text[index]))
        {
            return false;
        }
    }
    return true;
}

bool ascii_fold_contains(const char *_Nonnull candidate, const char *_Nonnull text, size_t length)
{
    size_t available = strlen(candidate);
    if (length > available)
    {
        return false;
    }
    for (size_t at = 0; at <= available - length; ++at)
    {
        if (same_folded_span(candidate + at, text, length))
        {
            return true;
        }
    }
    return false;
}
