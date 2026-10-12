# いまのタスク — NeNe Folio

記録時点: 2026-10-12 21:44 JST。#292の実装基点main `913bc41`、公開tag `v0.1.0`。
このファイルは状態の要約で、作業許可の正本ではない。hideの現在指示、checkout、Issue/PRの実状態を照合する。
経緯は日報へ置き、現在欄は置き換えて保つ（[ADR0043](../adr/0043-current-instructions-and-single-agent.md)）。

## 現在の到達点

- hideの推奨案反映指示に基づく[Issue #292](https://github.com/hideyukiMORI/nene-folio/issues/292)は、
  表示3択の共通枠/選択下線、Ctrl+Shift+1/2/3、説明/キー表示の実装と対象検証を受理した。
  契約は[ADR0048](../adr/0048-layout-controls-and-native-tooltips.md)、実コマンド・画像・限界は[品質記録](../quality/2026-10-12-layout-control-checks.md)。
- root IME抑止、標準tooltipのABIサイズとSDKマクロの実行時評価を修正し、最終候補のnative確認は494 assertions成功。
  3言語×明暗の狭幅などPNG9枚を設計席が視認した。ゲート抑制・waiver・保存schema変更はない。
- この記録後の最終head必須CI、main統合、通常exe反映/起動、終了整理は、紐付くPRと
  `D:/NeNeFolio/agents/292-layout-controls/completion.json`を正本とする。結果追記だけでCIを再起動しない。
  恒久証跡は同ディレクトリ。追加作業木`D:/NeNeFolio/worktrees/292-layout-controls`と一時出力`D:/NeNeFolio/outputs/292-layout-controls`は完了条件を満たしてから整理する。
- 今回の明示分担はAstraが設計/判断/受理/統合、SOLが実装/対象検証。一般の既定は単体実行のまま。
  検証済み入力286件は不変を照合し、変更していないunitと#290の最大化/幅復元を再利用した（[ADR0044](../adr/0044-tests-at-completion-and-integration.md)）。

## 製品と公開版

- #286/PR#287、#288/PR#289、#290/PR#291はclosed/merged。設定中央ダイアログ、カテゴリ追加の＋、3表示モードは
  必須CI・通常exe反映・追加作業木/一時出力整理まで完了した。#290の3表示要件はhideが実機で受理した。
- 公開済み[v0.1.0ベータ](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.1.0)は上記UI更新より前の版。
  通常の最新exeは `C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`。今回のUI変更で公開tag/Releaseを差し替えない。
- 機能は[README](../../README.md)、要件は[SPECIFICATION](../../SPECIFICATION.md)、操作は[キー表](../KEY_BINDINGS.md)を参照する。

## 残る確認と判断条件

- #292の物理hoverによるtooltip popup、物理IME、OSの個別言語キー割当、別OS/DPI/実複数画面、スクリーンリーダーは未測定。
  120 DPIのquery/画像/合成キー確認や#290のhide受理を、これらの受理に広げない。
- 過去の258-1〜3・260-1〜3・266-1などは[実機チェックリスト](../quality/2026-09-22-visual-checklist.md)と各品質記録の条件を維持する。
- #243は、codepage65001なしでU+FFFDと隣接VTを保持できる新しい根拠が再開条件。同じ測定を繰り返さない。
- 旧C側41作業木と `D:/NeNeFolio/worktrees/210-rename-state` は従来のhide判断待ち。この変更だけで削除しない。

## 再開と履歴

[ワークフロー§10](../DEVELOPMENT_WORKFLOW.md#10-エージェントの運用)、[QLT-012](../QUALITY_GATES.md#qlt-012--検証の頻度は変更に従う)、
[当日日報](../reports/2026-10-12.md)、[引き継ぎ](../handoffs/2026-10-12.md)を時点付きで参照する。Waivers: none。
