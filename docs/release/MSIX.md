# Microsoft Store用MSIX候補

正典の入口は `eng/package-release.ps1`。Windows SDKのMakeAppx版は
`eng/tool-versions.json`に固定している。ZIPと同じclang-cl Release buildとpayload検証を使う。

```powershell
pwsh -NoProfile -File ./eng/package-release.ps1 `
  -Format Msix `
  -WorkDirectory D:/NeNeFolio/outputs/store-candidate-work `
  -OutputDirectory D:/NeNeFolio/outputs/store-candidate
```

両方とも未作成の絶対パスを指定し、親だけ先に用意する。既存出力を上書き・削除しない。
成功すると `NeNeFolio-<version>-win-x64.msix` と `SHA256SUMS` が出力される。
作業先には入力hash、Release build、stage、SDK展開結果、`release-evidence.json`が残る。
SDK作成・展開、版、PE、manifest、全ファイル集合/hash、書体license、PNG、block mapを確認してから公開する。
失敗時に出力先へ完成品を置かない。未完作業先と失敗記録を保全し、再実行には新しいパスを使う。

版はCMakeの一入力から生成する。Store向けは先頭1以上、4成分目0とする。
登録済みidentity、3言語、画像のサイズ/hashは`eng/msix.json`が正本。
`packaging/windows/render-icons.ps1`は紙/ノートの候補図案を新規directoryへ描き出す。
PNGの更新時は目視確認とhash更新を行う。図案原本や512px確認画像は配布物に入れない。

## 次の確認段階

このMSIXは未署名の提出候補で、生成成功はStore受理やインストール成功を意味しない。
Storeは公開時にMicrosoft署名へ置き換えるため、Store提出用のCA証明書購入は不要。
ローカルの実MSIXインストールでは、別に適切な署名と信頼が必要になる。
開発用manifestの登録は実MSIXのインストール試験と区別する。

実インストール後に、初回の保存先確認、合成ノートの保存と再起動、更新後の保持、
アンインストール後の外部ノート保持、Windows App Certification Kitを確認する。
利用者がAppData配下を保存先に選んだ場合は仮想化とアンインストール時の削除に注意が必要で、
Documentsなどの通常フォルダーと同じ保持を未検証のまま保証しない。
Storeページの説明・画像・プライバシーポリシー・サポートURL・価格/地域/レーティング・申請は別途整える。

関連: [ADR0080](../adr/0080-store-msix-through-release-pipeline.md)、[保存先ADR0077](../adr/0077-note-storage-location.md)。

## 1.0.0候補の実測（2026-11-01 / #486）

テスト署名した別コピーで実インストール、初回確認/取消、合成ノートの保存/再起動、
同一binaryの版違いfixtureからのOS更新、削除後のD側データ保持を確認した。
試験packageと一時証明書の信頼は解除済み。提出原本のhashは変更していない。
WACKは総合WARNING（22 PASS、任意88 FAIL、必須92 WARNING）で、全合格ではない。
検出箇所のフォント構造/HTTP・HTTPS起動経路と、実package窓のPerMonitorV2を別に確認した。
[実測・警告・未確認範囲](../quality/2026-11-01-msix-installation.md)を参照する。
3言語の掲載欄とpackage validationは完了したが、認定提出・Store公開は別段階として残る。

一次資料: [MSIX要件と署名](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/app-package-requirements)、
[SDK MakeAppx](https://learn.microsoft.com/en-us/windows/msix/package/create-app-package-with-makeappx-tool)、
[保存先と仮想化](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-behind-the-scenes)。
