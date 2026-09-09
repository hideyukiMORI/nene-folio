/* 失敗の 1 行を利用者に見せる唯一の場所（FR-015 / ARC-001）。文言は application が作る（ARC-011）。
 */
#ifndef NENEFOLIO_FAILURE_BOX_H
#define NENEFOLIO_FAILURE_BOX_H

#include "folio_state_outcome.h"

#include <windows.h>

void failure_box_show(HWND _Nullable owner, enum folio_state_outcome outcome);

#endif
