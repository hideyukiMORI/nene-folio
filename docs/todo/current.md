# いまのタスク — NeNe Folio

記録日: 2026-10-24。hideの「区切りのいいところで日報と引き継ぎ書を書いて」に従い、実装途中の再開点を確定した（[#361](https://github.com/hideyukiMORI/nene-folio/issues/361)）。
再開時は[引き継ぎ](../handoffs/2026-10-24.md)を読み、現在のhide指示・実Issue/PR・checkoutを照合する。この記録を新しい停止/再開の許可として扱わない。

## 完了と確認先

- GUI統一I-A/I-B（#348/#349）と空状態の描画ループ修正#356は統合済み。
- I-C [#350](https://github.com/hideyukiMORI/nene-folio/issues/350) は [PR #360](https://github.com/hideyukiMORI/nene-folio/pull/360) の最終head `5f11cad` 必須CI成功後、main `80e2814`へ統合済み。
  通常試遊build・未公開0.2.0 ZIP・実配布exe確認も完了。[品質記録](../quality/2026-10-24-menu-surface.md)。
- 最新の確認先: `D:/NeNeFolio/check/2026-10-24/350-menu-surface/NeNeFolio-0.2.0-win-x64/NeNeFolio.exe`。
  実120DPI・入力なし20秒の主thread CPUは0.03125秒、正常終了。公開Releaseは作成していない。
  旧`check/2026-10-19/349-action-bar`は空状態で固まるため起動しない。稼働中アプリや未保存本文を操作しない。

## I-D（#351）の再開点

- 作業木 `D:/NeNeFolio/worktrees/351-surface-rules`、枝 `feat/351-surface-rules`。製品チェックポイント `f1ec547d1ac7de18c4eb7349ff56258f62670d4c`、未統合。
- native名前入力/失敗箱、同名改名の結果、dialog配色、native暗幕、共通検索置換帯の実装まで。[途中検証記録](../quality/2026-10-24-surface-checkpoint.md)。
- 固定toolchainの `cmake --build D:/NeNeFolio/outputs/351-surface-rules/a/build --target nenefolio -j 4` はexit 0。合流したDebug compile/tidy/linkが成功。型配置とnullability注記の是正後に影響TUを再確認した。
- 設定/テーマの共通外形、ヘルプのscope見出し/S2/UIA、履歴の外形は未着手。製品GUI・画像・fresh native UIA・実音・独立レビュー・#351最終head CIは未実施。
  #351は完了/Readyではなく、本体試遊と配布候補は検証済み#350を維持する。
- 設計の最新正本は[作業枝ADR0060](https://github.com/hideyukiMORI/nene-folio/blob/feat/351-surface-rules/docs/adr/0060-ui-part-styles-and-state-rule.md)の決定22〜26と補足、同枝ADR0035/0052。
  mainのADR本文へ未統合の設計補正を取り込んだと誤認しない。議論・診断・実装引き継ぎは`D:/NeNeFolio/agents/351-surface-research/`。

全ノート検索EDIT Nameは[独立native再診断](../quality/2026-10-23-native-edit-uia-probe.md)で正常。製品への追加providerは不要。native検証clientをmanaged UIAと同じprocessにしない。
今回のGUI再開にはhideのAstra設計・SOL/LUNA実装/調査/検証・ClaudeCode `/design`との議論の明示分担を適用した。他案件へモデル固定を広げない。

## 保持する作業物

#350追加作業木は確認後に削除し、枝/commitを保持。#351作業木と`outputs/351-surface-rules/`は未完了・次回検証用のため保持する。
自動承認レビューが再帰削除を「blocked by policy」で拒否したため、次の4箇所を保持し、別手段での削除は試さない:
`D:/NeNeFolio/outputs/350-menu-surface/`、`350-release-20261024/`、`351-native-name/`、`351-dialog-feasibility/`（後3つも同じoutputs配下）。
詳細理由は提示なし。監査・29件追加収載・削除結果は`D:/NeNeFolio/agents/350-menu-surface/cleanup-completion.json`。
旧#336/#338/#340/#342/#348/#349の一時物は今回対象外。恒久agents・release-candidates・checkは保持。

schema・実行時依存・gate閾値/除外は不変。#351作業枝では音分類表を既存CNF-009へ追加登録した。Waivers: none。
物理IME・実reader・DWM窓外影・10,000件一覧の性能の未測定/未改善は引き継ぐ。
