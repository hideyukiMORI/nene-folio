# OS終了通知の保存拒否・承認・取消のnative回帰 — 2026-11-06

Issue #751。FR-006/015/018/019、ARC-001/004/005/008/011/012、C-002/008/012/014/016/017/018、QLT-008/010/012/013、GIT-001〜004、ADR0006/0050/0053/0064。
追加waiver none、既存WVR-0001継続。

## 問題と正典経路

ADR0053はQUERYで共通保存が成功してから終了へ同意し、同意後は入力を止め、取消なら入力を戻し、承認後のEND(TRUE)だけで最終破棄する。
#309の専用診断は存在するが、現行恒久CTestにはこのOS通知経路が無かった。製品不具合の新規確認ではなく、既存契約の恒久回帰を追加した。
`image_status_tests --session-save`が専用desktop/dataの実Win32窓・RichEdit・md・履歴を使う。専用rootへQUERY/ENDを送るだけで実OSログオフや再起動は行わない。
製品はquery_session_end→store_body→既存履歴/保存、取消はcancel_session、最終破棄はfinish_quitの既存経路を使う。

## 固定した境界

- 履歴付きoneをEDITで開き、日本語/絵文字追記、非空選択、modified/Undo、本文focusを準備する。
- 専用mdを共有なしhandleで保持してQUERY(LOGOFF)。FALSE・同root生存/有効・公開ready、本文/選択/modified/UndoRedo/focus、EDIT/CHANGED/NAMED/元選択/countを保持する。独立literalでOS通知用の保存未完了理由を照合する。
- modal observerなしで同期通知が戻る。modalへ逸脱すればCTest30秒で失敗する。拒否後のEND(TRUE)は破棄/WM_QUITを起こさず、END(FALSE)で理由を解除し入力を保持する。handle解除後のmd/全履歴/revisionは不変。
- 再QUERYはTRUE、期待md/全履歴一回shift/revision+1。root無効/公開ready=false、重複QUERYは追加保存なし。実folio_window_translateへの本文キーが消費され、全文/selection/modified/UndoRedoを保持する。無効中のfocus位置は保存取消後と分けて扱う。
- END(FALSE)で同root有効/公開ready=true、元本文focusとnative状態が復帰する。保存済み内容はSAMEで保存物を巻き戻さない。実Undo/Redo後の再QUERYも無追加保存。
- 承認後END(TRUE)でroot破棄・公開root=null・WM_QUIT0。保存物/履歴/revisionは最初の成功保存一回のまま。最後のfolio_window_destroyは所有資源の解放だけ。

## 対象検証と全試行

| 試行 | 結果 |
| --- | --- |
| build-01 | 新selectorでrun_extended_fixtureの複雑度11が上限10を超えtidy失敗。child/wrapper1、runtime未起動。記録保持 |
| build-02 | run_fixtureへ移したが同関数も複雑度11/上限10で失敗。child/wrapper1、runtime未起動 |
| build-03 | 新caseか既存runnerかを選ぶ短いrun_remaining_fixtureへ分離し、既存runnerの条件不変でDebug/tidy成功 |
| test-01 | 新session_save_native成功 |
| test-repeat-01 | 台帳と追加ノートを保持する別条件成功 |
| test-routing-01 | 新selectorがfalseの場合の既存locked_save_nativeへの転送も対象確認 |

session_save_native 233checks/0.78秒、CTest0.78秒成功。台帳あり/source追加2md/target追加1mdのrepeatは253checks/0.63秒、CTest0.63秒成功。runtime stderr空・child/wrapper0・入力不変。追加3mdと台帳の全bytes/hash/作成更新日時不変。 既存振分け確認locked_save_native 459checks/1.14秒、CTest1.15秒成功。

repeatの非辞書順台帳はone→zzsession-second→zzsession-first。追加3mdと既存台帳を外側でも全bytes/hash/作成更新日時照合した。


正典toolchainでDebug configure、`cmake --build build --target image_status_tests`。
`ctest --test-dir build -R '^session_save_native$' --output-on-failure --verbose`。
`clang-format --dry-run --Werror tests/win32/image_status_tests.c`、`python -B eng/conformance.py --build-dir build`、`git diff --check`。
成功済み対象結果は最終sourcehashを照合して再利用。最終headの必須CI `pwsh -NoProfile -File ./eng/check.ps1`で統合を確認する。
入力/source/raw stdout/stderr/child・wrapper exit、全準備と自己レビューはD:/NeNeFolio/agents/751-session-save-nativeと同outputsに収載。
補助静読でtests/image_status_tests.cと旧agentsのprepare-source.pyを推測して不存在だった。rg --filesでtests/win32の実pathと既存helperを確認して訂正した。製品試験とは分離する。

## 変更と受理条件

製品src/API/schema/依存/既存ゲートは不変。CMakeはsession_save_nativeの登録と既存data lock/TIMEOUT30の2行だけ。
主担当単体の自己レビュー。新caseとselector/seed条件以外の既存sourceが変わらないこと、CMake試験2行以外がbaseと一致することも確認する。
品質記録/QUALITY_GATES/current/日報/引き継ぎへ#749確定結果と今回の全試行を収載する。
最終head必須CI→正典統合→製品treeとCMake差分の照合による通常exe再利用→全保全・安全整理。確定原本は同agents/integration-completion.json。

## 限界

名前付き本文・LOGOFF通知・共有違反による履歴保存拒否と解除後の保存だけ。実ログオフ/WindowsUpdate/再起動/強制終了/電断、全flags、UNTITLED/改名pending/削除/IME/modalの全組合せ、保存中再入/取消/理由API故障注入、書込途中/容量/ACL/公開後失敗/資源失敗は対象外。
OS側の理由表示成功や待機時間内の保存完了は保証しない。#309の旧172件は今回の実測に加算しない。
通常app/実ノート/clipboard/前面focus/Storeを使わない。個別保留466/634/530と旧削除拒否領域は維持。
