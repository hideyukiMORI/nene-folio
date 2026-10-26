# いまの状態 — NeNe Folio

記録日: 2026-10-27。[v0.3.0 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.3.0)は01:14:32 JSTに公開済み。
配布・専用smoke・再取得照合まで成功した。[変更履歴](../release/0.3.0.md)、
[Markdownの範囲](../MARKDOWN.md)、[品質記録](../quality/2026-10-27-beta-release.md)を参照する。
公開結果と再開情報は、hideの「日報と引き継ぎ書を書いて」に基づく[#400](https://github.com/hideyukiMORI/nene-folio/issues/400)で記録した。
再開時は最新のhide指示・Issue/PR・checkoutを照合し、過去の記録を新しい実行許可として扱わない。

## 完了した公開と残る文書確定

[#398](https://github.com/hideyukiMORI/nene-folio/issues/398) /
[PR #399](https://github.com/hideyukiMORI/nene-folio/pull/399)は最終head `f50a25b24b2438c107ae96b18c654ea628d5febf`の
[必須CI](https://github.com/hideyukiMORI/nene-folio/actions/runs/40354478601)に成功し、
main `a0df9932620e3575d692c71134465eb704ec5e01`へ統合した。
clean mainから正規配布器で0.3.0を生成し、Pre-releaseへZIP/SHA256SUMSを公開した。
再取得bytes/hashとZIP内11 membersが一致した。旧v0.2.0 Releaseは維持している。

専用fresh/legacy/upgradeで起動・設定版・日本語合成ノート・旧版生成data 5ファイルの更新保持を確認した。
upgradeの設定画像はmodalが映っておらず版表示証拠へ使わず、同じexeのfresh成功とhashを再利用した。
#392/#394/#396の機能結果とhideの通常サンプル実機受理は入力不変で再利用し、保存schema・設定・依存・gate・配布仕様は変えていない。

#398はarchive200ファイルの全hash照合と安全監査を経て、01:18:08 JSTに当該worktree/4出力の整理までexit0で完了した。
branch/commitと恒久証跡は保持し、`D:/NeNeFolio/agents/beta-release-20261027/completion.json`の`release-complete`へ記録した。
#400自身の最終CI・統合・整理の確定値は、実Issue/PRと同所の`docs-completion.json`を照合する。
#400で使用した文書作業木は`D:/NeNeFolio/worktrees/400-beta-handoff`。成果物保全・安全監査後の整理結果も同記録へ分け、branch/commitを保持する。

## 通常exeとhideの実機確認

hide受理済み合成サンプル`data/サンプル/Markdown構文サンプル.md`は通常dataへ残している。実ノート内容は記録・同梱しない。
現在の通常`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`はsource `a0df993`・表示版0.3.0/Windows版0.3.0.0。
通常process0件を確認してから正規ツールで反映しexit0だった。01:16:51 JST更新済みで、更新後は起動していない。
`normal-build-verification.json`に通常exeの版・hashと、サンプルhashの不変を記録した。通常dataは操作していない。

## 次の予定: ノートの保存先設定（未実装）

hideの23時台の方針を[#390](https://github.com/hideyukiMORI/nene-folio/issues/390)で文書化した。
インストール版は初回に保存先を確認し、Windowsのドキュメント内`NeNeFolio`を既定とする。
portable版はexe隣の`data`を既定とし、両方とも設定から「ノートの保存先」を変更できるようにする予定。
起動設定の分離、既存ノートを勝手に移動しない切替、書込/ロック確認、未保存本文とアンインストール時のデータ保護を含む。
詳細と次回の設計事項は[引き継ぎの保存先計画](../handoffs/2026-10-25.md#store-data-directory-plan)を参照する。
今回の依頼は引き継ぎと日報への記録まで。保存先設定・MSIX作成・Store申請は未着手で、実装開始はその時点のhide指示と照合する。

## 継続する確認範囲

- list内quoteから同じ親list継続への復帰は未保証、表は63列まで、リンクは装飾のみ。HTML/画像等は対応外。
- 大量ノートの初回検索には同期処理と準備待ちが残る。[検索品質記録](../quality/2026-10-25-first-search-responsiveness.md)を参照する。
- 専用desktopの合成入力/描画と、物理IME・実reader聴取・DWM実画面呈示を区別する。
- 署名・インストーラ・Store・自動更新は未実装/対象外。日付付き事実は[日報](../reports/2026-10-27.md)と[引き継ぎ](../handoffs/2026-10-27.md)。
- 下記の旧削除拒否済み作業物は再試行しない。

## 保持中の旧作業物

下記の14箇所は自動承認レビューが削除を拒否したため未削除。再試行や別手段での削除をしない。

- #392: `D:/NeNeFolio/outputs/view-wheel-20261026`（残るのは`runtime`。窓終了後の22ファイルも保管・照合済みだが、削除要求は`blocked by policy`で拒否）。
- #368: `D:/NeNeFolio/worktrees/368-ui-latency`、`D:/NeNeFolio/outputs/performance-20261024`。
- #373: `D:/NeNeFolio/worktrees/373-gui-responsiveness`、`D:/NeNeFolio/outputs/gui-responsiveness-20261025`。
- `D:/NeNeFolio/outputs/`配下の9箇所:
  `350-menu-surface`、`350-release-20261024`、`351-native-name`、`351-dialog-feasibility`、`351-surface-rules`、
  `351-release-20261024`、`364-theme-cursor`、`theme-navigation-20261024`、`366-theme-preview`。

旧#336/#338/#340/#342/#348/#349の一時物も今回の更新対象外。
旧 `check/2026-10-19/349-action-bar` は空状態で固まるため起動しない。
稼働中アプリや未保存本文を終了しない。今後の実画面操作・追加公開は、その時点のhide指示と照合する。
