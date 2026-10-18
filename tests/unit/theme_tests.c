/* Fixed upstream identity/body-color contract for Issue #294.
 * Independently captured from NeNeNib 5b7dbb2; Folio-specific derived roles are tested by contrast.
 */
#include "folio_command.h"
#include "folio_theme_choice.h"
#include "theme_contrast.h"
#include "theme_palette.h"
#include "unit_tests.h"
#include <math.h>
#include <string.h>

static const struct
{
    const char *_Nonnull name;
    struct rgb_color background;
    struct rgb_color foreground;
} nib_expected[] = {
    {"ubuntu-aubergine", {0x30, 0x0A, 0x24}, {0xEE, 0xEE, 0xEC}},
    {"neutral-light", {0xF4, 0xF5, 0xF7}, {0x1B, 0x1F, 0x24}},
    {"solarized-dark", {0x00, 0x2B, 0x36}, {0x83, 0x94, 0x96}},
    {"solarized-light", {0xFD, 0xF6, 0xE3}, {0x58, 0x6E, 0x75}},
    {"monokai", {0x27, 0x28, 0x22}, {0xF8, 0xF8, 0xF2}},
    {"dracula", {0x28, 0x2A, 0x36}, {0xF8, 0xF8, 0xF2}},
    {"one-dark", {0x28, 0x2C, 0x34}, {0xAB, 0xB2, 0xBF}},
    {"night-owl", {0x01, 0x16, 0x27}, {0xD6, 0xDE, 0xEB}},
    {"night-owl-light", {0xFB, 0xFB, 0xFB}, {0x40, 0x3F, 0x53}},
    {"blue", {0x00, 0x00, 0x87}, {0xFF, 0xD7, 0x00}},
    {"catppuccin", {0x1E, 0x1E, 0x2E}, {0xCD, 0xD6, 0xF4}},
    {"darkblue", {0x00, 0x00, 0x40}, {0xC0, 0xC0, 0xC0}},
    {"default", {0x00, 0x00, 0x00}, {0xFF, 0xFF, 0xFF}},
    {"delek", {0xFF, 0xFF, 0xFF}, {0x00, 0x00, 0x00}},
    {"desert", {0x33, 0x33, 0x33}, {0xFF, 0xFF, 0xFF}},
    {"elflord", {0x00, 0x00, 0x00}, {0x00, 0xFF, 0xFF}},
    {"evening", {0x33, 0x33, 0x33}, {0xFF, 0xFF, 0xFF}},
    {"habamax", {0x1C, 0x1C, 0x1C}, {0xC7, 0xC7, 0xC7}},
    {"industry", {0x00, 0x00, 0x00}, {0xDA, 0xDA, 0xDA}},
    {"koehler", {0x00, 0x00, 0x00}, {0xFF, 0xFF, 0xFF}},
    {"lunaperche", {0x00, 0x00, 0x00}, {0xC6, 0xC6, 0xC6}},
    {"morning", {0xE4, 0xE4, 0xE4}, {0x00, 0x00, 0x00}},
    {"murphy", {0x00, 0x00, 0x00}, {0x87, 0xFF, 0x87}},
    {"novum", {0x14, 0x16, 0x1B}, {0xE0, 0xE2, 0xEA}},
    {"pablo", {0x00, 0x00, 0x00}, {0xFF, 0xFF, 0xFF}},
    {"peachpuff", {0xFF, 0xDA, 0xB9}, {0x00, 0x00, 0x00}},
    {"quiet", {0x00, 0x00, 0x00}, {0xDA, 0xDA, 0xDA}},
    {"retrobox", {0x1C, 0x1C, 0x1C}, {0xEB, 0xDB, 0xB2}},
    {"ron", {0x00, 0x00, 0x00}, {0x00, 0xFF, 0xFF}},
    {"shine", {0xFF, 0xFF, 0xFF}, {0x00, 0x00, 0x00}},
    {"slate", {0x26, 0x26, 0x26}, {0xFF, 0xFF, 0xFF}},
    {"sorbet", {0x16, 0x18, 0x21}, {0xDA, 0xDA, 0xDA}},
    {"torte", {0x00, 0x00, 0x00}, {0xCC, 0xCC, 0xCC}},
    {"unokai", {0x28, 0x29, 0x23}, {0xF8, 0xF8, 0xF2}},
    {"wildcharm", {0x00, 0x00, 0x00}, {0xD0, 0xD0, 0xD0}},
    {"zaibatsu", {0x0E, 0x00, 0x24}, {0xFF, 0xFF, 0xFF}},
    {"zellner", {0xFF, 0xFF, 0xFF}, {0x00, 0x00, 0x00}},
    {"abyss", {0x00, 0x0C, 0x18}, {0x66, 0x88, 0xCC}},
    {"vscode-dark-2026", {0x12, 0x13, 0x14}, {0xBB, 0xBE, 0xBF}},
    {"vscode-light-2026", {0xFF, 0xFF, 0xFF}, {0x20, 0x20, 0x20}},
    {"vscode-dark-modern", {0x1F, 0x1F, 0x1F}, {0xCC, 0xCC, 0xCC}},
    {"vscode-dark-plus", {0x1E, 0x1E, 0x1E}, {0xD4, 0xD4, 0xD4}},
    {"vscode-dark", {0x1E, 0x1E, 0x1E}, {0xD4, 0xD4, 0xD4}},
    {"vscode-high-contrast", {0x00, 0x00, 0x00}, {0xFF, 0xFF, 0xFF}},
    {"vscode-high-contrast-light", {0x1E, 0x1E, 0x1E}, {0xD4, 0xD4, 0xD4}},
    {"vscode-light-modern", {0xFF, 0xFF, 0xFF}, {0x3B, 0x3B, 0x3B}},
    {"vscode-light-plus", {0xFF, 0xFF, 0xFF}, {0x00, 0x00, 0x00}},
    {"vscode-light", {0xFF, 0xFF, 0xFF}, {0x00, 0x00, 0x00}},
    {"kimbie-dark", {0x22, 0x1A, 0x0F}, {0xD3, 0xAF, 0x86}},
    {"monokai-dimmed", {0x1E, 0x1E, 0x1E}, {0xC5, 0xC8, 0xC6}},
    {"quiet-light", {0xF5, 0xF5, 0xF5}, {0x33, 0x33, 0x33}},
    {"red", {0x39, 0x00, 0x00}, {0xF8, 0xF8, 0xF8}},
    {"tomorrow-night-blue", {0x00, 0x24, 0x51}, {0xFF, 0xFF, 0xFF}},
    {"monalisa", {0x12, 0x0B, 0x0D}, {0xF6, 0xD6, 0x66}},
    {"catppuccin-latte", {0xEF, 0xF1, 0xF5}, {0x4C, 0x4F, 0x69}},
    {"catppuccin-frappe", {0x30, 0x34, 0x46}, {0xC6, 0xD0, 0xF5}},
    {"catppuccin-macchiato", {0x24, 0x27, 0x3A}, {0xCA, 0xD3, 0xF5}},
    {"nord", {0x2E, 0x34, 0x40}, {0xD8, 0xDE, 0xE9}},
    {"gruvbox", {0x28, 0x28, 0x28}, {0xEB, 0xDB, 0xB2}},
    {"gruvbox-light", {0xFB, 0xF1, 0xC7}, {0x3C, 0x38, 0x36}},
    {"tokyo-night", {0x1A, 0x1B, 0x26}, {0xC0, 0xCA, 0xF5}},
    {"tokyo-night-storm", {0x24, 0x28, 0x3B}, {0xC0, 0xCA, 0xF5}},
    {"tokyo-night-moon", {0x22, 0x24, 0x36}, {0xC8, 0xD3, 0xF5}},
    {"tokyo-night-light", {0xE6, 0xE7, 0xED}, {0x34, 0x3B, 0x59}},
    {"github-dark", {0x0D, 0x11, 0x17}, {0xC9, 0xD1, 0xD9}},
    {"github-light", {0xFF, 0xFF, 0xFF}, {0x24, 0x29, 0x2F}},
    {"github-dark-dimmed", {0x22, 0x27, 0x2E}, {0xAD, 0xBA, 0xC7}},
    {"one-light", {0xFA, 0xFA, 0xFA}, {0x38, 0x3A, 0x42}},
    {"intellij-light", {0xFF, 0xFF, 0xFF}, {0x00, 0x00, 0x00}},
    {"darcula", {0x2B, 0x2B, 0x2B}, {0xA9, 0xB7, 0xC6}},
    {"rose-pine", {0x19, 0x17, 0x24}, {0xE0, 0xDE, 0xF4}},
    {"rose-pine-moon", {0x23, 0x21, 0x36}, {0xE0, 0xDE, 0xF4}},
    {"rose-pine-dawn", {0xFA, 0xF4, 0xED}, {0x46, 0x42, 0x61}},
    {"everforest-dark", {0x2D, 0x35, 0x3B}, {0xD3, 0xC6, 0xAA}},
    {"everforest-light", {0xFD, 0xF6, 0xE3}, {0x5C, 0x6A, 0x72}},
    {"kanagawa", {0x1F, 0x1F, 0x28}, {0xDC, 0xD7, 0xBA}},
    {"kanagawa-dragon", {0x18, 0x16, 0x16}, {0xC5, 0xC9, 0xC5}},
    {"kanagawa-lotus", {0xF2, 0xEC, 0xBC}, {0x54, 0x54, 0x64}},
    {"material-oceanic", {0x26, 0x32, 0x38}, {0xB0, 0xBE, 0xC5}},
    {"material-darker", {0x21, 0x21, 0x21}, {0xB0, 0xBE, 0xC5}},
    {"material-lighter", {0xFA, 0xFA, 0xFA}, {0x54, 0x6E, 0x7A}},
    {"material-palenight", {0x29, 0x2D, 0x3E}, {0xA6, 0xAC, 0xCD}},
    {"material-deep-ocean", {0x0F, 0x11, 0x1A}, {0x8F, 0x93, 0xA2}},
    {"ayu-dark", {0x10, 0x14, 0x1C}, {0xBF, 0xBD, 0xB6}},
    {"ayu-mirage", {0x24, 0x29, 0x36}, {0xCC, 0xCA, 0xC2}},
    {"ayu-light", {0xFC, 0xFC, 0xFC}, {0x5C, 0x61, 0x66}},
    {"cobalt2", {0x19, 0x35, 0x49}, {0xFF, 0xFF, 0xFF}},
    {"synthwave-84", {0x26, 0x23, 0x35}, {0xD4, 0xD4, 0xD4}},
    {"alloy-core", {0x19, 0x15, 0x1B}, {0xF3, 0xE9, 0xDC}},
    {"verdant-rage", {0x14, 0x20, 0x1B}, {0xE4, 0xEC, 0xD9}},
    {"scarlet-web", {0x12, 0x1C, 0x31}, {0xE8, 0xEC, 0xF3}},
    {"relic-sand", {0xF1, 0xE4, 0xC6}, {0x3A, 0x2C, 0x20}},
    {"soap-rebellion", {0x1B, 0x25, 0x24}, {0xE1, 0xE8, 0xD6}},
    {"code-rain", {0x08, 0x13, 0x0E}, {0xC9, 0xE9, 0xCD}},
    {"arcane-library", {0x18, 0x1B, 0x30}, {0xEA, 0xE2, 0xD1}},
    {"royal-kinetic", {0x11, 0x0E, 0x1C}, {0xE6, 0xDF, 0xF3}},
    {"arena-sunset", {0x19, 0x2C, 0x29}, {0xEC, 0xE8, 0xCF}},
    {"cosmic-mixtape", {0x25, 0x16, 0x30}, {0xEE, 0xE0, 0xF5}},
    {"prism-sanctum", {0x10, 0x28, 0x25}, {0xE9, 0xE9, 0xD7}},
    {"winter-sentinel", {0x14, 0x24, 0x31}, {0xDD, 0xE8, 0xEF}},
    {"infinite-dusk", {0x2C, 0x1D, 0x32}, {0xF0, 0xE2, 0xE8}},
    {"stellar-pilot", {0x14, 0x1E, 0x35}, {0xED, 0xE7, 0xD9}},
    {"quantum-honey", {0x28, 0x22, 0x18}, {0xEE, 0xE6, 0xD3}},
    {"crimson-mercenary", {0x27, 0x16, 0x1B}, {0xED, 0xE1, 0xDC}},
    {"last-outlaw", {0x2A, 0x22, 0x1C}, {0xE8, 0xDD, 0xC9}},
    {"ivory-agent", {0xF2, 0xEF, 0xE8}, {0x31, 0x36, 0x3B}},
    {"celestial-brass", {0xEE, 0xE8, 0xD8}, {0x34, 0x36, 0x31}},
    {"amber-haze", {0x30, 0x1C, 0x18}, {0xF1, 0xE0, 0xCF}},
    {"desert-oracle", {0xEB, 0xE0, 0xC9}, {0x46, 0x3C, 0x31}},
    {"chrome-wasteland", {0x25, 0x32, 0x39}, {0xE8, 0xE0, 0xC9}},
    {"neon-grid", {0x06, 0x1D, 0x27}, {0xD9, 0xF0, 0xF2}},
    {"biolume-forest", {0x0C, 0x20, 0x38}, {0xD5, 0xE8, 0xF1}},
    {"pastel-concierge", {0xF6, 0xE7, 0xE6}, {0x54, 0x3D, 0x4C}},
    {"twilight-jazz", {0x1C, 0x21, 0x46}, {0xE9, 0xE4, 0xF2}},
    {"noir-vigil", {0x1D, 0x18, 0x1A}, {0xE5, 0xDC, 0xDD}},
    {"carnival-ashes", {0x20, 0x33, 0x2A}, {0xE3, 0xE5, 0xCF}},
    {"orbital-frost", {0xEB, 0xF0, 0xF3}, {0x35, 0x43, 0x4E}},
    {"folded-city", {0x20, 0x2F, 0x3A}, {0xDE, 0xE8, 0xEB}},
    {"midnight-drive", {0x12, 0x34, 0x3C}, {0xE7, 0xE6, 0xD3}},
    {"editorial-cafe", {0xE5, 0xED, 0xE7}, {0x36, 0x4A, 0x48}},
    {"striped-haunt", {0xEF, 0xED, 0xEA}, {0x33, 0x2B, 0x40}},
    {"curious-garden", {0xE7, 0xEE, 0xDD}, {0x3A, 0x48, 0x40}},
    {"tidebound-gold", {0x29, 0x2B, 0x23}, {0xE9, 0xDF, 0xC7}},
    {"velvet-cabaret", {0x35, 0x18, 0x20}, {0xF1, 0xDE, 0xD5}},
    {"lagoon-whisper", {0x10, 0x34, 0x37}, {0xDB, 0xE9, 0xDF}},
};

