# 重なり修正後のRelease生成時間 — #569

#563のsequence枠見出し、#565のmessage背景塗り、#567のflow label描画順で直接変わった4入力を比較した。
旧/現行の交互6組は12実行・48画像の照合に成功。5参加者messageだけ6組中5組で増加したため、その1入力を追加6組測った。
追加も12実行・12画像成功。増加傾向を隠さず残す一方、高いhost負荷下の小差から製品起因の持続した遅化を確定せず、性能修正は追加しない。
速度向上・差なし・UI応答時間の保証も行わない。

## 入力・build・計測境界

現行sourceは統合main`be19334f0601565fa95147348bcdbeda31cb0134`、tree`c089196f4a642edcde254aad31803d6ade373e9d`。
旧版は[#559](2026-11-04-mermaid-performance.md)のsource`87251022dc37434fa63f82676b16779c75e752a3`で成功したRelease benchmark exeを使った。
製品差分はflow/sequence layoutの2ファイルだけ。fonts、固定toolchain、canvas、metricsは不変である。
旧保全ZIPのSHA256をmanifestと照合し、成功measure-01の入力に記録されたexe SHA256
`3c3d5d6e24eeffff47684a23fd577f177329ab8008ad52b8b5f15af1c4381618`（269824bytes）と一致する8ファイルを抽出した。
旧/現行のfonts7ファイルも全hash一致。記録は`prior-files.json`、sourceは`source-context.json`へ収載した。

| 入力 | 原文の正本 | 寸法（両build） |
| --- | --- | --- |
| sequence-frame.mmd | #563 frame-heading-2-0、activationをまたぐ枠見出し | 282×455 |
| sequence-message.mmd | #565 message-label-3-0、5参加者・番号・二重activation | 560×287 |
| flow-branch.mmd | #567 flow-label-0-0-0、TBの分岐 | 515×332 |
| flow-return.mmd | #567 flow-label-1-3-0、RLの戻り/並行/非表示辺 | 604×367 |

現行native試験のC文字列を抽出して原文を作り、`inputs.json`へbytes/hash・元関数/添字・寸法・旧red/現行greenの期待bitmap hashを保存した。
原文を抽出したファイル自体も`fixture-source.c`として保持する。すべて明themeの測定である。
旧24枚/現行24枚（追加は各6枚）を、それぞれ対応する修正前/修正後のbitmapと全SHA256照合した。
旧と現行を同じbitmapだとは扱わず、意図した可読性の差を保っている。現行4画像も目視で確認した。

固定CMake Releaseの`diagram_render_tests` targetをparallel1でbuildし、その製品3libraryへ外部harnessを/O2・/MTでlinkした。
`benchmark-native.c`は#559からinclude先だけを変更し、計時コードは全一致を確認した。
QPCで`diagram_render_read`の解析/font作成・計測/layout・2倍GDI描画/縮小/BMP所有までを測り、cache copyは別に測る。
各入力は生成21回・copy21回で、初回を別記し、続く20回の中央値を使う。bitmap保存は最後のcopy計時の後。
process起動、bundle font登録、worker待ち、UI timer、原文読み込み、bitmap保存は生成時間に含めない。

## 最初の4入力比較 — paired-01

旧→現行、現行→旧を組ごとに交互へ替えた6組。各processは上表の同じ4入力を同順で処理する。
下の生成値は「各processの初回を除く20回中央値」を6個集めた中央値。
組ごとの差率の中央値は別集計であり、旧/現行の集計値どうしを単純に割った値ではない。

| 入力 | 旧生成 ms | 現行生成 ms | 組ごとの差率中央値 | 現行が遅い組 | 現行cache copy ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 枠見出し | 2.504 | 2.430 | −6.56% | 2/6 | 0.02595 |
| 5参加者message | 3.482 | 3.594 | +3.79% | 5/6 | 0.03440 |
| TB分岐 | 2.938 | 2.999 | −0.01% | 3/6 | 0.03697 |
| RL戻り/並行 | 3.556 | 3.433 | −3.69% | 1/6 | 0.04350 |

process全体に対するOS CPU busyは24.24〜65.14%。process wallは0.606〜18.701秒で揺れている。
このwallには計時境界外の起動/登録/出力等が含まれ、生成中央値やUI時間へ読み替えない。
個々の初回、全20sample、phase、cache、順序、CPU前後値とwallはraw stdoutおよび`paired-01-summary.json`に残す。
初回生成の最大観測値は108.776ms、初回を除く最大は8.244ms、cache copy最大は0.1983msだった。

## messageだけの追加比較 — paired-02

初回の増加傾向を判断するため、同じexe・原文・21回の境界でmessageだけを交互6組測った。
この回は単独入力なのでprocess内の最初の図となり、初回を別記する。初回4入力の成功結果は置き換えない。
両runを合計すると生成1260回・cache copy1260回で、初回も全rawに保持している。

旧生成は6.056ms、現行6.107ms、集計値の差は+0.051ms。組ごとの差率中央値は+0.75%、現行が遅い組は再び5/6だった。
OS CPU busyは98.81〜100.00%へ上がり、両版の絶対時間も初回の約1.7倍になった。
同じ条件へ補正した差や、他processを制御した測定とは扱わない。
追加回の初回生成の最大観測値は239.225ms、初回を除く最大は32.536ms、cache copy最大は7.029msだった。
これらを外れ値として削除せず、全sampleの件数・最大値の所在を`raw-audit.json`へ収載した。最悪時間の保証値ではない。

phaseの中央値差を確認すると、初回はrasterが+0.0805ms、measure/layoutが+0.0153ms、追加回はそれぞれ+0.0139ms/+0.0540msだった。
各phaseの中央値は個別の集計なので、足して生成中央値の差になるとは限らない。各sampleのphase和は計時総量と照合した。
主要な差の段階が一定でなく、変更していない計測処理を含む段階も揺れている。背景塗りの追加などによる小さな費用は否定せず、
今回の値だけで原因を特定しない。可読性修正を維持し、ゲートや描画経路を変える追加最適化は行わない。

CPUはGetSystemTimesによるprocess実行前後のOS集計値で、個々のrenderのCPU使用率ではない。
64論理CPU以下は全processor、それを超える場合は呼出threadのprimary groupという[公式APIの範囲](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes)を記録した。
affinityや他processの負荷は制御していない。この参照・範囲はsummaryにも保持する。

## 実行記録と判断

作業先は`D:/NeNeFolio/worktrees/569-layering-performance`、出力は同名`outputs/`、恒久原本は同名`agents/`。
`tools/record-run.py`で入力・argv・raw・終了値・exe/bitmap/summaryを保存した。

| run | 操作 | 結果 |
| --- | --- | --- |
| build-release-01 | 固定CMake Release、`cmake --build <release-build> --target diagram_render_tests --parallel 1` | 子/記録側0 |
| link-01 | #559と同じ計時harnessを現行製品libraryへlink | 子/記録側0 |
| paired-01 | 4入力×旧/現行×交互6組 | 全12process終了0、48bitmap一致、子/記録側0 |
| paired-02 | messageのみ×旧/現行×交互6組 | 全12process終了0、12bitmap一致、子/記録側0 |

全runのrecordingErrorsとstderrは空。全sampleの正有限値・21件ずつの件数・phase和・寸法も確認した。
入力記録が一時遅れたが進行を確認し、自分のbuild・通常反映・保全/整理が終わってから測定した。
追加比較は入力selectorだけを外部scriptへ足し、harness/exe/製品は再buildしない。初回scriptのbytesも最初のrunのsnapshotに保持する。
各試行の失敗/判断は全体journalに残し、未知の値を成功へ読み替えない。

製品コード、schema、版、依存、ADR、ゲートは変更しない。文書だけを対象検査し、最終head必須CIへ進める。
単体で計測・自己レビュー・検証を行い、追加の独立レビューは使わない。
規則: ARC-001/003/004/008/012、QLT-008/010/012/013、GIT-001〜004、ADR0089。追加waiverはnone、既存WVR-0001はWinHTTP callbackだけで継続。
通常exeは#567の反映をhash照合して再利用する。実app/実ノートの試験操作、Store提出/公開は行わない。
最終CI・統合・保全整理・通常exe照合の確定値は`D:/NeNeFolio/agents/569-layering-performance/integration-completion.json`へ収載する。
