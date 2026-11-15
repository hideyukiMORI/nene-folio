# Releaseでの検索費用 — #831

2026-11-15、base main `1ec50d893a0242e4963b64e330cd39e9bab0fa71`。
作業木D:/NeNeFolio/worktrees/831-discovery-release、試行出力D:/NeNeFolio/outputs/831-discovery-release、
恒久原本D:/NeNeFolio/agents/discovery-release-20261115。
規則FR-047/048、ARC-001/002/003/007/010/011/012、QLT-008/009/010/012/013、GIT-001〜004、ADR0073/0094/0095。
新waiver none、既存WVR-0001継続。

## 問いと条件

[#829のDebug計測](2026-11-15-discovery-scale.md)で、1000件の初回検索と長い本文の全件一致に数秒かかった。
先回りして製品を変更せず、同じfixture/外側driverを最適化されたRelease構成で測り、残る区間を確認する。
製品・test・driver・品質ゲートは不変。追加するのは実行記録と文書だけである。

`eng/toolchain.ps1`とCMake/NinjaのRelease構成で既存`tags_native_tests`だけをbuildした。
対象のcompile commandは`tags-build-binding.json`、全入力と生成exeはconfigure-01/build-01に保全する。
clang-cl、C23、`/O2 /Ob2 /DNDEBUG -MT`、exe SHA256
`aaa40d6094d503c1fc8307bfed5928077945b9a46b14b34c39e2a65a4fdff684`。
ReleaseにはDebugのsanitizer費用が含まれない。厳密な同時A/Bやコンパイラ最適化だけの効果検証ではない。

Windows 11 Pro 10.0.26300、Intel Core i9-10850K（20 logical processors）、約64GiB memory。
private desktopの1050×720窓、motion OFF、合成ASCII本文、2カテゴリ、100件×4096 bytes・1000件×4096 bytes・32件×1048576 bytes。
各構成3個の新規processで初回本文・一致なし・全件一致・一意一致・名前検索を順に行う。
生成/原文照合/初期表示を含むsetupは検索区間の外。OS cacheを消去せず、setupでもcacheが温まる。
既存driverが保存原文のbytes/hash/作成更新日時を反復後に照合する。実dataと通常アプリは使わない。

phaseは外側pipe読取時刻。post→readは原文準備と同期UI、read→matchedは照合/位置計算とUI、
matched→resultはpreview・queue・期待値照合・通知を含み、純粋なdisk/matcher/paintの個別費用ではない。
CPUはresult後の未回収pingまでを含み、短い区間では量子化もある。物理keyboard/IME入力や全端末の上限は測っていない。

## 全試行

- configure-01: 正典toolchainとRelease configure、File API query生成に成功。child/wrapper0、recordingErrors=[]。
- build-01: `cmake --build build-release --target tags_native_tests`成功。child/wrapper0、recordingErrors=[]。
- build-binding.jsonは同じsourceを使う別targetのcommandを抽出していたため、実行targetの証明に使わない。
  commandのtarget名も照合したtags-build-binding.jsonを測定入力へ追加した。exe自体とbuild条件は変えていない。

- scale-release-01: `python -B tools/measure-discovery-scale.py <Release tags_native_tests.exe> <runtime-release-01> --samples 3`
  成功。3構成×3新規processの45操作が完了し、件数/最大200/byteと行/native preview/元文書保持を照合した。
  反復後の合成sourceのpath/bytes/hash/作成更新日時は一致。child/wrapper0、recordingErrors=[]。
  全rawはscale-release-01/runtime-release-01、集計は恒久measurement-summary.json、環境はenvironment.json。
- documents-01: 文書の空白・73個の相対参照先・製品/test/tool入力不変を確認。
  child/wrapper0、recordingErrors=[]。結果の一行追記だけでは製品検証を繰り返さない。

## 観測値

各値は3標本の **min / median / max、単位ms**。postから検証済みresultまでを測る。

| 構成 | 初回本文・一意一致 | 再検索・一致なし | 再検索・全件一致 | 再検索・一意一致 | 新sessionの名前検索 |
| --- | --- | --- | --- | --- | --- |
| 100件×4KiB | 504.459 / 511.150 / 512.651 | 17.168 / 20.253 / 20.713 | 57.601 / 59.979 / 74.182 | 30.059 / 32.146 / 39.236 | 44.272 / 46.542 / 53.893 |
| 1000件×4KiB | 4289.521 / 4823.575 / 5796.920 | 115.576 / 125.526 / 147.360 | 410.584 / 460.868 / 567.901 | 135.117 / 139.866 / 151.614 | 42.345 / 56.481 / 61.001 |
| 32件×1MiB | 1234.291 / 1249.694 / 1332.264 | 928.058 / 950.781 / 1017.559 | 3211.786 / 3274.946 / 3279.290 | 997.095 / 1038.299 / 1051.222 | 46.033 / 46.625 / 49.740 |

| 構成 | post→read | read→matched | matched→検証済みresult | 全操作中のping最大 |
| --- | --- | --- | --- | --- |
| 100件×4KiB | 479.579 / 485.977 / 488.623 | 13.498 / 13.638 / 14.445 | 10.531 / 10.728 / 11.242 | 22.934 |
| 1000件×4KiB | 4156.151 / 4679.803 / 5662.604 | 122.799 / 123.124 / 133.273 | 10.500 / 10.571 / 11.192 | 25.883 |
| 32件×1MiB | 210.101 / 215.969 / 218.803 | 1004.879 / 1029.580 / 1103.744 | 10.013 / 10.610 / 12.552 | 21.477 |

独立列の中央値の和は合計時間の中央値とは限らない。
setupは100件4670.249/4758.163/5044.057ms、1000件45882.326/47812.593/50231.036ms、
長い本文32件2179.775/2416.193/2419.561ms。fixtureによる生成/確認を含むため、製品の起動時間と解釈しない。

## 判断と次の切り分け

1000件の初回中央値はDebug5182.360msに対しRelease4823.575msで、依然として数秒かかる。
Releaseでもpost→readの中央値4679.803msが大きく、先に調べる対象は本文準備からUI反映までの経路である。
1件/idleのreadを無条件に増やす、別のcorpus/workerを作る、保存の検査を省く、といった変更はこの測定からは正当化できない。
次は既存read/prepare/syncの呼出しと費用を狭く切り分け、原文観測・取消・message応答を保つ受入条件を決める。

長い本文の全件一致中央値はDebug6488.023msに対しRelease3274.946ms。
同じReleaseの一意一致1038.299msより長く、全一致の位置完成を含む区間も改善候補として残る。
これは別時間の3標本ずつの条件付き観測であり、個別関数の割合や恒常的な高速化率を断定しない。
matcher/位置計数の変更は既存有限workと共通UTF-8/UTF-16投影を守る別の焦点Issueで扱う。

最終CI/統合・恒久保全/整理は未完了。最終head CIと統合はIssue/PR、確定原本は
D:/NeNeFolio/agents/discovery-release-20261115/completion.json。未作成を完了とは扱わない。
製品入力が不変なので通常exeの再buildを予定しない。物理IME/読み上げ、他OS/DPI、cold OS cache、Unicode主体の大量本文は未確認。

## 統合後の確定結果

#831 / PR832は最終head `e46ef18a134295fca8d05cca6301b3070b12e7a5`の必須CI43718941842成功後、
main `28093b7602bb40b74aafd74e82cde9f31a129b49`へ統合した。56 CTest、分岐10066/11090（90.766456%）。
製品入力/通常exe不変を確認し、既存反映を再利用した。全4,445ファイルを保全・照合し、今回追加2領域を整理済み。
確定原本はD:/NeNeFolio/agents/discovery-release-20261115/completion.json。branch/commitは保持。
GitHub connectorのPR作成は403で外部変更なし。認証済みghで作成し、正典merge-pr.ps1で統合した。