static double linear_channel(unsigned char value)
{
    double channel = (double)value / 255.0;
    return channel <= 0.04045 ? channel / 12.92 : pow((channel + 0.055) / 1.055, 2.4);
}

static double luminance(struct rgb_color color)
{
    return 0.2126 * linear_channel(color.red) + 0.7152 * linear_channel(color.green) +
           0.0722 * linear_channel(color.blue);
}

static void readable(struct rgb_color foreground, struct rgb_color background)
{
    double first = luminance(foreground);
    double second = luminance(background);
    double high = first > second ? first : second;
    double low = first > second ? second : first;
    require((high + 0.05) / (low + 0.05) >= 4.5,
            "readable theme roles contrast at least 4.5:1 against their actual background");
}

static bool same_color(struct rgb_color first, struct rgb_color second)
{
    return first.red == second.red && first.green == second.green && first.blue == second.blue;
}

static void verify_nib_import(void)
{
    require(folio_theme_count() == 127 && folio_theme_choice_count() == 128,
            "125 Nib themes coexist with both original Folio themes and the system policy");
    for (size_t index = 0; index < sizeof nib_expected / sizeof nib_expected[0]; ++index)
    {
        enum folio_theme_choice choice = FOLIO_THEME_CHOICE_SYSTEM;
        const char *_Nonnull name = nib_expected[index].name;
        require(folio_theme_choice_parse(name, strlen(name), &choice) == FOLIO_THEME_LOOKUP_FOUND,
                "each fixed Nib identifier is accepted by the canonical name boundary");
        enum folio_theme theme = folio_theme_choice_resolve(choice, FOLIO_THEME_LIGHT);
        require(theme == folio_theme_choice_resolve(choice, FOLIO_THEME_DARK),
                "an explicit Nib palette is independent of the OS appearance");
        const struct theme_palette *_Nonnull palette = folio_theme_palette(theme);
        require(same_color(palette->pane, nib_expected[index].background) &&
                    same_color(palette->editor_text, nib_expected[index].foreground) &&
                    same_color(palette->rtf.text, nib_expected[index].foreground),
                "Nib's fixed body colors survive the import in edit and Markdown view modes");
    }
}

