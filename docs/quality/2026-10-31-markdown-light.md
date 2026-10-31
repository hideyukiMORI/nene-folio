# 軽いMarkdown拡張と水平線の余白 — 2026-10-31

Issue #480 / ADR0078。基点main `424ae7f`。単体実行。
規則: ARC-001/003/007/008/011/012、C-002/004/005/008/012/013/014/016/017、QLT-008/010/012/013、GIT-001〜004。
Waivers none。保存schema・製品版・実行時依存・ゲートは不変。

## 変更

- `markdown_autolink`が山括弧付きURI/emailの境界を判定し、`markdown_inline_tokens`の同じ出力へ渡す。
  code/autolinkは原文を左から優先し、内部の装飾記号を解釈しない。
- `markdown_character_reference`が数値参照と基本6種のnamed参照を読み、既存UTF-8符号化/RTF escapeへ渡す。
  展開文字を再解析しない。コード・画像原文・autolink内は展開しない。原文保存と編集RTFは不変。
- `markdown_blocks`のruleが背景なし12pt空段落・1ptの線・背景なし12pt空段落を生成する。
  前後段落へ分散していた余白の状態を削除した。閲覧rawの空段落が2つ増えることを保存本文と区別する。
- `CMakeLists.txt`で純Cの2ファイルを既存coreへ収載し、`markdown_tests.c`で構文/境界/原文/確保失敗を確認する。
  対応範囲、未対応の棚卸し、仕様、ADR、current、日報/引継ぎを更新する。

## 検証原本と途中結果

`D:/NeNeFolio/agents/markdown-light-20261031/`、`D:/NeNeFolio/outputs/480-markdown-light/`。
対象実行は`tools/record-run.py`を使い、入力snapshot・raw stdout/stderr・子/記録側の終了値を保存する。
終了時の確定head/CI/main/通常exe/hash/保全整理は同agentsの`integration-completion.json`へ確定する。

| 実行 | 結果 |
| --- | --- |
| `build-01`: `build.ps1`（正典toolchain、CMake Debug/tidy、nenefolio/folio_tests） | 新関数の複雑度制限で失敗。役割を分けて修正 |
| `build-02`: 同上 | 成功。最初の表示候補 |
| `unit-01`: `folio_tests.exe --markdown-rtf` | 成功。ただし後の線/出力変更の最終検証とはしない |
| `compile-visual-baseline-01` | fixtureの`fopen`非推奨で失敗。`fopen_s`へ修正 |
| `compile-visual-baseline-02` | 成功。元note_pane.cをそのまま結び、元archive memberが重複選択されないことをmapで確認 |
| `run-visual-baseline-02` | 呼出しscriptがbaseline引数を文字ごとに分割し、比較条件を誤った。6failは成功扱いしない |
| `run-visual-baseline-03` | 引数修正後、同じexeのhashを照合して再利用。変更前の画像を取得 |
| `compile/run-visual-candidate-01` | native属性と文字検査36checks/0fail。ただし目視で線が太い帯になり不採用 |
| `build-03` | 出力処理の複雑度制限で失敗。文字出力の責務を分けて修正 |

水平線に`sb240/sa240`を直接与えた初案は、背景拡張が余白も塗るというADR0070の制約を満たさなかった。
native属性だけでは視覚品質を証明しないため、実画像で不採用とし、背景なしの空段落へ修正した。
最終候補までの確認結果は次のとおり。下表末尾の05/03が製品入力確定後の結果である。

| 最終候補の実行 | 結果 |
| --- | --- |
| `build-04`: `build.ps1` | 正典Debug/tidy成功。child/wrapperとも0、記録エラー0 |
| `unit-02`: `folio_tests.exe --markdown-rtf` | 対象単体成功。構文、無効境界、長い入力、原文保持 |
| `compile/run-visual-candidate-02`: `probe-driver.py` | 45checks/0fail。4本の1pt線、前後の8つの透明12pt段落、抽出文字とリンク下線、編集原文/cleanを確認 |
| candidate-02 light/dark/narrow PNG | 目視で線が細く上下が空くことを確認。幅の狭い表示も確認 |
| `measurement-run-01`: `measurement.py` | 子0だがartifact名を`folio_tests.exe`と誤指定して記録側125。成功扱いしない |
| `measurement-run-02`: `measurement-existing.py` | 既に生成した`folio_coverage.exe`を再利用し、同じ`--markdown-rtf`を再実行。child/wrapper0、記録エラー0 |

`final-check-01`はconformance成功後、symbol検査で未宣言の`strchr`を検出した。
許可集合を増やさず固定の文字集合走査に直し、直接影響する検証だけを更新した。

| 確定した製品入力の検証 | 結果 |
| --- | --- |
| `build-05` | 正典Debug/tidy成功 |
| `unit-03` | `folio_tests.exe --markdown-rtf`成功 |
| `measurement-run-03` | 正典coverage helperの計装buildと同selectorが成功 |
| `compile/run-visual-candidate-03` | 45checks/0fail。3枚のBMPが目視受理済みcandidate-02とbyte単位で一致 |

上の全runはchild/wrapper0、記録エラー0。計装selector内の構文/metadata/表の確保失敗試験も成功。新しいautolink/文字参照の関数網羅100%、
分岐網羅95.56%/94.83%。変更inlineは91.06%、blockは88.84%で、これは対象selectorの測定であり
全ゲートの代替ではない。最終PR headの必須CIで正典`eng/check.ps1`を要求する。

自己レビュー: ARC-001/003/007/008/011/012は既存純C→RTF経路と借用view・入力検証を維持。
C-002/004/005/008/012/013/014/016/017は閉じたenum分岐、長さ上限、符号化/escape、所有権と
確保失敗を確認。新しい外部効果・抑制・依存・waiver・ゲート変更なし。QLT-008/010/012/013は
対象結果と失敗候補を分離し、成功した最終入力の検証を統合で再利用する。

## 確認範囲

合成Markdownだけを使い、専用desktop内の実RichEditへ製品RTFを投入する。画像は同期WM_PRINTで取得する。
利用者の画面focus/clipboard/実ノートを使わない。明/暗テーマ、狭い窓、native書式/抽出文字、編集原文を確認する。
物理IME・実reader聴取・異なるDPI間の物理移動・DWM呈示は対象外。

完全なCommonMark/GFM互換、全named参照、外部link起動、Mermaid、言語選択/構文色分けは追加していない。
未対応一覧と#479の保留は[対応の整理](2026-10-31-markdown-scope.md)を参照する。
