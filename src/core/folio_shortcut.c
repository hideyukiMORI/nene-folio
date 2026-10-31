#include "folio_shortcut.h"
#include "ui_text.h"
#include "ui_text_request.h"
struct folio_shortcut
{
    enum folio_shortcut_action action;
    enum folio_command command;
    enum folio_shortcut_key key;
    enum folio_shortcut_modifiers modifiers;
    enum folio_shortcut_route route;
    enum ui_text label;
};
static const struct folio_shortcut bindings[] = {
    {FOLIO_SHORTCUT_ACTION_CONTEXT_MENU, FOLIO_COMMAND_HELP, FOLIO_SHORTCUT_KEY_F10,
     FOLIO_SHORTCUT_MOD_SHIFT, FOLIO_SHORTCUT_HACCEL, UI_TEXT_KEY_CONTEXT_MENU},
    {FOLIO_SHORTCUT_ACTION_CONTEXT_MENU, FOLIO_COMMAND_HELP, FOLIO_SHORTCUT_KEY_APPS,
     FOLIO_SHORTCUT_MOD_NONE, FOLIO_SHORTCUT_HACCEL, UI_TEXT_KEY_APPS},
    {FOLIO_SHORTCUT_ACTION_COMMAND, FOLIO_COMMAND_SAVE, FOLIO_SHORTCUT_KEY_S,
     FOLIO_SHORTCUT_MOD_CONTROL, FOLIO_SHORTCUT_CHARACTER, UI_TEXT_KEY_SAVE},
    {FOLIO_SHORTCUT_ACTION_COMMAND, FOLIO_COMMAND_NEW, FOLIO_SHORTCUT_KEY_N,
     FOLIO_SHORTCUT_MOD_CONTROL, FOLIO_SHORTCUT_CHARACTER, UI_TEXT_KEY_NEW},
    {FOLIO_SHORTCUT_ACTION_COMMAND, FOLIO_COMMAND_HELP, FOLIO_SHORTCUT_KEY_F1,
     FOLIO_SHORTCUT_MOD_NONE, FOLIO_SHORTCUT_KEYDOWN, UI_TEXT_KEY_HELP},
    {FOLIO_SHORTCUT_ACTION_COMMAND, FOLIO_COMMAND_HELP, FOLIO_SHORTCUT_KEY_P,
     FOLIO_SHORTCUT_MOD_CONTROL, FOLIO_SHORTCUT_CHARACTER, UI_TEXT_KEY_PALETTE},
    {FOLIO_SHORTCUT_ACTION_COMMAND, FOLIO_COMMAND_FIND, FOLIO_SHORTCUT_KEY_F,
     FOLIO_SHORTCUT_MOD_CONTROL, FOLIO_SHORTCUT_HACCEL, UI_TEXT_KEY_FIND},
    {FOLIO_SHORTCUT_ACTION_TOGGLE_MODE, FOLIO_COMMAND_EDIT, FOLIO_SHORTCUT_KEY_E,
     FOLIO_SHORTCUT_MOD_CONTROL, FOLIO_SHORTCUT_HACCEL, UI_TEXT_KEY_TOGGLE_MODE},
    {FOLIO_SHORTCUT_ACTION_OPERATIONS, FOLIO_COMMAND_HELP, FOLIO_SHORTCUT_KEY_M,
     FOLIO_SHORTCUT_MOD_CONTROL, FOLIO_SHORTCUT_HACCEL, UI_TEXT_KEY_OPERATIONS},
    {FOLIO_SHORTCUT_ACTION_COMMAND, FOLIO_COMMAND_SAVE_AS, FOLIO_SHORTCUT_KEY_S,
     FOLIO_SHORTCUT_MOD_CONTROL_SHIFT, FOLIO_SHORTCUT_HACCEL, UI_TEXT_KEY_SAVE_AS},
    {FOLIO_SHORTCUT_ACTION_COMMAND, FOLIO_COMMAND_RENAME, FOLIO_SHORTCUT_KEY_F2,
     FOLIO_SHORTCUT_MOD_NONE, FOLIO_SHORTCUT_HACCEL, UI_TEXT_KEY_RENAME},
    {FOLIO_SHORTCUT_ACTION_COMMAND, FOLIO_COMMAND_QUIT, FOLIO_SHORTCUT_KEY_F4,
     FOLIO_SHORTCUT_MOD_ALT, FOLIO_SHORTCUT_OS, UI_TEXT_KEY_CLOSE},
    {FOLIO_SHORTCUT_ACTION_COMMAND, FOLIO_COMMAND_LAYOUT_DRAWER, FOLIO_SHORTCUT_KEY_ONE,
     FOLIO_SHORTCUT_MOD_CONTROL_SHIFT, FOLIO_SHORTCUT_HACCEL, UI_TEXT_LAYOUT_KEY_DRAWER},
    {FOLIO_SHORTCUT_ACTION_COMMAND, FOLIO_COMMAND_LAYOUT_BOTH, FOLIO_SHORTCUT_KEY_TWO,
     FOLIO_SHORTCUT_MOD_CONTROL_SHIFT, FOLIO_SHORTCUT_HACCEL, UI_TEXT_LAYOUT_KEY_BOTH},
    {FOLIO_SHORTCUT_ACTION_COMMAND, FOLIO_COMMAND_LAYOUT_NOTE, FOLIO_SHORTCUT_KEY_THREE,
     FOLIO_SHORTCUT_MOD_CONTROL_SHIFT, FOLIO_SHORTCUT_HACCEL, UI_TEXT_LAYOUT_KEY_NOTE},
    {FOLIO_SHORTCUT_ACTION_FILTER, FOLIO_COMMAND_FIND, FOLIO_SHORTCUT_KEY_F,
     FOLIO_SHORTCUT_MOD_CONTROL_SHIFT, FOLIO_SHORTCUT_HACCEL, UI_TEXT_KEY_FILTER},
    {FOLIO_SHORTCUT_ACTION_SEARCH_FORWARD, FOLIO_COMMAND_FIND, FOLIO_SHORTCUT_KEY_F3,
     FOLIO_SHORTCUT_MOD_NONE, FOLIO_SHORTCUT_HACCEL, UI_TEXT_KEY_SEARCH_FORWARD},
    {FOLIO_SHORTCUT_ACTION_SEARCH_BACKWARD, FOLIO_COMMAND_FIND, FOLIO_SHORTCUT_KEY_F3,
     FOLIO_SHORTCUT_MOD_SHIFT, FOLIO_SHORTCUT_HACCEL, UI_TEXT_KEY_SEARCH_BACKWARD},
    {FOLIO_SHORTCUT_ACTION_SEARCH_NEXT, FOLIO_COMMAND_FIND, FOLIO_SHORTCUT_KEY_ENTER,
     FOLIO_SHORTCUT_MOD_NONE, FOLIO_SHORTCUT_SEARCH_HACCEL, UI_TEXT_KEY_SEARCH_NEXT},
    {FOLIO_SHORTCUT_ACTION_SEARCH_PREVIOUS, FOLIO_COMMAND_FIND, FOLIO_SHORTCUT_KEY_ENTER,
     FOLIO_SHORTCUT_MOD_SHIFT, FOLIO_SHORTCUT_SEARCH_HACCEL, UI_TEXT_KEY_SEARCH_PREVIOUS}};
