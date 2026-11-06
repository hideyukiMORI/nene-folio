# 名前入力面のOS終了拒否と取消後の制約保持 — 2026-11-06

Issue #757。FR-006/015/018/019/030、ARC-001/004/005/008/011/012、C-002/008/012/014/016/017/018、QLT-008/010/012/013、GIT-001〜004、ADR0006/0021/0053/0064。
追加waiver none、既存WVR-0001継続。

## 問題と正典経路

ADR0064ではnative modal中も製品rootが有効であり、内容操作だけを止める。ADR0053の終了拒否と取消はこのmodal所有の制約を維持する。
#309の旧root無効モデル/内部注入診断と区別して、現行の実SaveAs面を恒久nativeへ収載する。製品不具合の新規確認ではない。
既存image_status_testsの専用desktop/data、実CtrlShiftS/名前面/Cancel、query_session_end/cancel_session/共通保存/finish_quitを通す。OS終了APIや製品内部stateの注入は使わない。

## 固定した境界

- 履歴付きoneをEDITにし、日本語/絵文字追記・非空本文選択・native modified/Undo/focusを準備。元md/全履歴/revision・元/targetの全md集合・両台帳有無と全row順/countをsnapshotする。
- 既存name observerがcurrentthread/ownerchain/titleを確認して、未保存名「zzcancel 日本語 🐾」とtarget、name選択1..4を入れる。50ms/100callbackの有限observerを検査前にKillTimerし、queued callbackはtimer0で戻る。
- 専用rootへQUERY(LOGOFF)→ENDTRUE→ENDFALSEを2回送る。QUERYはFALSE、独立literal「開いている操作を終えてから終了してください。」を照合。ENDTRUEで破棄/WM_QUITを発行せず、ENDFALSEで理由だけ解除して次QUERYも拒否する。
- 各通知後に同root/同dialog・全name/選択/category/namefocusを保持。root有効、本文/drawer/filter無効、name/category/Save/Cancel有効、公開ready=falseを固定する。
- 本文全体/selection/modified/UndoRedo（focusはname）とEDIT/CHANGED/NAMED/one選択/count、全md/履歴/revision/両集合台帳を保持。候補名のmd/history/台帳行を作らない。
- 同面Cancelで閉じ、1面だけの終了とroot/本文/drawer復帰、元本文focus/全native状態/全保存物を照合。既存approve_session_save/finish_session_saveで元oneの保存承認/重複QUERY無追加保存/入力凍結/取消/実UndoRedo/SAME再承認/ENDTRUE最終破棄/publicrootnull/WM_QUIT0へ進む。保存一回/履歴一回shift/revision+1だけ。
- 閉じたNAME_DIALOG_SESSION_CANCEL actionを追加し、既存name_rejection_probeを同期呼出しのstackから借用する。既存session helperと他actionの意味は不変。

## 全試行と検証

| 試行 | 結果 |
| --- | --- |
| build-01 | 正典Debug/tidy image_status_tests/save_as_native_tests成功 |
| test-01 | save_as_nativeとsession_modal_native成功 |
| test-repeat-01 | 既存台帳/元2追加md/target1追加mdの別条件成功 |

save_as_native 377checks/1.59秒、session_modal_native 438checks/1.14秒、計2.74秒成功。台帳あり/元追加2md/target追加1mdのrepeatはsession_modal_native 524checks/1.13秒、CTest1.13秒成功。runtime stderr空・child/wrapper0・入力不変。追加3mdと台帳の全bytes/hash/作成更新日時不変。

正典toolchainから`cmake --build build --target image_status_tests save_as_native_tests`、
`ctest --test-dir build -R '^(session_modal_native|save_as_native)$' --output-on-failure --verbose`、別条件の`^session_modal_native$`を実行。
元台帳はone→zzmodal-second→zzmodal-firstの非辞書順。追加3mdと台帳の全bytes/hash/作成更新日時を外側でも照合する。
`clang-format --dry-run --Werror tests/win32/image_status_tests.c`、`python -B eng/conformance.py --build-dir build`、`git diff --check`で文書と最終差分を確認する。
対象source/CMakeの最終6hash比較で成功結果を再利用し、最終head必須CIの`pwsh -NoProfile -File ./eng/check.ps1`へ統合検証を集約する。
全入力/raw stdout/stderr/child・wrapper exit/自己レビューをD:/NeNeFolio/agents/757-session-modal-nativeと同outputsに収載。
build前の自己レビューでsource選択条件への新selector追加漏れを訂正し、実行失敗はなし。補助readでADR0064名を推測して不存在、rgの実pathへ訂正した。製品試験と分離する。

## 変更・受理条件

製品src/API/schema/依存/既存session helper/ゲート不変。既存owned有限observerの閉じたactionと同期stack借用contextを使う。
CMakeは試験登録と既存lock/TIMEOUT30の2行のみ。単体自己レビュー、追加waiver none、既存WVR-0001継続。
品質記録/QG/current/日報/引き継ぎへ#755完了と今回試行を収載する。
最終headCI→正典統合→製品tree/CMake試験2行以外不変による通常exe再利用→全保全・安全整理。確定原本は同agents/integration-completion.json。

## 限界

実OS終了/Update/全flags/nested modal/無題/改名復旧/色選択/メッセージbox/IMEとの全組合せ、通常WM_CLOSEの遅延終了要求、既に無効なroot、保存中再入取消/理由API故障/容量/ACL/公開後失敗/資源失敗は対象外。
OS側理由表示や待機時間内保存完了を保証しない。通常app/実ノート/clipboard/前面focus/Store未使用。個別保留466/634/530と旧削除拒否領域を維持。
