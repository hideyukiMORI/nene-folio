/* #1152 / ADR0119: 本文を編集していて保存していない変更があるときに、外から「このノートを開いて」
 * の依頼が来た。どうするかを利用者に選んでもらう面（hide の判断 2026-11-22: モーダルで聞く）。
 * 縦に 3 つ: 保存して開く（既定・Enter）／別の名前で残して開く／開かない（Esc）。どの釦にも、
 * 押すと何が起きるかを 1 行で添える。塗りは dialog_theme（名前入力面・失敗の箱と同じ）。 */
#ifndef NENEFOLIO_REQUEST_PROMPT_H
#define NENEFOLIO_REQUEST_PROMPT_H
#include "request_prompt_choice.h"
#include "request_prompt_request.h"

#include <windows.h>

struct request_prompt;
/* 面を閉じるまで戻らない。owner はトップレベルの窓。 */
[[nodiscard]] enum request_prompt_choice
request_prompt_show(HWND _Nullable owner, const struct request_prompt_request *_Nonnull request);
#endif
