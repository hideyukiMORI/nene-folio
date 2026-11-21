#ifndef NENEFOLIO_REQUEST_OUTCOME_H
#define NENEFOLIO_REQUEST_OUTCOME_H
/* ADR0119 の決定 6: 依頼のファイルを 1 つ取り出した結果。 */
enum request_outcome : unsigned char
{
    REQUEST_NONE,      /* 取り出せる依頼のファイルは無い */
    REQUEST_TAKEN,     /* 中身を渡し、そのファイルは消した */
    REQUEST_DISCARDED, /* 読まずに消した（大きすぎる・古すぎる）。まだ次があるかもしれない */
    REQUEST_OUT_OF_MEMORY
};
#endif
