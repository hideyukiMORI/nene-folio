/* 全ノートの絞り込みの生成結果（C-005 / ADR 0024 の決定 2）。 */
#ifndef NENEFOLIO_INDEX_FILTER_OUTCOME_H
#define NENEFOLIO_INDEX_FILTER_OUTCOME_H

enum index_filter_outcome : unsigned char
{
    INDEX_FILTER_ACCEPTED,
    INDEX_FILTER_NO_TERM,   /* 語が空。絞り込みをしない（集合は作らない） */
    INDEX_FILTER_MALFORMED, /* 語が整形式の UTF-8 ではない。前の語を保つ */
    INDEX_FILTER_OUT_OF_MEMORY
};

#endif
