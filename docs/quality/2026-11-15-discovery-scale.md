# 検索の規模別計測 — #829

2026-11-15、base main 8e1ef719247db68acca5e82bf95f3a9c66ec507b。
作業木D:/NeNeFolio/worktrees/829-discovery-scale、試行出力D:/NeNeFolio/outputs/829-discovery-scale、
恒久原本D:/NeNeFolio/agents/discovery-scale-20261115。
規則FR-047/048/049、ARC-001/002/003/007/010/011/012、C-002/005/008/013/016/018、
QLT-008/009/010/012/013、GIT-001〜004。新waiver none、既存WVR-0001継続。

## 問いと範囲

#823/#825/#827の3ノート計測では、ノート数と本文量による初回準備の費用は未確認だった。
100件×4096 bytes、1000件×4096 bytes、32件×1048576 bytesの合成入力を用意し、
新しいapplication processの最初の本文検索と、同じcorpusでの3つの再検索、名前検索の新しいsessionを測る。
各構成3標本を初期計画とし、実行した条件を結果節へ記載する。OS cacheは消去しない。

製品コード・公開API・schema・品質ゲートを変更しない。既存のnative fixtureを拡張し、
同じprivate desktop、folio_window/folio_state/共有corpusと有限workの正典経路を通る。
外側Pythonが実時刻・UI thread CPUとmessage往復を観測する。製品やCのfixtureに時計・workerを追加しない。
計測中のloopは#827の実message pumpを共用する。開始操作はUIAを同期呼出しし、補助pumpで計測messageを消費しない。

結果の一致なし/一意の末尾一致/全件一致と最大200表示、byte位置・行番号・native preview選択を独立の期待値で照合する。
準備時に生成した2カテゴリの原文は反復processで同一性を照合し、追加上書きせず再利用する。
外側でも全mdのpath/bytes/hash/作成更新日時を照合する。実ノートと通常アプリは使用しない。

## 観測の意味

phaseの到着時刻は外側のpipe/UI/OS scheduling費用を含む。
readは全保存本文を観測した最後のidle単位の後、matchedは完成集合を採用したidle単位の後。
resultはpreviewを含むidle workが無くなりqueueが空になったことを一度確認してから、結果・位置・previewの照合を終え通知した時点。
UIの同期描画は各idleに含まれるため、read区間を純粋なdisk I/O時間、matched区間を中核matcher単体時間とは呼ばない。
postからの値とphase差を分けて記載する。readは最後のidleでの描画や一致計算の開始を含み得る。
matchedからresultの差にはpreview・queue処理・検証・通知も含まれ、純粋なpreview描画時間ではない。
setupの生成・初期本文表示は別の値とする。CPUはpost直前からresultと残ったpingの回収後までで、wall時間と厳密な同区間ではない。
今回のCPU値には量子化もあるため、短い操作のCPU比率を導出しない。pipe読取threadの配送とrawログ書込みの影響も残る。
20ms間隔を目安に最大1個だけ送るpingは、UI threadが確認messageに応答した往復時間であり、物理keyboard/IMEの入力遅延ではない。
pingとphase出力自体が観測対象へ小さな負荷を加える。少数標本の最大値を全端末の上限にしない。

## 全試行

調査読取ではdiscovery_row.hをapplication、note_match.h/note_match_position.h/persistence_outcome.hをcoreと仮定したpathが存在しなかった。
実ファイル検索とfolio_stateの宣言からindex_ranked_note、note_source_match、application/persistence_outcomeを確認した。
これらの読取失敗をbuild/検証成功へ数えない。

- configure-01: 正典eng/toolchain.ps1・CMake/Ninja Debug構成が成功。child/wrapper0、recordingErrors=[]。
- build-01: 既存tags_native_tests/motion_native_testsの対象buildが、select_tags_fixtureの複雑度12（上限10）で失敗。
  両exe未生成のartifact snapshotエラー2件も記録。製品libraryのcompileは成功した。計測構成の選択をselect_scale_fixtureへ分離し、閾値を変えず修正した。
- build-02: 同じ対象を修正後に再buildし、child/wrapper0、recordingErrors=[]。
- scale-01: `python -B tools/measure-discovery-scale.py <tags_native_tests.exe> <専用runtime-01> --samples 3`成功。
  3構成×3新規process、45操作が完了。結果数・最大200表示・末尾のbyte/行・native preview選択・元文書の保持を確認。
  反復後も全合成mdのpath/bytes/hash/作成更新日時が一致した。child/wrapper0、recordingErrors=[]。
- motion-live-01: 共用loopの変更境界として既存`tools/measure-discovery-motion.py`を実行し成功。
  静止20000.512msのmain thread CPU0ms、追加motion tick0、遅延配送2回×35msでも242.437msで本文1件へ完了。
  fixture156項目、child/wrapper0、recordingErrors=[]。製品の演出実装・visualは変わらないため再実行しない。
