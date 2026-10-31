#ifndef NENEFOLIO_ACCESSIBLE_REQUEST_H
#define NENEFOLIO_ACCESSIBLE_REQUEST_H

#include "accessible_action.h"
#include "accessible_item.h"

struct accessible_request
{
    struct accessible_item item;
    enum accessible_action action;
    int scroll_target; /* SCROLLのときだけ使う、検証済みの絶対画素位置。 */
    int drawer_width;  /* SET_WIDTHのときだけ使うlogical px。現在の範囲で検証する。 */
};

#endif
