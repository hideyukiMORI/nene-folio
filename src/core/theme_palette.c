#include "theme_palette.h"

static bool same_text(const struct theme_palette *_Nonnull left,
                      const struct theme_palette *_Nonnull right)
{
    return rgb_color_equal(left->header_text, right->header_text) &&
           rgb_color_equal(left->category_text, right->category_text) &&
           rgb_color_equal(left->note_text, right->note_text) &&
           rgb_color_equal(left->current_text, right->current_text) &&
           rgb_color_equal(left->editor_text, right->editor_text) &&
           rgb_color_equal(left->gutter_text, right->gutter_text);
}

static bool same_surfaces(const struct theme_palette *_Nonnull left,
                          const struct theme_palette *_Nonnull right)
{
    return rgb_color_equal(left->window, right->window) &&
           rgb_color_equal(left->pane, right->pane) &&
           rgb_color_equal(left->selected_background, right->selected_background) &&
           rgb_color_equal(left->selected_text, right->selected_text) &&
           rgb_color_equal(left->chip_background, right->chip_background) &&
           rgb_color_equal(left->chip_text, right->chip_text) &&
           rgb_color_equal(left->border, right->border) &&
           rgb_color_equal(left->breadcrumb_background, right->breadcrumb_background) &&
           rgb_color_equal(left->breadcrumb_text, right->breadcrumb_text);
}

static bool same_dialog(const struct theme_palette *_Nonnull left,
                        const struct theme_palette *_Nonnull right)
{
    return rgb_color_equal(left->dialog_surface, right->dialog_surface) &&
           rgb_color_equal(left->dialog_border, right->dialog_border) &&
           rgb_color_equal(left->dialog_control, right->dialog_control) &&
           rgb_color_equal(left->dialog_secondary, right->dialog_secondary) &&
           rgb_color_equal(left->dialog_backdrop, right->dialog_backdrop) &&
           left->dialog_backdrop_alpha == right->dialog_backdrop_alpha;
}

static bool same_rtf(struct rtf_palette left, struct rtf_palette right)
{
    return rgb_color_equal(left.text, right.text) && rgb_color_equal(left.heading, right.heading) &&
           rgb_color_equal(left.muted, right.muted) && rgb_color_equal(left.link, right.link) &&
           rgb_color_equal(left.code_text, right.code_text) &&
           rgb_color_equal(left.code_background, right.code_background);
}

bool theme_palette_equal(const struct theme_palette *_Nonnull left,
                         const struct theme_palette *_Nonnull right)
{
    return left->contrast == right->contrast && same_text(left, right) &&
           same_surfaces(left, right) && same_dialog(left, right) &&
           same_rtf(left->rtf, right->rtf);
}
