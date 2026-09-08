#include "rtf_palette.h"

/* 採用したデザイン「案2 堅」の値（2026-09-09・ADR 0005）。 */
static const struct rtf_palette dark_palette = {
    .text = {0xC3, 0xC8, 0xCF},
    .heading = {0xFF, 0xFF, 0xFF},
    .muted = {0x7A, 0x82, 0x8C},
    .link = {0x9F, 0xB3, 0xCC},
    .code_text = {0xB4, 0xBA, 0xC2},
    .code_background = {0x06, 0x07, 0x08},
};

static const struct rtf_palette light_palette = {
    .text = {0x2B, 0x31, 0x38},
    .heading = {0x10, 0x14, 0x18},
    .muted = {0x6B, 0x72, 0x80},
    .link = {0x2F, 0x5A, 0xA8},
    .code_text = {0xD5, 0xD8, 0xDC},
    .code_background = {0x1B, 0x1F, 0x24},
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
