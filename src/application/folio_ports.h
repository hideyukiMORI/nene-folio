/* folio_state を作るときに渡す 3 つのポートの束（ADR 0029 の決定 3）。引数の数が C-012 の
 * 4 つで飽和したので、署名ではなくこの型が増える側になる（ADR 0028 の決定 2 の予告）。
 * どれも借りるだけで、束そのものは呼び出しの間しか生きなくてよい（folio_state は 3 つの
 * ポートを複製して持つ）。ポートの adapter は state より長く生きていなければならない。
 * 全メンバーが独立に妥当なので完全型で公開する（C-003 の例外）。 */
#ifndef NENEFOLIO_FOLIO_PORTS_H
#define NENEFOLIO_FOLIO_PORTS_H

struct appearance_port;
struct persistence_port;
struct regex_port;

struct folio_ports
{
    const struct persistence_port *_Nonnull persistence;
    const struct appearance_port *_Nonnull appearance;
    const struct regex_port *_Nonnull regex;
};

#endif
