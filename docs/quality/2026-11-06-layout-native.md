# 表示切替の編集保持 — 2026-11-06

Issue #711。FR-039、ARC-001/004/005/011/012、C-008/016/017、QLT-008/010/012/013、CNF-006、ADR0047/0048/0075/0076。
追加waiver none、既存WVR-0001継続。主担当が試験実装と検証を行い、失敗したfocus期待の解釈だけpaste_reviewが読み取り専用で独立静読した。

## 固定した契約

`tests/win32/image_status_tests.c`の既存製品fixtureへ`--layout`、CTestへ`layout_edit_native`を追加した。
専用desktop/dataを使い、同じ実RichEditで未保存変更状態と、Undo後のRedo保有状態のそれぞれを確認する。
各巡回は公開BODY FOCUSで開始し、NOTE → BOTH → DRAWER → BOTH → NOTE → BOTHの6ステップと、各到達先の同表示再選択を行う。
通常幅は公開3択のSELECT、狭幅は公開chooserを開いて既存FOLIO_MENU_LAYOUTのcommand行をINVOKEする。
隠れた3択はdisabled/offscreenであることを確かめ、操作しない。30秒timeoutと既存data lockを維持する。

- 全文（日本語・絵文字・Markdown記法）、選択範囲、変更印、Undo/Redoの可用性、EDIT modeを保持する。
- DRAWERだけ本文を隠す。各段階でnative HWND focusと公開focus itemを照合する。
- 本文からDRAWERへ移ると一覧へfocusを戻す。同表示をchooserで選び直すと、可視のchooserへfocusが戻る。
- chooserからBOTHへ広げると、header focusは可視のBOTH項目へ写る。その後のNOTE/BOTHへのSELECTでも、可視の同じheader focusを保持する（ADR0075）。
- 各ステップ後に保存済み本文、5履歴slot、store_revisionが不変であることを確認する。
- 巡回後の実Redo/Undoでも全文を確認し、画像処理要求が無いことを照合する。

製品の状態/保存/Undo実装、公開API、schema、依存、ゲートの閾値・除外は変更していない。
今回の6ステップで異なる方向は4種類。NOTE→DRAWERとDRAWER→NOTEの直接遷移まで網羅したとは主張しない。

## 実行結果

原本は`D:/NeNeFolio/agents/711-layout-native`と同名outputs、全体journalは`D:/NeNeFolio/agents/quality-20261106/journal.md`。
全試行の入力snapshot、exe/fonts/CTest定義、raw stdout/stderr、child/wrapper終了値は`tools/record-run.py`で保持した。

| 試行 | 結果と判断 |
| --- | --- |
| build-01 | 固定toolchainの正典Debug、既存sanitizer/tidyで成功 |
| test-01 | 0.73秒で公開focus期待に失敗。直前の本文/選択/dirty/UndoRedoは成功、全巡回は未完了 |
| build-diagnostic-01 | 各ステップと公開focus itemを記録する最小診断。成功 |
| test-diagnostic-01 | DRAWER初回はNOTE(27)、chooserから同表示再選択後はLAYOUT_CHOOSER(8)。常に一覧focusという試験期待がADR0075のmenu復帰契約と異なることを確認 |
| build-02 | 既存契約に合わせfocus期待と各巡回の開始条件を訂正し成功。製品srcは不変 |
| test-02 | 486checks成功、native1.36秒/CTest1.37秒、child/wrapper0、記録エラーなし |

独立静読でも、chooser→BOTH header→後続SELECTの可視header保持と、各巡回前の公開BODY FOCUSは既存仕様と一致した。
本文/選択/dirty/UndoRedo/保存履歴の条件は緩めていない。初期失敗は製品不具合のredではなく、試験側の期待誤りとして残す。
整形後の差分へ最初に当てた編集patchは一致せず拒否され、sourceは変わらなかった。現行行を読んで訂正した。

Windows11 Pro10.0.26300、Msftedit10.0.26100.9949。固定toolchainと入力の版/hashは各snapshotへ収載した。
ASan起動時の`interception_win: unhandled instruction`一行がnative出力にあり、rawのまま保持した。計装は無効化していない。
物理キー/IME、外部UIA reader、viewportの画素、長文の論理行/scroll、無題、最大化・全DPI・幅復元は今回の対象ではない。
通常app/実ノート/利用者clipboard/前面focus/Store/Ledgerを試験に使っていない。

最終head必須CI・統合・通常exe不変・全保全整理の確定結果は、agents内`integration-completion.json`へ収載する。

## 統合・整理済み

PR712最終head df27ea776d452418abcb430e4885027bdef781ec、必須CI42132598999 full-gate/check成功。
main47f80045ce3f33b71e176d31c38b43f84caa6d94へ統合し、通常exe不変を確認した。
4402filesを全hash保全し、絶対path/同一squash tree/clean・untracked・ignored/link/稼働参照を照合後、専用worktree/outputを整理した。
branch/commit保持。確定原本は上記integration-completion.json。
