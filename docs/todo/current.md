# いまのタスク — NeNe Folio

記録時点: 2026-10-12 22:48 JST。#294の実装基点main `e992d7e`、公開tag `v0.1.0`。
これは状態の要約で、作業許可の正本ではない。hideの現在指示、checkout、Issue/PRの実状態を照合する。

## 現在の到達点

- hideの指定した[Issue #294](https://github.com/hideyukiMORI/nene-folio/issues/294)の対象検証を受理した。
  NeNeNibと同じ125テーマを追加し、既存light/darkとsystemを含む128候補を検索/色見本/Enter/クリックで選ぶ。
  設定保存とExはcoreの1つの名前解析/配色表と既存採用経路へ合流する。settings.jsonは版4、旧版1〜3は読める。
- 契約は[ADR0049](../adr/0049-builtin-theme-catalog.md)、検証コマンド/失敗と修正/実画像の範囲は
  [品質記録](../quality/2026-10-12-theme-catalog-checks.md)。unit、Debug build/tidy、規約/依存、配布16件、
  native202 assertionsとPNG16枚の視認が成功。独立読み取りレビューに未解決のP1/P2はない。
- 最終headの必須CI、main統合、通常exe反映/起動と終了整理は、この記録後に紐付くPRと
  `D:/NeNeFolio/agents/294-theme-catalog/completion.json`へ記録する。結果追記だけでCIを再起動しない。
  恒久証跡は同ディレクトリ。`D:/NeNeFolio/worktrees/294-theme-catalog`と`D:/NeNeFolio/outputs/294-theme-catalog`は
  完了条件・唯一の成果物・稼働参照を確認してから整理する。
- 今回は調査/設計/実装/検証を単体実行し、必要な独立レビューだけ読み取りで依頼した。
  #292の一時的なAstra/SOL分担は継承していない。Waivers: none。

## 製品と公開版

- #286/PR#287、#288/PR#289、#290/PR#291、#292/PR#293はclosed/merged。設定、カテゴリ＋、表示3モードと
  共通枠/直接キー/tooltipはmainと通常exeへ反映済みで、追加作業木/一時出力を整理した。
- 公開済み[v0.1.0ベータ](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.1.0)は上記UI更新より前の版。
  通常exeは `C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`。今回の更新で公開tag/Releaseを差し替えない。
- 機能は[README](../../README.md)、要件は[SPECIFICATION](../../SPECIFICATION.md)、操作は[キー表](../KEY_BINDINGS.md)。

## 残る確認と判断条件

- #294の全125配色の個別実画面目視、物理IME、別OS/DPI、スクリーンリーダー、hideの実機受理は未測定。
  外部テーマファイルは依頼範囲外。#292の物理tooltip popupやOS個別キー割当などは既存の未測定区分を保つ。
- #243は、codepage65001なしでU+FFFDと隣接VTを保持できる新しい根拠が再開条件。同じ測定を繰り返さない。
- 旧C側41作業木と `D:/NeNeFolio/worktrees/210-rename-state` は従来のhide判断待ち。この変更だけで削除しない。

[運用](../DEVELOPMENT_WORKFLOW.md#10-エージェントの運用)、[当日日報](../reports/2026-10-12.md)、
[引き継ぎ](../handoffs/2026-10-12.md)を時点付きで参照する。
