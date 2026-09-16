/* note_search の結果（C-005 / ADR 0023 の決定 2）。 */
#ifndef NENEFOLIO_NOTE_SEARCH_OUTCOME_H
#define NENEFOLIO_NOTE_SEARCH_OUTCOME_H

enum note_search_outcome : unsigned char
{
    NOTE_SEARCH_FOUND,
    NOTE_SEARCH_NOT_FOUND, /* 語は妥当だが本文に無い。選択は変えない */
    NOTE_SEARCH_NO_TERM,   /* 語が空。何もしない */
    NOTE_SEARCH_MALFORMED, /* 本文か語の UTF-16 に孤立サロゲートがある。何もしない */
    /* anchor の start が end より後ろ。公開契約の違反なので黙って直さず拒む（2026-09-17 の補正） */
    NOTE_SEARCH_BAD_SPAN
};

#endif
