/* line_index の結果（C-005 / ARC-010 / ADR 0026 の決定 2）。 */
#ifndef NENEFOLIO_LINE_INDEX_OUTCOME_H
#define NENEFOLIO_LINE_INDEX_OUTCOME_H

enum line_index_outcome : unsigned char
{
    LINE_INDEX_READY,
    /* 文字位置が本文の外、または論理行番号が 1..行数 の外。出力は触らない */
    LINE_INDEX_OUT_OF_RANGE,
    LINE_INDEX_OUT_OF_MEMORY
};

#endif
