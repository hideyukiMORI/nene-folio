# 状態遷移図の開始と終了の点の品質記録 — #876

2026-11-17。開始main `9d789d6`（#872統合後）。#872の品質記録で気づいた見た目の差（開始・終了がふつうの円の大きさ）の調整。
作業木E:/NeNeFolio/worktrees/876-state-dots（branch feat/876-state-dots）、対象実行の原本E:/NeNeFolio/outputs/876-state-dots、画像D:/NeNeFolio/agents/state-dots-20261117。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 変更

- 形の種類に`DIAGRAM_STATE_START`（線の色で塗った小さな点、18）と`DIAGRAM_STATE_END`（小さな輪、26）を足した。寸法は文字の大きさに依らない（`diagram_geometry_node`の入口で決め、文字に合わせる形は`labelled_size`へ分けた）。
- 配置が開始を塗りの点（面の色の役割に線＝accentを使う）で出し、終了は輪と中の点の2つの描画要素で出す。描画（adapters/win32）は楕円を描くだけ。新しい色の役割は無い。
- `mermaid_state`の`[*]`が新しい形を使う。フローチャートの円・二重円は変えない。

## 試行

| 試行 | 結果 |
| --- | --- |
| build-01 / 02 | clang-tidyの認知的複雑度（`diagram_geometry_node`・試験の`state_dots`が11）。入口と文字に合わせる寸法を分け、試験は1要素を数える関数へ分けた |
| unit-01 / 02 | 成功。#872の試験の期待（円・二重円）を新しい形へ直し、配置の描画要素（点2つ・輪1つ・大きさ・塗り）と寸法が文字に依らないことを足した |
| 描画 | `diagram_render_tests` 533 checks成功。`visual/state-{light,dark}.png`で、開始が塗りの点、終了が輪と点になり、図が縦に詰まったことを目で確かめた |

## 最終の対象検証

原本は`v1-*.log`（`E:/NeNeFolio/temp/876/verify.ps1 -Tag v1`、#874統合後のmainを取り込んだ後）。build（clang-tidy込み）・`eng/coverage.py`（分岐90.87%、閾値・除外不変）・
`eng/symbols.py`・`eng/conformance.py`（source/`--build-dir`）・`git diff --check`が成功し、`ctest`は61件すべて成功（153.6秒）。
