# ER図の多重度の記号の品質記録 — #898

2026-11-17。開始main `ba83d93`（#896統合後）。#882の「多重度は文字」を、線の端の記号へ置き換える。ADR0105の補足。
作業木E:/NeNeFolio/worktrees/898-er-markers（branch feat/898-er-markers）、対象実行の原本E:/NeNeFolio/outputs/898-er-markers、画像D:/NeNeFolio/agents/er-markers-20261117。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 変更

- 矢印の種類に4つ（`DIAGRAM_ARROW_ER_ONE` / `_ZERO_ONE` / `_MANY` / `_ZERO_MANY`）。描画はadapters/win32の`arrow_er`（縦棒・鳥の足・丸）。矢印の1.6倍の大きさ。
- `mermaid_er`: 左右の多重度を辺の始点・終点の印にし、ラベルは語だけ（語が無ければラベルなし）。多重度の文字の行と、そのための文字の組み立てを削除。
- 配置: 印を横に並べる対象（#890）にER図の4つを足した（閉じた`switch`にした）。

## 試行

| 試行 | 結果 |
| --- | --- |
| build-01 / unit-01 | 成功（最初の構築・試験で失敗なし）。模型の期待を、印とラベルが語だけ・語が無ければ空、へ直した |
| 描画1 | 563 checks成功。目で見て、記号は正しいが矢印と同じ大きさ（10px）では小さく読みにくい |
| 大きさ | ER図の記号だけ1.6倍にした |
| 描画2 | 成功。`visual-02/er-{light,dark}.png`で、縦棒2本・丸と鳥の足・縦棒と鳥の足・破線を明暗で目で確かめた |
