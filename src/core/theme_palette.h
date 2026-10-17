/* Platform-independent colors, owned by the fixed theme catalog (ADR0049).
 * Members are independently valid RGB values; a complete value type is intentional (C-003). */
#ifndef NENEFOLIO_THEME_PALETTE_H
#define NENEFOLIO_THEME_PALETTE_H

#include "folio_contrast.h"
#include "rgb_color.h"
#include "rtf_palette.h"

struct theme_palette
{
    enum folio_contrast contrast;
    struct rgb_color window;
    struct rgb_color pane;
    struct rgb_color header_text;
    struct rgb_color category_text;
    struct rgb_color note_text;
    struct rgb_color selected_background;
    struct rgb_color selected_text;
    struct rgb_color current_text;
    struct rgb_color editor_text;
    struct rgb_color chip_background;
    struct rgb_color chip_text;
    struct rgb_color border;
    struct rgb_color breadcrumb_background;
    struct rgb_color breadcrumb_text;
    struct rgb_color gutter_text;
    struct rgb_color dialog_surface;
    struct rgb_color dialog_border;
    struct rgb_color dialog_control;
    struct rgb_color dialog_secondary;
    struct rgb_color dialog_backdrop;
    unsigned char dialog_backdrop_alpha;
    struct rtf_palette rtf;
};

[[nodiscard]] bool theme_palette_equal(const struct theme_palette *_Nonnull left,
                                       const struct theme_palette *_Nonnull right);

#endif
