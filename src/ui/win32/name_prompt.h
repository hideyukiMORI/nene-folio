/* 日本語の初回保存・別名保存・名前変更の面。親を無効にする同期モーダルで、
 * 本文は呼出し中だけ借りる（ADR0020 / ADR 0022 の決定 1）。 */
#ifndef NENEFOLIO_NAME_PROMPT_H
#define NENEFOLIO_NAME_PROMPT_H
#include "folio_state_outcome.h"
#include "name_prompt_request.h"
#include <windows.h>
struct folio_palette;
/* palette は開く瞬間の色で、面はそれを写して塗る（ADR 0035 の決定 3・5）。 */
[[nodiscard]] enum folio_state_outcome
name_prompt_show(HWND _Nonnull owner, const struct name_prompt_request *_Nonnull request,
                 const struct folio_palette *_Nonnull palette);
#endif
