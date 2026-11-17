# 複数行の脚注の品質記録 — #878

2026-11-17。開始main `62e8cda`（#874統合後）。#869（ADR0103）の脚注を、字下げで続く行まで読むよう広げた。
作業木E:/NeNeFolio/worktrees/878-footnotes（branch feat/878-footnote-continuation）、対象実行の原本E:/NeNeFolio/outputs/878-footnotes。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 変更

- `markdown_references`: 脚注の定義の直後の「空白でなく字下げ4桁以上の行」を同じ脚注の続きとして記録する（`continues`、64行まで）。項目に範囲（`extent`）を持ち、`markdown_references_defines`は「その行が定義の範囲の中か」で答える（最後に始まった定義を二分探索）。
- `markdown_references_footnote_text`: 1行目の文と続きの行の内容を、ビューの区切り（段落と同じ行の区切り）でつないだ部分の並びを返す。
- `markdown_blocks`の脚注の行は、その並びを行内の描画へ渡す。続きの行は既存の「定義の行を出さない」判定でそのまま隠れる（判定は表の一か所）。

## 試行

| 試行 | 結果 |
| --- | --- |
| build-01 / unit-01 | 成功（最初の構築・試験で失敗なし） |
| 赤の確認 | `continues`の字下げと空行の条件を常に「続かない」に変えると、新しい試験が「field destination and structure」で落ち、脚注は`1. first`だけになった（unit-red.log）。戻して成功（unit-02）。戻す手順のPowerShellの書き込みが道具の中で失敗したため、該当行をPythonで元の文に戻し、差分の行数で確かめた |
