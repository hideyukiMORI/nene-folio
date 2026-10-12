# いまのタスク — NeNe Folio

確認基点: 2026-10-12、main `bc8fbe6`（#274統合まで）。このファイルは状態の要約で、作業許可の正本ではない。
再開時はhideの現在の依頼、checkoutの差分、対象Issue/PRの実状態を照合する。
現在欄は置き換えて保ち、経緯を追記し続けない（[ADR0043](../adr/0043-current-instructions-and-single-agent.md)）。

## 現在の作業

- #274 / PR #275はmain `bc8fbe6`へ統合済み。運用と入口を整理し、ローカルメモリ6件を反映、追加作業木も整理した。
  実結果は `D:/NeNeFolio/agents/274-agent-context/completion.json`。
- #276: 残る技術メモ4件と索引、設定/検索/アイコンの旧説明、ADR索引を現在の正本へそろえる。製品変更は含まない。
  統合状態は [Issue #276](https://github.com/hideyukiMORI/nene-folio/issues/276)と紐付くPR、メモリ反映と終了整理は
  `D:/NeNeFolio/agents/276-context-followup/completion.json`で確認する。
- 直前の#270 / #271はclosed。未統合の製品実装は無く、次の製品作業は現在の依頼か具体的な再現から選ぶ。

## 製品の到達点

- 製品source: `69e0a61`（#271 / PR #272）。BOMをファイル境界で一度だけ解釈し、本文のU+FEFFを保持する（ADR0042）。
- 通常exeは同sourceを反映済み: `build/NeNeFolio.exe`、3,253,760 bytes、
  SHA256 `974C23E0D0DD779211A3B3A46F5105867630EBDFD78C3104EDF11F2BFA3CBAB2`。
- 対象unit・正典gate（分岐93.86%、proof19）・実OS196 assertions・CI成功。
  範囲と限界は[品質記録](../quality/2026-10-12-text-boundary-checks.md)。文書更新だけで製品を再ビルドしない。
- 利用できる機能は [README](../../README.md)、要件は [SPECIFICATION](../../SPECIFICATION.md)、判断は受理ADRを参照する。
  Phase 4の配布物公開は未完了。

## 残る確認と判断条件

- hideの目視258-1〜3・260-1〜3・266-1は未確認。過去の他項目も含む結果は
  [実機チェックリスト](../quality/2026-09-22-visual-checklist.md)と各品質記録を参照し、probe成功で目視済みにしない。
- #243の再開条件: codepage65001なしでU+FFFDと隣接VTを保持できる表現が見つかった場合。
  保持を満たす新しい根拠なしに同じ性能測定を繰り返さない。
- 物理IME・別DPI/別OSなど、未測定条件は品質記録に残る。過去の文字欠落を自動復旧するものではない。
- 旧C側41作業木と `D:/NeNeFolio/worktrees/210-rename-state` は従来のhide判断待ち。
  今回の運用整理だけで削除や再開の許可を得たものとして扱わない。
- #270/#271の追加作業木と不要な生成物は整理済み。保持証跡と結果は
  `D:/NeNeFolio/agents/2026-10-12-astra-resume/completion.json`。

## 再開と履歴の参照先

現在の運用は[ワークフロー§10](../DEVELOPMENT_WORKFLOW.md#10-エージェントの運用)。既定は単体実行。
対象プロジェクト/作業に明示された分担だけをその範囲で適用し、過去の停止・自走・許可は現在の指示にしない。
直近の[日報](../reports/2026-10-12.md)・[引き継ぎ](../handoffs/2026-10-12.md)は日付と節の時点を区別して読む。
旧現在欄595行は[整理前の固定版](https://github.com/hideyukiMORI/nene-folio/blob/2b0e376fb6838b13f6cf162ad395507b83c25607/docs/todo/current.md)に残る。
Waivers: none。
