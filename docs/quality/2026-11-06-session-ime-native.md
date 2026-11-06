# IME変換中のOS終了拒否と解除後の保存 — 2026-11-06

Issue #755。FR-006/015/018/019、ARC-001/004/005/008/011/012、C-002/008/012/014/016/017/018、QLT-008/010/012/013、GIT-001〜004、ADR0006/0024/0031/0053/0064。
追加waiver none、既存WVR-0001継続。

## 問題と正典経路

ADR0053は未確定IMEを保存より先に非対話的に拒否する。#309の専用診断はあるが、恒久nativeのsession_save/untitledは変換中を含まなかった。既存契約を固定する追加で、製品不具合の新規確認ではない。
`image_status_tests --session-ime`で専用desktop/dataの実Win32窓とRichEditを使う。本文・主窓・filter・commandの4所有者を、本文/主窓/全ノート検索/Find/設定面の5か所で確認する。
query_session_endと既存notice、cancel_session、共通保存/履歴、finish_quitを通す。実OS終了APIや製品内部stateへの直接代入は使わない。

## 固定した境界

- 日本語/絵文字の実追記とnative modified/Undoを準備。本文はcaret10、主窓は本文の非空選択10..12で合成STARTCOMPOSITIONを送り、QUERY(LOGOFF)はFALSEで独立literal「文字の変換を確定してから終了してください。」を確認する。
- 各対象でQUERY→ENDTRUE→ENDFALSEを2回行う。窓は同じまま有効、公開ready=false、WM_QUIT未発行。ENDFALSEは理由を解除し、変換中の制約を維持する。modal observerなしで同期的に戻るため、promptへ逸脱すればCTest30秒で失敗する。
- 毎通知後に全文/selection/modified/UndoRedo/focus、EDIT/CHANGEDまたはSAME/NAMED/元選択/count、md/全履歴/revisionを照合。ENDCOMPOSITIONとENDFALSE後はready=trueへ復帰し同じ内容を保つ。
- 実Undoで元本文SAME/Redo保有へ戻し、全ノート検索を公開focusで開く。query「Current」、選択1..4、保存済み一致1件を固定。Findは「日本語」、選択1..3、本文10..13/1件を固定し、両検索語の別所有とsurface revisionも確認する。
- 設定面はEDITを持たない（ADR0031）。公開ACCESSIBLE_SURFACEが返すvisible/focusedなcommand_layerへ通知し、非表示command_inputには触れない。同じ面/選択row/revision/focusと全settings.json bytesを保つ。
- 全5対象の後に実Redoで追記へ戻る。既存approve_session_save/finish_session_saveで保存承認/重複QUERY無追加保存/入力凍結/取消/実UndoRedo/SAME再承認/ENDTRUE最終破棄/WM_QUIT0を確認する。mdと全履歴は一回shift/revision+1のみ。

## 全試行と対象検証

| 試行 | 結果 |
| --- | --- |
| build-01 | Debug/tidy成功 |
| test-01 | 本文不変assertでchild/wrapper8。製品不具合と断定せず記録保持 |
| build-diagnostic-01 / test-diagnostic-01 | 最小診断build成功、runtimeは同assertで8。RICHEDIT50Wの選択10..12の「日本」が消えていると特定 |
| build-diagnostic-02 / test-diagnostic-02 | 開始直後/QUERY直後を比較する診断build成功、runtime8。QUERY前のSTARTだけで同じ2文字が消え、QUERY後には追加変化なし |
| build-02 / test-02 | 本文だけcaretで開始、主窓と検索欄の非空選択検査は維持。build成功、runtime8は設定操作が操作メニューに無いという試験側assert |
| build-03 / test-03 | 既存ACCESSIBLE_SETTINGSボタンへ訂正、Debug/tidyと全5対象のruntime成功 |
| test-repeat-01 | 台帳と追加ノートを残した別条件成功 |

RichEditの合成START通知自体が選択内容を置換する実測を、終了通知によるデータ変更と分離した。fixtureの開始位置をcaretへ訂正し、本文/UndoRedo/保存物の一致assertは維持した。診断printfは最終sourceから撤去。設定面は既存公開ボタンを使い、invoke_operationの操作メニュー探索を拡張して別経路を足さない。

