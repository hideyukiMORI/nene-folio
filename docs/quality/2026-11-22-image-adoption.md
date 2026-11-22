# 品質記録 — 2026-11-22 落とされた画像の取り込みの意図（#1147、ADR0120の段取りの4）

[ADR0120](../adr/0120-image-drop.md)の段取りの4。applicationに、落とされたファイルを1件ずつ`images/`へ取り込み、入れるMarkdownをためる一連を足した。窓からはまだ呼ばない。
段取りの2・3（部品）は[#1134の品質記録](2026-11-22-image-adopt.md)。

## 前提（席の仮決め。hideには未確認）

取り込みをどのスレッドで走らせるかは、11月22日の引き継ぎの「hideに決めてもらうこと」の2だった。hideが一日不在で、「止めるまで続けて」の指示だったので、席の推しの(c)（UIのスレッドで、入力待ちの回ごとに1件ずつ）で進めた。
applicationの一連は「始める・1件進める・結果・閉じる」で、(a)（そのまま同期）に替える場合も、窓の側の呼び方を替えるだけで済む。

## 入れたもの

- `src/application/image_adoption.{h,c}`: 一連の本体。宛先のノートの写し、パスの写し（64個まで）、次に扱う位置、理由ごとの数、ためたMarkdown。
- `image_adoption_start.h`（始めた・断った理由の閉じた値）、`image_adoption_step.h`（1回進めた結果）、`image_adoption_summary.h`（数）。
- `folio_state`: `folio_state_begin_image_adoption`・`_continue_image_adoption`・`_image_adoption_summary`・`_image_adoption_text`・`_image_adoption_lands`・`_close_image_adoption`。
- 憲章の状態の表（ARC-004）に1行。

## 単体の試験（`tests/unit/image_tests.c`の`adoption_*`）

偽の画像のポートの`adopt`を、台本つき（呼ばれた順に結果と名前を返す）にした。台本が無いときは、これまでどおり「何も取り込まない」。

| 試験 | 確かめること |
| --- | --- |
| `adoption_refusals` | 始めていないときの数・文・進める。ノート無し／閲覧表示／0件／始まる／2回目はBUSY／無題。断ったときと始めたときに、ポートを1回も呼ばない |
| `adoption_outcomes` | 9件の混ざった結果: 1回の呼び出しで1件だけ扱う。ポートへ渡るカテゴリとパス。理由ごとの数。名前を返さない「写した」は「書けない」。文は1つずつ改行で区切り、前後に改行が無い。呼んだ側のパスを先に捨てても動く（写しを持つ）。閉じると忘れる |
| `adoption_separators` | 失敗が先頭・あいだ・最後にあっても、余分な改行が入らない。同じ画像を2回落とすと、リンクは2つ |
| `adoption_limit` | 66件: 64件を扱い、2件は数だけ。64件目で終わり、65件目からは読まない。文は64行 |
| `adoption_interrupted` | 3件のうち1件の後に別のノートを開く: 残りを扱わずに終える（`interrupted`）。写した1件は数と文に残り、2件は未処理のまま。**元のノートへ戻っても、入れてよいにはならない。** 閉じるまでBUSY |
| `adoption_left` | 同じノートのまま編集を終える（保存して閲覧へ）: 残りを扱わずに終え、もう一度編集に入っても、入れてよいにはならない。本文を読み直す（`folio_state_reload_document`）: その場で終わる |
| `adoption_allocations` | 長い名前3件の取り込みで、確保を1回ずつ失敗させる（測定ビルドでだけ失敗が起きる）: 始まらないか、最後まで進んで「取り込んだ数＝文の行数」「取り込んだ＋記憶が足りない＋未処理＝3」が保たれる |

## 独立レビューの指摘と対応

読み取り専用の席（同じモデルの新しい席）に、最初のコミット（`ef0e94b`）だけを読ませた。確定の欠陥は無し。危うい所4つ・試験の穴3つ・書き足し2つ。**全部、統合の前に対応した。**

| # | 指摘 | 対応 |
| --- | --- | --- |
| 1 | 中断した取り込みが、元のノートへ戻ると「入れてよい」に戻る（落とした位置は古い） | 中断を一連が覚える（`displaced`）。一度離れたら`lands`は偽のまま。試験を足した |
| 2 | 同じ名前のノートの本文が入れ替わっても（読み直し）、「入れてよい」のまま | `folio_state_reload_document`が一連を止める。ヘッダに「始めてから入れるまで、本文への入力を止めておくこと」と書いた。試験を足した |
| 3 | ためた文が伸ばせなくなった後も、残りのファイルを`images/`へ写し続ける（最大63枚の、リンクの無い写し） | 伸ばせなくなったら、残りを写さずに終える（未処理に数える）。測定ビルドの確保の試験が、文が伸びる道を通るようにした |
| 4 | `lands`が、範囲を確かめずに一覧を引く | `folio_state_resolve_note`（範囲を確かめる、既存の道）で引き直して、いまの選択と比べる形にした |
| 5 | ノートを写し取れなかった理由を、どれも「記憶が足りない」と答える | 記憶不足だけをそう答え、ほかは「ノート無し」 |
| 6 | 先頭が失敗のときの改行の試験が、測定ビルドでしか走らない | `adoption_separators`を足した |
| 7 | 中断の試験が、名前の違いしか確かめていない | 編集を終える・読み直す・元へ戻る、を足した（`adoption_left`ほか）。カテゴリの改名の途中は足していない（下の「やっていないこと」） |
| 8 | 文が伸びる道を通っていない。64件の文を確かめていない | 確保の試験の名前を長くした。64件の行数を確かめるようにした |
| 9 | 窓の側への注意（中断は`pending`で分かる。どの終わり方でも閉じる） | `folio_state.h`に書いた。数に`interrupted`を足した |

## 変異（17本。記録 D:/NeNeFolio/temp/20261122/mutations-1147.log と mutations-1147-2.log）

道具は D:/NeNeFolio/temp/20261122-1147-mutate.py と -mutate-2.py。レビューの対応の後のコードに当てた。**17本のうち15本で、単体の試験が落ちた。**

| 変異 | 結果（落ちた確認） |
| --- | --- |
| A1 上限より1件少なく扱う | 64 files are handled and the rest only counted |
| A2 名前の無い「写した」にリンクを作る | every outcome is counted once; a copy without a name is not stored |
| A3 改行を1つ目の前に入れる | the text is one image per line, with no line break before or after |
| A4 既にある画像にリンクを作らない | 同上 |
| A5 最後の1件で終わらない | the last file finishes the drop and nothing runs after it |
| A6 上限を超えた数を数えない | 64 files are handled and the rest only counted |
| A7 未処理の数が減らない | every outcome is counted once; … |
| A8 文が伸ばせなくなっても写し続ける | **落ちない（想定どおり）**: 確保の失敗は測定ビルドでしか起きない。測定ビルドでは`adoption_allocations`がこの道を通る（分岐の測定で確かめた）が、変異そのものは測定ビルドに当てていない |
| A9 止めた取り込みが、まだ入れられる | coming back to the note does not revive the drop: its position is stale |
| S1 無題を閲覧表示と答える | a drop onto an untitled draft is refused |
| S2 2回目をノート無しと答える | a second drop waits until the first is closed |
| S3 別のノートを開いても続ける | the rest is left alone once the note is no longer the one dropped onto |
| S4b 閲覧表示を受けて、編集表示を断る | a drop onto the reading view is refused |
| S5 ノートの位置を比べない | the rest is left alone once the note is no longer the one dropped onto |
| S6 読み直しても止めない | a reloaded body ends the drop at once |
| S7 ノートを写し取れない理由を、全部「記憶が足りない」と答える | **落ちない**: 選んでいるノートが一覧に無い状態を、単体の試験で作っていない（外から見える形では作れなかった） |
| S8b 編集を終えたことを見ない | the rest is left alone once the note is no longer being edited |

S4とS8の最初の形は、変異で関数の複雑度が上限（10）を超えて構築が落ちたので、「走っていない」。形を変えて流し直したのが S4b・S8b。

## 実行した検証（作業木 E:/NeNeFolio/worktrees/955-pie、`--parallel 4`、ほかの席の構築と同じ端末）

レビューの対応の後のコードで流した。

- `ctest -j 2`: 73件成功（記録 D:/NeNeFolio/temp/20261122/ctest-all-1147.log）。
- `python eng/conformance.py --build-dir build`: 違反0。`python eng/symbols.py --build-dir build --require core application`: 違反0。
- `python eng/coverage.py`: 成功。全体 90.87%（13396/14742、下限90%）。`image_adoption.c`は 86.1%（62/72）。通っていない分岐は、確保の結果が空でないことの二重の確かめ（`_Nullable`の受け）と、扱う位置が空のときの守り。
- 全`.c`/`.h`の整形の確かめ: 違反0。

## 途中の失敗（道具）

- 試験の足し込みで、Bashのheredocと`python -c`にバックスラッシュを含む文を渡して、3回崩れた（Cの文字列の`\n`が本物の改行になる。変異の表が構文エラーになる）。どれも構築か構文の確かめで気づいた。スクリプトをWriteで書いてから実行する形に直した。
- `adoption_left`の最初の形は、偽のディスクの読みを画像の試験用（決まった本文を返す）に差し替えていたため、編集を終える保存が「外で変わった」と断られた。読みを既定に戻した。

## 分かっていないこと・やっていないこと

- 窓からの呼び出し（段取りの5）。1件のあいだ窓が止まる長さの実測。
- カテゴリの改名の途中で始めたときに断ること（`NO_NOTE`）は、単体の試験に入れていない。
