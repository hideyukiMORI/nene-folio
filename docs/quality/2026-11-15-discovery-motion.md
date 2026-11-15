# 検索面の演出と静止fallback — #827

2026-11-15、base main dbfba3be21680af52962f1868d32192734bd07a3。
作業木D:/NeNeFolio/worktrees/827-discovery-motion、試行出力D:/NeNeFolio/outputs/827-discovery-motion、
恒久原本D:/NeNeFolio/agents/discovery-motion-design-20261115。
規則ARC-001/002/003/004/007/010/011/012、C-002/003/005/008/012/013/014/016/017/018、
QLT-008/009/010/012/013、GIT-001〜004。追加waiver none、WVR-0001継続。

## 先行調査

PR826の最終headを読み、既存appearance_port/folio_settings/command layer/idle経路を確認した。
公式Win32資料はADR0096末尾へリンクした。19:57〜19:59の背景Claude /designはRead/Glob/Grep/Skillだけで相談し、
編集/build/起動なし。brief、stream、標準error、終了0、助言を恒久原本へ保持する。
外枠/新行下線/進行可能な処理中の線、即時閉鎖、最終静止一致、保存選択とOS実効値の分離を採った。
mode下線slide/閉鎖fade/preview pulseは範囲外。

調査の失敗も残す。19:54と19:57のrgはPowerShellの明示pathへwildcardを渡しos123。
20:08以後はappearance_snapshotをapplicationと誤記、ADR0057とWVR-0001のbasename、ui_text_id.hを誤記した読取が失敗した。
rg --filesで実在pathを確認して読み直した。これらをbuild/検証成功へ数えない。
GitHub connectorの書込み403は直前の#825で確認済みのため、Issue827は認証済みghのbody-fileで作った。

## 現在の範囲

ADR0096はproduction変更前の独立読取レビューを経て受理。設定版7、OS三値観測、実効判定、検索の装飾と設定UIを実装した。最終CI・統合・通常exe反映は未実施。
未作成のcompletion.jsonを成功と解釈しない。

## 実装中の対象確認（最終確認は継続中）

全raw・入力snapshot・標準出力/error・終了値は上記outputsの各run名に保持する。

| 試行 | 結果と修正 |
| --- | --- |
| configure-01 | 正典toolchain/CMake configure成功 |
| build-01 | UIのdraw_settings_row/paint_command_layerの複雑度、on_input_messageの61行で失敗。描画単位/clip準備/IME入口を分け、閾値は変えない |
| build-02 / motion-unit-01 | build成功。`folio_tests.exe --motion`で旧1〜6/版7形、保存OFFの保持、保存拒否、OS三値/高コントラスト/外観失敗、RTF/本文不変を確認 |
| native-build-01 | 新native targetのicon生成依存が漏れ、app-icon.rc未生成で失敗。既存icon_target列へ追加 |
| native-build-02〜05 | それぞれ対象native build成功 |
| native-01 | 記録先を先に作ったためrecord-runが子起動前に拒否。後続はcapture先とrun先を別にした |
| native-02 / native-03 | 設定を操作メニューの通常commandとして探す誤り、次にSELECTを使うfixtureの誤りで失敗。公開ACCESSIBLE_SETTINGSのINVOKEへ修正 |
| native-04 | finder内のreport_outcomeをmodalと思い込んだfixtureが失敗。既存仕様はinline表示であり、静止を確認する試験へ修正 |
| native-05 | 名前modeでnative拒否からmodalを想定したfixtureが失敗。既存の本文mode・同じVIEWからEDITへ移る実際の拒否経路へ修正中 |
| native-build-06 | 新しい外側計測用message loopとcommand関数が複雑度/深さ制限で失敗。query・dispatch・一回のpumpへ分離 |
| native-build-07 | 修正後の対象native build成功 |
| measurement-build-01 / measurement-motion-01 | 正典eng.coverage.build_measurement、同じASan/UBSan/nullability/recover禁止とlink optionで成功。`folio_coverage.exe --motion`はfactoryとapplicationの確保拒否、配色準備失敗で旧snapshotを持つ間の静止を確認。full suiteは実行しない |

開く/選択の論理frame captureでは最終frameとOFFのRGB差分は0。途中の差分は外枠と選択下線だけだった。
この比較はnative-03の8枚で実施し、native実行全体の成功とは分ける。frameのみのupdate clipでnative previewの
EM_GETRECT/EM_GETFIRSTVISIBLELINE/WM_GETTEXT/WM_GETTEXTLENGTHとWM_SETTEXT/EM_SETSELが0、UIA revision不変も確認した。
IME・非表示・最小化・resize・閉鎖の停止はnative-04でmodalの誤った期待へ達する前に通過。

