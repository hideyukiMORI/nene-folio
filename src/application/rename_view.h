/* 未完了の改名の表示値（ARC-011 / ADR 0022 の決定 2）。
 * 文字列は folio_state が所有し、次の意図まで有効。表示用の文言は UI 側が持つ。 */
#ifndef NENEFOLIO_RENAME_VIEW_H
#define NENEFOLIO_RENAME_VIEW_H

struct rename_view
{
    const char *_Nonnull from; /* 終端付き UTF-8。いまも data/ にあるはずの名前 */
    const char *_Nonnull to;   /* 終端付き UTF-8。移し終えたい名前 */
};

#endif
