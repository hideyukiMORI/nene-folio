#include "folio_menu_action.h"
#include "folio_shortcut.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include <string.h>
struct folio_menu_action
{
    enum folio_menu_action_kind kind;
    enum folio_command command;
    enum folio_menu_group group;
    char key;
};
static const struct folio_menu_action operations[] = {
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_SAVE_AS, FOLIO_MENU_GROUP_FIRST, 'A'},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_RENAME, FOLIO_MENU_GROUP_FIRST, 'R'},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_VIEW, FOLIO_MENU_GROUP_FIRST, 'V'},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_REPLACE, FOLIO_MENU_GROUP_SECOND, 'P'},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_HISTORY, FOLIO_MENU_GROUP_SECOND, 'H'},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_DISCARD_EDITS, FOLIO_MENU_GROUP_DESTRUCTIVE, 'E'},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_TRASH_NOTE, FOLIO_MENU_GROUP_DESTRUCTIVE, 'T'}};
static const struct folio_menu_action category[] = {
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_NEW, FOLIO_MENU_GROUP_FIRST, 'N'},
    {FOLIO_MENU_ACTION_RECOLOR_CATEGORY, FOLIO_COMMAND_NEW, FOLIO_MENU_GROUP_SECOND, 'C'},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_RENAME_CATEGORY, FOLIO_MENU_GROUP_SECOND, 'R'},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_NEW_CATEGORY, FOLIO_MENU_GROUP_THIRD, 'A'},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_DELETE_CATEGORY, FOLIO_MENU_GROUP_DESTRUCTIVE, 'D'}};
static const struct folio_menu_action note[] = {
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_RENAME, FOLIO_MENU_GROUP_FIRST, 'R'},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_TRASH_NOTE, FOLIO_MENU_GROUP_DESTRUCTIVE, 'T'}};
static const struct folio_menu_action layout[] = {
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_LAYOUT_DRAWER, FOLIO_MENU_GROUP_FIRST, '1'},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_LAYOUT_BOTH, FOLIO_MENU_GROUP_FIRST, '2'},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_LAYOUT_NOTE, FOLIO_MENU_GROUP_FIRST, '3'}};

size_t folio_menu_action_count(enum folio_menu_context context)
{
    switch (context)
    {
    case FOLIO_MENU_OPERATIONS:
        return sizeof operations / sizeof operations[0];
    case FOLIO_MENU_LAYOUT:
        return sizeof layout / sizeof layout[0];
    case FOLIO_MENU_CATEGORY:
        return sizeof category / sizeof category[0];
    case FOLIO_MENU_NOTE:
        return sizeof note / sizeof note[0];
    case FOLIO_MENU_EMPTY:
        return 1;
    }
    return 0;
}
const struct folio_menu_action *_Nonnull folio_menu_action_at(enum folio_menu_context context,
                                                              size_t index)
{
    switch (context)
    {
    case FOLIO_MENU_OPERATIONS:
        return &operations[index];
    case FOLIO_MENU_LAYOUT:
        return &layout[index];
    case FOLIO_MENU_CATEGORY:
        return &category[index];
    case FOLIO_MENU_NOTE:
        return &note[index];
    case FOLIO_MENU_EMPTY:
        return &category[3];
    }
    return &operations[0];
}
enum folio_menu_action_kind folio_menu_action_kind(const struct folio_menu_action *_Nonnull action)
{
    return action->kind;
}
enum folio_command folio_menu_action_command(const struct folio_menu_action *_Nonnull action)
{
    return action->command;
}
enum folio_menu_group folio_menu_action_group(const struct folio_menu_action *_Nonnull action)
{
    return action->group;
}
char folio_menu_action_access_key(const struct folio_menu_action *_Nonnull action)
{
    return action->key;
}
enum ui_text_format_outcome
folio_menu_action_caption(const struct folio_menu_action *_Nonnull action,
                          enum folio_language language, char *_Nonnull out, size_t capacity)
{
    switch (action->kind)
    {
    case FOLIO_MENU_ACTION_COMMAND:
        return folio_command_caption(action->command, language, out, capacity);
    case FOLIO_MENU_ACTION_RECOLOR_CATEGORY:
    {
        struct ui_text_request request = {.id = UI_TEXT_COMMAND_CAPTION,
                                          .language = language,
                                          .name = ui_text_line(UI_TEXT_MENU_RECOLOR, language)};
        return ui_text_format(&request, out, capacity);
    }
    }
    return UI_TEXT_FORMAT_TOO_LONG;
}

enum ui_text_format_outcome folio_menu_action_hint(const struct folio_menu_action *_Nonnull action,
                                                   enum folio_language language, char *_Nonnull out,
                                                   size_t capacity)
{
    const char *hint = "";
    enum ui_text format = UI_TEXT_MENU_LOCAL_KEY;
    if (action->kind == FOLIO_MENU_ACTION_COMMAND)
    {
        hint = folio_shortcut_command_hint(action->command, language);
        if (hint[0] != '\0')
            format = UI_TEXT_COMMAND_CAPTION;
        if (folio_command_alias_count(action->command) > 0 &&
            strcmp(hint, folio_command_alias(action->command, 0)) == 0)
            format = UI_TEXT_MENU_EX_HINT;
    }
    char key[2] = {action->key, '\0'};
    struct ui_text_request request = {
        .id = format, .language = language, .name = hint[0] == '\0' ? key : hint};
    return ui_text_format(&request, out, capacity);
}
