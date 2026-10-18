# いまのタスク — NeNe Folio

記録日: 2026-10-18。hideの「それに着手して」に基づく[#331](https://github.com/hideyukiMORI/nene-folio/issues/331)の本文部分一致改善は完了。
[PR #332](https://github.com/hideyukiMORI/nene-folio/pull/332)は最終head b4e3ffcの[必須CI](https://github.com/hideyukiMORI/nene-folio/actions/runs/39102770310)成功後、d65dcdcへ統合。IssueはCLOSED。
通常exeは15:08:29更新済み。検索結果、ASCIIのみ大小同一、明示長、失敗時の採用済み状態を維持して比較を減らした。
全16条件で入力/結果一致・回帰条件を達成。10,000件のzは531.333→329.921ms（37.907%短縮）、aaaaabは1096.857→259.008ms（76.386%短縮）。
性能値は合成データ・Debug sanitizerのcore factory比較に限り、UI/I/O/Release全体の短縮率ではない。

受理条件・実測は[品質記録](../quality/2026-10-18-filter-matching-checks.md)、再開の入口は[引き継ぎ](../handoffs/2026-10-18.md)。
恒久証跡はD:/NeNeFolio/agents/331-filter-matching/。PR #333も必須CI成功後に統合し、追加作業木は15:16に削除済み。
最終PR/CI・exe・作業木整理は同所completion.json、後の手動削除は下記実行記録で確認する。
成功済みの対象検証・性能比較・独立レビューは再実行不要。schema・依存・ゲート変更なし、Waivers: none。

hideの手動実行により15:40:09、#319/#320/#321/#328/#331と計画・デザインの一時出力7フォルダを削除済み。
40,279ファイル、1,323,547,077 bytes。15:48と15:50にも対象パスの不在を確認し、15:48には削除processの終了も確認した。
実行記録はD:/NeNeFolio/agents/manual-cleanup-20261018/cleanup-result-20261018-154009.json。
outputsに残る282-release-prepと284-beta-releaseは保存対象の配布ZIPで、恒久証跡も保持している。
過去の削除拒否は当時の事実であり、現在の削除待ちとして扱わない。状態文書の整合は[#334](https://github.com/hideyukiMORI/nene-folio/issues/334)で収載する。
新しい作業は現在のhide指示と実Issue/PR・checkoutを照合してから開始する。
