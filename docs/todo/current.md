# いまのタスク — NeNe Folio

確認基点: 2026-10-12、main `7ff9b96`（#280 / PR #281統合まで）。このファイルは状態の要約で、作業許可の正本ではない。
再開時はhideの現在の依頼、checkoutの差分、対象Issue/PRの実状態を照合する。
現在欄は置き換えて保ち、経緯を追記し続けない（[ADR0043](../adr/0043-current-instructions-and-single-agent.md)）。

## 現在の作業

- #270/#271/#274/#276/#278/#280はclosed。BOM修正、運用と入口、ローカルメモリ全10件の整理・反映、検証頻度の整理は完了した。
  到達点・検証・証跡は[当日日報](../reports/2026-10-12.md)と[引き継ぎ](../handoffs/2026-10-12.md)を参照する。
- hideの「配布準備を進める」を受け、[Issue #282](https://github.com/hideyukiMORI/nene-folio/issues/282)で版の一元化と配布ZIP生成を実装した。
  [ADR0045](../adr/0045-single-version-and-verified-release-bundle.md)に判断を記録する。公開tag/GitHub Releaseは本Issueの範囲外。
  実状態は紐付くPR、対象検証・最終main・CI・成果物・終了整理は `D:/NeNeFolio/agents/282-release-prep/`の証跡で確認する。
  Release生成、配布検査器16/16、dataなし起動、3言語/最小窓の版表示は成功。範囲とhashは[品質記録](../quality/2026-10-12-release-prep-checks.md)。
- 今回のhideの明示指定ではAstraの設計サナが要件・設計・判断・受理、SOL/LUNAの実装サナが適した調査・実装・検証を背景で担当する。
  #282はSOL席がビルド・製品・配布検査の実装を担当。Astra席が設計・文書・受理・統合を担当する。
  この指定を他案件へ広げず、再開時は現在の依頼と適用範囲を照合する。
- 検証は[ADR0044](../adr/0044-tests-at-completion-and-integration.md)・QLT-008/012に従い、完成候補の対象検証と最終headの必須CIへ集約する。

## 製品の到達点

- #282でCMakeの版をexe資源・設定面・配布名へ導出し、検証済みのportable ZIPを生成できる。
  通常exeの最終反映source/hashと保持したZIPの場所は`D:/NeNeFolio/agents/282-release-prep/completion.json`で確認する。
- 直前の製品基点は`69e0a61`（#271 / PR #272）。BOMをファイル境界で一度だけ解釈し、本文のU+FEFFを保持する（ADR0042）。
  既存の対象unit・正典gate・実OS196 assertions・CIの範囲と限界は[品質記録](../quality/2026-10-12-text-boundary-checks.md)。
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
