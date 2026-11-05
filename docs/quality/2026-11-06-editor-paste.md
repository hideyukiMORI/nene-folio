# 編集pasteの書式混入 — 2026-11-06

Issue #479。規則: ARC-001/004/008/011/012、C-002/005/008/014/016/017、QLT-008/012/013、GIT-001〜004。
FR-006 / ADR0006 / ADR0054。追加waiver none。既存WVR-0001は無関係なWinHTTP callbackの範囲で継続する。

## 現在の依頼と原因

hideの07:08の継続品質対応と、編集モードで貼り付けがrich textになる報告を受けて#479を再開した。
10月31日は職場でのモード/貼付元が不明なため保留していた。今回modeはEDITと判明したが、
職場の貼付元そのものを再現したとまでは扱わない。

現行main `a783661`では、閲覧と編集が同じRichEditとRTF投入を共有し、clipboard形式の制限がない。
合成RTFとUnicode textを持つclipboardからWM_PASTEすると、本文とcaretは正しい一方、11ptの編集本文へ
18pt・Times New Roman・太字/赤字・中央揃えを持ち込める。修正前の回帰は文字書式の不一致で落ちた。
観測した高さは220/360/360twip。単に保存内容が平文であることでは、編集中の書式混在を防げなかった。

## 修正する正典経路

- `folio_window`の既存EN_MSGFILTERでsession/STREAMING拒否を通したあと、Ctrl+V・Ctrl+Shift+V・Shift+InsertをWM_PASTEへ渡す。
  Altを含む組合せは対象にせず、TranslateMessageが生成したWM_CHAR 22は二重実行しない。
- `note_pane`のWM_PASTEが`EM_PASTESPECIAL(CF_UNICODETEXT)`を一回呼ぶ。READY、有効、編集可能、IME変換中でないことと
  Undo再開待ちがないことを確認する。親の主窓が無効な場合も拒否する。
- 文字の挿入/選択置換/dirty/UndoRedoはnativeのまま。clipboardを自前で読み直したり本文の第二の写しを保持したりしない。
  Unicodeへ変換できる文字形式が無い場合は無操作とし、rich形式へ退避しない。
- 閲覧装飾と編集原文のRTF投入、本文保存/履歴、schema、依存、製品版、品質ゲートは不変。

TM_PLAINTEXT固定は閲覧と共有するRTF経路を壊すため不採用。OLE callback追加はCOM寿命/ABI/waiverを増やすため採らなかった。
subclassでkeydownを先取りする案も、終了照会中の既存EN_MSGFILTER保護を飛ばすため不採用。
これらは読み取り専用の独立レビュー`paste_review`と主担当の静読で照合した。実装・試験は主担当単体。

