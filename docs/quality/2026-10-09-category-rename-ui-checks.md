# D3 #211 — カテゴリ改名UIの対象確認

- Issue #211 / 親 #169 / FR-037・FR-038 / ADR0041。D1/D2統合main `40871d7`上の最終source `6224e3d`。
- 規則: ARC-001/003/010/011、C-002/011/012/018、CNF-009/011、QLT-008/012/013。Waivers: none。
- command/catalog/行メニュー/name_prompt/aftercare/3言語と文書を接続した。D2のbreadcrumb getter・計測/描画・has-previewによるSTALEの実装は重複させていない。

## 対象と実測

実測はWindowsの製品Win32/RichEdit部品、private product fonts、**120 DPI（125%）、最小560×360 DIP**。
`folio_window.c`の正典をincludeした隔離probeと製品name_promptライブラリを使い、persistence portだけをメモリ内で制御した。
OS改名・削除・ごみ箱・利用者app/dataは呼んでいない。NOTE retry後cache OOMは製品note_corpus.cのmallocだけを一回失敗させた。

- renamecategory/rencatのNAME引数・listed・全表・3言語をunitで確認。カテゴリ行は対象添字を直接渡し、文書選択を変えない。
- strict cursorが現在文書より優先する。対象NAMED+EDITだけ先保存し、別カテゴリ・VIEW・UNTITLED・NONEの本文を保存しない。
- 通常CATEGORY面は元名全選択、カテゴリ一覧HWNDなし。取消し・不正名はSTARTなし。
- 先行NOTE/CATEGORYを対象・doc-kind・名前引数・保存より先に実kindの専用面で再開する。閉じると元要求のSTART/create/writeは0。
- NONEやUNTITLEDから既存Renameへ入っても先行CATEGORY復旧が先。READY後だけ元要求を改めて実行し、保存のcreateを復旧の完了と混同しない。
- 表示後にunexpected PENDINGとなったFIRST/SAVE_ASは名前とカテゴリ選択を維持し、保存を開始せず、retry面へ化けない。
- この面のSTARTだけPENDING/HALTEDへ固定する。RetryはRESUMEだけで、本文・名前を再保存しない。retry後は借用viewを再取得する。
- 先行NOTEおよび今回NOTEの採用後OOMでplanが消えた場合、存在しないretry面を残さず失敗を返す。先行の場合は元保存を開始しない。
- body text/selection/scroll/document/modeと元の有効focusを保持し、RichEditの実Undoが成立する。置換の第2欄focus、両欄の文字・選択・Undoを保持し、追加境界では両欄の実EM_UNDOも確認した。
- READYのEx/Paletteだけ閉鎖する。search/replace/settings/有効historyは保持し、先保存でhistory0になった面は改名の成否にかかわらず閉じる。
- 現在previewの破棄は非空パターンのSTALE表示へ写る。有効0件を「previewなし」と扱わず、別カテゴリの有効previewは保持する。

## 表示と補正

表示画像を確認した組合せは、**明示LightのJA/EN/ZH通常・PENDING、JA/ZH HALTED、DarkのEN HALTED/PENDINGと保持後、JAの長名専用復旧**。
全言語×全状態×両テーマを撮影済みとはしない。STATICは製品font/幅/折返しのDrawText実測で必要高以下を確認した。

固定欄はread-only EDITでHome/Endと選択・コピーができる。作成時ES_NOHIDESEL、新名範囲選択、EM_SCROLLCARETによりRetry focusでも新名の帯が見える。
category_nameおよび外部scanのname_listはいずれも255 UTF-8 bytes以下。旧名255＋新名255＋区切り5＋終端1は516 bytesでpending_line_capacity600に収まる。
実際に両名255 ASCII bytesを表示し、新名末尾Bの初期選択、Homeの旧名先頭、Endの新名末尾を確認した。

内部の短時間PrintWindowで黒/文字抜けが出たため、部品状態のassertと画像成功を分けた。eventloopを15秒保持し、正典`tools/capture-window.ps1 -TargetPid`で外部撮影した。
それでも再配置後のZHにlabel/釦の欠けが残り、reflowの末尾へRDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDRENの非同期再描画予約を追加した。
同じHALTED→PENDINGの高さ310→307pxで欠けが解消した。撮影用の代替描画・同期UpdateWindowを製品へ追加していない。
今回STARTの保留案内は未完了だけを示す。面内PENDINGは説明単独、HALTEDは原因＋説明、通常保存面のunexpected PENDINGは外側の入口案内を保つ。

