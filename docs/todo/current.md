# いまのタスク — NeNe Folio

記録日: 2026-10-25。[#381](https://github.com/hideyukiMORI/nene-folio/issues/381)のメニュー操作文字の統合。
最終状態はIssue/PRと恒久証跡の`completion.json`を照合する。現在のhide指示とcheckoutを正とする。

## 現在の到達点

- メニューはノート名ではなく、同じ操作を並べるヘルプ「操作」の役割へ揃える。hideの「その方向性で対応して」に基づく。
- ADR0060決定17を補正し、MENUの項目名/キー列を既存mono_fontのConsolas11・標準の太さ・共通字間へ統合した。
  専用menu_fontと専用幅計測を撤去し、下線位置と省略時の下線も共通計測へ合わせた。行高32・余白・色・枠は維持する。
- 調査/実装/検証は単体。変更前の明示依頼によるOpusの画像批評を再利用した。
- Debug/tidy・規約検査・3言語/3倍率の4,947 checks・比較画像13枚を確認済み。
  一覧320幅では英語5条件に省略が残り、等幅/字間によって長い項目の必要幅は増える。全文UIAを保持し、孤立した下線は解消した。
- 作業checkout: `D:/NeNeFolio/worktrees/381-menu-operation-style`、出力: `D:/NeNeFolio/outputs/menu-operation-style-20261025`。
  恒久証跡: `D:/NeNeFolio/agents/menu-operation-style-20261025`。現存状況は完了記録を照合する。
- 詳細: [品質記録](../quality/2026-10-25-menu-operation-style.md)。保存schema・依存・ゲート変更なし、Waivers: none。

## 次の判断

最終headの必須CI成功後、`tools/merge-pr.ps1`で統合し、`tools/prepare-real-machine.ps1`で通常exeへ反映する。
稼働中の実機アプリはhideが閉じてから更新し、自動起動しない。最終結果は実際のPRと`completion.json`が正。
#379/PR #380はmain統合・通常反映・当該worktree/出力整理まで完了。#369の大量ノート初回読込は別件で未着手。
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
