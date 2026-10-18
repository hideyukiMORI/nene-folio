# いまのタスク — NeNe Folio

記録日: 2026-10-18。hideの0.2.0実機フィードバックを単体で対応。
#340の表示修正はPR #341、最終head0795381の必須CI39111663236成功後、a31e428へ統合済み。
編集の行間/行番号、検索placeholder、新規ボタンの表示を修正。[表示の品質記録](../quality/2026-10-18-editor-layout.md)。

現在は[#342](https://github.com/hideyukiMORI/nene-folio/issues/342)の検索focus保存。
名前付きEDITは既存保存経路、UNTITLEDは質問せず検索へ進んで対象外。保存失敗時は本文/Undo/検索語を保つ。
4入口・同本文no-op・履歴/本文失敗と再試行・再入・IMEガード・UNTITLEDの対象native確認は成功。
[品質記録](../quality/2026-10-18-filter-focus-save.md)とD:/NeNeFolio/agents/filter-focus-save-20261018/を参照。
最終CI/main・通常exe・更新候補・起動/整理の実結果は同所completion.jsonを確認する。

新しい確認先はD:/NeNeFolio/check/2026-10-18/342-beta-refresh/。稼働中の#338候補は未保存本文を含むため操作しない。
物理IME 1はhideの暫定所感、3は未確定中切替なし。人の読み上げ聴取は未受理。
カテゴリ色はテーマと独立した識別色。最初の候補の青は確認データにサナが指定した色であり、仕様変更はしていない。
公開版v0.1.0/候補0.2.0。公開tag/Releaseなし。schema・依存・ゲート変更なし、Waivers: none。
既存の対象core/OOM/UIA/reader/性能と#340のDPI成功結果は再実行しない。
以前の一時物整理の残留は各completion.jsonを参照し、拒否済み操作を再試行しない。
