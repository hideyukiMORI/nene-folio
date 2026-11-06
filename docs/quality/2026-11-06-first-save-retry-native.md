# 初回保存の名前拒否と同じ画面での訂正 — 2026-11-06（#743）

## 契約と正典経路

FR-029/ADR0020の初回保存の名前面は、入力を拒否しても無題の本文を残し、理由を表示して訂正を受ける。
#741の恒久first_save_modal_nativeに、実CtrlSで開いた同一の面で空名/予約名CON/元カテゴリscheduleの既存oneを順に送る境界を追加する。
9月13日の部品診断の予約名/同名の結果と区別し、現行のname_prompt submit/show_failure/note_name/store_newをそのまま通す。
製品src/API/schema/CMake/依存/ゲート不変。製品不具合を認定する変更ではない。

- 所有thread/rootowner/class/title/controlを限定した既存observerの閉じたactionにRETRY_SAVEを加える。保存承認は既存と同じBM_CLICKの一つのhelperへ揃える。
- 空名は「名前を入力してください。」、CONは予約名等の理由、oneは同名ノートの理由を、独立した日本語literalで全文照合する。
- 各拒否で同一dialogの生存/可視性、名前欄focus、全入力文字列/カテゴリ保持、readonly multiline理由欄の可視性、有効Save/Cancel、root有効/本文一覧無効を確認する。
- native全本文/非空選択/modified/UndoRedo、UNTITLED/元カテゴリ/空文書名/選択なし/count/既存cursorを保持する。面内focusは名前欄、最終終了後focusは#741の本文snapshotで区別する。
- 拒否の各回で元md/全履歴/store_revisionと、両カテゴリ実md集合・台帳の有無/全行/順序を照合する。台帳が存在するときはpublic全行とも一致する。
- 最後に有効な日本語/空白/絵文字名とtargetへ訂正し、同じ面のSaveで成功する。#741の全保存結果/元保存物保持/実UndoRedo/SAME保存へ続ける。
- 期待値contextは同期呼出しのstack上のnative/保存物snapshotを借り、面の終了まで有効。各台帳snapshotを終了時に破棄する。
- 発見時KillTimerとqueue残り無操作、50ms/100callback上限、30秒timeoutを維持する。拒否は同期的に同じ面へ戻るため、callback内の有限3件を確認し、別timer状態機械を作らない。
- 別目的repeatではtarget台帳ありで同じ3拒否を確認する。前回target1fileの全bytes/hash/作成更新日時/集合と履歴不在を保持し、新名1件だけ追加する。source oneはseed書込をするので外側の不変対象は旧targetである。

主担当単体で自己レビュー。新しい製品経路/保存ポートはなく、独立レビューは不要と判断。
FR-006/029/030、ARC-001/004/005/008/011/012、C-002/008/012/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0006/0020/0021/0050/0064。
追加waiver none、既存WVR-0001継続。

## 試行と結果

tools/record-run.pyで入力snapshot、raw stdout/stderr、child/wrapper終了値を保持する。

| 試行 | 結果 |
| --- | --- |
| build-01 | 正典toolchainのDebug configure、cmake --build build --target first_save_modal_native_tests first_save_native_tests save_as_native_tests成功。既存sanitizer/tidyを維持 |
| test-01 | ctest --test-dir build -R '^(first_save_modal_native\|first_save_native\|save_as_native)$' --output-on-failure --verbose。first_save_modal_native308checks/1.19秒、first_save_native485checks/1.76秒、save_as_native377checks/1.98秒、CTest計4.93秒成功。stderr空・child/wrapper0・入力不変。 |
| test-repeat-01 | 新規caseだけ同じCTest経路で再実行。first_save_modal_native339checks/1.13秒、CTest1.13秒成功。旧target1fileの全bytes/hash/作成更新日時保持、新旧history不在、新名1件のみ追加。stderr空・child/wrapper0・入力不変。 |

静的検証はclang-format --dry-run --Werror、conformance実build graph、git diff --check。
共用observerが直接変わるfirst_save_native/save_as_nativeを含め、無関係なローカル全件試験は実行しない。
成功対象結果は最終sourcehashを照合して再利用する。改行変換warningはrawへ保持。
Windows11 Pro10.0.26300、Msftedit10.0.26100.9949、専用desktop/verification-first-save-modal・verification-first-save・verification-save-as/data、各30秒timeout。
全原本D:/NeNeFolio/agents/743-first-save-retry-native、試行同outputs、作業木同worktrees。
最終head必須CI→正典統合→通常exe不変→全保全・安全整理の確定原本は同agents/integration-completion.jsonへ収載する。

## 限界

OS大小文字alias/最大長/全予約名列挙、実ディスク保存失敗/台帳部分失敗/資源失敗、離脱時保存承認/別ノート切替/主窓終了/OSsession/IME中断、物理keyboard/Enter・全OS/DPI・外部UIAは対象外。
通常app/実ノート/clipboard/前面focus/Storeを試験に使わない。個別保留466/634/530と旧削除拒否領域を維持する。

## 統合・保全整理の確定

#743/PR744は最終head21f81e16a44ff2f0381418fcab2621e8fe3bb788、必須CI42173704433 full-gate/check成功。
main8c81e0402156e21c3334bd54c973288576990052へ正典統合し、通常exeのhash/bytes/更新日時不変を確認した。src/eng/fonts/packaging/CMake不変で通常build/app起動なし。
2812filesを全hash保全し、絶対path/同一squash tree/clean・untracked・ignored/link/稼働参照0を確認後、専用worktree/outputを整理。
branch/commit保持。確定原本D:/NeNeFolio/agents/743-first-save-retry-native/integration-completion.json。
