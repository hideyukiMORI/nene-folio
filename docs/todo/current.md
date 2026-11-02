# いまの状態 — NeNe Folio

## 画像の高さ・wheel・ミニマップ — #522（2026-11-03）

hideの実機指摘を受け、実RichEditの文書高からwheel上限とミニマップ座標を揃えた。
画像付近のwheel量が跳ねず、ミニマップに画像の実寸ブロックを描く。対象検証は
[品質記録](../quality/2026-11-03-image-geometry.md)の173/95/298checks。
最終headの必須CI・統合・通常反映・保全整理は
`D:/NeNeFolio/agents/image-geometry-20261103/integration-completion.json`の確定結果を参照する。
作業木は`D:/NeNeFolio/worktrees/522-image-geometry`。hideから、この修正後に
コードシンタックスカラーと言語指定の対応を別Issueで続ける新指示を受けている。

## 画像表示と持ち出し — #520（2026-11-02、統合済み）

hideの依頼でローカル/HTTPS画像の遅延表示を追加した。Markdown/パスを自動変更せず、
images/への配置案内とZIP同梱を整える。方針は[ADR0085](../adr/0085-lazy-markdown-images.md)。
実装と対象検証は[品質記録](../quality/2026-11-02-markdown-images.md)へ。原文/画像表示/ZIP、加工表示、
移動/改名取消、HTTPSの期限/取消/証明書/容量を確認した。最終headの必須CI・統合・通常反映・整理は
`D:/NeNeFolio/agents/markdown-images-20261102/integration-completion.json`の確定結果を参照する。
PR #521の最終head `539c214`・必須CI `41464892662`成功でmain `ddcf151`へ統合した。
通常版へ反映し、画像入りサンプル2枚を追加してhideが実機確認した。
追加worktree/outputは全hash保全後に削除済み。恒久原本は
`D:/NeNeFolio/agents/markdown-images-20261102`。下記のStore確認待ち・#466/#479の保留は継続する。

## 1.1.0公開とStore認定待ち（2026-11-01 23:35）

[GitHub 1.1.0 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v1.1.0)を23:25:01 JSTに公開済み。
#516 / PR #517はhead `9152a45`・必須CI `41315187416`成功でmain `8ef5d03`へ統合した。
正規ZIP/MSIX生成、配布exeの新規起動/設定版/保存/旧0.3.0データ読込、公開後の全bytes/hash照合が成功。
[変更一覧](../release/1.1.0.md)・[品質記録](../quality/2026-11-01-release-1.1.0.md)。Waivers none。

Storeの既存Submission 1（提出原本1.0.0）は23:17:25のhide提供画面で公開を確認した。
続く1.1.0はhideが提出し、日報依頼後に「認定中になった」と報告した。認定中画面の直接確認と新Submission番号は未確認。
**1.1.0の認定合格・Store公開・実Store署名版導入は未確認。** 常時監視は依頼されていない。
現在の申請状況は`D:/NeNeFolio/agents/handoff-release-20261101/store-status.json`。
#516の23:26完了原本にある未提出は、その後のhide報告で更新された時点付きの事実である。

配布原本は`D:/NeNeFolio/releases/1.1.0/github/`と`D:/NeNeFolio/releases/1.1.0/store-candidate-20261101/`。
MSIXと3言語の更新説明をhideへ渡した。配布・通常版・保全整理の確定原本は
`D:/NeNeFolio/agents/release-1.1.0-20261101/integration-completion.json`。
次回は審査結果と公開状況を新しい証拠で確認し、新機能や再提出を古い許可だけで始めない。

## 通常版と実機受理

#494〜#514の転送・操作表示・ヘルプ・ミニマップ・応答改善はmain統合/通常反映/保全整理済み。
hideは#512/#514の結果を「かなり良くなってる」と受理し、その状態のリリース更新を依頼した。
完了履歴・各PR/CI/品質記録は[本日の日報](../reports/2026-11-01.md)、再開手順は[引き継ぎ書](../handoffs/2026-11-01.md)へ集約する。

通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`。#520でsource `ddcf151`・版1.1.0へ反映済み。
続く#522の最新反映/hash/data保持は冒頭の確定原本を参照する。
00:40時点では通常アプリの稼働なし。反映の直前にも実状態を確認し、保存/終了前の上書きや強制終了をしない。

本日の日報・引き継ぎ・本頁の更新は#518。最終head/必須CI/main・追加worktree整理は
`D:/NeNeFolio/agents/handoff-release-20261101/integration-completion.json`で確定する。
文書3件のみで、製品・schema・版・依存を変えず、対象試験を再実行しない。Waivers none。

## 継続保留と確認範囲

- #479の職場での文字ウェイト混在はhide指定で診断終了・修正保留。
  `D:/NeNeFolio/worktrees/479-editor-paste`と`D:/NeNeFolio/outputs/479-editor-paste`を未統合診断原本として保持する。
- #466の表幅診断と停止済み継続品質改善は再開しない。
  `D:/NeNeFolio/worktrees/466-table-caller-width`と`D:/NeNeFolio/outputs/466-table-caller-width`を未完として保持する。
- 構文色分け/言語指定は11月3日のhideの新指示で#522後に着手する。
  [未対応の棚卸し](../quality/2026-10-31-markdown-scope.md)は指示前の参考情報。Mermaid等は今回開始しない。
- 大文書のRichEdit投入/表配置・大量ノートの検索準備・極狭幅表の可読性は残る。
  合成入力/専用desktopの検証を物理IME・実reader・全DPI/OSの保証と混同しない。
- 1.0.0テスト署名版の実MSIX lifecycleは確認済み。一時信頼/秘密鍵/packageは解除済み。
  WACKの総合WARNINGとAppDataノートroot保持の未検証は[品質記録](../quality/2026-11-01-msix-installation.md)へ。
  1.1.0のStore署名版導入、認定合格/公開は別に確認する。

## 保持中の旧作業物

#494〜#516の完了worktree/outputは全hash保全後に削除し、branch/commitと恒久成果物を保持した。
10月31日にhideが削除した保全済み47出力の不在確認は
`D:/NeNeFolio/agents/storage-location-20261031/prior-cleanup-confirmation.json`が正本。
古いcompletionの保持記録だけで現存扱いに戻さない。

旧削除拒否で残るのは`D:/NeNeFolio/worktrees/`配下の9件。再試行や別手段で削除しない。

- `368-ui-latency`、`373-gui-responsiveness`
- `402-markdown-line-endings`、`404-markdown-containers`、`405-equal-table-columns`、`407-nullable-allocation`
- `410-table-container-indent`、`411-replacement-size`、`416-escaped-image-link`

未完#466/#479は上記と別に保持する。旧336/338/340/342/348/349等の指定外一時物も対象外。
旧`check/2026-10-19/349-action-bar`は空状態で固まるため起動しない。
既定は単体実行。次回は現在のhide指示・正本・実Issue/PR・checkoutを照合する（ADR0043）。
