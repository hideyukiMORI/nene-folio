# 初回本文準備の読み込み区間 — #833

2026-11-15、base main `28093b7602bb40b74aafd74e82cde9f31a129b49`。
作業木D:/NeNeFolio/worktrees/833-discovery-cost、試行D:/NeNeFolio/outputs/833-discovery-cost、
恒久原本D:/NeNeFolio/agents/discovery-cost-20261115。
規則FR-047/048、ARC-001/003/007/010/011/012、C-002/005/008/013/016、QLT-008/009/010/012/013、GIT-001〜004、ADR0065/0073/0094/0095。
新waiver none、既存WVR-0001継続。

## 問いと観測の境界

[#831](2026-11-15-discovery-release.md)で、Release1000件×4KiBの初回検索中央値4.824秒のうちpost→readが4.680秒だった。
この区間の大きさだけでは、読み込みとcorpus登録・画面更新のどこに費用があるかを断定できない。
既存native fixtureの同じprivate desktopと公開検索経路へ試験側だけの観測を加える。製品・work政策・schemaは変えない。

`--trace-reads --profiles many`の初回だけ、実idleの前後とその内部のread_noteポート前後にmarkerを出す。
試験のreadラッパーは同じpersistence_adapterへ一度転送し、結果・所有権をそのまま返す。
setup・seed・初期文書・後続再検索はtraceに含めない。時刻は外側Pythonのpipe読取時刻だけを使い、Cの時計を追加しない。
初回一巡が観測済みになった後は追加markerを出さず、最初の`read`通知でmarker集合を封じる。

各idleの連番と4境界の順序、最大1read/idle、未完了/重複/別command/後着/時刻逆転を検査する。
原文一巡時に1000readが閉じたことを確認し、満たさない記録を成功集計しない。
元の結果数・最大200・byte/行位置・native preview・元文書・合成source bytes/hash/日時の照合を維持する。

read区間はpath生成/ファイル読込/UTF-8検証/note_text所有/試験転送とmarker処理を含む。
idle内の残りにはcorpus登録/配置/同期描画/通知などが入る。idle間のqueue処理はその和に含めない。
したがってreadを純粋なdisk時間、残りを純粋なpaint時間とは呼ばない。pipe/log書込み・外側threadの配送遅延も残る。
同じReleaseの無計装/計装を各3process測り、観測を加えた負荷も併記する。異なるprocess/時点なので厳密な同期A/Bではない。
受信時刻の差は実行区間そのものではなく、滞留/配送で隣接区間へ時間が偏り得る。
無計装との差が小さくてもこの偏りは校正できないため、ArrivalMsを実時間の割合へ変換せず、最適化対象を断定しない。

## 全試行と状態

調査で仮定したdiscovery_window.c/win32_disk.c/0073-public-idle-continuation.md/tools/README.mdは存在しなかった。
実ファイル検索でfolio_window.c/persistence_adapter.c/ADR0073-bounded-index-matchingを確認し、操作の説明は本品質記録へ集約した。
GitHub connectorのIssue作成は403で外部変更なし。認証済みghで#833を作成した。記録は恒久原本に保持する。

自己レビューと独立読取レビューを実施し、P1/P2相当の問題なし。上記の配送による時間偏りを記載した。
configure-01は正典Release/File API生成成功、trace-parser-01は8つの境界試験成功。どちらもchild/wrapper0、recordingErrors=[]。
build-01はtags_native_tests/motion_native_testsの対象build成功。source-check-01は変更Cの整形と実graphを含む規約違反0、空白検査成功。
どちらもchild/wrapper0、recordingErrors=[]。構成生成と検査中に結果file未生成の読取を試したが、後の正常な完了記録で確定した。

baseline-01は無計装1000件の2標本・10操作が完了した後、3標本目のsetup中に中止した。
setupが77.295秒/103.447秒と大きく変動し、22:50:56の短いprocess CPU観測で別案件のclang-cl/clang-tidyが同時にCPUを使っていた。
ADR0073の性能計時の環境条件に従い、正確なexe pathと親driverを照合した今回専用fixtureだけを停止した。
外側はstartupでEOFを受けて失敗し、child/wrapper1、recordingErrors=[]。他案件のprocessは停止していない。
中止と全rawは恒久baseline-01-abort.jsonと試行directoryに保持する。完了した2標本も性能の成功集計へ入れない。
負荷が収まってから新しいruntime/run-dirで性能計測だけをやり直す。不変のbuild・parser・規約検査は再利用する。

baseline-02はcompiler processがないことを間隔を空けて2回確認して再開したが、1標本・5操作完了後に別のコンパイルが再開した。
2標本目のsetup中に同じ専用fixture照合で停止した。child/wrapper1、recordingErrors=[]、全値を性能集計から除外。
起動前の観測だけでは後続負荷を除けないため、以後は数分連続の低負荷を確認してから新試行を始め、途中も確認する。
baseline-02-environment/abort/disposition.jsonと全rawを保持し、失敗の痕跡を上書きしない。

23:03時点ではbaseline-03は未起動。compiler不在を3分観測した後も、23:03:54の1秒観測で別案件のテストがCPU約1.078秒を使っていたため延期した。
23:09以降は30秒間隔の1秒CPU観測をquiet-cpu-observations.jsonlへ記録し、compiler以外のテスト負荷も確認している。
これは観測点の記録で、専用端末や間の無負荷を保証しない。他案件のprocessは操作しない。
待ち時間には文書参照確認・完了記録/保全/整理の手順を準備し、未実行を成功とは扱っていない。
23:15:28にも別案件のテストが1秒観測でCPU約1.016秒を使っていた。連続する3ターンで同じ外部条件待ちを確認した。
今回専用のpwsh監視をpath/引数/親PIDで照合して23:15:45に終了した（monitor-stopped.json）。
外側sessionの終了値1は意図した監視停止であり、検証の失敗・成功へ数えない。性能計測は未再開、他案件は操作していない。

hideの23:42の確認依頼後、3秒観測で既知のcompiler/testは0件となり、その後約5分の観測でも重い別案件の処理を認めなかった。
23:47:01にはDockerが1秒でCPU0.703125秒の短い上昇を示したが、後続の3秒観測では0.15秒の報告閾値未満だった。
専用端末ではなく通常の背景処理がある環境で、継続的な別案件build/test負荷が見られないことを再開判断にした。
測定入力15点のbytes/hash不変を確認し、成功済み検証を再利用。23:48に新しいbaseline-03/runtime-baseline-03で再開した。
再開前の観測はresume-load-sample.json、入力照合はresume-input-verification.json、判断はbaseline-03-start-environment.json。

baseline-03 / trace-01は各3process・15操作が成功。いずれもchild/wrapper0、recordingErrors=[]。
初回から3標本の合成source bytes/hash/作成・更新日時を照合し、結果数/最大200/原文位置/native preview/元文書の既存assertionを保った。
traceは各processで1000idle・1000read、計4000markerが閉じた。後続4操作にはtraceを出さない。
measurement-environment.jsonlの途中/境界観測でも既知の別案件compiler/testを認めなかった。観測間の無負荷は保証しない。
集計確認の一行Pythonは一度summary行へmeasurement用keyを要求してKeyError、別の結果一覧表示はliteralの改行escapeでSyntaxErrorとなった。
どちらも測定終了後/記録読取側の失敗で、測定を再実行せず対象phaseを限定した読取とPowerShellで記録を確認した。

motion-live-01もchild/wrapper0、recordingErrors=[]、native162項目が成功。
実pumpの静止20.0006秒で追加tick0・主thread CPU0ms。演出ON/OFF各7標本で2検索の意味を確認し、
35msの配送遅延8回でも65idleを進めて1件の結果が完成した（429.848ms、保証値ではない）。
変更した共有loopの確認に限定し、既存3言語/3配色/全寸法の画像や無関係な機能を再実行していない。

## 完了した測定

1000件×4096byte、2カテゴリ。各列は独立した3processのmin / median / max、単位ms。
setupはfixture生成を含むので検索時間に加えない。OS cacheは消去していない。

| 操作 | 無計装 | read/idleを観測 |
| --- | --- | --- |
| 初回本文・一意語 | 4588.195 / 4617.959 / 4683.687 | 4606.146 / 4799.925 / 4866.629 |
| 同じcorpus・不一致語 | 134.546 / 134.792 / 136.493 | 127.868 / 128.915 / 129.190 |
| 同じcorpus・全件一致語 | 429.540 / 439.979 / 447.073 | 421.021 / 423.712 / 427.410 |
| 同じcorpus・一意語 | 141.622 / 143.502 / 145.865 | 138.192 / 138.531 / 146.392 |
| 新しく開く名前検索 | 40.719 / 41.870 / 45.620 | 40.100 / 44.163 / 49.888 |
| setup | 45560.226 / 47472.171 / 48892.152 | 46404.392 / 46944.706 / 47813.052 |
| 初回post→read通知 | 4449.474 / 4482.898 / 4547.235 | 4467.995 / 4665.991 / 4729.548 |

初回の中央値差は+181.966ms。別process・別時点のため、この差全体をmarkerの純粋な費用とは呼ばない。
合成messageの最大往復は無計装17.841ms / 計装31.867ms。物理入力遅延や上限の保証ではない。

初回1000idleの受信区間合計（ms）。同じ行ではreadと残りの和がidleとなる。

| process | idle内合計 | readポート内 | read以外のidle内 |
| --- | --- | --- | --- |
| 0 | 4513.199 | 69.139 | 4444.060 |
| 1 | 4536.890 | 53.793 | 4483.097 |
| 2 | 4345.579 | 46.475 | 4299.104 |

readポートの受信区間合計は中央値53.793ms、残りは4444.060ms。
配送の偏りを校正していないため、実行時間の割合へ変換しない。read以外の区間を次の優先調査候補とするが、
corpus登録・配置・同期描画のどれが主因かは未特定。純粋paint/IOへの断定や製品最適化の受理根拠にはしない。
コード読取では各idleで配置/全体再描画を呼ぶ。後続で省略を検討する場合も、source進捗/失敗/UIA、
明示preview切替/tag cursor、resize/DPI/言語/配色、motionのidle再開を保持し、revisionだけで全refreshを短絡しない。
製品側の変更は別Issueとし、このIssueでは製品/API/work/schemaを変更していない。

全raw・入力・各source snapshotは対応run/runtimeに、集計とraw SHA256は恒久measurement-summary.jsonに保存した。

## コマンドと残る統合

各実行は`tools/record-run.py`の新run-dirで包み、上記の成功/中止を区別した。

```text
pwsh -NoProfile -File D:/NeNeFolio/agents/discovery-cost-20261115/configure.ps1
pwsh -NoProfile -File D:/NeNeFolio/agents/discovery-cost-20261115/build.ps1
python -B -m unittest discover -s tests/conformance -p test_discovery_trace.py -v
pwsh -NoProfile -File D:/NeNeFolio/agents/discovery-cost-20261115/verify-source.ps1
python -B tools/measure-discovery-scale.py <tags_native_tests.exe> <new-runtime> --profiles many --samples 3
python -B tools/measure-discovery-scale.py <tags_native_tests.exe> <new-runtime> --profiles many --samples 3 --trace-reads
python -B tools/measure-discovery-motion.py <copied-motion_native_tests.exe>
```

文書の空白・相対参照・製品入力不変の最終確認はdocuments-01の記録を正本とする。
最終headの必須CI/統合、全試行の保全/整理は未完了。
通常exeは製品入力不変を確認して反映を再利用する予定。確定原本は恒久completion.json、未作成を完了とは扱わない。
