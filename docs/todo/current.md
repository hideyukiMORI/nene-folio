# いまの状態 — NeNe Folio

記録日: 2026-10-30。06:52 JST頃のhideの依頼で品質改善を再開した。
この継続作業ではAstraの設計サナが設計・受理、SOL/LUNAの実装サナが調査・実装・検証を分担し、停止指示まで進める。
今回の明示分担を他案件・次の作業へ自動継承しない。00時台の停止記録は当時の区切りとして保持する。
最新は[日報](../reports/2026-10-30.md)・[引き継ぎ](../handoffs/2026-10-30.md)、
10月29日の完成成果は[23件の一覧](../reports/2026-10-29.md#一日の成果一覧2026-10-30の終了整理で確定)。

## 品質改善の現在位置

[#460](https://github.com/hideyukiMORI/nene-folio/issues/460)の共通UTF-8 decoder内ASCII早期returnをmain`3dbed2a`へ統合し、通常反映・保全・整理を完了した。
PR #461の最終head`12b4971`・必須CI`41026891563`成功、Debug sanitizer/tidyと限定text/NUL/size境界も成功。
公開API・保存形式と全8callsiteの契約を維持し、実Release objectのASCII直接returnを確認した。
固定Bは検索63.0590→49.3964ms、Large検証32.3232→18.7385ms、最長continue43.3406→30.4207ms。
13契約不一致0。一回の計装付きASCII標本であり一般速度を保証せず、一件の同期準備約30.42msは残る。
7612files全hash保全後にworktree/outputを削除し、branch/commit・失敗原本を保持した。
詳細は[品質記録](../quality/2026-10-30-ascii-decode.md)、確定原本は同taskの`integration-completion.json`。

現在の焦点は[#464](https://github.com/hideyukiMORI/nene-folio/issues/464)の本文検証の出力変数再利用である。
#460後もLarge検証18.7385msが残り、計装付きRelease objectで毎scalarのzero storeを確認した。
private validateの初期化をloop直前へ移す一箇所だけを採り、decoder一経路・UTF8→NUL→sizeの優先を保つ。
正典Debug/既存text-validationとReleaseを確認し、未計装・計装両objectでloop内zero storeなしを受理した。
固定Bは検証18.7385→18.0793ms、検索49.3964→47.9622ms、一件準備30.4197→29.9140ms。
13契約不一致0、Observe/同raw Auditの実0/error0・所有0と全記録を受理した。
単発値で改善幅を確定せず、同期準備約29.91msは残る。最終CI・統合・通常反映・保全整理へ進む。
詳細は[品質記録](../quality/2026-10-30-validation-output.md)。

#462の元SET拒否診断は、PR #463のhead`79ae8cb`・必須CI`41036377740`成功後、main`d137e212`へ文書統合した。
通常exe hash維持、3937files/1609813447bytes全hash保全、worktree削除済み。出力削除は実行前に拒否され50箇所目として保持する。
確定原本は同taskの`integration-completion.json`、限定結果は[品質記録](../quality/2026-10-30-table-layout-failure.md)。

#456ではroot3列の左右到達・wrap・本文状態/実RedoUndoの29契約が一致したが、63列2条件は配置失敗で横到達未完。
通常段落も横へ動くため、製品方式・最小列幅は未採用。既存結果は[診断記録](../quality/2026-10-30-table-horizontal-feasibility.md)へ保つ。
#458の初回検索と準備内訳は[計測記録](../quality/2026-10-30-search-preparation-profile.md)へ、
#448の現在ノート検索・#451の縁resize・#452の窓操作を含む完成履歴は[日報](../reports/2026-10-30.md)へ集約した。

本文/置換の追加静読は新具体的不具合0。置換4Mの生成片/完成本文の差を新規不具合とせず、既存のedit資源上限を維持した。
背景読込・極狭幅表は未解決で、#458/#460の限定実測から実UIの一般保証へ広げない。
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
現在の通常`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`はsource `3dbed2a`・表示版0.3.0/Windows版0.3.0.0。
通常process0件を確認して正規ツールで反映しexit0、10月30日17:38:56 JST更新済み。更新後は起動していない。
版・hashは`D:/NeNeFolio/agents/460-ascii-decode/normal-preservation.json`。通常dataは操作していない。

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

下記の50箇所は自動承認レビューの削除拒否により保持する。再試行や別手段での削除をしない。

- #462: `D:/NeNeFolio/outputs/462-table-layout-failure`。文書統合・通常exe hash維持・3937files全hash保全と安全監査後、出力削除要求が実行前に`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定原本は同taskの`integration-completion.json`。

- #458: `D:/NeNeFolio/outputs/458-search-preparation-profile`。文書統合・通常exe hash維持・12932files全hash保全と安全監査後、出力削除要求が実行前に`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定原本は同taskの`integration-completion.json`。

- #456: `D:/NeNeFolio/outputs/456-table-horizontal-feasibility`。文書統合・通常exe hash維持・4107files全hash保全と安全監査後、出力削除要求が実行前に`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定原本は同taskの`integration-completion.json`。

- #451: `D:/NeNeFolio/outputs/451-window-edge`。統合・通常反映・5536filesの全hash保全と安全監査後、出力削除要求が実行前に`blocked by policy`で拒否された。追加理由なし。worktreeは削除済み、branch/commit保持。確定原本は同taskの`integration-completion.json`。

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
