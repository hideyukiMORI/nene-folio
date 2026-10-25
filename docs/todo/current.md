# いまのタスク — NeNe Folio

記録日: 2026-10-25。[#375](https://github.com/hideyukiMORI/nene-folio/issues/375)の連続入力中の描画停止と他部品の横断調査。
実装と対象検証を完了し、最終headの必須CI・統合・通常exe反映へ進む。最新状態はIssue/PRと恒久証跡の`completion.json`を照合する。
現在のhide指示とcheckoutを正とし、過去の分担・一時的な許可を継承しない。

## 現在の到達点

- 調査・設計・実装・検証は単体。候補所有/IME/同期paintと完成差分の独立レビューのみ読み取り専用で実施した。
- [ADR0065](../adr/0065-candidate-index-and-input-presentation.md)で操作/テーマ候補の対応を所有者の派生資源へ集約。
  描画・hit・キー・UIAの重複走査を除去し、入力後の合流点で可視の更新領域を描いてから通知する。
- 全操作を横断する60入力の最大post→paintはkey 2,921.10→11.62ms、wheel 1,203.62→44.08ms。
  専用非入力desktopの合成入力/GDI処理完了であり、実display呈示や一般的な性能保証ではない。
- 3言語/2幅/3描画DPI比率のoracle照合20,457 checks、失敗注入/Enter/paint中close 11 checks、native 59 checks、
  Debug build/tidy・規約検査が成功。独立レビューのEnter OOM誤報は修正し、再レビュー済み。
- 設定・メニュー・履歴・検索/置換・1,000ノート索引・キー説明も横断した。テーマの候補照会は改善したがnative再着色の負荷は残る。
  本文の合成wheelは未判定、native PageUp/Downでは60回描画と表示行の移動を確認した。
- 作業checkout: `D:/NeNeFolio/worktrees/375-continuous-ui-response`、一時出力: `D:/NeNeFolio/outputs/375-ui-input-latency`。
  恒久証跡: `D:/NeNeFolio/agents/375-ui-input-latency`。現在も存在するかは完了記録を照合する。
- 詳細/限界は[#375品質記録](../quality/2026-10-25-continuous-ui-response.md)、[日報](../reports/2026-10-25.md)、[引き継ぎ](../handoffs/2026-10-25.md)。
  保存schema・依存・ゲート変更なし、Waivers: none。

## 次の判断

最終PR headの必須CI成功後、`tools/merge-pr.ps1`で統合する。通常exeは`tools/prepare-real-machine.ps1`で反映し、自動起動しない。
[#369](https://github.com/hideyukiMORI/nene-folio/issues/369)の大量ノート初回全文読込は別件で未着手。
キー説明の折り返し計測、巨大本文のnative再着色/行番号index再構築、実reader接続時の大規模な参照更新は別の境界として残る。
前件#373/PR #374と#368/PR #370は統合・通常反映済み。各件の検証を新しい変更の合格へ読み替えない。

## 運用と保持

稼働中アプリ/未保存本文を終了しない。Computer Useや実画面の入力は使わず、公開Releaseは作らない。
統合/必要反映/恒久成果物を確認後、このtaskの追加worktreeと不要出力を整理し、branch/commitを保持する。
下記の旧taskは削除が自動承認レビューに拒否されて未削除であり、再試行/別手段での削除をしない。

- #368: `D:/NeNeFolio/worktrees/368-ui-latency`、`D:/NeNeFolio/outputs/performance-20261024`。
- #373: `D:/NeNeFolio/worktrees/373-gui-responsiveness`、`D:/NeNeFolio/outputs/gui-responsiveness-20261025`。

過去の自動承認レビューが再帰削除を拒否した9箇所は再試行/別手段の削除をしない（すべて `D:/NeNeFolio/outputs/` 配下）:
`350-menu-surface`、`350-release-20261024`、`351-native-name`、`351-dialog-feasibility`、`351-surface-rules`、`351-release-20261024`、
`364-theme-cursor`、`theme-navigation-20261024`、`366-theme-preview`。
旧#336/#338/#340/#342/#348/#349の一時物は今回対象外。旧 `check/2026-10-19/349-action-bar` は空状態で固まるため起動しない。
物理IME・実reader・物理DPI移動・DWM窓外影は未測定。
