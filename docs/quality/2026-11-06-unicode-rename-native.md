# ノート行の選択状態を主窓で保持する — 2026-11-06

Issue #731。FR-018/031、ARC-001/004/005/008/009/011/012、C-008/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0056/0060、改名のADR0022/0041。
追加waiver none、既存WVR-0001継続。主担当が実装・自己レビュー・検証し、paste_reviewは所有境界と回帰の読み取り専用独立レビューを行った。

## 発見した不具合と修正

当初は既存rename_nativeの改名先をASCIIだけから日本語・内部空白・絵文字へ広げ、公開行名と台帳を照合する追加だった。
初回test-01は1.08秒、child/wrapper8で公開行の名前/選択状態の複合検査を拒否した。期待するUnicode名mdは実在した。
静読で、drawer_window_accessible_readがapplicationから正しく導いたselectedを、主窓末尾のaccessible_selection_viewが
layout/mode専用のpart_selectedでfalseへ上書きしていると確認した。通常のノート行にも同じ問題があり、UIA IsSelectedとRemoveFromSelectionもこの値を参照する。

folio_window_accessible_readのaccessible_selection_viewを、drawer委譲より前へ移した。
共通値を先に設定し、行の選択は既存drawer/applicationの唯一の正本から受け取る。第二の選択状態や再判定処理は追加しない。
Treeの選択取得・selection_required、行revision失効、surface早期return、focus算出は既存経路を維持する。
設計/API/schema/CMakeは不変で、既存のADR0056/0060の状態公開契約を回復する修正である。

## 回帰で固定した範囲

- note_switch_native: 初期選択、同一行のSelect/Invoke、別行のSelectと帰還で選択行だけtrue、非選択行/categoryはfalse。TreeのGetSelectionと一致しselection_requiredも保持する。
- 未保存EDITから別行へFocusし、実際の公開focusが移ったことと選択が元の行に残ることを確認する。既存本文/選択/Undo・履歴非保存と、その後の通常切替1保存を維持する。
- layout_edit_native: 表示3択とVIEW/EDITの公開selectedが、現在の表示とEDITだけtrueになることを既存往復操作で確認する。
- rename_native: 変更EDITとVIEWの改名先を「zz改名 猫🐾-件数-段階」、Undo後はASCIIへ戻す。UTF-8期待とUTF-16入力/表示期待は独立literal。公開行名とselectedを別検査し、全台帳の件数/順序/UTF-8名、既存2保存/全5履歴slot/旧名とjournal不在/native本文選択UndoRedoを保持する。

再実行は既存Unicode名の非上書きを確認する1回だけ。初回redで残った途中名も消さず、全旧Unicode名mdと履歴のbytes/hash/作成更新日時・集合、旧md集合を保持し、新名1件だけ追加する。
通常app/実ノート/clipboard/前面focusは試験入力に使わない。既存専用desktop/data/30秒timeout/resource lockを維持する。

## 実行結果と全試行

| 試行 | 結果 |
| --- | --- |
| build-01 | 製品修正前のUnicode改名回帰をDebug/tidyでbuild成功 |
| test-01 | 1.08秒、child/wrapper8。公開行の名前/選択の複合検査でred。失敗時の途中Unicode名と履歴を保持 |
| build-02 | 選択値の所有順修正と通常選択/layout検査を追加し成功。runtime未実行 |
| build-03 | 独立レビューの実focus確認を追加し成功 |
| test-02 / test-repeat-01 | rename_native273checks/2.05秒、note_switch_native197checks/1.4秒、layout_edit_native606checks/0.65秒、計4.13秒成功。既存Unicode名保持の再実行は276checks/1.41秒成功。stderr空・child/wrapper0・入力不変。旧Unicode名md2件/履歴7件のbytes/hash/作成更新日時・集合不変、新規1件のみ追加 |

buildは固定toolchainのcmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug、
cmake --build build --target rename_native_tests image_status_tests。ASan/UBSan/nullability/no-recover/tidyは不変。
対象はctest --test-dir build -R '^(rename_native|note_switch_native|layout_edit_native)$' --output-on-failure --verbose。
再実行は'^rename_native$'だけ。工程変更による同一入力の反復はしない。clang-format --dry-run --Werror、conformance実build graph、git diff --checkも確認する。

独立静読は製品の1行移動に阻害事項なし。FOCUS後の実focus確認不足を指摘し、実行前に追加した。
build-02の入力snapshotを守って終了を待ってから補ったため、最終build-03を行う。build-02候補のruntimeは実行していない。
tools/record-run.pyで入力/成果物snapshot、raw stdout/stderr、child/wrapper終了値を保持する。
原本D:/NeNeFolio/agents/731-unicode-rename-native、試行同outputs、作業木同worktrees。
補助readでaccessible_view.h/accessible_read.c/drawer_accessibility.c/ADR0056のpathを誤指定し、rg --filesと実装本体へ訂正した。
layout_stepのfieldをkindと誤記した候補は、実field controlと照合してbuild前に訂正。製品検証の失敗と分離する。

最終head必須CI・正典統合・通常Debug反映/実data保持・全保全整理の確定原本は同agents/integration-completion.jsonへ。

## 限界

nativeの公開値を確認し、外部UIA client/reader発話/通知タイミング・glyphの画素描画・全OS/DPIを実測したとは扱わない。
カテゴリ改名、Unicode正規化/大小文字だけの改名、長いpath/予約名/末尾空白、名前衝突/I/O失敗/journal復旧、物理IME/keyboard/fresh-process復旧は対象外。
Store署名版の検証や新規配布は行わない。
