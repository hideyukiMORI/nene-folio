# 一覧の配置準備費用 — 2026-11-05 / #637

## 結論と範囲

#635は行選択の探索を改善したが、クリック前には一覧配置の作成も行う。
1カテゴリ16384本を展開した正常入力で、create→mark→scroll→末尾hit→destroyは、
短い名前453.84762µs、204byte日本語名1469.63016µsだった。create+destroyは各406.88095/1346.48095µs。
この観測は、単独hitの高速化率をクリック全体の高速化率として扱えない根拠になる。
別processで測ったphaseの中央値の差を足し引きし、因果的な寄与率としては扱わない。

新しい製品欠陥は検出せず、製品・API・所有者・確保経路は変更しない。次の設計判断の基点として残す。
通常app・実ノート・Storeは試験に使っていない。

## 正典経路と規則

- `src/ui/win32/drawer_window.c` のcurrent_layoutが、`folio_state_drawer_layout`を呼ぶ。
- `src/application/folio_state.c` の同関数は、既存coreのcreate→mark→scrollを使う。
- `src/core/drawer_layout.c` が行・名前・カテゴリごとのノート数を所有し、座標変換も同ownerで行う。
- 今回のobserverはそのcore関数を順に呼ぶ外部計測補助。製品内に計装や別cacheを入れない。

対象規則: ARC-001/003/004/007/011/012、QLT-008/010/012/013、GIT-001〜004。
契約はADR0004/0009/0015/0024。追加waiver none、既存WVR-0001はWinHTTP callbackだけ。

## 入力・方法

基点main9a84beb4c0519392fb8dd3a4fcd76695859a7f51、正典Release core83actions成功。
`eng/toolchain.ps1`とCMakeの実compile vectorを使い、/O2・/Ob2・/DNDEBUG・/MT・C23・/WXを維持。
製品libraryとsource/vectorをSHA256一致で凍結し、外部observerを直接linkした。
MAPで使用する9個のdrawer_layout関数が正典archive由来であることを確認した。

32正常入力 = 8/128/2048/16384本 × 1/8カテゴリ × 全展開/全折り畳み × 10byte ASCII/204byte日本語名。
各カテゴリの本数は等分、名前は連番で一意。同じ読み取り専用note ledgerをカテゴリの各参照へ渡す。
64JSON fileを生成・hash照合し、全名前が通常の252byte以下であることも独立に確認した。
絞り込み・混在した展開状態・空一覧・実ノートの分布は今回の計測対象に含めない。

寸法はtop8、category gap6、高さ34、note高さ28、viewport600、bottom10。
選択/カーソルは最後のカテゴリの最後のノート。閉カテゴリではその行は配置に無い。

| phase | 計時範囲 |
| --- | --- |
| create | drawer_layout_create → 行数の定数時間確認 → destroy |
| mark | 既存配置にselection/cursorの印を付け直す |
| reveal | scroll0の既存配置で最後のノートを表示する量を求め、期待値と比較 |
| pipeline | create → 行数確認 → mark → 上限までscroll → 最終表示行の中央hit照合 → destroy |

parse/readと全行の名前・種類・所属・座標・scroll上限・選択印の照合は計時外。
warmup1回、5sample。反復数=max(32,min(32768,floor(1048576/表示行数)))。
phase分岐・結果の定数時間確認・反復loopは計時内。allocation/freeはcreate/pipelineに含む。
QPCとthread cyclesを記録し、同じthreadをmask4へ固定、優先度は変更しない。
環境はIntel Core i9-10850K（10core/20thread）、Windows 11 Pro build26300。取得値はenvironment.jsonへ保持。
計時開始時に既知のbuild/observer processが無いことを確認し、親のbuild/hash/archive作業を重ねなかった。
fixtureは固定順、phaseもcreate/mark/reveal/pipelineの順。OS全体のidle・cache・周波数は制御していない。

## 実測

各値は5sampleの中央値、単位µs。全128case/640sampleのraw QPC/cycleはmeasure.jsonに保持する。

