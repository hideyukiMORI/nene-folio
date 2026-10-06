/* 名前入力面が受け持つ操作の種類（ADR0020 / ADR0021 / ADR 0022 の決定 1・ADR 0039 の決定 10）。
 * 面は 1 つで、題・説明・初期値・カテゴリの一覧の有無・実行する意図だけがこの値で決まる。
 * NEW_CATEGORY はカテゴリの一覧を持たず、未完了の改名の保留経路も通らない。 */
#ifndef NENEFOLIO_NAME_PROMPT_KIND_H
#define NENEFOLIO_NAME_PROMPT_KIND_H

enum name_prompt_kind : unsigned char
{
    NAME_PROMPT_FIRST_SAVE,
    NAME_PROMPT_SAVE_AS,
    NAME_PROMPT_RENAME,
    NAME_PROMPT_NEW_CATEGORY
};

#endif
