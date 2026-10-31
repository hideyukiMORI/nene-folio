#include "accessible_item.h"

bool accessible_item_shell(enum accessible_kind kind)
{
    return kind == ACCESSIBLE_CLOSE || kind == ACCESSIBLE_MINIMIZE || kind == ACCESSIBLE_MAXIMIZE;
}

bool accessible_item_menu(enum accessible_kind kind)
{
    return kind == ACCESSIBLE_MENU || kind == ACCESSIBLE_LAYOUT_CHOOSER;
}

bool accessible_item_row(struct accessible_item item)
{
    return item.kind == ACCESSIBLE_CATEGORY || item.kind == ACCESSIBLE_NOTE;
}

bool accessible_item_root(struct accessible_item item)
{
    return item.kind == ACCESSIBLE_WINDOW || item.kind == ACCESSIBLE_DRAWER ||
           item.kind == ACCESSIBLE_SURFACE;
}

bool accessible_item_surface(struct accessible_item item)
{
    return item.kind >= ACCESSIBLE_SURFACE && item.kind <= ACCESSIBLE_SURFACE_STATUS;
}

enum accessible_kind accessible_item_host(struct accessible_item item)
{
    switch (item.kind)
    {
    case ACCESSIBLE_DRAWER:
    case ACCESSIBLE_NEW_CATEGORY:
    case ACCESSIBLE_TREE:
    case ACCESSIBLE_CATEGORY:
    case ACCESSIBLE_NOTE:
        return ACCESSIBLE_DRAWER;
    case ACCESSIBLE_WINDOW:
    case ACCESSIBLE_SPLITTER:
    case ACCESSIBLE_HEADER:
    case ACCESSIBLE_NEW_NOTE:
    case ACCESSIBLE_LAYOUT_GROUP:
    case ACCESSIBLE_LAYOUT_DRAWER:
    case ACCESSIBLE_LAYOUT_BOTH:
    case ACCESSIBLE_LAYOUT_NOTE:
    case ACCESSIBLE_LAYOUT_CHOOSER:
    case ACCESSIBLE_MINIMIZE:
    case ACCESSIBLE_MAXIMIZE:
    case ACCESSIBLE_SETTINGS:
    case ACCESSIBLE_CLOSE:
    case ACCESSIBLE_PANE_TOOLS:
    case ACCESSIBLE_BREADCRUMB:
    case ACCESSIBLE_MODE_GROUP:
    case ACCESSIBLE_ACTION_GROUP:
    case ACCESSIBLE_VIEW:
    case ACCESSIBLE_EDIT:
    case ACCESSIBLE_MENU:
    case ACCESSIBLE_SAVE:
    case ACCESSIBLE_HELP:
    case ACCESSIBLE_FIND:
    case ACCESSIBLE_FILTER:
    case ACCESSIBLE_BODY:
    case ACCESSIBLE_NOTICE:
    case ACCESSIBLE_GUIDE:
    case ACCESSIBLE_COMMAND_INPUT:
    case ACCESSIBLE_REPLACE_PATTERN:
    case ACCESSIBLE_REPLACE_TEXT:
        return ACCESSIBLE_WINDOW;
    case ACCESSIBLE_SURFACE:
    case ACCESSIBLE_SURFACE_TABS:
    case ACCESSIBLE_SURFACE_TAB:
    case ACCESSIBLE_SURFACE_LIST:
    case ACCESSIBLE_SURFACE_CHOICE:
    case ACCESSIBLE_SURFACE_LANGUAGE:
    case ACCESSIBLE_SURFACE_TOGGLE:
    case ACCESSIBLE_SURFACE_BUTTON:
    case ACCESSIBLE_SURFACE_TEXT:
    case ACCESSIBLE_SURFACE_HELP:
    case ACCESSIBLE_SURFACE_STATUS:
        return ACCESSIBLE_SURFACE;
    }
    return ACCESSIBLE_WINDOW;
}

bool accessible_item_equal(struct accessible_item left, struct accessible_item right)
{
    if (left.kind != right.kind)
    {
        return false;
    }
    if (accessible_item_surface(left) && !accessible_item_root(left))
    {
        return left.revision == right.revision && left.slot == right.slot;
    }
    return !accessible_item_row(left) ||
           (left.revision == right.revision && left.ref.category == right.ref.category &&
            left.ref.note == right.ref.note);
}
