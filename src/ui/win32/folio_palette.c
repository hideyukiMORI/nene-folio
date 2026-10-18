#include "folio_palette.h"

#include "theme_contrast.h"
#include "theme_palette.h"

#include <math.h>

static COLORREF to_color(struct rgb_color color)
{
    return RGB(color.red, color.green, color.blue);
}

/* RGB-to-Win32 adaptation only; the core catalog owns every fixed color (ADR0049). */
/* 未保存の合図の 3 値を写す。 */
static void adopt_unsaved(struct folio_palette *_Nonnull out,
                          const struct theme_palette *_Nonnull palette)
{
    struct theme_unsaved unsaved = theme_contrast_unsaved(palette);
    out->unsaved_filled = unsaved.filled;
    out->unsaved_fill = to_color(unsaved.fill);
    out->unsaved_ink = to_color(unsaved.ink);
}

struct folio_palette folio_palette_from(const struct theme_palette *_Nonnull palette)
{
    struct folio_palette adapted = (struct folio_palette){
        .contrast = palette->contrast,
        .window = to_color(palette->window),
        .pane = to_color(palette->pane),
        .header_text = to_color(palette->header_text),
        .category_text = to_color(palette->category_text),
        .note_text = to_color(palette->note_text),
        .selected_background = to_color(palette->selected_background),
        .selected_text = to_color(palette->selected_text),
        .current_text = to_color(palette->current_text),
        .editor_text = to_color(palette->editor_text),
        .chip_background = to_color(palette->chip_background),
        .chip_text = to_color(palette->chip_text),
        .border = to_color(palette->border),
        .breadcrumb_background = to_color(palette->breadcrumb_background),
        .breadcrumb_text = to_color(palette->breadcrumb_text),
        .gutter_text = to_color(palette->gutter_text),
        .dialog_surface = to_color(palette->dialog_surface),
        .dialog_border = to_color(palette->dialog_border),
        .dialog_control = to_color(palette->dialog_control),
        .dialog_secondary = to_color(palette->dialog_secondary),
        .dialog_backdrop = to_color(palette->dialog_backdrop),
        .dialog_backdrop_alpha = palette->dialog_backdrop_alpha,
        .hover_line = to_color(theme_contrast_line(palette, THEME_LINE_HOVER)),
        .focus_line = to_color(theme_contrast_line(palette, THEME_LINE_FOCUS)),
        .focus_item_line = to_color(theme_contrast_line(palette, THEME_LINE_FOCUS_ITEM)),
        .focus_selected_line = to_color(theme_contrast_line(palette, THEME_LINE_FOCUS_SELECTED)),
        .focus_primary_line = to_color(theme_contrast_line(palette, THEME_LINE_FOCUS_PRIMARY)),
        .field_line = to_color(theme_contrast_line(palette, THEME_LINE_FIELD)),
        .field_focus_line = to_color(theme_contrast_line(palette, THEME_LINE_FIELD_FOCUS)),
    };
    adopt_unsaved(&adapted, palette);
    return adapted;
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
