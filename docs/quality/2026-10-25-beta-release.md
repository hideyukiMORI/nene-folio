# #388 — v0.2.0 β公開の確認

日付: 2026-10-25。hideの「β公開して」に基づくGitHub Pre-release公開。
Issue: [#388](https://github.com/hideyukiMORI/nene-folio/issues/388)。規則: ADR0045 / FR-014 / QLT-008・012・013 / GIT-001〜004。
製品コード・版入力・schema・依存・ゲート変更なし。Waivers: none。

## 公開物と由来

[v0.2.0 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.2.0)を23:19:01 JSTに公開。
`isDraft=false`、`isPrerelease=true`。annotated tag `v0.2.0`の対象はclean main
`3bc46969564b9422cbc8f49230cb815a3103f704`。
そのtreeはPR #387の最終head `590a53e1e44cde5d9379563f976407854dc6dd68`と一致し、
[必須CI](https://github.com/hideyukiMORI/nene-folio/actions/runs/40199179102)の成功を確認した。
本公開記録の文書PRも最終headの必須CIを要求する。製品入力不変の機能試験は繰り返さない。

| 添付物 | bytes | SHA256 |
| --- | ---: | --- |
| NeNeFolio-0.2.0-win-x64.zip | 22,829,677 | `e4d382f1ad19a0fb019d587744c1c97eae77c0fc1a1f866b661348e5a7a37d44` |
| SHA256SUMS | 95 | `f095f49ded407e8981c540f95429ea5dd04edec53dc0adc08471348181597049` |

Release exe SHA256: `6b79439968563005f1d464d55a013c37e983dee493cf2b4e42c168fb22471e63`。
通常の`build/NeNeFolio.exe`はこの作業で再ビルド・終了・起動していない。

## 配布と再取得の検証

```powershell
pwsh -NoProfile -File ./eng/package-release.ps1 -WorkDirectory D:/NeNeFolio/outputs/388-beta-work -OutputDirectory D:/NeNeFolio/outputs/388-beta-package
python D:/NeNeFolio/agents/beta-release-20261025/verify-before-publish.py
gh release download v0.2.0 --pattern NeNeFolio-0.2.0-win-x64.zip --pattern SHA256SUMS --dir D:/NeNeFolio/outputs/388-beta-download
python D:/NeNeFolio/agents/beta-release-20261025/verify-download.py
```

すべてexit 0。正典配布器のRelease build、conformance、symbols、実exeのAMD64・版0.2.0/0.2.0.0・manifest・
宣言されたOS DLL 14種・同梱11ファイル・書体hash/license・ZIP/展開照合が成功。
生成時の`sourceDirty=false`と入力hashを保存した。公開後のZIP/SHA256SUMSは生成物とbyte単位で一致し、
再取得したZIP内11ファイルも配布器の検査結果とhashが一致した。利用者データは含まない。

## 配布exeの起動・保存・更新

Windows 11の専用desktop上で実際の配布exeを起動。既存の`tools/post-keys.ps1`と
`tools/capture-window.ps1`へ正確なPIDと主窓の題を渡した。SwitchDesktop、foreground入力、通常ノートの操作は行わない。

- 新しい展開先に`data/`が無い状態から起動し、設定の`Version 0.2.0`を画像で確認した。
- カテゴリとノートを作り、合成本文`Beta release save check 日本語。`を保存。UTF-8の36bytesが期待値と一致し、正常終了した。
- 公開済みv0.1.0のZIPも再取得し、そのSHA256SUMSと一致を確認。同じ合成本文を旧版の実exeで作成・保存・正常終了した。
- 旧版が作成した`data/`の5ファイルを新しいv0.2.0展開先へコピーして開いた。更新確認の終了時点で5ファイルすべてのhashが一致。
  本文のhashは新規保存・旧版保存・新版再読込で同一。各採用processのexit codeは0。
- 閲覧paneから取得したUTF-16も日本語まで一致した。初回のPrintWindow画像では末尾が描画されず、遅延のみの再取得では本文全体が空に見えた。
  明示的なRedrawWindow後の再取得では英語UIでも全文を確認した。初回画像を表示受理に使わず、DWMの実画面で同じ現象が起きるとは断定しない。
  この診断では確認用データの表示言語だけを変更し、本文の一致を維持した。
- 最初の保存確認driverは編集本文へ`:q`を入力したため終了待ちに失敗した。対象PID/実行パスを確認して通常WM_CLOSEで終了。
  入力手順をEsc後の終了に修正し、新しい展開先で保存・正常終了を確認した。初回ログ・画像も保持した。

外部driver: `run-ui.ps1`の`fresh-final` / `legacy` / `upgrade`、文字列・再描画の追加確認は`run-pane-delay.ps1`。
初回の失敗を成功に数えず、最終結果・実行パス・PID・時刻は各`*-ui-result.json`、byte比較は`smoke-data-verification.json`に記録。

物理IME候補操作、人によるreader聴取、DWM呈示、全対応OSの実機受理は未確認。署名・インストーラ・自動更新は提供しない。
大量検索の準備時間と同期処理の制約は[変更履歴](../release/0.2.0.md)と[検索品質記録](2026-10-25-first-search-responsiveness.md)に明記した。

## READMEの最新画面

hideの追加指示に基づき、公開ZIPと同じexeを別の専用展開先で起動し、README用の画像を撮影した。
3カテゴリ7ノートの本文は公開用に作ったサンプルで、利用者の通常データは使わない。
`docs/images/0.2.0-main.png`はライトテーマの閲覧画面、`docs/images/0.2.0-themes.png`はNordのテーマ候補画面。
どちらも1200×800の実ウィンドウを、RedrawWindow後に既存PrintWindowツールで取得した。画像の合成・装飾・加工は行わない。
画像を実際に確認し、最初のサンプルの不自然な一文字折返しは本文を短くして再撮影した。
採用画像は`readme-final`の正常終了したprocessから取得し、原本とのbyte一致を保って収載した。hashと由来は`screenshots.json`。

READMEのv0.1.0配布案内・古い新規ノート位置・カテゴリ操作/通知説明も現行へ更新し、直接ダウンロード、倍率、初回検索準備、テーマプレビューを追記した。

## 証跡と整理

恒久証跡: `D:/NeNeFolio/agents/beta-release-20261025/`。公開物そのものは`published-assets/`にも保管。
`source-ci.json`、`release-evidence.json`、`source-inputs.json`、`pre-publish-verification.json`、
`published-release.json`、`download-verification.json`が公開の根拠。
文書の差分・参照・規約検査と最終PR CI、作業物の収載・削除前監査・整理結果は同所の確定記録へ保存する。
整理対象は#388のworktreeと配布生成・展開・再取得に使ったD側出力だけ。旧削除拒否済み作業物は触らない。
