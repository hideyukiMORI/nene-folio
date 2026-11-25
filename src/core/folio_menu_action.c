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
    enum folio_text_edit edit; /* #883: TEXT actions only */
};
static const struct folio_menu_action operations[] = {
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_SAVE_AS, FOLIO_MENU_GROUP_FIRST, 'A',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_RENAME, FOLIO_MENU_GROUP_FIRST, 'R',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_VIEW, FOLIO_MENU_GROUP_FIRST, 'V',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_REPLACE, FOLIO_MENU_GROUP_SECOND, 'P',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_HISTORY, FOLIO_MENU_GROUP_SECOND, 'H',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_EDIT_TAGS, FOLIO_MENU_GROUP_SECOND, 'G',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_FILTER_TAGS, FOLIO_MENU_GROUP_SECOND, 'L',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_MANAGE_TAGS, FOLIO_MENU_GROUP_SECOND, 'J',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_DISCOVER_NOTES, FOLIO_MENU_GROUP_SECOND, 'Q',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_TOGGLE_FAVORITE, FOLIO_MENU_GROUP_SECOND, 'F',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_TOGGLE_FAVORITES_ONLY, FOLIO_MENU_GROUP_SECOND, 'O',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_TOGGLE_FAVORITES_FIRST, FOLIO_MENU_GROUP_SECOND, 'B',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_TOGGLE_MINIMAP, FOLIO_MENU_GROUP_SECOND, 'M',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_TOGGLE_OUTLINE, FOLIO_MENU_GROUP_SECOND, 'D',
     FOLIO_TEXT_EDIT_NONE},
    /* #1264: 'H' is History's in this menu. */
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_TOGGLE_NOTE_HEADER, FOLIO_MENU_GROUP_SECOND, 'N',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_IMPORT, FOLIO_MENU_GROUP_THIRD, 'I',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_EXPORT_NOTE, FOLIO_MENU_GROUP_THIRD, 'X',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_EXPORT_CATEGORY, FOLIO_MENU_GROUP_THIRD, 'C',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_EXPORT_ALL, FOLIO_MENU_GROUP_THIRD, 'Z',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_DISCARD_EDITS, FOLIO_MENU_GROUP_DESTRUCTIVE, 'E',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_TRASH_NOTE, FOLIO_MENU_GROUP_DESTRUCTIVE, 'T',
     FOLIO_TEXT_EDIT_NONE}};
static const struct folio_menu_action category[] = {
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_NEW, FOLIO_MENU_GROUP_FIRST, 'N',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_RECOLOR_CATEGORY, FOLIO_COMMAND_NEW, FOLIO_MENU_GROUP_SECOND, 'C',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_RENAME_CATEGORY, FOLIO_MENU_GROUP_SECOND, 'R',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_NEW_CATEGORY, FOLIO_MENU_GROUP_THIRD, 'A',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_IMPORT, FOLIO_MENU_GROUP_THIRD, 'I',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_EXPORT_CATEGORY, FOLIO_MENU_GROUP_THIRD, 'X',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_DELETE_CATEGORY, FOLIO_MENU_GROUP_DESTRUCTIVE, 'D',
     FOLIO_TEXT_EDIT_NONE}};
static const struct folio_menu_action note[] = {
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_TOGGLE_FAVORITE, FOLIO_MENU_GROUP_FIRST, 'F',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_RENAME, FOLIO_MENU_GROUP_FIRST, 'R',
     FOLIO_TEXT_EDIT_NONE},
    /* #941: to another category; the destination is chosen on its own surface. */
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_MOVE_NOTE, FOLIO_MENU_GROUP_FIRST, 'M',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_TRASH_NOTE, FOLIO_MENU_GROUP_DESTRUCTIVE, 'T',
     FOLIO_TEXT_EDIT_NONE}};
/* #883: the note body's menu. Viewing is read-only, so it has no Undo, Cut or Paste. */
static const struct folio_menu_action text_edit[] = {
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_FIRST, 'U', FOLIO_TEXT_EDIT_UNDO},
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_SECOND, 'T', FOLIO_TEXT_EDIT_CUT},
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_SECOND, 'C',
     FOLIO_TEXT_EDIT_COPY},
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_SECOND, 'P',
     FOLIO_TEXT_EDIT_PASTE},
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_THIRD, 'A',
     FOLIO_TEXT_EDIT_SELECT_ALL}};
/* #1012 / ADR0116: viewing also copies the Markdown of the selected blocks and opens the editing
 * view at the clicked paragraph's source. */
static const struct folio_menu_action text_view[] = {
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_FIRST, 'C', FOLIO_TEXT_EDIT_COPY},
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_FIRST, 'M',
     FOLIO_TEXT_EDIT_COPY_MARKDOWN},
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_SECOND, 'E',
     FOLIO_TEXT_EDIT_EDIT_HERE},
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_THIRD, 'A',
     FOLIO_TEXT_EDIT_SELECT_ALL}};
