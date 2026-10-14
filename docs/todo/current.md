# いまのタスク — NeNe Folio

記録日: 2026-10-14。開始main `b239027dd8e29fa8c1bb7a51e0d560ff2b33641d`、公開tag `v0.1.0`。
これは状態の要約で、hideの現在指示・Issue/PR・checkoutを優先する。

## 現在の到達点

- hideの継続依頼でGUIレビュー最優先のドロワー新規入口を[Issue #302](https://github.com/hideyukiMORI/nene-folio/issues/302)として実装した。
  常設「＋ 新しいノート」とカテゴリ右クリックが対象付きの共通作成へ合流する。
  成功時だけ本文を表示し、取消/失敗では本文・Undo・検索語・一覧の幅を保つ。[ADR0051](../adr/0051-drawer-new-note.md)。
- Debug build/tidy、文言/コマンドunit、規約検査0違反、空白確認と、実Win32の通常/カテゴリ0件/絞り込み・最小サイズを確認済み。
  [品質記録](../quality/2026-10-14-drawer-new-note-checks.md)、[日報](../reports/2026-10-14.md)、[引き継ぎ](../handoffs/2026-10-14.md)を参照。
- 最終headの必須CI、main統合、通常exe反映、今回の作業木/outputs整理の実結果は#302のPRと
  `D:/NeNeFolio/agents/302-drawer-new-note/completion.json`が記録する。PRがMERGEDで同記録が完了なら、本枝を再開しない。
  枝は`feat/302-drawer-new-note`。完了後も枝・commit・agentsの恒久証跡を保持する。

## 次回の判断条件

- GUIレビューの残りはノート内検索、名前フォーム、設定/ヘルプ、目的別メニュー。
  条件は[10月13日の引き継ぎ](../handoffs/2026-10-13.md)と
  `D:/NeNeFolio/agents/gui-review-20261013/NeNeFolio-GUI-review-20261013.html`にある。
  現在のhide指示で次の焦点を決める。既定は単体実行で、以前の分担を継承しない（ADR0043）。
- 物理IME/実打鍵、UIA/実読み上げ、別OS/DPI、全テーマと実利用者評価は未測定。
  #302は合成入力・120 DPIの実Win32であり、hideの実機受理とは区別する。
- 通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`。反映と最終hashはcompletion.jsonを確認する。
  更新時は起動状態を取り直し、正典prepare-real-machine.ps1の保護を使う。未保存内容を持つアプリを強制終了しない。
  公開[v0.1.0](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.1.0)には後続UIと#298/#302を含めていない。
- #243はcodepage65001なしでU+FFFDと隣接VTを保てる新しい根拠が再開条件。
  旧C側41作業木と`D:/NeNeFolio/worktrees/210-rename-state`の整理判断は別件。

成功済み対象検証は関係入力が不変なら再利用し、最終headの必須CIを維持する（QLT-012 / ADR0044）。Waivers: none。
