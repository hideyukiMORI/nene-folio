/* 失敗の 1 行を利用者に見せる唯一の場所（FR-015 / ARC-001）。文言は application が作る（ARC-011）。
 */
#ifndef NENEFOLIO_FAILURE_BOX_H
#define NENEFOLIO_FAILURE_BOX_H

#include "folio_state_outcome.h"

#include <windows.h>

struct folio_state;

/* 箱の中で言語（folio_state_language）と palette（folio_state_theme）を引き、
 * dialog_theme で塗る自前のモーダルに 1 行を出す（ADR 0035 の決定 4）。state は読むだけ。
 * 自前のモーダルを組み立てられないときだけ MessageBoxW へ退避して必ず知らせる（補正 3）。
 * 箱の題は製品名なので翻訳しない（ADR 0032 の決定 7）。owner はトップレベルの窓（決定 6）。 */
void failure_box_show(HWND _Nullable owner, enum folio_state_outcome outcome,
                      const struct folio_state *_Nonnull state);

#endif