static void verify_catalog_names(void)
{
    for (size_t index = 0; index < folio_theme_count(); ++index)
    {
        enum folio_theme theme = folio_theme_at(index);
        const char *_Nonnull name = folio_theme_name(theme);
        enum folio_theme_choice choice = FOLIO_THEME_CHOICE_SYSTEM;
        require(folio_theme_choice_parse(name, strlen(name), &choice) == FOLIO_THEME_LOOKUP_FOUND &&
                    folio_theme_choice_resolve(choice, FOLIO_THEME_LIGHT) == theme,
                "each persisted name uniquely identifies its own palette");
    }
    enum folio_theme_choice untouched = FOLIO_THEME_CHOICE_DRACULA;
    require(folio_theme_choice_parse("Dracula", 7, &untouched) == FOLIO_THEME_LOOKUP_UNKNOWN &&
                untouched == FOLIO_THEME_CHOICE_DRACULA,
            "identifiers are exact and a rejected name leaves the result unchanged");
    require(folio_theme_choice_parse("", 0, &untouched) == FOLIO_THEME_LOOKUP_UNKNOWN,
            "an empty theme name is rejected");
    require(folio_theme_choice_parse("dracula\0tail", 12, &untouched) == FOLIO_THEME_LOOKUP_UNKNOWN,
            "an embedded NUL cannot hide an identifier suffix");
}

