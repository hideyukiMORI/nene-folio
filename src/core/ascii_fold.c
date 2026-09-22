#include "ascii_fold.h"

char ascii_fold_byte(char value)
{
    return value >= 'A' && value <= 'Z' ? (char)(value + ('a' - 'A')) : value;
}

char16_t ascii_fold_unit(char16_t unit)
{
    return unit >= u'A' && unit <= u'Z' ? (char16_t)(unit + (u'a' - u'A')) : unit;
}