API根拠: [RichEdit clipboard](https://learn.microsoft.com/en-us/windows/win32/controls/use-rich-edit-clipboard-operations)、
[EM_PASTESPECIAL](https://learn.microsoft.com/en-us/windows/win32/controls/em-pastespecial)、
[EM_SETTEXTMODE](https://learn.microsoft.com/en-us/windows/win32/controls/em-settextmode)、
[Clipboard Formats](https://learn.microsoft.com/en-us/windows/win32/dataxchg/clipboard-formats)。

## 試行と記録

原本は`D:/NeNeFolio/agents/479-editor-paste-20261106/`、run-dir親は`D:/NeNeFolio/outputs/479-editor-paste-20261106/`。
全体journalは`D:/NeNeFolio/agents/quality-20261106/journal.md`。旧10月31日原本も保持する。
各対象実行は`tools/record-run.py`でsource/config/実exeのsnapshot、raw stdout/stderr、子/wrapper終了値を保存した。

| 試行 | 実行 | 結果と判断 |
| --- | --- | --- |
| build-baseline-01 | 固定toolchain、CMake Debug、image_status_tests target | 試験が存在しない公開API名を使いcompile失敗。exe未生成のartifact記録も失敗。製品のredとは扱わない |
| build-baseline-02 | 同じ対象build、公開UIA handle取得にfixtureを修正 | child/wrapper 0、stderr空 |
| test-baseline-01 | `ctest --test-dir build -R '^paste_native$' --output-on-failure` | 2.36秒、文字書式不一致の意図したred。child/wrapper 8、記録エラーなし |
| build-candidate-01 | 最小修正と対象fixtureのDebug build/tidy | on_notify認知的複雑度13>10で失敗。child/wrapper 1。旧exeのartifactは新候補生成を意味しない |
| build-candidate-02 | 通知振分けと本文入力を分離、root無効試験/画面証跡を追加 | child/wrapper 0、stderr空 |
| test-candidate-01 | 同じ対象CTest | 撮影に流用したstatus色assertionで停止、child8。画像が無いためwrapperは記録失敗。製品の文字/書式失敗とは扱わない |
| build-candidate-03 | 既存status色検査を保持して画像記録部分を分離 | child/wrapper 0、stderr空 |
| test-candidate-02 | 同じ対象CTest | 414checks成功、native1.91秒、child/wrapper0、stderr空。ただしPrintWindow画像は黒一色で、画面確認は未成立 |
| build-candidate-04 | 実RichEdit本文をEM_FORMATRANGEでmemory DCへ描画 | child/wrapper 0、stderr空。製品ソースはcandidate-02から不変 |
| test-candidate-03 | 同じ対象CTest、実本文のnative出力画像を保存 | 414checks成功、native2.35秒（CTest計2.43秒）、child/wrapper0、stderr空 |
| test-capture-01 | `ctest --test-dir build -R '^note_status_ja$' --output-on-failure --verbose` | 画像保存共用部の既存status色検査28checks成功、native10.87秒、child/wrapper0、stderr空 |

コマンドの完全なargvは各run.json、入力bytes/hashと結果はresult.jsonに保持する。
buildはagents内`build.ps1`、対象CTestは`test.ps1`で既存正典コマンドを呼ぶ。
新しい完了定義を作るものではなく、最終headの必須CIは引き続き必要。

## 検証範囲と限界

同じ製品library/manifest/sanitizerを使う`paste_native`を既存CTestへ追加した。専用portable dataと
WinSta0とは別の匿名WindowStation/desktopだけを使い、通常ノート、利用者clipboard、前面focusには触れない。
キーボードはその試験threadのSetKeyboardStateと製品のtranslate/TranslateMessage/DispatchMessageで処理し、物理打鍵とは区別する。

対象は4入力方式、文字/段落書式、Unicode/改行/選択/caret、後続typing、Undo/Redo、ANSIからUnicodeへのOS変換、
異なるRTF/Unicode表現の選択、RTF-only/HTML-only/空clipboardの保持、readonly/子と主窓の無効/IME中の拒否、閲覧装飾の保持。
4入力すべてで元本文/貼付先頭/貼付後半の高さが220/220/220twipとなり、字体・色・文字効果・段落揃えも保持した。
Windows11 Pro 10.0.26300、Msftedit 10.0.26100.9949。実DLLのhashとtoolchainはagents内environment.json/入力snapshotに残す。

PrintWindowの黒画像もtest-candidate-02のartifactとして保持する。代わりに同じ実RichEdit本文を
[EM_FORMATRANGE](https://learn.microsoft.com/en-us/windows/win32/controls/em-formatrange)でmemory DCへ出力し、
全短文が収まることを確認してnative cacheを解放した。出力画像を目視し、編集は均一書式、閲覧は見出しの大きさ/太字が維持されることを確認した。
画像はagents内`paste-editor-native.bmp`と`paste-view-native.bmp`。viewport/DWMを撮影したものとは区別する。
bitmap保存だけを既存status_pixelsと共用し、そちらの色assertionは変更していない。

物理IME、職場環境、全DPI/OS、OLE drag/drop、外部からの直接TOM/EM_PASTESPECIAL、
STREAMING/終了照会中の同期再入pasteは未実行。最後の2状態の拒否順序は既存guardと差分の静読で照合した。
最終head必須CI・統合・通常版反映・全保全整理の確定結果は、agents内`integration-completion.json`へ収載する。
対象試験の成功だけを統合完了とは扱わない。

## 統合と通常反映の確定

PR708の最終head `eed344b126bfe82025f1b2ecf2cdf2279ccaf71c`で、必須CI42122814552のfull-gate/checkが成功した。
main `50fa28ea6ebc120023f1bbd30a753f9c3a5e9ab3`へ統合し、通常版を正典prepare-real-machineで反映。
実data32fileのbytes/hash/作成更新日時は不変で、通常appは起動していない。
通常exe SHA256は`496dd3b7455f425c7400e23cdd9017e79881817ef2ee039a2f71db78531aec14`。

旧10月31日output・今回output・作業木の11811filesをarchive内の全hashで照合した。
archive SHA256は`63ce1848c5f252a55cb591334294b8fb067ac7d6d6cc107579ef9ca580faf11e`。
絶対path、同一squash tree、clean/ignored、全file不変、linksと稼働参照を確認して3領域を整理し、branch/commitを保持した。
確定原本は`D:/NeNeFolio/agents/479-editor-paste-20261106/integration-completion.json`。
