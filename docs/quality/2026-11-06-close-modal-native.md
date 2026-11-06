# 名前入力中の通常終了と保存拒否からの復帰 — 2026-11-06

Issue #759。FR-006/015/018/019/030、ARC-001/004/005/008/011/012、C-002/008/012/014/016/017/018、QLT-008/010/012/013、GIT-001〜004、ADR0006/0021/0050/0053/0064。
追加waiver none、既存WVR-0001継続。

## 問題と正典経路

ADR0064はnative名前面中の主窓終了要求を最内面の取消へ渡し、最外dispatchが戻ってから元ノートの保存・終了へ進む。
#373の専用診断と区別し、既存恒久nativeへ実SaveAs→名前取消→元保存拒否の境界を追加する。製品不具合の新規確認ではない。
既存image_status_testsの専用desktop/data、実CtrlShiftS/名前面、同rootのWM_CLOSE、modal_owner_cancel/show_name_prompt/dispatch_finished/resume_normal_close/depart_document/finish_quitを通す。
内部closephase/dispatchdepthの注入や新しいmodal loopは作らない。

## 固定した境界

- 履歴付きoneをEDITにし日本語/絵文字を実追記。非空本文選択10..12、native modified/Undo/focusを準備し、元md/全履歴/revision・両カテゴリ全md集合/両台帳有無と全row順/countをsnapshot。
- oneだけを共有なしhandleで保持。実CtrlShiftSからSaveAsを開き、既存nameobserverがcurrentthread/ownerchain/titleを確認して未保存名「zzcancel 日本語 🐾」/target/name選択1..4を入れる。
- observerは50ms/100callbackの有限期限を保ち、検査前にKillTimer。新ROOT_CLOSE actionが既存historyfailure observerを開始してrootWM_CLOSEを同期送信する。借用name_rejection_probeは同じ同期呼出しstack内だけで使う。
- rootWM_CLOSE直後も同root生存、readyfalse、WM_QUITなし、failure未発火、全文/selection/modified/UndoRedo・EDIT/CHANGED/NAMED/one/count/revision不変。名前取消途中のfocusはOSへ任せ、本文内容だけを照合する。完了後のfocusは事前の本文focusと厳密比較する。
- 外側keyhelperの既存pumpで遅延保存へ進む。独立literalの履歴保存拒否を一つのowned面で確認して閉じ、名前面が一つだけ終了したこと、root/body/drawer/filter有効/readytrue、元本文focus/全native状態/全保存物保持を検査。
- handle解除後、実Undoで元本文SAME、実Redoで追記CHANGEDへ戻り、どちらも全md/履歴/revision不変。既存close_theme_windowで通常終了を再要求し、元oneだけを一回保存、全履歴一回shift/revision+1、rootnull/WM_QUIT0を確認。元台帳/target/全file集合とcountは保持する。
- #757のファイルsnapshotと照合の2helperは共用名へ改名だけ行う。既存session/helper/他actionの意味は変えない。

## 全試行と検証

| 試行 | 結果 |
| --- | --- |
| build-01 | 正典Debug/tidy image_status_tests/save_as_native_tests成功 |
| test-01 | save_as_native/session_modal_native/close_modal_native成功 |
| test-repeat-01 | 既存台帳・元追加2md・target追加1mdの別条件成功 |

save_as_native 377checks/1.69秒、session_modal_native 438checks/1.17秒、close_modal_native 197checks/1.18秒、計4.04秒成功。台帳あり/元追加2md/target追加1mdのrepeatはclose_modal_native 215checks/1.24秒、CTest1.25秒成功。runtime stderr空・child/wrapper0・入力不変。追加3mdと台帳の全bytes/hash/作成更新日時不変。

`cmake --build build --target image_status_tests save_as_native_tests`、`ctest --test-dir build -R '^(close_modal_native|session_modal_native|save_as_native)$' --output-on-failure --verbose`、別条件の`^close_modal_native$`を実行。
元台帳はone→zzclose-second→zzclose-firstの非辞書順。追加3mdと台帳の全bytes/hash/作成更新日時を外側でも照合する。
`clang-format --dry-run --Werror tests/win32/image_status_tests.c`、`python -B eng/conformance.py --build-dir build`、`git diff --check`で文書と最終差分を確認する。
最終source/CMakeの6hash比較で対象成功結果を再利用し、最終head必須CIの`pwsh -NoProfile -File ./eng/check.ps1`へ統合検証を集約する。
全入力/raw stdout/stderr/child・wrapper exit/自己レビューをD:/NeNeFolio/agents/759-close-modal-nativeと同outputsに収載。

起票後の静読で、共有なしhandle中に既存verify_history_unchangedを呼ぶと試験側の元md再読込も拒否されると確認した。
排他条件は維持し、その場の公開state/revisionと解除後の全bytes照合へ分けた。試験実行前の設計補正であり、失敗した期待値の変更ではない。
旧Issue本文もissue-initial.jsonへ保持、補正版をissue-refined.jsonへ収載。実行失敗なし。
対象結果集計のPython stdoutは既定コードページで文字化け表示したが、UTF-8のJSON/文書原本を確認した。以後補助Pythonに-X utf8を指定する。

## 変更・受理条件

製品src/API/schema/依存/既存session helper/ゲート不変。既存owned有限observerへ閉じたROOT_CLOSE action、既存file helper2つは共用名へ改名のみ。
CMakeは試験登録と既存lock/TIMEOUT30の2行のみ。単体自己レビュー、追加waiver none、既存WVR-0001継続。
品質記録/QG/current/日報/引き継ぎへ#757完了と今回試行を収載。
最終headCI→正典統合→製品tree/CMake試験2行以外不変による通常exe再利用→全保全・安全整理。確定原本は同agents/integration-completion.json。

## 限界と一次資料

専用rootへの同期WM_CLOSEと実native面の確認。物理主窓×/外部UIA/IME取消拒否/nested modal/全面種/無題3択/OS終了との複合/部分保存/容量/ACL/資源失敗は対象外。
通常app/実ノート/clipboard/前面focus/Store未使用。個別保留466/634/530と旧削除拒否領域を維持。

Microsoft Learnの[EndDialog](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enddialog)は終了flagを設定し、systemへ制御が戻った後にdialogloopを終えると定める。
[WM_CLOSE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-close)は終了要求の通知であり、保存と取消の契約は製品側で保持する。
このためcallback中のroot生存と外側復帰後の保存結果を分け、名前面の取消後の可視性/focusを過剰に固定しない。
