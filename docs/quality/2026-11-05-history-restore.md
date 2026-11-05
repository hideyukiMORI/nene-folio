# 履歴採用とUndoの恒久native回帰 — 2026-11-05 / #649

## 結論

履歴から選んだ版を未保存の編集として採用し、Undo一回で直前の本文へ戻せることを、
既存native試験へ追加した。対象実行は148checks/1.734454秒、子/記録0、stderr空。
VIEWからは保存済み本文、未保存EDITからは直前の下書きへ戻る。
採用/Undoの間は保存revisionと保存済み本文・全履歴bytesが不変で、最後の明示保存だけが1世代更新する。
既存application回帰と過去の実機確認がある契約を恒久化したもので、新たな製品欠陥の発見ではない。

## 正典経路と変更範囲

[ADR0038](../adr/0038-history-restore.md)決定1/2/3/4と#310補正、
[ADR0012](../adr/0012-note-history-before-save.md)の保存前archiveに従う。
tests/win32/image_status_tests.cの既存native fixture・専用desktop・公開invoke_operationを再利用し、
--history selectorを追加。CMakeLists.txtのCTest history_restoreは同じexe、既存image_status_data lock、30秒上限を使う。
新target/製品入口/テスト用の製品分岐/依存を作らず、元のimage/diagram/status/icon selectorを維持する。

fixtureは既存schedule/oneを使い、archive portで古い本文2世代を用意してから現在本文を書く。
日本語・絵文字・Markdownの見出し/引用/リスト/強調/code記号を含む正常UTF-8の一行本文を使用する。
他のnative selectorと同じ専用dataで、先行試験の履歴が残っていても先頭2世代だけを固定し、
最大5世代を実際に読み取って比較する。新カテゴリ/一覧順/保存先の分岐を増やさない。

## 対象の一連

1. 保存済みVIEWから操作メニューのHISTORYを開き、公開navigateで2行目を得て現在世代のINVOKEで採用する。
2. EDITとnative読取専用解除、MarkdownとUnicode全units、未保存判定、先頭caretと本文focus、履歴面の世代終了を確認する。
3. native EM_UNDO一回で保存済みの現在本文へ戻り、未保存判定SAMEを確認する。
4. native全文置換で未保存の下書きを作る。同じ公開履歴経路で1行目を採用し、Undo一回で下書きへ戻る。
5. 再び1行目を採用して公開Saveを実行する。編集modeを維持してSAMEとなり、store_revisionが正確に1進む。
6. 保存後のノート全bytesは採用版、履歴1は以前の保存本文、履歴2〜5は元の1〜4と一致する。

採用/Undo/下書き/保存直前の6時点で、現在ノートと履歴1〜5の全file bytes（BOMを含む）と不存在を照合し、
store_revisionが変わらないことを確認する。保存時の6file比較も含め、保存契約を製品と同じportから読む。
通常app/実ノート/Storeは使わない。試験開始時の専用dataは0file、終了時の6file hashをcontract.jsonへ保存した。
normalな本文だけのため画像処理要求は0。実UIの入力は公開操作とnative messageであり、物理keyboardや外部readerの実測ではない。

## 実行記録

全実行はtools/record-run.pyの新run-dirへ入力snapshot、raw stdout/stderr、子/記録終了値を保存した。

| run | コマンドと結果 |
| --- | --- |
| configure-debug-01 | 固定toolchainでcmake -S <task> -B <debug> -G Ninja -DCMAKE_BUILD_TYPE=Debug、成功 |
| build-debug-01 | clang-format --dry-run --Werror、cmake --build <debug> --target image_status_tests --parallel 2。tidy/警告を維持、142actions成功 |
| build-debug-02 | #647統合main8e74a91へff後、変更されたnote_textを取り込む3actions成功。試験exeは未実行の段階で更新 |
| contract-01 | image_status_tests.exe --history。148checks/1.734454秒、stderr空。30秒上限と初期data0fileを確認 |
| conformance-01 | 6docsの参照、git diff --check、eng/conformance.py --root <task> --build-dir <debug>を確認する |

成功済みの実行は入力不変なら工程だけで繰り返さない。ローカルでは履歴境界のnativeだけを実行し、
最終headの必須CIで同じeng/check.ps1を通してから正典統合する。追加の独立委任は行わず、親が差分を自己レビューした。
補助readのパス誤指定/過大出力は全体journalへ分離し、製品試験失敗と混同しない。

## 限界・保全

正常な一行本文と同一windowのVIEW/EDIT/保存の一連だけで、空本文・改行各型・巨大本文・全OS/DPI/IME・失敗注入の新保証ではない。
store_revisionとfile bytesの不変は照合するが、OSの全I/Oを捕捉して同一bytesの書戻しが無いと証明するものではない。
不正本文の拒否、確保/保存失敗、電源断は既存の対象試験の範囲であり、今回追加の実機成功とはしない。
製品/API/schema/版/ゲート/依存は不変。規則ARC-001/004/008/012、C-008/012/014/016、QLT-008/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。#634は保留し再開しない。

原本D:/NeNeFolio/agents/649-history-restore/。作業木/一時出力は同worktrees/outputsの649-history-restore。
最終head必須CI、正典統合、通常exe/data不変、全保全/hash照合と専用領域整理は同taskのintegration-completion.jsonへ。
branch/commit保持。全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。

## 14:43 統合・保全整理完了

PR650 headcf158dff6cc5418b312205e75356d5e48c0dc9d8の必須CI41975099692 full-gate/check成功。
main13ce4ee3add602b63776a378da71b2b147dfb3f6へ正典統合、Issue649 CLOSED。
通常exe/data32fileは#647後のbytes/hash/作成更新日時と一致。製品不変で通常build/app起動なし。
1370file全保全ZIPの再hashと取込tree/clean-untracked/ignored/link/稼働参照監査後、専用worktree/outputを削除。
branch/commit保持。確定原本は同taskのintegration-completion.json。
