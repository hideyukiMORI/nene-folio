# いまの状態 — NeNe Folio

## 2026-11-15 — タグ付与と絞り込みの対象確認

hideの停止まで継続する実装指示に基づき、#819でロードマップ段階2aを実装した。
限定したMarkdown front matterのtagsを正本とし、付与/解除、保存済み候補と使用数、AND/OR、
星・検索とのAND、全条件解除を提供する。原文保持、Undo/Redo、保存失敗、再読込、改名/移動/別名保存、
単票/ZIP、3言語/最小幅/IME/UIAを対象確認した。設計は[ADR0092](../adr/0092-markdown-tags-and-filter.md)、
全試行と未確認範囲は[品質記録](../quality/2026-11-15-note-tags.md)。
最終CI・統合状態は#819のIssue/PR、通常exe反映と整理の確定結果は
D:/NeNeFolio/agents/tags-20261115/completion.jsonを正本とする。未作成を成功とは解釈しない。

#817 / PR818のお気に入りUIはmain `5593835b6ee1169dfc49ddba7fd04348d3a0cf5a`へ統合済み。
最終head `02645f198d15fc8c1a5816f174e0a7e339423b5d`、必須CI43656795092成功。
右端へ星を整列し、字形と当たり判定を分け、未登録星の条件表示と文字付きの範囲切替を設けた。
[ADR0091](../adr/0091-favorites-scope-and-quiet-stars.md)、[品質記録](../quality/2026-11-15-favorites-ui.md)、
D:/NeNeFolio/agents/favorites-ui-20261115/completion.jsonが確定原本。追加worktree/outputsは整理済み。

次は[ロードマップ](../ROADMAP.md)段階2bのタグ全体改名/削除。複数ファイルの更新単位・部分失敗と
再試行・未保存本文/外部改変を先行ADRで決める。検索プレビュー・演出・About・次版は未着手。
版上げ・配布/Store操作はしていない。到達点と判断は[日報](../reports/2026-11-15.md)、
保持領域と次の条件は[引き継ぎ](../handoffs/2026-11-15.md)。
11月12日の[PR810](https://github.com/hideyukiMORI/nene-folio/pull/810)は11月15日12:35でもDraft/open、今回変更していない。
過去の停止・継続・起動・相談の許可を、新しい作業へ持ち越さない。

## 通常版と公開版

通常exeはC:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe、版1.3.2 / Windows1.3.2.0。
#817を11月15日09:24にsource `5593835b6ee1169dfc49ddba7fd04348d3a0cf5a`から正典経路で反映した。
SHA256 `0558e7983b658f0c7f0920a4ae8656913279adebecdc736be34705d6ca8ae94e`。
反映前後の実data34ファイルのbytes/hash/作成・更新日時は一致。反映時にアプリは起動していない。
確定原本はD:/NeNeFolio/agents/favorites-ui-20261115/completion.json。#819の後続反映は上記原本へ記録する。

台帳版2・設定版6は旧アプリでは読めない。旧台帳1/設定1〜5は読め、起動だけでは書き換えない。
星はMarkdown/ZIPに含めない。通常版の#783書体修正と#813お気に入りは公開済み1.3.2には未収載。

[GitHub1.3.2 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v1.3.2)は11月6日公開済み。
tag/配布sourceは `ba1e697deccb71e2f248f4759349d51ddec99e29`、恒久配布はD:/NeNeFolio/releases/1.3.2/。
配布hashと確認範囲は[1.3.2品質記録](../quality/2026-11-06-release-1.3.2.md)へ。

| 対象 | 状態 | 未確認のこと |
| --- | --- | --- |
| Store掲載1.3.2 | hideの本人報告で公開確認済み（#789） | 認定・公開の日時、portalの独立確認 |
| Store経由の1.3.2更新 | hideの本人報告で確認済み（#793） | 更新日時・端末、Store署名版の品質試験 |
| Store掲載文の11月7日原稿 | 11月10日に公開ja-JP/en-US/zh-CNとの一致を照合済み（#801） | 認定日時・申請番号 |
| 掲載文のMermaid訴求 | 11月12日に訂正案#803を準備。hideが申請済み（#805）、貼り付け用は全部入力したと報告（#807） | 申請番号、入力と原稿の一致、認定、公開反映。図画像は未着手 |

[掲載原稿](../release/store-listing/README.md)の正本はMermaid訴求の訂正案。
貼り付け用はD:/NeNeFolio/releases/1.3.2/store-listing-20261112/、申請範囲は[品質記録](../quality/2026-11-12-store-listing-mermaid.md)。
最後の公開照合は旧11月7日原稿に対するもの。製品の公開・更新と掲載文の反映を区別する。
英語UI画像2枚はmain収載済み（#791）、dev.to記事の差し替えは統合側担当。詳細は[11月10日日報](../reports/2026-11-10.md)。

## 保留・保持物

#813の全16,551ファイルは保全・照合済み。削除は自動承認審査が `blocked by policy` として起動前に拒否したため未実行。
D:/NeNeFolio/worktrees/813-note-favorites と D:/NeNeFolio/outputs/813-note-favorites を保持し、再試行しない。
保全原本・manifest・削除拒否記録はD:/NeNeFolio/agents/favorites-20261114/。

#466の表幅診断と#634のJSON変異試験は保留。#634の追加build/runをしない。
#530と旧削除拒否9作業木も保持し、削除を再試行しない。#479は完了・整理済み。
正確な所在、未統合PR810の領域、前日のUI案の原本は[11月15日引き継ぎ](../handoffs/2026-11-15.md#保持領域再試行しないもの)。
大文書/大量ノート/極狭表と未対応Mermaid図種、WACK/AppData root/Store署名版の限界は従前どおり。
追加waiver none、既存WVR-0001継続。
