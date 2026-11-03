#include "diagram_palette.h"
#include "theme_contrast.h"
static struct rgb_color tinted(struct rgb_color background, struct rgb_color accent, unsigned parts)
{
    return (struct rgb_color){
        (unsigned char)(((16 - parts) * background.red + parts * accent.red) / 16),
        (unsigned char)(((16 - parts) * background.green + parts * accent.green) / 16),
        (unsigned char)(((16 - parts) * background.blue + parts * accent.blue) / 16)};
}
static struct rgb_color readable_fill(struct rgb_color background, struct rgb_color accent,
                                      struct rgb_color text, unsigned parts)
{
    struct rgb_color fill = tinted(background, accent, parts);
    return theme_contrast_ratio(fill, text) >= 4.5 ? fill : background;
}
struct diagram_palette diagram_palette_for(const struct theme_palette *_Nonnull theme)
{
    struct rgb_color text = theme->rtf.text;
    if (theme_contrast_ratio(text, theme->pane) < 4.5)
    {
        struct rgb_color dark = {0, 0, 0}, light = {255, 255, 255};
        text = theme_contrast_ratio(dark, theme->pane) >= theme_contrast_ratio(light, theme->pane)
                   ? dark
                   : light;
    }
    struct rgb_color node = readable_fill(theme->pane, theme->rtf.link, text, 2);
    struct diagram_palette palette = {
        .background = theme->pane,
        .text = text,
        .muted = tinted(theme->pane, text, 10),
        .accent = theme_contrast_ratio(theme->rtf.link, node) >= 3.0 ? theme->rtf.link : text,
        .node = node,
        .group = readable_fill(theme->pane, theme->rtf.link, text, 1),
        .note = readable_fill(theme->pane, theme->rtf.link, text, 3)};
    if (theme_contrast_ratio(palette.muted, palette.background) < 3.0 ||
        theme_contrast_ratio(palette.muted, palette.node) < 3.0 ||
        theme_contrast_ratio(palette.muted, palette.group) < 3.0 ||
        theme_contrast_ratio(palette.muted, palette.note) < 3.0)
        palette.muted = text;
    return palette;
}
struct rgb_color diagram_palette_ink(const struct diagram_palette *_Nonnull palette,
                                     enum diagram_ink ink)
{
    switch (ink)
    {
    case DIAGRAM_INK_NONE:
    case DIAGRAM_INK_BACKGROUND:
        return palette->background;
    case DIAGRAM_INK_TEXT:
        return palette->text;
    case DIAGRAM_INK_MUTED:
        return palette->muted;
    case DIAGRAM_INK_ACCENT:
        return palette->accent;
    case DIAGRAM_INK_NODE:
        return palette->node;
    case DIAGRAM_INK_GROUP:
        return palette->group;
    case DIAGRAM_INK_NOTE:
        return palette->note;
    }
    return palette->text;
}
bool diagram_palette_equal(const struct diagram_palette *_Nonnull first,
                           const struct diagram_palette *_Nonnull second)
{
    return rgb_color_equal(first->background, second->background) &&
           rgb_color_equal(first->text, second->text) &&
           rgb_color_equal(first->muted, second->muted) &&
           rgb_color_equal(first->accent, second->accent) &&
           rgb_color_equal(first->node, second->node) &&
           rgb_color_equal(first->group, second->group) &&
           rgb_color_equal(first->note, second->note);
}
