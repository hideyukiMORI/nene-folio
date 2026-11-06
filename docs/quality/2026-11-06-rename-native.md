# 改名の先行保存とnative編集保持 — 2026-11-06

Issue #717。FR-031、ARC-001/004/005/011/012、C-008/016/017、QLT-008/010/012/013、CNF-006、GIT-001〜004、ADR0022/0041/0060。
追加waiver none、既存WVR-0001継続。主担当単体で実装/検証し、paste_reviewは契約と差分の読み取り専用独立レビューを担当した。

## 固定した契約

既存image_status_tests fixtureへ--renameを追加し、公開TREE FOCUS→実Ex入力「:rename 名前」→Enterを通す。
製品のRENAME/store_note/rename_plan/apply_renameへ合流し、製品hookや別の改名経路は増やしていない。
既存別名保存のEx入力、名前付き履歴read/比較、本文保持確認を共用する。古いヘッダー説明だけ
READY→NAME_UNCHANGEDへ訂正した。受理済みADR0060の同名入力保持契約に従い、製品実行コード/API/schemaは不変。

- 変更済みEDITで同名を指定すると、Exの理由「同じ名前です。」と入力/可視性/focusを保ち、本文/選択/UndoRedo・元md/全履歴/store_revisionを変えない。Escで閉じる。
- 別名への改名は変更本文を1回保存し、旧本文を履歴1、既存履歴を後続slotへ移す。md/全履歴を新名へ移し、旧md/旧履歴slot/旧履歴directoryが不在になる。
- 同じ実RichEditの全文（日本語/絵文字/Markdown記法）・選択・Undo/Redo可用性を保持する。保存後はSAME、実UndoでCHANGED、RedoでSAME。
- UndoしてRedoを残した状態から再改名すると、その本文を新たに1回保存して履歴をもう1段進める。保存後SAME、RedoでCHANGED、再UndoでSAME。
- VIEWからの3回目の改名では保存revisionを増やさず、md/全履歴のbytesを維持したまま名前だけを移す。描画済み本文には強調記号が無いことを確かめ、保存物にはMarkdown原文を要求する。
- 各成功後にapplicationの保留意図が無く、完了journalも物理的に不在であることを確認する。VIEW/readonlyも保持する。

3回の実改名でstore_revision増分は2（変更EDITの2保存だけ）。同名では0、VIEWでは0。
本文の変更印はfolio_state_unsavedで確認し、Ex操作で移るfocusやEM_GETMODIFYの値を改名の保持契約へ混ぜない。
同名拒否後のEx focusは再入力に必要な別の契約として確認する。

専用rename_native_testsを既存CMake共通host設定から作り、build/verification-renameだけを使う。
再実行は索引位置を固定せず、専用カテゴリ内で元名oneを探す。総数と工程番号から新名を作り、既存名/履歴directoryの不在を先に確認する。
以前の改名済みノートは保持する。30秒timeoutと既存image_status_data lockを維持する。

## 実行結果と全試行

原本はD:/NeNeFolio/agents/717-rename-nativeと同outputs。全体journalは同agents/quality-20261106/journal.md。
tools/record-run.pyで入力snapshot、生成exe、raw stdout/stderr、child/wrapper終了値を保持した。

| 試行 | 結果と判断 |
| --- | --- |
| build-01 | 正典Debug/tidyで試験振り分けrun_other_fixtureの複雑度11>10を拒否。child1/wrapper125、2exe未生成のartifact snapshotエラーも保持。製品実行前の試験コード不備 |
| build-02 | 名前操作の振り分けを小関数へ分け、同名理由後のGetFocus()==input確認を追加。既存閾値/計装のまま成功、入力不変 |
| test-01 | ctestのrename_nativeとsave_as_nativeを対象実行。rename252checks/2.21秒、直接共用するsave_as164checks/1.25秒、計3.48秒、stderr空 |
| test-repeat-01 | 再実行契約のためrenameだけを同専用dataで1回再実行。252checks/1.19秒、stderr空。前回md+4履歴の全bytes/hash/作成更新日時と履歴集合不変、今回の最終名1件追加、元パス/journal不在を外側でも照合 |

buildは固定toolchainでcmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debugと、
cmake --build build --target rename_native_tests save_as_native_testsを実行した。
対象はctest --test-dir build -R '^(rename_native|save_as_native)$' --output-on-failure --verbose、
再実行では正規表現を'^rename_native$'に限定した。既存sanitizer/tidyを維持している。
成功runはchild/wrapper0、入力不変、記録エラーなし。別名保存を確認した理由は共用Ex/履歴helperの直接影響であり、無関係な操作は追加しない。

独立レビューは保存revision/2段階履歴シフト/再実行時の選定/コメント訂正に問題無しと判断し、同名後のfocus assertion不足1件を指摘した。
上記build-02とtest-01で補った。振り分けの分割は同じselectorを同じ試験へ渡し、検査数/意味を減らしていない。
補助C文字列のbackslash escapeとコメント整形は最初のbuild前に訂正した。文書生成helperの最初の送信はJavaScript文字列構文で拒否され、
file/tool処理の実行前に停止した。文書を直接作る形へ訂正し、製品検証の失敗とは分離する。

Windows11 Pro10.0.26300、Msftedit10.0.26100.9949。製品のローカルNTFS制約を保ち、専用D領域で確認した。
固定toolchain、exe/fonts/CTest定義とsourceのhash/版は入力snapshotへ収載。ASan起動警告無し。
成功結果は関連入力不変のままPR/CI/統合へ再利用する。最終head必須CI・正典統合・通常exe不変・全保全整理の確定原本はagents内integration-completion.jsonへ。

## 限界

カテゴリ改名、物理F2/IME、name modal、名前衝突/未選択/無題、I/O部分失敗/クラッシュ復旧、別volume、viewport画素/長文scroll/全DPIは対象外。
先保存と移動の完了結果を実測したもので、電源断耐久性や全ファイル競合を証明しない。通常app/実ノート/利用者clipboard/前面focus/Storeを試験に使っていない。
通常exeの実行内容を変える変更はなく、通常buildは反復しない。

## 統合・保全・整理の確定

#717/PR718は最終head6ab95285e7f67c73b21d08b5e122d467df5871da、必須CI42141683575 full-gate/check成功。
main8ebc8b8a7b28fe81f9bf68630c58acf26617b1aaへ正典統合し、通常exeのhash/bytes/更新日時不変を確認した。
4099filesを全hash保全し、絶対path/同一squash tree/clean・untracked・ignored/link/稼働参照0を確認後、専用worktree/outputを整理した。
branch/commit保持。確定原本D:/NeNeFolio/agents/717-rename-native/integration-completion.json。
