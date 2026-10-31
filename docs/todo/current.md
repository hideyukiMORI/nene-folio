# いまの状態 — NeNe Folio

## キー操作更新・Store認定結果待ち — #490 / #488

hideの新依頼でCtrl+Eの閲覧/編集切り替え、Ctrl+Mの操作メニューと、5部品の「説明 (キー)」を追加した。
既存の保存処理とshortcut記述子へ合流し、Debug/tidy・対象単体・実Win32の88checksが成功。
[品質記録](../quality/2026-11-01-mode-shortcuts.md)・[キー仕様](../KEY_BINDINGS.md)・[ADR0081](../adr/0081-mode-shortcut-and-action-hints.md)。
最終head必須CI/main/通常exe反映・全保全/整理は
`D:/NeNeFolio/agents/mode-shortcuts-20261101/integration-completion.json`で確定する。
保存schema・版入力・依存は不変、Waivers none。これは申請済みMSIXより後の変更で、提出物は差し替えていない。

hideが1.0.0のSubmission 1を提出し、2026-11-01 00:31:51 JSTの画面で「認定中 / 前処理中」を確認した。
認定合格後すぐ自動公開する設定。補足文も保存済み。申請済みだが、合格・Store公開は未確認。
最新証跡は`D:/NeNeFolio/agents/msix-validation-20261031/submission-status.json`。常時監視はしていない。
#486の実MSIX初回/保存/再起動/OS更新/削除後の外部データ保持は確認済み、試験package・一時信頼・秘密鍵は解除/削除済み。
WACKは22 PASS・任意88 FAIL・必須92 WARNING、総合WARNING。根拠を添えたが全合格や審査受理とは扱わない。
#486/PR487は必須CI成功でmain統合済み。254files全hash保全後に追加worktreeと2一時出力を削除し、提出原本と15枚の画像を保持した。
[品質記録](../quality/2026-11-01-msix-installation.md)・[日報](../reports/2026-11-01.md)・[引き継ぎ](../handoffs/2026-11-01.md)。
実機確認/整理は同agentsの`integration-completion.json`、今回文書の最終CI/main/整理は
`D:/NeNeFolio/agents/handoff-20261101/integration-completion.json`で確定する。
次は認定結果と公開画面の確認。AppDataノートroot保持と実Store署名版導入は未確認。製品変更0、Waivers none。
#479/#466・停止済み継続品質改善は再開しない。構文色分け等は申請後ではなくStore公開後のhide指示と照合する。

## 直前のMSIX生成 — #484

hideがStore登録/予約とidentity提供を完了し、初回候補1.0.0と紙/ノートの図案作成を指定した。
同じ`eng/package-release.ps1`へ`-Format Msix`を追加。CMake一入力の版、共通Release payload、
登録値・PNG・SHA256 block mapを検査し、SDK pack/unpack後も全ファイルを照合する。
23対象proof、正典MSIX生成と展開、既存ZIPの互換確認が成功。保存schema/runtime依存は不変、Waivers none。
[手順](../release/MSIX.md)・[品質記録](../quality/2026-10-31-store-msix.md)・[ADR0080](../adr/0080-store-msix-through-release-pipeline.md)。
最終head必須CI・main・clean mainのMSIX候補・hash・保全/終了整理は
`D:/NeNeFolio/agents/msix-20261031/integration-completion.json`で確定する。
#486で実MSIXとページ素材の段階を確認した。WACK警告と認定提出/公開は上段を参照する。
通常exeは#482の0.3.0を保持し、当時起動中だったため終了・上書きしなかった。必要反映先は別のMSIX候補とした。

## 閲覧中のWebリンク起動 — #482

hideの依頼で、閲覧中のHTTP/HTTPSリンクを左クリックまたはリンク内Ctrl+Enterで既定browserへ開く機能を追加した。
文字選択drag、修飾付きクリック、編集/IME/modal/stream中は開かない。メール・file・裸URLは起動対象外。
同じURI factoryと明示portへ合流し、native hidden位置と検索/選択を対応した。保存schema/版/依存は不変。
Debug/tidy・対象単体/確保失敗、実RichEdit89checks・隔離copy81checks・固定example.comの実OS dispatchが成功。
[品質記録](../quality/2026-10-31-web-links.md)・[ADR0079](../adr/0079-view-web-link-activation.md)。Waivers none。
最終head必須CI・main・通常exeの版/hash/反映/起動・全保全整理は
`D:/NeNeFolio/agents/web-links-20261031/integration-completion.json`を確定原本とする。
物理IME/reader/DWM/異なるDPIとbrowserのページ読込完了は上の確認とは区別する。
#479/#466・停止済み継続品質改善は再開しない。

