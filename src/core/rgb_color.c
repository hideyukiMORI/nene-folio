#include "rgb_color.h"

static int hex_digit_value(char digit)
{
    if (digit >= '0' && digit <= '9')
    {
        return digit - '0';
    }
    if (digit >= 'a' && digit <= 'f')
    {
        return digit - 'a' + 10;
    }
    if (digit >= 'A' && digit <= 'F')
    {
        return digit - 'A' + 10;
    }
    return -1;
}

static int hex_pair_value(const char *_Nonnull pair)
{
    int high = hex_digit_value(pair[0]);
    int low = hex_digit_value(pair[1]);
    if (high < 0 || low < 0)
    {
        return -1;
    }
    return high * 16 + low;
}

enum rgb_color_outcome rgb_color_parse_hex(const char *_Nonnull text, size_t length,
                                           struct rgb_color *_Nonnull out)
{
    if (length != rgb_color_hex_length || text[0] != '#')
    {
        return RGB_COLOR_MALFORMED;
    }
    int red = hex_pair_value(text + 1);
    int green = hex_pair_value(text + 3);
    int blue = hex_pair_value(text + 5);
    if (red < 0 || green < 0 || blue < 0)
    {
        return RGB_COLOR_MALFORMED;
    }
    out->red = (unsigned char)red;
    out->green = (unsigned char)green;
    out->blue = (unsigned char)blue;
    return RGB_COLOR_PARSED;
}

static void write_pair(unsigned char value, char *_Nonnull out)
{
    static const char digits[] = "0123456789ABCDEF";
    out[0] = digits[value / 16];
    out[1] = digits[value % 16];
}

void rgb_color_write_hex(struct rgb_color color, char *_Nonnull out)
{
    out[0] = '#';
    write_pair(color.red, out + 1);
    write_pair(color.green, out + 3);
    write_pair(color.blue, out + 5);
}
