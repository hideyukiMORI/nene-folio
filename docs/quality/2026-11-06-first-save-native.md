# 新規ノートと初回保存のnative編集保持 — 2026-11-06

Issue #725。FR-006/029/039、ARC-001/004/005/008/009/011/012、C-008/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0006/0012/0020/0050/0051/0059。
追加waiver none、既存WVR-0001継続。主担当単体で実装・自己レビュー・検証した。製品src/API/保存schemaは不変。

## 固定した一連

ADR0020の新規と初回保存を、既存image_status_testsの実窓・公開ヘッダーNEW・Exの:wから通す恒久回帰を追加した。
Save Asとは、空の無題採用とUNTITLED→NAMED、初回履歴なしの境界が異なる。新しい製品不具合を再現したものではない。
新規ファイルを残すため既存共通host設定からfirst_save_native_testsを作り、verification-first-saveの専用dataを使う。
30秒timeout/image_status_data lockを維持し、製品hook/保存経路/依存/ゲート設定は追加しない。

- 名前付きoneをEDITで追記し、公開ヘッダーの新規ボタンをInvokeする。元本文は1回だけ保存・履歴追加され、空の無題がEDIT/CHANGEDとなる。
- 無題はカテゴリだけを持ち、実名/選択中の一覧行は無い。body focusとnative modified/Undo/Redoの初期状態を確認し、前文書のUndoを持ち込まない。
- 作成前後にadapterのscan_notesと公開note_countを照合し、無題の採用だけではmdファイル数と一覧件数を増やさない。
- 日本語/絵文字/複数行をnative EM_REPLACESELで入力し、元oneの全履歴/本文とrevisionは不変。非空選択を用意して既存Exの:w 未使用名で初回保存する。
- 名前付き/SAMEになり、native全文/選択/modified/UndoRedoを保持する。新mdは独立literalのLF本文25bytesに一致し、初回履歴5slotはすべて不存在。
- 公開の選択先と文書名、増加したmd/一覧件数、永続ledgerの全行と公開順を照合する。保存revisionは初回保存で1だけ進み、元oneと全履歴は変えない。
- 実Undo1回で空本文/CHANGED、Redoで保存本文/SAMEへ戻る。本文への公開Focus操作でフォーカスだけを移し、Ctrl+Sを2回送っても追加保存/履歴を作らずnative状態を保つ。

元oneの保存1回と初回作成1回を分けて照合し、全体のrevision増分は2。保存済みbytesと履歴の一致は確認するが、
OSの全I/Oを捕捉して同一bytesの書戻しが無いことまで証明する試験ではない。
共通helperは意味を変えず再利用し、既存の無関係なnative試験はローカルで反復しない。

## 全試行と結果

原本D:/NeNeFolio/agents/725-first-save-native、試行同outputs、作業木同worktrees。
tools/record-run.pyへ入力snapshot/hash、raw stdout/stderr、生成exe、child/wrapper終了値を収載した。

| 試行 | 結果と判断 |
| --- | --- |
| build-01 | 正典Debug/tidy build成功、入力不変 |
| test-01 | 0.96秒で試験側のfocus期待が失敗。選択済みEDITのSelectは既存accessible_dispatchがno-opにするため、Ex前のtree focusから本文へ戻らなかった。初回保存/実UndoRedoまでは通過し、zznew-1.mdを保持。child/wrapper8、CTest失敗を隠さない |
| build-02 | EDITの再Selectを公開BODY Focusへ訂正。focusだけを変えた期待snapshotで本文/選択/modified/UndoRedoも照合する。製品不変、Debug/tidy成功 |
| test-02 | first_save_native213checks、native1.33秒/CTest1.34秒、stderr空、child/wrapper0、入力不変・記録エラーなし。旧fileを保持する再実行確認も同じrunで成功 |

test-02の外側repeat.pyは、test-01で作られたzznew-1.mdの25bytes/SHA256/作成更新日時が不変であることを確認した。
元のmd集合one.md/zznew-1.mdを保持し、zznew-2.mdだけを追加。両新規ノートの履歴directoryは不存在。
原本はrepeat-before.json/repeat-result.json。入力不変の追加再実行はせず、最終headのCIへ成功結果を再利用する。

ビルドは固定toolchainのcmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debugとcmake --build build --target first_save_native_tests。
対象コマンドはctest --test-dir build -R '^first_save_native$' --output-on-failure --verbose。
ASan/UBSan/nullability/tidyを維持した。format/conformance実build graph/diffと最終head必須CI・正典統合・通常exe不変・全保全整理の確定原本はintegration-completion.jsonへ。
補助読取で誤ったADR0056ファイル名を指定した失敗もjournalへ分離して記録した。実判定はaccessible_dispatchのSELECT済みno-opと公開FOCUS実装を照合した。

## 限界

Windows NT10.0.26300.0、Msftedit10.0.26100.9949、ローカルNTFS・専用desktop/D領域。
短い正常な名前付き本文からの新規・初回保存が対象。物理keyboard/IME、名前入力modal/取消/保存しない、カテゴリ0/別カテゴリ、名前衝突/I/O失敗/台帳部分失敗、全DPI、fresh-process復旧は対象外。
ローカル成功runは旧試行のノートが残ったfixtureであり、全組合せやまっさらな実アプリ初回起動の保証ではない。
通常app/実ノート/前面focus/Storeは試験に使わず、通常buildも反復せずexeのhash/bytes/更新日時を照合する。
