# いまの状態 — NeNe Folio

## 11月5日 — hide不在中の品質改善を継続

hideの現在指示は、停止の指示まで調査・試験・計測・必要な修正を続け、全試行と変更を記録すること。
主担当は単体で実行し、必要な保存系/本文処理の独立静読だけを委任した。古い停止・固定分担は持ち込まない。

**#622はMarkdown段落分布の基点計測。** 製品変更なし。24RTF構築/12lookup、180sampleの監査成功。
1MiBのRTF中央値はASCII一行11.4459ms、79文字+LF59.7733ms、1文字+LF838.4193ms。
実UIやlookupの寄与率は未測定。次の改善候補をこの根拠から別単位で検討する。
文書検査/最終head必須CI/統合/保全整理の確定原本は
`D:/NeNeFolio/agents/622-markdown-paragraph-scale/integration-completion.json`。

#620はPR621/必須CI41933849295成功、main6107a26へ統合済み。通常反映/data32file不変、
1,401file全保全・専用領域整理済み。原本は同agents/620-editor-normalization/integration-completion.json。
改行正規化でASCII16MiB LF26.95035→14.92035ms。短い混在行の約6%増は残る。

#616はPR619/必須CI41930597783成功後、main`2c2f0be`へ統合済み。通常版反映・data32file不変、
1,378file全保全と専用worktree/output整理を完了した。原本は同agents/616-text-validation/integration-completion.json。
#617はPR618/main`038b8b7`、必須CI41924699377・通常反映・1,573file全保全・専用領域整理まで完了。
原本は同agents/617-save-preparation/integration-completion.json。branch/commitは保持する。

全体journalは`D:/NeNeFolio/agents/quality-20261105/journal.md`。
[Markdown段落](../quality/2026-11-05-markdown-paragraph-scale.md)・[改行正規化](../quality/2026-11-05-editor-normalization.md)・[本文検証](../quality/2026-11-05-ascii-validation.md)・
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

通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、#620反映時sourceは`6107a26`。
SHA256は`3fd9d8a9a97858782b7a45daa5d9eaaf6d973776f08523db28fe60317a4fa030`。
#622は文書だけなので通常exeを再buildしない。GitHub/Store配布1.3.0のsource`4359e9e`とは区別する。
通常app/実ノートは試験に使っていない。hideによる1.3.0の実機受理は未確認。

以前のStore1.2.0は#536/PR538、source`eb10419`でMermaidを含まない。
hideによる11月3日20:19:59の「Pre-processing」を、現在の1.2.0/1.3.0の認定/公開状態へ流用しない。
旧提出原本は`D:/NeNeFolio/agents/release-1.2.0-20261103/store-status.json`。新しい1.3.0原本とは別に保持する。
WACKとAppDataノートroot、Store署名版導入の従前の検証限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)を参照。
11月4日の品質journalは`D:/NeNeFolio/agents/quality-20261104/journal.md`。11月5日の品質作業とは根拠と実行を分けて保持する。

## 保留・保持物

#466の表幅診断と#479の職場での文字ウェイト混在は保留。#530と旧削除拒否9作業木は保持し、削除を再試行しない。
正確な所在・理由は引き継ぎ書へ。10月31日にhideが削除した47出力を古い記録だけで現存扱いに戻さない。
大文書の投入/表配置、大量ノート検索準備、極狭幅表、flow/sequence以外のMermaidは未対応/保留。
WACKとAppDataノートroot、Store署名版導入の従前の検証限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)を参照する。
