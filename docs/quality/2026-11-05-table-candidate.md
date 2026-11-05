# 表候補の判定順と通常段落の費用 — 2026-11-05 / #683

markdown_table_startsで、副作用のない既存predicateの順序だけを入れ替えた。
通常段落/引用の直接判定は逆順確認で約46〜66%短縮し、全文RTFは約3〜8%短縮した。
長い日本語本文の全文RTFは5.592→5.423ms、引用は5.621→5.436ms。
実際の表はほぼ横ばいで、短い3列表の全文は14.547→14.713µs（約1.14%増）。
長い3列表の初回約3.20%増は確認では約1.00%短縮となり、一般的な表の改善率は主張しない。
この最小の順序変更を採用する。RichEdit投入・描画・入力を含むUI全体の速度は測っていない。

## 正典と同値性

[ADR0070](../adr/0070-markdown-syntax-and-native-tables.md)の既存Markdown→RTFのまま、src/core/markdown_table.cだけを変更する。
従来はheaderとdelimiterの列数をsplitで求め、等しいときにdelimiter成立を調べていた。
候補はdelimiter成立を先に調べ、成立したときだけ同じ列数比較を行う。

両入力のmarkdown_line_validとheaderのblank/heading/thematic/fence判定は従来の順番を維持する。
columnsとdelimiterは借用bytesを読むだけで、各呼出しの固定stack以外の状態・確保・I/O・out更新・callbackを持たない。
同じ2つの真偽値のANDを入れ替えるため、成立/不成立とRTF出力、確保/失敗/所有の経路は変わらない。
不成立の通常行ではheader列数分割とdelimiter列数の再分割を省ける。実表では同じ関数を同じ回数呼ぶ。
新parser/cache/owner/API/製品hook/構文拡張を作らず、既存判定の第2実装を置かない。

既存--markdown-emissionはtable開始とその周囲の通常段落/見出し/引用/各blockを通すため、その対象を再利用する。
実装の並べ替えをそのまま写した試験や、新selector/CTestは追加しない。tests/CMake/eng差分0。

## 比較の方法

#681統合後mainfc6faea053c224fbaf7c446c4b2e274b1c5f1765から、正典Release core83actionsを構築した。
元source・archive・compile vectorを凍結し、同じ/O2・/Ob2・/DNDEBUG・/MT・C23・/WXで明示markdown_table objectを作り、
同じcore archiveの前にlinkする。2observer×2版の20symbol bindingをMAPで確認した。
markdown_line_readは両版とも#681の同じarchiveを使い、先行の行末改善を今回の効果へ混ぜない。

ASCII、日本語emoji、引用、見出し、3列表、8列表の短/長12文書。
短側は主本文16文字・通常16行、長側は主本文1024文字・通常128行を基礎に生成し、表のbodyはその半分の行数。
表は有効なheader/delimiterと日本語cell、直接predicateは明示true。通常段落・引用・見出しは明示false。
本文は有効UTF-8だけで埋め込みNUL/変異入力を含めない。全12入力の実bytes数とhashはfixtures.jsonへ。
直接predicateと全文RTFの24条件、36fixture file。全文RTF期待は基準版の全bytesを固定した。

同じaffinity mask4・優先度不変、1warmup/5sample/run。
直接は事前に読んだ2行に対するtable_startsと真偽確認を計時内、全文はRTF生成/長さ確認/破棄を計時内に置く。
file読取り/fixture作成/note_text生成/全文bytes比較は計時外。計時前後の全結果比較を行い、親build/archive/hashを並行しない。
profile120sample→ABBA480sample→同source/exeをBAABで480sample、計1080sample全一致。
ABBAはprofile反復4倍、確認は16倍。全反復が既存上限65536以内であることを起動前に確認した。

## 全条件の実測

確認は各版10sample中央値。初回差はABBA、確認値はBAABであり混ぜて集計しない。
時間はµs、差は候補/基準-1。cycle差も各版の中央値から計算した。

