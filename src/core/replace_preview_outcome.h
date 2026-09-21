/* replace_preview の生成の結果（C-005 / ADR 0028 の決定 6）。 */
#ifndef NENEFOLIO_REPLACE_PREVIEW_OUTCOME_H
#define NENEFOLIO_REPLACE_PREVIEW_OUTCOME_H

enum replace_preview_outcome : unsigned char
{
    REPLACE_PREVIEW_READY,
    /* 置換文字列が replace_template の文法に合わない（未知の `\x`・末尾の単独の `\`） */
    REPLACE_PREVIEW_BAD_TEMPLATE,
    REPLACE_PREVIEW_OUT_OF_MEMORY
};

#endif
