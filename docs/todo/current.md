# いまの状態 — NeNe Folio

## 11月5日 — hide不在中の品質改善を継続

hideの現在指示は、停止の指示まで調査・試験・計測・必要な修正を続け、全試行と変更を記録すること。
主担当は単体で実行し、必要な保存系/本文処理の独立静読だけを委任した。古い停止・固定分担は持ち込まない。

**#659は本文倍率の恒久native回帰を追加。** 通常wheelの倍率と本文/選択/dirty/Undo/Redo保持、VIEW/EDIT再投入後の倍率保持を固定する。
倍率175checks/0.864898秒、共有fixture/追記helperのノート切替162checks/0.899475秒。両stderr空、子/補助/記録0。製品不変。
最終CI/統合/通常exe-data不変/全保全整理はD:/NeNeFolio/agents/659-zoom-native/integration-completion.jsonへ。

#657はPR658/最終CI41983221237成功、main5d776cbへ統合済み。通常exe/data32file不変、1377file全保全・専用領域整理済み。
原本は同agents/657-note-switch/integration-completion.json。
本日のそれ以前の統合結果・計測値・保全先は[日報](../reports/2026-11-05.md)と[引き継ぎ](../handoffs/2026-11-05.md)へ集約する。

全体journalは`D:/NeNeFolio/agents/quality-20261105/journal.md`。
[本文倍率](../quality/2026-11-05-zoom-native.md)・[ノート切替](../quality/2026-11-05-note-switch.md)・[ノート内検索](../quality/2026-11-05-search-native.md)・[通常置換](../quality/2026-11-05-replace-native.md)・[行索引](../quality/2026-11-05-line-index.md)・[履歴復元](../quality/2026-11-05-history-restore.md)・[編集可能判定](../quality/2026-11-05-editor-eligibility.md)・[設定操作](../quality/2026-11-05-settings-controls.md)・[文字変換](../quality/2026-11-05-text-conversion.md)・[設定再開](../quality/2026-11-05-settings-reopen.md)・[設定資源](../quality/2026-11-05-settings-resources.md)・[配置準備](../quality/2026-11-05-drawer-preparation.md)・[行選択](../quality/2026-11-05-drawer-hit-lookup.md)・[検索oracle](../quality/2026-11-05-index-matcher-oracle.md)・[名前集合](../quality/2026-11-05-name-list-lookup.md)・[設定固定語](../quality/2026-11-05-settings-tokens.md)・[取り込み再試行](../quality/2026-11-05-import-batch-retry.md)・[借用行参照](../quality/2026-11-05-markdown-view-lookup.md)・[Markdown段落](../quality/2026-11-05-markdown-paragraph-scale.md)・[改行正規化](../quality/2026-11-05-editor-normalization.md)・[本文検証](../quality/2026-11-05-ascii-validation.md)・
[保存](../quality/2026-11-05-save-preparation.md)・[日報](../reports/2026-11-05.md)・[引き継ぎ](../handoffs/2026-11-05.md)。
追加waiver none、既存WVR-0001継続。実app/実ノート/Storeは試験に使っていない。

## 11月4日22時 — 1.3.0 Store申請済み（hide報告）

hideのリリース依頼により#610/PR611を統合し、**[GitHub 1.3.0 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v1.3.0)を公開した**。
配布source/tagは`4359e9e2268bdeef135a4630dbaa241d57c3063d`。Mermaidと本日までの統合済み修正を含む。
最終head必須CI、正規ZIP/MSIX生成、payload起動・1.3.0表示・正常終了、公開後再取得/全11ZIP member照合が完了。
5,584fileを全保全/hash照合し、610の追加worktree/outputを整理済み。branch/commitは保持。

Store提出用MSIXと3言語更新文は`D:/NeNeFolio/releases/1.3.0/store-candidate-20261104/`。
**22:30頃、hideが「申請した」と報告したため、1.3.0（Windows1.3.0.0）は申請済みとして記録する。**
根拠はhideの報告であり、画面の独立確認ではない。申請番号・正確な申請時刻・審査段階・認定/公開は未確認。
申請記録は`D:/NeNeFolio/agents/release-1.3.0-20261104/store-status.json`。次はhideの審査結果/公開の報告を待つ。
記録更新#614の原本は`D:/NeNeFolio/agents/store-submission-20261104/integration-completion.json`。

リリース完了原本: `D:/NeNeFolio/agents/release-1.3.0-20261104/integration-completion.json`。
文書収載#612の原本: `D:/NeNeFolio/agents/release-1.3.0-final-20261104/integration-completion.json`。
[変更一覧](../release/1.3.0.md)・[品質記録](../quality/2026-11-04-release-1.3.0.md)・[日報](../reports/2026-11-04.md)・[引き継ぎ](../handoffs/2026-11-04.md)。
単体実行。追加waiver none、既存WVR-0001（WinHTTP callback）継続。

## 通常版と実機確認の範囲

通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、#651反映時sourceは`fe4703b`。
SHA256は`baffeebbc2179f4ff598f186628f8dfdeec88acdd8516b850a5c7387850149c5`。
#653/#655/#657/#659は製品ロジック不変で再buildしない。GitHub/Store配布1.3.0のsource`4359e9e`とは区別する。
通常app/実ノートは試験に使っていない。hideによる1.3.0の実機受理は未確認。

以前のStore1.2.0は#536/PR538、source`eb10419`でMermaidを含まない。
hideによる11月3日20:19:59の「Pre-processing」を、現在の1.2.0/1.3.0の認定/公開状態へ流用しない。
旧提出原本は`D:/NeNeFolio/agents/release-1.2.0-20261103/store-status.json`。新しい1.3.0原本とは別に保持する。
WACKとAppDataノートroot、Store署名版導入の従前の検証限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)を参照。
11月4日の品質journalは`D:/NeNeFolio/agents/quality-20261104/journal.md`。11月5日の品質作業とは根拠と実行を分けて保持する。

## 保留・保持物

#634のJSON変異試験は安全判定の通知を受けて保留。比較は未実行、repo変更/PR無し。
未完了のためD:/NeNeFolio/worktrees/634-json-boundary-oracleと同outputsを保持し、追加build/runは行わない。
入力・補助build失敗・通知とhideの通常品質対応継続指示は同agentsと全体journalへ記録した。

#466の表幅診断と#479の職場での文字ウェイト混在は保留。#530と旧削除拒否9作業木は保持し、削除を再試行しない。
正確な所在・理由は引き継ぎ書へ。10月31日にhideが削除した47出力を古い記録だけで現存扱いに戻さない。
大文書の投入/表配置、大量ノート検索準備、極狭幅表、flow/sequence以外のMermaidは未対応/保留。
WACKとAppDataノートroot、Store署名版導入の従前の検証限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)を参照する。
