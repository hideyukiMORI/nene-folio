# 別名保存の名前入力取消 — 2026-11-06（#735）

## 契約と正典経路

FR-006/030、ADR0020/0021/0064の取消保持を、既存save_as_nativeの実HACCELから確認する。
Ctrl+Shift+S→command_save_as→show_name_prompt→name_prompt_show→native cancel→end_native_modal/restore_valid_focusが唯一の経路。
製品不具合を認定する変更ではなく、既存の同期名前入力とnative編集保持を恒久回帰にする。
変更はtests/win32/image_status_tests.cと文書。src/API/schema/CMake/依存/計装/timeout/data lockは不変。

- 変更したEDITから名前と別カテゴリを入力し、キャンセルbuttonのBM_CLICKで取消す。
- 最初の別名保存成功後、実UndoによりRedoが残るEDITから同じ面を開き、入力後に名前面WM_CLOSEで取消す。
- どちらも専用threadのtop-levelだけを列挙し、GA_ROOTOWNER/visible/#32770/title「別名で保存」を照合する。
  編集可能なname EDIT・カテゴリLISTBOX・IDCANCEL、name focus、root有効/本文・一覧無効を確認してから操作する。
- 50ms timerは最大100callback、発見後にKillTimerしてqueue残りは無操作。面1件の破棄・root生存/内容の再有効化を確認する。
- 取消後の全native本文/非空選択/modified/UndoRedo/本文focus、NAMED/元名/選択、store_revision、
  両カテゴリの実md集合/count、元md/全履歴と作成済みコピーが保持される。既存の実UndoRedo・衝突拒否・3コピー成功へ続ける。

本文を設定し直して復元したことにせず、取消直後のnative snapshotを操作前と比較する。
name LISTBOXは試験所有のtargetカテゴリを選ぶだけで、保存を承認しない。通常のdesktopへ切り替えない。
ループ・timer・dialog/ledger/name_listの所有を主担当が自己レビューした。追加の独立レビューは不要と判断。
ARC-001/004/005/008/011/012、C-008/012/014/016/017、QLT-008/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。

## 試行と結果

tools/record-run.pyで入力/成果物snapshot、raw stdout/stderr、child/wrapper終了値を保持する。
buildは正典toolchainのcmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debugと
cmake --build build --target save_as_native_tests。ASan/UBSan/nullability/no-recover/tidyを維持する。

| 試行 | 結果 |
| --- | --- |
| build-01 | Debug/tidy対象build成功、child/wrapper0、入力不変 |
| test-01 | 0.96秒、child/wrapper8。抽象TREEのhost HWNDを実drawerと誤認し、modal中のdisable検査で停止。製品のredではない |
| build-02 | ACCESSIBLE_DRAWERの実HWNDを使い、rootと別物であること・所有childを確認する試験へ訂正。Debug/tidy成功 |
| test-02 | ctest --test-dir build -R '^save_as_native$' --output-on-failure --verbose。save_as_native389checks/1.83秒、CTest計1.84秒成功。stderr空・child/wrapper0・入力不変。 |

clang-format --dry-run --Werror、conformance実build graph、git diff --checkを静的検証に使う。
成功した対象結果は関連source/exe不変を照合してPR/CI/統合へ再利用する。#733の既存台帳あり再実行はその確定原本を参照し、
今回は取消の新境界を確認し、初回失敗は上表の試験側誤認を訂正して再実行した。成功後に工程変更だけの再試行はしない。
Windows11 Pro10.0.26300、Msftedit10.0.26100.9949、専用desktop/verification-save-as/data、30秒timeoutと既存lock。
原本D:/NeNeFolio/agents/735-save-as-cancel-native、試行同outputs、作業木同worktrees。
最終head必須CI→正典統合→通常exe不変→全保全・安全整理の確定原本は同agents/integration-completion.jsonへ収載する。

## 限界

物理keyboard/IME、名前面の保存成功/衝突/validation/資源失敗、全OS/DPI、主窓終了・再入の広い回帰、外部UIA readerを検査したものではない。
通常app/実ノート/clipboard/前面focus/Storeは使わない。個別保留466/634/530と旧削除拒否領域を維持。
