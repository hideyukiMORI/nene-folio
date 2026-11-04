# Mermaidラベルの表記同値性 — #585

文字参照と改行/引用表記が、用途の違うラベルでも同じ文字列と画像になるかを確認した。
9用途×7表記×明暗の126件で、実対象のmodel labelが期待UTF-8と一致し、基準18画像に対する変形108画像の全SHA256が一致した。
製品不具合は再現せず、製品修正を行わない。

## 根拠と正典経路

既存diagram_statementは文字参照中のセミコロンを文の区切りにせず、diagram_label_storeは
既存markdown_character_referenceで数値/基本名前付き参照を復元する。生成modelを同じ実font/native rendererへ渡す。
第二のdecoder/rendererや新しい製品入口を作らない。

公式の[flowchart entity節](https://mermaid.js.org/syntax/flowchart.html#entity-codes-to-escape-characters)と
[sequence entity節](https://mermaid.js.org/syntax/sequenceDiagram.html#entity-codes-to-escape-characters)で、
Mermaidの10進数値・名前付き参照と、sequence本文のセミコロンを文字参照で書く方法を確認した。
閲覧時の公式ページは12.3.0を表示しているが、製品参照版12.2.0を更新していない。Mermaid.jsとの実行画像比較ではない。
HTML5の全名前追加は対象外で、現行の基本6名amp/lt/gt/quot/apos/nbspと数値に限った。

#581の保全ZIP全hashと対象3lib/font7のhashを照合して抽出した。元は#577成功Debug head`63853f1`、調査時mainは`2f07266`。
src/tests/eng/fonts/CMake入力が不変であることを確認し、ASan/UBSan/nullability計装済みlibraryを再利用する。
外部harnessは既存native helperをincludeし、正規lld-link/明示runtime/wholearchiveでリンクした。

## 比較入力

| 用途 | modelから選ぶ実対象 |
| --- | --- |
| flow node | 最初のnodeのlabel |
| flow edge | 最初のlinkのlabel |
| flow group | 最初のgroupのlabel |
| sequence participant | 最初のparticipantのlabel |
| sequence message | 最初のmessage eventのlabel |
| sequence note | 最初のnote eventのlabel |
| sequence frame | 最初のframe eventのlabel。`loop  [`と終端`]`を期待値に含む |
| sequence title | modelのtitle text |
| sequence numbered | 最初のmessage label。`5. `のprefixを期待値に含む |

期待文字列には日本語、ampersand、山括弧、二重/単引用符、NBSP、hash、semicolon、pipe、backslash、改行を含む。
全textの中から同じ文字を探すだけではなく、上表の実対象を選んで照合する。期待文字列は生成原文とは別のUTF-8 fileへ保存する。

7表記はliteral混在、Mermaid10進、HTML10進、HTML16進、Mermaid基本名、HTML基本名、backslash改行。
literal基準でも、文区切りと競合するsemicolonだけは`#59;`を使い、quote/backslashをescapeし、改行を`<br>`とする。
名前付き2形式は`<br />`/`<br/>`、最後の形式はbackslash+n、数値形式はLFの数値を使う。
名前付き形式で基本6名にない記号は数値参照を使うため、名前付き参照だけで全文を表しているとは扱わない。
63原文/9期待文字列と各hash、全variant、参照URLをinputs.jsonとinputs directoryへ保存した。
literalのbackslashがquote用1個とbackslash用2個であることを静的に確認してから実行した。

## 実行と判定

tools/record-run.py経由でlink.ps1のlink-01、`python check.py`→labels.exeのcheck-01を実行した。
native/子/記録側0、stderr/recordingErrors空、180秒timeoutなし。
126件すべてでparser受理・期待model label一致・IMAGE_READY/bitmap公開を確認し、全画像を保存した。
9用途×明暗のliteral18画像を基準に、残り108画像の全SHA256が一致した。基準自身を含む一致数は126。
辺ラベル明、番号付きmessage暗、frame見出し明の3画像を視認した。
公開された画像の寸法とbytes/hash、全stdout行、入力snapshot、期待文字列、実exe/libraryを保持する。
失敗した製品試験はない。探索中の推測mermaid_shape.cパスとWindows rg glob指定の失敗は訂正し、検証成功へ算入しない。

## 範囲と収載

この9用途/7表記/固定書体/明暗の確認を、全HTML5名、任意Unicode/書体/DPI、任意図の衝突、layout全域、性能、実UIへ拡張しない。
製品/test/schema/版/依存/ADR/gate変更なし。ARC-001/003/010/012、QLT-008/010/012、GIT-001〜004、ADR0089、開発ワークフロー§8に従う。
追加waiverはnone、既存WVR-0001はWinHTTP callbackだけで継続する。実app/実ノート/Store操作なし。
原本は`D:/NeNeFolio/agents/diagram-label-equivalence-20261104`、実行出力は同名outputs。
品質記録/gate-proofs/日報/引き継ぎ/currentの5文書を更新する。最終head必須CI、正典統合、通常exe hash照合、
全証跡保全・追加作業木/output整理の確定値は同agentsのintegration-completion.jsonへ収載する。
