#ifndef NENEFOLIO_NOTE_DESTINATION_H
#define NENEFOLIO_NOTE_DESTINATION_H
#include <stddef.h>
struct note_name;
/* 保存先。nameは呼出し中だけ借りる。categoryの有無はfolio_stateが検証する。 */
struct note_destination
{
    size_t category;
    const struct note_name *_Nonnull name;
};
#endif
