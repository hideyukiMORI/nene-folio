# いまの状態 — NeNe Folio

記録日: 2026-10-29。hideの継続品質改善依頼により、設計Astra・実装SOL/LUNAの分担で調査と修正を進める。
この分担は今回の作業に適用し、将来の再開では最新のhide指示・実Issue/PR・checkoutを照合する。
経緯は[日報](../reports/2026-10-29.md)、再開情報は[引き継ぎ](../handoffs/2026-10-29.md)。
文書整理#414 / PR #415は最終head `1726364`のCI `40799073589`成功後、main `815afde`へ統合・作業木整理済み。
文書のみの変更で、通常exeの再buildは不要。確定値は`D:/NeNeFolio/agents/414-quality-status/completion.json`。

## 品質改善の現在位置

検索の一致計算分割[#428](https://github.com/hideyukiMORI/nene-folio/issues/428)は対象検証済みの完成候補。
[ADR0073](../adr/0073-bounded-index-matching.md)と所有/可変区画/C-013等を実装前に受理した。
新語と初回末尾の計算を既存idleから65536比較workずつ進め、確定集合の保存/改名修復は同じengineを同期完了する設計。
warmも準備中の全索引へ移し、完成集合だけを採用する。取消/更新時の進捗破棄と失敗時の契約を明記した。
親が設計/規約/統合、SOLが製品/unit/OOM/品質記録、LUNAが実loopのnative4ケースを担当する。
専用作業木は`D:/NeNeFolio/worktrees/428-bounded-index-matching`、原本/出力はD側agents/outputsの同名領域。
旧sourceのwarm同期完了を限定redで確認し、完成候補のDebug build/tidy・検索対象unit・同vectorの確保失敗試験が成功した。
初回buildは新テストのmalloc後の静的診断で停止し、テスト2箇所の失敗分岐を明示して解消。製品sourceは不変だった。
専用desktopで実loopの通常message/clear/close/native modalの4ケースも成功し、read1を維持した。
親は成功したunit側5runとnative4runの指定入力/生成物hashを照合し、再実行していない。
診断build自体は子0だったが、未生成compiler PDB4件の宣言による記録wrapper125を分離して残した。
最終static/CI・統合/通常反映/整理の確定値は実PRと同taskの`integration-completion.json`を照合する。
同期layout・OS read・初期集計/確保等は残り、実時間上限や速度改善率は主張しない。

表開始と見出しの優先順位#426 / [PR #427](https://github.com/hideyukiMORI/nene-folio/pull/427)は、
最終head `5bc03fa`のCI `40817131821`でfull-gate/check成功後、main `9d611cf`へ統合した。
共用開始判定を既存行分類へ揃え、独立red2件と完成候補のDebug build/tidy・対象selectorが成功した。
通常exeへ13:26:47 JST反映済み、1243ファイルの全hash保全とworktree整理も完了。
出力削除は拒否されたため保持する。[品質記録](../quality/2026-10-29-table-start-priority.md)、
確定値は`D:/NeNeFolio/agents/426-table-start-priority/integration-completion.json`。

対象実行の証跡保存[#424 / PR #425](https://github.com/hideyukiMORI/nene-folio/pull/425)は、最終head `3a8eacd`の
CI `40813130935`でfull-gate/check成功後、main `1cae763`へ統合した。
#422で途中exe/inputを同名上書きした問題へ、既存コマンドの実行前入力・終了時成果物・raw出力・終了値を新規directoryへ記録する小さな道具を加える。
既存run-dirは子起動前に拒否する。入力の十分性やテスト判定は自動認定しない。親が設計/規約/統合、SOLがtool/test/品質記録を担当する。
初回9件は成功。追加Windows終了値の1件でhigh-bitの喪失を検出し、CLI末尾のsigned32変換後に同じ1件が成功した。
受理設計は`D:/NeNeFolio/agents/424-run-record/design-decision.md`、試行結果は[品質記録](../quality/2026-10-29-run-record.md)。
CIでも新規10件を含む107件のconformanceが成功。103ファイルを全hash保全し、worktreeは整理済み。
出力削除は拒否され保持する。確定値は実PRと同taskの`integration-completion.json`。製品無変更で通常exe再buildは不要だった。

閉じた未対応画像の内部記号保持[#422 / PR #423](https://github.com/hideyukiMORI/nene-folio/pull/423)は、
最終head `16c4a94`のCI `40807935836`でfull-gate/check成功後、main `88cdf79`へ統合した。
通常exeへ11:48:26 JST反映済み、112ファイルの全hash保全・worktree整理も完了。出力削除は拒否されたため保持する。
途中失敗と証跡欠落は[品質記録](../quality/2026-10-29-image-literal.md)、確定値は`D:/NeNeFolio/agents/422-image-literal/integration-completion.json`。

リンク宛先から別LINKを作る安全性課題#420 / [PR #421](https://github.com/hideyukiMORI/nene-folio/pull/421)は、
最終head `bb513b7`のCI `40804908767`でfull-gate/check成功後、main `0793434`へ統合した。
18文字の合成入力の旧UBSan診断に対し、成立LINKのdestination終端まで消費する1行で修正した。
[対象記録](../quality/2026-10-29-link-destination-scope.md)。通常exe反映と112ファイル全hash保全も完了。
作業木削除は成功、出力削除は拒否され保持する。確定値は`D:/NeNeFolio/agents/420-link-destination-scope/integration-completion.json`。

検索結果集合の件数容量#418 / [PR #419](https://github.com/hideyukiMORI/nene-folio/pull/419)は、最終head `06a2082`の
CI `40803188868`でfull-gate/check成功後、main `88c441f`へ統合した。通常exe反映と278ファイル全hash保全も完了。
公開core queryの不可能な宣言件数を確保前に拒否する。通常UIの巨大件数到達とは区別する。
[対象記録](../quality/2026-10-29-index-filter-size.md)、確定値は`D:/NeNeFolio/agents/418-index-filter-size/integration-completion.json`。
worktreeは整理済み、出力削除は拒否され保持する。application集計全般や検索性能は変更していない。

エスケープした画像印の直後の通常link #416 / [PR #417](https://github.com/hideyukiMORI/nene-folio/pull/417)は、
最終head `76dac10`のCI `40801128237`でfull-gate/check成功後、main `de2a53a`へ統合した。
既存escape判定1か所で修正し、限定red/greenとbackslash偶奇・code・未閉じlinkを確認した。
[対象記録](../quality/2026-10-29-escaped-image-link.md)。通常exe反映と111ファイル全hash保全も完了。
確定値は`D:/NeNeFolio/agents/416-escaped-image-link/integration-completion.json`。
内部pycと出力の削除は自動承認レビューに拒否されたため、worktree/outputを保持する。

今回の製品修正#402/#404/#405/#407/#410/#411/#416/#418/#420/#422/#426は、各最終headの必須CI成功後にmainへ統合し、通常exeへ反映済み。
最新の製品sourceは`9d611cf`、通常反映は13:26:47 JST、版0.3.0.0。更新後は起動していない。

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
idle一致計算の追加検討では、保存修復/削除後の結果・scroll・所有移転を分ける必要があると分かった。
13時台に#428で、確定集合の同期修復を保ちながら入力/初回末尾だけを分割する設計を受理した。
背景I/Oやmemchr案は引き続き未採用。matching以外の同期費用を残す限定もADR0073へ明記した。
調査・診断原本は `D:/NeNeFolio/agents/quality-20261029/`。
10:33の次段階確認でも別案件のcompile/tidyが稼働しており、新たな性能計時は0件のまま。
11:46の13.24秒CPU監査では別案件のtest process2件が各約12.8秒増えたため、候補3種の計時は開始しなかった。
13ファイルの既存入力/binary hashは一致し、正確性検証を再実行していない。原本は上記領域の`matching-probe/attempt-1145/`。
入力時をidle、保存/改名後を同engineの同期完了とする案も読取レビューした。変更範囲を狭める余地はあるが、
当時は遅延OOM、同期修復scan、所有移転、無効化、budgetが未決で未採用だった。後の#428で契約と限定を受理した。
12:53の再開前監査も新しいcompile/tidyと別案件testのCPU増加を確認し、計時0件で停止した。原本は`matching-probe/attempt-1253/`。
分割計算案はsealed ownerの移譲と12/13操作を比較し、13操作・65536 work・限定的な可変ownerを#428/ADR0073で受理した。
公開契約の変更と同期layout等の残費用を明記した。これから対象実装/検証を行い、速度の数値は採否条件にしない。
未対応画像内部の文字保持は#422で統合済み。通常LINK labelを跨ぐ先行分類案は不採用とし、親label内へ収まる範囲だけを保護した。
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
現在の通常`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`はsource `9d611cf`・表示版0.3.0/Windows版0.3.0.0。
通常process0件を確認してから正規ツールで反映しexit0だった。10月29日13:26:47 JST更新済みで、更新後は起動していない。
版・hashは`D:/NeNeFolio/agents/426-table-start-priority/normal-build-verification.json`。通常dataは操作していない。

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

下記の35箇所は自動承認レビューの削除拒否により保持する。再試行や別手段での削除をしない。

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
