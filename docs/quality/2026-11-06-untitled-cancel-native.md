# 無題の保存・離脱取消 — 2026-11-06（#737）

## 契約と正典経路

FR-006/029/030、ADR0020/0050/0064の取消保持を既存first_save_nativeで固定する。
NEWは公開header Invoke→depart_document→show_name_prompt、明示CtrlSは既存native_key_input/WM_CHAR→SAVEから同じname_promptへ進む。
新しいhost/製品hook/保存経路は作らず、src/API/schema/CMake/依存/timeout/data lockは不変。

- 空の無題、入力済み、実Undo後の空＋Redo保有の3状態で次の新規を取消す。
- 日本語/絵文字/改行を含む入力済み無題では、明示CtrlSも取消す。
- 所有thread/rootowner/visible/#32770/titleで面を特定し、編集可能なname・category LISTBOX・IDCANCEL・name focusを確認。
  次の新規だけdiscard controlが存在し有効、明示保存と別名保存には存在しない。
  名前と別カテゴリを実入力してからbutton取消する。titleは実captionの「無題のノート」「保存」「別名で保存」を独立literalで照合。
- 既存#735のtimerをstart/finishへ共用し、50ms/100callback上限・発見時KillTimer・queue残り無操作を維持。
  root有効/本文一覧無効、取消後面1件破棄/root生存/内容再enableを確認する。
- 取消前後の全native本文/選択/modified/UndoRedo/本文focus、UNTITLED/カテゴリ/空名/選択なし/公開count、
  元md/全履歴/revision・両カテゴリの実md集合/countを比較する。入力済みの非空選択とUndo保有を明示確認する。
- その後の実Redo・既存Ex初回保存・UndoRedo/SAME保存へ続ける。共用observerを使うsave_as_nativeも直接影響する対象として確認する。

本文を再設定して保持を装わず、取消前後のnative snapshotを比較する。
所有thread/timer/window、name_list解放、後続の実UndoRedoを主担当が自己レビューした。独立レビューは不要と判断。
ARC-001/004/005/008/011/012、C-008/012/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0006/0020/0021/0050/0064。
追加waiver none、既存WVR-0001継続。

## 試行と結果

tools/record-run.pyで入力snapshot、raw stdout/stderr、child/wrapper終了値を保持する。

| 試行 | 結果 |
| --- | --- |
| build-01 | 正典toolchainでcmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug、cmake --build build --target first_save_native_tests save_as_native_tests成功。既存sanitizer/tidyを維持 |
| test-01 | ctest --test-dir build -R '^(first_save_native\|save_as_native)$' --output-on-failure --verbose。first_save_native385checks/1.52秒、save_as_native377checks/1.56秒、CTest計3.08秒成功。stderr空・child/wrapper0・入力不変。 |

静的検証はclang-format --dry-run --Werror、conformance実build graph、git diff --check。
成功対象結果は最終sourcehashを照合して再利用し、工程変更だけの反復をしない。
補助readでADR0050の旧推測pathが不存在、実0050-unsaved-note-departureへ訂正。ui_text検索1件は該当なしで終了1、実prompt_title/captionを静読して照合した。製品試験失敗ではない。
Windows11 Pro10.0.26300、Msftedit10.0.26100.9949、既存専用desktop/verification-first-saveとverification-save-as/data、各30秒timeout。
全原本D:/NeNeFolio/agents/737-untitled-cancel-native、試行同outputs、作業木同worktrees。
最終head必須CI→正典統合→通常exe不変→全保全・安全整理の確定原本は同agents/integration-completion.jsonへ収載する。

## 限界

名前面での保存/保存しない承認、保存失敗・資源失敗、主窓終了/OS session/IME中断・物理keyboard・全OS/DPI・外部UIAは対象外。
通常app/実ノート/clipboard/前面focus/Storeを試験に使わない。個別保留466/634/530と旧削除拒否領域を維持する。
