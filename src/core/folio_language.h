/* 表示に使う言語（ADR 0030 の決定 1）。閉じた選択肢で、単位 C（ADR 0029）が値を足す。
 * core は「いまの言語」を可変状態として持たない（ARC-005）。値は application の
 * folio_state_language が答え、UI が ui_text_line / ui_text_format へ渡す。 */
#ifndef NENEFOLIO_FOLIO_LANGUAGE_H
#define NENEFOLIO_FOLIO_LANGUAGE_H

enum folio_language : unsigned char
{
    FOLIO_LANGUAGE_JA
};

#endif
