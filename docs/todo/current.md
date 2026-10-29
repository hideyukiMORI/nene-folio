# いまの状態 — NeNe Folio

記録日: 2026-10-30。06:52 JST頃のhideの依頼で品質改善を再開した。
この継続作業ではAstraの設計サナが設計・受理、SOL/LUNAの実装サナが調査・実装・検証を分担し、停止指示まで進める。
今回の明示分担を他案件・次の作業へ自動継承しない。00時台の停止記録は当時の区切りとして保持する。
最新は[日報](../reports/2026-10-30.md)・[引き継ぎ](../handoffs/2026-10-30.md)、
10月29日の完成成果は[23件の一覧](../reports/2026-10-29.md#一日の成果一覧2026-10-30の終了整理で確定)。

## 品質改善の現在位置

[#448](https://github.com/hideyukiMORI/nene-folio/issues/448) / [PR #453](https://github.com/hideyukiMORI/nene-folio/pull/453)の
現在ノート検索の無確保線形化を、最終head`47c1333`・必須CI`40968520980`成功後、main`6f20dcc`へ統合した。
固定36callは結果一致・`2N+5m`以内。Debug/tidy・限定search・比較計数・symbols・最終conformanceが成功した。
通常exe反映、9051filesの全hash保全、追加worktree削除まで完了。出力削除拒否は下記へ保持する。
詳細は[品質記録](../quality/2026-10-30-linear-note-search.md)、確定原本は`agents/448-linear-note-search/integration-completion.json`。
比較量の改善であり、実時間や実UI応答改善の測定ではない。#446以前の完成履歴は日報を参照する。

[#451](https://github.com/hideyukiMORI/nene-folio/issues/451)はdrawer/backdropが窓縁を遮る経路を修正中。
旧baselineは73checks/1failure・既知遮断8点、候補は73checks/0fail・8点とも子から親のedgeへ到達した。
最大化中の親HTLEFT誤判定も修復。合成操作から実nativeリサイズ開始/一回取消まで16checks/0fail、親の原本監査も受理済み。
物理dragと実寸法変更は未確認、必須CI・統合は未完了。[品質記録](../quality/2026-10-30-window-edge-routing.md)。

[#452](https://github.com/hideyukiMORI/nene-folio/issues/452)は最小化/最大化/復元の常設入口を設計した。
狭幅では表示3択だけを既存MENUへまとめ、shell操作は常時可視にする。デザインリナの提案と独立静読を受理し、
一覧中のOS最大化から退出するplacementの2経路を静的fixtureで準備中。製品編集/実行はまだ0。

本文/置換の追加静読は新具体的不具合0。置換4Mの生成片/完成本文の差を新規不具合とせず、既存のedit資源上限を維持した。
背景読込・極狭幅表は未解決で、新しい性能値や実UIの一般保証はない。
文書収載#449の統合・通常exe hash維持・安全整理の確定原本は`agents/day-close-20261030/integration-completion.json`。
Waivers none。

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
現在の通常`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`はsource `6f20dcc`・表示版0.3.0/Windows版0.3.0.0。
通常process0件を確認して正規ツールで反映しexit0、10月30日07:55:57 JST更新済み。更新後は起動していない。
版・hashは`D:/NeNeFolio/agents/448-linear-note-search/normal-build-verification.json`。通常dataは操作していない。

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

下記の46箇所は自動承認レビューの削除拒否により保持する。再試行や別手段での削除をしない。

- #448: `D:/NeNeFolio/outputs/448-linear-note-search`。統合・通常反映・9051filesの全hash保全と安全監査後、出力削除要求が実行前に`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定原本は同taskの`integration-completion.json`。

- #446: `D:/NeNeFolio/outputs/446-linear-index-matching`。統合・通常反映・6593filesの全hash保全と安全監査後、出力削除要求が実行前に`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定値は同taskの`integration-completion.json`。

- #444: `D:/NeNeFolio/outputs/444-bounded-search-diagnostic`。文書統合・通常exe hash維持・2753ファイル全hash保全と安全監査後、出力削除要求が実行前に`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定原本は同taskの`integration-completion.json`。

- #442: `D:/NeNeFolio/outputs/442-inline-spaces`。統合・通常反映・6818ファイル全hash保全と安全監査後、出力削除要求が実行前に`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定値は同taskの`integration-completion.json`。

- #440: `D:/NeNeFolio/outputs/440-inline-storage`。統合・通常反映・6180ファイル全hash保全と安全監査後、出力削除要求が実行前に`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定原本は同taskの`integration-completion.json`。

- #438: `D:/NeNeFolio/outputs/438-read-sharing-feasibility`。文書統合・通常exe hash保持・2632ファイル全hash保全と安全監査後、出力削除要求が実行前に`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。自作ごみ箱の1組は別途保持してpurgeしない。確定値は同taskの`integration-completion.json`。

- #436: `D:/NeNeFolio/outputs/436-quality-checkpoint`。文書だけの作業で作った空directoryの非再帰削除要求が、実行前に`blocked by policy`で拒否された。追加理由なし。空のまま保持し、検証記録はagentsへ直接残す。確定原本は同taskの`empty-output-cleanup.json`。

- #434: `D:/NeNeFolio/outputs/434-drawer-scroll-delta`。統合・通常反映・2589ファイル全hash保全と安全監査後、出力削除要求が`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定値は同taskの`integration-completion.json`。

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
