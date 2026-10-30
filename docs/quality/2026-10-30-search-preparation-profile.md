# 一ノートの検索準備時間を分ける計測 — #458

2026-10-30。基点main`c149fbc`、製品sourceは#452の`ee65c6d`と同じ。
規則: ARC-001/004/005/007/010/011/012、C-005/008/012/013/014/016/017、QLT-008/010/012/013、GIT-001〜004。
関連: ADR0068/0073/0074。Waivers none。製品source・schema・gate・版の変更はない。

## 結果と次の判断

固定Releaseの合成入力で、16MiB本文を読む最長continueは43.3406ms、その中のUTF-8/NUL検証は32.3232msだった。
同じcontinue内のopen/size/read/close API合計は4.8974ms。次は文字列検証の処理量と契約を調べる。
この標本だけで背景I/Oや保存方式の変更を採用しない。#438のheld reader中の保存失敗は現sourceにも関係する。

| 入力・phase | query wall ms | continue数 | read数 | continue p95 / 最大 ms | 最終identity件数 |
| --- | ---: | ---: | ---: | ---: | ---: |
| A: 1,000件×10KiB、初回alpha | 81.9740 | 1032 | 1000 | 0.1019 / 3.8143 | 333 |
| A: query clear | 3.1373 | 0 | 0 | 対象なし | 1000 |
| A: 同processのwarm beta | 15.6790 | 40 | 0 | 0.2298 / 3.1919 | 199 |
| B: 128byte Anchorと16MiB Large、初回alpha | 63.0590 | 54 | 2 | 0.2991 / 43.3406 | 1 |

Aは3phase・27契約、Bは1phase・13契約で不一致0。worker/controller/runnerと最終audit・記録wrapperはすべて0、残存所有process0。
一continue最大一read、比較work上限、部分結果非公開、最終集合、未保存Anchorの本文/選択/dirty/実UndoRedoなどを確認した。
初回とwarmは語と一致集合が異なり、この表から速度倍率を計算しない。OS cacheのcold化もしていない。

A初回のAPI内経過合計は38.5188ms、検証20.1018ms、所有copy4.0127ms、matching step6.6492ms、UI更新9.7176ms。
warmはread0、matching step8.5601ms、UI更新6.7317ms。Aの比較workは初回2060262、warm2571192だった。
Bは比較work3355490、全matching step10.9820ms、最大step0.2984ms、UI更新8.5332ms。
これらは包含関係を保ったscopeから取り出した値で、nested inclusiveを重ねて加算しない。

## BのLarge一件の内訳

直下childを引いたexclusive区間を再集計した。最長continue内のcorpus loadは43.3398msだった。

| 区間 | ms |
| --- | ---: |
| UTF-8/NUL検証 | 32.3232 |
| OS open/size/read/close | 4.8974 |
| 2回のadoptによる複製/確保 | 4.8984 |
| その他のload/port/file/create/corpus | 1.2208 |
| 合計corpus load | 43.3398 |

API内訳はopen0.0192、size0.0020、read4.8636、close0.0126ms。
複製はnote作成側2.4386ms、corpus putのnote copy側2.4598ms。
包含側のport read40.2840ms、note作成34.7624ms、corpus put2.4618msを上表へ再加算しない。
API内経過にはCPUと待ちが混在し、API外の残差もCPU busy時間そのものではない。
QPC計装の負荷込みの単一標本であり、一般的な上限や物理操作時の応答保証ではない。

## 入力・観測経路・制約

正典ReleaseをD専用領域でconfigure/buildし、その実compile/link metadataから計装5TUだけを派生した。
元16処理本体とmain.cのbyte列を保持し、元main loopを同TU末尾の非製品forwarderから呼ぶ。
compile差分はsource/Fo/Fdだけ。linkは5置換object・console subsystem・出力/map/verbose指定に限定し、
正典4libraryとsystem libraryの順序を保持した。実map/nmでも置換ownerと既存core memberを照合した。
最終fixtureは689664bytes、SHA256 `3f70e51c1ad58a678ce0f8de987be1f584eefc16f99f92936dccbd265284e6eb`。

seed Aは1,000本文/1,012data files/10,240,000本文bytes。独立期待集合はalpha333、beta199、重なり66。
seed Bは2本文/5data files/16,777,344本文bytes。128byte Anchorと16,777,216byte Largeで、alphaはLargeの末尾だけ。
generatorをimportしない監査で全file集合/bytes/hash/UTF8/BOMなし/LF/codecと実保存本文の集合を照合した。
原seedは不変、各fixtureはruntime copyを使う。初期選択は小Anchorとして巨大本文のMarkdown描画と分けた。

検索欄focusには先行保存があるため、focus移動なしのprogrammatic EN_CHANGEを使う。
未保存Anchorの文字列は保存本文検索へ混ぜず、検索の前後で保持と実UndoRedoを確認する。
query wallはSetWindowText開始から元loop復帰直後まで。重いidentity/本文/UndoRedo oracleとJSON出力はwall endの後に行う。
setupの所有timerは計時前に終了し、計時中のheartbeat/追加pump/標準出力を加えない。
worker/controller両方の終了を確認してからdata inventory/ZIPへ進み、未確認なら技術失敗で止める。
終了dataを全file/hash/ZIPで照合し、seedからの変更はempty lock file追加だけだった。

QPC raw tick/frequency、scope親子、read bytes/outcome、実argv、tool/SDK環境allowlist、seed hashを原本へ保存した。
固定65536scope bufferの容量超過を失敗とし、LastError/戻り値/所有を保つ。直接子の時間を引く会計と全体恒等式を検査した。
OSの任意待ち上限、後続UIA/paint/DWM完了、物理入力latency、実reader聴取、cold-cacheを保証しない。
CPU事前観測の12件は並べ替えが成立していない部分集合で、上位負荷一覧やOS全体のidle証明として扱わない。
親の計測専用slotでは他の既知build/test/probeを起動せず、他担当も計測を行わなかった。

