/* replace_template の生成の結果（C-005 / ADR 0028 の決定 5）。 */
#ifndef NENEFOLIO_REPLACE_TEMPLATE_OUTCOME_H
#define NENEFOLIO_REPLACE_TEMPLATE_OUTCOME_H

enum replace_template_outcome : unsigned char
{
    REPLACE_TEMPLATE_READY,
    /* 未知の `\x`、または末尾の単独の `\`。黙って消さずに断る（決定 5） */
    REPLACE_TEMPLATE_MALFORMED,
    REPLACE_TEMPLATE_OUT_OF_MEMORY
};

#endif
