# いまのタスク — NeNe Folio

記録日: 2026-10-19。hideの依頼による日報・引き継ぎの確定は[#354](https://github.com/hideyukiMORI/nene-folio/issues/354)。
GUI統一は[#348](https://github.com/hideyukiMORI/nene-folio/issues/348)（部品体系と状態）と[#349](https://github.com/hideyukiMORI/nene-folio/issues/349)（操作帯と保存状態）をmain `c78a398`へ統合済み。
[#350](https://github.com/hideyukiMORI/nene-folio/issues/350)（メニューの器・失敗の一本化）と[#351](https://github.com/hideyukiMORI/nene-folio/issues/351)（面の規則）は設計済み・未着手で、順序はI-C→I-D。
最新確認先は`D:/NeNeFolio/check/2026-10-19/349-action-bar/NeNeFolio-0.2.0-win-x64/NeNeFolio.exe`。稼働中アプリや未保存本文を操作しない。

再開時は[引き継ぎ書の最新節](../handoffs/2026-10-19.md)を読み、実Issue/PR・checkoutと照合する。
当日の到達点は[日報](../reports/2026-10-19.md)、対象検証は[I-A](../quality/2026-10-18-ui-part-styles.md)と[I-B](../quality/2026-10-18-action-bar-and-save-state.md)の品質記録。
設計の正本は[ADR0060](../adr/0060-ui-part-styles-and-state-rule.md)。議論・文献・撮影・下書きは`D:/NeNeFolio/agents/ui-consistency-20261018/`、
完了結果は`D:/NeNeFolio/agents/348-ui-parts/completion.json`と`D:/NeNeFolio/agents/349-action-bar/completion.json`。
文書だけの変更では既存の製品検証・通常exe・配布候補を維持する。

hideの分担指定（設計=Fable 5.1の設計席、実装・調査・測定=Opusの実装席、独立レビューは読み取り専用）はこのGUI統一の作業に限る。
hideの確定: 閲覧/編集は右上、新規はヘッダー左、橙は変更ありの保存とダイアログの確定だけ、破壊的操作は確認なし、操作▾は自前の面へ、余白は窓の縁16・枠の内側8。
全ノート検索EDITのUIA Nameは未達（#351）。物理IME・人の聴取・10,000件一覧の描画負担は従来どおり未測定/未改善。
公開版v0.1.0 / 未公開候補0.2.0。schema・依存・ゲート変更なし、Waivers: none。
一時出力の削除はhideの指定どおり手動用ワンライナーの運用。#336/#338/#340/#342/#348/#349の一時物は保持中。
