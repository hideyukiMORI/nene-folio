# いまのタスク — NeNe Folio

記録日: 2026-10-25。[#373](https://github.com/hideyukiMORI/nene-folio/issues/373)の設定/モーダル応答と主窓操作について、実装・対象検証を完了。統合・通常exe・整理の最新状態はPRと下記の完了記録を照合する。
現在のhide指示、Issue/PR、checkoutを照合して再開する。前件#351の分担や過去の一時的許可を継承しない。

## 現在の到達点

- hideの今回の明示指定により、対話席のAstraが設計と受理を担当し、Sol/Lunaが調査・実装・検証を担当する。ClaudeCodeのデザインリナと設計を批評した。この分担は今回の作業に限る。
- [ADR0063](../adr/0063-derived-ui-metrics-and-surface-arrangement.md)で、固定UIの派生計測を所有者へ保持し、初期化途中の重複配置と不変な背景子窓の再配置を削減する。
- [ADR0064](../adr/0064-content-modal-and-window-shell.md)で、内容操作の禁止と主窓の移動・終了を分ける。native同期loopを保ち、root所有のmodal ownerを使う。保存/取消/IME/最外dispatchの寿命を維持する。
- 作業checkoutは `D:/NeNeFolio/worktrees/373-gui-responsiveness`。恒久証跡は `D:/NeNeFolio/agents/gui-responsiveness-20261025`、一時出力は `D:/NeNeFolio/outputs/gui-responsiveness-20261025`。
- Debug build/tidyと直接境界371、名前計測22/独立oracle288条件、native261、最小窓追加188 checksが成功。独立レビューのfocus指摘を修正し、完成画像も確認した。
- 同条件8回中央値で設定open21.05→11.93ms、名前入力初回paint40.24→17.09ms。起動後の初回設定約81msは残り、実display呈示の保証ではない。
- 統合は最終headの必須CI成功後に正規ツールで行う。通常exeへ反映後も自動起動せず、hideの実機試用へ渡す。現在のPR/CIと恒久証跡の`completion.json`を照合する。
- hideの並行作業へ干渉しないため、再開後はComputer Useを使わず専用非入力desktopと保存画像で確認した。
- 詳細/限界は[#373品質記録](../quality/2026-10-25-gui-responsiveness.md)、[日報](../reports/2026-10-25.md)、[引き継ぎ](../handoffs/2026-10-25.md)。schema/依存/ゲート変更なし、Waivers: none。

前件#368/PR #370の統合・通常反映は完了済み。過去の数値やCIを#373の合格へ読み替えず、履歴は[前件品質記録](../quality/2026-10-25-ui-latency.md)を読む。

## 次の判断

[#369](https://github.com/hideyukiMORI/nene-folio/issues/369)は大量ノートの初回全文読込中のUI停止。実装未着手。
新規合成データ初読と既読ファイルでI/O時間が大きく異なる。今回の描画改善によるI/O短縮として扱わない。
同期方針を変えるなら先に所有者/取消/最新語の採用/保存済み本文の契約をADRで固定する。
VIEWのテーマ変更ではRichEdit再投入、巨大本文の行番号index再構築も残る。未測定の一般性能を保証しない。

## 運用と保持

通常試遊の正規更新は `tools/prepare-real-machine.ps1`。稼働中アプリ/未保存本文を終了しない。公開Releaseは今回の依頼に含めない。
統合/必要反映/恒久成果物を確認後、追加worktreeと不要出力を整理し、branch/commitを保持する。
今回の `D:/NeNeFolio/worktrees/368-ui-latency` と `D:/NeNeFolio/outputs/performance-20261024` は、
必要物の収載・全entry照合後の削除コマンドを自動承認レビューが `blocked by policy` で実行前に拒否したため保持。
両パスとも未削除。拒否回避のため別手段で削除しない。詳細は恒久証跡の `cleanup-final.json` と引き継ぎ書に記録した。
前件#366は[品質記録](../quality/2026-10-24-theme-preview.md)、#351は[面の規則](../quality/2026-10-24-surface-rules.md)を参照。

過去の自動承認レビューが再帰削除を拒否した9箇所は再試行/別手段の削除をしない（すべて `D:/NeNeFolio/outputs/` 配下）:
`350-menu-surface`、`350-release-20261024`、`351-native-name`、`351-dialog-feasibility`、`351-surface-rules`、`351-release-20261024`、
`364-theme-cursor`、`theme-navigation-20261024`、`366-theme-preview`。
旧#336/#338/#340/#342/#348/#349の一時物は今回対象外。旧 `check/2026-10-19/349-action-bar` は空状態で固まるため起動しない。
物理IME・実reader・物理DPI移動・DWM窓外影は未測定。
