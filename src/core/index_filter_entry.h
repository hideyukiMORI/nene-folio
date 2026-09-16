/* 絞り込みが判定する 1 ノートの入力（ADR 0024 の決定 2）。
 * 名前と本文はどちらも UTF-8 で、借りるだけ（集合は番号しか持たない）。
 * body が nullptr のノートは「本文の写しが無い」＝読めなかったノートで、名前によらず一致しない。
 * 全メンバーが独立に妥当なので完全型で公開する（drawer_row と同じ理由・C-003 の例外）。 */
#ifndef NENEFOLIO_INDEX_FILTER_ENTRY_H
#define NENEFOLIO_INDEX_FILTER_ENTRY_H

#include "note_ref.h"

#include <stddef.h>

struct index_filter_entry
{
    struct note_ref ref;        /* 台帳のカテゴリ番号とノート番号 */
    const char *_Nonnull name;  /* ノート名（終端付き UTF-8） */
    const char *_Nullable body; /* 本文の写し。無ければ判定せず見えない */
    size_t length;              /* body のバイト数 */
};

#endif
