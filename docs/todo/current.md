# いまの状態 — NeNe Folio

## 11月4日朝 — 品質改善を継続中

hideの新指示で、停止まで調査・試験・計測・必要な修正を継続する。試行と判断をすべて文書化し、
現行規約の対象検証・最終head必須CI・正典統合・必要反映・整理まで進める。
開始時mainは`ab7b732`、origin/main一致・clean、open PRなし。#541/PR542のCI41660944733は成功。

#543のMermaid形状境界はPR545の最終CI成功・main統合・通常反映・5434ファイル保全と作業木整理まで完了。
確定原本は`D:/NeNeFolio/agents/543-mermaid-shape-end/integration-completion.json`。

#544の画像cache退去はPR547/CI41734856635成功、main`786d6d5`へ統合・通常反映済み。
同じ9要求のmissは8から5へ減り、273checks成功。通常data32ファイルも反映前後で一致した。
完了原本は`D:/NeNeFolio/agents/544-image-cache-eviction/integration-completion.json`へ。

現在の焦点は#546。引用pipeの修正前redと、修正後の9正例/4拒否例・関連diagram対象の成功を確認した。
独立R/Oレビューは追加指摘なし。詳細は[品質記録](../quality/2026-11-04-mermaid-quoted-pipe.md)。
最終head/CI/main/通常反映/整理は`D:/NeNeFolio/agents/546-mermaid-quoted-pipe/integration-completion.json`へ収載する。
次は深いsequence activationの幅不足候補を実行で確認する。

日報・次回の詳細は[11月4日の日報](../reports/2026-11-04.md)と[引き継ぎ書](../handoffs/2026-11-04.md)。
全体journalは`D:/NeNeFolio/agents/quality-20261104/journal.md`。
追加waiverはnone、既存WVR-0001はWinHTTP callbackのみで継続する。

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
