# 共通UTF-8 decoderのASCII処理 — #460

2026-10-30。基点main`78243449af0c5b09199bd88426409099b910b66a`、製品基点`ee65c6d`。
規則: ARC-001/003/007/008/010/012、C-005/008/012/013/014/016、QLT-008/009/010/012/013、GIT-001〜004。
関連: ADR0042/0044。Waivers none。

## 結果

共通decoder内のASCII早期returnを実装し、Debugの文字列境界と固定Release Bの13契約を受理した。
計装付きのASCII中心16MiB本文を使う一回値は次のとおり。旧#458の成功測定は再実行していない。

| 観測範囲 | 旧実装 ms | 候補 ms |
| --- | ---: | ---: |
| 検索全体 | 63.0590 | 49.3964 |
| 最長continue | 43.3406 | 30.4207 |
| Large一件の準備 | 43.3398 | 30.4197 |
| その内訳: UTF-8/NUL検証 | 32.3232 | 18.7385 |
| その内訳: OS API内経過 | 4.8974 | 4.7005 |
| その内訳: 複製/確保 | 4.8984 | 5.9178 |
| その内訳: その他 | 1.2208 | 1.0629 |

Largeの4内訳は重ならない区間へ分けた値で、親子inclusiveの単純加算ではない。
両方とも54continue・2read・matching work 3355490、最終ID集合[1]。本文・選択・modified・実Undo/Redo等13契約に不一致0。
検索全体のQPCは旧630590tick、候補493964tick、周波数10000000Hz。親もrawから上表を再計算し照合した。
実objectでASCIIが共通判定を通らないことと、この固定標本での経過短縮を受理する。一般的な速度倍率や時間上限にはしない。
一件の同期準備はなお約30.42msを要し、背景I/Oや大きな本文の処理分割を解決したものではない。

## 根拠と設計

