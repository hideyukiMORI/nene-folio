# いまのタスク — NeNe Folio

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う（「動くもの」は `pwsh -NoProfile -File ./eng/check.ps1` が通ったもの）。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-09（69 ケース・`docs/quality/phase0-results.json`） |
| Phase 1 文書とゲートの足場 | ✅ 2026-09-09（Issue #1・フルゲート緑） |
| Phase 2 negative proof | ✅ 2026-09-09（17 行・`docs/quality/gate-proofs.md`。ruleset と CI の実行証拠は PR #2 で取った） |
| Phase 3 縦切り 1 本 | ✅ 2026-09-09（Issue #3・ADR 0004。core / application / adapters / ui / app と tests/unit。ARC-002 / ARC-003 / ARC-007 / QLT-009 が active） |
| Phase 4 公開 | 🔲 |

## 動くもの

- 起動して、実行ファイルと同じ場所の `data/` を走査し、`categories.json` / `index.json` と照合した索引を枠なし窓の左ドロワーに描く（96 DPI・1 モニタで実機確認。`docs/quality/gate-proofs.md` 第 5 節）
- 台帳が版 1 の形でなければ、理由 1 行を出して終了コード 1（FR-015）
- 中核の分岐 707/744（95.03%）。確保失敗の経路は測定ビルドの注入で全部走る

## 動かないもの

- 右ペイン（RichEdit のビュー／編集・md → RTF）
- カテゴリのトグル・ドラッグの並び替え・右クリックの色変更・検索・スクロール
- 台帳の保存（core の書き手は往復テスト済みだが、起動時に書き戻さない）
- DPI 切り替え・スナップ・縁のリサイズは実装したが実機で未確認

## 次の 1 手

Phase 3 の 2 本目: カテゴリ行のクリックでアコーディオンをトグルし、`categories.json` の `expanded` を保存する（FR-004 / FR-007 の保存側）。
application に `folio_intent` と reducer、adapters に書き込みポートを足す。ドロワーのヒットテストは core の `drawer_layout` に置く。