/* #900: viewing, on a link: the link's own actions first. */
static const struct folio_menu_action text_link[] = {
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_FIRST, 'O',
     FOLIO_TEXT_EDIT_OPEN_LINK},
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_FIRST, 'L',
     FOLIO_TEXT_EDIT_COPY_LINK},
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_SECOND, 'C',
     FOLIO_TEXT_EDIT_COPY},
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_SECOND, 'M',
     FOLIO_TEXT_EDIT_COPY_MARKDOWN},
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_SECOND, 'E',
     FOLIO_TEXT_EDIT_EDIT_HERE},
    {FOLIO_MENU_ACTION_TEXT, FOLIO_COMMAND_HELP, FOLIO_MENU_GROUP_THIRD, 'A',
     FOLIO_TEXT_EDIT_SELECT_ALL}};
static const struct folio_menu_action layout[] = {
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_LAYOUT_DRAWER, FOLIO_MENU_GROUP_FIRST, '1',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_LAYOUT_BOTH, FOLIO_MENU_GROUP_FIRST, '2',
     FOLIO_TEXT_EDIT_NONE},
    {FOLIO_MENU_ACTION_COMMAND, FOLIO_COMMAND_LAYOUT_NOTE, FOLIO_MENU_GROUP_FIRST, '3',
     FOLIO_TEXT_EDIT_NONE}};

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
    case FOLIO_MENU_TEXT_EDIT:
        return sizeof text_edit / sizeof text_edit[0];
    case FOLIO_MENU_TEXT_VIEW:
        return sizeof text_view / sizeof text_view[0];
    case FOLIO_MENU_TEXT_LINK:
        return sizeof text_link / sizeof text_link[0];
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
    case FOLIO_MENU_TEXT_EDIT:
        return &text_edit[index];
    case FOLIO_MENU_TEXT_VIEW:
        return &text_view[index];
    case FOLIO_MENU_TEXT_LINK:
        return &text_link[index];
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
enum folio_text_edit folio_menu_action_text_edit(const struct folio_menu_action *_Nonnull action)
{
    return action->edit;
}
static enum ui_text text_caption(enum folio_text_edit edit)
{
    switch (edit)
    {
    case FOLIO_TEXT_EDIT_NONE:
    case FOLIO_TEXT_EDIT_UNDO:
        return UI_TEXT_MENU_UNDO;
    case FOLIO_TEXT_EDIT_CUT:
        return UI_TEXT_MENU_CUT;
    case FOLIO_TEXT_EDIT_COPY:
        return UI_TEXT_MENU_COPY;
    case FOLIO_TEXT_EDIT_PASTE:
        return UI_TEXT_MENU_PASTE;
    case FOLIO_TEXT_EDIT_SELECT_ALL:
        return UI_TEXT_MENU_SELECT_ALL;
    case FOLIO_TEXT_EDIT_OPEN_LINK:
        return UI_TEXT_MENU_OPEN_LINK;
    case FOLIO_TEXT_EDIT_COPY_LINK:
        return UI_TEXT_MENU_COPY_LINK;
    case FOLIO_TEXT_EDIT_EDIT_HERE:
        return UI_TEXT_MENU_EDIT_HERE;
    case FOLIO_TEXT_EDIT_COPY_MARKDOWN:
        return UI_TEXT_MENU_COPY_MARKDOWN;
    }
    return UI_TEXT_MENU_UNDO;
}
/* The keys the note body already answers to; the same in every language. */
static const char *_Nonnull text_key(enum folio_text_edit edit)
{
    switch (edit)
    {
    case FOLIO_TEXT_EDIT_NONE:
    case FOLIO_TEXT_EDIT_OPEN_LINK:
    case FOLIO_TEXT_EDIT_COPY_LINK:
    case FOLIO_TEXT_EDIT_EDIT_HERE:
    case FOLIO_TEXT_EDIT_COPY_MARKDOWN:
        return ""; /* no key of their own: the menu shows its access key */
    case FOLIO_TEXT_EDIT_UNDO:
        return "Ctrl+Z";
    case FOLIO_TEXT_EDIT_CUT:
        return "Ctrl+X";
    case FOLIO_TEXT_EDIT_COPY:
        return "Ctrl+C";
    case FOLIO_TEXT_EDIT_PASTE:
        return "Ctrl+V";
    case FOLIO_TEXT_EDIT_SELECT_ALL:
        return "Ctrl+A";
    }
    return "";
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
    case FOLIO_MENU_ACTION_TEXT:
    {
        struct ui_text_request request = {.id = UI_TEXT_COMMAND_CAPTION,
                                          .language = language,
                                          .name =
                                              ui_text_line(text_caption(action->edit), language)};
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
    if (action->kind == FOLIO_MENU_ACTION_TEXT)
    {
        hint = text_key(action->edit);
        if (hint[0] != '\0')
            format = UI_TEXT_COMMAND_CAPTION;
    }
    char key[2] = {action->key, '\0'};
    struct ui_text_request request = {
        .id = format, .language = language, .name = hint[0] == '\0' ? key : hint};
    return ui_text_format(&request, out, capacity);
}