static void verify_readable_roles(void)
{
    for (size_t index = 0; index < folio_theme_count(); ++index)
    {
        const struct theme_palette *_Nonnull palette = folio_theme_palette(folio_theme_at(index));
        readable(palette->editor_text, palette->pane);
        readable(palette->category_text, palette->window);
        readable(palette->note_text, palette->window);
        readable(palette->current_text, palette->window);
        readable(palette->current_text, palette->pane);
        readable(palette->header_text, palette->window);
        readable(palette->header_text, palette->pane);
        readable(palette->selected_text, palette->selected_background);
        readable(palette->chip_text, palette->chip_background);
        readable(palette->breadcrumb_text, palette->breadcrumb_background);
        readable(palette->gutter_text, palette->pane);
        readable(palette->current_text, palette->dialog_surface);
        readable(palette->current_text, palette->dialog_control);
        readable(palette->dialog_secondary, palette->dialog_surface);
        readable(palette->rtf.text, palette->pane);
        readable(palette->rtf.heading, palette->pane);
        readable(palette->rtf.muted, palette->pane);
        readable(palette->rtf.link, palette->pane);
        readable(palette->rtf.code_text, palette->rtf.code_background);
    }
}

static double ratio_of(struct rgb_color first, struct rgb_color second)
{
    double left = luminance(first);
    double right = luminance(second);
    double high = left > right ? left : right;
    double low = left > right ? right : left;
    return (high + 0.05) / (low + 0.05);
}

