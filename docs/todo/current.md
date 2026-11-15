# いまの状態 — NeNe Folio

## 2026-11-15 — 名前検索と原文previewの対象確認

#821 / PR822はmain `0851f66de66313586d68fc80bf480b8ce88a2091`へ統合済み。
最終head `c3b36f7d1230e2511c9e8195698294549e304339`、必須CI43686022314成功（51 CTest、分岐90.8667%）。
通常exe反映・実data34件不変・全7,599ファイル保全を確認し、821の追加worktree/outputsは整理した。
D:/NeNeFolio/agents/tag-management-20261115/completion.jsonが確定原本。

継続指示に基づき#823で名前のあいまい検索と保存済み原文previewを共通面へ実装した。
[ADR0094](../adr/0094-name-discovery-and-source-preview.md)、仕様FR-047、[操作表](../KEY_BINDINGS.md#名前検索と原文プレビュー)。
既存engineのscope/有限work、条件の独立copy、最大200件と全一致数、候補移動と明示Openを使う。
原文は先頭80行/16KiBまで、native本文/Undoは候補移動で変えない。保存prompt後は宛先名を再解決する。
対象unit/確保注入、native拒否/復旧とコピー、3言語・3配色・最小/広幅、7回の外側計測を確認した。
全成功/失敗と限界は[品質記録](../quality/2026-11-15-note-discovery.md)。最終CI・統合は#823のIssue/PR、
通常exe反映・実data照合・保全/整理はD:/NeNeFolio/agents/discovery-design-20261115/completion.jsonを確定原本とする。
原本未作成を完了と解釈しない。作業中のD:/NeNeFolio/worktrees/823-note-discoveryと対応outputsは保持する。
本文live grep/一致行は次の焦点単位3b、演出は段階4。About・版上げ・公開は未着手。

hideの停止まで継続する実装指示に基づき、#821でロードマップ段階2bを実装した。
全カテゴリの保存済みノートを新しく確認して、タグを全体改名/解除する。既存名は集合へ統合し、
一件ずつ期待本文を照合して共通の履歴/公開へ渡す。変更済み・見送り・失敗・中止時の未処理を区別し、
再試行は同じ意図で確認し直す。選択本文は公開直後に採用し、native投入失敗でも公開済み件数を保つ。
設計は[ADR0093](../adr/0093-global-tag-management.md)、全試行と未確認範囲は
[品質記録](../quality/2026-11-15-tag-management.md)。最終CI・統合状態は#821のIssue/PR、
通常exe反映と整理の確定結果はD:/NeNeFolio/agents/tag-management-20261115/completion.jsonを正本とする。
未作成を成功とは解釈しない。

#819 / PR820のタグ付与と絞り込みはmain `11080b0c75cf75f85ccb6de4006af8926fb4d730`へ統合済み。
最終head `e79cefeca87c9df125c045c7683c00e2358332de`、必須CI43676165610成功（50 CTest、分岐90.9667%）。
通常exe反映・実data34件不変・全証跡保全済み。D:/NeNeFolio/agents/tags-20261115/completion.jsonが確定原本。
作業木の全ファイルとGit登録、outputsは整理済み。OSが最後の空directory削除を拒否したため
D:/NeNeFolio/worktrees/819-note-tagsを0ファイルで保持し、盲目的な再試行はしない。
[ADR0092](../adr/0092-markdown-tags-and-filter.md)、[品質記録](../quality/2026-11-15-note-tags.md)。

#817 / PR818のお気に入りUIはmain `5593835b6ee1169dfc49ddba7fd04348d3a0cf5a`へ統合済み。
最終head `02645f198d15fc8c1a5816f174e0a7e339423b5d`、必須CI43656795092成功。
右端へ星を整列し、字形と当たり判定を分け、未登録星の条件表示と文字付きの範囲切替を設けた。
[ADR0091](../adr/0091-favorites-scope-and-quiet-stars.md)、[品質記録](../quality/2026-11-15-favorites-ui.md)、
D:/NeNeFolio/agents/favorites-ui-20261115/completion.jsonが確定原本。追加worktree/outputsは整理済み。

次は[ロードマップ](../ROADMAP.md)段階3bの本文live grep・一致行プレビュー。
最新入力の世代、未保存本文、候補移動と確定、星/タグとの組合せ、段階4の演出/低動作を先行ADRで決める。
About・次版は未着手。
版上げ・配布/Store操作はしていない。到達点と判断は[日報](../reports/2026-11-15.md)、
保持領域と次の条件は[引き継ぎ](../handoffs/2026-11-15.md)。
11月12日の[PR810](https://github.com/hideyukiMORI/nene-folio/pull/810)は11月15日12:35でもDraft/open、今回変更していない。
過去の停止・継続・起動・相談の許可を、新しい作業へ持ち越さない。

## 通常版と公開版

通常exeはC:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe、版1.3.2 / Windows1.3.2.0。
#821を11月15日15:44にsource `0851f66de66313586d68fc80bf480b8ce88a2091`から正典経路で反映した。
SHA256 `316e208282bd485a853cbb6ad71f45ea82ee045f488ffde8c5615c2b51b9af33`。
反映前後の実data34ファイルのbytes/hash/作成・更新日時は一致。反映時にアプリは起動していない。
確定原本はD:/NeNeFolio/agents/tag-management-20261115/completion.json。

台帳版2・設定版6は旧アプリでは読めない。旧台帳1/設定1〜5は読め、起動だけでは書き換えない。
星はMarkdown/ZIPに含めない。通常版の書体修正・お気に入り・タグは公開済み1.3.2には未収載。

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
