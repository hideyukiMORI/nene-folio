# 見出しの中の行内の装飾の大きさの品質記録 — #853

2026-11-17。hideの報告「Hタグの中に強調か何かの装飾を入れるとフォントサイズが小さくなるのを見かけた」。
作業木E:/NeNeFolio/worktrees/853-heading-size（branch fix/853-heading-inline-size、#852の枝から開始し統合後のmainを取り込む）、
対象実行の原本E:/NeNeFolio/outputs/853-heading-size（完了後はD:/NeNeFolio/agents/heading-size-20261117へ移す）。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 調査

隔離desktopのnative pane（`minimap_structure_tests`）へ、見出しh1〜h6とsetext h1の7通り × 行内の装飾9通り
（太字・斜体・取消・太字斜体・code span・表示名付きリンク・`<URL>`・囲まないURL・文字参照）を読み込み、
装飾の前の文字と装飾の中の文字の`yHeight`（twips）を`EM_GETCHARFORMAT`で比べた（investigate-01.log）。

| 装飾 | 結果 |
| --- | --- |
| code span | **すべての見出しで200（10pt）**。h1 440・h2 340・h3 280・h4〜h6 220・setext h1 440に対して小さい |
| それ以外の8通り | すべて見出しと同じ大きさ |

原因: 行内の出力`emit_code`がcode spanを`{\f1\fs20\cf4 ...}`と固定の10ptで書いていた。周りの文字の大きさを知る手段が無かった。
本文（11pt）では「一回り小さい等幅」という意図どおりだが、見出しの中では大きく縮んで見える。

## 直し

- `markdown_render`に周りの文字の大きさ（RTFのhalf-points）を足し、見出しの段落は見出しの大きさ、本文・表のセルは22を渡す。
  見出しの大きさの表は`text_half_points`の一か所にまとめた（見出しの書式と行内の出力が同じ値を使う）。
- code spanは本文と同じ比（20:22）で周りに合わせる: h1 40・h2 31・h3 25・h4〜h6と本文20（本文は従来どおり）。
  GitHubも見出しの中のcodeを見出しに合わせて縮小表示するので、同じ考え方にした。比の値は見た目の判断で、hideの確認で変えられる。
- 編集表示・原文・保存・版は不変。

## 試行

| 試行 | 結果 |
| --- | --- |
| investigate-01 | 上の表（code spanだけ200） |
| build-02 / 03 | clang-tidyの認知的複雑度（試験の二重の繰り返し）。1件を調べる関数へ分けた |
| unit-02 | 1件失敗: 行内の出力を直接呼ぶ単体試験が`markdown_render`を新しい項目なしで作り、code spanが`\fs0`になった。試験の7か所に本文の22を渡した |
| native-02 | 成功（716 checks）。63通りすべてで装飾が見出しの大きさ（codeは比どおり） |
| unit-03 | 成功。見出し・setext・表のセル・本文のcode spanのRTFの大きさを追加 |
| 実画面 | `--bare-url-visual`の見本へ「## 見出しの `code` と **太字**」を足して採取（visual/after-*.png）。見出しの中のcodeが見出しの大きさになった。等幅の書体がConsolasのままであることも`szFaceName`で確かめた（一時的な出力、確認後に外した） |

## 最終の対象検証

原本は`v1-*.log`（`E:/NeNeFolio/temp/853/verify.ps1 -Tag v1`、#852統合後のmainを取り込んだ後）。

| コマンド | 結果 |
| --- | --- |
| `cmake --build <E:build> --parallel 4`（全target・clang-tidy込み） | 成功 |
| `python -B eng/coverage.py` | 成功。分岐90.88%（下限90・除外不変） |
| `python -B eng/symbols.py --build-dir <E:build> --require core application` | 成功（違反0） |
| `python -B eng/conformance.py` / `--build-dir <E:build>` | 成功（違反0） |
| `ctest --test-dir <E:build> --output-on-failure --no-tests=error` | 61件すべて成功（88.6秒） |
| `git diff --check origin/main` | 成功 |
