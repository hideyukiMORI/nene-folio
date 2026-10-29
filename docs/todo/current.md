# いまの状態 — NeNe Folio

記録日: 2026-10-29。hideの継続品質改善依頼により、設計Astra・実装SOL/LUNAの分担で調査と修正を進める。
この分担は今回の作業に適用し、将来の再開では最新のhide指示・実Issue/PR・checkoutを照合する。
経緯は[日報](../reports/2026-10-29.md)、再開情報は[引き継ぎ](../handoffs/2026-10-29.md)。

## 品質改善の現在位置

[#446](https://github.com/hideyukiMORI/nene-folio/issues/446)の一般語検索は、部分一致を保持して総比較workを線形に抑える完成候補が対象検証を通過した。
prefixが無い候補だけ既存tail skipを使い、不一致は一比較ずつ継続する。[ADR0074](../adr/0074-linear-index-matching.md)の各source長Nに対する3N上限を維持する。
周期4入力の総workは8180/32756/7940/32516となり、旧73442/294626/990722/4161026から縮んだ。5予算scheduleで集合と総workが一致し、single65538と同phrase5057も成立した。
Debug/tidy・限定matching・測定build・strict OOMは子/wrapper0・記録エラー0。親が420/429/456/478入力と19生成物を全hash照合した。
prefixを含むcreate4失敗点、途中SCAN破棄、prefix不要3経路、warm多文字語の第4確保失敗と再要求回復を確認した。旧binding proof3件をhash付き再利用し、新proof実行0。
strict stderrのinterception診断一行を保持し、全sanitizer正常・実時間速度・実UI応答改善は主張しない。一般語createのO(m)メモリ/前処理と同期I/O等の費用は残る。
単文字/公開signature/65536政策/UI/loop/I/O/schemaは不変。core OOMの既存間接配線を使い、当初の重複追加案は未実行で撤回した。
詳細は[品質記録](../quality/2026-10-29-linear-index-matching.md)、親の受理は`agents/446-linear-index-matching/parent-green-acceptance.json`。
最終File API規約検査は違反0。文書末尾の空行2件を訂正し、diff-final-02は773入力不変・子/wrapper0・記録エラー0で成功した。最終stage・必須CI・統合・通常反映・全保全と安全整理を続ける。作業木は`D:/NeNeFolio/worktrees/446-linear-index-matching`。Waivers none。

[#444](https://github.com/hideyukiMORI/nene-folio/issues/444)の現行matcher診断は、同じ公開APIによる6入力×2予算の12rowsが成功した。
各stepは予算4096/65536以内、両予算で完成集合と総workが一致した。周期的late-mismatchの総workは`2+(N-m+1)(m+1)`と一致し、有限stepだけでは総費用を線形に抑えないことを確認した。
N16384/m17は294626work、N16384/m257は4161026work。現UIは127 UTF-16単位制限のため、257 ASCIIのcaseは公開core限定、17 ASCIIも今回の実UI操作ではない。
10.2671577秒の負荷確認で外部CPU2.765625秒・不明16件となり、WORK_ONLYで一回実行した。診断時計API0・全時間nullで、速度値や実UI応答時間は主張しない。
新4TUのDebug/tidy/linkとnativeは子/wrapper0・記録エラー0。親がbuild1275入力/16生成物、native1306入力/19生成物を全hash照合した。stderrのinterception一行は保持し、全sanitizer正常とはしない。
製品source/API/schema/ADRは不変。詳細は[品質記録](../quality/2026-10-29-bounded-search-diagnostic.md)、親受理は`agents/444-bounded-search-diagnostic/parent-native-acceptance.json`。
[PR #445](https://github.com/hideyukiMORI/nene-folio/pull/445)最終head`88e1f3e`のCI`40880367466`成功後、main`84efcff`へ統合した。
通常exeは#442のhashを維持し、2753ファイル全hash保全とworktree削除済み。出力は削除拒否のため保持する。
確定値は`D:/NeNeFolio/agents/444-bounded-search-diagnostic/integration-completion.json`。

[#442](https://github.com/hideyukiMORI/nene-folio/issues/442)の通常literal内部SPの集約は、完成候補v2の対象5段が成功した。
通常literalの境界からSPを外し、先頭SP runとLF直前SP runを維持する。pair/emit・code/link/image・明示改行・OOM契約は不変。
65536bytesの空白区切り文で解析用calloc最大要求は旧1835008bytesから56bytesへ縮み、厳密RTF全bytesとfinishも一致した。
短phrase7bytesと既存通常literalも56bytes。metadata確保失敗2件と公開factory失敗8件を個別確認した。
最初のbuildはC-012複雑度12>10で停止したが、境界定義とLF以外の早期returnへ整理し、閾値・期待を保ったv2が実tidyを通過した。
Debug/tidy・canonical Markdown・測定build・strict資源/境界・strict OOMは全て子/wrapper0・記録エラー0。
親が452/455/456/461/461入力と11生成物を全hash照合し、実3compile commandとproof再利用3件も確認した。
各strictのinterception診断行を保持し、全sanitizer正常・全heap量・速度改善率は主張しない。最大backtick比例scratchは残る。
詳細は[品質記録](../quality/2026-10-29-inline-spaces.md)、親の受理は`agents/442-inline-spaces/parent-green-acceptance.json`。
最終File API規約検査と差分確認は各772入力不変、子/wrapper0・記録エラー0。親が全6150保全ファイルと最終8ファイルを照合した。
[PR #443](https://github.com/hideyukiMORI/nene-folio/pull/443)最終head`1e41166`のCI`40868567105`成功後、main`89b5873`へ統合した。
20:57:03 JSTに通常exeへ反映、6818ファイル全hash保全とworktree削除済み。出力は削除拒否のため保持する。
確定値は`D:/NeNeFolio/agents/442-inline-spaces/integration-completion.json`。

[#440](https://github.com/hideyukiMORI/nene-folio/issues/440)の行内解析は、通常文字の連続部分をまとめ、
共通の字句範囲で数えたtoken数Tと最長backtick長Bから容量を決める完成候補で対象検証が成功した。
token容量T・共用scratch容量max(T,B+1)とし、構文/出力/失敗契約と既存の長さ上限を保つ。
65536bytesの通常本文で解析用calloc最大要求は旧3670072bytesから56bytesへ縮み、正確なRTF出力も一致した。
Debug build/tidy、canonical Markdown、測定用build、資源/境界、OOMの5段は子/wrapper0・記録エラー0。
metadata確保失敗2件と公開factory失敗8件を個別確認し、成功済みの検出設定proofは再利用した。
各strict実行のinterception診断行を保持し、全sanitizer正常・全heap量・速度改善率は主張しない。
詳細は[品質記録](../quality/2026-10-29-inline-storage.md)、親の受理は`agents/440-inline-storage/parent-green-acceptance.json`。
最終File API規約検査・差分確認は各768入力不変、子/wrapper0・記録エラー0。親が全snapshotと最終10ファイルを照合した。
[PR #441](https://github.com/hideyukiMORI/nene-folio/pull/441)最終head`dbf0829`のCI`40861826966`成功後、main`591e5c4`へ統合した。
20:04:07 JSTに通常exeへ反映、6180ファイル全hash保全とworktree削除済み。出力は削除拒否のため保持する。
確定値は`D:/NeNeFolio/agents/440-inline-storage/integration-completion.json`。

索引スクロールの加算境界 #434 / [PR #435](https://github.com/hideyukiMORI/nene-folio/pull/435)は、
最終head `64a8b78`のCI `40836070731`でfull-gate/check成功後、main `875d058`へ統合した。
旧実装の有効位置15＋INT_MAXをUBSanで確認し、同じscroll意図内で移動量を先に丸める修正と対象検証が成功した。
16:39:26 JSTに通常exeへ反映、2589ファイル全hash保全とworktree削除まで完了。出力削除は拒否され保持する。
[品質記録](../quality/2026-10-29-drawer-scroll-delta.md)、確定値は`D:/NeNeFolio/agents/434-drawer-scroll-delta/integration-completion.json`。

本文入力・表示位置の静読と、検索pending中の保存/改名/削除/新規公開・借用寿命の独立静読は、新しい具体的不具合候補0件だった。
今回の試験成功や全経路の安全性証明を意味しない。新しい製品修正は採用していない。
直近16:34の負荷照会でも他案件compile/tidyが稼働しており、追加の性能計時は0件。
#428後のmatcherへ旧候補の計測値を読み替えず、背景I/Oと極狭幅表の可読性は未解消のまま扱う。
結果と未確認範囲は[品質の現在地](../quality/2026-10-29-quality-checkpoint.md)へ整理した。
文書反映 #436 / [PR #437](https://github.com/hideyukiMORI/nene-folio/pull/437)は、最終head `96ad8c5`のCI `40837772905`成功後、main `c860e0b`へ統合・作業木整理済み。通常exeは#434のhashを維持した。確定値は`agents/436-quality-checkpoint/integration-completion.json`。

[#438](https://github.com/hideyukiMORI/nene-folio/issues/438)の共有診断では、read-only readerと改名・ごみ箱の両立は合成条件で成立したが、保存はMoveFileExWのERROR_ACCESS_DENIEDで失敗した。
reader close後は同じ保存が成功したため、この共有設定だけの背景読込案は採用しない。初回45確認/2FAIL、保存限定12確認/2FAILを負の成立性証拠として保持する。
製品source・共有規則・背景thread・ADR・通常exeは変更していない。[品質記録](../quality/2026-10-29-read-sharing-feasibility.md)を参照する。
文書[PR #439](https://github.com/hideyukiMORI/nene-folio/pull/439)は最終head`1c176b8`のCI`40849927435`でfull-gate/check成功後、
main`f032adc`へ統合した。2632ファイルの全hash保全とworktree整理済み。出力は削除拒否により保持する。
確定値は`D:/NeNeFolio/agents/438-read-sharing-feasibility/integration-completion.json`。

以前の完了履歴は[日報](../reports/2026-10-29.md)・[引き継ぎ](../handoffs/2026-10-29.md)と各品質記録へ置く。
旧計測/設計原本は`D:/NeNeFolio/agents/quality-20261029/`。成功済み対象は関係入力が不変なら再実行しない。

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
現在の通常`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`はsource `89b5873`・表示版0.3.0/Windows版0.3.0.0。
通常process0件を確認してから正規ツールで反映しexit0だった。10月29日20:57:03 JST更新済みで、更新後は起動していない。
版・hashは`D:/NeNeFolio/agents/442-inline-spaces/normal-build-verification.json`。通常dataは操作していない。

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

下記の44箇所は自動承認レビューの削除拒否により保持する。再試行や別手段での削除をしない。

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
