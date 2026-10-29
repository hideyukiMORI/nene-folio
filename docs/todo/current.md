# いまの状態 — NeNe Folio

記録日: 2026-10-29。hideの継続品質改善依頼により、設計Astra・実装SOL/LUNAの分担で調査と修正を進める。
この分担は今回の作業に適用し、将来の再開では最新のhide指示・実Issue/PR・checkoutを照合する。
経緯は[日報](../reports/2026-10-29.md)、再開情報は[引き継ぎ](../handoffs/2026-10-29.md)。

## 品質改善の現在位置

索引スクロールの加算境界 [#434](https://github.com/hideyukiMORI/nene-folio/issues/434)は対象検証済みの完成候補。
正常な小さい索引で有効位置15へINT_MAXを加えた旧sourceのUBSan診断を確認した。
通常wheelの自然到達とは区別する。旧redの250指定入力と原本のhash一致を親が受理した。
既存scroll意図一箇所で移動量を先に許容範囲へ丸め、現在の有効量・0〜上限・失敗時状態保持を維持する。
親が設計/状態文書/統合、SOLがapplication/test/品質記録を担当する。新API/確保/Win32/schemaは無い。
作業木は`D:/NeNeFolio/worktrees/434-drawer-scroll-delta`、原本/出力はD側agents/outputsの同名領域。
完成候補のDebug build/tidyと同selectorは子/wrapper0で成功し、503指定入力/3生成物の現物・snapshot全hash一致を親が確認した。
成功対象は再実行しない。[品質記録](../quality/2026-10-29-drawer-scroll-delta.md)と受理設計は同taskに保持する。
最終整形・File API規約検査・差分検査も成功した。最終head CI・統合/通常反映/整理の確定値は実PRとintegration-completionを照合する。

対象記録の入力一覧 #432 / [PR #433](https://github.com/hideyukiMORI/nene-folio/pull/433)は、
最終head `b418a77`のCI `40832797981`でfull-gate/check成功後、main `0803ff5`へ統合した。
対象tool15件と規約検査が成功し、CIの既存discoveryでも112件が成功した。製品無変更で通常exeのhashを維持した。
2361ファイル全hash保全とworktree削除まで完了。出力削除は拒否されたため保持する。
[品質記録](../quality/2026-10-29-run-record-input-list.md)、確定値は`D:/NeNeFolio/agents/432-run-record-input-list/integration-completion.json`。

以前の完了履歴は[日報](../reports/2026-10-29.md)・[引き継ぎ](../handoffs/2026-10-29.md)と各品質記録へ置く。
検索のmemchr候補は未採用。直近12:53の負荷監査も他案件compile/test稼働で計時せず、以後の新計時は0件。
#428でmatcher構造が変わったため、旧候補の結果を現在版の改善率へ読み替えない。
旧計測/設計原本は`D:/NeNeFolio/agents/quality-20261029/`。背景I/Oと極狭幅表の可読性は未解消。

## 公開済みbeta

[v0.3.0 Beta](https://github.com/hideyukiMORI/nene-folio/releases/tag/v0.3.0)は2026-10-27 01:14:32 JST公開済み。
配布・専用smoke・再取得照合まで成功した。[変更履歴](../release/0.3.0.md)、
[Markdownの範囲](../MARKDOWN.md)、[品質記録](../quality/2026-10-27-beta-release.md)を参照する。
#398 / PR #399に続く文書#400 / [PR #401](https://github.com/hideyukiMORI/nene-folio/pull/401)も
最終head `499984d135ae6b27e13a8276b33f66ee706aac57`の必須CI成功後、main `1845f9f`へ統合・整理済みと照合した。
確定値は `D:/NeNeFolio/agents/beta-release-20261027/docs-completion.json`。
公開済み成果物を今回の調査のために再生成していない。
## 通常exeとhideの実機確認

hide受理済み合成サンプル`data/サンプル/Markdown構文サンプル.md`は通常dataへ残している。実ノート内容は記録・同梱しない。
現在の通常`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`はsource `e2da1ab`・表示版0.3.0/Windows版0.3.0.0。
通常process0件を確認してから正規ツールで反映しexit0だった。10月29日15:24:07 JST更新済みで、更新後は起動していない。
版・hashは`D:/NeNeFolio/agents/430-task-marker-tab/normal-build-verification.json`。通常dataは操作していない。

## 次の予定: ノートの保存先設定（未実装）

hideの23時台の方針を[#390](https://github.com/hideyukiMORI/nene-folio/issues/390)で文書化した。
インストール版は初回に保存先を確認し、Windowsのドキュメント内`NeNeFolio`を既定とする。
portable版はexe隣の`data`を既定とし、両方とも設定から「ノートの保存先」を変更できるようにする予定。
起動設定の分離、既存ノートを勝手に移動しない切替、書込/ロック確認、未保存本文とアンインストール時のデータ保護を含む。
詳細と次回の設計事項は[引き継ぎの保存先計画](../handoffs/2026-10-25.md#store-data-directory-plan)を参照する。
保存先設定・MSIX作成・Store申請は未着手。今回の品質改善では、まず既存機能の不具合と応答性を優先する。

## 継続する確認範囲

- 表は63列/字下げ32767twipまで、極狭幅可読性は未解消。リンクは装飾のみ、HTML/画像等は対応外。
- 大量ノートの初回検索には同期処理と準備待ちが残る。[検索品質記録](../quality/2026-10-25-first-search-responsiveness.md)を参照する。
- 専用desktopの合成入力/描画と、物理IME・実reader聴取・DWM実画面呈示を区別する。
- 署名・インストーラ・Store・自動更新は未実装/対象外。日付付き事実は[日報](../reports/2026-10-27.md)と[引き継ぎ](../handoffs/2026-10-27.md)。
- 下記の削除拒否済み作業物は再試行しない。

## 保持中の旧作業物

下記の38箇所は自動承認レビューの削除拒否により保持する。再試行や別手段での削除をしない。

- #432: `D:/NeNeFolio/outputs/432-run-record-input-list`。統合・通常exe hash維持・2361ファイル全hash保全と安全監査後、出力削除要求が`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定値は同taskの`integration-completion.json`。

- #430: `D:/NeNeFolio/outputs/430-task-marker-tab`。統合・通常反映・2319ファイル全hash保全と安全監査後、出力削除要求が`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定値は同taskの`integration-completion.json`。

- #428: `D:/NeNeFolio/outputs/428-bounded-index-matching`。統合・通常反映・2564ファイル全hash保全と安全監査後、出力削除要求が`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定値は同taskの`integration-completion.json`。

- #426: `D:/NeNeFolio/outputs/426-table-start-priority`。統合・通常反映・1243ファイル全hash保全と安全監査後、出力削除要求が`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定値は同taskの`integration-completion.json`。

- #424: `D:/NeNeFolio/outputs/424-run-record`。統合・103ファイル全hash保全と安全監査後、出力削除要求が`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commitは保持。確定値は同taskの`integration-completion.json`。

- #422: `D:/NeNeFolio/outputs/422-image-literal`。統合・通常反映・112ファイル全hash保全と安全監査後、出力削除要求が`blocked by policy`で拒否された。追加理由なし。追加worktreeは削除済み、branch/commitは保持。確定値は同taskの`integration-completion.json`。

- #420: `D:/NeNeFolio/outputs/420-link-destination-scope`。統合・通常反映・112ファイル全hash保全と安全監査後、出力削除要求が`blocked by policy`で拒否された。追加理由なし。追加worktreeの削除は成功しbranch/commitを保持。確定値は同taskの`integration-completion.json`。

- #418: `D:/NeNeFolio/outputs/418-index-filter-size`。統合・通常反映・278ファイル全hash保全と安全監査後、出力削除要求が`blocked by policy`で拒否された。追加理由なし。追加worktreeの削除は成功しbranch/commitを保持。確定値は同taskの`integration-completion.json`。

- #416: `D:/NeNeFolio/worktrees/416-escaped-image-link`と`D:/NeNeFolio/outputs/416-escaped-image-link`（2箇所）。作業木内部の`eng/__pycache__`/pyc削除と、統合・通常反映・111ファイル全hash保全・安全監査後の出力削除が`blocked by policy`で拒否された。追加理由なし。含む作業木の削除も行わない。確定値は同taskの`integration-completion.json`。

- #410/#411と境界診断: `D:/NeNeFolio/worktrees/`と`D:/NeNeFolio/outputs/`それぞれの`410-table-container-indent`、`411-replacement-size`、および`D:/NeNeFolio/outputs/table-indent-boundary-20261029`（5箇所）。統合・通常反映・420ファイルのarchive全hash照合・未保存/ignored/link/稼働参照監査後、削除要求全体が実行前`blocked by policy`で拒否された。追加理由なし。
- #404/#405/#407: `D:/NeNeFolio/worktrees/`と`D:/NeNeFolio/outputs/`それぞれの`404-markdown-containers`、`405-equal-table-columns`、`407-nullable-allocation`（6箇所）。統合・通常反映・archive全hash照合・未保存/ignored/link/稼働参照監査後、削除要求全体が実行前`blocked by policy`で拒否された。追加理由なし。
- 非製品表診断: `D:/NeNeFolio/outputs/table-indent-20261029`。40ファイルのarchive/hash照合後、削除要求が実行前`blocked by policy`で拒否。追加理由なし。

- #402: `D:/NeNeFolio/worktrees/402-markdown-line-endings`、`D:/NeNeFolio/outputs/402-markdown-line-endings`。統合・通常反映・98ファイルのarchive/hash照合後、削除要求が実行開始前に`blocked by policy`で拒否。追加理由は返っていない。
- #392: `D:/NeNeFolio/outputs/view-wheel-20261026`（残るのは`runtime`。窓終了後の22ファイルも保管・照合済みだが、削除要求は`blocked by policy`で拒否）。
- #368: `D:/NeNeFolio/worktrees/368-ui-latency`、`D:/NeNeFolio/outputs/performance-20261024`。
- #373: `D:/NeNeFolio/worktrees/373-gui-responsiveness`、`D:/NeNeFolio/outputs/gui-responsiveness-20261025`。
- `D:/NeNeFolio/outputs/`配下の9箇所:
  `350-menu-surface`、`350-release-20261024`、`351-native-name`、`351-dialog-feasibility`、`351-surface-rules`、
  `351-release-20261024`、`364-theme-cursor`、`theme-navigation-20261024`、`366-theme-preview`。

旧#336/#338/#340/#342/#348/#349の一時物も今回の更新対象外。
旧 `check/2026-10-19/349-action-bar` は空状態で固まるため起動しない。
稼働中アプリや未保存本文を終了しない。今後の実画面操作・追加公開は、その時点のhide指示と照合する。
