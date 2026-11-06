# #749 — 通常終了の保存拒否と解除後の再終了

2026-11-06、hideの07:08の継続品質対応に基づく。[Issue #749](https://github.com/hideyukiMORI/nene-folio/issues/749)。
base main `7d5f1ad92cff5c0ac0734ff0a4a6bffa40e7c722`。主担当単体実装・自己レビュー。
FR-006/015、ARC-001/004/005/008/011/012、C-002/008/012/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0006/0016/0050/0053/0064。
追加waiver none、既存WVR-0001継続。src/API/schema/CMake/依存/ゲート不変。

## 正典と固定する境界

既存locked_save_native（image_status_tests --locked-save）の実Win32窓・専用desktop/data/実RichEdit/実mdを使う。
通常終了の正典はWM_CLOSE→close_windowのdeferred request→run_normal_close→depart_document→共通store_body→finish_quit。
名前付きの保存拒否は終了せず、本文と窓を保持する。#677のtheme正常終了と#309のOS通知診断から分離する。

- 既存trash拒否・CtrlS共有違反拒否・解除後保存/SAME保存の後に、実Undoで保存済み本文から異なる本文へ戻す。全md/履歴/revisionを新snapshotに取り、Redoと非空選択を残す。
- 専用one.mdだけを共有なしhandleで保持し、同rootへWM_CLOSEを送る。既存owned有限failure observerで履歴保存失敗面を一つだけ確認して閉じる。
- 最大1000messageの拒否用pumpはWM_QUITを無視せず失敗とし、各消息と終了時のroot生存を要求する。通常pumpと同じtranslate/dispatch/WM_TIMER扱いで、製品への新窓口を作らない。
- 拒否後に公開rootの同一性・root/body/tree有効・本文focus/全文/選択/modified/UndoRedo・EDIT/CHANGED/NAMED/元選択/countを照合する。handle解放後に元md/全履歴/revision不変を実ファイルで確認する。
- 実Redoで保存済み本文/SAME、再Undoで終了保存対象/CHANGEDへ戻し、保存物が不変と確認する。
- 同じrootへの再WM_CLOSEは既存close_theme_window helperを変更せず使い、root破棄・公開root null・WM_QUIT0を確認する。全md bytesと全履歴一回shift、revision+1を要求する。

変更はimage_status_tests.cの専用3helperとlocked_save runnerの1callだけ。他case/共有observer/終了helperは不変。
既存30秒timeoutとimage_status_data resource lockを維持する。最後のfolio_window_destroyは所有資源の解放にだけ使う。

## 完成時の検証・全試行

| 試行 | 結果 |
| --- | --- |
| build-01 | 正典Debug/tidy configureと対象build成功 |
| test-01 | 終了要求前の複合前提assertで失敗。製品closeは未実行 |
| build-diagnostic-01 / test-diagnostic-01 | 前提assertを分けてbuild成功、本文focus前提だけで失敗と特定。先行trash操作の索引focusが残っていた |
| build-02 / test-02 | 公開ACCESSIBLE_BODYのFOCUSで本文へ戻す準備を追加。期待を緩めずbuild/対象試験成功 |
| test-repeat-01 | 既存追加物と非辞書順台帳の別条件で成功 |

locked_save_native 426checks/1.08秒、CTest1.08秒成功。台帳あり/source2追加md/target1追加mdのrepeatは458checks/1.17秒、CTest1.17秒成功。成功runtime stderr空・child/wrapper0・入力不変。追加3mdと台帳の全bytes/hash/作成更新日時不変。

初回と診断の失敗は全原本に保持。失敗終了値はtest-summary.jsonへchild/wrapperを分けて収載する。
repeat台帳順はone→zzclose-second→zzclose-first。fixtureがseedで上書きするoneと、保持する追加物3md/台帳を区別する。


正典toolchainで`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`、`cmake --build build --target image_status_tests`。
`ctest --test-dir build -R '^locked_save_native$' --output-on-failure --verbose`で直接変更した境界だけを実行する。
別条件repeatは追加source2md/target1mdと非辞書順のsource台帳を持ち、追加物の全bytes/hash/作成更新日時を外側でも照合する。
`clang-format --dry-run --Werror tests/win32/image_status_tests.c`、`python -B eng/conformance.py --build-dir build`、`git diff --check`。
全試行はrecord-runの入力snapshot/hash/raw stdout/stderr/childとwrapper結果へ残し、成功最終入力は再実行せず再利用する。

## 統合・通常版・保全

Draftでhead/title/bodyを確定し、正典tools/merge-pr.ps1でReady→最終head必須CI→squash→main同期を行う。
通常exeはsrc/eng/fonts/packaging/CMake不変を照合し再利用する。通常build/app起動は追加しない。
確定結果はD:/NeNeFolio/agents/749-close-busy-native/integration-completion.jsonへ収載する。
最終CI・main統合・通常exe不変・全保全整理は文書作成時未完了。
絶対path/取込/clean・untracked・ignored/唯一成果物/links/稼働参照を確認し、全fileをhash照合して保全後に専用worktree/outputを整理する。branch/commit保持。

## 限界

名前付き本文・通常WM_CLOSE・共有違反での履歴拒否と解除後保存だけを対象とする。
UNTITLEDの名前面/破棄/取消、theme/modalを開いた終了の全組合せ、OSsession/実ログオフ/強制終了、IME/物理AltF4/外部UIA、容量/ACL/部分書込/資源失敗は対象外。
通常app/実ノート/clipboard/前面focus/Storeは使わない。個別保留466/634/530と旧削除拒否領域を維持する。

## 統合・保全整理の確定

#749/PR750は最終heada3f962d6327a70d1aaa80ae6ca69652f146e6909、必須CI42179944433 full-gate/check成功。
mainf9b0151962f14a5cd433062a38c5b9d1aa259e78へ正典統合、src/eng/fonts/packaging/CMake不変と通常exeのhash/bytes/更新日時不変を確認し、通常build/app起動なし。
4202filesを全hash保全し、絶対path/同一squash tree/clean・untracked・ignored/link/稼働参照0確認後、専用worktree/outputを整理。branch/commit保持。
確定原本D:/NeNeFolio/agents/749-close-busy-native/integration-completion.json。