size_t folio_shortcut_count(void)
{
    return sizeof bindings / sizeof bindings[0];
}
const struct folio_shortcut *_Nonnull folio_shortcut_at(size_t index)
{
    return &bindings[index];
}
enum folio_shortcut_route folio_shortcut_route(const struct folio_shortcut *_Nonnull shortcut)
{
    return shortcut->route;
}
enum folio_shortcut_key folio_shortcut_key(const struct folio_shortcut *_Nonnull shortcut)
{
    return shortcut->key;
}
enum folio_shortcut_modifiers
folio_shortcut_modifiers(const struct folio_shortcut *_Nonnull shortcut)
{
    return shortcut->modifiers;
}
enum folio_shortcut_action folio_shortcut_action(const struct folio_shortcut *_Nonnull shortcut)
{
    return shortcut->action;
}
enum folio_command folio_shortcut_command(const struct folio_shortcut *_Nonnull shortcut)
{
    return shortcut->command;
}
const char *_Nonnull folio_shortcut_label(const struct folio_shortcut *_Nonnull shortcut,
                                          enum folio_language language)
{
    return ui_text_line(shortcut->label, language);
}
const char *_Nonnull folio_shortcut_command_hint(enum folio_command command,
                                                 enum folio_language language)
{
    for (size_t at = 0; at < folio_shortcut_count(); ++at)
    {
        if (bindings[at].action == FOLIO_SHORTCUT_ACTION_COMMAND && bindings[at].command == command)
            return folio_shortcut_label(&bindings[at], language);
    }
    return folio_command_alias_count(command) > 0 ? folio_command_alias(command, 0) : "";
}

