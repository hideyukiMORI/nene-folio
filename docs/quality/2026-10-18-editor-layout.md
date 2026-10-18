# #340 — 編集画面の行送り・行番号・ドロワー表示

2026-10-18、hideの0.2.0候補に対する実機フィードバックを修正。
Issue: [#340](https://github.com/hideyukiMORI/nene-folio/issues/340)。基点main 4084829。
規則: FR-015/031/032/034、ADR0006/0026/0051、ARC-001/004、C-012/017/018、QLT-008/012/013、GIT-001〜004。
schema・依存・ゲート変更なし、Waivers: none。単体で調査・実装・検証した。
恒久証跡: D:/NeNeFolio/agents/editor-layout-20261018/。

## 原因と変更

編集用RTFが閲覧用の段落後8pt/1.25行送りを流用し、各生テキスト行に余白が付いていた。
編集だけ段落前後0・1行送りへ変更。120 DPIの合成日本語では行送りが56pxから34pxになった。
行番号は異なる書体の上端を揃えていたため、独立したTOM範囲の実baselineへ揃える。選択範囲は使わない。
検索placeholderは35pxの枠中央に描いていたが、native EDITの文字領域は17pxだった。
EDITを実フォント高さで中央へ置き、placeholderはEM_GETRECT、左右余白はEM_SETMARGINSへ一本化した。
外側余白のクリックも既存の検索focusへ接続し、clear図形を実EDIT高に収めた。
新規ノートは既存themeのdialog_control/dialog_border/category_textで描き、常設の文字付き入口とcommand/UIAを維持。
カテゴリ識別色の仕様は変更していない。候補fixtureの青#3D7EFFはサナが設定したカテゴリ色で、テーマのアクセントとは別。

## 対象検証

- `cmake --preset dev` と `cmake --build build --target nenefolio`: 成功。最終UI追加後の増分buildも成功。Debug ASan/UBSan/nullabilityと既定clang-tidyを維持。
- `cmake --build build --target folio_tests` と `./build/folio_tests.exe --markdown-rtf`: exit 0。編集の期待RTFだけ更新し、既存の閲覧/往復検証を利用。
- 外部 `build-probe.py <repo> <build> <output>`: 実compile_commandsの全flags・4製品lib・resourceを利用。ソースを緩和せず、実Win32の専用desktopで実行。
- `NativeDesktopPerf.Run`: final1/extra ともexit 0、前景HWND不変。hideの稼働アプリ・編集中データは操作していない。
- final1: 実モニター120→144→168→120 DPI、最小700×450物理px@120を確認。実paragraph前後0/単一行送り、各行baseline一致、本文/選択/modified/Undo不変、Undo実行、新規ボタン既存動作を確認。
- extra: 検索外側余白のfocus、clear矩形/クリア結果、日英中placeholder、長い日本語折返しとスクロールのbaselineを確認。JA通常窓と各DPI/最小/折返し画像を実際に確認した。

baseline2.log/BMP、final1.log/desktop.json/BMP、extra.log/desktop.json/BMP、build.log/build-2.log、unit-markdown.logを保存。
source-inputs.jsonに成功時の関係入力hashを保存し、push/統合時に同じ試験は繰り返さない。
表示修正の通常exe/配布候補反映は、同じフィードバックで合意した次の検索focus保存修正とまとめる。
最終headの必須CI・統合・入力照合・整理の実結果は同所completion.jsonへ記録する。

## 失敗と限界

初回probeはTOM SetRangeのS_FALSEを誤って失敗扱いし、成功判定をSUCCEEDEDへ修正。
同名ログへの再実行はCreateNewで拒否され、新しいラベルで実行した。最初のbuild対象名とprobeのenum名の誤記も修正した。
初回normalと一部言語切替直後のPrintWindow画像は未変更部が欠落し、全画面の受理には使わなかった。
後続の完全な日本語通常/DPI/最小画像とnative値で確認。英中はplaceholderの文字領域の確認に限定する。
ASan起動時のinterception_win診断はログに残るが、ASan違反は報告されず対象workerはexit 0。
物理IMEについてhideは「1は大丈夫そう、3は未確定中に切り替わらない」と回答。1は暫定の所感として扱う。
人による読み上げ聴取は引き続き未受理。検索focus保存は別Issueで扱い、このPRでは検索対象仕様を変更しない。
