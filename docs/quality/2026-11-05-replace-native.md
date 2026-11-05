# 通常置換・Undo・保存のnative回帰 — 2026-11-05 / #653

## 結論

実Win32 windowの公開操作メニュー、native入力欄、世代付き公開ボタンから1件/全置換を行い、各Undo一回と明示保存を確認する恒久回帰を追加した。
最終Debug対象は置換158checks/1.380633秒と、共通本文読取へ直接影響する履歴168checks/1.660135秒。両方とも子/補助/記録0、stderr空。
保存前はstore_revisionと現在ノート/履歴1〜5の全bytes・不存在を保持し、明示保存時だけ正確に一世代進む。
[ADR0028](../adr/0028-regex-replace.md)の既存契約を恒久化したもので、新しい製品欠陥は認定していない。

## 正典経路と範囲

既存tests/win32/image_status_tests.cのnative fixture、専用desktop/data、実regex/persistence adapter、archive port、
公開invoke_operationとhistory_snapshotを共有する。--replace selectorとCTest replace_nativeを、既存30秒上限/image_status_data lockへ追加した。
新target/製品API/テスト用製品分岐/依存/schemaはない。mainのselector振分だけrun_fixtureへ分ける。
共通expect_editorはEM_GETTEXTEX(GT_RAWTEXT,1200)で本文を取り出し、CR一単位・サロゲート対とMarkdown記号を正確に照合する。
これは製品note_pane_textと同じ公開native読取方式だが、試験側の固定期待値を読むだけで製品を置き換えない。

本文は「猫1 😀」「猫2 **saved**」「猫1 `old`」の3行。正常なパターン(猫)([12])と、群2・群1・足跡絵文字・改行の置換を使う。
最初はVIEWのwindowを作り、公開EDIT操作から編集用本文を表示する。下見は実ICUで3件となる。

1. 公開「1件」で先頭の猫1だけを置換。全UTF-16本文、挿入直後caret7、残り2件、帯の表示を確認。
2. 公開closeで置換帯を閉じ、EM_UNDO一回で元の3行へ戻す。未保存判定はSAME。
3. 再開し公開「すべて」で3件を置換。全本文、先頭caret、残り0件、帯の表示を確認。
4. 帯を閉じUndo一回で全件を戻す。再度全置換し、保存直前まで全保存内容の不変を確認する。
5. 帯を閉じて公開Saveを実行。編集modeを保ちSAMEとなり、保存revisionが正確に1増える。
6. 保存済み本文は期待するUTF-8/LF全bytes、履歴1は以前の保存本文、履歴2〜5は以前の1〜4と一致。

文字列は短い正常な有効入力だけで、画像処理要求0。通常app/実ノート/Storeは使わず、SwitchDesktopも呼ばない。
最初の専用dataは0file、再試行ごとに実行前dataの全copy/hashを別名保持し、終了時hashを記録。
既存data lockによりCIの他selectorと直列化する。先行試験の履歴数でcheck総数は変わるが、全履歴の契約は同じである。

## 全試行と修正

全試行はtools/record-run.pyの新run-dirでraw/stdout/stderr/入力snapshot/終了値を保持。