[#458の計測](2026-10-30-search-preparation-profile.md)では、16MiB ASCII中心本文一件の準備43.3398ms中、
UTF-8/NUL検証が32.3232msを占めた。QPC経過であり、正味CPU cyclesや普遍的な速度ではない。
既存Release core libraryの静的disassemblyでも、ASCII分岐後にminimum/range/surrogate判定へ合流していた。
単にsourceの見た目だけで削減効果を推定せず、実objectの構造を根拠に最小案を選び、上記の固定計測で確認した。

既存`utf8_text_decode`内で空入力を先に拒否し、unsigned leadがASCIIならcode pointを設定して消費1を返す。
私有`sequence_length`から対応するASCII分岐を除き、非ASCIIのcontinuation/assemble/well_formed判定を維持する。
ASCIIの現count1/assemble値/範囲判定の結果がleadそのものと同値であるため、公開APIや拒否集合を変えない。
一scalarを返すAPIのままとし、ASCII spanの長さを返したり、note_text側へ別decodeを作ったりしない。
同じ境界内の実装詳細なので新ADRは不要。所有・確保・schema・thread・保存方式も変更しない。

## 保持する契約

- 長さ0はbytesを読まず0。失敗時code_point不変。
- ASCII0〜127はNUL/制御/DELを含め正当で、消費1・同値code point。
- 2/3/4byteの最小値・最大値・continuation・切断・過長・surrogate・U+10FFFF超の判定は既存経路。
- note_textはgeneric decoderでNULを受理した後、自身の本文契約で拒否する。NULを見ても残りを走査し、不正UTF8を優先する。
- createの先頭BOM一個消費、editorの本文U+FEFF保持、正規化後/保護署名込み16MiB、失敗out不変と確保順を保つ。

直consumerはnote_textの検証/編集可能性、utf16_textの作成/事前計数/fill、name_list、json_reader、index_filter_buildの5TU/8callsite。
名前/JSON/本文の制御文字拒否は各ownerに残し、generic decoderの意味へ混ぜない。
既存copyは検証済み本文を複製するだけで、今回再検証を導入しない。

## 調査と実装範囲

SOLの読取報告v2と45入力、disassemblyのtool/library/元source/出力等7hashを親が照合した。
開始base`c149fbc`から調査中に文書だけのmain`7824344`へ更新された。v1の時点表記は保持し、v2で区別した。
実toolはllvm-objdump19.1.5、version/disassemblyのexit0。archive他memberのmissing-symbol warningはstderrへそのまま保持する。
元library前後hash不変。これは静的読取であり、新製品実行や新しい速度計測ではない。
原本は`D:/NeNeFolio/agents/quality-post458-text-validation/`、受理は`parent-investigation-acceptance.json`。

専用branchは`refactor/460-ascii-decode`。作業木/原本/一時出力はD側各領域の`460-ascii-decode`。
製品変更を`src/core/utf8_text.c`に限定し、`tests/unit/text_tests.c`と`folio_tests.c`で直接境界のtest/selectorを整える。
SOLが実装/自己レビュー/対象検証、親が設計/受理/文書/CI統合/通常反映/保全整理を担当する。

## 対象検証

全256先頭byteのlength1、one-past pointerの長さ0、既存多byte正常/異常、NUL+不正UTF8優先を確認した。
既存text/BOM/NUL境界を明示selectorで再利用し、coverage-negativeの名前を通常targetへ流用しない。
新確保がないため新OOM sweepを追加しない。完成候補/自己レビュー後、正典Debug sanitizer/tidyと対象testをまとめた。
実source/test/config/tool/実argv、生成exe/raw/終了値を`tools/record-run.py`へ保全した。

configure-debug-02、build-debug-01、unit-text-validation-01は各child0/wrapper0/記録error0。
正典DebugのASan/UBSan/nullability・C23/MTと、変更3TUの実tidy配線を生成metadataで確認した。
buildは`cmake --build .../canonical-debug --target folio_tests --parallel 2`の76actions。
そのexeへの`--text-validation`は既存text/NUL/sizeと追加境界を各一回実行し、stdoutは`text validation tests passed`、stderrは空。
親は実argv・成功exe・固定候補3fileと記録原本を照合し、16:04 JSTに受理した。
原本は`parent-configure02-metadata-acceptance.json`、`parent-build01-acceptance.json`、`parent-unit01-acceptance.json`。
親のtidy照合で生成Ninja変数の位置/括弧を誤想定した読取assertion2件は、実rulesと各sourceのbindingで訂正した。
製品build/testは再実行していない。

性能は#458と同じseed/fixture意味/階層観測と現Release metadataの対応を先に固定した。
親専用slotで固定Bだけを計測し、旧baselineやA全matrixは繰り返さなかった。ms閾値や速度倍率を合否条件にしていない。
旧成功helper17fileの実bytes/差分を照合し、task path/識別名/参照先とtool解決の補正に限定した。
5wrapped原sourceは同一で、候補decoderは新しい正典core libraryから結ぶ。LargeのMarkdown描画0とAnchorの初期描画を区別する。
v2計画は各段の実終了・不変入力・source/vector/env確認を条件にRelease configure/build/生成/inspect/compileを逐次実行した。
親は成功構成を照合してから専用slotを付与した。計画受理は`parent-profile-packet-acceptance-v1.json`。
Releaseのconfigure-release-01/build-release-01は各child0/wrapper0/記録error0、buildは81actionsで完了した。
configure時の既存5tool・SDK6env・RCは前回と一致し、復号元と5wrapped TUのO2/DNDEBUG/MT/C23を実metadataで確認した。
Generate/Inspect/Compile01は各0/0/error0。465/473/477入力は不変、6/2/29成果物をcurrent/snapshotで照合した。
実生成5file・16観測箇所と全compile/link引数は旧版からtask pathを置換したものと一致した。
新core libraryのdecoderが単一ownerで、元4archive memberの重複はない。実objdump/nmも各0/0/error0。
空長は入力読取前に失敗returnへ分岐し、ASCIIは0x12の消費1、0x17の出力store、0x1aのreturnへ直行する。
保全済みseed6fileを新taskへ一回コピーし、12入力不変・7成果物一致を確認した。seedは再生成していない。

| 実行したdriver/段階 | 結果 |
| --- | --- |
| `profile-release-record-v1.ps1 -Stage Configure/Build` | 各実0/0/error0、81build actions |
| `measurement-record-stage-v4.ps1 -Stage Generate/Inspect/Compile` | 各実0/0/error0、5compile/link/nm成功 |
| 実objectの`llvm-objdump --disassemble-symbols=utf8_text_decode --no-show-raw-insn`、core libの`llvm-nm --print-file-name --defined-only --extern-only` | 各実0/0/error0、実tool/argv/raw保全 |
| `measurement-probe-record-v2.ps1 -Stage Observe -Case B -Attempt measure-b-02 -RuntimeLabel b-v2 -CompileAttempt compile-profile-01` | 518入力不変/19成果物一致、実0/0/error0 |
| `measurement-audit-record-v2.ps1 -Case B -Attempt audit-b-02 -RuntimeLabel b-v2 -ObserveAttempt measure-b-02` | 540入力不変/1成果物一致、実0/0/error0 |

driverの所在は`D:/NeNeFolio/agents/460-ascii-decode/`、実出力は同taskの`outputs/`。各recordの`run.json`が完全な実argvを保持する。
runtimeの全7fonts/MSFTEDIT・OS版/processor数は旧Bと一致し、成功回の既知blockerは0。
worker/controller/runner/audit/記録wrapperは0、worker終了signalと所有process0を確認した。
親の最終受理は`parent-native-b02-acceptance-v1.json`、構成受理は`parent-release-generation-acceptance-v1.json`、
`parent-vector-acceptance-v1.json`、`parent-compile-acceptance-v1.json`、`parent-assembly-seed-acceptance-v1.json`とそのargv補足。

## 計測前停止の原本と限界

初回measure-b-01は、既知のbuild/test/probe名に一致する10PIDをpreflightで検出して計時process起動前に停止した。
runner3/wrapper125/外PowerShell1、未生成controller/worker/data-afterの記録error8。518入力は不変、計時/契約観測0。
後の読取時点では全10PIDが不在で、元の名前/argvはPID-only原本から復元できない。所属を推測せず、他processを終了していない。
現在のblocker0を確認して新runtime b-v2/measure-b-02を受理した。旧失敗runtimeと原本を保持し、runner/検査を変更していない。
原本は`measure-b-01-preflight-failure-v1.json`、判断は`parent-native-b02-decision-v1.json`。実計時の成功標本は一回だけ。

QPC値は計装・待機・preemptionを含む。OS API内経過は純I/O待ちでも、残差はCPU busyそのものでもない。
CPU事前観測は順位未保証の12件部分集合で、全OS idleを証明しない。seedのcopy/hashによるOS cacheへの影響も制御していない。
programmatic EN_CHANGEから元loop復帰までを測り、物理focus入力、後続UIA/paint/DWM完了、非ASCII性能を保証しない。
最終PR headの必須CIを統合条件にする。通常反映・全保全・安全整理の確定結果は同taskの`integration-completion.json`へ残す。
旧49削除拒否pathsと通常data/save/稼働アプリは操作しない。#458の保全確定は同taskの`integration-completion.json`を参照する。

## 初回設定生成の停止と補助scriptの訂正

configure-debug-01は、tool identity収集でGet-CommandがPython実体とWindowsApps aliasの2pathを返し、
配列を一つのコマンド名として呼んだため、CMake起動前に停止した。実child1/wrapper125、未生成identity/metadata/ZIPの記録error3。
410入力のcurrent/snapshotは一致し、canonical-debug directory未作成を確認した。製品compile/unitは未実行だった。
旧v1/失敗原本を保持し、v2は通常のコマンド解決で選ばれる先頭Applicationを一つ取得する修正と版参照だけに限定した。
親も実差分を静読し、候補3source/test・toolchain/flags・版/hash検査を維持した。原本は`parent-configure01-failure-review.json`。
性能helper照合の親補助では任意metadataを全entry必須と誤想定し、一度KeyErrorで停止した。
実schemaに合わせて訂正し、17組のold/new/patchの実hashと完全差分一致を確認した。製品実行や測定bodyの変更はない。
親読取補助のinspection metadata所在と任意command keyの想定も、実output path/argv keyへ訂正し補足へ記録した。
既に成功したsource/artifact照合を再利用し、対象製品やobjdumpを再実行していない。
