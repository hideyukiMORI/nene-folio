# ADR 0058 — 補助面を既存の UIA host と操作状態から公開する

- 状態: 受理（対象検証・実NVDA発話・独立寿命レビューを確認。統合には最終head必須CIを要求）
- 日付: 2026-10-18
- Issue: #321、親 #312
- 規則: ARC-001/002/004/008/010/011、C-002/007/012/018、QLT-008/010/012/013
- 関連: ADR0016、0031、0035、0038、0055、0056

## 決定

1. `NeNeFolioCommandLayer` を既存 host の第3の fragment root とする。
   子の EDIT は既存 native provider と Value/Text を維持し、直接注釈で名前を補う。
   自前の候補、設定、ボタン、ヘルプ、結果だけを同じ node 実装から公開する。
2. `command_surface`、`command_selection`、設定値、検索/置換結果が引き続き正本。
   UIA の Select/Invoke/Toggle は既存の選択・採用・共通コマンドへ入る。
   列挙・名前の取得では本文、Undo、変更印、候補選択を変更しない。
3. 補助面の子参照は型付きの行添字と世代番号を持つ。開閉、面の差替え、候補クエリの変更で
   世代を進め、古い参照は `UIA_E_ELEMENTNOTAVAILABLE` にする。HWND に対応する root は安定。
   イベント中の再入、STA、非同期 Invoke、破棄時の切断は ADR0056 の所有規則を維持する。
4. 名前と矩形は描画用の既存翻訳・整形・配置関数から読む。スクロール外の候補も列挙できるが
   Offscreen と空の矩形を返す。ScrollIntoView は既存の表示位置更新を使う。
   native 入力にフォーカスがある場合に候補へ偽の focus を通知しない。
5. UIA 操作は現在の面と IME、保存、モーダルの可否を実行時にも確認する。
   設定/テーマのモーダル中は背後の native 子窓も無効化する。
   閉じる操作は既存の `surface_return_target` により可視の HWND へ復帰する。
6. 結果・失敗・完了は既存状態から読む polite live region として公開し、表示内容が変わった時だけ
   LiveRegionChanged を送る。UIA イベントを送る目的のタイマーや別の通知履歴を作らない。
   名前入力と標準確認の native provider は維持する。名前入力/保存先カテゴリの不足ラベルも
   `accessibility_native`の一つの直接注釈関数を通す。呼出し元のUI STA内だけで使用し、注釈COM参照は呼出し内で解放する。

## 検証

#321 の3言語、各面のマウスなし操作、0件/長い候補/スクロール、IME、背後操作の拒否、
保存失敗と取消、参照を保持したままの閉鎖、本文/Undo/変更印の不変を native fixture で測る。
実 reader の名前・状態・結果発話を記録し、最終 PR head の必須 CI 成功を統合条件とする。
保存 schema・外部実行時依存・ゲート閾値/除外の変更はない。Waivers: none。

対象の実測と制約は[品質記録](../quality/2026-10-18-accessible-surfaces-checkpoint.md)に収載する。

## 参照

- [UI Automation property identifiers](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-automation-element-propids)
- [UI Automation event identifiers](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-event-ids)

## 2026-10-24 の補正（#351 / I-D）

ヘルプの「操作｜キー」は[ADR0060 決定26と再開時の明確化](0060-ui-part-styles-and-state-rule.md)に従い排他的に切り替える。
既存host/nodeへS2のSelection groupとRadioButton/SelectionItemを追加し、選択は`command_keys_visible`から読む。候補Listの選択とは別containerにする。
scope見出しは非選択Text、候補は既存ListItemで、描画と同じ群順・候補写像を参照する。
キー面では候補と検索EDITを公開・実行せず、切替による既存の世代更新で前の子参照を失効する。独立した選択値・command実行経路・native入力providerは追加しない。
