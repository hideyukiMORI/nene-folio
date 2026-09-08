# いまのタスク — NeNe Folio

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う（「動くもの」は `pwsh -NoProfile -File ./eng/check.ps1` が通ったもの）。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-09（69 ケース・`docs/quality/phase0-results.json`） |
| Phase 1 文書とゲートの足場 | ✅ 2026-09-09（Issue #1・フルゲート緑） |
| Phase 2 negative proof | ✅ 2026-09-09（17 行・`docs/quality/gate-proofs.md`。ruleset と CI の実行証拠は PR で取る） |
| Phase 3 縦切り 1 本 | 🔲 |
| Phase 4 公開 | 🔲 |

## 動くもの

（まだ無い。製品コードは 0 行。`tests/build/toolchain_smoke.c` は C23 基盤の確認であって製品ではない）

## 動かないもの

（まだ無い）

## 次の 1 手

Issue #1 の PR を Ready にして CI の必須 check を通し、ruleset（PR 必須・必須 check `check`・force push 禁止）を設定して gate-proofs 第 4 節に読み戻す。
そのあと Phase 3 の縦切り Issue（起動して `data/` の 1 カテゴリ・1 ノートをドロワーに描く。core の `drawer_layout` と `categories.json` の読み書きから）を立てる。
