# いまのタスク — NeNe Folio

記録日: 2026-10-18。hideの「それに着手して」に基づく[#331](https://github.com/hideyukiMORI/nene-folio/issues/331)の本文部分一致改善は完了。
[PR #332](https://github.com/hideyukiMORI/nene-folio/pull/332)は最終head b4e3ffcの[必須CI](https://github.com/hideyukiMORI/nene-folio/actions/runs/39102770310)成功後、d65dcdcへ統合。IssueはCLOSED。
通常exeは15:08:29更新済み。検索結果、ASCIIのみ大小同一、明示長、失敗時の採用済み状態を維持して比較を減らした。
全16条件で入力/結果一致・回帰条件を達成。10,000件のzは531.333→329.921ms（37.907%短縮）、aaaaabは1096.857→259.008ms（76.386%短縮）。
性能値は合成データ・Debug sanitizerのcore factory比較に限り、UI/I/O/Release全体の短縮率ではない。

受理条件・実測は[品質記録](../quality/2026-10-18-filter-matching-checks.md)、再開の入口は[引き継ぎ](../handoffs/2026-10-18.md)。
恒久証跡はD:/NeNeFolio/agents/331-filter-matching/。最終PR/CI・exe・整理状態は同所completion.jsonに記録する。
成功済みの対象検証・性能比較・独立レビューは再実行不要。schema・依存・ゲート変更なし、Waivers: none。

一時出力D:/NeNeFolio/outputs/331-filter-matching（2,910ファイル、284,362,460 bytes）の削除は自動承認審査がblocked by policyで拒否したため保持する。
唯一未収載の成果物・稼働参照・リンクは監査で見つからず、保存先との全hashも一致した。拒否を別手段で回避しない。
追加作業木は完了記録の収載後に整理し、枝とcommitは保持する。旧#319/#320/#321/#328の残留物は前件の記録に従って保持する。
新しい作業は現在のhide指示と実Issue/PR・checkoutを照合してから開始する。
