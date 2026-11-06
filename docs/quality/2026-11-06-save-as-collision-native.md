# 別名保存の名前衝突 — 2026-11-06（#733）

## 目的・契約・範囲

FR-006/030とADR0021決定3は、別名保存の公開前失敗で元ノートと未保存本文を保持する。
#715の既存save_as_nativeに、未保存EDITからの:saveas oneと:saveas ONEの2拒否を追加する。
前者はfolio_state_store_newの既知名拒否、後者はWindowsの既存file aliasに対するcreate_note/FILE_WRITE_CREATEの公開拒否を通す。
製品の不具合はこの調査で未確認。src/API/schema/CMake/timeout/data lockは不変で、試験だけを強める。

実Exは同じ入力語とfocus/可視性を保ち、既存の名前衝突理由を公開する。Esc後もnative本文/非空選択/modified/UndoRedo、
EDIT/CHANGED、NAMED/元選択名、store_revision、元md/全履歴、公開/永続台帳の全行順/countと台帳の有無が保持されることを検査する。
拒否後の実Undo/Redoと、従来の変更EDIT/Redo保有/VIEWで3コピーを作成する成功経路も同じ試行で確認する。
scan_notesの全名前集合/countと、対象category内.nenefolio-write-*.tmp不在を前後で確認する。既存fixtureファイルを削除しない。

ONEとoneが同じ物理fileを指す条件は、読取属性だけの短期handleからVolumeSerialNumberとFileIndexHigh/Lowを照合する。
handleは操作前に閉じ、共有違反を試験自身で作らない。GetFileInformationByHandleの
[Microsoft公式仕様](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileinformationbyhandle)を確認した。
専用directoryの実条件を検査し、case-sensitive directoryの挙動一般へ広げない。

規則: ARC-001/004/005/008/011/012、C-008/014/016/017、QLT-008/010/012/013、GIT-001〜004。
変更はtests/win32/image_status_tests.c。既存native_file_pathは呼出し位置の前へ移しただけでbytes/意味不変。
新しい製品hook/第二の保存経路/host/環境依存は加えない。主担当が単体で自己レビューした。
テスト専用の有限操作とhandle/ledger/name_list所有を確認し、この変更に独立レビューは追加しない。
追加waiver none、既存WVR-0001継続。

## 実行と原本

固定toolchainのcmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug、
cmake --build build --target save_as_native_tests。ASan/UBSan/nullability/no-recover/tidyは維持。
tools/record-run.pyで入力/生成exe snapshot、raw stdout/stderr、child/wrapper終了値を保持する。

| 試行 | 結果 |
| --- | --- |
| build-01 | Debug/tidy対象build成功、child/wrapper0、入力不変 |
| test-01 | 1.38秒、child/wrapper8。台帳を必ず既存とする試験前提が誤り、最初の衝突操作より前で拒否。製品のredではない |
| build-02 | 台帳なし/ありの両方を保持する正しいfixture契約へ訂正。公開行は別snapshot、永続台帳は存在状態と全行を照合。Debug/tidy成功 |
| test-02 / test-repeat-01 | ctest --test-dir build -R '^save_as_native$' --output-on-failure --verbose。台帳なしのsave_as_native302checks/1.39秒、台帳ありの再実行332checks/1.03秒成功。両stderr空・child/wrapper0・入力不変。旧3コピーのbytes/hash/作成更新日時と履歴不在も保持。 |

clang-format --dry-run --Werror、conformance実build graph、git diff --checkを対象の静的検証に使う。
test-repeat-01は成功で作成した実台帳が存在する条件と、既存コピー保持という別の境界を確認する1回である。
成功対象入力が不変のままPR/CI/統合へ再利用し、工程変更だけでは再試行しない。
Windows11 Pro10.0.26300、Msftedit10.0.26100.9949。既存専用desktop/verification-save-as/dataだけを使い、30秒timeoutとdata lockを維持。
原本D:/NeNeFolio/agents/733-save-as-collision-native、試行同outputs、作業木同worktrees。
最終head必須CI→正典統合→通常exe不変→全保全・安全整理の確定原本は同agents/integration-completion.jsonへ収載する。

補助readでtests/integration/folio_window_native_tests.c、ADR0021-save-as-note.md、729のprepare-final-pr.pyを誤指定した。
実tests/win32/image_status_tests.c、ADR0021-save-as-preserves-original.md、729/prepare-pr.pyへ訂正した。
最初の整形呼出しは存在しないC:/Program Files/LLVM/binを指定して失敗、正典eng/toolchain.ps1の固定LLVMへ訂正して整形した。
補助readで715/repeat-test.ps1も不存在だったため実repeat.pyから既存の呼出しを確認した。
これらは製品build/runtimeの失敗とは区別する。初回台帳前提の誤りも全rawを保持した。

## 限界

対象は既定のcase-insensitiveな専用Windows directory内の、同期Ex保存の既知同名とASCII case alias。
並行レース、case-sensitive directory、Unicode正規化/長いpath/予約名、名前modal/ACL/容量不足/台帳部分失敗、
物理keyboard/IME/外部UIA client/全OS/DPIの確認ではない。通常app/実ノート/clipboard/前面focus/Storeは試験に使わない。
個別保留466/634/530と旧削除拒否領域を維持する。
