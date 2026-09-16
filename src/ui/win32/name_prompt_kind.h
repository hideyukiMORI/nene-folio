/* 名前入力面が受け持つ操作の種類（ADR0020 / ADR0021 / ADR 0022 の決定 1）。
 * 面は 1 つで、題・説明・初期値・カテゴリの扱い・実行する意図だけがこの値で決まる。 */
#ifndef NENEFOLIO_NAME_PROMPT_KIND_H
#define NENEFOLIO_NAME_PROMPT_KIND_H

enum name_prompt_kind : unsigned char
{
    NAME_PROMPT_FIRST_SAVE,
    NAME_PROMPT_SAVE_AS,
    NAME_PROMPT_RENAME
};

#endif