採用画像は[category-rename-ui](category-rename-ui/)へ収載した。
例: [JA通常](category-rename-ui/japanese-light-normal-external.png)、[ZH HALTED](category-rename-ui/chinese-light-halted-external-redraw.png)、
[ZH PENDING](category-rename-ui/chinese-light-pending-external-redraw.png)、[EN Dark HALTED](category-rename-ui/english-halted-external.png)、
[長名の初期選択](category-rename-ui/prior-category-long-other-form-replace-small-dark-initial-to-selected.png)、
[置換面保持後](category-rename-ui/form-ownpending-halted-retryfail-replace-small-dark-english-kept.png)。

## コマンドと証跡

ログ・隔離probe source/build scriptの正本は `D:/NeNeFolio/agents/211-ui-resume/`。
39完了scenarioの保存ログにPASS 1781 / FAIL 0（試行中の再確認を含む合計）。`probe-results.json`に各ログの数・port回数・SHA256を記録した。
sourceの機能境界が不変の成功結果は再利用し、後のcopy/再配置補正は直接影響する表示だけ確認した。

| 実行 | 結果 | 証跡 |
| --- | --- | --- |
| CMake Debug configure/build | exit0、C23/clang-cl/clang-tidy | configure.log、build.log、build3.log、build4.log、build5.log |
| `build/folio_tests.exe --commands-ui` | D2最終取込後exit0、copy補正後exit0 | unit-commands-ui.log、unit-commands-ui-finalcopy.log |
| `pwsh -NoProfile -File .../build-probe.ps1` | exit0、ASan/UBSan付き製品UIリンク | probe-build.log、probe-link.log、corpus-build.log |
| `ui_probe.exe <scenario>` | 各完了scenario exit0 | probe-*.log、probe-results.json |
| `pwsh -NoProfile -File tools/capture-window.ps1 -Title <title> -TargetPid <owned pid> -Out <png>` | 対象pidの通常/保留面を外部撮影、上記採用画像を目視 | *-external*.png |
| 設計席の独立静読 | 主要経路P1/P2なし、copyと再描画補正も受理 | review-root.md、設計リナのcategory-rename批評記録 |

旧D2 checkpointのPENDING期待文言によるunit失敗はmain取込で解消した。新UIの実装不良とは分けて記録した。
完了probeにもASanのinterception_win出力があり、非致命の終了0と対象assert成功を区別して記録する。
初回Start-Process外部起動で同出力後modalに達しなかった試行は画像成功へ数えず、通常CLI実行と外部撮影を組み合わせた。
物理打鍵・IME・別DPI・実data/OS adapter改名は今回の測定範囲外。成功済みB/C/D1/D2の独立試験は反復していない。

## 文書とschema

README、SPECIFICATION FR-037/038、KEY_BINDINGS、ADR0041 D3補足を実装へ合わせた。
schema・永続化版・ゲート・依存・除外・閾値は変えていない。親のdaily/handoff/current/CLAUDEは同じPRへ別の文書commitで収載する。

## 最終単一ゲート

最終source `6224e3d`の初回`pwsh -NoProfile -File ./eng/check.ps1`はexit1。
compile/clang-tidy、CTest 2/2、symbols 2 libraries/0違反、coverage3214/3430=93.70%までは通過した。
最後のproofの隔離snapshotでは、親が未追跡のまま保持するよう指示したdaily/handoffが複写されず、CLAUDE/currentからの4リンクがCNF-006となった。
証跡はgate.log。親の文書収載順を修正し、親4文書と品質記録/画像をstageしてconformance exit0を確認した。
ゲート/snapshot/除外を変更せず、今回持ち込んだリンク収載失敗を直した再試行として同じ正典コマンドを実行し、**exit0**（gate-retry.log）。
CTest 2/2、conformance 0違反、symbols 2 libraries/0違反、中核分岐3214/3430=**93.70%**、反例7.35%は90%未満として拒否、実ツールproof **19本**が成功した。
source/test/CMakeは`6224e3d`から差分0。品質追記・親の最終文書commit・push/レビューでこの成功を再利用する。