/* 線の役割と、その線が接する地（ADR0060の決定3）。正典の表とは独立に書き下す。 */
static void line_meets(const struct theme_palette *_Nonnull palette, enum theme_line line,
                       const struct rgb_color *_Nonnull grounds, size_t count)
{
    struct rgb_color ink = theme_contrast_line(palette, line);
    for (size_t index = 0; index < count; ++index)
    {
        require(ratio_of(ink, grounds[index]) >= 3.0,
                "state, boundary and focus lines contrast at least 3:1 with every adjacent color");
    }
}

static void verify_line_roles(void)
{
    for (size_t index = 0; index < folio_theme_count(); ++index)
    {
        const struct theme_palette *_Nonnull palette = folio_theme_palette(folio_theme_at(index));
        const struct rgb_color parts[] = {palette->window, palette->pane};
        const struct rgb_color outside[] = {palette->window, palette->pane,
                                            palette->dialog_surface};
        const struct rgb_color fields[] = {palette->window, palette->pane, palette->dialog_surface,
                                           palette->dialog_control};
        line_meets(palette, THEME_LINE_HOVER, parts, 2);
        line_meets(palette, THEME_LINE_FOCUS, outside, 3);
        line_meets(palette, THEME_LINE_FOCUS_ITEM, parts, 2);
        line_meets(palette, THEME_LINE_FOCUS_SELECTED, &palette->selected_background, 1);
        line_meets(palette, THEME_LINE_FOCUS_PRIMARY, &palette->chip_background, 1);
        line_meets(palette, THEME_LINE_FIELD, fields, 4);
        line_meets(palette, THEME_LINE_FIELD_FOCUS, fields, 4);
    }
}