| ノート数 | カテゴリ | 展開 | 名前 | create + destroy | mark | reveal | pipeline |
| ---: | ---: | --- | --- | ---: | ---: | ---: | ---: |
| 8 | 1 | 開 | 10byte ASCII | 0.26100 | 0.01466 | 0.01044 | 0.29171 |
| 8 | 1 | 開 | 204byte日本語 | 0.49397 | 0.01443 | 0.01240 | 0.53155 |
| 8 | 1 | 閉 | 10byte ASCII | 0.17454 | 0.00596 | 0.00760 | 0.18252 |
| 8 | 1 | 閉 | 204byte日本語 | 0.17033 | 0.00563 | 0.00760 | 0.18199 |
| 8 | 8 | 開 | 10byte ASCII | 0.40685 | 0.02084 | 0.01355 | 0.43984 |
| 8 | 8 | 開 | 204byte日本語 | 0.61178 | 0.02099 | 0.01357 | 0.67929 |
| 8 | 8 | 閉 | 10byte ASCII | 0.30128 | 0.01087 | 0.00983 | 0.30172 |
| 8 | 8 | 閉 | 204byte日本語 | 0.29007 | 0.01033 | 0.01003 | 0.30840 |
| 128 | 1 | 開 | 10byte ASCII | 1.66129 | 0.20278 | 0.06625 | 1.92399 |
| 128 | 1 | 開 | 204byte日本語 | 5.42506 | 0.19798 | 0.08986 | 5.64893 |
| 128 | 1 | 閉 | 10byte ASCII | 0.16243 | 0.00511 | 0.00757 | 0.17452 |
| 128 | 1 | 閉 | 204byte日本語 | 0.16147 | 0.00512 | 0.00757 | 0.17614 |
| 128 | 8 | 開 | 10byte ASCII | 1.78634 | 0.16294 | 0.16110 | 1.98930 |
| 128 | 8 | 開 | 204byte日本語 | 5.36324 | 0.15192 | 0.16350 | 5.75370 |
| 128 | 8 | 閉 | 10byte ASCII | 0.30811 | 0.01003 | 0.01024 | 0.31714 |
| 128 | 8 | 閉 | 204byte日本語 | 0.28972 | 0.00982 | 0.01006 | 0.32023 |
| 2048 | 1 | 開 | 10byte ASCII | 36.85205 | 3.55773 | 1.47906 | 41.02779 |
| 2048 | 1 | 開 | 204byte日本語 | 138.90607 | 3.47436 | 1.31566 | 145.87045 |
| 2048 | 1 | 閉 | 10byte ASCII | 0.16506 | 0.00553 | 0.00760 | 0.17946 |
| 2048 | 1 | 閉 | 204byte日本語 | 0.16378 | 0.00757 | 0.00757 | 0.18104 |
| 2048 | 8 | 開 | 10byte ASCII | 35.23804 | 2.63686 | 1.31196 | 38.41627 |
| 2048 | 8 | 開 | 204byte日本語 | 130.91471 | 2.61863 | 1.57373 | 133.64451 |
| 2048 | 8 | 閉 | 10byte ASCII | 0.28796 | 0.01023 | 0.01003 | 0.31883 |
| 2048 | 8 | 閉 | 204byte日本語 | 0.28762 | 0.01023 | 0.01003 | 0.31010 |
| 16384 | 1 | 開 | 10byte ASCII | 406.88095 | 32.71746 | 19.20794 | 453.84762 |
| 16384 | 1 | 開 | 204byte日本語 | 1346.48095 | 36.94127 | 20.86032 | 1469.63016 |
| 16384 | 1 | 閉 | 10byte ASCII | 0.16425 | 0.00563 | 0.00757 | 0.17847 |
| 16384 | 1 | 閉 | 204byte日本語 | 0.16647 | 0.00583 | 0.00757 | 0.17794 |
| 16384 | 8 | 開 | 10byte ASCII | 337.12222 | 27.53651 | 18.02857 | 376.22381 |
| 16384 | 8 | 開 | 204byte日本語 | 1301.15714 | 26.73175 | 18.47460 | 1353.15397 |
| 16384 | 8 | 閉 | 10byte ASCII | 0.28472 | 0.01034 | 0.01164 | 0.31113 |
| 16384 | 8 | 閉 | 204byte日本語 | 0.29238 | 0.01033 | 0.01005 | 0.30350 |

折り畳んだ場合、配置作成は隠れたノート名を走査・複写しない。1カテゴリは一覧の総数に関わらず
pipeline約0.18µsだった。展開した大きな一覧では名前の長さによる費用差も観測した。
単独mark/revealにも行数に比例する走査が残るが、今回のデータだけでcache導入の妥当性は決めない。

次に配置を再利用する設計へ進む場合は、row revision・選択/カーソル・scroll・寸法・絞込の失効条件と
既存UIA用配置の所有を先に確認する。所有者や公開APIを変えるならADRを先行する。
実UI/GDI/イベント配送を含むフレーム時間や体感改善は、今回の計測から主張しない。

## 検証と原本

全対象実行はtools/record-run.py。原本はD:/NeNeFolio/agents/637-drawer-preparation/。
一時出力はD:/NeNeFolio/outputs/637-drawer-preparation/、作業木はD:/NeNeFolio/worktrees/637-drawer-preparation/。
全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。

| run | 結果 |
| --- | --- |
| configure-release-01 / build-core-01 | 正典Release core、83actions成功 |
| fixtures-01 / reference-01 / compile-observer-01 | 正常入力生成・正典library固定・observer構築成功 |
| measure-01 | 128case/640sample成功、stderr空、入力終了時不変 |
| audit-01 | 64入力hash、全raw統計、9正典symbol、source不変、全run成功を再照合 |
| conformance-01 | 補助driverがFile API未設定のRelease build-dirを指定して停止。未生成docs-checkのartifact記録error1も保持 |
| conformance-02 | 文書だけの差分に合わせ任意build-dir指定を外し、相対参照/diff/source conformanceを確認 |

今回は製品変更がないためDebug/製品回帰を追加で走らせない。文書差分・相対参照・conformanceを確認する。
最終headの必須CIは従来どおりFile APIのbuild graphを含むeng/check.ps1全体を実行する。
最終head必須CI/統合/通常exe-data不変/全保全/専用領域整理は同taskのintegration-completion.jsonで確定する。
branch/commitは保持。#634の保留試験の追加build/runは行っていない。
