# いまの状態 — NeNe Folio

記録日: 2026-10-29。hideの継続品質改善依頼により、設計Astra・実装SOL/LUNAの分担で調査と修正を進める。
この分担は今回の作業に適用し、将来の再開では最新のhide指示・実Issue/PR・checkoutを照合する。
経緯は[日報](../reports/2026-10-29.md)、再開情報は[引き継ぎ](../handoffs/2026-10-29.md)。

## 品質改善の現在位置

リスト内の引用から親本文へ戻る修正[#404 / PR #406](https://github.com/hideyukiMORI/nene-folio/pull/406)は、
最終head `59bab9e`の必須CI `40790608210`が成功し、main `6047bcf`へ統合済み。
初回coverage不足へ境界テストを追加し、全体4612/5078分岐（90.823%）を確認した。閾値・除外は不変。
設計は[ADR0071](../adr/0071-ordered-markdown-containers.md)、対象結果は[品質記録](../quality/2026-10-29-markdown-containers.md)。
通常exeへの反映と作業物整理は後続修正の統合後に行う。確定値は`D:/NeNeFolio/agents/404-markdown-containers/completion.json`を照合する。

現在は[#405](https://github.com/hideyukiMORI/nene-folio/issues/405)の表の等幅配分と、
[#407](https://github.com/hideyukiMORI/nene-folio/issues/407)の確保失敗時nonnull契約を進める。
#405は実native対象79 checks/0 failures、修正前の不均等幅も再現済み。#404統合後のmainを取り込み、最終head必須CIへ進む。
#407は7factoryのnullable確認と測定sanitizerの統一を実装し、対象失敗注入とtool正負例が成功。完成記録を確認中。

CR改行の[#402 / PR #403](https://github.com/hideyukiMORI/nene-folio/pull/403)は最終head必須CI成功後、main `5c1240e`へ統合し通常exeへ反映済み。
確認範囲は[品質記録](../quality/2026-10-29-markdown-line-endings.md)、確定値は
`D:/NeNeFolio/agents/402-markdown-line-endings/completion.json`。作業物の削除は自動承認レビューが拒否し、下記2箇所を保持する。

検索は既存計測を照合し、ファイルopen待ちと一致計算のCPU時間を分けて調査した。
1文字検索の3候補は非製品の正確性診断まで完了。初回計時は他案件buildと競合したため不採用とし、負荷が続く間は再計時しない。
idle一致計算や背景I/Oの設計案は比較段階であり、製品採用はしていない。
調査・診断原本は `D:/NeNeFolio/agents/quality-20261029/`。
引用内表のnative字下げ/残幅・狭幅可読性は、#404の所属修正とは別課題として残す。
通常段落の折返しを変える水平bar案等は採用していない。診断と残る範囲は日報・引き継ぎを参照する。

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
現在の通常`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`はsource `5c1240e`・表示版0.3.0/Windows版0.3.0.0。
通常process0件を確認してから正規ツールで反映しexit0だった。10月29日07:24:08 JST更新済みで、更新後は起動していない。
版・hashは#402の`completion.json`。通常dataは操作していない。

## 次の予定: ノートの保存先設定（未実装）

hideの23時台の方針を[#390](https://github.com/hideyukiMORI/nene-folio/issues/390)で文書化した。
インストール版は初回に保存先を確認し、Windowsのドキュメント内`NeNeFolio`を既定とする。
portable版はexe隣の`data`を既定とし、両方とも設定から「ノートの保存先」を変更できるようにする予定。
起動設定の分離、既存ノートを勝手に移動しない切替、書込/ロック確認、未保存本文とアンインストール時のデータ保護を含む。
詳細と次回の設計事項は[引き継ぎの保存先計画](../handoffs/2026-10-25.md#store-data-directory-plan)を参照する。
保存先設定・MSIX作成・Store申請は未着手。今回の品質改善では、まず既存機能の不具合と応答性を優先する。

## 継続する確認範囲

- 表は63列まで、container内のnative表字下げと極狭幅可読性は未解消。リンクは装飾のみ、HTML/画像等は対応外。
- 大量ノートの初回検索には同期処理と準備待ちが残る。[検索品質記録](../quality/2026-10-25-first-search-responsiveness.md)を参照する。
- 専用desktopの合成入力/描画と、物理IME・実reader聴取・DWM実画面呈示を区別する。
- 署名・インストーラ・Store・自動更新は未実装/対象外。日付付き事実は[日報](../reports/2026-10-27.md)と[引き継ぎ](../handoffs/2026-10-27.md)。
- 下記の旧削除拒否済み作業物は再試行しない。

## 保持中の旧作業物

下記の16箇所は自動承認レビューが削除を拒否したため未削除。再試行や別手段での削除をしない。

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
