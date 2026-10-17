#include "accessible_item.h"

bool accessible_item_row(struct accessible_item item)
{
    return item.kind == ACCESSIBLE_CATEGORY || item.kind == ACCESSIBLE_NOTE;
}

bool accessible_item_root(struct accessible_item item)
{
    return item.kind == ACCESSIBLE_WINDOW || item.kind == ACCESSIBLE_DRAWER;
}

enum accessible_kind accessible_item_host(struct accessible_item item)
{
    switch (item.kind)
    {
    case ACCESSIBLE_DRAWER:
    case ACCESSIBLE_NEW_NOTE:
    case ACCESSIBLE_NEW_CATEGORY:
    case ACCESSIBLE_TREE:
    case ACCESSIBLE_CATEGORY:
    case ACCESSIBLE_NOTE:
        return ACCESSIBLE_DRAWER;
    case ACCESSIBLE_WINDOW:
    case ACCESSIBLE_HEADER:
    case ACCESSIBLE_LAYOUT_GROUP:
    case ACCESSIBLE_LAYOUT_DRAWER:
    case ACCESSIBLE_LAYOUT_BOTH:
    case ACCESSIBLE_LAYOUT_NOTE:
    case ACCESSIBLE_SETTINGS:
    case ACCESSIBLE_CLOSE:
    case ACCESSIBLE_PANE_TOOLS:
    case ACCESSIBLE_MODE_GROUP:
    case ACCESSIBLE_VIEW:
    case ACCESSIBLE_EDIT:
    case ACCESSIBLE_MENU:
    case ACCESSIBLE_PANE_NEW:
    case ACCESSIBLE_SAVE:
    case ACCESSIBLE_HELP:
    case ACCESSIBLE_FIND:
    case ACCESSIBLE_FILTER:
    case ACCESSIBLE_BODY:
        return ACCESSIBLE_WINDOW;
    }
    return ACCESSIBLE_WINDOW;
}

bool accessible_item_equal(struct accessible_item left, struct accessible_item right)
{
    if (left.kind != right.kind)
    {
        return false;
    }
    return !accessible_item_row(left) ||
           (left.revision == right.revision && left.ref.category == right.ref.category &&
            left.ref.note == right.ref.note);
}