| run | 結果と判断 |
| --- | --- |
| configure-debug-01 | 固定toolchain/CMake Debug、成功 |
| build-debug-01 | mainのcognitive complexity11が上限10で拒否。子1/記録125/未生成exe artifact1。規則は変えず振分だけhelperへ分けた |
| build-debug-02 | #651 mainfe4703bへffしline_index更新込み6actions成功。format/tidy/警告維持 |
| contract-01 | 子試験1、補助/記録1。最初の本文期待値不一致。製品変更なし |
| build-debug-03 / contract-02 | 一時的なUTF-16診断を追加してbuild成功。試験は21単位対27単位で失敗。stateだけをwindow作成前にEDITへ変え、閲覧RTFを表示していた準備ミスを確認。ASan interception_win診断もraw保持 |
| build-debug-04 / contract-03 | window作成後の公開EDITへ訂正しbuild成功。置換/Undo照合は進んだが、帯を開いたまま無効なtoolbar Saveを呼び試験1。ASan診断も保持 |
| build-debug-05 / contract-04 | 公開close後Saveの通常手順へ訂正。build成功、native158checks/子0だがASan診断で補助のstderr空assertが失敗し補助/記録1 |
| history-01 | 同binaryの共通読取回帰はnative168checks/子0、ASan診断で補助/記録1。成功記録へ書き換えない |
| build-debug-06 | 原因特定用の一時UTF-16出力を除いた最終test、2actions成功。診断抑制・sanitizer設定・期待値・ゲートは変更しない |
| contract-05 / history-02 | 最終testで158/168checks、各1.380633/1.660135秒。子/補助/記録0、stderr空 |
| conformance-01 | 文書参照/diff/source/build graphと最終入力SHAを確認して提出する |

ASan診断は「interception_win: unhandled instruction … 0f 29 74 24 30 0f 28 f0」。
[LLVMの対応ソース](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.5/compiler-rt/lib/interception/interception_win.cpp)は命令を解釈できない診断を非致命として出す。
当該binaryでどの関数のinterceptionが関わったかは特定しておらず、診断が出た実行を完全なsanitizer捕捉の証明としない。
一時出力を除いた最終binaryでは両対象がstderr空だった。診断の原因を一時ログだけへ断定せず、全rawと失敗結果を保つ。

GitHub pluginのIssue作成は403で、認証済みghの同内容作成へ切替。初回clang-formatはtoolchain未読込でcommand-not-found、正典toolchain後に成功。
補助結果表示のPython構文誤り、推測path/Windowsglob/過大表示は全体journalへ分離して記録し、製品試験と混同しない。

## 検証と限界

対象buildはclang-format --dry-run --Werrorとcmake --build <debug> --target image_status_tests --parallel 2。
対象nativeはimage_status_tests.exe --replaceと--history。最終コードの成功入力SHAを照合し、工程だけで再実行しない。
最終PR head必須CIでeng/check.ps1を通し、正典tools/merge-pr.ps1で統合する。
製品差分がないため通常版再buildは行わず、#651反映後のexe/data32fileとのbytes/hash/日時一致を確認する。

物理keyboard/外部reader/全OS-DPI-IME/巨大本文/失敗注入の新保証ではない。CR挿入は試すが全改行型の組合せではない。
OS全I/Oを捕捉しないため、同一bytes書戻しが一度もない証明ではない。製品/API/schema/版/依存/ゲート不変。
親が単体で実装・自己レビュー・検証し、追加委任なし。ARC-001/004/008/012、C-008/012/014/016、QLT-008/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。#634保留を維持。

原本はD:/NeNeFolio/agents/653-replace-native/。最終CI/統合/通常exe-data不変/全保全・専用領域整理は同integration-completion.jsonへ。
全試行のcopy/hashを保持し、完了後はD専用worktree/outputだけ整理、branch/commit保持。全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。

## 15:24 統合・保全整理完了

PR654 head13fdd0e8893d7ba6ebf0c419cb641c68cb8693af、必須CI41979295240 full-gate/check成功。
main7176bf49ac5573e466327a80c718062ee10d1099へ正典統合、Issue653 CLOSED。通常exe/data32file bytes/hash/作成更新日時不変。
1374file全保全ZIP再hashと取込tree/clean-untracked/ignored/link/稼働参照の監査後、653専用worktree/outputだけ削除。branch/commit保持。
全失敗実値を含む確定原本は同taskのintegration-completion.json。通常app起動なし。
ASan説明の参照は、固定toolchainと同じLLVM19.1.5の公式sourceでも同じ非致命診断を確認し、リンクをその版へ揃えた。