一次資料はMicrosoftの[QPCの計時指針](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)と
[QueryPerformanceCounter](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter)。
同じUI threadのtick差を使い、QPC負荷を自動的に差し引かず、RDTSCやaffinity変更を加えない。

## 実行と失敗原本

実コマンドは同taskの`agents/`にあるdriverから`tools/record-run.py`へ渡し、全argvとraw stdout/stderrを保存した。

| 実行段階 | 結果 |
| --- | --- |
| `canonical-record-stage-v2.ps1 -Stage Configure -Attempt configure-release-01` | 410入力/3成果物、child/wrapper0、記録error0 |
| 同driverの`-Stage Build -Attempt build-release-01 -ConfigureAttempt configure-release-01` | 81actions、434入力/9成果物、0/0/error0 |
| `seed-record-v1.ps1 -Case A` / `-Case B` | 各26入力不変、0/0/error0、全実seed監査成功 |
| source生成/実vector inspect各01〜03 | 各321入力/6成果物、329入力/2成果物、0/0/error0 |
| compile-profile-01 | loop0→window1、controllerのHANDLE注釈不足2件。child1/wrapper125、未生成artifact記録error21 |
| compile-profile-02 | loop/window/state0→file1、OS open戻りHANDLE注釈不足1件。child1/wrapper125、記録error13 |
| `measurement-record-stage-v4.ps1 -Stage Compile -Attempt compile-profile-03 -InspectAttempt vector-inspect-03` | 5compile/link/nmすべて0、333入力/29成果物、0/0/error0 |
| native v2、measure-a-01、CompileAttemptを明示03 | 1397入力/19成果物、0/0/error0、3phase完遂 |
| audit-a-01、旧audit v1 | 正常な独立UI更新rootを拒否し、実3/3・記録error0。製品不一致ではない |
| audit-a-02、audit-only v2 | 同じA rawを再解析、1425入力/1成果物、0/0/error0 |
| native v2、measure-b-01、CompileAttemptを明示03 | 390入力/19成果物、0/0/error0、1phase完遂 |
| audit-b-01、audit-only v2 | 418入力/1成果物、0/0/error0 |

compile失敗は補助codeのnullability注釈だけを直し、flags/警告/製品sourceを変更しなかった。旧helper/生成物も保持する。
audit v2は許容rootへ既存`REFRESH_AFTER`を加える一行とlabelだけを変更した。
正典dispatch→accessibility変更のposted message→UI更新が独立rootを生む経路をsourceで照合した。
rawが記録したのはscopeであり、message IDや厳密生成元を実測したとは扱わない。既存3phaseの全数値はv1/v2で一致した。
nesting・子区間差引き・恒等式・read/work・identity・本文/Undo・data・終了値の検査は維持し、Aの製品実行を繰り返していない。

独立静読で見つけた計画のoracle混入、外部compiler flag拒否不足と、親が見つけたworker終了確認不足も実行前に補正した。
成功済みRelease/seed/元処理本体のレビューを再利用し、各訂正版だけを別fileと新run-dirで扱った。

## 保全と統合条件

親は各入力/成果物のsnapshot/current hash・size、raw・終了値と集約値を照合し、A/Bを14:56/14:59 JSTに受理した。
受理原本は`parent-a01-profile-acceptance.json`と`parent-b01-profile-acceptance.json`。
最終報告は`measurement-technical-report-v1.md`、集約値は`measurement-completion-v1.json`。
全18実行段階・72raw hashes・9025利用可能snapshotを`stage-preservation-index-v1.json`へ集約した。
失敗で未生成の34artifactは記録errorのまま保持する。成功検証やnative実行を索引のために繰り返していない。
恒久原本は`D:/NeNeFolio/agents/458-search-preparation-profile/`、実出力は同名`outputs/`、作業木は同名`worktrees/`。

文書の差分/参照/conformanceを確認し、最終PR headの必須CIを統合条件にする。通常exeはhash維持で、再build/起動しない。
統合後の全出力保全と安全監査、作業木/不要出力の整理結果は同taskの`integration-completion.json`へ記録する。
既に削除拒否された48箇所は再試行しない。実測の一般化、文字列検証の改善実装、背景I/O・保存方式変更は未実施。

## 文書統合と保全の確定

文書5file/31参照/conformance違反0、760入力/1成果物のcurrent/snapshot一致、child/wrapper0・記録error0。
PR #459の最終head`a0477246b801cb2e2f3032fede6a585f204030d7`は必須CI`41012681318`のfull-gate/check成功後、
main`78243449af0c5b09199bd88426409099b910b66a`へ統合した。Issue終了・tree一致・clean main/originを照合した。
通常exeはsource`ee65c6d`・SHA256 `0ADACB5BE97FB89E51B3FE092F3258897E7DE7A3AAFBC6B0E21A567229FD7DAA`を維持し、再build/起動/通常data操作0。
12932files/741475903bytesを同taskの`agents/output-archive`へ全hash照合して保全した。
未保存/未追跡/ignored・link・稼働参照0を確認してworktreeを削除し、branch/commitと失敗原本を保持した。
出力`D:/NeNeFolio/outputs/458-search-preparation-profile`の削除は自動承認レビューが実行前に`blocked by policy`で拒否した。
追加理由は返らず、再試行せず49箇所目として保持。確定原本は同taskの`integration-completion.json`。
