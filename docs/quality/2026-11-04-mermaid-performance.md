# 品質修正後のMermaid生成時間 — #559

11月4日の形状・辺接続・生命線の修正を統合したmain`87251022dc37434fa63f82676b16779c75e752a3`で、
既存の生成経路を測定した。製品code・API・schema・版・依存・ゲートは変更しない。
初回の6例はcacheなしの後続20回中央値が2.470〜5.480ms、cacheコピー中央値が0.018〜0.280msだった。
昨日より小さいflowの中央値が高かったため、保全済み旧exeとの交互比較も行った。
現行だけが一貫して遅くなる結果は再現せず、性能修正を追加する根拠にはしない。

## 入力・ビルド・測定範囲

作業先は`D:/NeNeFolio/worktrees/559-mermaid-performance`、出力は同名`outputs/`、恒久原本は同名`agents/`。
11月3日の保全ZIPから旧3入力を元bytesで抽出し、`benchmark-after-design.json`のSHA256と照合した。
追加3入力は#553の8行hexagon、#554のLR/parallelogramのforward/return/self、#557の混在見出しである。
原文・由来・hash・既存bitmapの期待hashは`inputs.json`、実入力は保全ZIP内の`outputs/inputs/`に置く。

固定toolchain（LLVM19.1.5、MSVC14.44.35207、CMake3.31.6-msvc6、Ninja1.12.1）で
CMake Releaseの`diagram_render_tests`と依存libraryだけを並列数1でbuildした。
旧外部`benchmark-native.c`を再利用し、同じRelease libraryへ`/O2 /MT`でlinkする。
変更したharnessはinclude先、出力先引数、最後のcacheコピー計時後のbitmap保存だけ。
製品へ計測APIや別rendererを追加せず、`diagram_render_read`と`image_cache_get`を直接使う。

計時はQPCで、解析/font生成/実計測/layout/GDI2倍描画/平均縮小/BMP所有と関数終了まで。
phaseは既存のprogress境界で分け、raster欄には終了時のscene/metrics/canvas/model解放も含む。
process起動、同梱fontの登録、worker queue、UI timer、bitmap保存は個々の計時へ入れない。
各入力で初回を分離し、後続20回のcacheなし中央値を採る。cacheコピーも初回と後続20回を分離する。
phaseごとの中央値の和は総時間の中央値と必ずしも一致しない。

環境はWindows11 build26300、Python3.12.10、論理CPU20。同じ同梱Noto Sans JP regularを使った。
CPU負荷は`GetSystemTimes`をprocess全体の前後で観測した値で、個々のrenderのCPU使用率ではない。
64CPU以下では全processor、超過時は呼出threadのprimary groupという
[Microsoftの仕様](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes)に従う。
他の作業負荷、CPU affinity、clockは制御していない。raw値と範囲は`environment.json`へ保存した。

## 10:24の6例

`measure-01`は10:24:14〜15、process全体0.810秒、観測CPU負荷32.44%。各21sampleを保持する。

| 入力 | 初回ms | 後続20回中央値ms | 後続の最小〜最大ms | cacheコピー中央値ms | 出力px |
| --- | ---: | ---: | ---: | ---: | --- |
| flow-small | 13.183 | 3.067 | 2.386〜3.666 | 0.031 | 309×541 |
| flow-group | 5.758 | 5.409 | 4.862〜7.167 | 0.280 | 1228×423 |
| sequence | 12.420 | 5.480 | 4.820〜7.010 | 0.229 | 520×724 |
| polygon-eight | 8.645 | 2.777 | 2.531〜4.725 | 0.018 | 384×276 |
| flow-attachment | 18.961 | 2.470 | 2.256〜3.571 | 0.034 | 804×222 |
| sequence-lifeline | 86.362 | 2.692 | 2.596〜3.918 | 0.032 | 360×465 |

初回86.362msも除外せず、中央値で隠さない。初回はprocess起動時間そのものではなく、当該入力の最初の生成呼出し。
追加3bitmapはそれぞれ#553/#554/#557の成功時のSHA256と完全一致した。6枚すべてを視認し、
多行ラベルの余白、斜辺への接続、混在見出しからの生命線を確認した。旧3入力の寸法は昨日と同じ。
解析/font、計測/layout、rasterの各中央値と入力・bitmap hashは`summary.json`に保持する。