| 条件 | 初回時間差 | 確認基準µs | 確認候補µs | 確認時間差 | 確認cycle差 |
| --- | ---: | ---: | ---: | ---: | ---: |
| table-ascii-short | -43.54% | 0.103 | 0.056 | -46.05% | -45.57% |
| markdown-ascii-short | -1.98% | 20.612 | 19.950 | -3.21% | -3.28% |
| table-japanese-short | -48.71% | 0.122 | 0.061 | -49.66% | -49.33% |
| markdown-japanese-short | -4.08% | 22.064 | 20.294 | -8.02% | -7.33% |
| table-quoted-short | -49.70% | 0.124 | 0.062 | -49.66% | -48.89% |
| markdown-quoted-short | -3.00% | 22.523 | 21.706 | -3.63% | -3.52% |
| table-heading-short | -1.75% | 0.019 | 0.019 | -0.89% | -1.02% |
| markdown-heading-short | -2.74% | 13.612 | 13.350 | -1.92% | -2.01% |
| table-table3-short | +1.28% | 0.130 | 0.133 | +2.17% | +2.14% |
| markdown-table3-short | -1.41% | 14.547 | 14.713 | +1.14% | +1.21% |
| table-table8-short | +0.47% | 0.227 | 0.226 | -0.22% | +0.08% |
| markdown-table8-short | -3.82% | 30.891 | 30.760 | -0.42% | +0.08% |
| table-ascii-long | -64.74% | 1.068 | 0.369 | -65.50% | -65.50% |
| markdown-ascii-long | -3.79% | 2592.352 | 2506.686 | -3.30% | -3.74% |
| table-japanese-long | -64.77% | 2.213 | 0.746 | -66.27% | -66.16% |
| markdown-japanese-long | -5.88% | 5591.812 | 5422.886 | -3.02% | -3.01% |
| table-quoted-long | -65.57% | 2.206 | 0.743 | -66.32% | -66.21% |
| markdown-quoted-long | -2.00% | 5621.148 | 5436.364 | -3.29% | -4.09% |
| table-heading-long | -1.55% | 0.019 | 0.018 | -0.46% | -1.11% |
| markdown-heading-long | -1.49% | 2255.083 | 2184.509 | -3.13% | -3.29% |
| table-table3-long | -0.23% | 0.573 | 0.569 | -0.71% | -0.72% |
| markdown-table3-long | +3.20% | 786.422 | 778.584 | -1.00% | -0.83% |
| table-table8-long | +1.69% | 0.681 | 0.671 | -1.39% | -1.39% |
| markdown-table8-long | -0.98% | 888.542 | 888.345 | -0.02% | -0.16% |

通常段落/引用の直接と全文の短縮が両順序で続き、実表は小さい差に留まるため採用した。
短3列表の直接約2.17%増/全文約1.14%増も保持する。変更前に早期returnする見出し直接にも約0.46〜0.89%の差があり、
微小な差を原因確定や保証倍率に読み替えない。OS scheduling/cache/frequencyは固定していない。

## 対象検証と記録

- configure-release-01/build-core-01、baseline/candidate個別compile、fixture/期待生成、profile/ABBA/BAABすべて成功。
- audit-01: 全36file/1080sampleのraw/QPC/cycle/中央値、20MAP、同vector/archive、最終sourceがpredicateの並べ替えだけであることを照合。
- configure-debug-01/build-debug-01: 正典Debug folio_tests 119actions、変更sourceのformat/静的解析を含め成功。
- contract-01: --markdown-emission成功、stderr空。既存Markdown構文/出力/前後のblock契約を再利用。
- symbols-01: 2libraries、0違反。
- build-measurement-01/allocation-01: 正典eng.coverage.build_measurementとDebug sanitizer vector、--markdown-emission成功。
  確保失敗注入active=1、既存の失敗/所有解放/out保持を確認。ASan/UBSan/nullability/no-recoverとbinding正例/負例を維持。
  実行時interception_win通知1行、命令列0f 29 74 24 30 0f 28 f0は先行と同じ。今回はfunction未同定、全intercept保証はしない。抑制なし。

本taskの測定/対象検証runは子/記録0、記録error0。成功時だけでなく全試行の入力snapshot/hash/raw/終了値を
D:/NeNeFolio/agents/683-table-candidateへ保存する。作業木/出力はD:/NeNeFolio/worktrees/683-table-candidateと同outputs。
最終source/docsSHA自己レビューとsource/buildgraph conformance、最終PR head必須CIのeng/check.ps1を経て正典統合する。
成功した対象は実装不変なら反復しない。通常反映/data全bytes・hash・作成更新日時不変/全保全整理は同agents/integration-completion.jsonへ。

ARC-001/004/008/012、C-004/008/012/014/016、QLT-008/009/010/012/013、GIT-001〜004。
API/schema/版/owner/依存/gate不変。追加waiver none、既存WVR-0001は今回の範囲外。
主担当単体、追加委任なし。実ノート/通常app/Storeを試験に使わず、#634保留は再開しない。
[日報](../reports/2026-11-05.md)・[引き継ぎ](../handoffs/2026-11-05.md)。

自己レビュー初回は補助scriptの数値表参照が前案件syntax-textのままで停止（子1、成果物未生成で記録125/error1）。
今回の検査対象文書一覧から表を読むよう直し、失敗原稿/rawも保持する。製品/test/測定値は不変で対象結果を再利用し、文書検査と最終自己レビューだけ更新する。

## 20:05〜20:06 統合・通常反映・全保全整理完了

#683はPR684/headad1d7e8d3cadb073fd5487b26bcb9f6702b5c9b7、最終CI42015255066 full-gate/check成功。
main17708db74ddcfd301f53b42fc29d661e44e94ab5へ正典統合、Issue683 CLOSED。通常反映後data32fileの全bytes/hash/作成更新日時不変。
通常exeSHAa47cb41d146153f2665d97902b0ccb2d2033280752c5a8df58eb5f46dda1f837、通常app起動なし。
1652file/54432403bytesを全保全ZIP再hash/取込tree/clean-untracked/ignored/link/稼働参照監査後、683専用worktree/outputを削除。
branch/commitと自己レビュー補助参照の失敗を保持。原本D:/NeNeFolio/agents/683-table-candidate/integration-completion.json。
