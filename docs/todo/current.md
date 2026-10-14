# いまのタスク — NeNe Folio

記録日: 2026-10-14 21:45 JST。製品基点 `0b080f5919e1a5c7d08a66d4eb339dd98e54c8e1`、公開tag `v0.1.0`。
これは状態の要約で、hideの現在指示・Issue/PR・checkoutを優先する。

## 現在の到達点

- ドロワー新規入口の[Issue #302](https://github.com/hideyukiMORI/nene-folio/issues/302)は
  [PR #303](https://github.com/hideyukiMORI/nene-folio/pull/303)で統合済み、CLOSED。
  最終headの必須CI成功・通常exe反映・当該作業木/outputs整理まで20:37 JSTに完了した。本枝を再開しない。
- 振る舞いは[ADR0051](../adr/0051-drawer-new-note.md)、検証は[品質記録](../quality/2026-10-14-drawer-new-note-checks.md)、
  完了実測と次回条件は[日報](../reports/2026-10-14.md)・[引き継ぎ](../handoffs/2026-10-14.md)を参照。
  枝・commit・`D:/NeNeFolio/agents/302-drawer-new-note/`の恒久証跡は保持している。
- 現在の依頼は日報と引き継ぎ書の作成。[Issue #304](https://github.com/hideyukiMORI/nene-folio/issues/304)は文書3件だけの更新。
  統合実結果は紐付くPRと`D:/NeNeFolio/agents/docs-handoff-20261014/completion.json`を照合する。

## 次回の判断条件

- GUIレビューの残りはノート内検索、名前フォーム、設定/ヘルプ、目的別メニュー。
  優先順位と条件は[同日の引き継ぎ](../handoffs/2026-10-14.md)にまとめた。
  現在のhide指示で次の焦点を決める。既定は単体実行で、以前の分担を継承しない（ADR0043）。
- 物理IME/実打鍵、UIA/実読み上げ、別OS/DPI、全テーマと実利用者評価は未測定。
  #302は合成入力・120 DPIの実Win32であり、hideの実機受理とは区別する。
- 通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`へ反映済み。hashは日報と#302のcompletion.jsonを参照。
  更新後の起動・hide実機受理は未確認。
  更新時は起動状態を取り直し、正典prepare-real-machine.ps1の保護を使う。未保存内容を持つアプリを強制終了しない。
  公開[v0.1.0](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.1.0)には後続UIと#298/#302を含めていない。
- #243はcodepage65001なしでU+FFFDと隣接VTを保てる新しい根拠が再開条件。
  旧C側41作業木と`D:/NeNeFolio/worktrees/210-rename-state`の整理判断は別件。

成功済み対象検証は関係入力が不変なら再利用し、最終headの必須CIを維持する（QLT-012 / ADR0044）。Waivers: none。