## 小さいflowの差を切り分けた交互比較

昨日のflow-small中央値2.410msに対し最初の測定は3.067ms（+27.27%）だった。
時刻と負荷の違う測定から製品の退行と決めず、同じホストで旧版と現行版を交互に測定した。
追加実行の理由はこの未解決の差であり、成功済み機能テストの反復ではない（QLT-012）。

旧Release exeは昨日の成功run`benchmark-native-release-02`の入力hash
`296a376e63504f05a6584c1288ccf75b6fd1c57c7bf0092e730764c63616a946`と一致。
同梱fonts一式7ファイルも現行版と全hash一致した。原本は`prior-files.json`。
各processで旧3入力を順に各21回測る。6組の実行順は旧→現行、現行→旧を交互にし、全12processを保持する。
各processの初回を分けた20回中央値を求め、次の表ではその6個の中央値を示す。

| 入力 | 旧版ms | 現行版ms | 各組の増減率の中央値 | 各組の増減率の範囲 |
| --- | ---: | ---: | ---: | ---: |
| flow-small | 3.513 | 3.271 | -6.20% | -26.60〜+34.25% |
| flow-group | 8.063 | 6.441 | -15.27% | -27.45〜+35.58% |
| sequence | 7.752 | 6.930 | -14.03% | -27.68〜+30.40% |

各組の増減率の中央値は、表の時間の比から計算する値とは異なる。
process全体の観測CPU負荷は52.24〜78.53%で、単発測定より高く、組ごとの時間の揺れも大きかった。
各入力で現行版の時間が旧版を上回った組は6組中1組。現行だけが一貫して遅い結果は再現しなかったが、
負荷を統制していない6組から速度向上や差がないことの統計的保証はしない。性能変更は追加しない。
現行版が保存した18bitmapは`measure-01`と全hash一致。旧harnessにはbitmap保存がないが、
現行harnessの保存は各入力の最後のcacheコピー計時後で、個々の測定境界は同じである。
全sample、順序、process時刻/負荷、argvと終了値は`paired-01/stdout.log`、集計は`paired-summary.json`へ。

## 実行記録・完了条件

各対象runは`tools/record-run.py`でsource/設定/exe/font/入力のsnapshot、raw出力、終了値を記録した。

| run | コマンド・用途 | 結果 |
| --- | --- | --- |
| build-release-01 | 固定toolchainで`cmake -S . -B <release-build> -G Ninja -DCMAKE_BUILD_TYPE=Release`、`cmake --build <release-build> --target diagram_render_tests --parallel 1` | 子/記録側0 |
| link-01 | `link-benchmark.ps1`、外部harnessを同じRelease libraryへlink | 子/記録側0 |
| measure-01 | `measure.py`→`diagram_benchmark.exe <bitmaps> <6入力>` | 各21生成/21cache sample、6bitmap保存、子/記録側0 |
| paired-01 | `paired-measure.py`、旧/現行の交互6組、旧3入力だけ | 12processすべて0、現行18bitmap一致、子/記録側0 |

全runのrecordingErrorsとstderrは空。集計scriptはsample数、有限値、寸法、入力hash、対象bitmap hashを検査した。
別時点の増加も含めた全試行を残し、良い数字だけを選び直していない。JSの再測定や一般的な速度倍率は主張しない。
6つの限られた入力の測定であり、最悪時間、全OS/DPI/書体/任意図の保証、hideの実機受理ではない。

主担当が単体で準備・測定・画像確認・文書化した。独立レビューの追加はなし。
規則: ARC-001/003/004/008/012、C-008/014/016、QLT-008/010/011/012/013、GIT-001〜004、ADR0089。
追加waiverはnone、既存WVR-0001はWinHTTP callback限定で継続。実app/実ノートの操作は行わない。
最終head必須CI、正典統合、恒久保全・整理の確定値は
`D:/NeNeFolio/agents/559-mermaid-performance/integration-completion.json`へ収載する。
製品入力が不変の文書PRなので、通常exeは#557の反映原本を参照し、再buildしない。
