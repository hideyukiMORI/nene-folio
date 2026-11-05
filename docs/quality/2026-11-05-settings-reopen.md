# 同じ窓で設定を開き直すときの参照 — 2026-11-05 / #641

## 結論と範囲

同じwindowで設定を閉じて再び開き、前回の9行・一覧・閉じるボタンの公開参照は使えず、
新しい行は正しく読取・巡回・focusできることを既存native回帰へ追加した。
3言語×2巡、6window/12設定open、対象2299checksが9.732596秒で成功した。
資源数もGDI55/USER36/process handles267が前後一致、stderr空。製品欠陥は検出していない。

ADR0058が既に定める契約で、#321の外部試験にもclose/reopen/queryの観測がある。
今回の目的は、恒久CTestで閉鎖直後の保存先1行だけをFAILED確認していた範囲を補うこと。
製品・公開API・保存schema・CMake・30秒上限・依存・ゲート・除外に変更はない。

## 正典経路と確認

tests/win32/settings_accessibility_tests.cの専用desktop/dataと公開folio_window_accessible_*を使う。
既存の3言語warmupと資源確認のもう1巡、それぞれのverify_language内で同じwindowを再利用する。
private実装のinclude、製品への計装、別起動経路は追加しない。

- 初回の9行は既存の名前・親・leaf・前後巡回・一意性とボタンfocus/矩形を確認する。
- 閉鎖後、旧9行/list/closeの読取、全5方向navigate、focusが正確なunavailableを返す。
- 再開後、9行のkind/slotは対応を維持し、全行が新revisionを持ち、旧identityと異なる。
  新しい行でも名前・巡回・focus/矩形を確認し、旧11参照の失効をもう一度確認する。
- 旧closeへInvokeしてもunavailableとなり、新設定の世代とfocusを変えず、現在の行は読める。
- surface rootは同じ公開itemで読め、閉状態に応じてoffscreenが変わる。最後は新closeで閉じる。

unavailableは既存accessibility_node境界がSDK定数から公開するHRESULTを使う。
古い参照を現在の同じ行へ読み替える経路や、独自のエラー値は作らない。

規則: ARC-001/003/004/008/010/011/012、C-008/012/016、QLT-008/010/012/013、GIT-001〜004。
ADR0055/0058。追加waiver none、既存WVR-0001はWinHTTP callbackだけで変更なし。

## 実行と試験側の訂正

基点main6d10a210fa32bedfeecaed79c0c6c6fa9bff7071。正典Debug/tidy/formatを使う。
専用desktopへthreadを割り当て、SwitchDesktopは呼ばない。隣接portable dataは初期0fileで、
終了時の専用settings/lock hashを保存した。通常app・実ノート・Storeは試験に使っていない。

| run | 実行・結果 |
| --- | --- |
| 初回configure準備 | 旧debug inventoryに未生成のCache/compile_commandsがあり停止。record-runもinput-list未生成で子起動前拒否、run-dirなし |
| configure-debug-02 | source639件を入力に正典Debug configure成功、生成後のbuild入力は641件 |
| build-debug-01 | UIAutomation.hをtestへ加えたためSDK定数が既存node TUと重複しlink失敗。子1/記録125、未生成exeの記録error1を保持 |
| build-debug-02 | 既存accessibility_node.hの公開定数へ統一。test TU/linkの2actions、Debug/tidy/format成功 |
| contract-01 | 引数なしsettings_accessibility_tests.exe、2299checks/9.732596秒、stderr空、子/記録0、入力不変 |
| conformance-01 | 関係6docsの相対参照・git diff --check・source/build graphを確認する |

初回link失敗の後、自己レビューで旧要求前後のfocus一致も追加した。SDK定数の複製やlinker抑制はしない。
configure入力は既存sourceと生成後artifactを分け、上限/閾値/除外を緩和していない。
補助の不存在run-dir表示や、SDK header検索の過大出力も試験結果と分離してjournalへ記録する。

## 原本と限界

全対象実行はtools/record-run.py。原本D:/NeNeFolio/agents/641-settings-reopen/、
作業木は同worktrees/641-settings-reopen/、一時出力は同outputs/641-settings-reopen/。
全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。

これは公開native関数と有限の通常開閉の回帰。実UIA clientのCOM identity/通知/発話、
全OS/DPI/IME、任意回数の反復、全メモリの無漏れを新たに保証するものではない。
成功nativeのsource/exe hashを最終sourceと照合し、文書化やpushだけでは再実行しない。
最終head必須CIのeng/check.ps1、正典統合、通常exe/data32file不変、全保全と専用領域整理は
同taskのintegration-completion.jsonで確定する。branch/commit保持。#634は保留を維持する。
