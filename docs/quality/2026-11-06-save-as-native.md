# 別名保存のnative編集保持 — 2026-11-06

Issue #715。FR-030、ARC-001/004/005/011/012、C-008/016/017、QLT-008/010/012/013、CNF-006、GIT-001〜004、ADR0021。
追加waiver none、既存WVR-0001継続。主担当単体で設計/実装/検証、paste_reviewは契約と差分の読み取り専用独立レビューだけを行った。

## 固定した契約

既存`tests/win32/image_status_tests.c`へ`--save-as`を加えた。公開TREE FOCUSから主窓WM_CHAR `:`で実Ex欄を開き、
`:saveas 名前`とEnterを送る。既存command_input → SAVE_AS → store_new → create_editedを通し、製品のtesthookは増やしていない。
保存前後の同じ実RichEditで全文（日本語/絵文字/Markdown記法）・選択・Undo/Redo可用性を照合する。
Exを開く際はfocusを移すため、focusやnativeのEM_GETMODIFY値を保存の保持契約へ加えてはいない。
変更判定は既存folio_state_unsavedを使う。

- 変更済みEDITから別名保存。新内容はSAME、実Undoで元本文となりCHANGED、Redoで新内容へ戻りSAME。
- Undo後のRedo保有状態から別名保存。保存時本文はSAME、実RedoでCHANGED、再UndoでSAME。
- VIEWの表示にはMarkdown強調記号が無いことを確認し、その状態から別名保存。新mdは表示文字ではなく記法を含む原文、VIEW/readonly/表示/選択を保持する。
- 元mdと5履歴slotは存在/不在と全bytesを比較し、3保存でstore_revision増分も3に限定する。新mdは全bytes一致、全履歴slot不在。
- 二つ目の保存とそのUndo/Redo後にも一つ目のコピーを照合する。単にUndo可用フラグだけでなく、操作後の本文/変更判定で履歴保持を確認する。

同じsource・resource・fonts・製品libsを共通CMake foreachから組み立てる`save_as_native_tests`を追加した。
出力は`build/verification-save-as`とし、保持するコピーで既存image_status試験のnote orderを汚さない。
`save_as_native`は30秒timeoutと既存image_status_data lockを維持する。再実行時の名前は専用索引の総数から導き、
保存前に不在を確認する。古いコピーの削除・上書き・ごみ箱移動は行わない。
製品src/API/保存schema/依存/ゲート閾値・除外は不変。

## 実行結果と全試行

原本はD:/NeNeFolio/agents/715-save-as-nativeと同outputs、全体journalは同agents/quality-20261106/journal.md。
`tools/record-run.py`で入力snapshot、生成exe、raw stdout/stderr、child/wrapper終了値を保持した。
buildは固定toolchainを読み`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`と
`cmake --build build --target save_as_native_tests`を実行。既存sanitizerとclang-tidyを維持する。

| 試行 | 結果 |
| --- | --- |
| build-01 | 正典Debug/tidy成功、child/wrapper0、入力不変 |
| test-01 | `ctest --test-dir build -R '^save_as_native$' --output-on-failure --verbose`、164checks成功、native1.16秒/CTest1.17秒、stderr空 |
| test-repeat-01 | 既存コピーを保持する再実行契約を対象に同コマンドを再実行。170checks成功、0.93秒、stderr空。前回3コピーの全bytes/hash/作成更新日時不変、履歴directory不在、新3コピー追加を外側でも照合 |

再実行の6checks増加は、seedが元履歴を2slot追加し、3回の元履歴比較で「不在確認1assertion」から
「読取とbytes一致の2assertion」へ変わるため。テスト契約の変更ではない。
成功した2回の対象結果は、関連入力不変のままPR/CI/統合へ再利用する。以降の繰返し実行は行わない。
独立静読でもFR-030/ADR0021との不一致・偽陽性・対象範囲内の重要不足は見つからなかった。

補助readで誤ったsrc/adapters/win32/folio_window.c、存在しないeng/format.ps1を指定して失敗した。
正しいsrc/ui/win32と既存clang-format入口へ訂正。製品試験の失敗とは混同しない。
Windows11 Pro10.0.26300、Msftedit10.0.26100.9949。固定toolchainの版/hashと試験exe/fonts/CTest定義はsnapshotへ収載。
#713反映後のDebug設定で実行し、ASan起動警告は無い。最終head必須CI・統合・通常exe不変・全保全整理の確定原本は
agents内integration-completion.jsonへ収載する。

## 限界

物理Ctrl+Shift+S/IME、名前入力modalの取消・衝突、I/O失敗・台帳部分失敗、改名、無題、外部UIA reader、viewport画素、長文scroll、全DPIを確認したものではない。
通常app/実ノート/利用者clipboard/前面focus/Storeを試験に使っていない。今回のfixtureは一行Unicode本文を対象にし、改行正規化の全組合せまで広げない。
通常exeの生成に関係するsrc/eng/fontsは不変で、CMake差分はnative試験hostの共通設定と追加に閉じる。通常buildは反復しない。

## 統合・整理済み

#715/PR716は最終head852f92602a3ecbd587459c57715f35d3eb1ca0b7、必須CI42139348824 full-gate/check成功。
main4096ce691e151bbba32bc08bf3739952ebf18535へ正典統合し、通常exeのhash/bytes/更新日時不変を確認した。
3358filesを全hash保全し、絶対path/同一squash tree/clean・untracked・ignored/link/稼働参照0を照合後、専用worktree/outputを整理した。
branch/commit保持。確定原本D:/NeNeFolio/agents/715-save-as-native/integration-completion.json。
