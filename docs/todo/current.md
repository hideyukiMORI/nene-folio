# いまのタスク — NeNe Folio

記録日: 2026-10-25。[#379](https://github.com/hideyukiMORI/nene-folio/issues/379)のメニュー文字階層の統一。
最終状態はIssue/PRと恒久証跡の`completion.json`を照合する。現在のhide指示とcheckoutを正とする。

## 現在の到達点

- hide指定のOpusによるdesign-critiqueと現行画像/実測を基に、メニュー項目16を既存の14 logical pxへ揃えた。
- ADR0060決定17にサイズを追記し、`folio_window`の一つのmenu_font生成で操作/右クリックの全MENUへ適用する。
  言語別face・キー列11・行高32・余白・色・枠は維持する。
- 調査/実装/検証は単体。変更前の明示依頼による批評を再利用し、軽微な差分でレビューを繰り返さない。
- 作業checkout: `D:/NeNeFolio/worktrees/379-menu-typography`、出力: `D:/NeNeFolio/outputs/menu-typography-20261025`。
  恒久証跡: `D:/NeNeFolio/agents/menu-typography-20261025`。現存状況は完了記録を照合する。
- 詳細: [品質記録](../quality/2026-10-25-menu-typography.md)。保存schema・依存・ゲート変更なし、Waivers: none。

## 次の判断

最終headの必須CI成功後、`tools/merge-pr.ps1`で統合し、`tools/prepare-real-machine.ps1`で通常exeへ反映する。
稼働中の実機アプリはhideが閉じてから更新し、自動起動しない。最終結果は実際のPRと`completion.json`が正。
#377/PR #378はmain統合・通常反映・当該worktree/出力整理まで完了。#369の大量ノート初回読込は別件で未着手。
今回の確認はメニューの文字と直接影響する表示境界に限定する。
## 運用と保持

稼働中アプリ/未保存本文を終了しない。Computer Useや実画面の入力は使わず、公開Releaseは作らない。
統合/必要反映/恒久成果物を確認後、このtaskの追加worktreeと不要出力を整理し、branch/commitを保持する。
下記の旧taskは削除が自動承認レビューに拒否されて未削除であり、再試行/別手段での削除をしない。

- #368: `D:/NeNeFolio/worktrees/368-ui-latency`、`D:/NeNeFolio/outputs/performance-20261024`。
- #373: `D:/NeNeFolio/worktrees/373-gui-responsiveness`、`D:/NeNeFolio/outputs/gui-responsiveness-20261025`。

過去の自動承認レビューが再帰削除を拒否した9箇所は再試行/別手段の削除をしない（すべて `D:/NeNeFolio/outputs/` 配下）:
`350-menu-surface`、`350-release-20261024`、`351-native-name`、`351-dialog-feasibility`、`351-surface-rules`、`351-release-20261024`、
`364-theme-cursor`、`theme-navigation-20261024`、`366-theme-preview`。
旧#336/#338/#340/#342/#348/#349の一時物は今回対象外。旧 `check/2026-10-19/349-action-bar` は空状態で固まるため起動しない。
物理IME・実reader・物理DPI移動・DWM窓外影は未測定。
