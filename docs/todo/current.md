# いまの状態 — NeNe Folio

記録日: 2026-10-25。直近の機能実装タスクはすべて完了。
本書の更新開始前（22:43 JST）の確認でOpen Issue / PRはともに0件。本書の状態記録更新は機能変更に含めない。
再開時は最新のhide指示、Issue / PR、checkoutを照合し、過去の記録を新しい実行許可として扱わない。

## 完了状態と根拠

- 直近の[#369](https://github.com/hideyukiMORI/nene-folio/issues/369)はCLOSED、
  [PR #385](https://github.com/hideyukiMORI/nene-folio/pull/385)はMERGED。
  製品コードの統合commitは`6a294697c804de15e30e06e0a9507369906a9bf3`。
- 最終PR head `ad5e812f264ad87e33fbe868ee79eaa48582d53f`の
  [必須CI](https://github.com/hideyukiMORI/nene-folio/actions/runs/40197477671)は成功。
  対象検証と性能条件は[品質記録](../quality/2026-10-25-first-search-responsiveness.md)、判断は[ADR0068](../adr/0068-idle-index-preparation.md)を参照する。
- 通常exe `C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`は上記製品commitから22:31 JSTに更新済み。
  更新後の自動起動はしていない。本書だけの変更では製品を再ビルドしない。
- #369の追加worktree `D:/NeNeFolio/worktrees/369-first-search-responsiveness`と
  一時出力 `D:/NeNeFolio/outputs/first-search-20261025`は、統合・通常反映・成果物保全の確認後に削除済み。
  作業branch / commitは保持している。
- 恒久証跡: `D:/NeNeFolio/agents/first-search-20261025/`。
  `completion.json`、`final-ci.json`、`final-pr.json`、`prepare-result.json`、`cleanup-final.json`に確定結果を記録。
  `frozen-fixtures-and-builds.zip`は22,957ファイルを保全し、全entryのSHA256照合済み。
  当日の経緯は[日報](../reports/2026-10-25.md)と[引き継ぎ](../handoffs/2026-10-25.md)を参照する。

## 未確認事項と次の判断

- 未完了の機能修正Issueは無い。新しい作業はhideの次の指示と、その時点のIssue / PRを起点にする。
- 物理IME・実UIA reader・物理DPI移動・DWM呈示は未測定。専用desktopの合成入力/描画検証と区別する。
- 一件分の重いI/O / UTF-8検証 / 写し作成と、最後の一致計算は同期処理のまま。
  入力が続くと検索準備の完了が遅れる。既知の制約と実測条件は上記品質記録を参照する。
- 下記の旧作業物は削除拒否のため保持している。本書の更新は削除や実機確認の追加実行を許可するものではない。

## 保持中の旧作業物

下記の13箇所は過去の自動承認レビューが削除を拒否したため未削除。再試行や別手段での削除をしない。

- #368: `D:/NeNeFolio/worktrees/368-ui-latency`、`D:/NeNeFolio/outputs/performance-20261024`。
- #373: `D:/NeNeFolio/worktrees/373-gui-responsiveness`、`D:/NeNeFolio/outputs/gui-responsiveness-20261025`。
- `D:/NeNeFolio/outputs/`配下の9箇所:
  `350-menu-surface`、`350-release-20261024`、`351-native-name`、`351-dialog-feasibility`、`351-surface-rules`、
  `351-release-20261024`、`364-theme-cursor`、`theme-navigation-20261024`、`366-theme-preview`。

旧#336/#338/#340/#342/#348/#349の一時物も今回の更新対象外。
旧 `check/2026-10-19/349-action-bar` は空状態で固まるため起動しない。
稼働中アプリや未保存本文を終了せず、過去の記録だけを根拠に実画面操作・公開Releaseを始めない。
