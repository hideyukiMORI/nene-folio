/* 日本語の初回保存面。親を無効にする同期モーダルで、本文は呼出し中だけ借りる（ADR0020）。 */
#ifndef NENEFOLIO_NAME_PROMPT_H
#define NENEFOLIO_NAME_PROMPT_H
#include "folio_state_outcome.h"
#include <stddef.h>
#include <uchar.h>
#include <windows.h>
struct folio_state;
[[nodiscard]] enum folio_state_outcome name_prompt_show(HWND _Nonnull owner,
                                                        struct folio_state *_Nonnull state,
                                                        const char16_t *_Nonnull units,
                                                        size_t count);
#endif
