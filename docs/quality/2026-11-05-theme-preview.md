# テーマpreviewの取消と未保存編集 — 2026-11-05 / #661

## 結論と正典

テーマ157checks/1.287395秒、共有本文snapshotの倍率187checks/0.828516秒、分岐移動の画像経路75checks/0.925461秒。全stderr空、子/補助/記録0。
[ADR0061](../adr/0061-theme-live-preview.md)の非永続preview・取消・本文保持を恒久CTest theme_preview_nativeへ追加した。
既存unit/外部native受理がある通常契約をCIに固定するもので、新規製品欠陥の認定ではない。
既存image_status_tests、実folio_window/persistence/appearance、同exe/30秒/image_status_data lockを使う。
製品ロジック/API/schemaは不変。新製品入口・別再着色経路・gate緩和は作らない。

## 固定する一連

短い正常な日本語/絵文字/Markdown本文と既存archive portによる2世代をseedする。
window作成後に公開EDIT、native編集一回で「 追記 🙂」を追加、選択10〜12単位を置く。
開始時の保存choice・実効theme・palette、settings.json全bytes、現在file/履歴1〜5を取得する。
同じfixtureの先行テストがテーマを保存していても成立するよう、保存choiceと異なるnordまたはdraculaを選ぶ。

1. 公開設定→テーマ面→native検索欄へ候補名を入力。現在revisionの一覧先頭をnavigateし、公開名が候補と一致することを確認。
2. その行へSELECTを送り、実効themeが候補へ変わることと保存choice/settings.json全bytesの不変を比較する。
   本文全UTF16 units・選択・dirty・Undo・Redoと入力focusを、テーマ面を開いた直後と比較する。
3. CLOSEで取消。開始時のpalette/実効themeへ戻り、本文/選択/dirty/Undo/Redo/focusが設定を開く前と一致する。
4. 再び同じ候補をpreviewし、BACKで設定へ一段戻る。palette/実効theme/保存設定と本文を比較し、CLOSEする。
   BACKのsurface focus移動だけは正当な変化として扱い、閉じた後のfocusは開始時と比較する。
5. 各一巡後にstore_revisionと全保存内容/不存在が変わらないことを比較する。
   Undo一回で事前の追記だけが戻りSAME、保存設定/全履歴不変、画像要求0を確認する。

settings.jsonはstorage_state_directory→storage_native_path_fillで専用rootを得てfile_bytes_readする。
cwdの推測や製品の保存APIは使わず、生bytesを比較し所有bufferを解放する。HCを含むOS外観設定は変更せず、取消後paletteは開始時の値と比較する。
明示確定INVOKEは実行しない。候補の絞込み自体も既存previewへ合流する。物理キーや外部UIA readerの新試験ではない。

## 共有部分と検証

zoomの本文snapshot helperをnative_body_snapshot/capture_native_body/expect_native_bodyへ一般名にした。比較処理は同一でmessageだけ一般化。
末尾のstatus/image振分をrun_other_fixtureへそのまま移し、既存complexity上限を守る。
直接影響する--zoomと引数なし画像経路だけを1回確認した。成功済みの他native/全条件は工程だけで繰り返さない。

| run | コマンド・結果 |
| --- | --- |
| configure-debug-01 | 固定toolchain、正典cmake Debug/Ninja、成功 |
| build-debug-01 | clang-format --dry-run --Werror、cmake --build <debug> --target image_status_tests --parallel 2、初回142actions中に既存iconのnull診断で停止。呼出し階層を元に戻したbuild02(4actions)、build03/04(各2actions)でtidy成功 |
| contract-03 | image_status_tests.exe --theme-preview、157checks/1.287395秒 |
| zoom-01 | 同exe --zoom、187checks/0.828516秒 |
| image-01 | 同exe引数なし、75checks/0.925461秒 |
| conformance-01 | 7docsの参照/diff/source/build graphと最終source入力SHAを確認する |

全runをtools/record-run.pyでsnapshotし、raw/終了値/入力不変を保全する。nativeごとに開始時専用dataのcopy/hashと終了時hashを記録。
初回編集補助はCMake read_textのUTF8指定漏れでcp932 UnicodeDecodeError。testsourceだけ書込済み、build/run前に停止した。
初期driverを保持し、CMakeだけ明示UTF8で処理してから整形した。製品/テストの実行失敗とは分離する。
build-debug-01はicon経路の呼出し階層を一段増やすと既存pixelsのnull診断で停止。icon分岐を元のrun_fixtureへ戻し、status/imageだけ抽出したbuild02以降は規則を変えず成功した。
contract-01はSELECT専用mode helperで設定を開こうとして拒否。公開INVOKEへ訂正したcontract-02は、公開名をNord/Draculaと誤って期待し停止。
固定catalogの実名nord/draculaへ訂正したbuild04/contract03で成功。いずれも失敗のraw/入力/終了値/dataを保持。製品欠陥として扱わない。

補助readの誤った所在/Windows glob/過大出力、CRLF警告は全体journalへ保持する。親単体、追加委任無し。

## 範囲・完了条件

短い正常本文の通常preview/取消成功が対象。全テーマ/OS/DPI/IME、物理入力、故障注入、全OS I/O、性能の新保証ではない。
store_revisionと保存内容を比較し、全I/Oを捕捉したとはしない。先行fixtureにより履歴数とchecks数は変わる。
実app/実ノート/Storeなし、SwitchDesktop無し。通常版は#651反映後exe/data不変確認だけで再buildしない。
規則ARC-001/004/007/008/012、C-008/012/014、QLT-008/010/012/013、GIT-001〜004。追加waiver none、既存WVR-0001継続、#634保留。
最終PR headの必須CI eng/check.ps1成功後にtools/merge-pr.ps1で統合する。
原本D:/NeNeFolio/agents/661-theme-preview/。最終CI/統合/通常exe-data不変/全保全/専用領域整理は同integration-completion.jsonへ。
全保全hash・取込tree・dirty/ignored/link/稼働参照監査後、当該D専用worktree/outputだけ整理。branch/commitを保持。
全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。