## 最新の表示調整 — Markdown #480

hideの依頼で水平線の上下を背景なし12pt余白にし、山括弧付きURI/emailの表示と数値/基本6種の文字参照を追加した。
[未対応の棚卸し](../quality/2026-10-31-markdown-scope.md)に負担と次候補を整理した。
言語選択/構文色分けはhide指定でStore公開後、Mermaidは描画基盤が必要なため今回見送り。
正典Debug/tidy、対象単体/確保失敗、実RichEdit45checksと明暗/狭幅の画像目視が成功。
[品質記録](../quality/2026-10-31-markdown-light.md)・[ADR0078](../adr/0078-lightweight-markdown-inline-rendering.md)。
保存schema・版・依存は不変、Waivers none。最終CI/main/通常exe反映と保全整理は
`D:/NeNeFolio/agents/markdown-light-20261031/integration-completion.json`で確定する。

職場での文字ウェイト混在は[#479](https://github.com/hideyukiMORI/nene-folio/issues/479)へ記録して修正保留。
合成WM_PASTEで書式混入経路は再現したが、職場の現象との同一性は不明。hide指定で診断を終了し、production変更0。
未完`worktrees/479-editor-paste`と`outputs/479-editor-paste`は診断原本と未統合記録のため保持する。
停止済み継続goalと#466は再開しない。

## 直前の機能追加 — ノート保存先 #477

hideが#475の一覧resize・両表示の境界dragを実機受理し、次の保存先設定へ進むよう依頼した。
[#477](https://github.com/hideyukiMORI/nene-folio/issues/477)で設定の「ノートの保存先…」を追加し、
現在の場所と次回起動の場所を分離した。portableの既定はexe隣data、package identityを持つ版は
Documents/NeNeFolioを初回候補として確認する。ノートを自動移動せず、変更中の本文・Undo・dirtyを保持する。
起動設定`nenefolio-startup.json`版1を別に置き、ノート側settings版4は変更しない。

Debug/tidy、対象単体/確保失敗、実adapter41checks、UI機能258checks、表示82checks、実exe4casesは成功した。
詳細と未確認範囲は[品質記録](../quality/2026-10-31-storage-location.md)と[ADR0077](../adr/0077-note-storage-location.md)。
Waivers none。最終headの必須CI・main・通常exeの版/hash/反映・全保全/整理は
`D:/NeNeFolio/agents/storage-location-20261031/integration-completion.json`で確定する。

#475/PR476はhead15a578d・必須CI41165338305成功でmain6232320へ統合・通常反映済み。
hideはテーマ選択・窓resize・本文wheelに続き、一覧resize/境界dragも受理した。
またhideが保全済み47一時出力を削除し、全対象の不在を読取確認した。旧削除拒否のworktree9箇所は残す。
[日報](../reports/2026-10-31.md)・[引き継ぎ](../handoffs/2026-10-31.md)を参照する。
停止済み継続goalと#466は再開しない。

## 継続品質改善の停止記録

記録日: 2026-10-30。20:14 JSTのhideの「区切りのいいところで止めて日報と引き継ぎ書を書いて」で品質改善を停止する。
#464の完了を確定し、#466は診断未実行の区切りで保持した。新しい改善・診断は始めない。
06:52頃からのAstra設計/受理・SOL/LUNA調査/実装/検証の分担は今回の継続作業に限った指定で、次回へ自動継承しない。
日報・引き継ぎの終了整理は[#467](https://github.com/hideyukiMORI/nene-folio/issues/467)。
最新の[日報](../reports/2026-10-30.md)・[引き継ぎ](../handoffs/2026-10-30.md)を参照し、再開時はhideの新指示と実Issue/PR・checkoutを照合する。
10月29日の完成成果は[23件の一覧](../reports/2026-10-29.md#一日の成果一覧2026-10-30の終了整理で確定)。

## 停止時の到達点

最新の製品改善は[#464](https://github.com/hideyukiMORI/nene-folio/issues/464)。本文検証の出力変数の初期化を走査前へ移した。
PR #465のhead`262ff18`・必須CI`41043496466`成功でmain`e4e1823`へ統合し、通常反映・全保全・整理まで確定した。
未計装/計装Release objectともloop内zero storeなし。Debug/既存text-validationと固定Bの13契約不一致0。
固定B一回は検索47.9622ms・Large検証18.0793ms・一件準備29.9140msで、改善幅や一般UI速度は保証しない。
7204files全hash保全後にworktreeを削除。output削除は実行前拒否で51箇所目として保持し、再試行しない。
確定原本は`D:/NeNeFolio/agents/464-validation-output/integration-completion.json`、詳細は[品質記録](../quality/2026-10-30-validation-output.md)。

未完の[#466](https://github.com/hideyukiMORI/nene-folio/issues/466)は、root63列に既存caller幅上端31679twipを一回渡す非製品診断の設計まで。
この値はnative上限や最小可読幅ではなく、現在製品の不具合再現とも扱わない。
helper/fixture/manifest作成0、parse/format0、Configure/Build/Inspect/Compile/Probe/Auditすべて0で停止した。
Issueはopenのまま。D側の`worktrees/466-table-caller-width`と`outputs/466-table-caller-width`を未完作業として保持する。
原本は同名`agents/`と`agents/quality-post464-table-width/`、再開条件と未実行範囲は[停止記録](../quality/2026-10-30-table-caller-width.md)。

#448の現在ノート検索・#451の縁resize・#452の窓操作・#460のASCII decoderは統合済み。
#456の水平表診断・#458の検索準備計測・#462の元SET拒否診断も文書統合済み。成果は日報へ集約した。
背景読込と極狭幅表は未解決。本文/置換の静読は新具体的不具合0で、既存の編集資源上限を維持した。
Waivers none。

## 公開済みbeta

[v0.3.0 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.3.0)は2026-10-27 01:14:32 JST公開済み。
配布・専用smoke・再取得照合まで成功した。[変更履歴](../release/0.3.0.md)、
[Markdownの範囲](../MARKDOWN.md)、[品質記録](../quality/2026-10-27-beta-release.md)を参照する。
#398 / PR #399に続く文書#400 / [PR #401](https://github.com/hideyukiMORI/nene-folio/pull/401)も
最終head `499984d135ae6b27e13a8276b33f66ee706aac57`の必須CI成功後、main `1845f9f`へ統合・整理済みと照合した。
確定値は `D:/NeNeFolio/agents/beta-release-20261027/docs-completion.json`。
公開済み成果物を今回の調査のために再生成していない。
## 通常exeとhideの実機確認

hide受理済み合成サンプル`data/サンプル/Markdown構文サンプル.md`は通常dataへ残している。
実ノート内容は記録・同梱しない。#477前の通常版はsource6232320・表示版0.3.0/Windows版0.3.0.0。
通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`。
最新の版/hash/反映状態は`D:/NeNeFolio/agents/web-links-20261031/integration-completion.json`を参照する。
起動中なら保存/終了を待ち、強制終了しない。通常dataの保存先を試験のために変更しない。

## 次に残る配布確認

#390の保存先方針は#477/ADR0077で実装し、MSIX生成は#484/ADR0080で追加した。
実インストールでの初回/更新/アンインストールは#486で確認。WACK警告の補足を添えて申請済み、認定/公開結果は未確認。
package identityを持たないMSI等は今回のinstalled判定対象ではない。
物理IME・reader・異なるDPI間の確認も合成fixtureとは区別する。次の作業はhideの新指示と照合する。

## 継続する確認範囲

- 表は63列/字下げ32767twipまで、極狭幅可読性は未解消。Webリンクは明示操作で起動。メール等は装飾のみ、HTML/画像等は対応外。
- 大量ノートの初回検索には同期処理と準備待ちが残る。[検索品質記録](../quality/2026-10-25-first-search-responsiveness.md)を参照する。
- 専用desktopの合成入力/描画と、物理IME・実reader聴取・DWM実画面呈示を区別する。
- MSIX生成は#484、テスト署名/実インストールは#486で確認。WACK警告とStore公開は未完、独自自動更新は対象外。
- 下記の削除拒否済み作業物は再試行しない。

## 保持中の旧作業物

2026-10-31 17:13、hideが提示済みワンライナーで47一時出力を削除した。対象は350/351系から475までの
保全済み出力だけで、全47pathの不在を読取確認した。過去の各completion原本の「保持」は当時の事実で、
現在の追記は`D:/NeNeFolio/agents/storage-location-20261031/prior-cleanup-confirmation.json`が正本。

旧削除拒否で残るのは`D:/NeNeFolio/worktrees/`配下の次の9箇所。再試行や別手段で削除しない。

- `368-ui-latency`、`373-gui-responsiveness`
- `402-markdown-line-endings`、`404-markdown-containers`、`405-equal-table-columns`、`407-nullable-allocation`
- `410-table-container-indent`、`411-replacement-size`、`416-escaped-image-link`

未完#466のworktree/outputは別に保持し、再開しない。旧336/338/340/342/348/349等の今回指定外の一時物も対象外。
旧 `check/2026-10-19/349-action-bar` は空状態で固まるため起動しない。
稼働中アプリや未保存本文を終了しない。今後の実画面操作・追加公開は、その時点のhide指示と照合する。
