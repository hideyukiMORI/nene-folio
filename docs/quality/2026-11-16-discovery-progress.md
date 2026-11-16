# 本文準備の進捗だけのidleで配置を繰り返さない — #837

2026-11-16、base main `f35d051fe7dd5da8e20c6a71416c382d1189885a`。
作業木D:/NeNeFolio/worktrees/837-discovery-progress、試行D:/NeNeFolio/outputs/837-discovery-progress、
道具と記録D:/NeNeFolio/agents/837-discovery-progress。
規則ARC-001/004/011/012、C-012/013/017、FR-047/048、QLT-008/009/010/012/013、GIT-001〜004、ADR0065/0073/0094/0095/0096。
新waiver none、既存WVR-0001継続。

## 問いと変更

[#833](2026-11-15-discovery-cost.md)は、1000件×4KiBの初回本文検索でread以外のidle内に時間があることまでを観測し、主因を特定しなかった。
`continue_discovery`は一ノート読むたびに`discovery_refresh`を呼び、summaryが読込件数しか変わらなくても
暗幕regionの作り直し、`SetWindowPos`/`MoveWindow`、入力欄とpreviewの配置、tooltip更新、面全体の無効化を行っていた。

変更は`src/ui/win32/folio_window.c`の継続処理だけ。idleの前後でsummaryを比べ、
`source_checked`/`source_total`以外（phase・mode・preview・notice・total・shown・selected・unreadable・unsupported・revision・results_revision）が同じで、
継続の結果がREADYなら、状態行の矩形だけを無効化し、演出の同期とUIA更新予約を従来と同じ順で行う。
それ以外のidle、query変更・明示操作・resize/DPI/言語/配色からの呼出しは従来の`discovery_refresh`のまま。
一idle最大一ノート・65536 workの政策、検索engine/corpus/位置計算、保存schema、版は変えていない。新しいcache・worker・時計読取もない。

## 回帰試験（修正前red）

`tests/win32/image_status_tests.c`の`body_discovery_progress_idle`を`--body-discovery`へ追加した。
実窓の検索面を一時的に観測し、summaryが読込件数だけ進んだidleで`WM_WINDOWPOSCHANGING`が0回、
継続後に未描画の更新領域が残らないこと、そのようなidleが1回以上あること、最終的な一致位置のpreviewが従来どおりであることを確認する。
比較の述語は製品側と独立に試験側へ書いた。

| 試行 | 内容 | 結果 |
| --- | --- | --- |
| red-01 | 修正前の製品＋新試験 | child 1。`a progress-only idle paints its count without arranging the surface again`で失敗（意図した赤） |
| green-01 | 修正後＋新試験の`--body-discovery` | child/wrapper 0、273 checks |
| source-check-01 | 変更Cの`clang-format --dry-run --Werror`、`eng/conformance.py --build-dir build`、`git diff --check` | 違反0、child/wrapper 0 |
| ctest-01 | 対象CTest | 起動失敗（bashから`ctest`が見つからない。wrapper 125）。試験の成否には数えない |
| ctest-02 | toolchain経由の対象8件 | 6件成功、`tag_management_native`と`body_discovery_native`が30秒のTimeout（下記の環境要因） |
| ctest-duration-probe-02 | 同じ3 selectorを基準/修正後/修正後＋新試験のexeで各2回、直接起動 | 18回すべて終了値0。所要は基準でも`--tag-management` 54.0〜54.6秒、`--body-discovery` 4.8〜45.0秒、`--discovery` 3.5〜28.2秒と変動 |

Timeoutは修正前の基準exeでも同じ規模で起きており、変更の影響ではなく環境の負荷と判断した。
timeout値・閾値・除外は変更していない（QLT-010）。最終headの必須CIで全ゲートを通す。
ctest-duration-probe-01は相対pathの指定誤りで子を起動できず、計測値はない。

## 環境（重要）

他製品の対話席が同じ端末でbuild/testを続けており、CPUは観測点で2〜96%、
D:（HDD）は`% Idle Time` 0.0・待ち行列2〜7が続いた（C:/E:のSSDは99%以上idle）。
fixtureの種まきは通常約47秒/1000件に対し、100件で72秒（ノート間隔の中央値225ms、最大7.1秒、fixtureのCPUは0.2秒未満）まで遅くなった。
このためmeasure-base-01 / measure-patch-01は種まきが道具の固定120秒を超えて`queue.Empty`で失敗し、計測値を得ていない。
`tools/measure-discovery-scale.py`へ`--setup-timeout SECONDS`（既定120、120〜3600）を追加し、種まきの待ちだけを延ばせるようにした。
計測する区間と既定の動作は変えていない。置き場はhideの決まりどおりD:のまま使った。

壁時計は負荷の影響を受けるので、同じ道具が記録する**主スレッドのCPU時間**を併記し、判断の主な根拠にする。
基準と修正後は別process・別時刻で、修正後の計測時のほうがCPU使用率が高かった（基準開始時48〜72%、修正後開始時79〜96%）。
専用端末の値ではなく、全端末の保証や物理入力遅延の上限でもない。OS cacheは消していない。

## 測定（Release・1000件×4096byte・各3process）

min / median / max、単位ms。基準exe SHA256 `7460ec84…5aeca8`、修正後exe `f2da9e38…60df7ed`（新試験を含むbuild。計測経路の製品コードはpatch-bin-01と同じ）。

| 操作 | 基準 壁時計 | 修正後 壁時計 | 基準 主スレッドCPU | 修正後 主スレッドCPU |
| --- | --- | --- | --- | --- |
| 初回本文・一意語 | 4316.775 / 6424.503 / 11234.594 | 1241.329 / 1470.481 / 1983.781 | 4203.125 / 5843.750 / 6937.500 | 1187.500 / 1468.750 / 1781.250 |
| 同じcorpus・不一致語 | 110.667 / 177.874 / 231.525 | 45.188 / 46.812 / 62.529 | 109.375 / 171.875 / 203.125 | 46.875 / 46.875 / 62.500 |
| 同じcorpus・全件一致語 | 386.149 / 514.996 / 758.958 | 129.400 / 161.242 / 197.870 | 375.000 / 500.000 / 625.000 | 125.000 / 171.875 / 187.500 |
| 同じcorpus・一意語 | 122.875 / 183.410 / 281.330 | 46.674 / 61.378 / 67.361 | 125.000 / 171.875 / 203.125 | 46.875 / 62.500 / 78.125 |
| 新しく開く名前検索 | 38.261 / 58.152 / 107.075 | 51.364 / 52.529 / 53.739 | 46.875 / 46.875 / 62.500 | 46.875 / 46.875 / 62.500 |
| 合成messageの最大往復 | 111.639 | 48.569 | — | — |

初回本文検索の主スレッドCPUは中央値で5843.750ms→1468.750ms（約75%減）、基準の最小値4203.125msと比べても修正後の最大値1781.250msが下回る。
基準の時間のほぼ全部が主スレッドのCPU時間で、入出力の待ちではなかった。
再検索（一致計算を65536 workごとに続けるidle）でも同じ経路が効き、約3分の1になった。名前検索はidle継続が少なく変化なし。
CPU時間の刻みは15.625msなので、小さい値の差は刻み以下として扱う。

修正後に残る約1.2〜1.8秒は、状態行の同期描画・UIA更新予約・一ノートの読込とcorpus登録・idle間のmessage処理を含む。内訳は未測定。
長い本文（32件×1MiB）は次節。

## 測定（Release・32件×1MiB・各3process）

measure-long-base-01 / measure-long-patch-01。開始時のCPU使用率は基準28〜54%、修正後51〜68%。

| 操作 | 基準 壁時計 | 修正後 壁時計 | 基準 主スレッドCPU | 修正後 主スレッドCPU |
| --- | --- | --- | --- | --- |
| 初回本文・一意語 | 1495.290 / 1773.928 / 1915.325 | 525.169 / 528.392 / 8552.325 | 1484.375 / 1625.000 / 1843.750 | 531.250 / 531.250 / 562.500 |
| 同じcorpus・不一致語 | 1149.328 / 1622.879 / 2028.947 | 359.241 / 368.818 / 398.947 | 1093.750 / 1375.000 / 1515.625 | 359.375 / 375.000 / 406.250 |
| 同じcorpus・全件一致語 | 3912.500 / 4498.348 / 5094.756 | 1256.671 / 1297.206 / 1722.708 | 3765.625 / 4312.500 / 4796.875 | 1140.625 / 1250.000 / 1281.250 |
| 同じcorpus・一意語 | 1288.087 / 1691.969 / 12715.836 | 337.362 / 376.156 / 397.903 | 1015.625 / 1250.000 / 1609.375 | 328.125 / 375.000 / 390.625 |
| 新しく開く名前検索 | 51.686 / 55.263 / 68.836 | 50.586 / 55.863 / 68.485 | 46.875 / 62.500 / 62.500 | 46.875 / 62.500 / 78.125 |

#831で別課題としていた長い本文の全件一致再検索も、主スレッドCPUの中央値が4312.500ms→1250.000msになった。
一致位置を65536 workずつ進めるidleのたびに同じ全配置が走っていたためで、位置計算そのもの（engine）は変えていない。
基準の12715.836msと修正後の8552.325msは、同じ標本の主スレッドCPUが1609.375ms / 562.500ms以下で、
合成messageの往復も12715.530ms / 5810.429msだった。主スレッドがCPUを使わずに待った外れ値で、D:の混雑時に両側で1回ずつ起きた。
原因（読込の待ちかpipeか）は特定していない。中央値とCPU時間で比較し、この2標本を速度の根拠にしない。

## 演出の実pump

`tools/measure-discovery-motion.py`を修正後のmotion_native_tests.exeの写しで実行（motion-live-01、child/wrapper 0）。
native 162 checks成功。演出ONの2検索は各標本で完了し、35msの配送遅延8回でも65 idleで1件の結果が完成した（1234.871ms、保証値ではない）。
進捗だけのidleでも`discovery_motion_wait`の解除と`discovery_motion_sync`を従来と同じ順で行うことを、この実pumpと`motion_native`で確認した。

## 確認していないこと

実読み上げ、物理IME、全OS/DPI、cold OS cache、Store署名版。状態行の件数表示が各idleで実際に画面へ出ることは
更新領域が残らないことと既存描画経路で確認し、画像の逐次採取はしていない。
