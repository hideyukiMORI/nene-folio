# 品質記録 — 2026-11-22 配色の比較とリンクの宛先の読みに、直接の単体の試験を足す（#1157）

中核の分岐のカバレッジ（QLT-009、下限90%）は、11月22日の時点で90.8%前後で、余裕が分岐で約120個しか無かった。閾値と除外は触らず（QLT-010）、通っていない分岐のうち、**意味のある試験で埋められる所**に試験を足した。製品コードは変えていない。

## どこを選んだか

道具: D:/NeNeFolio/temp/20261122-uncovered.py（測定の出力`out/coverage/complete.json`から、片側しか通っていない分岐の行を、ファイルごとに並べる）。

| ファイル | 前 | 通っていなかったもの | 足した試験 |
| --- | --- | --- | --- |
| `src/core/theme_palette.c` | 60.5%（46/76） | 配色の比較の、欄ごとの「違う」の側。4つの構文の色しか確かめていなかった | `tests/unit/code_syntax_tests.c`の`colors`: 32の色を1つずつ変えて「違う」と答えること、暗幕の濃さ、コントラスト。色の数が構造体の大きさと合うことも確かめる（欄を足して一覧に入れ忘れると落ちる） |
| `src/core/markdown_destination.c` | 74.5%（79/106） | リンクと画像の宛先の読み（山括弧・題・逆斜線・文字の参照・範囲）。直接の試験が無く、描画の試験を通して一部だけ通っていた | `tests/unit/markdown_tests.c`の`verify_destination_shapes`（21通り）・`verify_destination_decoding`（逆斜線で打ち消せる字と打ち消せない字、文字の参照、範囲の端で切れた参照、宛先の範囲）・`verify_destination_allocations`（確保の失敗） |

選ばなかったもの: `src/core/folio_command.c`（84.5%）の通っていない分岐の多くは、`switch`の`case`を並べた行で、1つずつが分岐に数えられているもの。試験で埋めても意味が薄い。

## 見つけたこと

- 宛先の `a\ b`（逆斜線・空白）が、1つの宛先として受け入れられる。CommonMarkでは、空白は逆斜線で打ち消せないので、リンクにならない形のはず（席の読み。参照の実装では確かめていない）。**製品の動きは変えず**、[#1158](https://github.com/hideyukiMORI/nene-folio/issues/1158)に事実を書いた。試験は、いまの動きを注記つきで固定している（決めたら、その行を直す）。

## 変異（8本。記録 D:/NeNeFolio/temp/20261122/mutations-1157.log）

足した試験が効くかを、製品コードを1か所ずつ変えて確かめた（変えたものは戻した）。**8本のうち6本で、足した試験が落ちた。**

| 変異 | 結果 |
| --- | --- |
| T1 枠の色を比較から抜く | every color takes part in palette equality |
| T2 暗幕の濃さを比較から抜く | the backdrop's opacity takes part |
| T3 本文の見出しの色を比較から抜く | every color takes part in palette equality |
| D1 閉じていない山括弧を宛先にする | **落ちない**: 山括弧の後の題の確かめが、同じ入力を別の理由で断る（結果が同じ）。閉じていない山括弧は、どちらでも宛先にならない |
| D2 引用符1文字の題を認める | text that is no destination（`a.png "`） |
| D3 題の前の空白を要らなくする | text that is no destination（`<a b>"title"`） |
| D4 逆斜線が、文字も打ち消す | **走っていない**: 変異で使われない関数ができ、構築が落ちた |
| D5 参照を、範囲の端を越えて読む | a reference cut by the end of the span stays as it is written |

## 実行した検証（作業木 E:/NeNeFolio/worktrees/955-pie）

- `ctest -R folio_unit`: 成功。
- `python eng/coverage.py`: 成功。全体 91.25%（13498/14792。この枝の元のmainは90.88%）。`theme_palette.c` 100%、`markdown_destination.c` 98.1%。
- `python eng/conformance.py --build-dir build`: 違反0。全`.c`/`.h`の整形の確かめ: 違反0。
- 全試験は流していない（変えたのは単体の試験の2ファイルだけ。必須CIが流す）。

## 次の候補

同じ道具で見ると、通っていない分岐が多く、意味のある試験で埋められそうなのは: `src/core/markdown_source_lookup.c`（84.9%）、`src/core/mermaid_gantt_axis.c`（84.9%）、`src/application/image_export.c`（82.1%）、`src/core/rename_plan.c`（87.5%）。いちばん数が多いのは`src/application/folio_state.c`（約87%、通っていない分岐が約340）。
