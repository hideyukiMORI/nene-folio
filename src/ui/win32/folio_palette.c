#include "folio_palette.h"

#include <math.h>

/* Ubuntu 風の配色（ADR 0031 の決定 5。実測 out/design/2026-09-22/theme-probe/ の候補 1・2）。
 * 「案2 堅」の骨格（ADR 0005）はそのままで、色だけを aubergine 系へ移した。
 * orange の面に白字は載せない（3.65:1）ので、札の字は地の色にしてある。
 * 3 対（本文と地・選択・番号帯）が 4.5:1 以上であることは Win32 部品 probe が固定する。 */
static const struct folio_palette dark_palette = {
    .window = RGB(0x1A, 0x00, 0x11),
    .pane = RGB(0x2C, 0x00, 0x1E),
    .header_text = RGB(0x9C, 0x85, 0x93),
    .category_text = RGB(0xF4, 0xEA, 0xEF),
    .note_text = RGB(0xC4, 0xAE, 0xBC),
    .selected_background = RGB(0x77, 0x29, 0x53),
    .selected_text = RGB(0xFF, 0xFF, 0xFF),
    .current_text = RGB(0xEA, 0xDC, 0xE4),
    .editor_text = RGB(0xEF, 0xE4, 0xEA),
    .chip_background = RGB(0xE9, 0x54, 0x20),
    .chip_text = RGB(0x2C, 0x00, 0x1E),
    .border = RGB(0x4B, 0x25, 0x40),
    .breadcrumb_background = RGB(0x77, 0x21, 0x6F),
    .breadcrumb_text = RGB(0xF7, 0xEA, 0xF5),
    /* 地 #2C001E に対し 6.33:1。本文より薄く、頭の薄い文字より濃い（ADR 0026 の決定 7）。 */
    .gutter_text = RGB(0xA9, 0x8F, 0xA0),
};

static const struct folio_palette light_palette = {
    .window = RGB(0xF7, 0xF7, 0xF7),
    .pane = RGB(0xFF, 0xFF, 0xFF),
    .header_text = RGB(0x74, 0x6B, 0x66),
    .category_text = RGB(0x1A, 0x0F, 0x14),
    .note_text = RGB(0x58, 0x4E, 0x4A),
    .selected_background = RGB(0xF0, 0xE4, 0xEC),
    .selected_text = RGB(0x2C, 0x00, 0x1E),
    .current_text = RGB(0x3A, 0x2A, 0x33),
    .editor_text = RGB(0x24, 0x13, 0x18),
    .chip_background = RGB(0x77, 0x29, 0x53),
    .chip_text = RGB(0xFF, 0xFF, 0xFF),
    .border = RGB(0xDB, 0xD3, 0xCF),
    .breadcrumb_background = RGB(0x77, 0x21, 0x6F),
    .breadcrumb_text = RGB(0xFF, 0xFF, 0xFF),
    /* 地 #FFFFFF に対し 6.10:1（ADR 0026 の決定 7）。 */
    .gutter_text = RGB(0x6A, 0x60, 0x5C),
};

struct folio_palette folio_palette_for(enum folio_theme theme)
{
    switch (theme)
    {
    case FOLIO_THEME_LIGHT:
        return light_palette;
    case FOLIO_THEME_DARK:
        return dark_palette;
    }
    return light_palette;
}

static double linear_channel(BYTE value)
{
    double channel = (double)value / 255.0;
    return channel <= 0.04045 ? channel / 12.92 : pow((channel + 0.055) / 1.055, 2.4);
}

COLORREF folio_palette_ink(COLORREF background)
{
    double luminance = 0.2126 * linear_channel(GetRValue(background)) +
                       0.7152 * linear_channel(GetGValue(background)) +
                       0.0722 * linear_channel(GetBValue(background));
    double black_contrast = (luminance + 0.05) / 0.05;
    double white_contrast = 1.05 / (luminance + 0.05);
    return black_contrast >= white_contrast ? RGB(0, 0, 0) : RGB(255, 255, 255);
}