独立実装レビューはSetTimer失敗後の行選択がtimerなしの装飾を残すP2を検出。
開始可否の共通判定へ失敗状態を含め、次の明示resetまで静止を保つよう修正した。
タイマー失敗そのもののOS注入はまだ行っていない。最終native、三言語/三配色/二寸法画像、実pump計測は継続中。
追加waiver・gate緩和なし。試験の都合で製品へ注入窓口を追加しない。

## 完成境界の追加確認

- native-build-08〜10は成功。native-06は158項目、連続選択/新集合/0件/DPI/空の本文modeを足したnative-07は179項目で成功。
  名前modeの拒否はinlineとなる既存仕様を保持し、本文modeの同一VIEW→EDIT投入を拒否して実際のmodal中にtimerが無いことを確認した。
- visual-01は669項目/36 BMP（ja/en/zh-Hans × light/dark/high contrast × 最小700×450/広幅1250×850 × 設定/検索）で成功。
  全36枚をcontact sheetで目視し、最小/広幅日本語設定は原寸でも確認。helpの折返し、静止理由、選択、枠、previewを確認した。
- settings-native-01: `ctest --test-dir build -R '^settings_accessibility$' --output-on-failure`成功、21.55秒。
  追加toggleを含む既存設定の有限巡回と公開UIA境界を確認した。
- live-01は外側Pythonからprivate desktopのthreadへ送信する段階でWinError1444。子processは終了回収し、計測成功には数えない。
- live-02: `python tools/measure-discovery-motion.py <motion_native_tests.exe>`成功。実message loopで静止20,000.357ms、
  主thread CPU15.625ms、追加motion tick0、結果/選択/UIA revision不変。
  3ノート・Debug計装・温かいOS cacheの名前query2回、各7標本のmin/median/maxはOFF35.822/38.783/43.625ms、
  ON36.477/39.846/46.273ms。pipe/UI/OS待ちを含み、全端末の性能差を示す統計ではない。
  262,154 bytesの本文に対し8回のtimer配送を外側で各35ms遅らせても、772.315msで正しい1件へ完了しidleは進んだ。
  最終166項目。時計とCPUの読取は外側Pythonだけ。製品の検索driver・時計port・背景workerは追加しない。
- SetTimer失敗後の静止修正は同じ独立読取席が再確認し、未解消P1/P2なし。OSのSetTimer失敗注入自体は未実施。

以上の成功runはchild/wrapperとも0、recordingErrors=[]。後続のtest-only計測入口変更で製品や既存visual設定は変えていないため、
成功済み画像・中核・設定UIAを繰り返さない。native-07は新しく足した世代境界だけを含む対象selectorを実行した。
読み取り時の誤ったtests/win32/persistence_tests.cとPowerShellのeng/*.pyも記録する。実在fileのrg検索へ修正した。
物理IME・実スクリーンリーダー・全OS/DPI・実OS設定の切替・冷たいcache・大量ノートの時間保証は未確認。
同期I/Oと低優先timerの遅延は残る。新waiverなし、台帳版2/製品1.3.2は維持、設定版のみ7へ進める。

### C-013に沿った最終計測

live-02のfixture用stdin reader threadはC-013が検証driverを例外にしていないと独立読取で指摘されたため削除した。
規約・waiverを変えず、外側Pythonが同じprivate desktopへSetThreadDesktopして、起動した子のthreadだけへ配送する。
SwitchDesktopは呼ばず、表示中desktopを変えない。終了時は外側threadを元へ戻しhandleを閉じる。
[PostThreadMessageWのdesktop条件](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postthreadmessagew)、
[SetThreadDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setthreaddesktop)を確認した。

native-build-11とlive-03はchild/wrapper0、recordingErrors=[]。fixture161項目成功。実pumpの静止20000.510ms、
主thread CPU0.000ms、追加motion tick0。最終7標本ずつの検索2回min/median/maxは
OFF 36.003/36.437/46.528ms。
ON 35.579/37.214/45.876ms。
262,154 bytes本文へ2回の35ms配送遅延を与えても227.847msで1件へ完了した。前のlive-02と同じ製品実装だが、
最終のdriverで得た値はこの節を正とし、旧値も試行として残す。native-07の8画像もframe-comparison.jsonで再照合し、
開く/選択の最終frameとOFFがpixel一致、中間frameは相違することを確認した。

## 統合前の規約確認

conformance-01: `python eng/conformance.py --build-dir build`は実graphを含め違反0。
symbols-01: `python eng/symbols.py --build-dir build --require core application`は2library/違反0。
format-01: 変更したC/headerへのclang-format --dry-run --Werrorとgit diff --check成功。いずれもchild/wrapper0、recordingErrors=[]。
ローカルの全件gateは繰り返さず、最終headの必須CIで`pwsh -NoProfile -File ./eng/check.ps1`を実行する。
最終CI・統合・通常exe反映・全証跡保全/整理の確定結果は恒久completion.jsonに記録し、未作成は未完了とする。
