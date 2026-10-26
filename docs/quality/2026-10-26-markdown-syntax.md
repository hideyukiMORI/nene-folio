# Markdown基本構文とnative表 — 2026-10-26

Issue: [#396](https://github.com/hideyukiMORI/nene-folio/issues/396)。基点main `cfbff7cd3269b2a63ecd2f5509e128d728e23638`。
設計は[ADR0070](../adr/0070-markdown-syntax-and-native-tables.md)、利用者向け範囲は[Markdownの書き方](../MARKDOWN.md)。
規則: ARC-001/002/003/004/005/009/010/011/012、C-002/003/005/008/011/012/013/014/016/017、QLT-008/009/012/013、GIT-001〜004。
Waivers: none。

## 要求と変更境界

hideからMarkdownの未対応構文を実装する依頼を受け、具体例として`----`を含む一般的な記法を指定された。
表、入れ子リスト/引用、task、strike、水平区切り、setext、明示改行、fence/code spanの区切りを同じ変換へ加える。
原文・plain編集・保存・検索位置の正本は既存経路に保つ。完全CommonMark/HTML/画像埋込みは対象外。

## 実装前の成立性

証跡は`D:/NeNeFolio/agents/markdown-syntax-20261026/`。
`investigation.md`に基点の不足/誤解釈の根拠行、公式仕様の該当節と責務分割案を記録した。

- `build-table-probe.ps1`: compile/link/run exit 0、43 checks / 0 failures。
  実core検索からnative rawの位置を選択し、表のheader・日本語/emoji・第2セル・表直後で取得文字が一致した。
  600px/100%→320px/150%の幅変更はRTF再投入なし。TOM Undo停止/再開とmodified復元でraw/選択/dirty/Undoを保った。
  無保護対照のdirty/Undo変化は`table-probe-initial.c/.log`に保持する。
- `build-hr-probe.ps1`: compile/link/run exit 0。空の1pt段落は120dpiで2pxの横線となった。
  同じ段落の前後余白は22pxの帯になるため不採用。隣の段落の余白を使う画像を設計サナが目視で受理した。
  rawは前段落/空段落/後段落で、後続のfont/backgroundも戻った。

これらはAPIの成立性であり、完成製品の検証とは区別する。入力hashは`diagnostic-inputs.json`。
設計の読み取り専用独立レビューは、途中のtable配置失敗、実製品のraw確保長、63列超のliteral行境界を
完成候補で確認すべき点として挙げた。設計上の原文保存経路に矛盾は見つからなかった。最終コードの承認とは扱わない。

## 完成候補

候補の初回読解では、空白入り水平区切りとlist marker、table直後のblock開始、
見出し/表header内の強調の復帰、リンク先と外側のdelimiter、長い空白列の反復走査を確認した。
さらに空行を挟むlist継続、code spanのescape parity、listの外へ戻る未閉じfence、
本文に文字として存在するnative table markerの扱いを対象へ加えた。記録は`parent-review.json`。
これらは実装中に見つかった事項であり、最終候補の不具合残存や検証成功を示す記録ではない。

読み取り専用の規約レビューでは、借用viewの長さ/offsetの不変条件と、単一の実装だけで使う可変recordの
公開範囲も確認した。CNF-002との両立を含めてADR0070へ判断を追記し、規約やgateの除外を追加しない。

10月27日未明に対象候補を確認した。Markdown生成物とplain編集の入口は`markdown_rtf`に保ち、
block/containerは`markdown_blocks`、行の境界は`markdown_line`、inlineの短命metadataは`markdown_inline_tokens`、
list stackは`markdown_list`、tableのセル/RTFは`markdown_table`、借用spanと文字escapeは`markdown_view`/`rtf_text`へ整理した。
旧行分類と共有token APIは撤去した。`note_pane`は既存のUndo停止/回復を所有し、native row書式だけを`note_pane_table`へ渡す。
列数は`EM_GETTABLEPARMS`の返却値を使い、原文の制御字から推測しない。

| 検証 | 実行と結果 |
| --- | --- |
| Debugの製品/対象unit | 固定toolchainで`cmake --build D:/NeNeFolio/outputs/markdown-syntax-20261026/build --target nenefolio folio_tests`。clang-tidy込みexit 0 |
| Markdown unit | 同buildの`folio_tests.exe --markdown-rtf`はexit 0。追加構文、優先順位、閉じ記号、交差した装飾、原文のSave/SaveAs/EDIT保持、512段のlist、大量未閉じ記号/空白、63列超の正/不正なdelimiter、借用入力不整合を含む |
| 確保失敗の注入 | `build-measurement.ps1`による対象測定buildと同selectorはexit 0。26箇所の確保拒否でoutを変更せず、その後の成功まで確認。canonical buildのinactive表示は注入成功に数えない |
| 実製品native | `build-product.ps1`初回は221 checks中218成功。残る3件は同じ段落字下げoracleの誤り。raw取得/表後tail、検索→選択→取得文字、列幅/倍率、dirty/Undo/Redo/focus、部分配置失敗とUndo再開失敗からの回復、literal marker混在、plain再投入、通常wheelは成功 |
| 字下げの対象再確認 | `fixture.exe --indent`は37 checks / 0 failures、exit 0。3配色で親/子/親復帰のnative書式と実文字座標を照合した。製品の字下げは変更せず、成功済み218件を再利用 |
| 画像 | lightの複合gallery、darkの狭幅150%、synthetic高コントラスト300%を設計サナが確認。入れ子、見出し、表の寄せ/折返し、コード、区切り、未閉じ文字を確認。`parent-visual-review.json`と`images/`へ保存 |

実製品の取得長は3配色とも`note_pane_text = 直接GT_RAWTEXT = GTL_NUMCHARS = 124`で、末尾まで一致した。
検索対象の日本語/emoji、code内pipe、表直後の本文は、同じraw位置で選択し取得文字も一致した。
`EM_GETTABLEPARMS`へcapacity 63を渡すと実際の列数が返ることも診断で確認した。

初回の字下げoracleは`dxStartIndent`を本文の左位置と扱っていた。
[PARAFORMATの契約](https://learn.microsoft.com/en-us/windows/win32/api/richedit/ns-richedit-paraformat)では同値は第1行、
`dxOffset`が後続行の差である。実値は親220+220、子660+220、親復帰220+220twip、文字xは38/74/38pxだった。
合計・hanging幅・親子差・文字座標を照合して測定を訂正した。
初回失敗と、`--indent`が渡らず全条件へ入った誤selector実行は、それぞれ`product.log`と
`product-indent-dispatch-failure.log`へ保持する。一度の221件全成功とは報告しない。

対象測定のASan起動には`interception_win: unhandled instruction`の診断行が残るが、プロセスはexit 0まで完了した。
最終のsource hash・コマンド・対象範囲は証跡の最終検証記録と実装報告へ集約する。
ローカル全件gateは実行せず、最終PR headのCI `pwsh -NoProfile -File ./eng/check.ps1`と必須`check`を統合条件にする。

## 契約と限界

生成関数の呼出形は維持し、借用入力不整合を区別する`MARKDOWN_RTF_INVALID_INPUT`とUI内部のreflow入口を追加した。
既存applicationは生成成功だけを採用し、変換不成立の既存経路へ戻す。原文・保存schema・設定・実行時依存・gateは変更しない。
CommonMark/GFM完全互換ではなく、63列超の表は元の記法で表示する。非対応構文は[対応範囲](../MARKDOWN.md#対応範囲)を参照する。
list内にquoteを挟み、同じ親listの継続本文へ戻る組合せは階層/字下げを保証しない。
今回受理した引用内listと各々の入れ子とは区別し、追加parser拡張をこの変更へ混ぜない判断とした。
物理入力、全OS/全配色、人によるreader聴取、DWM呈示は未検証。高コントラストは入力paletteを与えた専用desktop上の検証で、OS設定は変更していない。

## 反映と整理

作業木は`D:/NeNeFolio/worktrees/396-markdown-syntax`、一時出力は`D:/NeNeFolio/outputs/markdown-syntax-20261026/`。
main統合・通常exe反映・恒久成果物保全後に、未保存/未追跡/無視ファイル・唯一の成果物・稼働参照・リンクを確認して整理する。
branch/commitは保持する。旧削除拒否済み14箇所は操作しない。通常アプリを自動終了せず、GUI入力を行わない。
確定したPR/CI・通常exe・整理は証跡directoryの`completion.json`と実際のIssue/PRを照合する。
