# いまの状態 — NeNe Folio

## 2026-11-16 — 再開後: #837統合、About と コードブロックのコピーを実装中

hideの11月16日朝の指示（外回りの間、ロードマップに沿って進め、試したことを全部記録する）で再開した。
文書#835 / PR836はCI43768142385成功でmain `f35d051fe7dd5da8e20c6a71416c382d1189885a`へ統合・整理済み。

#837 / PR840はmain `07c3783030704ce0064789107dda038f627c8af2`へ統合済み。最終head
`3462300bdf0a38baed12435f9440653601b09ae8`、必須CI43777269038成功。
本文準備で読込件数だけが進んだidleは、検索面の全配置と全体再描画をやめて状態行だけを描き直す。
Release・1000件×4KiBの初回本文検索は主スレッドCPU中央値5843.750ms→1468.750ms、
32件×1MiBの全件一致再検索は4312.500ms→1250.000ms。共有端末で他席の負荷がある条件付き観測で、全端末の保証ではない。
修正前red/修正後greenの回帰試験を追加。全試行と限界は[品質記録](../quality/2026-11-16-discovery-progress.md)、
確定原本はD:/NeNeFolio/agents/837-discovery-progress/completion.json。

進行中（未統合。完了と扱わない）:

| Issue | 内容 | 所在と状態 |
| --- | --- | --- |
| #838 | 設定から開く「このアプリについて」。ADR0097案 | D:/NeNeFolio/worktrees/838-about、枝feat/838-about。実装とlocal commitまで、見た目の仕上げ中。未push |
| #839 | 閲覧表示のコードブロックのコピー（hideの11月16日の依頼）。ADR0098案 | D:/NeNeFolio/worktrees/839-code-copy、枝feat/839-code-copy。実装中。未push |

設計の原本はD:/NeNeFolio/design/about-20261116/handoff.md、D:/NeNeFolio/agents/about-20261116、D:/NeNeFolio/agents/code-copy-20261116。
状態の正本は各Issue/PRと作業木。詳細は[日報](../reports/2026-11-16.md)と[引き継ぎ](../handoffs/2026-11-16.md)。

環境: 他製品の席が同じ端末で作業しており、D:（HDD）は空き時間0%の時間帯が続く。
基準exeでもnative試験が4.8〜54.6秒と変動し、30秒のCTest timeoutを超えることがある。timeout・閾値は変えず、
直接実行と最終headの必須CIで判断する。壁時計の計測は主スレッドCPU時間を併記して扱う。

[ロードマップ](../ROADMAP.md)の段階1〜4（お気に入りUI、タグ付与/全体管理、名前/本文検索、検索演出）は
main統合・通常exe反映・必要検証まで完了。仕様と操作は[操作表](../KEY_BINDINGS.md)、
到達点と確定原本は[日報](../reports/2026-11-15.md)と[引き継ぎ](../handoffs/2026-11-15.md)を参照する。
#819の作業木は全ファイルとGit登録を整理済みだが、OSが最後の空directory削除を拒否したため
D:/NeNeFolio/worktrees/819-note-tagsを0ファイルで保持し、盲目的な再試行はしない。
Aboutは#838で実装中。版上げ・配布/Store操作は未着手。11月12日のPR810は11月15日12:35時点でDraft/open、今回変更していない。
古い停止/継続/起動/相談の許可を新しい作業へ持ち越さず、現在のhide指示を優先する。

## 通常版と公開版

通常exeはC:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe、版1.3.2 / Windows1.3.2.0。
#837を11月16日09:03にsource `07c3783030704ce0064789107dda038f627c8af2`から正典経路で反映した。
SHA256 `6a48faeb133b743330ec751e1e50897d7e8da54d3243b94b6b5dbcc9d1c490fe`。
反映前後の実data36ファイルのbytes/hash/作成・更新日時は一致。反映時にアプリは起動していない。
確定原本はD:/NeNeFolio/agents/837-discovery-progress/completion.json。

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
