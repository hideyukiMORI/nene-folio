# カテゴリ間ドラッグの編集保持と保存先追随 — 2026-11-06

Issue #721。FR-009/006、ARC-001/004/005/011/012、C-008/012/016/017、QLT-008/010/012/013、CNF-006、GIT-001〜004、ADR0007/0008/0012。
追加waiver none、既存WVR-0001継続。主担当単体で実装・検証し、paste_reviewは契約と期待値の読み取り専用独立レビューを担当した。

## 固定した契約

ADR0008決定5のカテゴリ間移動は、編集中のRichEdit本文を保存せず保持し、現在の宛先を付け替える。
ADR0012決定5により、過去の履歴は元カテゴリに残る。改名の履歴移動とは異なる既存契約である。

既存image_status_testsのfixture/sourceから専用move_edit_native_testsを作り、verification-move-editの専用dataだけを使う。
途中で失敗して別カテゴリに元mdが残っても、他のnative試験の元ノートを変えないための分離である。
CMakeの共通host設定と30秒timeout/image_status_data lockを維持し、製品hook・移動経路を追加しない。

- 公開accessibilityの現在のノート行/カテゴリ行矩形をdrawer座標へ写し、実HWNDへWM_LBUTTONDOWN→MOUSEMOVE→LBUTTONUPを送る。captureの開始/終了も確認する。
- 日本語・絵文字・Markdown原文を含む変更済みEDITをscheduleからtargetへ移動する。全native UTF-16/選択/modified/UndoRedo/EDIT/dirtyを保持し、OS focusはpointer押下どおりrootへ移る。
- 保存済みmdは全bytesを保ってtargetへ移り、元mdは不在。選択中の宛先/パンくずと両ledgerが追随し、保存revisionは不変。元の全5履歴slotは不変、移動先の既存履歴も不変。
- 実Undo/Redo後にtargetでCtrl+Sを2回送り、変更本文を1回だけ保存する。targetの履歴1に元md、後続slotにtargetの旧履歴を積む。scheduleの履歴は変えない。
- Undoで元本文/CHANGED・Redoありとなった状態からscheduleへ戻す。保存済みmdを移すだけで両カテゴリの履歴とrevisionを変えず、native状態を保持する。
- Redo/Undo後にscheduleでCtrl+Sを2回送り、元本文を1回だけ保存する。scheduleの履歴1は追記本文、後続slotは元のschedule履歴となり、target履歴は残る。
- さらに実Redo/Undoで新保存基準に対するCHANGED/SAMEを確認する。全体の保存revision増分は2だけで、targetのmdは不在。

両カテゴリの開始時履歴をsnapshotし、既存履歴を空と決めつけない。再実行時も旧履歴を消して初期化しない。
5世代の通常回転による最古版の脱落は保存契約の範囲である。前回失敗でtargetにmdが残る場合は、削除せずfixture前提の失敗で止まる。

## 全試行と結果

原本D:/NeNeFolio/agents/721-move-edit-native、試行同outputs、作業木同worktrees。
tools/record-run.pyへinput snapshot/hash、raw出力、生成exe、child/wrapper終了値を収載した。

| 試行 | 結果と判断 |
| --- | --- |
| build-01 | 最初の往復dragと戻った後の保存を含む候補が正典Debug/tidyで成功。runtime未実行。自己静読で保存先追随の確認不足を特定 |
| build-02 | targetでの明示保存と別々の履歴を加えて成功。runtime未実行。再実行時のtarget履歴保持をさらに補う必要を特定 |
| build-03 | 両カテゴリsnapshotの追加後、expect_target_historyの複雑度11>10をtidyが拒否。child/wrapper1。artifactは残っていた旧build-02のexeであり、新候補成功の証明ではない |
| build-04 | 期待する履歴slotの選択をmoved_target_versionへ分離。閾値/条件/計装不変で成功、入力不変 |
| test-01 | move_edit_nativeの358checks成功、native/CTest1.54秒、stderr空 |
| test-repeat-01 | 既存履歴を持つ再実行の確認。398checks成功、native1.47秒/CTest1.48秒、stderr空。前回target履歴1がbytes/hash/作成更新日時を保って履歴2へ移り、target mdは不在 |

成功runはchild/wrapper0、入力不変、記録エラーなし。358→398の差40は、既存source履歴slotの存在による全bytes照合30項目と、
既存target履歴1の比較10項目である。期待を減らした差ではない。外側の再実行原本はrepeat-before.json/repeat-result.json。

ビルドは固定toolchainのcmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debugとcmake --build build --target move_edit_native_tests。
対象はctest --test-dir build -R '^move_edit_native$' --output-on-failure --verbose。ASan/UBSan/nullability/tidyを維持した。
format/conformance実build graph/diffの結果と最終head必須CI・正典統合・通常exe不変・全保全整理の確定原本はintegration-completion.jsonへ。

独立静読はrevision増分2、各カテゴリの履歴の残置とシフト、pointer押下後のroot focus、既存target履歴の再実行保持に阻害指摘なし。
焦点はOSのfocus HWNDまでで、root内のkeyboard targetを独立に網羅したものではない。製品src/API/保存schemaは不変。

## 限界

Windows11 Pro10.0.26300、Msftedit10.0.26100.9949、ローカルNTFS・専用desktop/D領域。
物理mouse/IME、全DPI、名前衝突、絞り込み中のdrag、I/O部分失敗、VIEW画像、全scroll位置、target履歴5世代の飽和回転は対象外。
専用drawerのHWNDへ送った合成nativeメッセージの確認であり、OS全体のpointer配送を証明しない。
通常app/実ノート/利用者clipboard/前面focus/Storeを試験に使っていない。通常buildは反復せず、同一exeのhash/bytes/更新日時を照合する。
