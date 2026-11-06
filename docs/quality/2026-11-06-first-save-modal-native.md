# 名前入力面から別カテゴリへ初回保存 — 2026-11-06（#741）

## 契約と正典経路

FR-029/ADR0020の初回保存を実CtrlS→名前面→保存buttonのBM_CLICKで固定する。
既存folio_window/save/name_prompt→folio_state_store_new→create_note/台帳公開が正典経路。
9月13日の部品診断には保存成功例があるが、現恒久first_save_nativeの成功はEx経由だった。
既存CMake共通host/icon登録に専用first_save_modal_native_testsとverification-first-save-modal出力を追加した。
別カテゴリに残るnoteを、targetが空と仮定する既存first_save_nativeのfixtureへ混ぜない。
製品src/API/schema/依存/ゲートは不変。製品不具合を認定する変更ではない。

- 保存済みoneから実NEWで無題を作り、日本語/絵文字/改行本文と非空選択を入力する。元md/全履歴/revision、両カテゴリの保存物を保持。
- 同thread/rootowner/class/title/control限定の既存observerへSAVEと入力名を追加。実CtrlSで「保存」を開き、日本語/空白/絵文字を含む未使用名とtargetカテゴリを指定する。
- 保存前後のnative全本文/非空選択/modified/UndoRedo/本文focusを完全比較。UNTITLED→NAMED、targetの選択/名前、SAMEと元カテゴリcursor保持を確認する。
- 独立UTF8 literalと新mdのLF全bytesを照合。初回history全slot不存在、全公開数+1、全台帳行/順序と実md集合、元md/全履歴保持、store_revision+1を確認。
- 実Undoで空→RedoでSAMEへ戻る。SAME CtrlSを2回送ってもnative状態・revision・保存物/履歴を増やさない。
- 別目的の再実行では、前回targetファイルの全bytes/hash/作成更新日時/集合を保持して次の未使用名だけ追加する。source oneのseed書込を外側の不変対象に含めない。
- observerの50ms/100callback、発見時KillTimer/queue残り無操作、30秒timeout、root有効/内容無効と終了後の再enableを維持。

主担当単体の自己レビュー。新しい製品経路・hook・保存ポートはなく、独立レビューは不要と判断。
FR-006/029/030、ARC-001/004/005/008/011/012、C-002/008/012/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0006/0020/0021/0050/0064。
追加waiver none、既存WVR-0001継続。

## 試行と結果

tools/record-run.pyで入力snapshot、raw stdout/stderr、child/wrapper終了値を保持する。

| 試行 | 結果 |
| --- | --- |
| build-01 | tidyの引数数4/関数長60行上限に新規試験が違反しchild/wrapper1。runtime未実行 |
| build-02 | requestと保存物snapshotへ関連値をまとめ、draft/accept/roundtripへ分割。正典toolchainのDebug configure、3対象build成功。既存sanitizer/tidyを維持 |
| test-01 | 3対象CTest、first_save_native485checks/2.18秒、save_as_native377checks/2.15秒成功。新規caseはNEWによるcursor解除を見落とし、保存名面に入る前の準備確認で0.68秒失敗。計5.02秒、child/wrapper8。製品不具合ではない |
| build-03 / test-02 | 無題作成後に公開focus_cursorを置く準備へ訂正。newcaseのみDebug/tidy buildとCTest成功。first_save_native485checks/2.18秒、save_as_native377checks/2.15秒、first_save_modal_native219checks/1.59秒。前2件はtest-01の成功結果を再利用。新規caseはtest-02でCTest1.61秒成功、stderr空・child/wrapper0・入力不変。 |
| test-repeat-01 | 新規caseだけ同じCTest経路で再実行。first_save_modal_native233checks/1.05秒、CTest1.06秒成功。旧target1fileの全bytes/hash/作成更新日時保持、旧/新とも履歴不在、新名1件のみ追加。stderr空・child/wrapper0・入力不変。 |

静的検証はclang-format --dry-run --Werror、conformance実build graph、git diff --check。
共用observerが直接変わるfirst_save_native/save_as_nativeを含め、無関係なローカル全件試験は実行しない。
成功対象結果は最終sourcehashを照合して再利用する。
test-01の共用2caseは、その後の変更が#741の専用section内だけであることを機械比較して再利用した。
補助readでadaptersのfolio_window literal glob/applicationのaccessible_kindを誤指定したが、実src/ui/win32へ訂正した。製品試験失敗ではない。改行変換warningはrawへ保持。
Windows11 Pro10.0.26300、Msftedit10.0.26100.9949、既存専用desktop/各verification出力/data、各30秒timeout。
全原本D:/NeNeFolio/agents/741-first-save-modal-native、試行同outputs、作業木同worktrees。
最終head必須CI→正典統合→通常exe不変→全保全・安全整理の確定原本は同agents/integration-completion.jsonへ収載する。
通常exe再利用はsrc/eng/fonts/packaging不変に加え、CMakeから今回の試験登録だけ除くと前mainに一致することを機械比較する。

## 限界

名前面validation/同名/保存失敗/台帳部分失敗/資源失敗、離脱時保存承認・別ノート切替/主窓終了/OSsession/IME中断、物理keyboard/Enter・全OS/DPI・外部UIAは対象外。
通常app/実ノート/clipboard/前面focus/Storeを試験に使わない。個別保留466/634/530と旧削除拒否領域を維持する。

## 統合・保全整理の確定

#741/PR742は最終headc6bc85e51a98a60f742b3d4bcc5a31d6a2c78102、必須CI42167822732 full-gate/check成功。
main4656997c09edc2896d889cf811adfa4be2ab9f16へ正典統合し、#731反映済み通常exeのhash/bytes/更新日時不変を確認した。
CMakeは専用試験登録だけを除くと前mainと完全一致し、src/eng/fonts/packagingも不変。
4229filesを全hash保全し、絶対path/同一squash tree/clean・untracked・ignored/link/稼働参照0を確認後、専用worktree/outputを整理。
branch/commit保持。確定原本D:/NeNeFolio/agents/741-first-save-modal-native/integration-completion.json。
