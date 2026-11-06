# 共有違反の保存拒否とnative編集保持 — 2026-11-06

Issue #729。FR-006/017、ARC-001/004/005/008/009/011/012、C-008/012/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0006/0012/0054/0064。
追加waiver none、既存WVR-0001継続。主担当単体で実装・自己レビュー・検証。製品src/API/保存schemaは不変。

## 固定した境界

既存image_status_testsで、試験所有one.mdだけをCreateFileWのshare0で保持し、保存前の履歴読取を実Windowsの共有違反で拒否する。
applicationのarchive失敗単体に加え、主窓のCtrl+S→themed失敗面→編集保持→解除後再保存を通す恒久回帰。
新しい製品不具合を認定したものではない。専用desktop/data、30秒timeout、既存image_status_data lockを維持する。

- 名前付きoneをEDITで追記し、非空選択/本文/modified/UndoRedo/focusとmd/全履歴/revisionをsnapshotする。
- 排他handleを保ったまま本文Ctrl+Sを送る。archive_noteはsourceのfile_bytes_readが成功してからstore_historyへ進むため、履歴回転前の拒否を確認する。
- 試験thread内でGA_ROOTOWNERが当該主窓に一致する可視#32770だけを対象とし、readonly EDITの履歴保存失敗の理由文とIDOKボタンを確認して、通常のBM_CLICKで閉じる。
- timerは50ms設定・100callbackの有限上限と外側CTest30秒枠。観測完了時/未使用時ともKillTimerし、残ったqueue callbackは無操作で終える。OSの全スケジューリング条件で5秒を保証するものではない。
- 排他handleを解放してから、元md・全5履歴slot・保存revision不変、NAMED/選択先one、EDIT/CHANGEDとnative全文/選択/modified/UndoRedo/focusを照合する。
- 実Undoで元本文/SAME、Redoで追記/CHANGEDへ戻る。各段階で履歴/revision不変を確認し、解除後Ctrl+Sを2回送る。保存は1revisionだけ進み、全履歴は1世代だけ更新する。新mdと履歴1は独立literalのraw bytesに一致し、保存前後のnative状態を保つ。

失敗面を閉じるための観測器は試験側にだけ置き、製品hookや第二の保存/失敗表示経路は追加しない。
既存raw path組立をnative_file_pathへ抽出し、排他openとraw readが同じ検証済みpathを使う。処理の意味は不変。
直接共用するempty_save_nativeで0bytesの実在/履歴rawも確認する。無関係なnative試験はローカルで反復しない。

## 全試行と結果

原本D:/NeNeFolio/agents/729-locked-save-native、試行同outputs、作業木同worktrees。
tools/record-run.pyへ入力snapshot/hash、raw stdout/stderr、生成exe、child/wrapper終了値を収載。

| 試行 | 結果 |
| --- | --- |
| build-01 | 正典Debug/tidy成功、入力不変 |
| test-01 | locked_save_native156checks/0.84秒、直接共用empty_save_native253checks/0.63秒、合計1.47秒成功。stderr空・child/wrapper0、入力不変。 |

verify-01はARC-007が未使用TIMERPROC引数名timeを禁止API名として検出しchild/wrapper1。
実時刻の読取はなく、引数名とその(void)参照をtick_millisへ変更した。build-02はDebug/tidy成功。
test-01の保存sourceへこの改名と正典整形だけを適用したbytesが最終sourceと一致することをrename-reuse.jsonで確認し、動作試験は再利用する。
prepare-pr初回はverify-01失敗を検出して停止した。失敗を迂回せずverify-02で規約検査をやり直す。

固定toolchainのcmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug、cmake --build build --target image_status_tests。
対象コマンドはctest --test-dir build -R '^(locked_save_native|empty_save_native)$' --output-on-failure --verbose。
ASan/UBSan/nullability/tidy維持。format/conformance実build graph/diff、最終head必須CI・正典統合・通常exe不変・全保全整理はintegration-completion.jsonへ確定結果を残す。

## 限界

Windows NT10.0.26300.0、Msftedit10.0.26100.9949、ローカルNTFS・専用desktop/D領域。
履歴読取前の共有違反と解除後の成功が対象。write/履歴回転途中失敗・容量不足・ACL改変・ネットワーク共有は対象外。
履歴回転と実writeの既知の非原子性（ADR0012）を解消したとは主張しない。
themed failure dialogだけを観測し、OS fallback面、全OS/DPI、物理keyboard/IME、外部UIA client、fresh-process復旧は未検証。
通常app/実ノート/前面focus/Storeを使わず、通常buildも反復せずexeのhash/bytes/更新日時を照合する。
