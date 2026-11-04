# いまの状態 — NeNe Folio

## 11月4日20時 — 品質改善を継続中

hideのチェックポイント再開依頼により、停止まで調査・試験・計測・必要修正と全記録を継続する。
#603/PR604は最終head必須CI・main統合・通常反映/data32内容と日時不変・全保全整理まで完了。
開始mainは9de90fd、origin/main一致clean。通常exe sourceも9de90fd。

現在は#605のZIP CRC計算を1byte表へ変更し、対象Debug/unit・規約検査・Release比較が成功。
全256singlebyte/14境界、全8ZIP旧版bytes一致・独立CRC/member照合を確認。
64MiBの反転順比較は旧222.764875→候補123.9672ms、44.35%短縮。表960bytes増、API/形式/上限不変。
最終head必須CI・正典統合・通常反映/data照合・3root全保全整理へ進む。
確定原本は`D:/NeNeFolio/agents/605-archive-crc/integration-completion.json`。
初期archive-generation-scale outputは完了まで保持し、同taskで保全整理する。

詳細は[日報](../reports/2026-11-04.md)、[引き継ぎ](../handoffs/2026-11-04.md)、[品質記録](../quality/2026-11-04-archive-crc.md)。
全体journalは`D:/NeNeFolio/agents/quality-20261104/journal.md`。
追加waiverはnone、既存WVR-0001はWinHTTP callbackのみ継続。単体実行、実app/実ノートは試験へ使わない。

## 実行中アプリと配布

開始時に稼働していたPID18964はWindowsApps配下のStore版1.2.0。起動pathの観測のみで、終了/操作していない。
通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、朝の開始時sourceは`cc84e45`（Mermaid統合済み）。
#543以後の反映は各完了原本に従う。保存前強制終了・起動中上書きをせず、試験へ実ノートを使わない。

Store提出1.2.0は#536/PR538、source `eb10419`でMermaidを含まない。hideの前回報告は11月3日20:19:59の
「Pre-processing」。今朝の起動観測を認定/公開の独立確認へ拡張しない。GitHub公開版は1.1.0 Beta。
新しい配布指示なしに再提出/公開は行わない。提出原本は`D:/NeNeFolio/agents/release-1.2.0-20261103/store-status.json`。

## 保留・保持物

#466の表幅診断と#479の職場での文字ウェイト混在は保留。#530と旧削除拒否9作業木は保持し、削除を再試行しない。
正確な所在・理由は引き継ぎ書へ。10月31日にhideが削除した47出力を古い記録だけで現存扱いに戻さない。
大文書の投入/表配置、大量ノート検索準備、極狭幅表、flow/sequence以外のMermaidは未対応/保留。
WACKとAppDataノートroot、Store署名版導入の従前の検証限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)を参照する。
