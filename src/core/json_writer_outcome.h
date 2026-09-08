/* json_writer の生成と完了の結果（C-005）。 */
#ifndef NENEFOLIO_JSON_WRITER_OUTCOME_H
#define NENEFOLIO_JSON_WRITER_OUTCOME_H

enum json_writer_outcome : unsigned char
{
    JSON_WRITER_ACCEPTED,
    JSON_WRITER_MALFORMED, /* 括弧の不一致・キーの無い値・値の無いキー・最上位が複数 */
    JSON_WRITER_OUT_OF_MEMORY
};

#endif
