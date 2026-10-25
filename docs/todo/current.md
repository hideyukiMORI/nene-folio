# いまのタスク — NeNe Folio

記録日: 2026-10-25。[#369](https://github.com/hideyukiMORI/nene-folio/issues/369)の初回全文検索中の応答維持。
hideの「#369に着手して」を受け、現在のサナが単体で調査・設計・実装・検証する。
最終状態はIssue/PRと恒久証跡の`completion.json`を照合し、現在のhide指示とcheckoutを正とする。

## 現在の到達点

- 基点main: `fe2b3766207233a5aa4f5152374418008071fffe`。#383/PR #384は通常反映・当該作業物整理まで完了。
- [ADR0068](../adr/0068-idle-index-preparation.md)に沿い、最新語/走査位置をapplicationが所有し、空queueで最大一件を読む。
  完成前は通常索引と準備中表示。取消/保存/索引変更/失敗、既存copy修復、UI寿命を同じ正典経路で扱う。
- 対象単体、21点の確保失敗/再試行、native16条件/221 checks、Debug/Release build/tidyが成功。
  読み取り専用独立レビューのカテゴリ改名時の巻戻し・再入close後通知も修正/再確認した。
- 新規10,000件の初回入力は64,602msから0.467ms。準備中も4,386確認message/4,334paintを処理した。
  候補の全準備は約140秒で、追加描画負荷があるため読込速度の改善とは扱わない。一回の最大継続119.5ms。
- 詳細は[品質記録](../quality/2026-10-25-first-search-responsiveness.md)。
  恒久証跡: `D:/NeNeFolio/agents/first-search-20261025`。
  worktree: `D:/NeNeFolio/worktrees/369-first-search-responsiveness`、出力: `D:/NeNeFolio/outputs/first-search-20261025`。

## 次の判断

最終headの必須CIへ進み、成功後は正規merge経路でmainを統合する。
統合後は通常exeへ反映し、成果物保全を確認してこのtaskのworktree/一時出力を整理する。
通常アプリが稼働中ならhideが保存して閉じた後に反映し、自動起動しない。
CI/統合/通常exe/整理の確定状態は実際のPRと恒久証跡の`completion.json`を照合する。

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
