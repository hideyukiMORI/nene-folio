#include "appearance_snapshot.h"

/* OSのforeground/background対を崩さず、通常配色と同じ役割へ写す唯一の経路。 */
static struct theme_palette high_palette(const struct appearance_snapshot *_Nonnull snapshot)
{
    return (struct theme_palette){
        .contrast = FOLIO_CONTRAST_HIGH,
        .window = snapshot->window,
        .pane = snapshot->window,
        .header_text = snapshot->window_text,
        .category_text = snapshot->window_text,
        .note_text = snapshot->window_text,
        .selected_background = snapshot->highlight,
        .selected_text = snapshot->highlight_text,
        .current_text = snapshot->window_text,
        .editor_text = snapshot->window_text,
        .chip_background = snapshot->button,
        .chip_text = snapshot->button_text,
        .border = snapshot->window_text,
        .breadcrumb_background = snapshot->window,
        .breadcrumb_text = snapshot->window_text,
        .gutter_text = snapshot->window_text,
        .dialog_surface = snapshot->window,
        .dialog_border = snapshot->window_text,
        .dialog_control = snapshot->window,
        .dialog_secondary = snapshot->window_text,
        .dialog_backdrop = snapshot->window,
        .dialog_backdrop_alpha = 255,
        .rtf = {.text = snapshot->window_text,
                .heading = snapshot->window_text,
                .muted = snapshot->window_text,
                .link = snapshot->window_text,
                .code_text = snapshot->window_text,
                .code_background = snapshot->window},
    };
}

struct theme_palette
appearance_snapshot_palette(const struct appearance_snapshot *_Nonnull snapshot,
                            enum folio_theme_choice choice)
{
    switch (snapshot->contrast)
    {
    case FOLIO_CONTRAST_STANDARD:
        return *folio_theme_palette(folio_theme_choice_resolve(choice, snapshot->theme));
    case FOLIO_CONTRAST_HIGH:
        return high_palette(snapshot);
    }
    return *folio_theme_palette(FOLIO_THEME_LIGHT);
}
