/* 置換で適用する一致の選び方（ADR 0028 の決定 6 と決定 8(b)）。閉じた選択肢なので
 * 専用の型で表す（C-002）。選別そのものは core の note_replace が行う。 */
#ifndef NENEFOLIO_REPLACE_SCOPE_H
#define NENEFOLIO_REPLACE_SCOPE_H

enum replace_scope : unsigned char
{
    /* anchor の開始以降で最初の一致（無ければ先頭から巡回）を 1 件だけ（欄の「1 件」・Enter） */
    REPLACE_ONE,
    /* 一致の全部（欄の「すべて」・Ctrl+Enter・`:%s/…/…/g`） */
    REPLACE_ALL,
    /* 各論理行（CR 区切り）の最初の一致だけ（`g` の無い `:%s`。Vim と同じ・決定 8(b)） */
    REPLACE_LINE_FIRST
};

#endif
