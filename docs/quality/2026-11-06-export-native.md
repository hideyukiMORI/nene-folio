# 現在ノートのnative書き出し — 2026-11-06（#767）

FR-041、ARC-001/004/008/011/012、C-008/014/016/017、QLT-008/010/012/013、GIT-001〜004。
[ADR0082](../adr/0082-markdown-transfer.md) / [ADR0064](../adr/0064-content-modal-and-window-shell.md) / [ADR0044](../adr/0044-tests-at-completion-and-integration.md)。
追加waiver none、既存WVR-0001継続。

## 問題と正典経路

#494の一回の隔離Win32診断は未保存単票と保存先選択面を確認したが、現行の継続CIにはこの組合せの恒久回帰が無かった。
新しい製品不具合の確認ではなく、既存の「編集中の単票は未保存内容を含み、元を保存しない」契約の固定である。
既存image_status_testsの専用desktop/portable dataと公開folio_window_accessible_actionの操作メニューを使用する。
実export_at/file_picker_save/export_to/take_text/folio_state_export_notes/transfer_adapter/file_bytesを通り、製品への計測口やprivate includeは追加しない。

## 対象と保持確認

#767は実操作メニューから同thread/root所有のWindows保存先選択を通し、変更EDIT/実Undo後Redo保有EDIT/VIEW原文を各1件書き出す。
変更EDITの取消は出力0。日本語名の全出力bytesを独立literalで照合し、本文全文/selection/modified/UndoRedo/focus/mode/選択noteと元md/全履歴/revision/両実file集合・台帳を保持する。
終了後の実Redo/Undo、modal中のroot有効/内容無効/readyfalseと復帰も固定。専用exe隣の新directoryだけへ出力し、前回物を上書きしない。

- 履歴付きoneを開き、日本語/絵文字の追記でCHANGEDと実Undoを作る。各回は2..6の非空選択と本文focusから開始し、全native snapshotを比較する。
- 50ms/最大100callbackのtimerは同threadの可視#32770、同root所有chainだけを観測。保存先面の独立literal題名と、初期one/one.mdを持つEdit ID1001が一つだけあることを確認して絶対pathを入力する。
- 実IDOK/IDCANCELを同dialogにPostし、成功時だけ別のowned noticeに独立literal「書き出しました。元のノートはそのままです。」を要求して閉じる。取消では成功noticeを要求しない。
- modal中は本文/drawer無効とroot有効/公開readyfalseを確認する。focusはmodal内であることを確認し、終了後は事前本文focusと厳密比較する。本文や選択を試験側で復元して保持の証明にしない。
- 取消0件・変更EDITのswitch_saved・Undo後EDITとVIEWのhistory_currentを、file_bytesの全長と全bytesで独立literalと比較。VIEWの装飾表示をMarkdown出力へ流用しない。
- 元oneと全履歴slot、store_revision、両カテゴリの全Markdown集合、台帳の有無と全row順、document kind/name/selection/countを毎回比較する。実Redoで元の追記に戻り、Undoで元本文SAMEに戻った後にVIEWを出力する。
- 出力は専用exe隣の新規export-native-NNNN directoryだけに置く。1000候補の有限範囲で既存directoryを飛ばし、既存出力を上書きしない。最終全列挙で指定3file以外・取消file・一時file・subdirectoryがないことを確認する。
- 再実行は前回3出力、元2追加md/target1追加md、非辞書順の元台帳を保持。既存7fileの全bytes/hash/作成更新日時と、今回3出力の初回同名内容との一致も外側で照合する。

## 全試行と対象検証

| 試行 | 結果 |
| --- | --- |
| build-01 | export_stepの引数6個が上限4個を超え、tidyが拒否。child/wrapper1、記録errorなし・入力不変。製品実行前 |
| build-02 | 出力先/期待値を借用requestにまとめ4引数へ修正。正典Debug/tidy image_status_tests成功、child/wrapper0・入力不変 |
| test-01 | export_nativeと共用保存物fixtureのclose_modal_native成功 |
| test-repeat-01 | 既存台帳・追加3md・前回3出力の別条件でexport_native成功 |

export_native 276checks/2.91秒、close_modal_native 193checks/1.08秒、計4.0秒成功。台帳あり/元追加2md/target追加1md/前回出力3fileのrepeatはexport_native 314checks/2.43秒、CTest2.43秒成功。runtime stderr空・child/wrapper0・入力不変。既存7fileの全bytes/hash/作成更新日時不変、新3出力も初回と全bytes一致。

実行コマンドは`cmake --build build --target image_status_tests`、
`ctest --test-dir build -R '^(export_native|close_modal_native)$' --output-on-failure --verbose`、再実行の`^export_native$`。
全実argv・source/config/実exe/fontsのsnapshot・raw stdout/stderr・child/wrapper exitはtools/record-run.pyで保持する。
補助formatterの推測path不存在ではformatter未実行。正典eng/toolchain.ps1の読込へ訂正し、design-review.jsonへ分離記録した。

