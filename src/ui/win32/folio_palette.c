#include "folio_palette.h"

/* 採用したデザイン「案2 堅」の値（2026-09-09・ADR 0005）。 */
static const struct folio_palette dark_palette = {
    .window = RGB(0x0A, 0x0B, 0x0D),
    .pane = RGB(0x10, 0x12, 0x14),
    .header_text = RGB(0x4A, 0x50, 0x59),
    .category_text = RGB(0xE2, 0xE4, 0xE7),
    .note_text = RGB(0x8A, 0x91, 0x9B),
    .selected_background = RGB(0x16, 0x19, 0x1D),
    .selected_text = RGB(0xFF, 0xFF, 0xFF),
    .current_text = RGB(0xC3, 0xC8, 0xCF),
    .chip_background = RGB(0x1C, 0x20, 0x26),
    .chip_text = RGB(0xE2, 0xE4, 0xE7),
    .border = RGB(0x24, 0x27, 0x2C),
};

static const struct folio_palette light_palette = {
    .window = RGB(0xF4, 0xF5, 0xF7),
    .pane = RGB(0xFF, 0xFF, 0xFF),
    .header_text = RGB(0x8A, 0x91, 0x9B),
    .category_text = RGB(0x1B, 0x1F, 0x24),
    .note_text = RGB(0x5C, 0x63, 0x6B),
    .selected_background = RGB(0xE9, 0xEB, 0xEF),
    .selected_text = RGB(0x1B, 0x1F, 0x24),
    .current_text = RGB(0x3C, 0x43, 0x4C),
    .chip_background = RGB(0xE9, 0xEB, 0xEF),
    .chip_text = RGB(0x1B, 0x1F, 0x24),
    .border = RGB(0xD9, 0xDD, 0xE3),
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
