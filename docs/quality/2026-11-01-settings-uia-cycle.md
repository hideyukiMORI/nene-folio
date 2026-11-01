# 設定のUIA循環と応答停止 — #504

hideが2026-11-01 16:48 JSTに、設定を開きミニマップを切り替える前の応答停止を報告した。
規則: ARC-001/002/004/008/011/012、C-002/008/012/016/017/018、QLT-007/008/012/013、GIT-001〜004。
[ADR0058補正](../adr/0058-accessible-command-surfaces.md)。Waivers none。

## 原因と修正

通常版PID47152はResponding=falseでCPU消費が継続した。終了せず2回のminidumpをD側へ採取し、
実行位置とstack内のsymbol候補にsurface読取とUIAutomationの列挙を観測した。stackの候補列は完全なunwindではない。
利用者データを含み得るsnapshotと元exe/PDBはローカルだけに保持し、本文やdumpをIssue/PRへ添付しない。

main b0478e4の実Win32 fixtureで設定のNextSiblingを有限回数たどると、9行目の保存先がテーマと同じbutton/slot7を返し、
先頭付近へ戻って循環した。#477の保存先追加に対するUIA対応が不足していた。ミニマップのsample処理ではない。
今回の操作、採取時の実行位置、同じ製品sourceの循環再現は一致する。15:33の過去AppHangにはdumpが残っておらず、
同じ原因だったことまでは断定しない。

設定行記述子へbutton識別を加え、保存先をテーマと区別する。同じ記述子から行と公開IDを往復し、列挙・名前・矩形・
focus・Invokeを実際の行へ結ぶ。Invokeも既存adopt_settings_rowへ合流する。provider、STA、世代失効は維持する。

## 検証

原本は`D:/NeNeFolio/agents/minimap-hang-20261101`。各runはtools/record-run.pyで入力と終了値を保全する。

- 修正前`red-run-01`: 3checks中1失敗。9行目で既出のテーマ要素へ戻ることを記録した。
- Debug/tidy `debug-01`成功。専用テスト起動用のmanifest結線後`debug-03`も成功。
- `ctest --test-dir build -R ^settings_accessibility$ --output-on-failure`（`contract-05`）成功。3言語318checks。
  前後の有限/一意な全行巡回、名前と親、leaf、保存先/テーマのfocus/矩形、閉鎖後の参照失効。
- `contract-negative-run-01`: 同じ新テストobjectとmanifestへ修正前UI libraryを結ぶと、重複検出で意図したexit1。
  反例driverはこの診断と終了値を確認してexit0。テストが今回の欠陥を検出できることを確認した。
- `uia-run-04`: 別process/MTAの実Windows UIAutomationから設定12要素のFindAllが完了。
  保存先とテーマの別identity、保存先SetFocus/Invoke、実native保存先dialogの取消、再FindAll、テーマInvokeが成功。
  合成保存済みMarkdownの読取/同一性2checksも成功。通常版へ入力しない専用desktopで実行した。

途中の試験側失敗も保持する。`contract-01`はctestがPATHになく子起動前に失敗。
`contract-02/03`は製品manifestを持たない試験exeが実windowを作れなかった。linkerによるmanifest合成は
side-by-side構成エラー14001になったため、`contract-04`は未起動。製品と同じ生成manifestをRCへ結ぶと解消した。
UIA clientの初回2件はFindWindowのdesktop選択による対象不一致、3件目はdialog探索の同じ問題。
明示desktopのEnumDesktopWindowsとPID照合へ直した。これらを製品検証成功とは数えない。

最初のCI `41288047946`ではDebug/tidy・新設を含む3つのCTest・coverageが成功したが、
QLT-007の隔離コピーに`tests/win32`が含まれず、反例検証前のCMake configureで失敗した。
同フォルダをコピー対象とARC-002のビルド未登録検査へ加えた。検査の除外・抑制は追加しない。
`proof-copy-run-03`で実際のコピーからCMake configure/toolchain_smoke buildが成功し、
ARC-002の正例と未登録Win32 translation unitの反例も成功。修正後のconformanceは違反0。
補助driverの初回は相対inputで起動前拒否、2回目は検査結果の属性名の誤記で失敗し、3回目で訂正した。

## 恒久回帰・限界

tests/win32は専用の検証合成ルートとして既存4libraryだけへ依存する。製品private実装の複製やテスト窓口を追加しない。
CTestのsettings_accessibilityは既存eng/check.ps1へ入り、同じsanitizer・静的解析・書式・30秒timeoutで検査する。
専用desktopとbuild/verification-settingsのportable dataを使い、通常data/保存先を変更しない。
実readerの発話、物理IME、OS全種類のclientは未確認。製品の設定schema/版1.0.0/依存と申請済みMSIXは不変。

最終head必須CI、main、通常版への反映と復旧、全保全/整理は同agentsのintegration-completion.jsonを確定原本とする。
