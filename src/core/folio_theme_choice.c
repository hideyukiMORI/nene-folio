#include "folio_theme_choice.h"

#include "ascii_fold.h"
#include "ui_text.h"

#include <string.h>

size_t folio_theme_choice_count(void)
{
    return folio_theme_count() + 1;
}

enum folio_theme_choice folio_theme_choice_at(size_t index)
{
    return (enum folio_theme_choice)index;
}

enum folio_theme folio_theme_choice_resolve(enum folio_theme_choice choice,
                                            enum folio_theme os_theme)
{
    if (choice == FOLIO_THEME_CHOICE_SYSTEM)
    {
        return os_theme;
    }
    /* Each fixed selection is defined as its palette ID + 1 in the enum itself. */
    return (enum folio_theme)(choice - 1);
}

const char *_Nonnull folio_theme_choice_name(enum folio_theme_choice choice)
{
    if (choice == FOLIO_THEME_CHOICE_SYSTEM)
    {
        return "system";
    }
    return folio_theme_name(folio_theme_choice_resolve(choice, FOLIO_THEME_LIGHT));
}

const char *_Nonnull folio_theme_choice_label(enum folio_theme_choice choice,
                                              enum folio_language language)
{
    if (choice == FOLIO_THEME_CHOICE_SYSTEM)
    {
        return ui_text_line(UI_TEXT_SETTINGS_THEME_SYSTEM, language);
    }
    if (choice == FOLIO_THEME_CHOICE_LIGHT)
    {
        return ui_text_line(UI_TEXT_SETTINGS_THEME_LIGHT, language);
    }
    if (choice == FOLIO_THEME_CHOICE_DARK)
    {
        return ui_text_line(UI_TEXT_SETTINGS_THEME_DARK, language);
    }
    return folio_theme_choice_name(choice);
}

enum folio_theme_lookup_outcome folio_theme_choice_parse(const char *_Nonnull text, size_t length,
                                                         enum folio_theme_choice *_Nonnull out)
{
    for (size_t index = 0; index < folio_theme_choice_count(); ++index)
    {
        enum folio_theme_choice choice = folio_theme_choice_at(index);
        const char *_Nonnull name = folio_theme_choice_name(choice);
        if (strlen(name) == length && memcmp(name, text, length) == 0)
        {
            *out = choice;
            return FOLIO_THEME_LOOKUP_FOUND;
        }
    }
    return FOLIO_THEME_LOOKUP_UNKNOWN;
}

static bool same_folded_span(const char *_Nonnull candidate, const char *_Nonnull text,
                             size_t length)
{
    for (size_t index = 0; index < length; ++index)
    {
        if (ascii_fold_byte(candidate[index]) != ascii_fold_byte(text[index]))
        {
            return false;
        }
    }
    return true;
}

static bool contains_query(const char *_Nonnull candidate, const char *_Nonnull text, size_t length)
{
    size_t available = strlen(candidate);
    if (length > available)
    {
        return false;
    }
    for (size_t at = 0; at <= available - length; ++at)
    {
        if (same_folded_span(candidate + at, text, length))
        {
            return true;
        }
    }
    return false;
}

bool folio_theme_choice_matches(enum folio_theme_choice choice, const char *_Nonnull text,
                                size_t length, enum folio_language language)
{
    return contains_query(folio_theme_choice_name(choice), text, length) ||
           contains_query(folio_theme_choice_label(choice, language), text, length);
}
