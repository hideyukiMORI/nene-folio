# いまの状態 — NeNe Folio

## 2026-11-10 — 当日の到達点と待ち状態

当日の実績は[11月10日日報](../reports/2026-11-10.md)、再開条件は[11月10日引き継ぎ](../handoffs/2026-11-10.md)。
hideの最後の指示は「日報と引き継ぎ書を書いたらすべて止めて」。記録の後に新しい作業を始めない。
過去の継続指示や許可を次の作業へ持ち越さず、hideの次の依頼と実際のIssue/PR・checkoutを照合して再開する。

| 対象 | 状態 | 未確認のこと |
| --- | --- | --- |
| Store掲載1.3.2 | hideの本人報告で公開確認済み（#789） | 認定・公開の日時、portalの独立確認 |
| Store経由の1.3.2更新 | hideの本人報告で確認済み（#793） | 更新した日時・端末、Store署名版の品質試験 |
| Store掲載文の変更 | hideの本人報告で申請済み（#797） | 入力した言語と欄、申請番号、認定、掲載への反映 |
| #783の書体修正 | hideが実機で見て受理（#795） | 項目ごとの操作内容。公開済み1.3.2とStore版には未収載 |
| 英語UIの画像2枚 | mainへ収載済み（#791） | dev.to記事の差し替えは統合側の担当 |
| 次版（About含む） | 未着手 | Nibの開発が落ち着いてから着手する（hideの判断） |

掲載文が実際に反映されたという報告が来たら、申請とは別に記録する。
英語UIのテーマ一覧が日本語より1行少ないのは既存設計の結果で、不具合ではない（[品質記録](../quality/2026-11-10-english-screenshots.md)）。
当日の4件（#791/#793/#795/#797）は最終headの必須CI成功・main統合済み。番号とCIは日報の表を参照する。
Storeの報告の原本はD:/NeNeFolio/agents/release-1.3.2-20261106/store-status.json。

## 通常版と公開版

#783/PR784は最終CI42239417180成功・main d343071へ統合し、00:24に通常exeへ反映した。
通常exeはC:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe、版1.3.2 / Windows1.3.2.0。
反映source `d343071aa918d6b5e7a41341009a2db607b5217c`、
SHA256 `ab97b69dcab303d8a75c09ed879237c9477939a64c1ddb6d8faabc02180018f1`。
反映時は実data33ファイルのbytes/hash/作成・更新日時不変。11月10日22:03にhideの指示で起動し、hideが見た目を確認した後、22:34の時点では終了していた。
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
[Store掲載原稿](../release/store-listing/README.md)は日本語・英語・簡体字の説明/機能/短い説明を準備済み。申請の状態は冒頭の表を参照する。
貼り付け用9ファイルはD:/NeNeFolio/releases/1.3.2/store-listing-20261107/。正本commit/hashと全出力一致を確認済み。
製品のStore公開と掲載文の訂正反映は別の状態。
1249ファイル全保全後に専用領域を整理済み。原本同agents/store-listing-current-20261107/integration-completion.json。
次版Aboutは未着手。追加waiver none、既存WVR-0001継続。

## 保留・保持物

#466の表幅診断と#634のJSON変異試験を保留。#634は安全判定の通知による保留で、追加build/runを行わない。
#530は実装受理済みだが整理の自動承認審査が削除を拒否した領域。旧削除拒否9作業木とともに保持し、削除を再試行しない。
正確な所在と理由は[11月10日の引き継ぎ](../handoffs/2026-11-10.md)へ。#479は完了・整理済み。
大文書の投入/表配置、大量ノート検索準備、極狭幅表、flow/sequence以外のMermaidは従前の未対応/保留。
WACK/AppData root/Store署名版の検証限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)を参照する。
以前の実績・全体journal・保全先は[11月6日日報](../reports/2026-11-06.md)と[引き継ぎ](../handoffs/2026-11-06.md)から辿れる。
