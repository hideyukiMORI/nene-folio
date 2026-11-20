/* #1071 / ADR0118: 依頼の箇所を原文の上で決める。リンクと依頼のファイルが共有する唯一の探し方
 * （ARC-001）。確保しない純粋な関数。 */
#ifndef NENEFOLIO_OPEN_PLACE_H
#define NENEFOLIO_OPEN_PLACE_H
#include "note_byte_span.h"
#include "open_place_kind.h"
#include <stddef.h>
struct open_request;
/* 「あたり」の幅: 指定の行の範囲の前後この行数までを先に探す。 */
constexpr size_t open_place_margin = 5;
/* source（UTF-8、length バイト）の上で request の箇所を探し、見つけた範囲を out へ入れる。
 * 指定の行のあたりで文を探す → 無ければ全体から探す → 無ければ指定の行 → どれも無ければ NONE。
 * 文は大小を区別して探し、最初の一致を取る。行の範囲は改行を含まない。ノートの終わりを越える行は、
 * ある所までに切り詰める（始まりが終わりを越えていれば、行は無いものとする）。NONE では out は
 * 変えない。 */
[[nodiscard]] enum open_place_kind open_place_find(const char *_Nonnull source, size_t length,
                                                   const struct open_request *_Nonnull request,
                                                   struct note_byte_span *_Nonnull out);
#endif
