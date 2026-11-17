# 参照形式のリンクの品質記録 — #861

2026-11-17。開始main `460ef16`（#855統合後）。設計は[ADR0101](../adr/0101-reference-links.md)。
作業木E:/NeNeFolio/worktrees/861-reference-links（branch feat/861-reference-links）、対象実行の原本E:/NeNeFolio/outputs/861-reference-links、
下書きと完了後の置き場D:/NeNeFolio/agents/reference-links-20261117。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 選んだ理由

hideの指示（ロードマップに沿って進める）で、#850（囲まないURL）の後の「MarkdownとMermaidの整理」の候補から選んだ。
同じ日のリンクの作業（#850・#852）と同じ経路（`web_uri`→field→起動）に乗り、本文の外へ長いURLを置けるので、メモでの効果が大きい。

## 設計の要点

- 定義の表`markdown_references`を描画の始めに1回だけ作る（前方参照のため）。読む規則はADR0101のCommonMarkの部分集合。
- 定義の行を隠す判定は「表が記録した行の開始位置」と「段落が開いていない」の両方。読む所と隠す所を表で結んだ。
- 行内は、既存の丸括弧の行先のリンクと画像の後に残った括弧の対を、完全・省略・短縮の形で引く。見つかれば既存のリンクの対（`MARKDOWN_INLINE_LINK`）にし、
  emitは`link_destination`の一か所で、丸括弧の行先か定義の行先かを選ぶ。

## 試行（時刻はJST）

| 試行 | 結果 |
| --- | --- |
| build-01 | `markdown_render`へ項目を足したため、位置で初期化している`markdown_rtf.c`が`-Wmissing-field-initializers`で落ちた。`nullptr`を明示した |
| build-02 | clang-tidyの認知的複雑度（`render_line`・`pair_references`）。空行と定義の行の扱いを`skipped_line`へ、作り済みのリンク・画像の飛ばしを`skip_formed`へ分けた。途中、clang-formatが行を折り返した後の文字列で置換が外れ、Editで直した |
| build-05 | clang-tidyの引数の数（`normalize`・`record`・`after_line`が5）。範囲を`note_search_span`で渡し、走査の状態を表の中へ置き、`record`へentryを渡す形にした |
| unit-05 | 1件失敗: 試験の行先に生の空白（`<.../a b>`）を書いた。`web_uri`が拒むので装飾だけになる（丸括弧の形と同じ）。試験の誤りとして`%20`へ |
| unit-06 | 4件の期待の誤り: 定義でない行（段落の途中・字下げ4・余計な文字）では、その行のURLが#850の囲まないURLとしてリンクになり、`HYPERLINK`が「無い」という期待が成り立たない。コードブロックには#839のコピーのfieldがある。期待を「`[a][x]`がリンクにならない」へ直した |
| unit-07 | 1件失敗（製品）: `![alt][x]`で、画像の印の付いた1つ目の括弧は飛ばしたが、2つ目の`[x]`だけが短縮の参照としてリンクになった。画像の参照形式は2つの括弧を含めて原文のままにする形へ直した |
| unit-08 / full-01 | 成功 |
| native-01 | 失敗（試験）: `ref。`をnativeの本文で探したが、リンクの直後にhiddenの区切りの空白があり連続しない。`ref`だけでは隠れた命令の中に先に当たる。`。`の位置から数える形にした |
| native-02 | 成功（775 checks）。表示名からの起動先、定義が出ないこと（検索用の本文）、編集表示の原文 |
| 規約 | CNF-006がADRの中のリンクの書き方の例を相対リンクと判定した。言い換えた |
