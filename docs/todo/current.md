# いまのタスク — NeNe Folio

記録日: 2026-10-15。開始main `f23d44c09346b635f0a8bca1a8f584f88c33356c`、公開tag `v0.1.0`。
これは状態の要約で、hideの現在指示・Issue/PR・checkoutを優先する。

## 現在の作業

- hideの継続依頼・全権委譲を受け、[Issue #306](https://github.com/hideyukiMORI/nene-folio/issues/306)で
  ノート内検索を本文上部へまとめる。設計は[ADR0052](../adr/0052-note-search-surface.md)。
  同じ入力面と検索処理を使い、本文を覆わず、件数・前後・閉じるをマウスで操作できるようにする。
- 実装と対象検証は完了。対象Debug/tidy・文言unit・規約検査、実Win32の製品確認344件と空語補足28件を受理した。
  初回の試験側誤判定と未測定条件は[品質記録](../quality/2026-10-15-note-search-checks.md)、
  経緯と再開条件は[日報](../reports/2026-10-15.md)・[引き継ぎ](../handoffs/2026-10-15.md)を参照する。
- 今回はhideがAstra設計、SOL/LUNAに適した実装・調査・検証、Claude Codeのデザイン相談を明示した。
  実装/検証はSOL、設計/受理/統合はAstra。通常の既定や次回へこの分担を自動継承しない（ADR0043）。
- 作業木は`D:/NeNeFolio/worktrees/306-note-search-surface`、build/fixtureは`D:/NeNeFolio/outputs/306-note-search/`。
  検証の恒久証跡は`D:/NeNeFolio/agents/306-note-search/`、設計相談は`D:/NeNeFolio/agents/search-ui-20261015/`。
  実装・対象検証と最終headの必須CI・統合・通常exe反映・整理を区別し、最終結果は紐付くPRとcompletion.jsonを照合する。
  PRがMERGEDでcompletionが反映/整理まで成功していれば本枝は完了で再開しない。

## 確認済みの基点と次の条件

- ドロワー新規入口の#302/PR #303、前回文書の#304/PR #305はCLOSED/MERGED。
  前回の実結果は[日報](../reports/2026-10-14.md)・[引き継ぎ](../handoffs/2026-10-14.md)と各completion.jsonを参照する。
- #306後に残るGUI候補は名前フォーム、設定/ヘルプ、目的別メニュー。
  条件は[10月13日の引き継ぎ](../handoffs/2026-10-13.md)。本件へ一括で混ぜない。
- 物理IME/実打鍵、UIA/実読み上げ、別OS/DPI、全テーマとhide実機受理は未測定。
  通常exe反映時は`tools/prepare-real-machine.ps1`の起動中保護を使い、未保存アプリを強制終了しない。
- 公開v0.1.0の差替え、#243、旧C側41作業木とD側`210-rename-state`の整理は別件。

成功済み対象検証は関係入力が不変なら再利用し、最終headの必須CIを維持する（QLT-012 / ADR0044）。Waivers: none。
