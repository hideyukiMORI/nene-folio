# いまの状態 — NeNe Folio

記録日: 2026-10-27。hideの実機サンプル受理と「ベータ版更新しておいて」に基づき、
[#398](https://github.com/hideyukiMORI/nene-folio/issues/398)でv0.3.0 betaの更新を準備している。
統合・配布生成・公開・再取得・整理の確定値は、実際のIssue/PR/Releaseと
`D:/NeNeFolio/agents/beta-release-20261027/completion.json`を照合する。ここでは公開実行済みとは扱わない。
再開時は最新のhide指示とcheckoutも照合し、過去の記録を新しい実行許可として扱わない。

## 今回の焦点: v0.3.0 beta

唯一のruntime版入力`CMakeLists.txt`のproject VERSIONを0.3.0へ更新する。
[変更履歴と更新手順](../release/0.3.0.md)、[Markdownの対応範囲](../MARKDOWN.md)、
[配布確認の計画と結果](../quality/2026-10-27-beta-release.md)を参照する。
最終PR headの必須CI後、clean mainから正規配布器でRelease ZIPを生成し、専用の展開先で起動・版表示・
合成データの更新を確認する。tag/Release公開と再取得照合、成果物保全・安全整理は設計サナが担当する。
公開済みv0.2.0のReleaseと旧release文書は置き換えない。署名・インストーラ・Store申請・自動更新は対象外。

v0.3.0は#392のホイール修正、#394の表示密度/小見出し/コード面、#396の基本構文とnative表を含む。
既存機能の入力不変な成功結果は[ホイール](../quality/2026-10-26-view-wheel.md)、
[表示調整](../quality/2026-10-26-markdown-style.md)、[構文](../quality/2026-10-26-markdown-syntax.md)から再利用する。
#396は[PR #397](https://github.com/hideyukiMORI/nene-folio/pull/397)をmain `063f5bab`へ統合し、必須CIと通常exe反映・
恒久保管・追加worktree/出力整理を完了した。確定値は`D:/NeNeFolio/agents/markdown-syntax-20261026/completion.json`。
list内quoteから親list継続へ戻る組合せ、表63列、装飾リンク等の制限は利用者向け文書に明記している。

hide確認用の合成サンプルを通常`data/サンプル/Markdown構文サンプル.md`へ作成し、hideが表示を受理した。
通常アプリPID56180は稼働中として引き継ぎ、操作・終了・exe上書きをしない。通常dataを配布物に含めない。
保存schema・設定・gate・packaging仕様は今回変更しない。現在の配布手順は[workflow§9](../DEVELOPMENT_WORKFLOW.md#9-リリース)と
[ADR0045](../adr/0045-single-version-and-verified-release-bundle.md)。日付付き経緯は[日報](../reports/2026-10-27.md)と[引き継ぎ](../handoffs/2026-10-27.md)。

## 次の予定: ノートの保存先設定（未実装）

hideの23時台の方針を[#390](https://github.com/hideyukiMORI/nene-folio/issues/390)で文書化した。
インストール版は初回に保存先を確認し、Windowsのドキュメント内`NeNeFolio`を既定とする。
portable版はexe隣の`data`を既定とし、両方とも設定から「ノートの保存先」を変更できるようにする予定。
起動設定の分離、既存ノートを勝手に移動しない切替、書込/ロック確認、未保存本文とアンインストール時のデータ保護を含む。
詳細と次回の設計事項は[引き継ぎの保存先計画](../handoffs/2026-10-25.md#store-data-directory-plan)を参照する。
今回の依頼は引き継ぎと日報への記録まで。保存先設定・MSIX作成・Store申請は未着手で、実装開始はその時点のhide指示と照合する。

## 継続する確認範囲

- 大量ノートの初回検索には同期処理と準備待ちが残る。[検索品質記録](../quality/2026-10-25-first-search-responsiveness.md)を参照する。
- 専用desktopの合成入力/描画確認と、物理IME・人によるreader聴取・DWM実画面呈示は区別する。
- 今回の作業木は`D:/NeNeFolio/worktrees/398-beta-030`。恒久証跡は`D:/NeNeFolio/agents/beta-release-20261027/`。
  設計サナが統合・必要反映・収載後に稼働参照/未保存物/唯一成果物/リンクを監査して整理する。branch/commitは保持する。
- 下記の削除拒否済み作業物は今回も再試行しない。

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
