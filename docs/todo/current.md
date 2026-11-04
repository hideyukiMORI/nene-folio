# いまの状態 — NeNe Folio

## 11月4日20時 — 品質改善を継続中

hideの再開依頼により、停止まで調査・試験・計測・必要修正と全記録を継続する。
#605/PR607まで最終head必須CI・main統合・通常反映/data32内容と日時不変・全保全整理が完了。
最新の製品main/通常exe sourceは3a40067。通常版は自動起動しておらず、hideの実機受理は未確認。
当日製品修正は20件。詳細と測定範囲は[日報](../reports/2026-11-04.md)と[引き継ぎ](../handoffs/2026-11-04.md)。

現在は#608の小ノートZIP件数診断を文書へ収載した。65534件を有効ZIPとして生成し、65535件を拒否、
全6ZIPの独立照合と18成果物再hash成功。対象条件の上限生成中央値20.39865msで、新たな製品修正根拠はない。
文書5件のみ、通常exeと成功済み製品検証を再利用。最終head必須CI・正典統合・exe SHA不変・全保全整理へ進む。
確定原本は`D:/NeNeFolio/agents/archive-entry-scale-20261104/integration-completion.json`。
診断outputは完了まで保持する。品質記録は[こちら](../quality/2026-11-04-archive-entry-count.md)。

全体journalは`D:/NeNeFolio/agents/quality-20261104/journal.md`。
追加waiverはnone、既存WVR-0001はWinHTTP callbackのみ継続。単体実行、実app/実ノートを試験へ使わない。
hideは必要なら実機試験できると申し出ている。実機でしか確認できない境界が生じたら手順と期待結果を絞って依頼する。

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
