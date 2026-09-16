/* 改名の入力値。所有権と妥当性はnote_renameの生成側が確定する。 */
#ifndef NENEFOLIO_NOTE_RENAME_TARGET_H
#define NENEFOLIO_NOTE_RENAME_TARGET_H
#include <stddef.h>
struct note_name;
struct note_rename_target
{
    size_t index;
    const struct note_name *_Nonnull name;
};
#endif
