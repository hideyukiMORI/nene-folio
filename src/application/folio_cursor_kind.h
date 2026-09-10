/* 索引のカーソルが止まっている行の種類（ADR 0015 の決定 1）。閉じた集合（C-002）で、
 * カーソルの有無は問い合わせの戻り値（bool）が答える。 */
#ifndef NENEFOLIO_FOLIO_CURSOR_KIND_H
#define NENEFOLIO_FOLIO_CURSOR_KIND_H

enum folio_cursor_kind : unsigned char
{
    FOLIO_CURSOR_NOTE,    /* 選択中のノートの行。カーソルは選択そのもの */
    FOLIO_CURSOR_CATEGORY /* 見えるノート行を持たないカテゴリ行（折り畳み・ノート 0 本） */
};

#endif
