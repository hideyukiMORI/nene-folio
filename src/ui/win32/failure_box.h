/* 失敗の 1 行を利用者に見せる唯一の場所（FR-015 / ARC-001）。文言は application が作る（ARC-011）。
 */
#ifndef NENEFOLIO_FAILURE_BOX_H
#define NENEFOLIO_FAILURE_BOX_H

#include "folio_language.h"
#include "folio_state_outcome.h"

#include <windows.h>

/* language は folio_state_language の値。箱の題は製品名なので翻訳しない（ADR 0032 の決定 7）。 */
void failure_box_show(HWND _Nullable owner, enum folio_state_outcome outcome,
                      enum folio_language language);

#endif
