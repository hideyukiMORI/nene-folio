# いまの状態 — NeNe Folio

記録日: 2026-10-26。v0.2.0をGitHubのβ版として公開済み。直近の修正は本文の通常ホイール配送とnativeスクロール。
再開時は最新のhide指示、Issue / PR、checkoutを照合し、過去の記録を新しい実行許可として扱わない。

## 直近の修正: 本文の通常ホイール配送とnativeスクロール

[#392](https://github.com/hideyukiMORI/nene-folio/issues/392) / [PR #393](https://github.com/hideyukiMORI/nene-folio/pull/393)で、
主窓から本文への通常ホイール配送と、本文クリック後もnativeが動かない生成設定を修正した。
バーの外観は隠したままnativeのスクロール能力を保ち、本文・倍率・focusの所有は維持する。
最終実製品197 checks / 0 failures、native64 checks / 0 failures、配送106 checksは再利用。
根拠と確認範囲は[品質記録](../quality/2026-10-26-view-wheel.md)、再開情報は[引き継ぎ](../handoffs/2026-10-26.md)。
最終PRの必須CI・統合・通常exe反映・整理は実際のPRと`D:/NeNeFolio/agents/view-wheel-20261026/completion.json`を照合する。
公開済みのv0.2.0 ZIPにはこの修正を含めない。物理ホイールによる実スクロールと専用desktopでの合成入力検証を区別する。
途中のGUI操作はhideの停止操作で中止済み。専用テスト窓の稼働参照がある一時出力は保持し、確定状態は完了記録に残す。

## 次の予定: ノートの保存先設定（未実装）

hideの23時台の方針を[#390](https://github.com/hideyukiMORI/nene-folio/issues/390)で文書化した。
インストール版は初回に保存先を確認し、Windowsのドキュメント内`NeNeFolio`を既定とする。
portable版はexe隣の`data`を既定とし、両方とも設定から「ノートの保存先」を変更できるようにする予定。
起動設定の分離、既存ノートを勝手に移動しない切替、書込/ロック確認、未保存本文とアンインストール時のデータ保護を含む。
詳細と次回の設計事項は[引き継ぎの保存先計画](../handoffs/2026-10-25.md#store-data-directory-plan)を参照する。
今回の依頼は引き継ぎと日報への記録まで。保存先設定・MSIX作成・Store申請は未着手で、実装開始はその時点のhide指示と照合する。

## 公開状態

- [v0.2.0 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.2.0)を23:19 JSTに公開。
  Windows x64向けの署名なしportable ZIPとSHA256SUMSを添付し、公開後の再取得一致を確認した。
- tagの対象はclean main `3bc46969564b9422cbc8f49230cb815a3103f704`。
  treeが一致するPR #387最終headの[必須CI](https://github.com/hideyukiMORI/nene-folio/actions/runs/40199179102)は成功。
  今回の公開・README画像・文書更新は[#388](https://github.com/hideyukiMORI/nene-folio/issues/388)へ追跡する。
- [変更履歴と更新手順](../release/0.2.0.md)、[配布・起動・保存・旧版データ・再取得の品質記録](../quality/2026-10-25-beta-release.md)を更新。
  [README](../../README.md)の画面は、実際のv0.2.0配布exeをサンプルノートで起動して撮影したもの。
- β公開では製品コード・版入力・保存schemaを変更していない。公開時点の通常exe
  `C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`は#369統合commit `6a294697c804de15e30e06e0a9507369906a9bf3`
  から10月25日22:31 JSTに更新したもの。その後の#392反映は上記の完了記録で確認する。
- 公開証跡と配布物の恒久コピーは`D:/NeNeFolio/agents/beta-release-20261025/`。
  `published-release.json`、`download-verification.json`、`published-assets/`が確定公開物の根拠。
  文書PRのCI・整理の確定結果は同所の`completion.json`を参照する。作業branch / commitは保持する。
- #369の追加worktree / 一時出力は削除済み。恒久証跡は`D:/NeNeFolio/agents/first-search-20261025/`。
  当日の経緯は[日報](../reports/2026-10-25.md)と[引き継ぎ](../handoffs/2026-10-25.md)を参照する。

## 未確認事項と次の判断

- 物理IME、人によるreader聴取、DWM呈示は今回の配布確認に含めていない。専用desktopの合成入力/描画確認と区別する。
- 初回全ノート検索は準備中も入力・描画を処理するが、一件分の重いI/O / UTF-8検証 / 写し作成と、最後の一致計算は同期処理のまま。
  入力が続くと準備完了が遅れる。条件と実測は[検索品質記録](../quality/2026-10-25-first-search-responsiveness.md)を参照する。
- Microsoft Store公開、署名、インストーラ、自動更新は今回のβ公開に含めていない。次の作業はhideの指示と実際のIssue / PRを起点にする。
- 下記の旧作業物は削除拒否のため保持している。今回も再試行しない。

## 保持中の旧作業物

下記の13箇所は過去の自動承認レビューが削除を拒否したため未削除。再試行や別手段での削除をしない。

- #368: `D:/NeNeFolio/worktrees/368-ui-latency`、`D:/NeNeFolio/outputs/performance-20261024`。
- #373: `D:/NeNeFolio/worktrees/373-gui-responsiveness`、`D:/NeNeFolio/outputs/gui-responsiveness-20261025`。
- `D:/NeNeFolio/outputs/`配下の9箇所:
  `350-menu-surface`、`350-release-20261024`、`351-native-name`、`351-dialog-feasibility`、`351-surface-rules`、
  `351-release-20261024`、`364-theme-cursor`、`theme-navigation-20261024`、`366-theme-preview`。

旧#336/#338/#340/#342/#348/#349の一時物も今回の更新対象外。
旧 `check/2026-10-19/349-action-bar` は空状態で固まるため起動しない。
稼働中アプリや未保存本文を終了しない。今後の実画面操作・追加公開は、その時点のhide指示と照合する。
