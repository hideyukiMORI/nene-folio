# 無題を保存せず次の新規へ進む — 2026-11-06（#739）

## 契約と正典経路

FR-029/ADR0020/0050/0051/0064の「保存しない」を、既存first_save_nativeの名前入力面から確認する。
公開header NEW→new_note_at/depart_document→show_name_prompt/name_prompt reads_as_discard→一回の許可→folio_state_new_note/pour_edit_rtfが正典経路。
中核state_testsの許可試験と、名前付きの編集破棄（#702/native discard_case）を維持し、無題の実面での承認を追加する。
src/API/schema/CMake/依存/timeout/data lockは不変。製品不具合を認定する変更ではない。

- 日本語/絵文字/改行を含む入力済み無題と、実Undoにより空＋Redo保有になった無題で、次の新規を要求する。
- 実面の名前と別カテゴリを入力し、文字列「保存しない(&D)」と有効なbuttonを確認してBM_CLICKする。
- 次の無題は、確認面の選択に左右されずNEWの元カテゴリ0に留まる。空名/選択行なし/公開count不変とEDITの空本文を確認する。
- 本文focus/先頭caret/modified0/Undo0/Redo0を確認。実EM_UNDO/EM_REDOも0で、空本文・native状態が変わらないことを比較する。
- 元md/全履歴/store_revision・両カテゴリの実md集合/countを保持。名前面の指定先に保存物を作らない。
- 後続の既存取消保持・通常入力・Ex初回保存・UndoRedo/SAME保存へ続ける。
- #735/737のobserverを1つのまま共用し、Cの閉じたenumで取消/WM_CLOSE/保存しないを網羅する。
  所有thread/rootowner/visible/class/title/control/focus、50ms/100callback上限、発見時KillTimer、queue残り無操作を維持。
  root有効/本文一覧無効と終了後のroot生存/内容再enableも引き続き確認する。

実体を持つ名前入力と新規作成を主担当が自己レビューした。新しい製品経路・hook・保存ポートは追加せず、独立レビューは不要と判断。
FR-006/029/030、ARC-001/004/005/008/011/012、C-002/008/012/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0006/0020/0021/0050/0051/0064。
追加waiver none、既存WVR-0001継続。

## 試行と結果

tools/record-run.pyで入力snapshot、raw stdout/stderr、child/wrapper終了値を保持する。

| 試行 | 結果 |
| --- | --- |
| build-01 | 正典toolchainでcmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug、cmake --build build --target first_save_native_tests save_as_native_tests成功。既存sanitizer/tidyを維持 |
| test-01 | ctest --test-dir build -R '^(first_save_native\|save_as_native)$' --output-on-failure --verbose。first_save_native485checks/1.99秒、save_as_native377checks/1.82秒、CTest計3.81秒成功。stderr空・child/wrapper0・入力不変。 |

静的検証はclang-format --dry-run --Werror、conformance実build graph、git diff --check。
共用observerが変わるsave_as_nativeを直接影響する境界として確認し、無関係なローカル全件試験は実行しない。
成功対象結果は最終sourcehashを照合して再利用する。
編集準備helperの初回は整形後の改行を含む引数宣言に一致せずassertで停止、source書込なし。
宣言断片の正確な一致へ訂正後に適用。追加patch初回も整形済みの条件式に一致せず未適用、実文脈へ訂正してbuild前に適用した。
これらは製品試験失敗ではない。改行変換warningはrawへ保持。
Windows11 Pro10.0.26300、Msftedit10.0.26100.9949、既存専用desktop/verification-first-saveとverification-save-as/data、各30秒timeout。
全原本D:/NeNeFolio/agents/739-untitled-discard-native、試行同outputs、作業木同worktrees。
最終head必須CI→正典統合→通常exe不変→全保全・安全整理の確定原本は同agents/integration-completion.jsonへ収載する。

## 限界

名前面での保存承認・validation/保存失敗・資源失敗、別ノート切替/主窓終了/OS session/IME中断、物理keyboard/Enter・全OS/DPI・外部UIAは対象外。
通常app/実ノート/clipboard/前面focus/Storeを試験に使わない。個別保留466/634/530と旧削除拒否領域を維持する。
