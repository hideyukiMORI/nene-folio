# 空本文保存とnative UndoRedo — 2026-11-06

Issue #727。FR-006、ARC-001/004/005/008/009/011/012、C-008/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0006/0012/0054。
追加waiver none、既存WVR-0001継続。主担当単体で実装・自己レビュー・検証した。製品src/API/保存schemaは不変。

## 固定した一連

既存image_status_testsの実窓・公開EDIT/VIEW/native入力を使い、名前付き本文の全削除から0bytes保存・履歴・UndoRedo・再投入までを恒久回帰にした。
正常な空RTFは戻り0でも成功する（ADR0054）。部品probeやapplication単体に加え、主窓の保存連鎖を通す。製品不具合を新しく再現したものではない。
既存hostの専用desktop/data、30秒timeout/image_status_data lockを維持する。製品hookや第二の保存経路は追加しない。

- 専用oneの非空本文を全選択し、native EM_REPLACESELの空文字列で一つのUndo単位として削除する。空/EDIT/CHANGED、modified/Undoあり・Redoなし、保存前md/全履歴/revision不変を確認。
- Ctrl+Sを2回送り、実在する0bytesファイルへ1回だけ保存する。NAMED/選択先oneを保持し、NONEやファイル不存在に置き換わらない。履歴1は元md全bytes、全5slotは元snapshotから1世代ずつ進み、revision増分は1。
- 保存前後のnative全文/選択/modified/UndoRedo/focusを比較。実Undo1回で元のUnicode本文/CHANGED、Redoで空/SAMEへ戻り、保存ファイル/全履歴/revisionを保つ。
- SAMEからVIEWへ進み、空本文/読み取り専用を確認。EDITへ戻って空/SAME・enabled/body focusを確認し、Ctrl+S2回で追加保存なし・native状態保持を照合する。

共通capture_native_bodyは取得先全体を非NULで初期化し、戻りの非負長・既存の短文容量条件・指定長位置のNULを確認する。
正常な0を受理しつつ、何も書かれなかった0や終端欠落を成功にしない。故障注入や全getter失敗条件の証明ではなく、空と非空の実API成功系を確認した。
既存Unicodeのraw helperはexpect_raw_fileへ一般名を揃えた。処理は変えず、PERSISTENCE_LOADEDと全raw bytesを照合し、0bytesの実在と履歴1の独立literalを確認する。
履歴全slot比較は既存helperを共用する。OS全I/Oを捕捉する試験ではなく、同一bytesの書戻しが無いことまで一般化しない。

## 全試行と結果

原本D:/NeNeFolio/agents/727-empty-save-native、試行同outputs、作業木同worktrees。
tools/record-run.pyへ入力snapshot/hash、raw stdout/stderr、生成exe、child/wrapper終了値を収載した。

| 試行 | 結果と判断 |
| --- | --- |
| build-01 | 試験が存在しないfolio_state_selected_noteを参照したコンパイル失敗、child1。exe artifact指定もverification-imagesではなくverificationだったため記録失敗、wrapper125。未実行の試験を成功扱いしない |
| build-02 | 公開folio_state_selectionのnote_refと実名oneの照合へ訂正。製品/計装/ゲート不変、正典Debug/tidy成功、正しいexeを収載 |
| test-01 | empty_save_native236checks/0.77秒、直接共用unicode_save_native254checks/0.65秒、CTest合計1.43秒成功。stderr空、child/wrapper0、入力不変。 |

build-01が成功した場合の証跡再収載helper案も保管したが、実buildが失敗したため未実行・不採用。失敗を隠さず修正してbuild-02を実行した。
ビルドは固定toolchainのcmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debugとcmake --build build --target image_status_tests。
対象はctest --test-dir build -R '^(empty_save_native|unicode_save_native)$' --output-on-failure --verbose。
ASan/UBSan/nullability/tidyを維持。format/conformance実build graph/diffと最終head必須CI・正典統合・通常exe不変・全保全整理の確定原本はintegration-completion.jsonへ。

## 限界

Windows NT10.0.26300.0、Msftedit10.0.26100.9949、ローカルNTFS・専用desktop/D領域。短い正常本文を全削除した成功経路。
物理keyboard/IME、外部clipboard、不正UTF16/NUL/16MiB拒否、I/O失敗、getter失敗注入、全DPI、fresh-process復旧は対象外。
VIEW→EDITは同一processでの再投入であり、Store署名版の導入・再起動ではない。
通常app/実ノート/前面focus/Storeは試験に使わず、通常buildも反復せずexeのhash/bytes/更新日時を照合する。
