# いまの状態 — NeNe Folio

## 2026-11-05 22時台 — 1.3.1リリース対応

22:13頃、hideが1.3.1のリリース提案へ「お願い」と指示したため、#697で版更新・配布生成・起動確認・GitHub公開・Store提出原本の準備を進める。
この再開はリリース対応だけ。朝からの品質調査は21:28の指示で停止したまま、新しい改善・試験テーマや保留事項へ進まない。
[1.3.1変更一覧](../release/1.3.1.md)・[リリース品質記録](../quality/2026-11-05-release-1.3.1.md)を参照。
生成/公開の確定原本はD:/NeNeFolio/agents/release-1.3.1-20261105/integration-completion.json。原本未完の段階を公開済みと扱わない。

## 最新の製品到達点

#693 / PR694、最終CI42026620126のfull-gate/checkが成功し、main `b898c79413b578bbfdb96c615ee0528ddae62fee` へ統合済み。
検索はサロゲートの無いUTF-16先頭を4単位ずつ検証し、候補塊と末尾は既存処理へ戻る。API/保存形式/検索の意味は不変。
20通常本文・80条件・3600sampleが全結果一致。262144単位のASCII/日本語/末尾絵文字は前方約92〜93%、count約31〜36%短縮。
先頭絵文字などの費用も残り、65536単位の後方は+5.15%（18.32µs）。全数値・対象試験・静読の範囲は[検索検証の品質記録](../quality/2026-11-05-search-validation.md)へ。
原本は `D:/NeNeFolio/agents/693-search-validation/integration-completion.json`。

本日の完了履歴・不採用候補・全試行・保全先は[日報](../reports/2026-11-05.md)と[引き継ぎ](../handoffs/2026-11-05.md)から辿る。
全体journalは `D:/NeNeFolio/agents/quality-20261105/journal.md`。この停止記録#695の最終CI・統合・整理は同agents/695-current-state/integration-completion.jsonへ。
主担当単体で実装・計測・検証し、必要な独立静読だけ委任した。追加waiver none、既存WVR-0001継続。

## 通常版と実機確認

通常exeは `C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、反映sourceは `b898c79413b578bbfdb96c615ee0528ddae62fee`。
SHA256は `80633f7b61241e75577f2c4095fe87914a291c45e6b2027e63a3d98904f9bd8f`。#693反映時、実data32fileの全bytes/hash/作成更新日時不変を確認した。
通常app/実ノート/Storeは品質試験に使っていない。hideによる1.3.0の実機受理は未確認。
文書・native試験のみの変更を理由に通常buildを反復しない。配布1.3.0のsourceとは区別する。

## 配布とStoreの未確認事項

[GitHub 1.3.0 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v1.3.0)は公開済み。配布source/tagは `4359e9e2268bdeef135a4630dbaa241d57c3063d`。
11月4日22:30頃のhideの「申請した」という報告により、Store1.3.0（Windows1.3.0.0）は申請済みとして記録した。
申請番号・正確な申請時刻・現在の審査段階・認定/公開は未確認。次はhideの結果報告を待つ。
提出用MSIX/3言語更新文は `D:/NeNeFolio/releases/1.3.0/store-candidate-20261104/`、申請原本は同agents/release-1.3.0-20261104/store-status.json。
[変更一覧](../release/1.3.0.md)・[リリース品質記録](../quality/2026-11-04-release-1.3.0.md)・[11月4日日報](../reports/2026-11-04.md)・[11月4日引き継ぎ](../handoffs/2026-11-04.md)を参照する。
以前の1.2.0のPre-processing報告を現在の認定/公開状態へ流用しない。WACK/AppData root/Store署名版の検証限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)へ。

## 保留・保持物

#634のJSON変異試験は安全判定の通知を受けて保留。比較は未実行、repo変更/PR無し。
未完了のためD:/NeNeFolio/worktrees/634-json-boundary-oracleと同outputsを保持し、追加build/runは行わない。
入力・補助build失敗・通知とhideの通常品質対応継続指示は同agentsと全体journalへ記録した。

#466の表幅診断と#479の職場での文字ウェイト混在は保留。#530と旧削除拒否9作業木は保持し、削除を再試行しない。
正確な所在・理由は引き継ぎ書へ。10月31日にhideが削除した47出力を古い記録だけで現存扱いに戻さない。
大文書の投入/表配置、大量ノート検索準備、極狭幅表、flow/sequence以外のMermaidは未対応/保留。
WACKとAppDataノートroot、Store署名版導入の従前の検証限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)を参照する。