const char *_Nonnull folio_shortcut_action_hint(enum folio_shortcut_action action,
                                                enum folio_language language)
{
    for (size_t at = 0; at < folio_shortcut_count(); ++at)
        if (bindings[at].action == action)
            return folio_shortcut_label(&bindings[at], language);
    return "";
}

static struct ui_text_request shortcut_help_request(enum ui_text id, enum folio_language language)
{
    struct ui_text_request request = {.id = id, .language = language};
    if (id == UI_TEXT_HELP_GLOBAL_KEYS)
    {
        request.name = folio_shortcut_command_hint(FOLIO_COMMAND_HELP, language);
        request.from = ui_text_line(UI_TEXT_KEY_PALETTE, language);
        request.to = folio_shortcut_command_hint(FOLIO_COMMAND_FIND, language);
    }
    if (id == UI_TEXT_HELP_GLOBAL_FILTER)
        request.name = folio_shortcut_action_hint(FOLIO_SHORTCUT_ACTION_FILTER, language);
    if (id == UI_TEXT_HELP_NOTE_ACTIONS)
    {
        request.name = folio_shortcut_action_hint(FOLIO_SHORTCUT_ACTION_TOGGLE_MODE, language);
        request.from = folio_shortcut_action_hint(FOLIO_SHORTCUT_ACTION_OPERATIONS, language);
    }
    if (id == UI_TEXT_HELP_FILE_KEYS)
    {
        request.name = folio_shortcut_command_hint(FOLIO_COMMAND_NEW, language);
        request.from = folio_shortcut_command_hint(FOLIO_COMMAND_SAVE, language);
        request.to = folio_shortcut_command_hint(FOLIO_COMMAND_SAVE_AS, language);
    }
    if (id == UI_TEXT_HELP_SEARCH_STEP || id == UI_TEXT_HELP_SEARCH_FIELD)
    {
        request.name = folio_shortcut_action_hint(id == UI_TEXT_HELP_SEARCH_STEP
                                                      ? FOLIO_SHORTCUT_ACTION_SEARCH_FORWARD
                                                      : FOLIO_SHORTCUT_ACTION_SEARCH_NEXT,
                                                  language);
        request.from = folio_shortcut_action_hint(id == UI_TEXT_HELP_SEARCH_STEP
                                                      ? FOLIO_SHORTCUT_ACTION_SEARCH_BACKWARD
                                                      : FOLIO_SHORTCUT_ACTION_SEARCH_PREVIOUS,
                                                  language);
    }
    return request;
}

enum ui_text_format_outcome folio_shortcut_help(enum ui_text id, enum folio_language language,
                                                char *_Nonnull out, size_t capacity)
{
    struct ui_text_request request = shortcut_help_request(id, language);
    if (id != UI_TEXT_HELP_FILE_KEYS)
        return ui_text_format(&request, out, capacity);
    char prefix[1024];
    if (ui_text_format(&request, prefix, sizeof prefix) != UI_TEXT_FORMAT_READY)
        return UI_TEXT_FORMAT_TOO_LONG;
    struct ui_text_request joined = {
        .id = UI_TEXT_HELP_RENAME_KEY,
        .language = language,
        .name = prefix,
        .from = folio_shortcut_command_hint(FOLIO_COMMAND_RENAME, language)};
    return ui_text_format(&joined, out, capacity);
}
