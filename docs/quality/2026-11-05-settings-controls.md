# 設定操作と状態・表示の対応 — 2026-11-05 / #645

## 結論と範囲

行番号・ミニマップの切り替えと3言語選択が、application正本・選択印・行の翻訳へ
揃って反映されることを既存native回帰に追加した。対象7657checksが12.146391秒で成功。
3言語×2巡の6windowで、各windowの設定は3回開く。GDI55/USER36/process handles267は前後一致、stderr空。
製品欠陥は検出していない。製品/API/保存schema/CMake/30秒上限/依存/ゲート/除外は不変。

既存試験には全行列挙・focus・閉鎖/再開・資源確認があり、単体の設定保存回帰と#321の外部UIA観測もある。
今回補うのは、恒久native試験で公開SELECTから設定値と表示の対応を固定していなかった範囲である。

## 正典経路と確認

tests/win32/settings_accessibility_tests.cの既存専用desktop/dataと公開folio_window_accessible_*を使う。
IToggleProviderのtoggle_runとISelectionItemProviderの選択先もACCESSIBLE_ACTION_SELECTへ入る。
今回の試験はこの公開要求を呼び、実COM clientを新たに起動する試験ではない。

- 各windowの従来open/close/reopen確認後に、設定をもう一度開く。
- 行番号→ミニマップ→行番号→ミニマップの順にSELECTする。開始値からbooleanの4組合せを通って元へ戻し、
  各回の値・両CheckBoxのselected・言語不変を期待値と照合する。
- 日本語/英語/中国語を順にSELECTし、言語正本・正確に一つの選択印・9行の翻訳、boolean不変を照合する。
  最後は開始言語へ戻す。
- 各回は現在のsurface revisionから9行を列挙し直す。既存walkで名前・親・leaf・一意性も検査する。
  最後は現在世代のcloseで閉じ、rootがoffscreenになることを確認する。

新しい製品入口・private実装include・別状態owner・計装は追加しない。
期待tupleをprivate settings_valuesへまとめ、補助関数は4引数とした。実行前の自己レビューによる整理であり、
静的解析の失敗を観測した結果ではない。

規則: ARC-001/003/004/008/011/012、C-008/012/016、QLT-008/010/012/013、GIT-001〜004。
ADR0031/0032/0055/0058。追加waiver none、既存WVR-0001はWinHTTP callbackだけで変更なし。

## 実行と原本

基点mainfec31fa9eb5a46187c2d52678ff5609e7ec2f78a。正典Debug/tidy/formatを使用する。
専用desktopへthreadを割り当て、SwitchDesktopなし。初期専用dataは0file、終了時のsettings/lock hashを保持。
通常app・実ノート・Storeは試験に使っていない。

| run | 実行・結果 |
| --- | --- |
| configure-debug-01 | source/config638件、正典Debug configure成功 |
| build-debug-01 | clang-format検査、cmake --build debug --target settings_accessibility_tests --parallel 2、142actions成功 |
| contract-01 | 引数なしsettings_accessibility_tests.exe、7657checks/12.146391秒、資源55/36/267前後一致、stderr空 |
| conformance-01 | 関係6docsの相対参照・git diff --check・source/build graphを確認する |

全対象実行はtools/record-run.py、子/記録0、記録error0、入力終了時不変。
build結果JSONを全文表示した補助読取は過大出力で切れたため、その後は必要値を集約して確認した。
これは実行失敗ではなくjournalへ分離する。

原本D:/NeNeFolio/agents/645-settings-controls/、作業木は同worktrees/645-settings-controls/、
一時出力は同outputs/645-settings-controls/。全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。

## 限界と統合

有限の通常操作を公開native関数から確認した。実reader発話・COM通知/identity・全OS/DPI/IME・
本文があるminimap描画・保存失敗の再試行・全メモリの無漏れを新たに保証しない。
成功nativeのsource/exe hashを最終sourceへ照合し、文書化やpushだけでは繰り返さない。
最終head必須CIのeng/check.ps1、正典統合、通常exe/data32file不変、全保全/hash照合と専用領域整理は
同taskのintegration-completion.jsonで確定する。branch/commit保持。#634は保留のまま。
