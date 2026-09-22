/* 表示に使う言語（ADR 0030 の決定 1・ADR 0032 の決定 1）。閉じた選択肢で、
 * core は「いまの言語」を可変状態として持たない（ARC-005）。値は application の
 * folio_state_language が答え、UI が ui_text_line / ui_text_format へ渡す。
 * 値を足すときは ui_text.c の表に列を 1 つ、ui_font.c と folio_settings.c の表に行を 1 つ足す
 * （CNF-009 が表の行を、CNF-011 が表の列を守る）。 */
#ifndef NENEFOLIO_FOLIO_LANGUAGE_H
#define NENEFOLIO_FOLIO_LANGUAGE_H

#include <stddef.h>

enum folio_language : unsigned char
{
    FOLIO_LANGUAGE_JA,
    FOLIO_LANGUAGE_EN,
    FOLIO_LANGUAGE_ZH_HANS
};

/* 言語の数。正本は上の列挙で、単体が一致を固定する（ADR 0032 の決定 1）。 */
constexpr size_t folio_language_count = FOLIO_LANGUAGE_ZH_HANS + 1;

#endif
