/* ドロワーの配置が読む索引（ADR 0024 の決定 3）。台帳の列と、いま効いている絞り込みを 1
 * つに束ねる。 filter
 * が無ければ台帳のとおりに、あれば「見えるノートを持つカテゴリだけを展開して」並べる。 配置の関数は
 * 1 本だけで、絞り込みの有無で経路を分けない（ARC-001 / C-012 の 4 引数）。
 * 全メンバーが独立に妥当なので完全型で公開する（drawer_metrics と同じ理由・C-003 の例外）。 */
#ifndef NENEFOLIO_DRAWER_SOURCE_H
#define NENEFOLIO_DRAWER_SOURCE_H

struct category_ledger;
struct index_filter;
struct note_ledger;

struct drawer_source
{
    const struct category_ledger *_Nonnull categories;
    /* categories と同じ数・同じ順の索引台帳。名前は配置へ複製され、台帳より長く生きてよい。 */
    const struct note_ledger *_Nonnull const *_Nonnull notes;
    const struct index_filter *_Nullable filter; /* 絞り込み。無ければ全部が見える */
};

#endif
