# コード言語ラベル — #532

## 変更と契約

fence info先頭語を、既存Markdown→RTF経路の同じコード面の先頭段落に表示する。
code_language_wordをcode_language_parseとmarkdown_blocksが共有し、言語の解釈を分岐させない。
本文書体9pt、既存code_comment色、上6pt・左右9pt・本文まで4pt。コード本文はConsolas10ptのまま。
大小文字・alias・未知語は入力どおり、未指定は表示しない。指定付き空fenceはラベルだけになる。
通常の閲覧テキストとして検索・選択・UIAに含め、本文だけのコピーと保存/編集の原文を維持する。
設計は[ADR0086](../adr/0086-fenced-code-syntax.md)。

hide指定によりClaudeのdesign:design-critiqueとdesign:design-handoffを実際に使い、
[canvas](https://claude.ai/artifact/4hmz67GuzYjexqiXyScUoN)の左上D案を採用した。
右寄せと別帯も比較し、読み順とコードとの書体差を優先した。canvas原本・評価・明暗/狭幅/HC画像は
`D:/NeNeFolio/design/532-code-label/`。Claudeはデザインだけ、製品実装と検証はサナが行った。

規則: ARC-001/003/004/005/008/009/010/012、C-002/003/005/008/011/012/013/016、
QLT-008/009/012/013、GIT-001〜004。保存schema・製品版1.1.0・依存は不変。
追加waiverはnone、既存WVR-0001は継続。

## 検証記録

tools/record-run.pyで実行前入力、raw stdout/stderr、終了値、実行後exeを保存する。
一時原本は`D:/NeNeFolio/outputs/532-code-label/`、恒久原本は`D:/NeNeFolio/agents/code-label-20261103/`。
最終head CI・main・通常反映・data照合・保全整理の結果は同agentsのintegration-completion.jsonへ確定する。

- target-03: 初案のcmake対象build、conformance、symbols、folio_tests --markdown-rtf、ctestのminimap_structureが成功。実RichEdit201確認。
- target-04: 採用D案へ書体/余白とnative検証を更新。対象buildは成功したが、作成途中の本品質文書への3参照がconformanceで未解決となり停止。製品テストはこの回では未実行。
- target-05: 本文書作成後、同じbuildのconformance/symbols、build/folio_tests.exe --markdown-rtf、ctest --test-dir build -R '^minimap_structure$' --output-on-failure --no-tests=errorが成功。child/wrapper終了値0、recordingErrorsなし。

対象はinfoの共通抽出、未知/空/大小文字/追加info、RTF escape、container/EOF/長い語、
原文・検索/選択位置、明暗/合成HC、native文字/書体/背景、後続コードと通常本文への書式境界、
狭幅160pxと既存zoom150%、ミニマップの文字/位置/選択/dirty/Undo保持。
確保失敗注入は既存CI測定ビルドへ結ぶ。ローカルで全件回帰は追加しない。
初期target-01/02は新しい表示段落の字下げ/期待RTF不足で失敗し、期待値修正後の03で成功した。

## 限界

ラベルを含めて選択するとコピーにも含まれる。コード本文だけの選択は元の文字列になる。
長い名前はRichEditで折り返す。別の見出しrole/言語選択GUI/コード実行を追加しない。
全OS/DPI/実screen readerやすべての言語書体組合せの実機保証ではない。
隣接コード面の間隔・字句色分けの既存限界、アイコン#533、保留#466/#479は別の範囲。
