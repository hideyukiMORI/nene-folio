# いまの状態 — NeNe Folio

記録日: 2026-10-29。hideの継続品質改善依頼により、設計Astra・実装SOL/LUNAの分担で調査と修正を進める。
この分担は今回の作業に適用し、将来の再開では最新のhide指示・実Issue/PR・checkoutを照合する。
経緯は[日報](../reports/2026-10-29.md)、再開情報は[引き継ぎ](../handoffs/2026-10-29.md)。

## 品質改善の現在位置

対象記録の入力一覧 [#432](https://github.com/hideyukiMORI/nene-folio/issues/432)は対象検証済みの完成候補。
#430で739入力/62518文字の外側argvが起動前に失敗したため、既存record-runへ入力一覧を渡す入口を受理した。
解析した一覧bytesと記録snapshotを結び、既存の実行・記録・終了値・上書き拒否を共用する。
親が設計/規約/状態文書/統合、LUNAがtool/test/品質記録と対象検証を担当する。
SOLの読取レビュー後、実装開始時のモデル混雑でLUNAへ交代した。設計と対象範囲は維持する。
作業木は`D:/NeNeFolio/worktrees/432-run-record-input-list`、原本/出力はD側agents/outputsの同名領域。
最初の準備は誤った一覧のJSON文字列を子起動前に拒否した。正しい配列で実行した対象tool15件は子/wrapper0で成功した。
親が7指定入力の現物/snapshot全hash一致を確認し、再実行していない。
[品質記録](../quality/2026-10-29-run-record-input-list.md)と同taskの記録に結果と限界を残す。
製品変更と通常exe再buildは対象外。最終head CI/統合/整理の確定値は実PRとintegration-completionを照合する。

チェックリスト印の後続tab #430 / [PR #431](https://github.com/hideyukiMORI/nene-folio/pull/431)は、
最終head `805433f`のCI `40828357919`でfull-gate/check成功後、main `e2da1ab`へ統合した。
旧redと完成候補Debug build/tidy・共用task selectorを確認し、15:24:07 JSTに通常exeへ反映した。
2319ファイル全hash保全とworktree削除まで完了。出力削除は拒否されたため保持する。
[品質記録](../quality/2026-10-29-task-marker-tab.md)、確定値は`D:/NeNeFolio/agents/430-task-marker-tab/integration-completion.json`。

検索の一致計算分割#428 / [PR #429](https://github.com/hideyukiMORI/nene-folio/pull/429)は、
最終head `d71540b`のCI `40825100739`でfull-gate/check成功後、main `ac4b1ad`へ統合した。
[ADR0073](../adr/0073-bounded-index-matching.md)の一つのmatcherをidle65536比較workと確定集合の同期修復で共用する。
unit/OOMと実loopの通常message/clear/close/native modalの4ケースを確認し、14:52:06 JSTに通常exeへ反映した。
2564ファイルを全hash保全、worktree削除済み。出力削除は拒否されたため保持する。
[品質記録](../quality/2026-10-29-bounded-index-matching.md)、確定値は`D:/NeNeFolio/agents/428-bounded-index-matching/integration-completion.json`。
同期layout・OS read・初期集計/確保等は残り、実時間上限や速度改善率は主張しない。

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

下記の37箇所は自動承認レビューの削除拒否により保持する。再試行や別手段での削除をしない。

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
