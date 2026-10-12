# いまのタスク — NeNe Folio

確認基点: 2026-10-12、main `8f2399c`（#278 / PR #279統合まで）。このファイルは状態の要約で、作業許可の正本ではない。
再開時はhideの現在の依頼、checkoutの差分、対象Issue/PRの実状態を照合する。
現在欄は置き換えて保ち、経緯を追記し続けない（[ADR0043](../adr/0043-current-instructions-and-single-agent.md)）。

## 現在の作業

- #270/#271/#274/#276/#278はclosed。BOM修正、運用と入口、ローカルメモリ全10件の整理・反映、前回の追加作業木の整理は完了した。
  到達点・検証・証跡は[当日日報](../reports/2026-10-12.md)と[引き継ぎ](../handoffs/2026-10-12.md)を参照する。
- [Issue #280](https://github.com/hideyukiMORI/nene-folio/issues/280)で検証頻度を整理。
  [ADR0044](../adr/0044-tests-at-completion-and-integration.md)・QLT-008/012を正本として、機能完成時の対象テストと統合時の最終head必須CIへ集約する。
  統合状態は紐付くPR、最終main・CI・終了整理は `D:/NeNeFolio/agents/280-test-cadence/completion.json`で確認する。
- 今回のhideの明示指定ではAstraの設計サナが要件・設計・判断・受理、SOL/LUNAの実装サナが適した調査・実装・検証を背景で担当する。
  #280はSOL席が調査と文書反映を担当。見た目を扱う仕事ではimagegenやClaude Codeのデザインリナ等を必要に応じて使う。
  この指定を他案件へ広げず、再開時は現在の依頼と適用範囲を照合する。
- 未統合の製品実装は無い。次の製品作業は現在の依頼か具体的な再現から選ぶ。

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

現在の運用は[ワークフロー§10](../DEVELOPMENT_WORKFLOW.md#10-エージェントの運用)、検証頻度は[QLT-012](../QUALITY_GATES.md#qlt-012--検証の頻度は変更に従う)。既定は単体実行。
対象プロジェクト/作業に明示された分担だけをその範囲で適用し、過去の停止・自走・許可は現在の指示にしない。
直近の[日報](../reports/2026-10-12.md)・[引き継ぎ](../handoffs/2026-10-12.md)は日付と節の時点を区別して読む。
旧現在欄595行は[整理前の固定版](https://github.com/hideyukiMORI/nene-folio/blob/2b0e376fb6838b13f6cf162ad395507b83c25607/docs/todo/current.md)に残る。
Waivers: none。