`clang-format --dry-run --Werror tests/win32/image_status_tests.c`、`python -B eng/conformance.py --build-dir build`、
`git diff --check`と相対参照確認を最終差分に実施する。成功したsource/CMake入力をhash照合し、push/統合だけを理由にruntimeを反復しない。
必須CIは最終headの`pwsh -NoProfile -File ./eng/check.ps1`へ集約する。

## 差分・限界・統合

製品src/API/schema/依存/版/ゲート/既存helper不変。既存sourceへ新case/selector/seed条件を加え、CMakeは登録と既存TIMEOUT30/data lockの2行だけ。
単体自己レビュー。専用OSダイアログのclass/control IDは現Windowsの観測であり、全Windows版のUI構造保証ではない。
物理mouse/IME、全OS/DPI、外部UIA client、無題、ZIP/画像同梱、取り込み、既存出力衝突、I/O/容量/ACL/資源失敗、OS終了との複合は対象外。
通常app/実ノート/clipboard/前面focus/Storeを試験に使わない。個別保留466/634/530と旧削除拒否領域を維持する。

原本はD:/NeNeFolio/agents/767-export-native、試行は同outputs。必須CI/統合/通常exe不変/全保全整理は完了後に同agents/integration-completion.jsonへ確定する。
成功した対象試験だけを統合完了とは扱わない。[日報](../reports/2026-11-06.md)・[引き継ぎ](../handoffs/2026-11-06.md)。

## CI専用失敗の診断（20:07 JST時点）

初回head d3c3839の必須CI42206482231は46対象中export_nativeの出力全bytes比較だけが失敗した。
正典merge toolの既存一回Ready再送が起こした42207000260でも同じ比較が失敗した。
両runのraw logをagentsに保持し、統合待ちprocessを停止、PR768をDraftへ戻した。統合はしていない。
GitHub連携のDraft mutationはResource not accessible by integrationだったため、既存認証CLIへ切替成功。

本文全文/選択/modified/UndoRedo/focusとmodal復帰の検査は失敗位置の直前まで成功している。
read結果・実bytes長/全hex・期待bytes・UTF16出力pathを失敗時だけ記録し、元の完全一致assertは維持した。
Windows Server 2022 CIでだけ再現し、ローカルでは同候補も成功するため、原因確定用に同CI環境で診断する。
この追加情報は原因の特定に必要な途中診断（QLT-012）であり、失敗を一時的とみなす再試行ではない。

- build-diagnostic-01: 診断を既存functionに足した結果、complexity22>10をtidyが拒否。child/wrapper1、入力不変。製品実行前。
- build-diagnostic-02: 読取結果検査と失敗情報formatterを関心ごとに分割。Debug/tidy成功、child/wrapper0、入力不変。
- test-diagnostic-01: export_nativeだけローカル成功、child/wrapper0、入力不変。CI原因はまだ未確定。

## 保存先入力の切分けと修正候補（20:16 JST）

診断head a936894のCI42207947041は変更EDITの最初の成功出力で、
`PERSISTENCE_ABSENT`（read=1）、期待46bytes、実0bytesを記録した。
期待絶対pathは専用export-native-0001の「変更 日本語.md」。成功notice後にこのpathが存在せず、
本文bytesが異なったという証拠ではない。製品不具合やWindows内部の特定の通知欠落と断定しない。

試験側はSetWindowTextと即時保存要求を、同owned実filenameへfocus→EM_SETSEL全選択→EM_REPLACESELへ変更する。
入力直後と通常modal dispatchへ戻った次の有限timer callbackで、全長と全文が期待絶対pathに一致することを要求し、
その後に同dialogのIDOK/IDCANCELへ進む。閉じたENTER/SUBMIT/NOTICEのenumをswitchで全列挙し、
固定sleep・再入力retry・判定緩和・private COM窓口・製品変更を加えない。既存50ms/100callback上限を維持する。

build-03はDebug/tidy成功。test-02のexport_nativeは358checks/3.37秒、CTest3.37秒成功。
child/wrapper0、stderr空、入力不変。既存7fileの全bytes/hash/作成更新日時不変、独立4directoryの指定3出力は全bytes一致。
既存共用helperは不変のため、初回close_modal_native193checks/1.08秒の結果を再利用する。
Windows Server 2022上の修正後結果は最終CIで確認し、現時点では改善済みと断定しない。

API操作の根拠はMicrosoftの[Edit control text operations](https://learn.microsoft.com/en-us/windows/win32/controls/edit-controls-text-operations)
と[Edit controls](https://learn.microsoft.com/en-us/windows/win32/controls/about-edit-controls)。
WM_SETTEXTにも通常の変更通知があるため、旧失敗を単に「通知しないAPI」と説明しない。
今回の検証結果は専用native入力条件に限り、全OSのUI構造や物理操作の保証へ拡張しない。

verify-02はCMake再生成前のreplyファイル一覧を使ったため、input snapshotで19不存在（前後計38記録error）を検出し、子を起動せず拒否（child null/wrapper125）。補助集計の初回はwrapper1との誤ったassertで停止し、実値125へ訂正した。古い一覧はsnapshotに残し、現実のreply一覧へ更新したverify-03で最終静的検査を行う。製品runtimeの失敗とは分離する。
