# いまのタスク — NeNe Folio

記録日: 2026-10-18。hideの「やりやすい順番で進めて」に基づき、Release検索応答→実機確認→次ベータ準備を単体で進める。
現在は[#338](https://github.com/hideyukiMORI/nene-folio/issues/338)の0.2.0配布候補準備。CMakeの唯一の版入力を更新し、製品の機能実装は変更していない。
候補の正典Release生成、版/manifest/AMD64/import/font/ZIP照合、dataなし起動と設定の0.2.0表示が成功。
実4モニターで120→144→168→144→120 DPIへ移動し、最小560×360論理画素で版表示と復帰を確認した。候補は通常終了済み。
詳しい結果・失敗/修正・限界は[品質記録](../quality/2026-10-18-beta-candidate.md)、利用者向け変更点と更新手順は[0.2.0候補ノート](../release/0.2.0.md)。
恒久証跡はD:/NeNeFolio/agents/beta-candidate-20261018/。最終headの必須CI・main・clean main由来の候補・通常exe・整理は同所completion.jsonを参照する。

物理日本語IMEの候補操作と、人による読み上げの聴取は未受理。専用確認データと短い手順を完成時に用意する。
公開版はv0.1.0のまま。この作業は配布準備で、公開tag/GitHub Releaseは作成しない。
schema・依存・ゲート変更なし、Waivers: none。成功済みのcore/OOM/UIA/reader/Release性能を再実行しない。

#336の計測記録はPR #337で必須CI成功後d63f655へ統合済み。10,000件の中央値は初回719ms、不一致z 52ms、全件一致212ms。
全件一致とscrollの主な負担は一覧描画で、画面外行への描画呼出し削減が次の改善候補。条件は[検索応答の記録](../quality/2026-10-18-release-response.md)。
計測用作業木は削除済み。一時出力D:/NeNeFolio/outputs/336-release-response（11,358 files / 294,728,398 bytes）の削除は自動承認審査が実行前にblocked by policyで拒否し、保持した。
全内容は恒久収載・照合済み。詳細はD:/NeNeFolio/agents/release-readiness-20261018/completion.json。拒否した操作を別手段で再試行しない。
以前の#319/#320/#321/#328/#331など7フォルダはhideの手動実行で削除済みであり、この新しい#336の残留と混同しない。
