# いまの状態 — NeNe Folio

## 11月4日未明 — Mermaid統合・通常反映・整理済み

#539は[PR #540](https://github.com/hideyukiMORI/nene-folio/pull/540)、製品main `cc84e45`へ統合済み。
フローチャートとシーケンス図をCで生成し、本文先行の遅延表示・既存画像worker・進捗・取消へ合流した。
Claude designのB案を反映し、[最終CI](https://github.com/hideyukiMORI/nene-folio/actions/runs/41656331551)は成功、
分岐網羅率90.6157%。対応構文と未対応項目は[利用案内](../MARKDOWN.md#mermaidの図)、
判断は[ADR0089](../adr/0089-native-lazy-mermaid.md)、対象検証は[品質記録](../quality/2026-11-03-native-mermaid.md)。

通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`、1.2.0 / Windows1.2.0.0。
11月3日23:53に反映し、隣のdata32ファイルは内容・作成/更新日時まで前後一致した。
22:33のhideのEsc以降は画面操作と自動起動を行っていない。更新版の実機受理は未確認。
32,293ファイル保全後、#539のworktree/outputと参照用clone出力を11月4日00:00に削除した。
確定原本は`D:/NeNeFolio/agents/mermaid-20261103/integration-completion.json`。

今回の日報・引き継ぎ作成は#541。詳細は[11月4日の日報](../reports/2026-11-04.md)と
[引き継ぎ書](../handoffs/2026-11-04.md)へ。文書統合でmainが進んでも、製品入力が同じなら通常exeを再生成しない。
この文書taskの最終head/CI/main/保全整理は`D:/NeNeFolio/agents/handoff-20261104/integration-completion.json`で確定する。

## 配布と受理状態

Store 1.2.0は#536/PR #538、source `eb10419`の提出原本。Mermaidは含まない。
hideの最新報告は11月3日20:19:59の「Pre-processing」で、認定完了・公開・Store署名版導入は未確認。
最新原本は`D:/NeNeFolio/agents/release-1.2.0-20261103/store-status.json`。
通常版と提出物は同じ1.2.0表記でもsourceが異なる。GitHub公開版は1.1.0 Betaを維持する。
新しい報告・指示なしにStore監視や再提出を始めない。

常設ステータスバー#530は11月3日16:09、言語ラベル#532とアイコン#533は同日19:24にhideが受理済み。
経過と各完了原本は[11月3日の日報](../reports/2026-11-03.md)から参照する。

## 継続保留と保持物

#466の表幅診断と#479の職場での文字ウェイト混在は保留。
#530と旧削除拒否9作業木は保持し、削除を再試行しない。正確な所在・理由は[引き継ぎ書](../handoffs/2026-11-04.md)へ。
10月31日にhideが削除した47出力を古い記録だけで現存扱いに戻さない。
大文書の投入/表配置、大量ノート検索準備、極狭幅表、flow/sequence以外のMermaidは未対応/保留。
WACKとAppDataノートroot、Store署名版導入の限界は[MSIX品質記録](../quality/2026-11-01-msix-installation.md)を参照する。

追加waiverはnone、既存WVR-0001は継続。単体実行が既定。
次回は新しいhide指示・実Issue/PR・checkout・通常アプリの稼働pathを照合し、保存前の強制終了や起動中上書きをしない。
