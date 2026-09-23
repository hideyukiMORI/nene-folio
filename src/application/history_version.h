/* 履歴の 1 版の宛先（ADR 0038 の決定 2）。port の read_history へ渡す。
 * 名前は呼び出しの間だけ借り、実装は複製せずに使い切る（他のポート関数と同じ約束）。
 * 引数を 4 つまでに収めるために束ねる（C-012）。全メンバーが独立に妥当なので完全型で公開する
 * （C-003 の例外）。 */
#ifndef NENEFOLIO_HISTORY_VERSION_H
#define NENEFOLIO_HISTORY_VERSION_H

#include <stddef.h>

struct history_version
{
    const char *_Nonnull category; /* 終端付き UTF-8 */
    const char *_Nonnull note;     /* 終端付き UTF-8 */
    size_t version;                /* 1（最新）〜note_history_depth */
};

#endif
