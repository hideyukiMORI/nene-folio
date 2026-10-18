# いまのタスク — NeNe Folio

記録日: 2026-10-18。hideの「やりやすい順番で進めて」に基づき、Release検索応答→残る実機確認→次ベータ準備を単体で進める。
最初の単位は[#336](https://github.com/hideyukiMORI/nene-folio/issues/336)。製品は変更せず、main 0d983a7のReleaseを測定した。
1,000/10,000件の初回・後続3語・描画・scroll、採用316 raw / 306統計試行の入力/結果/QPC/負荷照合が完了。
10,000件の中央値は初回719.264ms、不一致z 51.820ms、全件一致alpha 211.946ms、一覧描画203.217ms、scroll 206.007ms。
全件一致の時間の大半は描画。画面外行への描画呼出しが次の改善候補だが、このIssueでは最適化していない。
実測条件・失敗/再試行・限界は[品質記録](../quality/2026-10-18-release-response.md)。恒久証跡はD:/NeNeFolio/agents/release-readiness-20261018/。
最終PR/head/CI・archive・追加作業木/一時出力の整理は同所completion.jsonを参照する。

11,000 mdを含む全11,204入力のhashは実行前後一致。通常ノート/通常exeは変更していない。schema・依存・ゲート変更なし、Waivers: none。
成功済みのDebug core/OOM/UIA/reader検証は再実行不要。物理IME・混在DPI・人の聴取は未受理として次の確認へ持ち越す。
公開版はv0.1.0のまま。計測用に作った0.1.0 ZIPは公開済み配布物を置換せず、次版の公開候補にはしない。

#331の検索改善と#334の状態整合は統合済み。hideが手動削除した旧一時出力7フォルダの解消は[引き継ぎ](../handoffs/2026-10-18.md)に記録済み。
古い削除拒否やモデル分担を現在の残留条件・役割指定に戻さない。
次の作業は実Issue/PR・checkoutと現在のhide指示を照合して進める。
