# いまの状態 — NeNe Folio

## 2026-11-07 — Store 1.3.2公開報告と当日の記録

[Issue #789](https://github.com/hideyukiMORI/nene-folio/issues/789)に記録されたhideの
「Store は 1.3.2 出てたよ」に基づき、Store掲載1.3.2を本人報告による公開確認済みとした。
Issue作成03:10:16 JSTは記録日時で、実際の報告・認定・公開日時ではない。
Storeアプリ経由の1.3.2実機更新、Store署名版の品質試験、portal/package hashの独立確認は未確認。
原本D:/NeNeFolio/agents/release-1.3.2-20261106/store-status.jsonには旧#781申請報告も保持する。
Store1.3.1の実機更新成功報告#706は履歴であり、1.3.2の更新成功を意味しない。

当日の実績は[11月7日日報](../reports/2026-11-07.md)、再開条件は[11月7日引き継ぎ](../handoffs/2026-11-07.md)。
今回の依頼はこの記録作成。新しい品質テーマ・Store送信・通常app起動の指示へ広げない。
#789の最終head/CI/統合/整理の確定原本はD:/NeNeFolio/agents/789-daily-handoff/integration-completion.json。
記録が未完なら完了扱いにせず、実際のIssue/PRとcheckoutを照合する。

## 2026-11-10 — 英語UIの画像（#791）

dev.toの英語記事向けに、公開1.3.2の英語UI画像docs/images/1.3.2-main-en.png・1.3.2-themes-en.pngを追加した。
READMEの掲載画像も英語UIの2枚へ切り替えた。日本語UIの2枚は外部記事が参照するため保持する。
由来は[品質記録](../quality/2026-11-10-english-screenshots.md)、実績は[11月10日日報](../reports/2026-11-10.md)。
dev.to記事の差し替えは統合側の担当。統合の結果はIssue #791とそのPRが実状態。原本D:/NeNeFolio/agents/791-en-screenshots/。

## 通常版と公開版

#783/PR784は最終CI42239417180成功・main d343071へ統合し、00:24に通常exeへ反映した。
通常exeはC:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe、版1.3.2 / Windows1.3.2.0。
反映source `d343071aa918d6b5e7a41341009a2db607b5217c`、
SHA256 `ab97b69dcab303d8a75c09ed879237c9477939a64c1ddb6d8faabc02180018f1`。
実data33ファイルのbytes/hash/作成・更新日時不変、反映後は未起動。物理IMEと実ノートの見た目は未確認。
確定原本はD:/NeNeFolio/agents/editor-font-binding-20261106/integration-reflected.json。
旧integration-completion.jsonの反映待ちは過去の状態。専用領域は1481ファイル全保全後に整理済み。

[GitHub 1.3.2 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v1.3.2)は11月6日22:54:38 JST公開済み。
tag/配布sourceは `ba1e697deccb71e2f248f4759349d51ddec99e29`。通常版の#783修正は公開済みZIP/MSIXに未収載。
恒久配布はD:/NeNeFolio/releases/1.3.2/。ZIP/MSIXの正確なhashと検証範囲は
[1.3.2品質記録](../quality/2026-11-06-release-1.3.2.md)・[変更一覧](../release/1.3.2.md)を参照する。
公開・配布の確認済み結果を、Store署名版更新の検証へ広げない。

## README・Store原稿

#785/PR786は最終CI42245044660成功・main445315bへ統合済み。READMEの公式Store URLと公開1.3.2の画面2枚を更新した。
1268ファイル全保全後に専用領域を整理済み。原本D:/NeNeFolio/agents/readme-current-20261107/integration-completion.json。
#787/PR788は最終CI42247664237成功・maina1f6659へ統合済み。
[Store掲載原稿](../release/store-listing/README.md)は日本語・英語・簡体字の説明/機能/短い説明を準備済み、未送信。
貼り付け用9ファイルはD:/NeNeFolio/releases/1.3.2/store-listing-20261107/。正本commit/hashと全出力一致を確認済み。
製品のStore公開と掲載文の訂正反映は別の状態。次の外部反映を行う場合は各言語欄をこの原稿へ差し替える。
1249ファイル全保全後に専用領域を整理済み。原本同agents/store-listing-current-20261107/integration-completion.json。
次版Aboutは未着手。追加waiver none、既存WVR-0001継続。

## 保留・保持物

#466の表幅診断と#634のJSON変異試験を保留。#634は安全判定の通知による保留で、追加build/runを行わない。
#530は実装受理済みだが整理の自動承認審査が削除を拒否した領域。旧削除拒否9作業木とともに保持し、削除を再試行しない。
正確な所在と理由は[当日の引き継ぎ](../handoffs/2026-11-07.md)へ。#479は完了・整理済み。
大文書の投入/表配置、大量ノート検索準備、極狭幅表、flow/sequence以外のMermaidは従前の未対応/保留。
WACK/AppData root/Store署名版の検証限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)を参照する。
以前の実績・全体journal・保全先は[11月6日日報](../reports/2026-11-06.md)と[引き継ぎ](../handoffs/2026-11-06.md)から辿れる。
