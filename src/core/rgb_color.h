/* 8 bit 3 成分の色。全メンバーが独立に妥当なので完全型で公開する（C-003）。 */
#ifndef NENEFOLIO_RGB_COLOR_H
#define NENEFOLIO_RGB_COLOR_H

#include "rgb_color_outcome.h"

#include <stddef.h>

struct rgb_color
{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
};

/* "#RRGGBB" の文字数。終端は含まない。 */
constexpr size_t rgb_color_hex_length = 7;

[[nodiscard]] enum rgb_color_outcome rgb_color_parse_hex(const char *_Nonnull text, size_t length,
                                                         struct rgb_color *_Nonnull out);

/* out は rgb_color_hex_length 文字ぶんの領域。終端は書かない。大文字で書く。 */
void rgb_color_write_hex(struct rgb_color color, char *_Nonnull out);

#endif
