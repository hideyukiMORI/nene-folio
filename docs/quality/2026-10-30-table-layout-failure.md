# 表の拡張幅が失敗するnative境界 — #462

2026-10-30。基点main`3dbed2ae1e889b893e98721a5da135f76d203632`。
規則: ARC-001/004/008/011/012、C-008/012/013/014/016/017、QLT-008/010/012/013、GIT-001〜004。ADR0072/0044。Waivers none。

## 焦点と既存の根拠

[#456の診断](2026-10-30-table-horizontal-feasibility.md)で、root63列/100%・90720twipと、
63列/indent32767/300%・123487twipはTOM成功・表配置FAILEDだった。両条件の要求列幅は1440twipである。
失敗直後の全列は圧縮幅/0幅のままだったが、query/SET/postGET/indent-count-width照合のどこが失敗したかは未観測。
後から読んだ幅だけではAPI拒否・補正・失敗した最初の行やrollbackを断定できない。

この焦点ではroot63/100%の一条件だけを再現し、既存native呼出しの境界を識別する。
元の`src/ui/win32/note_pane_table.c`本体・呼出し順を保ち、表TU限定のSendMessageWを一回だけforwardして入力と結果を記録する。
row構造はSETからGETへ再利用されるため、各呼出し前後のrow/cellsを別に捕捉する。失敗出力を正しい0幅と扱わない。
独自配置、追加SET、幅探索、製品の最小幅/上限/fallbackは採用しない。

SOLの静読報告を親が元source/旧rawと照合し、8source/evidenceと11参照の現在hash/size一致を確認して受理した。
開始終了hash保証は17:50:35〜17:57:13 JSTで、17:47:35 JSTの初読以後すべてを保証した表現にはしない。
原本は調査領域の`read-only-failure-boundary-plan.md`、`input-hashes.json`、`parent-research-acceptance-v1.json`。
報告末尾の旧48保護pathsは現49の表記誤りとして受理記録に補足し、元報告を保持した。

静的準備の早期レビューでは、TOM不備も幅失敗の再現と判定する合成boolを分離し、typed layout結果とguard/trace完備を別にする。
traceの取得停止とdumpも分け、元Resume/Release/dirty復元の後に出力する。記録directoryの親作成漏れも実行前に訂正する。
これらは未実行の補助稿の修正で、製品の不具合やnative失敗の実測として数えない。

## 実行前の条件

読取報告・入力hash、元body不変、透過な一回forward、有効count内だけの出力を親が受理してから実行する。
現sourceの正典Debug metadata/libraryを優先する。#452の古い全source一致検査は#460のUTF8変更で成立しないため、弱めて通さない。
既存v4の本文/選択/dirty/UndoRedo flagsとowned cleanupを保持し、未実行の実RedoUndoや全char wrapを成功項目へ加えない。
root3列と深い字下げの成功/失敗原本を再実行せず再利用し、今回の技術失敗も実終了値のまま記録する。
製品source/config/API/schema・版・通常exe/dataは変更しない。

現sourceのConfigure・Debug Build・Inspect・Compileは各一回成功し、続くnative一回で元SET拒否を観測した。
調査原本は`D:/NeNeFolio/agents/quality-post460-table-failure/`、task原本は`D:/NeNeFolio/agents/462-table-layout-failure/`。
作業木/一時出力はD側同taskへ置き、統合・必要反映確認・全保全・安全監査後に整理する。旧削除拒否49pathsは操作しない。

## 一次API資料

[EM_SETTABLEPARMS](https://learn.microsoft.com/en-us/windows/win32/controls/em-settableparms)は成功時S_OK、
E_FAIL/E_INVALIDARG/E_OUTOFMEMORYを記載し、列数1〜63と行位置の条件を持つ。
[EM_GETTABLEPARMS](https://learn.microsoft.com/en-us/windows/win32/controls/em-gettableparms)は行delimiterでのqueryを要求する。
[TABLEROWPARMS](https://learn.microsoft.com/en-us/windows/win32/api/richedit/ns-richedit-tablerowparms)のbTableLevelはGET専用、
[TABLECELLPARMS](https://learn.microsoft.com/en-us/windows/win32/api/richedit/ns-richedit-tablecellparms)のdxWidthはLONG。
これらにnative表の数値総幅上限は明示されていない。型の大きさやcallerのviewport capから限界を推定しない。
確認記録は調査原本の`parent-primary-api-review-v1.json`。

## 実Debug構成と観測用コードの受理

静的packet28入力とcanonical source404入力を親がhash/size照合して受理した。
Configureは434入力/3成果物、Buildは471/9、Inspectは484/4で、全child/wrapper0・記録error0。
currentとsnapshotの全hash/size、入力after不変を確認した。Debug Buildはnenefolio targetの81actionsで、Unitは実行していない。
clang-cl/LLD/llvm-nm19.1.5、Windows SDK10.0.26100.0、VC14.44.35207、RC10.0.26100.7705を使用した。

Inspectは元table source3432bytesの前にtrace includeを置き、元全bytesの保持を確認した。
fixture/table-tracedの実compileは正典vectorのsource/Fo/Fdだけを変え、C23・警告・ASan/UBSan/nullability・no-recoverを維持する。
linkも元RC・4product libs・4sanitizer runtimes・各flagを維持し、fixture/traced objects・console・出力先/map/verboseを派生する。
実vectorはSHA256 `098ab37d0e3d10e50de693a714fabb4e9ab7683a92c95a70eff7dc9500e73617`。
親がInspect488組とvector485入力、body/source/argvを独立照合し、Compile一回だけを受理した。
原本は`configure-build-inspect-completion-v1.json`、`parent-vector-acceptance-v1.json`。

Compileはchild/wrapper0・記録error0、490入力不変/17成果物の507組一致。
mapに元archiveのtable memberはなく、nmのscan/layout両exportはtraced objectに各一つだけだった。
実exe SHA256 `138dfd33aa13d876b60e94cabb4e6e6c852444152496cb9923a3810a94482c05`。
link stderrは指定したverboseログを保全した。実tool/SDK環境もInspectと一致する。
報告読取utilityは古いidentity filenameでFileNotFoundになったが、実recordの名前へ読取先を訂正した。
製品/fixtureの実行失敗ではなく、Compileや成功段の再実行も行っていない。
原本は`compile-boundary-01-audit-v1.json`、`parent-compile-acceptance-v1.json`。

## root63条件の限定結果

18:42 JSTに専用desktopの合成本文で一回実行した。MSFTEDITの読込先はSystem32、対応file metadataは10.0.26100.9949。
実120DPI、client320×600、100%、indent0、63列×1440twip、入力90720twipで、styleを変えていない。
元table TU内のcaptureは2record、failure0。元table bodyが最初に失敗した枝は次の通りだった。

| 元の呼出し | 結果と意味 |
| --- | --- |
| 最初の行query、cp118 | EM_GETTABLEPARMSはS_OK。level1、63列、indent0、margin80を返した |
| 同じ行の幅設定、cp118 | 63列すべて1440twipを渡したEM_SETTABLEPARMSがE_INVALIDARG（-2147024809 / 0x80070057）を返した |
| 元処理の確認GETと次行cp374 | SET失敗時の即returnにより未実行。幅比較の不一致で失敗した結果ではない |

失敗したSET後の入力bufferコピーはnative readbackではない。後段の独立GETは両行で成功し、
両行とも36列×61+27列×60=3816twipの圧縮幅だった。hiddenな一時変更やrollbackの有無はこの前後値から断定しない。
TOM suspend/resumeはともにHRESULT1（SUCCEEDED）、typed layoutはFAILED、native幅成功はfalse。

baseline/失敗後の本文・選択2..7・dirty=-1・非空Undo/Redo flags・固定設定等の10契約は不一致0。
元fixtureで失敗後には行わない実RedoUndo・全char wrap比較・横scroll・幅restore・新画像は加えていない。
技術失敗1、worker/controller/runner/record child・wrapperの実終了値はすべて3、記録error0。
510入力不変と17成果物の527組を照合した。timeoutなし、pane/parent破棄、worker終了、desktop閉鎖、所有process0を記録した。

今回確定したのは、この固定条件の最初の元SETがE_INVALIDARGを返す境界までである。
過去#456には内部traceが無いため、同じ公的FAILEDの再現から過去の内部原因まで同一と認定しない。
native内の具体的な拒否条件・数値総幅上限・製品最小幅・横操作方式は未確定で、幅方式やfallbackは採用しない。

## 同raw監査と統合する範囲

受理済み`table-record-stage-v1.ps1`を通じた実行は以下の各一回だった。引数にPreviousAttemptを指定した行は直前の原本を要求する。

| Stage / Attempt | PreviousAttempt | child / wrapper | 入力 / 成果物 |
| --- | --- | --- | --- |
| Configure / configure-debug-01 | なし | 0 / 0 | 434 / 3 |
| Build / build-debug-01 | configure-debug-01 | 0 / 0 | 471 / 9 |
| Inspect / vector-inspect-01 | build-debug-01 | 0 / 0 | 484 / 4 |
| Compile / compile-boundary-01 | vector-inspect-01 | 0 / 0 | 490 / 17 |
| Probe / probe-boundary-01 | compile-boundary-01 | 3 / 3 | 510 / 17 |
| Audit / audit-boundary-01 | probe-boundary-01 | 3 / 3 | 529 / 1 |

全段で記録error0、入力after不変、current/snapshotのhash/size一致。Probe/AuditのRuntimeLabelはboundary-v1。
Probe/Auditの外側PowerShellは実終了1であり、記録内のchild/wrapper3と区別して原本へ残す。
Auditはnativeを再実行せず、同rawの元source枝・row/cell対応・実終了・cleanupを監査した。
`diagnosisComplete=true`、errors0、failureBoundaryはset_failed/cp118/E_INVALIDARGで、幅成功へ読み替えていない。
親もProbe527組/Audit530組とrawを照合し、`parent-native-acceptance-v1.json`へ受理を記録した。

統合対象は品質記録2件・日報・引き継ぎ・currentの5文書で、#460の統合/通常反映/7612files保全・整理の確定事実も収載する。
対象文書の差分/参照/conformanceを確認し、最終PR headの必須CI成功後に文書統合・通常exe hash維持・全保全・安全整理を行う。
確定原本は同taskの`integration-completion.json`とし、最終CI/統合/整理の未実施分を実施済みとしない。

## 統合・通常exe維持・保全の確定

親の限定文書検査は5file/37参照、conformance違反0、762入力不変、child/wrapper0・記録error0。
PR #463のhead`79ae8cbfa02a82b9c64b9399294095e43ad3550e`は、必須CI`41036377740`のfull-gate/check成功後、
main`d137e212486351355ddec9ee127b17aba68677c2`へ統合した。Issue終了・統合tree一致・clean main/originを照合した。
通常exeは#460のsource`3dbed2a`、0.3.0/0.3.0.0、5,560,320bytes、17:38:56 JST更新のまま、
SHA256 `DC3B3EE6FDDB8AE1B9BBA10F3828574C264E9737892C27F40AE3B3F32D534AA5`を維持した。再build・起動・通常data操作0。
7stage/28raw files/3732snapshotsを索引化し、3937files/1609813447bytesを恒久archiveへ全hash/size照合して保全した。
未保存/未追跡/ignored・link・稼働参照0の監査後、worktreeを削除しbranch/commitを保持した。
出力`D:/NeNeFolio/outputs/462-table-layout-failure`の削除は実行前に自動承認レビューが`blocked by policy`で拒否した。
追加理由は返らず再試行しない。50箇所目として保持し、失敗値を含む原本もarchiveへ収載済み。
確定原本は`final-ci-41036377740.json`、`normal-preservation.json`、`cleanup-audit.json`、`integration-completion.json`。
