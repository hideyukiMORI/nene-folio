#ifndef NENEFOLIO_OPEN_PLACE_KIND_H
#define NENEFOLIO_OPEN_PLACE_KIND_H
/* #1071 / ADR0118: 開いたノートの中で、依頼の箇所がどう決まったか。 */
enum open_place_kind : unsigned char
{
    OPEN_PLACE_NONE,           /* 箇所は無い（指定が無い、または行も文も見つからない）。開くだけ */
    OPEN_PLACE_TEXT_NEAR,      /* 探す文が、指定の行のあたりで見つかった */
    OPEN_PLACE_TEXT_ELSEWHERE, /* 探す文が、ノートのほかの所で見つかった（行の指定が無い場合も） */
    OPEN_PLACE_LINES           /* 探す文は無い・見つからないので、指定の行 */
};
#endif
