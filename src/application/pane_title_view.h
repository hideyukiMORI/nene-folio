/* 右ペインの頭に写す表示値（ARC-011）。文字列は folio_state が所有し、次の意図まで有効。 */
#ifndef NENEFOLIO_PANE_TITLE_VIEW_H
#define NENEFOLIO_PANE_TITLE_VIEW_H

#include "pane_recovery.h"
#include "rgb_color.h"

#include <stddef.h>

struct pane_title_view
{
    bool any; /* ノートを選んでいるか。偽なら他のメンバーは空 */
    /* 改名の復旧待ち（ADR0041）。category/note は実名のまま。
     * UIのパンくずが種別の区画だけをcore文言へ置き換え、計測と描画で同じ表示名を使う。 */
    enum pane_recovery recovering;
    size_t ordinal;                /* カテゴリの 1 始まりの番号 */
    const char *_Nonnull category; /* 終端付き UTF-8 */
    const char *_Nonnull note;     /* 終端付き UTF-8 */
    struct rgb_color color;        /* カテゴリの色 */
};

#endif
