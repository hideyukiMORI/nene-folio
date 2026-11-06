# UnicodeとCRLF本文のnative保存 — 2026-11-06

Issue #723。FR-006、ARC-001/004/005/008/009/011/012、C-008/014/016/017、QLT-008/010/012/013、GIT-001〜004、ADR0006/0012/0042/0054。
追加waiver none、既存WVR-0001継続。主担当単体で実装・自己レビュー・検証した。製品src/API/保存schemaは不変。

## 固定した境界

ADR0006の#234/#245補正とADR0042は、RichEditのraw UTF-16にある本文U+FEFF、U+FFFDと縦タブを保ち、元の改行へ正規化する。
過去の実Win32 probeと中核回帰に加え、既存image_status_testsの実窓から共通保存を通す恒久回帰を追加した。
製品不具合を新しく再現したものではない。製品hook・別の本文取得/保存経路・依存は増やしていない。

- ファイル署名1個と本文先頭U+FEFF2個（raw UTF-8ではBOM3連）、日本語・絵文字、U+FFFD直後のU+000B、途中U+FEFF、tab、backslash/braces、CRLFと末尾空行を含む短い本文1件を用意する。
- fixture作成直後のraw mdを独立literal bytesに照合する。EDIT投入後の全文を別のUTF-16 literalへ比較し、CRLF→native CR、縦タブの区別を確認する。
- 非空選択から未編集Ctrl+Sを2回送り、md全raw bytes・全履歴・revisionを変えない。native全文/選択/modified/UndoRedo/focusも不変。
- EM_REPLACESELで「 追記 🙂」を末尾に追加し、CHANGEDと保存前のファイル/履歴不変を確認する。別の非空選択からCtrl+Sを2回送り、保存と履歴追加は1回だけ。
- 保存済みmdと履歴1をfile_bytesで直接読み、UTF-8署名/本文FEFF/FFFD/VT/CRLF/末尾改行/追記を含む全literal bytesへ照合する。後続の全履歴slotも元snapshotから1世代ずつ進む。
- 保存前後のnative全文/選択/modified/UndoRedo/focus/EDITを保持する。実Undoで元本文CHANGED、Redoで保存本文SAMEとなり、全保存履歴/revisionは不変。
- SAMEのVIEW→EDITで同じ本文が再投入され、追加保存が無いことを確認する。fresh-processの再起動確認とは区別する。

raw期待とUTF-16期待は別々のliteralであり、製品の正規化関数から生成しない。既存のnote_textによる全履歴比較も併用するが、
保存後mdと履歴1のraw bytesは解釈を挟まない直接比較が判定する。初期seedもraw比較を通すため、seedと保存の同じ誤変換で成功にしない。
検索/Unicodeのfixture本文選択だけを小関数へ分離した。既存検索の意味が不変であることは直接対象に含めた。
CTest登録は既存hostへ2行追加し、30秒timeoutとimage_status_data lockを維持する。

## 全試行と結果

原本D:/NeNeFolio/agents/723-unicode-save-native、試行同outputs、作業木同worktrees。
tools/record-run.pyへ入力snapshot/hash、raw stdout/stderr、生成exe、child/wrapper終了値を収載した。

| 試行 | 結果と判断 |
| --- | --- |
| build-01 | seed_notesの複雑度11>10をtidyが拒否、child1。exe未生成のartifact snapshot errorでwrapper125。runtime未実行 |
| build-02 | fixture本文の特別選択をtext_fixture_sourceへ分離。閾値/条件/計装を維持しDebug/tidy build成功、入力不変 |
| test-01 | unicode_save_native251checks/0.74秒、直接共用search_native246checks/1.20秒、計1.94秒。stderr空、child/wrapper0、入力不変、記録エラーなし |

ビルドは固定toolchainのcmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debugとcmake --build build --target image_status_tests。
対象コマンドはctest --test-dir build -R '^(unicode_save_native|search_native)$' --output-on-failure --verbose。
ASan/UBSan/nullability/tidyを維持し、成功済み対象は入力不変ならpush/レビュー/統合時に再利用する。
format/conformance実build graph/diffと最終head必須CI・正典統合・通常exe不変・全保全整理の確定原本はintegration-completion.jsonへ。

補助操作では、誤ったLLVMパスの整形を正典toolchainへ訂正、存在しないrecord-run.ps1/note_archive.cと不適切globを実ファイルへ訂正した。
整理済み721作業木の状態取得は不存在となり、確定原本と現mainを採用した。これらは製品試験の結果と分離し、journalへ記録した。

## 限界

Windows NT10.0.26300.0、Msftedit10.0.26100.9949、ローカルNTFS・専用desktop/D領域。
小さい有効本文1ケースであり、物理IME/keyboard、外部clipboard、不正UTF-16/NUL/16MiB拒否、全改行組合せ、全DPI、I/O失敗は対象外。
VIEW→EDITは同一processの再投入であり、fresh-processやStore署名版の再起動ではない。
通常app/実ノート/前面focus/Storeは試験に使わず、通常buildも反復せずexeのhash/bytes/更新日時を照合する。
