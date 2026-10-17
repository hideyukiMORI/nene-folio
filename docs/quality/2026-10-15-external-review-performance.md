# 外部講評P2 — 1万本と数MiB本文の計測

2026-10-15、製品変更なしの測定。基点main e7d5259c（#309/#310修正前）をsource/lib/exeで凍結。
7 workerがexit 0、1493 samples/60 groups。恒久証跡はD:/NeNeFolio/agents/external-review-perf-20261015/。
report.txt、results.json、各CSV、method.json、source-and-libraries.json、実行source/exe、固定libs/fontとfixture ZIPを保持。
専用outputsはarchive照合・process/link確認後に整理済み。

Windows 11 Pro 10.0.26300、i9-10850K、64GiB、D側NTFS、120 DPI。
clang-cl 19.1.5、Debug /Od /Ob0 /MTとASan/UBSan/nullability。
100カテゴリ×10または100ノート、本文1本10240 UTF-8 bytesの決定的ASCII fixture。
warmは1回準備＋30回、初回は3 fresh process。cold OS cacheではない。
値は中央値 / nearest-rank p95 / 最大、単位ms。

| 区間 | 1000本 | 1万本 |
| --- | ---: | ---: |
| 初回本文取得込み検索 z（n=3） | 631 / 650 / 650 | 5915 / 8202 / 8202 |
| 後続検索 z（不一致） | 81.4 / 91.8 / 94.2 | 2215 / 3109 / 3382 |
| 後続検索 alpha（全件一致） | 11.1 / 14.6 / 15.2 | 1750 / 2218 / 3039 |
| 後続検索 aaaaab（密なprefix・不一致） | 164 / 196 / 214 | 3191 / 5433 / 6386 |
| entry組立のみ | 10.7 / 13.5 / 14.5 | 1453 / 1874 / 1932 |
| 比較のみ z | 67.9 / 76.6 / 80.3 | 592 / 643 / 676 |
| native入力→検索→描画 z | 91.7 / 98.1 / 99.2 | 1484 / 1657 / 1671 |
| drawer layout | 0.32 / 0.44 / 0.48 | 2.21 / 3.81 / 3.91 |
| drawer実WM_PAINT | 45.9 / 52.5 / 53.4 | 256 / 284 / 298 |
| 対象note台帳複製（10/100本） | 0.044 / 0.050 / 0.070 | 0.356 / 0.588 / 0.608 |
| 台帳port保存（serialize/flush/publish） | 38.4 / 51.4 / 51.8 | 270 / 499 / 553 |

| 本文・入力→描画 | 番号OFF | 番号ON |
| --- | ---: | ---: |
| 1MiB短行 | 1.89 / 3.37 / 3.51 | 9.51 / 13.20 / 14.50 |
| 4MiB短行 | 2.12 / 3.09 / 3.47 | 30.19 / 37.56 / 39.07 |
| 4MiB長行折返し | 2.90 / 4.56 / 4.57 | 18.18 / 20.93 / 21.42 |

4MiB短行のline_index再構築は16.73 / 20.40 / 22.61ms、cache再利用は0.0003 / 0.0006 / 0.0010ms。
raw全文取得は0.710 / 1.517 / 2.797ms。取得だけを全遅延の原因とはできない。
6入力条件で31文字増分・末尾選択・Undo可能・dirtyを確認し、fixtureの明示破棄後に全md hash一致。
台帳/settingsは測定した実保存による変更を記録。未保存Undoの全履歴やviewport回帰の検証ではない。

この条件の1万本では検索・drawer描画の待ち時間を実測できた。entry組立の伸びと線形lookupを優先調査候補とするが、
最適化比較やReleaseの遅さの証明ではない。台帳複製と保存portを分け、番号ONの再構築負担とcache再利用も区別する。
性能改善は未実装。次回は結果から焦点と受入条件を選ぶ。

共有ホストで並行compiler/clang-tidy等があり、phaseは別時点で測定。独立中央値を足してend-to-end値にしない。
2026-09-17の旧値はsource/toolchain/fixtureが一致せず回帰比較不可。
外部probeのnullability警告104件、全workerのASan interception_win起動診断を保持。
global foreground HWND変化を3回観測。専用desktopにSwitchDesktop権限/呼出はなく原因未確定。
最初のlauncherは変化で停止、成功済測定は繰り返さず残りだけ実行した。
メモリは区間末サンプル、GDI値は前後観測、描画完了はGdiFlushまでで、leak・連続peak・画面presentの証明ではない。
実OS/DPI/全テーマ、ランダムな実ノート分布、drawer scroll、fontのOS IOは未測定。Waivers: none。
