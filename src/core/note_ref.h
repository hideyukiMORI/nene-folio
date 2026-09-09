/* 索引の中のノートの居場所（カテゴリ番号とノート番号の組・ADR 0008 の決定 2）。
 * 移動の意図を 3 引数に収めるための型で、全メンバーが独立に妥当なので完全型で公開する
 * （drawer_row / drop_target と同じ理由・C-003 の例外）。 */
#ifndef NENEFOLIO_NOTE_REF_H
#define NENEFOLIO_NOTE_REF_H

#include <stddef.h>

struct note_ref
{
    size_t category; /* カテゴリ台帳の番号 */
    size_t note;     /* そのカテゴリの索引台帳の番号。落とし先としては 0〜ノート数（末尾） */
};

#endif