- conformance-01: CMake File API reply未生成の1件で失敗。正典eng/check.ps1と同じcodemodel-v2 query作成を補助configureへ追加した。
- configure-02: 同じCMake設定をquery付きで生成し成功。製品・テストのsourceやcompile条件は変わらないため、対象build/計測は再利用した。
- conformance-02: `python -B eng/conformance.py --build-dir build`で実graphを含め違反0。child/wrapper0、recordingErrors=[]。
- format-01: 変更C/headerの`clang-format --dry-run --Werror`と`git diff --check`が成功。child/wrapper0、recordingErrors=[]。
- merge-01: PR本文の必須見出しを「振る舞い・スキーマ:」と省略し、正典ツールがReady前に拒否。
  child/wrapper1、recordingErrors=[]。指定どおり「振る舞い・スキーマの変更:」へ訂正し、headを確定し直す。
  実装・検証入力は不変であり、成功済みの対象検証を再利用する。

独立読取レビューは重大な誤計測・隔離・契約問題なし。独立期待値と共用loopを確認し、上記のresult/CPU/pipeの時間境界を明記するよう指摘した。
指摘を本書へ反映した。レビューは編集・build・起動なしで実施し、恒久原本のcontract-review.mdに結果を保持する。

## 規模別の観測値

Windows 11 Pro 10.0.26300、Intel Core i9-10850K（20 logical processors）、物理memory約64GiB。
正典DebugのASan/UBSan/nullabilityとrecover禁止、private desktopの1050×720窓、motion OFF、合成ASCII本文。
2カテゴリに交互に保存し、1件だけ末尾にunique-scale、全件末尾にcommon-scaleを持つ。
各行は3個の独立process標本の **min / median / max、単位ms**。OS cache未消去、入力生成は測定区間の前に行う。

| 構成 | 初回本文・一意一致 | 再検索・一致なし | 再検索・全件一致 | 再検索・一意一致 | 新sessionの名前検索 |
| --- | --- | --- | --- | --- | --- |
| 100件×4KiB | 1010.952 / 1058.934 / 1081.868 | 56.974 / 63.537 / 65.663 | 192.691 / 194.847 / 197.016 | 79.883 / 81.352 / 85.737 | 88.436 / 89.370 / 95.725 |
| 1000件×4KiB | 4846.522 / 5182.360 / 5456.346 | 224.635 / 230.490 / 246.583 | 800.351 / 847.210 / 873.135 | 232.556 / 240.622 / 295.948 | 48.420 / 48.773 / 60.309 |
| 32件×1MiB | 2139.871 / 2196.890 / 2234.500 | 1691.923 / 1844.363 / 1894.421 | 6324.905 / 6488.023 / 6983.857 | 1936.918 / 1964.846 / 2183.773 | 74.789 / 83.499 / 91.358 |

| 構成 | post→read | read→matched | matched→検証済みresult | 全操作中のping最大 |
| --- | --- | --- | --- | --- |
| 100件×4KiB | 936.725 / 983.997 / 999.322 | 48.416 / 50.243 / 53.718 | 24.694 / 25.811 / 28.828 | 42.036 |
| 1000件×4KiB | 4618.184 / 4946.660 / 5200.297 | 216.200 / 223.243 / 243.273 | 12.138 / 12.457 / 12.775 | 24.467 |
| 32件×1MiB | 271.616 / 273.774 / 286.041 | 1854.319 / 1898.683 / 1946.440 | 11.777 / 12.166 / 16.444 | 57.831 |

独立した各列の中央値の和は、合計時間の中央値とは限らない。
setup（process作成・生成/既存source照合・初期文書・窓）は、100件で4612.570/4738.210/4746.967ms、
1000件で42562.189/48181.724/54112.018ms、長い本文32件で3295.262/4092.358/5348.831msだった。
setupを利用者のアプリ起動時間や検索準備時間とは解釈しない。fixtureは既存名へもcreate_noteを試し、NAME_TAKEN後に原文を照合する。
この生成/確認の手順でfilesystem cacheも温まる。純粋なOS cold測定ではない。

1000件では初回の完成集合より前の準備区間、32件×1MiBでは一致と位置計算を含む区間の比重が大きい。
これは上記の区間観測からの切り分けであり、disk/parse/matcher/paintの個別費用を特定した結論ではない。
全件一致では表示が200件に制限されても、対象全体と必要な位置計算は完了する契約を保つ。
検索完了までの時間と合成messageへ応答できることを分ける。数秒の完了時間を「即時検索」と呼ばず、ping最大を入力遅延の保証にしない。

全rawはscale-01とruntime-01、集計は恒久measurement-summary.json、環境はenvironment.jsonに保持する。
最終CI・統合・全証跡保全/整理は未完了。物理IME・実読み上げ・他OS/DPI・冷たいOS cache・Unicode主体の大量本文は本計測の対象外。
