# いまのタスク — NeNe Folio

記録時点: 2026-10-12 23:14 JST。製品基点main `1fe120cbadc6909af43ee009b7cf8cb50763e250`、公開tag `v0.1.0`。
これは状態の要約で、作業許可の正本ではない。hideの現在指示、checkout、Issue/PRの実状態を照合する。

## 現在の到達点

- [Issue #294](https://github.com/hideyukiMORI/nene-folio/issues/294)はclosed、[PR #295](https://github.com/hideyukiMORI/nene-folio/pull/295)はmerged。
  NeNeNibと同じ125テーマを追加し、既存light/darkとsystemを含む128候補を選択・保存できる。
  settings.jsonは版4、旧版1〜3を読める。契約は[ADR0049](../adr/0049-builtin-theme-catalog.md)。
- 最終headの[必須CI](https://github.com/hideyukiMORI/nene-folio/actions/runs/38258940750)、main統合、通常exe反映/起動、
  追加作業木/一時出力の整理まで完了。対象検証と未測定条件は[品質記録](../quality/2026-10-12-theme-catalog-checks.md)、
  完了証跡は`D:/NeNeFolio/agents/294-theme-catalog/completion.json`。Waivers: none。
- 現在の依頼は日報と引き継ぎの整備（[Issue #296](https://github.com/hideyukiMORI/nene-folio/issues/296)）。
  製品変更は含まない。文書PRの最終CI/main/整理結果は同Issueに紐付くPRと
  `D:/NeNeFolio/agents/296-night-report/completion.json`を参照する。

## 製品と次回の入口

- 通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、製品基点`1fe120c`。
  公開済み[v0.1.0ベータ](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.1.0)の基点`9e93ae1`とは異なり、
  公開版には後続の設定/カテゴリ/表示/テーマ更新を含まない。今回Releaseは差し替えていない。
- 最新の到達点は[当日日報](../reports/2026-10-12.md)、再開に必要なexe hash・証跡・判断条件は[引き継ぎ](../handoffs/2026-10-12.md)の先頭節。
  操作は[キー表](../KEY_BINDINGS.md)、機能は[README](../../README.md)、要件は[SPECIFICATION](../../SPECIFICATION.md)。
- 「両方」の文言と2つのトグル案は相談のみ。確定タスクにせず、次の変更はhideの現在指示から選ぶ。
- 全125配色の個別目視、物理IME、別OS/DPI、スクリーンリーダー、#294のhide実機受理は未測定。#292の未測定区分も維持する。
- #243はcodepage65001なしでU+FFFDと隣接VTを保てる新しい根拠が再開条件。
  旧C側41作業木と`D:/NeNeFolio/worktrees/210-rename-state`の整理は従来のhide判断待ち。

[運用](../DEVELOPMENT_WORKFLOW.md#10-エージェントの運用)に従い単体実行を既定とし、必要な独立レビューのみ読み取りで使う。
過去の停止・許可・一時的な担当分担を新しい実行の根拠にしない。
