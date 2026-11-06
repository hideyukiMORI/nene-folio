# #747 — 全ノート検索へのfocus保存の失敗と回復

2026-11-06、hideの07:08の継続品質対応に基づく。Issue [#747](https://github.com/hideyukiMORI/nene-folio/issues/747)。
base main `2dda66671c2af2017c7b05e5a1a39cf2f65477b8`。主担当単体実装・自己レビュー。
規則: FR-006/032、ARC-001/004/005/008/011/012、C-002/008/012/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0006/0024/0056/0064/0068/0073。
追加waiver none、既存WVR-0001継続。src/API/schema/依存/既存ゲート不変。

## 正典と試験境界

`image_status_tests.c`の専用desktop/専用data/実RichEdit/実windowを使い、`--filter-save`を追加。
実Ctrl+Shift+Fは既存native_key_inputのthread keyboard状態保存復元→folio_window_translate→TranslateMessage/DispatchMessage順を通す。
検索欄のWM_SETFOCUS→save_on_filter_focus→共通store_body→履歴→md→検索copy更新が正典。製品窓口・失敗注入hookは追加しない。
CMakeは既存exeにfilter_save_nativeを登録し、image_status_dataのRESOURCE_LOCKとTIMEOUT30を指定する2行だけ。

VIEWで全ノート検索欄へ「追記」、別所有のノート内検索語へ「Current」を設定する。
公開idle_work_ready/continue_idle_workを最大1000回で進め、pendingが無いことを要求する。
初期結果を0件とは仮定せず記録し、保存前oneに語が無く保存後本文にあることを独立literalで確認する。
EDITで追記と非空選択を作り、実mdを共有なしhandleで保持して検索focusを求める。

既存のowned failure observerは開始/終了だけを小関数へ抽出し、locked_save_nativeと共有。
同thread/同rootowner/可視#32770/独立した履歴保存失敗文/有効IDOKを限定する既存callback、50ms/最大100callback、KillTimer後queuedcallbackのno-opを維持する。
一つの失敗面を閉じ、handle解放後に元md・全履歴・revision不変を実ファイルで照合する。
本文focus、全文/非空選択/modified/UndoRedo、EDIT/CHANGED/NAMED/元選択、root/body/tree有効、検索語/結果件数/public countを保持する。

解除後の再Ctrl+Shift+Fはfilter focus/語の全選択を保ち、native本文を再投入しない。
元mdの全bytes、全履歴の一回回転、revision+1、検索結果が初期件数+1を要求する。
公開body focusへ戻して同じ操作を繰り返しても、SAME保存は同じ全ファイル/revisionに留まる。
実Undoは元本文/CHANGED、Redoは保存済み本文/SAMEへ戻り、その間も保存物と検索結果/別所有の語を保つ。

## 完成時の検証・全試行

| 試行 | 結果 |
| --- | --- |
| build-01 | 正典toolchainのDebug configure/image_status_tests build成功 |
| test-01 | 関係する2試験成功 |
| test-repeat-01 | toolchain未読のctestを直接指定しWinError2で子起動前失敗。child=null/wrapper125。記録を保全し、source変更なし |
| test-repeat-02 | 正典toolchainを読むrepeat.ps1へ訂正、新run-dirで別条件試験成功 |

locked_save_native 290checks/0.9秒、filter_save_native 213checks/0.65秒、計1.56秒成功。別カテゴリを含む既存一致2件/非一致1件/台帳ありのrepeatはfilter_save_native 213checks/0.66秒、CTest0.67秒成功。runtime stderr空・child/wrapper0・入力不変。追加3mdと台帳の全bytes/hash/作成更新日時不変。

repeatのsource台帳順はone→zzfilter-unmatched→zzfilter-matching。元と別カテゴリに各1件の「追記」入りmd、元に非一致mdを追加。
source初期1件/target空という前提へ依存せず、全既存追加物のbytes/日時保持も外側で照合した。


- Debug/tidy: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`、`cmake --build build --target image_status_tests`。
- 対象: `ctest --test-dir build -R '^(filter_save_native|locked_save_native)$' --output-on-failure --verbose`。
- 別条件repeat: `ctest --test-dir build -R '^filter_save_native$' --output-on-failure --verbose`。
- 整形/規約/差分: `clang-format --dry-run --Werror tests/win32/image_status_tests.c`、`python -B eng/conformance.py --build-dir build`、`git diff --check`。
- 補助探索の誤ったtests/platform pathとPowerShell literal globは読取失敗。実tests/win32とrgのdirectory/-g指定へ訂正し、製品試験と分離した。
- 全試行の入力snapshot/hash/raw stdout/stderr/childとwrapper結果はrecord-runで同outputsへ保持。最終入力が一致する成功結果は再実行せず統合へ使う。

## 統合・通常版・保全

Draftでhead/title/bodyを確定し、正典tools/merge-pr.ps1でReady→最終head必須CI→squash→main同期を行う。
通常exeは製品tree不変とCMakeの試験登録2行だけの変更を照合して再利用する。通常build/app起動は追加しない。
確定結果はD:/NeNeFolio/agents/747-filter-save-native/integration-completion.jsonへ収載する。
最終CI・main統合・通常exe不変・全保全整理はこの文書作成時未完了。
絶対path/取込/clean・untracked・ignored/唯一成果物/links/稼働参照を確認し、全fileをhash照合して保全後に専用worktree/outputを整理する。branch/commit保持。

## 限界

Ctrl+Shift+Fの実native経路と共有違反の履歴拒否を対象とする。全入口やディスク書込途中/容量/ACL/資源失敗ではない。
×ボタンpointer geometry/外部UIA/余白mouse/物理IME/全OS・DPI/UNTITLED/NONE/検索性能/外部更新検知は対象外。
10月18日の専用失敗注入診断は[当時の品質記録](2026-10-18-filter-focus-save.md)と区別する。
通常app/実ノート/clipboard/前面focus/Storeは使わない。個別保留466/634/530と旧削除拒否領域を維持する。

## 統合・保全整理の確定

#747/PR748は最終head7f242ac9755f28e5b21c5a9705db20334c2a7784、必須CI42178271367 full-gate/check成功。
main7d5f1ad92cff5c0ac0734ff0a4a6bffa40e7c722へ正典統合。製品treeとCMake試験登録2行以外の一致を確認し、通常exeのhash/bytes/更新日時不変、通常build/app起動なし。
2772filesを全hash保全し、絶対path/同一squash tree/clean・untracked・ignored/link/稼働参照0確認後に専用worktree/outputを整理。branch/commit保持。
repeat01の子起動前PATH不足wrapper125も原本へ保持。確定原本D:/NeNeFolio/agents/747-filter-save-native/integration-completion.json。
