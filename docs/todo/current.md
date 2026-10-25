# いまのタスク — NeNe Folio

記録日: 2026-10-25。[#383](https://github.com/hideyukiMORI/nene-folio/issues/383)の本文表示倍率。
最終状態はIssue/PRと恒久証跡の`completion.json`を照合する。現在のhide指示とcheckoutを正とする。

## 現在の到達点

- hideのCtrl+ホイール/Ctrl++/Ctrl+-依頼を受け、ADR0067を先に定めた。
- 閲覧/編集本文だけを50～300%・10%刻みで拡大縮小し、Ctrl+0で100%へ戻す。
  倍率はnote_paneの実行中の状態一つ。RTF再投入後も維持し、行番号font/幅/配置を追従させる。
- 単体で実装/自己レビュー/対象検証を実施した。48条件、入力/再投入/本文/Undo/資源/故障注入の成功結果を確認済み。
  初回fixtureのmodal戻り値期待1件を訂正し、当該キー群96 checks成功。ui_text 66,157 checks成功。
  PrintWindowの文字欠けは実WM_PAINTと分離し、8条件の部分/全面描画比較が差分0だった。
- 詳細: [品質記録](../quality/2026-10-25-note-zoom.md)。保存schema・依存・ゲート変更なし、Waivers: none。
- 作業checkout: `D:/NeNeFolio/worktrees/383-note-zoom`、出力: `D:/NeNeFolio/outputs/note-zoom-20261025`。
  恒久証跡: `D:/NeNeFolio/agents/note-zoom-20261025`。現存状況は完了記録を照合する。

## 次の判断

最終headの必須CI成功後、`tools/merge-pr.ps1`で統合し、`tools/prepare-real-machine.ps1`で通常exeへ反映する。
稼働中の実機アプリはhideが閉じてから更新し、自動起動しない。最終結果は実際のPRと`completion.json`が正。
#381/PR #382はmain統合・通常反映・当該worktree/出力整理まで完了。#369の大量ノート初回読込は別件で未着手。
今回の確認は本文倍率と直接影響する入力・再投入・行番号・描画に限定する。

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
