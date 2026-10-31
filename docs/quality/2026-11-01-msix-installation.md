# Store候補の実MSIX試験 — 2026-11-01

Issue: #486。関連: #477 / ADR0077、#484 / ADR0080。
規則: GIT-001〜004、QLT-008/012/013。文書のみ、製品・保存schema・版・依存・ゲートの変更なし。Waivers none。

## 対象と許可

hideがStore掲載を入力し、全6欄の「完了」とパッケージの「Validated」を画面で確認した。
その後の「すすめて」により、テスト証明書の一時信頼登録と実MSIX試験を実施した。
認定提出・公開は未実施。3言語の掲載文と各5枚の実1.0.0画面は合成ノートだけを使用した。
恒久素材は `D:/NeNeFolio/releases/1.0.0/store-listing-20261031/{ja-JP,en-US,zh-CN}/`。

提出原本はclean main `370cdf48f8d5cb4c982d730c78fca2a5396dc7f9` の1.0.0候補。
`D:/NeNeFolio/releases/1.0.0/store-candidate-20261031/NeNeFolio-1.0.0-win-x64.msix`。
SHA256は `6dcf68baf76e26f2235bae70309418dde5e9616158243f39b91ad7836ea69c4b` で不変。
通常exeは0.3.0を保持し、再build・強制終了・通常dataの操作は行っていない。

## 実行と結果

実行原本のディレクトリを `A = D:/NeNeFolio/agents/msix-validation-20261031` とする。
試験出力は `D:/NeNeFolio/outputs/msix-validation-20261031`。Windows build 26300、120dpi。

| 実行 | 結果 |
| --- | --- |
| `pwsh -NoProfile -File A/prepare.ps1` | 署名済み試験コピーと0.9.0.0 fixtureを生成。提出原本は不変 |
| `python A/audit-payload.py` | 元の16 entryは不変。署名/catalogとそのContent Types追加、全block/file hashを照合 |
| `A/trust-test-certificate.ps1 -Action Add` | 承認後に指定証明書1件だけをLocalMachine/TrustedPeopleへ登録。Rootは変更なし |
| `A/package-action.ps1 -Action InstallPrior` | 実MSIX 0.9.0.0を現在ユーザーへインストール |
| `A/run-ui.ps1 -Scenario first-cancel-recovered -DesktopScenario first-cancel` | 実processのpackage identity、Documents/NeNeFolioの初回確認、取消と正常終了。root/config作成なし |
| `A/run-ui.ps1 -Scenario setup` | D側合成rootを選択。編集/保存、履歴、monalisaと行番号設定を実アプリから保存 |
| `A/run-ui.ps1 -Scenario prior-restart` | 初回確認を繰り返さず同じrootを開き、本文・履歴・設定を保持 |
| `A/package-action.ps1 -Action UpdateCandidate` と `A/run-ui.ps1 -Scenario candidate-restart` | 1.0.0.0へOS更新し、root・本文・履歴・設定を保持 |
| `A/run-wack.ps1` | WACK 10.0.26100.7705の既定Centennial全workflow、終了0。ただし下記WARNING |
| `python A/audit-wack-warnings.py` と `A/run-ui.ps1 -Scenario dpi` | 埋込manifestの正典検査、フォントhash/構造、実package窓PerMonitorV2/120dpiを確認 |
| `A/package-action.ps1 -Action Remove` | 試験packageを削除。D側4filesの全hash保持、仮想領域の起動設定は削除 |
| `A/trust-test-certificate.ps1 -Action Remove` | 指定した一時信頼登録を解除し、不在を確認 |

fixtureは同じ1.0.0 exe/resourcesを使い、manifest版だけを0.9.0.0に変更した試験専用品である。
OSの更新トランザクションとデータ保持を確認するもので、異なる製品binary間の移行を保証しない。
署名用秘密鍵は非exportableで作成し、2コピー署名後に鍵ごと削除した。PFXは作成していない。

画面確認は専用desktopと正典PostMessage/PrintWindowを使用。初回試行はhidden起動したdialogを
「可視窓のみ」で探索したため取得できなかった。実PIDの同じdialogを観測し、専用desktop内で
非前面表示して取消試験を完了した。製品修正や強制終了は不要だった。
最初の署名auditはSDKが追加するCodeIntegrity.catを想定せず失敗した。追加集合とContent Typesを
明示して照合し、元payloadと全hashが不変なことを確認した。正典ゲートは変更していない。

## WACKの結果と限界

原本 `wack-01/report.xml` は **OVERALL_RESULT=WARNING / PARTIAL_RUN=FALSE**。
22 PASS、任意項目88「ブロック済みの実行可能ファイル」FAIL、必須項目92「DPIAwarenessValidation」WARNING。
実行exit 0を「全項目合格」と読み替えない。

- 88: `ShellExecuteExW` は検証済みHTTP/HTTPS URIを既定browserへ渡す唯一のadapter。
  command名・実行ファイルpathはURI factoryが拒否する。NotoSansJP-Bold / SC-Bold / SC-Regularの
  `CsI` / `cdb` 等は、正典fontと同hashのOpenType CFFテーブル内の文字列で、実行プログラムではない。
  MicrosoftはDesktop Bridgeのoptional testsを情報提供と説明している。
- 92: WACKはbinaryを処理できず「DPI対応ではない」と報告した。原因は確定していない。
  対象exeから正典`embedded_manifest`で取り出した宣言はPerMonitorV2 / dpiAware=true / asInvoker。
  実インストール窓も`GetWindowDpiAwarenessContext`と`AreDpiAwarenessContextsEqual(-4)`でPerMonitorV2、
  `GetDpiForWindow`は120だった。これはWACK警告の消去やStore受理の保証ではない。

Store審査にはこの根拠を添えられる。WACKの全PASS、実Store署名版での導入、審査結果は未確認。
Documents既定pathの確認/取消とD側の保存/削除保持を確認したが、Documentsへの実書込、
利用者がAppDataをノートrootに選んだ場合の仮想化/削除保持は今回保証しない。
起動設定は実際に`Packages/<family>/LocalCache/Local/NeNeFolio`へ仮想化され、削除時に消えた。
物理IME、reader、異なるDPI間の移動、Windows 10/S modeの実機試験は追加していない。

## 記録と整理

試験のJSON・scriptはA、掲載素材の証跡は`D:/NeNeFolio/agents/store-listing-20261031`。
最終PR head/必須CI/main・全hash保全・一時worktree/output整理は
`D:/NeNeFolio/agents/msix-validation-20261031/integration-completion.json`で確定する。
成功済み実機試験は文書統合のために再実行しない。追加証明書信頼・試験packageは解除済み。

一次資料: [WACK実行](https://learn.microsoft.com/en-us/windows/uwp/debug-test-perf/windows-app-certification-kit)、
[必須/任意検査](https://learn.microsoft.com/en-us/windows/uwp/debug-test-perf/windows-desktop-bridge-app-tests)、
[DPI宣言](https://learn.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process)、
[MSIX仮想化](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-behind-the-scenes)。
