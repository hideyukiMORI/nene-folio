/* 名前入力面に渡すもの（ADR0020 / ADR0021 / ADR 0022 の決定 1）。
 * 本文（units / count）は面を出しているあいだだけ借りる。
 * NAME_PROMPT_NEW_CATEGORY は本文を使わないので、呼ぶ側は u"" と 0 を渡す
 * （ADR 0039 の決定 10）。 */
#ifndef NENEFOLIO_NAME_PROMPT_REQUEST_H
#define NENEFOLIO_NAME_PROMPT_REQUEST_H

#include "name_prompt_kind.h"

#include <stddef.h>
#include <uchar.h>

struct folio_state;

struct name_prompt_request
{
    struct folio_state *_Nonnull state;
    enum name_prompt_kind kind;
    /* RENAME_CATEGORYだけが読む明示対象。RETRY_RENAMEは本文/targetを使わない。 */
    size_t target;
    const char16_t *_Nonnull units;
    size_t count;
};

#endif