static void verify_line_ratio_table(void)
{
    const struct rgb_color black = {0, 0, 0};
    for (unsigned value = 0; value < 256; ++value)
    {
        struct rgb_color gray = {(unsigned char)value, (unsigned char)value, (unsigned char)value};
        double expected = ratio_of(gray, black);
        double actual = theme_contrast_ratio(black, gray);
        require(fabs(actual - expected) < 1e-9,
                "the folded sRGB table matches the WCAG luminance formula for every channel value");
    }
}

/* 既定の色が届かないテーマでは決まった順の別の文字系トークンへ切り替わる。 */
static void verify_line_fallback(void)
{
    const struct rgb_color white = {0xFF, 0xFF, 0xFF};
    const struct rgb_color black = {0, 0, 0};
    const struct rgb_color pale = {0xC0, 0xC0, 0xC0};
    struct theme_palette palette = {.window = white,
                                    .pane = white,
                                    .dialog_surface = white,
                                    .dialog_control = white,
                                    .header_text = black,
                                    .current_text = white,
                                    .category_text = white,
                                    .editor_text = white,
                                    .selected_text = white,
                                    .note_text = white,
                                    .selected_background = white,
                                    .chip_background = white,
                                    .chip_text = white};
    require(rgb_color_equal(theme_contrast_line(&palette, THEME_LINE_FIELD), black),
            "a role keeps its own token when that token already contrasts enough");
    palette.header_text = white;
    palette.editor_text = black;
    require(rgb_color_equal(theme_contrast_line(&palette, THEME_LINE_FIELD_FOCUS), black),
            "a failing role switches to the first text token that reaches 3:1");
    palette.editor_text = white;
    palette.note_text = pale;
    require(rgb_color_equal(theme_contrast_line(&palette, THEME_LINE_FOCUS_SELECTED), pale),
            "when no text token reaches 3:1 the line takes the strongest available contrast");
}

static void verify_filter_and_ex(void)
{
    size_t tokyo = 0;
    for (size_t index = 0; index < folio_theme_choice_count(); ++index)
    {
        enum folio_theme_choice choice = folio_theme_choice_at(index);
        require(folio_theme_choice_matches(choice, "", 0, FOLIO_LANGUAGE_EN),
                "clearing the filter restores every choice");
        tokyo += folio_theme_choice_matches(choice, "TOKYO", 5, FOLIO_LANGUAGE_EN) ? 1 : 0;
        const char *_Nonnull name = folio_theme_choice_name(choice);
        char command[64] = "theme=";
        memcpy(command + 6, name, strlen(name) + 1);
        enum folio_theme_choice parsed = FOLIO_THEME_CHOICE_SYSTEM;
        require(folio_command_parse_theme(command, strlen(command), &parsed) && parsed == choice,
                "Ex resolves every selectable identifier through the same catalog");
    }
    require(tokyo == 4, "the case-insensitive filter finds all four Tokyo Night variants");
    require(!folio_theme_choice_matches(FOLIO_THEME_CHOICE_DRACULA, "no-such-theme", 13,
                                        FOLIO_LANGUAGE_EN),
            "an unmatched query has no candidate");
    require(folio_theme_choice_matches(FOLIO_THEME_CHOICE_SYSTEM, "従う", 6, FOLIO_LANGUAGE_JA),
            "the localized system label can also be found");
    enum folio_theme_choice unchanged = FOLIO_THEME_CHOICE_DARK;
    require(!folio_command_parse_theme("theme=dracula number", 20, &unchanged) &&
                !folio_command_parse_theme("theme=Dracula", 13, &unchanged) &&
                !folio_command_parse_theme("theme=", 6, &unchanged) &&
                unchanged == FOLIO_THEME_CHOICE_DARK,
            "an invalid Ex choice cannot apply a partial or default theme");
}

void run_theme_tests(void)
{
    verify_nib_import();
    verify_catalog_names();
    verify_readable_roles();
    verify_line_ratio_table();
    verify_line_roles();
    verify_line_fallback();
    verify_filter_and_ex();
}
