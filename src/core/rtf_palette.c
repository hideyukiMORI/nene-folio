#include "rtf_palette.h"

/* Ubuntu 風の配色（ADR 0031 の決定 5。実測 out/design/2026-09-22/theme-probe/ の候補 1・2）。
 * 「案2 堅」の骨格（ADR 0005）はそのままで、色だけを aubergine 系へ移した。
 * 本文と地のコントラスト比は ui の folio_palette.pane と対で見る（Win32 部品 probe が固定する）。
 */
static const struct rtf_palette dark_palette = {
    .text = {0xEF, 0xE4, 0xEA},
    .heading = {0xFF, 0xFF, 0xFF},
    .muted = {0xBF, 0xA6, 0xB6},
    .link = {0xFF, 0x92, 0x69},
    .code_text = {0xE3, 0xD3, 0xDC},
    .code_background = {0x16, 0x00, 0x0E},
};

static const struct rtf_palette light_palette = {
    .text = {0x24, 0x13, 0x18},
    .heading = {0x2C, 0x00, 0x1E},
    .muted = {0x6A, 0x60, 0x5C},
    .link = {0xB2, 0x3C, 0x12},
    .code_text = {0xED, 0xE1, 0xE8},
    .code_background = {0x2C, 0x00, 0x1E},
};

struct rtf_palette rtf_palette_for(enum folio_theme theme)
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
