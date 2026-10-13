# いまのタスク — NeNe Folio

記録日: 2026-10-13。製品基点`f432fa7f77c03a5fa549fd0f3bff4bc3d7e11338`、公開tag `v0.1.0`。
これは状態の要約で、作業許可の正本ではない。hideの現在指示、Issue/PR、checkoutを照合する。

## 現在の到達点

- 無題を離れる保存/保存しない/キャンセルは[Issue #298](https://github.com/hideyukiMORI/nene-folio/issues/298) /
  [PR #299](https://github.com/hideyukiMORI/nene-folio/pull/299)で完了。最終headの必須CI、main統合、通常exe反映、当該worktree/outputs整理まで済んだ。
  証跡は`D:/NeNeFolio/agents/unsaved-note-choice-20261013/completion.json`。この枝を再実装しない。
- hideの依頼によるGUIレビューは21画像・3専門家観点・3巡で完了。改善案の実装・実装Issue作成・仕様変更は行っていない。
  成果物は`D:/NeNeFolio/agents/gui-review-20261013/NeNeFolio-GUI-review-20261013.html`。
  撮影用アプリは終了し、一時出力は削除、資料と証跡は保持済み。
- 最後の依頼は日報と引き継ぎ書の更新。[Issue #300](https://github.com/hideyukiMORI/nene-folio/issues/300)で
  [当日日報](../reports/2026-10-13.md)と[引き継ぎ](../handoffs/2026-10-13.md)、本欄を整理する。
  文書PRの統合実結果は#300のPRと`D:/NeNeFolio/agents/docs-gui-review-20261013/`を確認する。

## 次回の判断条件

- 次の実装範囲はhideの新しい指示で決める。GUIレビューの推奨順は、ドロワー新規入口、ノート内検索、名前フォーム、
  設定/ヘルプ、目的別メニュー。対象カテゴリの固定、空カテゴリ/絞込/一覧のみ/未保存取消の条件は引き継ぎ書を参照する。
  提案を承認済みADRや実装許可に読み替えない。
- 調査の担当分担と#298のモデル分担を次へ自動継承しない。既定は現在の対話席で単体実行（ADR0043）。
- GUIレビューは日本語・120 DPIの合成データによる観察。実利用者試験、UIA/実読み上げ、物理Shift+Tab、実IME、
  別DPI、高コントラスト、全テーマの評価は未測定。画像05は取得欠落、設定19→20はスクロール到達済みとして区別する。
- 通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、製品基点と同じソース。
  SHA256 `AA288605F75DE4644D47765863D32071C06A555941F6CA8B7459C54C03E36521`。
  公開済み[v0.1.0](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.1.0)は別で、後続の設定/カテゴリ/表示/テーマと#298を含めていない。
  起動状態は時点依存なので、通常exe反映時に再確認して未保存内容を保護する。
- #243はcodepage65001なしでU+FFFDと隣接VTを保てる新しい根拠が再開条件。
  旧C側41作業木と`D:/NeNeFolio/worktrees/210-rename-state`の整理判断は別件。今回触らない。

成功済み対象検証は関係入力が不変なら再利用し、最終headの必須CIは維持する（QLT-012 / ADR0044）。Waivers: none。
