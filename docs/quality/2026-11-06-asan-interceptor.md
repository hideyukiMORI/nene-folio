# ASanのCRT interceptor — 2026-11-06

Issue #713。ARC-001/002/003/007/012、C-008/012/016、QLT-002/007/009/010/012、CNF-006、GIT-001〜004、ADR0003/0004/0044。
追加waiver none、既存WVR-0001継続。主担当単体で調査・実装・検証し、検出器の妥当性だけpaste_reviewが読み取り専用で独立静読した。

## 確定した欠陥と変更

native試験に記録されていた`interception_win: unhandled instruction`は、固定LLVM19.1.5の命令解析に由来する。
#711検査exe/PDBの命令列は一か所だけで、標準CRTの`frexp`先頭+4に位置した。元runのload baseは記録しておらず、
最初は推定として扱った。続く小programで実関数addressと警告addressの差4を観測し、同じ対象と確定した。
poisoned領域への直接storeはASanが検出する一方、`frexp`の指数出力先に同じ領域を渡すと書込後にexit0になった。
機能試験の成功を、そのCRT出力先まで検査できた証拠とは扱わない。

`/FUNCTIONPADMIN:6`だけを追加すると、正常入力は0.75/指数1・stderr空、直接storeと`frexp`はuse-after-poisonで停止した。
これは[Microsoftのx64 hotpatch準備](https://learn.microsoft.com/en-us/cpp/build/reference/hotpatch-create-hotpatchable-image)と
[関数前padding](https://learn.microsoft.com/en-us/cpp/build/reference/functionpadmin-create-hotpatchable-image)、
[固定LLVMのhotpatch→trampoline選択](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.5/compiler-rt/lib/interception/interception_win.cpp)に整合する。
`frexp`が指数領域を検査する実装は[共通interceptor](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.5/compiler-rt/lib/sanitizer_common/sanitizer_common_interceptors.inc)にある。

- `eng/targets.cmake`: 全Debug executableへ非incremental linkと6byte paddingを一つの変数から渡す。
  同変数の`sanitizer-link-options.json`をbuildへ生成する。Releaseは空配列で、linkへ適用しない。
- `eng/coverage.py`: 同metadataを検証して測定link・nullable binding proof・CRT proofへ渡す。欠落/非対応値への代替はない。
- `eng/sanitizer_probe.py`: 一つのC sourceと判定器を持つ。所有する16byteの先頭8byteをpoisonし、正常/直接store/CRTの3モードを独立実行する。
  直接storeが検出されなければunpoison/freeしてexit0となり、CRTへ進んで別の検出で通ることを防ぐ。
- `eng/prove-gates.py`: 既存の専用copy内toolchain_smokeを同sourceへ差し替え、正典CMakeで検出を確認してから元source/buildへ復帰する。
- `tests/conformance/test_coverage_sanitizers.py`: metadata・結果判定・timeout記録の反例を既存試験へ追加する。

正常は戻り値/指数・poison状態・stderr空を要求する。反例はmode/poison到達・WRITE4・use-after-poison・非zero終了を要求し、
`frexp`では同関数のstack frameも照合する。vendor source行番号は固定しない。単なる非zeroや別原因の失敗は受理しない。
各modeは15秒timeout。開始時に旧結果を空にし、timeout/起動失敗は部分stdout/stderrと未確定終了値を保存して失敗する。

製品C/API/保存schema/依存/toolchain版/Release設定は変更していない。生成JSONはbuild metadataであり保存形式ではない。
C-016全体のplanned、既存の90%下限、必須CI、警告重大度・除外を維持した。ゲートを強める変更で新ADRは設けない。
通常実機確認用exeはDebugなので、統合後は正典prepare-real-machineで今回のlink設定を反映する。

## 全試行と受理した結果

原本は`D:/NeNeFolio/agents/713-asan-interceptor`と同名outputs、先行静的調査は`D:/NeNeFolio/agents/quality-20261106/asan-investigation`。
対象実行は`tools/record-run.py`の新規run-dirへ入力snapshot/hash・raw stdout/stderr・child/wrapper終了値を記録した。
ASAN_OPTIONS/UBSAN_OPTIONS/ASAN_SYMBOLIZER_PATHはいずれも未設定。Windows11 Pro10.0.26300、固定toolchainはeng/tool-versions.jsonどおり。

| 試行 | 結果 |
| --- | --- |
| build-01 | 正典85sourceのsanitizer vectorから小probeを作成。成功。独立probeはtidy未実行 |
| valid-01 / direct-01 / frexp-01 | 正常exit0＋先頭+4警告、直接store検出exit1、CRT出力先未検出exit0 |
| build-hotpatch-01 | 補助scriptのslash/backslash置換誤りで旧生成先へ出力。予定artifact不在で失敗。baseline exe/PDBは元runの不変snapshotへ保全済み。失敗後の生成fileをbaselineと再分類しない |
| build-hotpatch-02 | 出力先を新directoryへ訂正し成功 |
| hotpatch-valid-01 / hotpatch-direct-01 / hotpatch-frexp-01 | 正常exit0/stderr空、2反例ともASan検出exit1。CRTのstackはsanitizer_common_interceptors.inc |
| build-candidate-01 | input-list生成のPowerShell property誤用でnull項目。子起動前拒否、build未実行 |
| build-candidate-02 | 正典Debugのimage_status_tests/toolchain_smokeを既存tidy付きでbuild成功 |
| test-candidate-01 | 判定器11tests成功後、正典CMakeのprobeがC-012 complexity12>10で拒否。sanitizer実行前に停止 |
| test-candidate-02 | poison期待の条件式を意味不変のbool比較へ簡素化。11tests、正典CMakeの3モード・元smoke復帰・Release非適用、coverageの3モード/nullability proof/測定が成功 |
| native-01 | `ctest --test-dir build -R '^layout_edit_native$' --output-on-failure --verbose`、447checks/1.12秒、child/wrapper0、stderr空、起動警告無し |

測定は`python -B eng/coverage.py`。全測定exeのlink経路を変更したため、この既存測定を一度実行した。
8238/9056branch（90.967314%）で既存90%下限を満たし、省略反例は3.63%で意図した拒否となった。
製品の無関係な操作matrixは追加しない。native447と#711の486との差39は、専用履歴の未存在3slotを13回照合する際、
存在slotの2assertionに対して未存在slotが1assertionになるため。本文・選択・Undo等の条件を削除したものではない。

独立静読ではdirectが後続CRTの検出で通り得る点と、timeout時に旧成功/部分出力が残る点を指摘し、最終対象検証前に訂正した。
GitHub pluginのIssue更新403は認証済みghへ切替えて成功。誤ファイル名/rg正規表現/raw web cache miss等の補助read失敗もjournalに分離した。
最終head CI・正典統合・通常反映/data保持・全保全整理の確定原本はagents内`integration-completion.json`へ収載する。

## 限界

この3モードは全CRT・全OS・全命令列・任意入力のメモリ安全性を証明しない。製品srcには直接frexp呼出しを認めていない。
今回消えた警告以外の未知interceptor失敗を一括無視する規則も追加していない。通常app/実ノート/利用者clipboard/前面focus/Storeを品質試験に使っていない。
Release配布物の再生成/公開は本件の対象ではなく、Releaseの確認は実smoke build/link引数と起動に限定した。
