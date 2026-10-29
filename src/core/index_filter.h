/* 全ノートの絞り込みの判定（FR-032 / #40 / ADR 0024 の決定 2）。
 * 語（UTF-8）と各ノートの名前・本文（UTF-8）から「見える (category, note) の集合」を作る。
 * 一致は UTF-8 のバイト順序比較で、**ASCII の英字だけ大小を無視**する。多バイト列に ASCII の
 * バイトは現れないので、これは ADR 0023 の UTF-16 の判定と同じ意味になる（全角／半角・かな／カナ・
 * 合成済みと分解は区別する。正規表現は #41 の範囲）。
 * 名前か本文のどちらかに部分一致すれば見える。本文の写しが無いノートは名前によらず見えない。
 * 集合は番号しか持たず、名前も本文も所有しない（ARC-001 / ARC-011）。 */
#ifndef NENEFOLIO_INDEX_FILTER_H
#define NENEFOLIO_INDEX_FILTER_H

#include "index_filter_finish_outcome.h"
#include "index_filter_outcome.h"
#include "note_ref.h"

#include <stddef.h>

struct index_filter;
struct index_filter_build;

/* 台帳から対象を除いた番号へ写す。入力は不変、確保失敗では out も変えない。 */
[[nodiscard]] enum index_filter_outcome
index_filter_removed(const struct index_filter *_Nonnull filter, struct note_ref target,
                     struct index_filter *_Nullable *_Nonnull out);

/* *build must be nonnull and owned by the caller. NOT_READY and OOM preserve both outputs
 * and the build, including a READY build that can retry without its original term.
 * Success alone transfers the sealed owner, sets *build=nullptr and commits out.
 * Former aliases must not be used after success. A READY empty result is accepted. */
[[nodiscard]] enum index_filter_finish_outcome
index_filter_finish(struct index_filter_build *_Nullable *_Nonnull build,
                    struct index_filter *_Nullable *_Nonnull out);
/* そのノートが見えるか。 */
[[nodiscard]] bool index_filter_note(const struct index_filter *_Nonnull filter,
                                     struct note_ref ref);
/* そのカテゴリに見えるノートが 1 つでもあるか（配置はこのカテゴリだけを展開して並べる）。 */
[[nodiscard]] bool index_filter_category(const struct index_filter *_Nonnull filter,
                                         size_t category);
/* 見えるノートの総数。 */
[[nodiscard]] size_t index_filter_count(const struct index_filter *_Nonnull filter);
void index_filter_destroy(struct index_filter *_Nullable filter);

#endif
