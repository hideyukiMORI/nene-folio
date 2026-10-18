# 本文部分一致の比較削減 — 2026-10-18（#331）

## 対象と判断

hideの「それに着手して」（13:16 JST）を受け、[#331](https://github.com/hideyukiMORI/nene-folio/issues/331)を開始した。
基点mainは85e8beb641cbf640bfc21f56172abada89bdfde1。#328で本文の宛先探索を改善した後に残った、
`src/core/index_filter.c`の名前/本文の長さ付きUTF-8部分一致を対象とする。
前件の10,000件Debug実測はwarm zが約524ms、aaaaabが約1040msだったが、新しいcore比較の直接baselineには使わない。

FR-032 / ADR0024、ARC-001/003/004/007/010/012、C-003/005/006/008/011/012/013/014/016、
QLT-008/009/010/012/013、GIT-001〜004に対応する。内部実装詳細なので新ADRは不要。Waivers: none。

## 実装の契約

検索語を検証した後、256要素のsize_t移動表をstack上に一度作る。ASCIIの大小は既存のascii_fold_byteから導出する。
候補末尾のraw byteで移動量を引き、末尾が一致する候補は先頭のfold済byteでも選別してから完全比較する。
名前と本文で同じ検索経路を使い、追加heap確保、公開API、型、cacheは作らない。
1byte語では表の0から一致可能なraw byteをqueryごと一度だけ取り出し、その1〜2byteとの直接比較に縮約する。
同値byteの集合は別のchar[2]に置く。textごとの前処理や本文loop内のfold/table loadは不要になる。

正の移動量は語の末尾以外の右端出現位置から得る。その距離より手前へ開始位置を進めても、現在の候補末尾byteを語に一致させられない。
末尾一致後の不成立は1byteだけ進める。語長が本文長を超える場合を先に除外し、次の移動量と残る候補距離を比較してから加算する。
前処理はO(m+256)、追加stackはx64で約2KiBと2byte。最悪O(n*m)は残るため、線形時間保証とは扱わない。

ASCIIのみ大小同一、非ASCIIはbyte一致、語/本文は明示長、本文欠落なら名前一致でも不可視、結果は台帳順という契約を維持する。
空語・不正UTF-8・OOMでoutとapplicationの採用済み状態を変えない。保存schema、依存、品質ゲート、UI、遅延読込方針は変更しない。

## 先に固定した受理条件

同一の実際のpublic index_filter_createへ1,000/10,000件のメモリfixtureを渡す。本文生成と結果検証を計測外に置き、
検索語の前処理と結果の確保は計測に含める。同じ固定toolchain・canonical Debug条件でwarmup 1回と30標本を取り、中央値/p95/最大を保存する。

- 10,000件のz中央値を25%以上、aaaaabを50%以上削減する。
- alpha、逆向きのbaaaaa、長い周期語、最後位置の一致、UTF-8混在も測る。
- 全群の中央値悪化を20%または0.25msの大きい方以内に収める。1,000件も同じ回帰条件を適用する。
- 全結果のmembershipと台帳順を確認する。失敗や負荷が重なった生標本も保持し、遅い標本だけを除かない。
- 外部buildと重なった群は全体を不採用として、必要な群だけを再測定する。他案件のprocessは停止しない。
- 最終PR headの必須CIでeng/check.ps1とcheckを通す。

## 測定方法

全本文は10,240 bytes。基本4語は#328と同じ100カテゴリ×10/100ノートのASCII本文をメモリ上で生成する。
追加群は長周期（abを16回とacの34byte語、4件に1件だけ末尾一致）、tailmatch（偶数件の末尾をTaIlMaTcHへ置換）、
UTF-8混在（日本語 CAFé、3件に1件だけ一致）とし、入力と期待membershipを測定前に固定する。
1byte語の見直し時にearly（語NでN000等の短い名前が全件一致）を追加し、同じ回帰上限を課した。
元7群の入力とQPC内statementは変更せず、旧harness v1と追加したv2を保存・識別している。
QPC開始からquery初期化・実public factory終了までを計り、destroyと全membership/座標・入力byteの検証は外に置く。
matcherの複製を作らず、canonical CMakeで作った同条件のcore libraryを使う。I/O、entry組立、UIの短縮率としては扱わない。

## 検証と実測

13:53時点の完成候補はindex_filter.c SHA256 `4db9842857ade22501d29dab72c68c36436888b7bd3a7fc1e103664a38b83bd6`。
以下を一区切りで実行し、すべてexit 0。source/configの204入力は検証前後で不変だった。

| コマンド / 確認 | 結果 |
| --- | --- |
| cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug | clang-cl 19.1.5でconfigure成功 |
| cmake --build build --target folio_tests --parallel 2 | 初回64工程と変更分の増分buildが成功。最終検証は同一204入力のbuildを再利用 |
| build/folio_tests.exe --filter-lookup | filter/core/application境界が成功 |
| python D:/NeNeFolio/outputs/331-filter-matching/implementation/target-allocation.py | eng/coverage.pyのASan-only設定で2selector成功 |
| 計装exe --filter-lookup-oom | factoryの3確保すべてで失敗注入、既存nonnull outと旧集合保持、同query再試行が成功 |
| clang-format --dry-run --Werror --style=file（変更C/header 5件） | 成功 |
| python eng/conformance.py --build-dir build | 0 violation |
| python eng/symbols.py --build-dir build --require core application | 2 library、0 violation |
| git diff --check | 成功 |

追加した境界は、単一byte、prefix密/逆向き、重なり、skip後/最後候補、未終端と明示長、空でないNUL語、
日本語/4byte/結合文字とASCII fold、空語/不正語での既存out保持、factoryの全3確保OOMと再試行。
既存の結果順・配置・遅延読込・保存後の本文修復/失敗状態を同じselectorで確認した。
完成候補のindex_filter LLVM実分岐は103/106（97.17%）、関数15/15、行197/204。これは対象測定で、全件CIの代わりではない。
ASan-only試験では従来と同じinterception_winの起動診断が出たが、2selectorとも成功した。診断を抑制せず生ログに残した。

SOL filter_reviewの独立読み取りレビューと候補差分レビュー（最終13:43）にblocker/high・修正要求なし。
skip根拠、ASCII aliasの固定点、明示長/加算境界、所有と失敗時状態、試験境界を確認し、実行試験や速度受理とは区別した。
報告は恒久証跡review.md、最終の実行ログはimplementation/attempt-3/。性能全群・最終head CI・統合はこの時点では未完了。

## 証跡と終了条件

恒久証跡はD:/NeNeFolio/agents/331-filter-matching/、一時出力はD:/NeNeFolio/outputs/331-filter-matching/。
実装・測定はhideの今回の明示分担によりSOL、設計/判断/受理/統合はAstraが担当する。
統合・通常exe反映・恒久成果物の照合後に、絶対パス、取込、未保存/未追跡/ignored、唯一成果物、process参照、リンクを確認して専用作業物を整理する。
旧taskの削除を拒否された出力には触れない。最終PR/CI/通常exe/整理の実結果はcompletion.jsonへ残す。

## 初候補の停止と設計見直し（13:34）

初候補45f184b6の1,000件は全7群で入力byteと結果が一致し、外部buildの負荷重複もなかった。
しかしzは中央値49.7229→101.01905ms（103.16%悪化）、p95 56.8836→106.8233ms、最大56.9934→116.5775msとなり、回帰条件を超えた。
この候補は受理せず、10,000件を開始せずに停止した。他6群は回帰条件内で、aaaaabは75.72%、長周期は96.30%短縮した。
初候補の全CSV/source/library/probeと不採用理由を恒久証跡へ保存し、受理閾値を変更していない。

語長1では、同じ256移動表の0値がそのまま完全一致を表す。この場合に限り、単純な1byte走査で表を参照し、
不要な動的移動量/残距離計算/先頭fold/完全比較を除く設計へ変更した。語長2以上のループは維持する。
ASCII判定やmatcherの別定義は作らず、同じfactory内の長さ別の縮約とする。追加API/型/heapはなく、表の準備は1回/queryのまま。
候補修正後は対象検証を行い、同じ1k zの30標本で先に診断する。10kの受理条件と全群の回帰条件は維持する。

## 2番目の候補の診断と見直し（13:40）

候補08d79734は同じ1k zで中央値52.60965ms、p95 59.9281ms、最大65.3647ms。基点49.7229msに対して5.81%悪化だった。
回帰上限は超えないが、10k本受理に進むための25%改善診断に届かず、他群/10kへ進まず停止した。
この候補の対象増分buildは3工程、通常/計装2selectorと規約検査は全成功、分岐89/92（96.74%）だったが、速度受理とは分ける。
独立差分レビューにも修正要求なし。失敗値とsource/config/binaryの識別・全生標本を残している。

3番目の候補では、m==1に限り既存の移動表の0値から一致可能なraw byteを1〜2個、queryごと一度だけ取り出す。
同種byteの集合char[2]とsize_t[256]移動表は別の配列とし、表の中へ別の意味の値を埋め込まない（C-006）。
本文走査はその2scalarとの直接比較だけとする。textごとに256要素を取り出す案は短い名前/早期一致を遅くするため採らない。
語長2以上のループと唯一のASCII fold定義は維持し、追加API/型/heapなし、追加stackは2byte。

この見直しに合わせ、語NでN000等の名前が全件早期一致するearly群を1k/10kへ追加し、既存と同じ回帰上限を適用する。
元7群の入力/QPC区間を変えず、成功済み基点を再利用する。旧harnessと入力metadataも保存して識別する。
必要な実装判断の最小診断として1k zを先行し、達成後に完成候補の対象検証と残る性能群をまとめる（QLT-012）。

## 完成候補の診断と対象検証（13:53）

候補4db98428のcleanな1k zは中央値49.7229→33.90945ms（31.803%短縮）、p95 56.8836→37.7965ms、最大56.9934→41.3591ms。
入力/結果が一致し、次へ進む条件を満たした。直前に外部buildが重なった別runは全31行を保持し、不採用としている。
この値は10k本受理の代わりではない。計時を止め、完成候補の新しい単一byte名前/本文正例も含む通常/計装2selectorと規約検査をまとめて実行し、全成功した。
残る性能群と最終head必須CIは未完了。以後、製品入力が不変ならこの対象検証を再利用する。
