# テーマpreview中の本文保存 — 2026-11-05 / #665

## 結論と対象

テーマpreview/取消/本文だけの保存 257checks/1.056087秒、stderr空、子/補助/記録0。
[ADR0061](../adr/0061-theme-live-preview.md)決定4のCtrl+S本文保存と、[ADR0016](../adr/0016-command-catalog-and-shared-execution.md)の入力面保持を、既存CTest theme_preview_nativeへ追加した。
#661のpreview取消試験の末尾に一連を加えた。新たな製品欠陥の認定ではない。
製品/API/schema/CMakeは不変。既存image_status_tests.exe、専用desktop/data、30秒timeoutとimage_status_data lockをそのまま使う。

## 保存と未確定テーマの境界

#661のCLOSE/BACK取消・全履歴保持・Undo確認を先に通し、正常な短い日本語/絵文字本文へ同じ追記をもう一度行う。
公開設定INVOKE→テーマ面→native欄へ保存choiceと異なるnord/dracula→現在行SELECTでpreviewする。
欄をfocusし選択1〜3を置き、本文全UTF16単位/選択/native modified/Undo/Redo/focusとpreviewの実効theme/paletteを取得する。
native欄へ通常Ctrl+SのWM_CHAR 0x13を送り、既存subclass→command_character→store_from_surface→command_save→store_bodyを通す。

1. 初回保存でstateの本文判定がSAMEになり、store_revisionが1だけ増える。
2. 現在本文の全bytesが追記済みの固定期待値に一致し、履歴1〜5は開始時0〜4の全bytes/不存在と一致する。
3. query全体/選択1〜3/focus/欄の可視、本文全体/選択/native modified/Undo/Redoを保つ。
4. previewの実効theme/paletteを保つ。保存choiceとsettings.json全bytesは開始時から変わらない。
5. 2回目のCtrl+Sでも同じ状態で、保存世代と全内容は増減しない。
6. CLOSEで保存済み配色へ戻り、本文状態は設定を開く前のnative snapshotと一致する。
7. Undo一回で元の本文に戻り、保存済み本文とは異なるCHANGEDになる。Redo一回で追記済み本文へ戻りSAMEになる。
   いずれも全保存内容/世代を変えず、設定/配色も保つ。

製品の保存関数の直接呼出しや新入口は使わない。物理keyboard stateを変更せず、通常のnative文字messageを配送する。
新helperはこのselector内だけで使い、共有helperの処理は変えていない。したがってローカルは既存--theme-previewだけを対象にする。

## 実行と再利用

| run | コマンド・結果 |
| --- | --- |
| configure-debug-01 | 正典toolchain、cmake Debug/Ninja、成功 |
| build-debug-01 | clang-format --dry-run --Werrorとcmake --build <debug> --target image_status_tests --parallel 2、成功 |
| contract-02 | image_status_tests.exe --theme-preview、257checks/1.056087秒、stderr空 |
| conformance-01 | 文書参照/diff/source/build graphを確認 |

tools/record-run.pyで各実行のsource/設定/実行体/入力をsnapshotし、raw/終了値/入力終了時不変を保存する。
native開始時の専用dataは0file、試験後の全file hashも保存する。開始時実行体とfontsを明示入力にする。
build/nativeそのものに失敗なし。contract-01はPowerShell wrapperの-Selector/-Label指定を省いたため引数検査で子起動前に停止した。
補助子1/記録125/artifact不存在error1を保持。明示引数でcontract-02を実行し、初回native実行が成功した。期待値/製品/試験sourceは不変。
補助読取の推測path/filename不在は実一覧から訂正し、全体journalへ製品試験と分離して記録した。
成功source/CMake hashを最終自己レビューで照合し、push/統合だけで対象試験を繰り返さない。

## 限界と統合

正常な短い本文、通常の成功保存とpreview/取消が対象。全テーマ/OS外観/DPI/IME、物理キー、外部UIA reader、故障注入、全OS I/Oの新保証ではない。
保存世代と全保存内容を比較したもので、I/Oを全捕捉したとはしない。native modifiedとapplicationのSAME/CHANGEDは別々に観測する。
実app/実ノート/Storeを使用せず、SwitchDesktop無し。通常版は#651反映後のexe/data不変確認だけで再buildしない。
規則ARC-001/004/007/008/012、C-008/012/014、QLT-008/010/012/013、GIT-001〜004。追加waiver none、既存WVR-0001継続。#634保留。
最終head必須CI eng/check.ps1成功→tools/merge-pr.ps1で統合→通常不変→全保全/取込tree/clean/ignored/link/稼働参照監査→当該D専用領域整理。
branch/commit保持。原本D:/NeNeFolio/agents/665-theme-save/、確定結果は同integration-completion.json。
全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。主担当単体、追加委任無し。
