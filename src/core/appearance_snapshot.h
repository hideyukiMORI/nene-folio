/* OSから一度に読んだ外観。全メンバーは独立した値、色はHIGHのときだけ使う（ADR0057）。 */
#ifndef NENEFOLIO_APPEARANCE_SNAPSHOT_H
#define NENEFOLIO_APPEARANCE_SNAPSHOT_H

#include "folio_contrast.h"
#include "folio_theme_choice.h"
#include "theme_palette.h"

struct appearance_snapshot
{
    enum folio_theme theme; /* adapterはLIGHT/DARKに限定する。 */
    enum folio_contrast contrast;
    struct rgb_color window;
    struct rgb_color window_text;
    struct rgb_color button;
    struct rgb_color button_text;
    struct rgb_color highlight;
    struct rgb_color highlight_text;
};

[[nodiscard]] struct theme_palette
appearance_snapshot_palette(const struct appearance_snapshot *_Nonnull snapshot,
                            enum folio_theme_choice choice);

#endif
