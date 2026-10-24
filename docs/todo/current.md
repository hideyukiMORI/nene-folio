# いまのタスク — NeNe Folio

記録日: 2026-10-25。[#368](https://github.com/hideyukiMORI/nene-folio/issues/368)で設定/テーマ等の遅延と一覧描画を改善。
現在のhide指示、Issue/PR、checkoutを照合して再開する。前件#351の分担や過去の一時的許可を継承しない。

## 最新の成果

- 同一GDI経路の折返し計測回数を減らし、設定/テーマの共通予約高を一つのcacheで再利用。画面外の一覧行は描画しない。
- Releaseで初回設定541.5→97.2ms、テーマ切替492.9→20.8ms、10,000件一覧235.9→1.52ms。機械/fixture固有の実測。
- Debug/Release build/tidy、折返し15,968件、Help101件、帯geometry95件、設定scroll48件が成功。6枚の一覧比較とdrag/contrast12件で画素差0。
- 判断は[ADR0062](../adr/0062-ui-measurement-and-visible-paint.md)、詳細/限界は[品質記録](../quality/2026-10-25-ui-latency.md)。
- 最終head CI・統合・通常試遊への反映・起動・整理は `D:/NeNeFolio/agents/performance-20261024/completion.json` と#368のPR Checksを照合する。
- 調査/設計/実装/検証は対話席が単体、必要な独立レビューだけ読み取り専用。schema/依存/ゲート変更なし、Waivers: none。

## 次の判断

[#369](https://github.com/hideyukiMORI/nene-folio/issues/369)は大量ノートの初回全文読込中のUI停止。実装未着手。
新規合成データ初読と既読ファイルでI/O時間が大きく異なる。今回の描画改善によるI/O短縮として扱わない。
同期方針を変えるなら先に所有者/取消/最新語の採用/保存済み本文の契約をADRで固定する。
VIEWのテーマ変更ではRichEdit再投入、巨大本文の行番号index再構築も残る。未測定の一般性能を保証しない。

## 運用と保持

通常試遊の正規更新は `tools/prepare-real-machine.ps1`。稼働中アプリ/未保存本文を終了しない。公開Releaseは今回の依頼に含めない。
統合/必要反映/恒久成果物を確認後、追加worktreeと不要出力を整理し、branch/commitを保持する。
前件#366は[品質記録](../quality/2026-10-24-theme-preview.md)、#351は[面の規則](../quality/2026-10-24-surface-rules.md)を参照。

過去の自動承認レビューが再帰削除を拒否した9箇所は再試行/別手段の削除をしない（すべて `D:/NeNeFolio/outputs/` 配下）:
`350-menu-surface`、`350-release-20261024`、`351-native-name`、`351-dialog-feasibility`、`351-surface-rules`、`351-release-20261024`、
`364-theme-cursor`、`theme-navigation-20261024`、`366-theme-preview`。
旧#336/#338/#340/#342/#348/#349の一時物は今回対象外。旧 `check/2026-10-19/349-action-bar` は空状態で固まるため起動しない。
物理IME・実reader・物理DPI移動・DWM窓外影は未測定。
