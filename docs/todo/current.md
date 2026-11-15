# いまの状態 — NeNe Folio

## 2026-11-15 — 初回本文準備の費用を切り分ける

#833は1000件×4KiBの初回本文検索を既存private desktopで観測した。
無計装/計装の各3process・計30操作と原文不変、対象build/8境界試験/実graph規約検査/共有motion実pumpは成功。
初回検索中央値は無計装4.618秒/計装4.800秒。1000readの受信区間合計中央値は53.793ms、read以外のidle内は4.444秒。
配送の偏りがあるため純粋IO/paintや実時間の割合とは解釈せず、read以外を次の調査候補とする。主因は未特定。
製品コード・保存schema・版・有限work政策は変えない。負荷で中止した2試行は保全し、成功集計へ含めない。
全試行と限界は[品質記録](../quality/2026-11-15-discovery-cost.md)。
D:/NeNeFolio/worktrees/833-discovery-costと対応outputsには未統合sourceと実行記録があり保持する。
最終head CI・統合はIssue/PR、恒久保全・整理の確定原本は
D:/NeNeFolio/agents/discovery-cost-20261115/completion.json。未作成を完了とは扱わない。

直前の#831 / PR832はmain `28093b7602bb40b74aafd74e82cde9f31a129b49`へ統合済み。
最終head `e46ef18a134295fca8d05cca6301b3070b12e7a5`、必須CI43718941842成功（56 CTest、分岐90.766456%）。
Release45操作と原文不変が成功。中央値は1000件初回4.824秒、32件×1MiB全件一致再検索3.275秒。
製品入力/通常exe不変を確認して反映を再利用し、全4,445ファイル保全・照合後に追加worktree/outputsを整理した。
確定原本はD:/NeNeFolio/agents/discovery-release-20261115/completion.json、[品質記録](../quality/2026-11-15-discovery-release.md)。
#829 / PR830のDebug45操作も統合・全12,967ファイル保全/照合・整理済み。
両構成は別時間の条件付き観測で、純粋I/O/paint、cold OS cache、物理入力遅延の保証ではない。

[ロードマップ](../ROADMAP.md)の段階1〜4（お気に入りUI、タグ付与/全体管理、名前/本文検索、検索演出）は
main統合・通常exe反映・必要検証まで完了。仕様と操作は[操作表](../KEY_BINDINGS.md)、
到達点と確定原本は[日報](../reports/2026-11-15.md)と[引き継ぎ](../handoffs/2026-11-15.md)を参照する。
#819の作業木は全ファイルとGit登録を整理済みだが、OSが最後の空directory削除を拒否したため
D:/NeNeFolio/worktrees/819-note-tagsを0ファイルで保持し、盲目的な再試行はしない。
About・版上げ・配布/Store操作は未着手。11月12日のPR810は11月15日12:35時点でDraft/open、今回変更していない。
古い停止/継続/起動/相談の許可を新しい作業へ持ち越さず、現在のhide指示を優先する。

## 通常版と公開版

通常exeはC:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe、版1.3.2 / Windows1.3.2.0。
#827を11月15日21:23にsource `8e1ef719247db68acca5e82bf95f3a9c66ec507b`から正典経路で反映した。
SHA256 `682c65dc3553988de10fa5dc1eef7780d43a616af194feda4fa494858e00ab2e`。
反映前後の実data34ファイルのbytes/hash/作成・更新日時は一致。反映時にアプリは起動していない。
確定原本はD:/NeNeFolio/agents/discovery-motion-design-20261115/completion.json。

台帳版2・設定版7は旧アプリでは読めない。旧台帳1/設定1〜6は読め、起動だけでは書き換えない。
星はMarkdown/ZIPに含めない。通常版の書体修正・お気に入り・タグ・検索は公開済み1.3.2には未収載。

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
