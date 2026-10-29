# いまの状態 — NeNe Folio

記録日: 2026-10-29。hideの継続品質改善依頼により、設計Astra・実装SOL/LUNAの分担で調査と修正を進める。
この分担は今回の作業に適用し、将来の再開では最新のhide指示・実Issue/PR・checkoutを照合する。
経緯は[日報](../reports/2026-10-29.md)、再開情報は[引き継ぎ](../handoffs/2026-10-29.md)。
文書整理#414 / PR #415は最終head `1726364`のCI `40799073589`成功後、main `815afde`へ統合・作業木整理済み。
文書のみの変更で、通常exeの再buildは不要。確定値は`D:/NeNeFolio/agents/414-quality-status/completion.json`。

## 品質改善の現在位置

エスケープした画像印の直後の通常link [#416](https://github.com/hideyukiMORI/nene-folio/issues/416)は、
既存のescape判定を画像抑止条件へ加えた完成候補。旧sourceの限定red exit1、修正後の対象build/tidy・unit exit0。
backslash偶奇、通常link、code、未閉じlinkを確認し、main `815afde`へのrebase後も4入力のhashが不変だった。
[対象記録](../quality/2026-10-29-escaped-image-link.md)。最終head必須CI・統合・通常反映・出力保全の確定値は
実PRと`D:/NeNeFolio/agents/416-escaped-image-link/integration-completion.json`を照合する。
文書検査が生成したpycの削除は自動承認レビューに拒否されたため、このworktreeは削除せず保持する。

今回の製品修正#402/#404/#405/#407/#410/#411は、各最終headの必須CI成功後にmainへ統合し、通常exeへ反映済み。
最新の製品sourceは`bb6dc01`、通常反映は10:07:08 JST、版0.3.0.0。更新後は起動していない。

置換factory容量[#411 / PR #413](https://github.com/hideyukiMORI/nene-folio/pull/413)は最終head `5a0a427`の
CI `40797586045`が成功し、main `bb6dc01`へ統合した。preview7失敗点と最大fit計2件、out/matches保持・同じ一覧でのretryを確認した。
[対象記録](../quality/2026-10-29-replacement-size.md)。通常UIの自然到達は未確認で、公開core APIの宣言長算術として扱う。
確定値は`D:/NeNeFolio/agents/411-replacement-size/integration-completion.json`。

引用/リスト内の表の字下げ[#410 / PR #412](https://github.com/hideyukiMORI/nene-folio/pull/412)は最終head `355d034`の
CI `40796594189`でfull-gate/check成功後、main `eb9706d`へ統合済み。
[ADR0072](../adr/0072-native-table-container-indent.md)、[対象記録](../quality/2026-10-29-table-container-indent.md)。
native317 checks/0 failures、失敗注入native25点/literal16点を確認した。#411と合わせ通常exeへ反映済み。
確定値は`D:/NeNeFolio/agents/410-table-container-indent/integration-completion.json`を照合する。

対象結果と各CIの確定値は日報・引き継ぎを参照する。完了成果物は各agents内output-archiveへ全hash照合済み。
作業物削除は自動承認レビューに拒否され、下記へ保持理由を記録した。

検索は既存計測を照合し、ファイルopen待ちと一致計算のCPU時間を分けて調査した。
1文字検索の3候補は非製品の正確性診断まで完了。初回計時は他案件buildと競合したため不採用とし、負荷が続く間は再計時しない。
10:02〜10:03の確認でも別案件のcompile/tidyが稼働していたため、新計時は0件のまま。
idle一致計算の追加検討では、保存修復/削除後の結果・scroll・所有移転も変更が必要と分かった。
背景I/Oを含め設計案は比較段階であり、製品採用はしていない。次は現行契約を守る小さい案と、計算分割の契約変更を分けて判断する。
調査・診断原本は `D:/NeNeFolio/agents/quality-20261029/`。
引用内表のnative字下げ/残幅は#410でmain統合済み、極狭幅の多列表の可読性は別課題として残す。
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
現在の通常`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`はsource `bb6dc01`・表示版0.3.0/Windows版0.3.0.0。
通常process0件を確認してから正規ツールで反映しexit0だった。10月29日10:07:08 JST更新済みで、更新後は起動していない。
版・hashは`D:/NeNeFolio/agents/411-replacement-size/normal-build-verification.json`。通常dataは操作していない。

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

既存28箇所と、拒否されたpycを含む#416作業木は保持する。再試行や別手段での削除をしない。

- #416: `D:/NeNeFolio/worktrees/416-escaped-image-link`。内部の`eng/__pycache__`と`conformance.cpython-312.pyc`の削除が`blocked by policy`で拒否された。追加理由なし。含む作業木の削除も行わない。出力領域の統合後保全・整理は同taskの`integration-completion.json`を照合する。

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
