/* 台帳へ渡す短命な値。名前は借用し、挿入時にname_listが検証/複製する。
 * 名前と星は独立した値で、台帳の順序や所有状態は持たない（C-003）。 */
#ifndef NENEFOLIO_NOTE_ENTRY_H
#define NENEFOLIO_NOTE_ENTRY_H
struct note_entry
{
    const char *_Nonnull name;
    bool favorite;
};
#endif
