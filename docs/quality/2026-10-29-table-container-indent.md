# native表の字下げと残幅 — 2026-10-29

Issue: [#410](https://github.com/hideyukiMORI/nene-folio/issues/410)。基点main
`f6d7f889c569a10147d6af763fccd0cc0088af6e`。
[ADR0072](../adr/0072-native-table-container-indent.md)の受理設計を実装した対象検証記録。
規則: ARC-001/003/004/008/010/011/012、C-002/003/005/008/011/012/014/016/017、
QLT-008/009/012/013。Waivers: none。

## 変更と範囲

`markdown_blocks`は既存`markdown_containers_indent`の値を、header/body共通の借用入力
`markdown_table_format`（kind・indent_twips）として`markdown_table_row`へ渡す。引数は4つのまま。
tableは`trleft=indent`と`cellx=indent+(index+1)*(9000/columns)`を生成する。
新しい純粋な`markdown_table_native`判定を、blocksの表示選択と直接row入力の検証で共有する。
製品の対応範囲は1〜63列・字下げ0〜32767twipで、32767はRichEditの絶対上限の主張ではない。
範囲外は既存の全行literal経路へ渡し、元の記号・prefix・番号を保持する。
直接row入力はkind・借用行・delimiter構文・列数・字下げを検証し、不正入力ではoutへ追記しない。

`note_pane_table`はnativeの`dxIndent`を取得し、pane幅>indentの場合だけ減算する。
残幅0も既存の#405の商/余り分配で扱い、更新後のindent・列数・各幅をreadbackで照合する。
負のpane幅またはnative indentは既存の型付きFAILEDを返す。
Markdownの再解釈、第二の行index、幅cache、最小可読幅、水平移動、保存schema、設定、依存、ゲートは追加しない。
MARKDOWNとPROJECT_LAYOUTへ対応範囲・責務を反映した。

## 対象unitとOOM

既存scenarioを共通entryへまとめた`folio_tests.exe --markdown-tables`はexit 0。
root0・quote480・list440・list/quote両順920のheader/body、後続root段落、
一回だけのmarkerと後続連番、invalid kind/借用line/delimiter、0/1/63/64列判定、
32767直接rowのcellx37267/41767、32768/SIZE_MAXの無追記拒否を確認した。
実container入力では32600twipがnative、33080twipが全行literalとなり、
literalのheader/delimiter/body全行と装飾記号・元番号が残り、後続の8番へ復帰する。
既存の64列container literalの全列・元prefix・番号保持も同じentryで実行した。

OOMは`eng/coverage.py`の`sanitizer_vector`でDebug compile_commandsの対象source/test全件から
一致するaddress/undefined/nullability・no-recover-allを取得し、同じ`build_measurement`で作成した。
`--table-indent-oom`だけを実行し、native経路25箇所・literal経路16箇所の確保失敗で公開outのsentinelを保持し、
各経路が最後に成功へ到達した（exit 0）。同vectorの必須binding proofは正常例0、
反例1と`_Nonnull binding to null pointer`診断を確認した。手製sanitizer fallbackは使っていない。
全件unit・local fullgate・coverage閾値測定・無関係検索やUIは実行していない。

## 実製品coreとnative配置

専用非入力desktopのfixtureにcanonical Debug buildの`nenefolio_core.lib`と
`note_pane_table.c.obj`を直接リンクし、製品のMarkdown→RTFとnative配置を通した。
通常画面・data・Nibへの入力、SwitchDesktop、SendInput、activationは行っていない。
合成ASCII入力、Segoe UI、120dpiを使用し、各表のheader/body計2行をroot/quote/list/mixedの4組で確認した。
TOMのUndo suspend/resumeとdirty復元は既存pane呼出側と同じ手順を使う。

完成fixture v2は317 checks / 0 failures、native exit 0。
pane有効幅は現行paneと同じformat rectangle・DPI・zoomから導いた。

| pane / zoom | 有効幅twip | root0 | quote480 | list440 | mixed920 |
| --- | --- | --- | --- | --- | --- |
| 600px / 100% | 7176 | 3588 : 3588 | 3348 : 3348 | 3368 : 3368 | 3128 : 3128 |
| 320px / 150% | 2544 | 1272 : 1272 | 1032 : 1032 | 1052 : 1052 | 812 : 812 |
| 117px / 150% | 920 | 460 : 460 | 220 : 220 | 240 : 240 | 0 : 0 |
| 直接幅0 | 0 | 0 : 0 | 0 : 0 | 0 : 0 | 0 : 0 |
| 320px / 150%へ復帰 | 2544 | 1272 : 1272 | 1032 : 1032 | 1052 : 1052 | 812 : 812 |

各header/bodyでindent・2列・gap80・残幅合計を維持した。セル先頭の座標は
format左端＋(indent+gap+先行cell幅)のDPI/zoom換算と2px以内で一致した。
mixedの先頭/第2セルはwideで85/346px、narrowで127/229pxとなり、後続Tailは1/2pxへ戻った。
幅0はnative配置APIの直接境界試験であり、実pane callerが物理幅0をskipする挙動は変えていない。
負幅-1は無変更でFAILED、実nativeへ設定してreadbackしたindent-480も無変更でFAILEDだった。
全段階でraw UTF-16全位置・選択2..7・dirty=-1・非空Undo=1/Redo=1・own-desktop focusを保持した。
原文再投入による復帰や履歴の作り直しは行っていない。

`native-v2-wide.bmp`と`native-v2-narrow.bmp`を目視し、表左端の字下げ、pane内の右端、
見出しの太字/本文の通常字、7番marker一回・後続8番・bullet一回、Tailの通常位置を確認した。
窓破棄・original thread desktop復帰・専用desktop解放も成功した。

## 初回失敗と証跡

native-v1は317 checks / 30 failures、native exit 1だった。fixtureがcell境界をU+FFFAと仮定したため、
第2セルcp=-1となり、位置の比較だけが失敗した。実rawのU+0007境界と行直後CRを踏まえ、
v2でglyph先頭cpと第2セル検出だけを訂正した。期待の字下げ・幅・座標式・2px許容は変えていない。
v1のsource/build/log/result/入力/画像は上書きせず残した。v1でも幅と本文状態のoracleは一致していた。

成果物は`D:/NeNeFolio/agents/410-table-container-indent/`、build/runtimeは
`D:/NeNeFolio/outputs/410-table-container-indent/`だけを使う。
`build-targets.ps1 -Attempt 1`でcanonical configureと`folio_tests`/native objectをbuild（clang-tidy込み）、
`run-native-v1.ps1`/`run-native-v2.ps1`でfixtureをcompile/link/runした。
`run-selector-attempt1.py`がcanonical計装とOOM selectorを実行した。
exact commandは`commands.md`と各script、各子process終了値は`*-result.json`、
入力source固定copyは`inputs-validated1/`と`validated1-inputs.json`、環境は`environment.json`、
全記録のhashは`hashes.json`、完成状態は`completion.json`へ残す。
wrapperが0でもnative-v1の子exit 1を成功へ付け替えない。

## 限界と引継ぎ

Windows/MSFTEDITの今回の専用desktopによる代表回帰であり、通常UIの物理入力や全OS環境の保証ではない。
ASan runtimeの`interception_win: unhandled instruction`警告を各生logへ残している。
極狭多列表と字下げがpane幅以上の場合の文字不可視は未解消であり、構造保持・復帰のoracleと分ける。
既存の#404所属/marker、#405等幅のred、非製品boundary v4の32767/2列・63列29 checks/0は再利用した。
追加の上限探索・Hscroll比較・性能計時は行っていない。

対象build/tidyと静的format/conformance/diffはexit0。実行script・結果へ記録した。
親の追加build-dir検査は最初CMake File API reply欠落で1件となったため、失敗logを保全した。
正典checkと同じcodemodel-v2 queryを置き同じDebug構成を再生成後、依存構成を含むconformanceは0件。
製品/対象testは変わらず投入時hash8件と一致し、成功済み対象結果を再利用した。
最終PR headの必須CI `pwsh -NoProfile -File ./eng/check.ps1`、commit/push/PR/mergeと通常反映は親担当。
worktree・出力は未統合のため保持し、稼働参照と保全状態はcompletionへ記す。
統合/反映/恒久成果物収載後に、親が絶対path・取込・未保存/未追跡/ignored・唯一成果物・稼働参照・リンクを監査して整理する。

## 統合後の確定値

PR #412のhead `355d034`はCI `40796594189`のfull-gate/check成功後、main `eb9706d`へ統合した。
全体4619/5084分岐（90.854%）。#411を含むmain `bb6dc01`を10:07:08 JSTに通常exeへ反映し、起動していない。
出力205ファイルの全hash保全と最終監査後、worktree/outputの削除要求は実行前に自動承認レビューで拒否されたため保持する。
確定値と再試行禁止は`D:/NeNeFolio/agents/410-table-container-indent/integration-completion.json`。
