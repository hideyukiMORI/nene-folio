# いまのタスク — NeNe Folio

確認基点: 2026-10-12 18:45 JST、main `3aaf37c`（#286 / PR #287統合）、公開tag `v0.1.0`。このファイルは状態の要約で、作業許可の正本ではない。
再開時はhideの現在の依頼、checkoutの差分、対象Issue/PRの実状態を照合する。
現在欄は置き換えて保ち、経緯を追記し続けない（[ADR0043](../adr/0043-current-instructions-and-single-agent.md)）。

## 現在の作業

- hideが中央の設定ダイアログ、ドロワー内のカテゴリ追加、一覧/両方/ノートの3表示モードを承認し、順に続けて実装するよう指示した。
  一覧だけのときは窓も細くし、両方へ戻すと元の幅へ戻す。現在は2件目の[Issue #288](https://github.com/hideyukiMORI/nene-folio/issues/288)。
  設計は[ADR0039補正8](../adr/0039-note-trash-and-category-operations.md)。後続の表示モードまで、対象確認・統合・通常exe反映・終了整理を続ける。
- #286 / PR #287はclosed / merged。設定中央ダイアログは対象確認・最終headの必須CI・通常exe反映・追加作業木と一時出力の整理まで完了。
  [ADR0046](../adr/0046-distinct-settings-dialog.md)、[品質記録](../quality/2026-10-12-settings-dialog-checks.md)、
  `D:/NeNeFolio/agents/ui-settings-dialog-20261012/completion.json`で最終状態を確認する。
- 今回のhide指定に従い、Astraが設計・判断・受理・統合、既存のSOL席が実装・対象検証を担当する。
  各Issueの実状態、完成候補の検証、最終headのCI、通常exeと整理結果は当日日報・引き継ぎ・各証跡で照合する。
- #270/#271/#274/#276/#278/#280はclosed。BOM修正、運用と入口、ローカルメモリ全10件の整理・反映、検証頻度の整理は完了した。
  到達点・検証・証跡は[当日日報](../reports/2026-10-12.md)と[引き継ぎ](../handoffs/2026-10-12.md)を参照する。
- #282 / PR #283はclosed / merged。版の一元化と配布生成の設計は[ADR0045](../adr/0045-single-version-and-verified-release-bundle.md)、
  対象検証は[品質記録](../quality/2026-10-12-release-prep-checks.md)、通常exe反映・終了整理は`D:/NeNeFolio/agents/282-release-prep/completion.json`。
- hideの公開指示を受け、[Issue #284](https://github.com/hideyukiMORI/nene-folio/issues/284)で
  [v0.1.0](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.1.0)をベータ版（Pre-release）として公開した。
  clean mainから生成したWindows x64 ZIPとSHA256SUMSを添付し、公開後の再ダウンロードで両方のhash一致を確認した。
  公開記録は[当日日報](../reports/2026-10-12.md)、文書PRの統合・整理は`D:/NeNeFolio/agents/beta-release-20261012/completion.json`を参照する。
- #284 / PR #285の公開・記録・統合・終了整理は完了。今回のUI変更で公開済みtag/Releaseの差替えは行わない。
- 検証は[ADR0044](../adr/0044-tests-at-completion-and-integration.md)・QLT-008/012に従い、完成候補の対象検証と最終headの必須CIへ集約する。

## 製品の到達点

- #282でCMakeの版をexe資源・設定面・配布名へ導出し、検証済みのportable ZIPを生成できる。
  通常exeの最終反映source/hashと保持したZIPの場所は`D:/NeNeFolio/agents/282-release-prep/completion.json`で確認する。
- 直前の製品基点は`69e0a61`（#271 / PR #272）。BOMをファイル境界で一度だけ解釈し、本文のU+FEFFを保持する（ADR0042）。
  既存の対象unit・正典gate・実OS196 assertions・CIの範囲と限界は[品質記録](../quality/2026-10-12-text-boundary-checks.md)。
- 利用できる機能は [README](../../README.md)、要件は [SPECIFICATION](../../SPECIFICATION.md)、判断は受理ADRを参照する。
  Phase 4の初回ベータ配布物は公開済み。署名・インストーラ・自動更新は未実装。

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
