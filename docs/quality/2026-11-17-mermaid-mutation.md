# Mermaidの読み取りの変異の試験の品質記録 — #912

2026-11-17。開始main `ef2aaef`（#910統合後）。この日に足した図の読み取り（状態遷移図・ER図・クラス図）と既存の2種を、崩した入力で確かめる。製品コードは変えていない。
作業木E:/NeNeFolio/worktrees/912-mutation（branch test/912-mermaid-mutation）、対象実行の原本E:/NeNeFolio/outputs/912-mutation。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 試験

- `tests/unit/diagram_tests.c`の`diagram_mutations`。種は5つ（フローチャート・シーケンス図・状態遷移図・ER図・クラス図、ASCII）。
- 1回ごとに1〜4個の編集（1文字を削る・記法の文字へ置き換える・差し込む）。乱数は固定の種（20261117）の線形合同法で、時刻や`rand`を読まない（ARC-007）。毎回同じ入力の列になる。
- 種ごとに3000回、計15000個。確かめること: 結果が閉じた結果の1つ、模型があるのは受理のときだけ、受理した模型は配置が完成するか拒否され、破棄できる。
- 受理と拒否の両方を通っていること（受理が1000より多く14000より少ない）も確かめる。

## 結果

| 試行 | 結果 |
| --- | --- |
| build-01 / unit-01 | 成功（最初の構築・実行で失敗なし） |
| unit-02 | 成功。`diagram mutations: 15000 damaged inputs, 2164 still accepted and laid out`。Debugのbuildは`-fsanitize=address`・`undefined`・`nullability`付き（compile_commands.jsonで確認）で、検出なし |

途中の手順の誤り: 件数の表示を足すとき、シェルの埋め込み文書が`\n`を本物の改行にしてCの文字列が壊れた（既知の落とし穴）。編集の道具で直した。

見つかった欠陥: なし。範囲と限界: 種はASCIIの短い文書で、編集は1回に4個まで。長い入力・多バイト文字・深い入れ子は、既存の上限の試験が別に確かめている。

## 最終の対象検証

原本は`v1-*.log`（`E:/NeNeFolio/temp/912/verify.ps1 -Tag v1`）。build（clang-tidy込み）・`eng/coverage.py`・`eng/symbols.py`・`eng/conformance.py`（source/`--build-dir`）・`git diff --check`が成功し、`ctest`は63件すべて成功（123.5秒）。
分岐の網羅率は全体90.74%→91.19%（閾値・除外不変）。`mermaid_class.c` 89.87%、`mermaid_er.c` 91.38%、`mermaid_state.c` 92.03%（変異が、書いた例では通らなかった拒否の分岐を通る）。
