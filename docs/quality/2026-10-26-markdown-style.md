# Markdown閲覧の密度・小見出し・コード面 — 2026-10-26

Issue: [#394](https://github.com/hideyukiMORI/nene-folio/issues/394)。基点main `5e7205bcf906392aca1d6f119310c8825640bb57`。
設計は[ADR0069](../adr/0069-markdown-reading-density-and-code-surface.md)、旧表示判断への補正はADR0005。
規則: ARC-001/003/004/005/009/011/012、C-002/003/008/012/013/014/016/017、QLT-008/009/012/013、GIT-001〜004。Waivers: none。

## 要求と変更境界

hideが#392のスクロールと拡大縮小を実機確認した後、閲覧の行間を詰め、h4を本文サイズ・色中心へ変え、
fenced codeを技術記事のコードブロックのような表示にすることを指定した。

- `src/core/markdown_rtf.c`: 本文・引用・リストを自然な1行送りへ。段落後は本文/引用4pt、リスト2pt。
  h1〜3は既存の大きさと太字を保ち、前10pt/後4pt。h4〜6は11pt・通常weight・既存heading色、前8pt/後3pt。
- fenced codeはConsolas10pt、左右9pt、最初/最後だけ上下6pt。行間の段落余白は0。
  同じRTFのgroup内で段落記号までhighlightを保ち、閉じfence/EOFでgroupを閉じる。空fenceに文字を足さない。
- `src/ui/win32/note_pane.c`: 唯一の生成時に`SES_EXTENDBACKCOLOR`を設定し、受理を確認する。
  空行や文字のない右側もnativeが同じ背景として描く。幅/倍率変更でRTFを流し直さない。
- `tests/unit/markdown_tests.c`: 変更した出力契約と空/未閉じ/連続codeの境界を確認する。

RTF色の正本・公開API・保存schema・依存・ゲートは変更しない。編集用plain RTFと原文の保存は既存経路を維持する。

## 設計時のnative診断

恒久証跡は`D:/NeNeFolio/agents/markdown-style-20261026/`。一時出力は`D:/NeNeFolio/outputs/markdown-style-20261026/`。
`native-block-style-initial`、`native-block-style-abcd`、`native-block-style`のsource/logと各画像で判断を追跡する。

- 元の文字highlightは、文字のある場所だけの背景で、空行に面を作れなかった。
- paragraph shading/borderは、診断したMSFTEDITで背景/罫線画素を描かなかった。
- 1cell tableは面を描くが、GT_RAWTEXTへU+FFF9/U+FFFB/U+0007と余分なCRを加え、17文字が21文字へ増えた。
  autofit/%幅も診断した条件では固定幅のまま、狭幅と拡大時に切れた。採用しない。
- 単一paragraphの`line`改行はCRをU+000Bへ変え、背景も全面にならなかった。採用しない。
- `SES_EXTENDBACKCOLOR`と通常段落で、空行を含む背景がつながった。highlightをpar前に戻すと
  空行に隙間が出るため、par後まで保ちgroupで外へ戻す方式を採った。
  前後pを含むraw/default/selectionは同じ24文字、CR保持・表記号なし。
  500/300px幅と150%倍率に面が追随し、plain再投入後は背景画素0となった。

captureの一部に古い描画の再使用があったため、最終診断では短い描画待機とRTF投入後の倍率再適用を揃えて確定した。
これは実装前にnativeの対応可否を決めるための診断であり、完成候補の製品検証とは区別する。

[EM_GETEDITSTYLE](https://learn.microsoft.com/en-us/windows/win32/controls/em-geteditstyle)と
[EM_SETEDITSTYLE](https://learn.microsoft.com/en-us/windows/win32/controls/em-seteditstyle)が背景拡張bitの根拠。
段落装飾の制約は[PARAFORMAT2](https://learn.microsoft.com/en-us/windows/win32/api/richedit/ns-richedit-paraformat2_1)も照合した。

## 設計の進め方

現在のhideの明示分担に従い、設計サナが判断・受理・文書・統合、SOLの実装サナが調査・実装・対象検証を担当した。
ClaudeCodeのdesignプラグインの`design-critique`で相談を試みたが、OAuth期限切れでAPI呼出し前に失敗した。
提案やレビューを受領したことにはせず、設計サナが同スキルの密度・階層・一貫性・可読性の観点を使って判断した。
requestと失敗結果は`design-request.txt` / `design-review.json`に保管する。新しい画像素材は不要で、native描画画像を確認対象とする。

## 完成候補の検証

`build-product.ps1`でDebugの`nenefolio` / `folio_tests`をbuildし、組み込みclang-tidyと変更3ファイルの
`clang-format --dry-run --Werror`を通過した。`folio_tests.exe --markdown-rtf`はexit 0。
初回buildの認知複雑度11は、文書終端処理を小さなhelperへ分けて解消した。閾値10は変えていない。
親の`python eng/conformance.py`は0 violations、`git diff --check`も成功した。

完成候補の実製品fixtureは120dpiの専用非入力desktopで、同じMarkdownを基点generatorと新generatorへ渡した。
初回257 checksは255成功、背景の画素測定2件のみ失敗した。画像は連続しており、行全体の厳密RGB割合では
文字描画の混色まで隙間として数えていた。文字のない左右余白2列を毎行厳密RGBで照合する方式へ修正し、
par前に背景を解除する負例では16行の隙間を検出した。色差の許容や閾値緩和はしていない。
関連するlight/darkの再確認は138 checks / 0 failures、背景の隙間は両方0。製品コードはこの測定修正で変えていない。
初回の成功結果と修正後の再確認を組み合わせて受理し、257件を一度ですべて成功したとは記録しない。

- light 600px/100%、dark 320px/150%、高コントラスト480px/300%を代表条件にした。
  最初と次の本文段落の実baseline距離は92→70、392→307、1024→808pxへ縮んだ。
- native書式から本文の自然な1行送り、h4/h5の11pt・通常weight、h3の14pt太字、コードのConsolas10ptと
  前後/左右余白を確認した。前後の通常段落へ背景を漏らさず、高コントラストはwindow対を維持する。
- 閲覧raw本文のCR・tab・空白・日本語/中文・emojiは基点と一致した。コード内検索の位置をそのままnative選択へ渡し、
  選択文字列も一致した。table markerやCR→VTの置換を加えていない。
- 編集用RTFは基点とbyte一致。編集再投入後は全文字が自動背景色へ戻り、追加編集・倍率変更・Undoも成功した。
  空/1空行/先頭/末尾/連続/未閉じfenceと、背景拡張bitの受理失敗時に生成を拒否する経路を確認した。
- 新しい表示から編集へ切り替えた通常wheelはY=0→97px、`EN_VSCROLL`は10回。本文・選択・dirty・Undo/Redo・focusを保持した。
  旧#392の広い配送/再入試験は経路不変のため再実行しない。
- 設計サナはlightの前後比較、darkの新表示、高コントラストの本文/見出し画像を確認した。
  空行を含むコード面、長行の折返し、本文の密度、小見出しの階層を受理した。

原ログ`product-style-initial.log`、対象再確認`product-style-panels.log`、sourceと入力hashを恒久証跡へ収載する。
物理入力・DWM呈示・全OS/全配色の目視確認は行っていない。高コントラスト300%の画像は本文/見出し部分で、
全コード面の画素比較とは扱わない。最終PR headの`pwsh -NoProfile -File ./eng/check.ps1`と必須`check`の成功を統合条件とする。

## 反映と整理

通常exeへの反映は既存`tools/prepare-real-machine.ps1`を使う。hideの稼働アプリを終了せず、保存して閉じられるまで更新しない。
作業木`D:/NeNeFolio/worktrees/394-markdown-style`と一時出力は、技術受理・統合・必要反映・恒久成果物の収載後に、
未保存/未追跡/無視ファイル・唯一の成果物・稼働参照・リンクを確認して整理する。branch/commitは保持する。
旧削除拒否済み14箇所は対象外。確定したPR/CI/通常exe反映/整理は証跡directoryの`completion.json`を参照する。
