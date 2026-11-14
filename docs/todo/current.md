# いまの状態 — NeNe Folio

## 2026-11-15 — お気に入りは通常版へ反映、UI改善案は未実装

#811/PR812の[ロードマップ](../ROADMAP.md)・[仕様案](../plans/2026-11-14-note-discovery.md)を基に、
#813/PR814でお気に入りを実装した。最終head `e7785fc` の必須CI43600639389成功、main `1b31ec2`へ統合済み。
星の付け外し、カテゴリ内の安定した優先表示、星だけと全文検索のAND、改名・移動・再起動の保持を提供する。
操作は[キー案内](../KEY_BINDINGS.md#お気に入り)、保存契約は[ADR0090](../adr/0090-note-favorites-and-display-order.md)、
検証は[品質記録](../quality/2026-11-14-note-favorites.md)。

hideの実機所感は星の位置・大きさ・未登録の枠線星の多さへの懸念。専門家ペルソナとClaude designへ相談し、
右端の整列、小さい字形と大きい当たり判定、未登録星の条件表示、文字付き「すべて／お気に入り」切替を推奨案にまとめた。
**UI改善案は未実装・最終受理前**。設計原本はD:/NeNeFolio/agents/favorites-design-20261114/interface-direction.md。
タグ・front matter・検索プレビュー・演出・About・次版は未着手。版上げ・配布/Store操作はしていない。

到達点・検証・担当と相談結果は[11月15日日報](../reports/2026-11-15.md)、次の判断条件・保持領域は
[11月15日引き継ぎ](../handoffs/2026-11-15.md)。この3文書の#815の最終CI・統合状態はそのIssue/PRを正本とする。
11月12日分は[PR810](https://github.com/hideyukiMORI/nene-folio/pull/810)が11月15日の確認でもDraft/openで未統合。
今回編集・統合していない。過去の停止・継続・起動・相談の許可を、新しい作業へ持ち越さない。

## 通常版と公開版

通常exeはC:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe、版1.3.2 / Windows1.3.2.0。
11月14日22:46にsource `1b31ec269cf6f9fd49e809651a2318beef41e20b` を正典経路で反映した。
SHA256 `c20e742458028081283d654b93df8b8703f45f6939d6ae2249e2d4e14016adb7`。
反映前後の実data34ファイルのbytes/hash/作成・更新日時は一致。23:23にhideの依頼で起動し、UI相談時は終了していた。
11月15日00:20の再照合でhash不変、その時点で稼働中に見つかったのはWindowsApps配下のStore版1.3.2。
確定原本はD:/NeNeFolio/agents/favorites-20261114/completion.json、後続の起動/UI相談は日報を参照。

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
正確な所在、未統合PR810の領域、未実装UI案の原本は[11月15日引き継ぎ](../handoffs/2026-11-15.md#保持領域再試行しないもの)。
大文書/大量ノート/極狭表と未対応Mermaid図種、WACK/AppData root/Store署名版の限界は従前どおり。
追加waiver none、既存WVR-0001継続。
