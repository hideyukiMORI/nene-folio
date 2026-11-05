# 設定画面の通常開閉と資源解放 — 2026-11-05 / #639

## 結論と範囲

既存の設定画面UIA試験で3言語を一巡してから、同じfixtureで再び3言語のwindowを生成・破棄した。
最終の対象実行は637checks、9.050630秒、GDI55/USER36/process handles267が前後で一致した。
製品欠陥は検出せず、製品コード・公開API・保存schema・CMake・30秒上限は変更しない。
今後も既存CTestのsettings_accessibilityが、初回利用後の資源数の非増加を検査する。

これは有限の6windowの確認であり、任意回数の反復、heap/private bytes、ピーク値、OSの全cache、
fixtureやadapter自体の破棄後まで含めた無漏れ証明ではない。実app・実ノート・Storeは試験に使わない。

## 正典経路と規則

変更はtests/win32/settings_accessibility_tests.c。既存create_fixture/verify_languageと
folio_window_create/destroy、公開UIAの設定Invoke、既存message pumpをそのまま使う。
JA/EN/ZH_HANSの呼出しをverify_languagesへまとめ、初回一巡後のverify_resourcesで同じ一巡を行う。
設定の9行について名前・親・leaf・focus・矩形・有限/一意/逆順の巡回・失効確認も各回に含む。

GetGuiResourcesとGetProcessHandleCountを前後で読み、後の値が前以下であることを要求する。
初回の遅延初期化はwarmupへ分け、終了後の解放による減少は許す。OS依存の絶対値は固定しない。
GetGuiResourcesは0件と失敗の両方で0を返すため、SetLastError(ERROR_SUCCESS)の後で呼び、
0のときはGetLastErrorも検査する。根拠は[MicrosoftのAPI仕様](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguiresources)。
全counterをstdoutへ保存し、API取得失敗と資源増加のどちらも試験失敗にする。

対象規則: ARC-001/003/004/007/008/011/012、C-008/012/016、QLT-008/010/012/013、GIT-001〜004。
ADR0055/0058の既存native検証経路に沿う。追加waiver none、既存WVR-0001はWinHTTP callbackだけ。

## 試行と変更

基点main332ad35d9d8565721f8ff155669577af6fb6ba64。D専用Debug buildの隣接portable dataと
専用desktopを使い、表示desktopのSwitchDesktopは呼ばない。製品への計装や別起動経路は追加しない。

初稿では3言語warmup後の12巡、計39windowを観測する補助selectorを作った。
observe-01は30秒上限に達し、所有childを停止した。rawではwarmup後と3巡終了時まで
GDI56/USER36/handles267で一定だったが、12巡全体は未完了であり成功扱いしない。
元source/exe・途中stdout・終了値を保全し、停止後に同test processが残っていないことも確認した。

上限内で通常CTestへ残す回帰として、warmup後の追加を1巡に限定して非増加assertを追加した。
補助selectorは最終版から削除し、既存の引数なし呼出しに含める。製品/CIの上限や閾値は緩和しない。
contract-01は633checks/9.243873秒成功。自己レビューでGUIcounter取得失敗の検査を補い、
contract-02は637checks/9.050630秒成功。両方とも55/36/267が前後一致、stderr空だった。
整形後のpatch context不一致2回と不存在の補助script参照も記録し、製品試験の失敗と分ける。

## 検証と原本

全対象実行はtools/record-run.py。原本はD:/NeNeFolio/agents/639-settings-resources/。
作業木は同worktrees/639-settings-resources/、一時出力は同outputs/639-settings-resources/。
全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。

| run | 実行と結果 |
| --- | --- |
| configure-debug-01 | 正典toolchain/Debug configure、File API query付きで成功 |
| build-debug-01 | cmake --buildのsettings_accessibility_tests、142actions成功 |
| observe-01 | run-observation.ps1、30秒で所有child停止。子/記録1、記録error0、入力不変 |
| build-debug-02 / contract-01 | test TU/link成功、引数なしnative633checks成功 |
| build-debug-03 / contract-02 | 最終test TU/link成功、引数なしnative637checks成功、stderr空 |
| conformance-01 | 関係6docsの相対参照・git diff --check・source/build graphを確認する |

成功済みnativeの実行source/exe hashと最終sourceを照合し、文書化・push・reviewだけでは繰り返さない。
専用dataの実行前copy/hashと終了時hashも保持する。通常exeは#635のsource9a84bebのまま再buildしない。
最終head必須CIのeng/check.ps1、正典統合、通常exe/data32file不変、全保全/hash再照合と
専用領域整理は同taskのintegration-completion.jsonで確定する。branch/commitは保持する。
#634は保留を維持し追加build/runを行っていない。
