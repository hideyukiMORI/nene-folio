# いまのタスク — NeNe Folio

記録日: 2026-10-18。hideの「それに着手して」を受け、[#331](https://github.com/hideyukiMORI/nene-folio/issues/331)の本文部分一致を改善中。
基点mainは85e8beb。#328の宛先探索改善後に残った、index_filterの反復比較を対象とする。
検索語ごとに固定長の移動表を作り、既存の名前/本文検索の一経路で使う。ASCIIのみ大小同一、明示長、失敗時の採用済み状態を維持する。

完成候補は対象build/filter/application/OOM試験・規約検査が成功し、独立レビューに修正要求なし。
全16条件で入力/結果一致・回帰条件を達成し、性能受理とした。10,000件のzは531.333→329.921ms（37.907%短縮）、aaaaabは1096.857→259.008ms（76.386%短縮）。
[PR #332](https://github.com/hideyukiMORI/nene-folio/pull/332)の最終head必須CI、統合、通常exe反映、専用作業物整理を進める。
実装証跡643件は恒久収載・hash照合済み。外部負荷と重なったrunも保持する。
受理条件・実測は[品質記録](../quality/2026-10-18-filter-matching-checks.md)、再開の入口は[引き継ぎ](../handoffs/2026-10-18.md)。
作業木D:/NeNeFolio/worktrees/331-filter-matching、恒久証跡D:/NeNeFolio/agents/331-filter-matching/。

成功済み検証は関係入力が不変なら再利用する。schema・依存・ゲート変更なし、Waivers: none。
旧#319/#320/#321/#328の残留物は前件の記録に従って保持し、今回の整理へ混ぜない。
