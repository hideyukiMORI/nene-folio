# いまのタスク — NeNe Folio

記録日: 2026-10-23。hideの依頼による日報・引き継ぎの確定は[#354](https://github.com/hideyukiMORI/nene-folio/issues/354)。
GUI統一は[#348](https://github.com/hideyukiMORI/nene-folio/issues/348)（部品体系と状態）と[#349](https://github.com/hideyukiMORI/nene-folio/issues/349)（操作帯と保存状態）をmain `c78a398`へ統合済み。
[#350](https://github.com/hideyukiMORI/nene-folio/issues/350)（メニューの器・失敗の一本化）は `feat/350-menu-surface` で実装・検証中。
[#351](https://github.com/hideyukiMORI/nene-folio/issues/351)（面の規則）は設計を実寸とnative挙動で確認中、製品未着手。順序はI-C統合→I-D。
[#356](https://github.com/hideyukiMORI/nene-folio/issues/356)（空状態で描画と配り直しが自己ループし入力が届かない退行）をmain `5457c48`へ統合済み。
最新確認先は`D:/NeNeFolio/check/2026-10-19/356-empty-state/NeNeFolio-0.2.0-win-x64/NeNeFolio.exe`。`check/2026-10-19/349-action-bar`の候補は文書を開いていない間固まるので起動しない。稼働中アプリや未保存本文を操作しない。

再開時は[引き継ぎ書の最新節](../handoffs/2026-10-19.md)を読み、実Issue/PR・checkoutと照合する。
当日の到達点は[日報](../reports/2026-10-19.md)、対象検証は[I-A](../quality/2026-10-18-ui-part-styles.md)と[I-B](../quality/2026-10-18-action-bar-and-save-state.md)の品質記録。
設計の正本は[ADR0060](../adr/0060-ui-part-styles-and-state-rule.md)。議論・文献・撮影・下書きは`D:/NeNeFolio/agents/ui-consistency-20261018/`、
完了結果は`D:/NeNeFolio/agents/348-ui-parts/`・`349-action-bar/`・`356-empty-state/`の各`completion.json`。候補確認は起動直後・入力なし・20秒の主スレッドCPUを必ず測る（#356の教訓）。
文書だけの変更では既存の製品検証・通常exe・配布候補を維持する。

hideの2026-10-23の分担指定は、Astraの設計サナが判断・設計・統合、SOL/LUNAの実装サナが実装・調査・測定を担当し、ClaudeCodeの /design を使うデザインリナと議論する形。今回のGUI統一の再開範囲に適用し、旧モデル指定は引き継がない。
hideの確定: 閲覧/編集は右上、新規はヘッダー左、橙は変更ありの保存とダイアログの確定だけ、破壊的操作は確認なし、操作▾は自前の面へ、余白は窓の縁16・枠の内側8。
全ノート検索EDITのUIA Nameは[独立native検証](../quality/2026-10-23-native-edit-uia-probe.md)で正常と確認。旧未達は検証client内のmanaged UIA初期化によるもので、製品修正は不要。物理IME・人の聴取・10,000件一覧の描画負担は従来どおり未測定/未改善。
公開版v0.1.0 / 未公開候補0.2.0。schema・依存・ゲート変更なし、Waivers: none。
今回の追加作業木・一時出力はD側に置き、統合・必要反映・証跡収載の後、絶対パス・取込・唯一の成果物・稼働参照・リンクを確認して整理する。旧記録の手動ワンライナー運用は今回の終了条件にしない。
#336/#338/#340/#342/#348/#349の旧一時物は今回の対象外として保持。
