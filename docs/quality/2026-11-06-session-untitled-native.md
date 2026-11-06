# 無題のOS終了拒否と手動保存後の回復 — 2026-11-06

Issue #753。FR-006/018/019/029、ARC-001/004/005/008/011/012、C-002/008/012/014/016/017/018、QLT-008/010/012/013、GIT-001〜004、ADR0006/0020/0050/0053/0064。
追加waiver none、既存WVR-0001継続。

## 問題と正典経路

ADR0053は無題をOS終了通知中に名前入力へ進めず、空でも未保存として終了を拒否する。#751の恒久回帰は名前付き保存の拒否/承認/取消に限られ、無題と手動保存後の復帰を追加した。製品不具合の新規確認ではない。
製品のquery_session_endとcancel_session、既存New/名前保存/finish_quitを通す。実OS終了APIは使わない。

## 固定した境界

既存first_save_modal_native_tests --session-untitledを専用desktop/dataで使い、実Newで無題を作る。
初期空/日本語改行絵文字を入力し非空選択/実Undo後空の3状態でQUERY(LOGOFF)がFALSE、独立literal理由を確認する。
各回で拒否後ENDTRUE無破棄/WM_QUIT未発行、ENDFALSE理由解除、同root/入力/本文全体/selection/modified/UndoRedo/focus/EDIT/CHANGED/UNTITLED/空名/元category/cursor/countを保持。
元md/全履歴/revision、両category実file集合・台帳有無/全row順を確認し、実Redoで元の日本語本文へ戻す。
実CtrlSの有限owned name observerで有効日本語名をtargetへ初回保存。同じ本文/native状態を保持し、新md1件/初回historyなし/revision+1、元ノート全履歴/元集合台帳/旧targetを保持する。
名前付きSAMEのQUERY承認/root無効、ENDFALSE入力/focus復帰、再QUERY→ENDTRUE破棄/publicrootnull/WM_QUIT0まで保存は手動一回だけ。

modal observerを起動しないQUERYが同期的に戻る。名前入力へ誤って入ればCTest30秒で失敗する。
手動CtrlSだけ既存50ms/100callbackのowned名前observerを使い、終了後timerとimage requestが残らないことを確認する。

## 全試行と対象検証

build-01のDebug/tidy、test-01の関連3試験、test-repeat-01の別条件確認。
first_save_modal_native 308checks/1.15秒、session_save_native 233checks/0.83秒、session_untitled_native 576checks/0.91秒、計2.89秒成功。台帳・追加ノート・前回保存物ありのrepeatはsession_untitled_native 702checks/1.09秒、CTest1.1秒成功。runtime stderr空・child/wrapper0・入力不変。元追加2md/target追加1md/前回保存物/元台帳の全bytes/hash/作成更新日時不変。

非辞書順台帳はone→zzuntitled-second→zzuntitled-first。追加3mdと以前のtarget保存物、元台帳を外側でも全bytes/hash/作成更新日時照合する。

正典toolchainでDebug configure、`cmake --build build --target first_save_modal_native_tests image_status_tests`。
`ctest --test-dir build -R '^(session_untitled_native|session_save_native|first_save_modal_native)$' --output-on-failure --verbose`。
別条件repeatは`^session_untitled_native$`。`clang-format --dry-run --Werror tests/win32/image_status_tests.c`、`python -B eng/conformance.py --build-dir build`、`git diff --check`。
対象成功後は最終sourcehashを照合して再利用し、最終head必須CIの`pwsh -NoProfile -File ./eng/check.ps1`へ統合検証を集約する。

補助readのADR0053ファイル名推測は不存在。rg --filesで正本名へ訂正。Python stdoutのcp932エラーはstdoutをUTF-8指定して訂正し、製品試験から分離した。
入力/source/コマンド/raw stdout/stderr/child・wrapper exit/自己レビューはD:/NeNeFolio/agents/753-session-untitled-nativeと同outputsに収載。

## 変更と受理条件

src/API/schema/依存/ゲート不変。CMakeはsession_untitled_native登録と既存RESOURCE_LOCK/TIMEOUT30の2行。
fixture準備/解放は既存初回保存試験から共用helperへ抽出。拒否時本文期待値は各操作前に独立literalで確認したsnapshotを使い、session notice helperへ独立reason引数を加える。
既存first_save_modal_nativeとsession_save_nativeを直接影響する対象として確認した。単体自己レビュー、追加waiver none、既存WVR-0001継続。

品質記録/QG/current/日報/引き継ぎへ#751確定と今回全試行を収載。最終CI→正典統合→製品tree/CMake試験2行以外不変による通常exe再利用→全保全・安全整理。
確定原本は同agents/integration-completion.json。

## 限界

専用rootのLOGOFF通知のみ。実OSログオフ/WindowsUpdate/再起動/強制終了/電断、全flags、改名pending/IME/modal/削除との全組合せ、保存中再入取消、理由API故障、保存途中/容量/ACL/公開後失敗/資源失敗は対象外。
OS側理由表示と待機時間内の保存完了を保証しない。通常app/実ノート/clipboard/前面focus/Store未使用。#309の旧診断は今回結果へ加算しない。個別保留466/634/530と旧削除拒否領域維持。

## 統合・保全整理の確定

#753/PR754は最終head770349fdb39e6acfac17e7449e40de947746630b、必須CI42184391726 full-gate/check成功。
main95bc500f6acf84e7e08dfb41d7680cda62cdb0d0へ正典統合。src/eng/fonts/packagingとCMake試験2行以外不変、通常exe hash/bytes/更新日時不変。通常build/app起動なし。
2795files全hash保全後、絶対path/同一squash tree/clean・untracked・ignored/link/稼働参照0を確認して専用worktree/output整理、branch/commit保持。
確定原本D:/NeNeFolio/agents/753-session-untitled-native/integration-completion.json。
