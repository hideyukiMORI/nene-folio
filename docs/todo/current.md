# いまの状態 — NeNe Folio

## 11月4日21時 — 1.3.0リリース対応

hideのリリース依頼により#610で1.3.0を準備する。最新製品mainは#605の3a40067、
#608/PR609の文書統合を含む開始mainは4c3190d。両taskのCI・必要反映・保全整理は完了済み。
本日の品質改善は追加調査へ広げず、Mermaidと統合済み修正を配布物へまとめる。

唯一の版入力と配布説明を更新し、最終head必須CI・正典統合後にclean mainからZIP/MSIXを生成する。
配布payloadの起動/版表示、公開後照合、Store提出原本/更新文の引渡しと実状態の記録へ進む。
確定原本は`D:/NeNeFolio/agents/release-1.3.0-20261104/integration-completion.json`。
[変更一覧](../release/1.3.0.md)、[品質記録](../quality/2026-11-04-release-1.3.0.md)を参照。
hideは必要なら実機確認できると申し出ている。完成配布物から必要な手順を絞って案内する。

単体実行。追加waiver none、既存WVR-0001継続。実app/実ノートを試験に使わない。
以前の全体journalは`D:/NeNeFolio/agents/quality-20261104/journal.md`。

## 実行中アプリと配布

開始時に稼働していたPID18964はWindowsApps配下のStore版1.2.0。起動pathの観測のみで、終了/操作していない。
通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、朝の開始時sourceは`cc84e45`（Mermaid統合済み）。
#543以後の反映は各完了原本に従う。保存前強制終了・起動中上書きをせず、試験へ実ノートを使わない。

Store提出1.2.0は#536/PR538、source `eb10419`でMermaidを含まない。hideの前回報告は11月3日20:19:59の
「Pre-processing」。今朝の起動観測を認定/公開の独立確認へ拡張しない。GitHub公開版は1.1.0 Beta。
今回の1.3.0リリース依頼では新しい成果物を使い、1.2.0原本は上書きしない。提出原本は`D:/NeNeFolio/agents/release-1.2.0-20261103/store-status.json`。

## 保留・保持物

#466の表幅診断と#479の職場での文字ウェイト混在は保留。#530と旧削除拒否9作業木は保持し、削除を再試行しない。
正確な所在・理由は引き継ぎ書へ。10月31日にhideが削除した47出力を古い記録だけで現存扱いに戻さない。
大文書の投入/表配置、大量ノート検索準備、極狭幅表、flow/sequence以外のMermaidは未対応/保留。
WACKとAppDataノートroot、Store署名版導入の従前の検証限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)を参照する。