session_ime_native 1158checks/1.07秒、CTest1.08秒成功。台帳あり/元追加2md/target追加1mdのrepeatは1158checks/1.02秒、CTest1.02秒成功。成功runtime stderr空・child/wrapper0・入力不変。追加3mdと台帳の全bytes/hash/作成更新日時不変。

正典toolchainでDebug configure、`cmake --build build --target image_status_tests`。
`ctest --test-dir build -R '^session_ime_native$' --output-on-failure --verbose`と台帳/追加mdありの別条件repeat。
追加source2件/target1件はquery非一致、元oneだけ一致する。元台帳はone→zzime-second→zzime-firstの非辞書順。追加3mdと台帳を外側でも全bytes/hash/作成更新日時照合する。
`clang-format --dry-run --Werror tests/win32/image_status_tests.c`、`python -B eng/conformance.py --build-dir build`、`git diff --check`。
成功後は最終sourcehashを照合して結果を再利用し、最終head必須CIの`pwsh -NoProfile -File ./eng/check.ps1`へ統合検証を集約する。
全入力/source/raw stdout/stderr/child・wrapper exit/自己レビューはD:/NeNeFolio/agents/755-session-ime-nativeと同outputsに収載。
補助readのfolio_note_mark.h推測不存在は実expect_editorのenum folio_note_changeへ訂正。ADR0031のPowerShellliteralglob失敗はdirectory/-gへ訂正し製品試験から分離した。
GitHubpluginはIssue作成403で未作成、既存認証CLIへ切替えた。PR作成も先行#753で同じ権限不足を確認済み。自動承認審査の拒否ではない。

## 変更と受理条件

製品src/API/schema/依存/既存session helper/既存ゲート不変。新case/selector/seedとCMake登録・既存RESOURCE_LOCK/TIMEOUT30の2行だけ。
通常sessionの「公開ready==enabled」を検査する既存helperは維持し、変換中の独立期待値を新caseで検査する。単体自己レビュー。
品質記録/QG/current/日報/引き継ぎへ#753確定と今回全試行を収載。最終headCI→正典統合→製品tree/CMake試験2行以外不変による通常exe再利用→全保全・安全整理。
確定原本は同agents/integration-completion.json。

## 限界とAPI根拠

合成START/ENDCOMPOSITIONによるアプリ通知境界の確認であり、物理IME/候補/未確定文字列のOS保持/TSF/全入力欄の確認ではない。
実OS終了/WindowsUpdate/全flags/無題や改名pendingとの複合/モーダル/削除/保存中再入取消/理由API故障/保存途中/容量/ACL/公開後失敗/資源失敗は対象外。
OS側理由表示や待機時間内の保存完了を保証しない。#309の旧実測は今回へ加算しない。通常app/実ノート/clipboard/前面focus/Store未使用。
個別保留466/634/530と旧削除拒否領域を維持する。

Microsoft Learnの[STARTCOMPOSITION](https://learn.microsoft.com/en-us/windows/win32/intl/wm-ime-startcomposition)と[ENDCOMPOSITION](https://learn.microsoft.com/en-us/windows/win32/intl/wm-ime-endcomposition)は引数のない開始前/終了通知を定める。
[Composition String](https://learn.microsoft.com/en-us/windows/win32/intl/composition-string)の文字列・句・属性取得とは別の境界である。この仕様から、合成通知だけでは実変換文字列の保持を保証しないと判断した。

## 統合・保全整理の確定

#755/PR756は最終headc120b5f811a55998b94ddc4a8817b62deddacb24、必須CI42187525767 full-gate/check成功。
main4c798e0cbfd0379928ea3c61bc3736d59926d5f5へ正典統合。src/eng/fonts/packagingとCMake試験2行以外不変、通常exe hash/bytes/更新日時不変。通常build/app起動なし。
5657files全hash保全後、絶対path/同一squash tree/clean・untracked・ignored/link/稼働参照0を確認して専用worktree/output整理、branch/commit保持。
確定原本D:/NeNeFolio/agents/755-session-ime-native/integration-completion.json。
