#ifndef NENEFOLIO_DIAGRAM_PALETTE_H
#define NENEFOLIO_DIAGRAM_PALETTE_H
#include "diagram_ink.h"
#include "rgb_color.h"
#include "theme_palette.h"
struct diagram_palette
{
    struct rgb_color background;
    struct rgb_color text;
    struct rgb_color muted;
    struct rgb_color accent;
    struct rgb_color node;
    struct rgb_color group;
    struct rgb_color note;
};
[[nodiscard]] struct diagram_palette
diagram_palette_for(const struct theme_palette *_Nonnull theme);
[[nodiscard]] struct rgb_color diagram_palette_ink(const struct diagram_palette *_Nonnull palette,
                                                   enum diagram_ink ink);
[[nodiscard]] bool diagram_palette_equal(const struct diagram_palette *_Nonnull first,
                                         const struct diagram_palette *_Nonnull second);
#endif
