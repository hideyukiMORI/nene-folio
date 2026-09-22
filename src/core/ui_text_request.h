/* ui_text_format に渡すもの（ADR 0030 の決定 3）。ID と言語と、置換子に入れる値を束ねる
 * （C-012 の引数 4 つの上限）。文字列は呼び出しの間だけ借り、NULL は空文字列として埋める。
 * 全メンバーが独立に妥当なので完全型で公開する（C-003 の例外）。 */
#ifndef NENEFOLIO_UI_TEXT_REQUEST_H
#define NENEFOLIO_UI_TEXT_REQUEST_H

#include "folio_language.h"
#include "ui_text.h"

#include <stddef.h>

struct ui_text_request
{
    enum ui_text id;
    enum folio_language language;
    size_t k;                   /* {k} */
    size_t n;                   /* {n} */
    size_t offset;              /* {offset} */
    const char *_Nullable name; /* {name} */
    const char *_Nullable from; /* {from} */
    const char *_Nullable to;   /* {to} */
};

#endif
