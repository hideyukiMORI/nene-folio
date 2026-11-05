# Markdown行末読取りの正常本文比較 — 2026-11-05 / #681

既存markdown_line_readでCR/LFを含まない8byteをまとめて進む候補を採用した。
逆順確認の256KiB本文では、一行ASCIIの直接読取りが110.274→39.613µs、日本語一行が111.930→29.275µs、
通常LF行が141.691→51.534µsだった。全文RTFは同じ3条件で2.789→2.640ms、2.831→2.731ms、7.622→6.812ms。
短LF行の直接読取りは229.881→315.430µs（約37.21%増）となる。全文RTFでは24.077→23.931msで約0.61%短縮に留まった。
1024byteの通常LF/CRLF/tab本文の全文RTFは約3.23〜4.78%増、約1.2〜2.19µsの費用増も残す。
全入力の改善や、RichEdit投入/描画/入力を含むUI全体の速度を保証しない。

## 正典と変更

[ADR0070](../adr/0070-markdown-syntax-and-native-tables.md)の長さ付きborrowed lineと既存block/container→RTFを維持する。
src/core/markdown_line.cのprivate unbroken_bytesが、残長8byte以上の場合だけuint64_tへmemcpyする。
CR/LFそれぞれとのXOR後にzero byteの存在を調べ、どちらも含まない塊だけ8byte進む。
引き算の借りが隣laneへ及ぶ可能性を位置情報として利用せず、一致の可能性がある塊と末尾は既存の順次照合で先頭のCR/LFを確定する。
反復中はposition<=length、加算8は残長確認の後、外側startは既存clamp後なので終端を越えて読まない。
その後のCR/LF/CRLF判定、prefixのtab幅、公開field/分類、所有者/確保順/失敗順/保存本文/RTF/構文範囲は不変。
新API/cache/state/計装/外部依存/製品hookを加えない。空見出し等、今回の静読で見た未対応構文をついでに拡張しない。

恒久試験はtests/unit/markdown_tests.cだけを追加。開始offset0〜7、本文長1〜24、終端なし/CR/LF/CRLFの768組合せを
終端NULのない正確な大きさで確保し、borrowed範囲・next・全初期field・元bytes・EOFを比較する。
日本語/emojiとtabの字下げも確認する。既存--markdown-emissionへ含め、新selector/CTest/CMakeは追加しない。

## 比較の方法

基点mainf1b5a13で正典Release core83actionsを構築し、元source・archive・compile vectorを凍結した。
途中の#679統合main3ccd5aeへFFした差分はdocs/native testとCTest登録2行で、src/eng/cmake不変を監査してReleaseを再利用した。
/O2・/Ob2・/DNDEBUG・/MT・C23・/WXを維持し、明示markdown_line objectを同じcore archiveの前にlinkする。
2observer×2版のMAPで14symbol bindingを確認した。製品関数の別実装へ置き換えた測定ではない。

ASCII一行、日本語emoji一行、通常LF、日本語CRLF、猫ab短LF、tabとCRの6分布×目標16/1024/65536/262144byte、24正常入力。
UTF-8文字途中で切らないため、実長は目標より最大3byte短い。直接lineと全文Markdownの48条件。
直接期待はPythonのCRLF/CR/LF分割とtab-stop計算で全line fieldを作り、監査ではbytes.splitlinesと独立したcolumn式から再照合した。
全文RTFの期待は基準の全出力bytes。全72fixture fileのhashを照合し、入力コード/外部URIは実行しない。

同一affinity mask4・優先度不変、1warmup/5sample/run、全結果照合は計時前後。
直接は全文の行読取りとchecksum/progress確認、全文は既存RTF生成/長さ確認/破棄を計時内に置く。
file読取り・fixture作成・note_text生成・全bytes比較は計時外。計測中に親build/archive/hashを並行しない。
profile240sample→ABBA960sample→同source/exeをBAABで確認960sample、計2160sampleが全一致。
ABBAはprofile反復4倍、確認は16倍を既存observer上限65536へcapした。初回確認の上限超過は後述の失敗として保持する。
測定後に製品sourceへ正規formatterを適用し、測定時sourceのclang-format出力と最終sourceの完全一致を監査した。
最終sourceの対象Debug/測定版の検証はその整形後に行った。書式変更だけで性能計測を反復しない。

## 全条件の実測

確認の各版10sample中央値。初回差はABBA、確認値はBAABであり混ぜて中央値を取らない。
時間の単位はµs、差は候補/基準-1。cycle差も各版の中央値から算出した。

| 条件 | 初回時間差 | 確認基準µs | 確認候補µs | 確認時間差 | 確認cycle差 |
| --- | ---: | ---: | ---: | ---: | ---: |
| line-ascii-one-16 | -29.21% | 0.010 | 0.007 | -29.16% | -29.07% |
| markdown-ascii-one-16 | +1.87% | 2.404 | 2.305 | -4.13% | -4.02% |
| line-ascii-one-1024 | -71.95% | 0.440 | 0.121 | -72.47% | -72.32% |
| markdown-ascii-one-1024 | -4.92% | 13.225 | 12.814 | -3.11% | -2.95% |
| line-ascii-one-65536 | -73.41% | 27.609 | 7.153 | -74.09% | -73.88% |
| markdown-ascii-one-65536 | +1.60% | 678.047 | 651.347 | -3.94% | -3.73% |
| line-ascii-one-262144 | -72.67% | 110.274 | 39.612 | -64.08% | -64.07% |
| markdown-ascii-one-262144 | -5.09% | 2789.319 | 2639.888 | -5.36% | -5.58% |
| line-japanese-one-16 | -10.85% | 0.013 | 0.008 | -33.12% | -33.11% |
| markdown-japanese-one-16 | -2.82% | 2.485 | 2.458 | -1.12% | -2.31% |
| line-japanese-one-1024 | -72.92% | 0.440 | 0.121 | -72.50% | -72.17% |
| markdown-japanese-one-1024 | -4.36% | 13.812 | 13.255 | -4.03% | -3.53% |
| line-japanese-one-65536 | -66.03% | 27.787 | 7.151 | -74.27% | -74.26% |
| markdown-japanese-one-65536 | -4.85% | 734.788 | 683.506 | -6.98% | -6.00% |
| line-japanese-one-262144 | -73.43% | 111.930 | 29.275 | -73.85% | -73.63% |
| markdown-japanese-one-262144 | -2.90% | 2830.725 | 2730.591 | -3.54% | -3.51% |
| line-ordinary-lf-16 | -28.95% | 0.010 | 0.007 | -29.16% | -29.20% |
| markdown-ordinary-lf-16 | +0.48% | 2.510 | 2.451 | -2.34% | -2.12% |
| line-ordinary-lf-1024 | -60.54% | 0.564 | 0.210 | -62.73% | -62.76% |
| markdown-ordinary-lf-1024 | +5.07% | 29.742 | 31.163 | +4.78% | +5.12% |
| line-ordinary-lf-65536 | -61.59% | 33.903 | 12.676 | -62.61% | -62.62% |
| markdown-ordinary-lf-65536 | -6.67% | 1857.806 | 1708.300 | -8.05% | -7.66% |
| line-ordinary-lf-262144 | -62.25% | 141.691 | 51.534 | -63.63% | -63.11% |
| markdown-ordinary-lf-262144 | -7.08% | 7622.269 | 6811.909 | -10.63% | -10.74% |
| line-ordinary-crlf-16 | -10.16% | 0.009 | 0.009 | -9.14% | -9.18% |
| markdown-ordinary-crlf-16 | -2.79% | 2.435 | 2.409 | -1.07% | -1.04% |
| line-ordinary-crlf-1024 | -55.02% | 0.472 | 0.209 | -55.83% | -55.31% |
| markdown-ordinary-crlf-1024 | +3.84% | 37.164 | 38.364 | +3.23% | +3.35% |
| line-ordinary-crlf-65536 | -56.27% | 30.480 | 12.891 | -57.71% | -57.47% |
| markdown-ordinary-crlf-65536 | -5.08% | 1810.087 | 1728.681 | -4.50% | -4.84% |
| line-ordinary-crlf-262144 | -56.06% | 119.497 | 52.867 | -55.76% | -55.42% |
| markdown-ordinary-crlf-262144 | -5.00% | 7097.956 | 6754.850 | -4.83% | -4.70% |
| line-short-lf-16 | +39.08% | 0.015 | 0.021 | +38.03% | +37.36% |
| markdown-short-lf-16 | +1.42% | 3.210 | 3.125 | -2.66% | -3.24% |
| line-short-lf-1024 | +41.08% | 0.891 | 1.239 | +39.03% | +38.96% |
| markdown-short-lf-1024 | -1.92% | 99.420 | 98.488 | -0.94% | -0.93% |
| line-short-lf-65536 | +28.89% | 58.287 | 79.825 | +36.95% | +36.91% |
| markdown-short-lf-65536 | -3.96% | 5956.438 | 5894.744 | -1.04% | -0.78% |
| line-short-lf-262144 | +32.00% | 229.881 | 315.430 | +37.21% | +37.45% |
| markdown-short-lf-262144 | -0.30% | 24076.872 | 23930.650 | -0.61% | -0.83% |
| line-tabs-cr-16 | -25.57% | 0.010 | 0.008 | -27.86% | -27.70% |
| markdown-tabs-cr-16 | -2.20% | 2.361 | 2.283 | -3.30% | -2.50% |
| line-tabs-cr-1024 | -32.25% | 0.566 | 0.385 | -31.97% | -32.50% |
| markdown-tabs-cr-1024 | +0.95% | 60.116 | 62.304 | +3.64% | +3.86% |
| line-tabs-cr-65536 | -33.68% | 36.189 | 24.413 | -32.54% | -32.06% |
| markdown-tabs-cr-65536 | +1.28% | 5418.247 | 5449.416 | +0.58% | +0.59% |
| line-tabs-cr-262144 | -32.34% | 144.597 | 100.230 | -30.68% | -30.43% |
| markdown-tabs-cr-262144 | -1.49% | 23601.112 | 23249.134 | -1.49% | -1.65% |

長い通常行の直接改善が両順序で続き、256KiBの全文6分布も確認では約0.61〜10.63%短縮したため採用する。
短LFの直接増加と1KiBの一部全文増加、tab64KiB全文0.58%増を隠さない。短い16byte全文の初回/確認差にも揺れがあり、一般倍率へ外挿しない。

## 対象検証と途中結果

- configure-release-01/build-core-01、baseline/candidate個別compile、fixture/期待生成成功。
- confirm-01は最初の16byte条件で反復262144が既存上限65536を越え、測定器入口で拒否。子/記録1、記録error0、製品処理・sampleは未実行。
  上限を広げずcapしたconfirm-02で確認を完了した。失敗したschedule/source/rawを保持する。
- configure-debug-01成功。build-debug-01は追加試験のmalloc失敗時をrequireだけで止めたため、静的解析が後続参照を指摘して停止（1/1/error0）。
  既存noreturnのtest_failureへ揃えたbuild-debug-02は19actions成功。製品/期待/抑制/ゲートは変えない。
- audit-01は全計測照合後、全run確認で上記build失敗を検出して停止。子1、成果物未生成で記録125/error1。初稿/raw保持。
  失敗の実値を含めたaudit-02で72file/2160sample/14MAP/同vector・archive/最終整形sourceを照合成功。
- contract-01: 正典Debug folio_tests --markdown-emission成功、stderr空。対象の全Markdown構文/出力・768通常行範囲を確認。
- symbols-01: 2libraries、0違反。
- build-measurement-01/allocation-01: 正典eng.coverage.build_measurementとDebug sanitizer vector、--markdown-emission成功。
  確保失敗注入active=1、既存各経路の失敗と所有解放/out保持を確認。ASan/UBSan/nullability/no-recover、bindingの正例成功/負例診断も維持。
  測定実行でinterception_win通知が1行あり、命令列0f 29 74 24 30 0f 28 f0は先行#667/#675と同じ。今回はfunction未同定で、全intercept成功の保証とはしない。抑制なし。

各runの入力snapshot/hash、終了値、raw出力、helper、失敗をD:/NeNeFolio/agents/681-markdown-lineへ保全する。
作業木/出力はD:/NeNeFolio/worktrees/681-markdown-lineと同outputs。成功した対象は実装不変なら再実行しない。
最終source/docsSHA自己レビュー、source/buildgraph conformance、最終PR head必須CIのeng/check.ps1を経て正典統合する。
統合後の通常版反映/data全bytes・hash・作成更新日時不変/全保全整理は同agents/integration-completion.jsonへ。

ARC-001/004/007/008/012、C-004/008/012/014/016、QLT-008/009/010/012/013、GIT-001〜004。
API/schema/版/owner/依存/gate不変。追加waiver none、既存WVR-0001は今回の範囲外。
主担当単体、追加委任なし。実ノート/通常app/Storeは試験に使わず、#634保留は再開しない。
補助readの推測path/glob誤用も全体journalへ残す。[日報](../reports/2026-11-05.md)・[引き継ぎ](../handoffs/2026-11-05.md)。

初回commitはGIT-003の許可typeにないperfを使いhookで拒否（終了1、commit/push/PRなし）。
規則を変えずbranchと件名をrefactorへ訂正した。製品/testは不変で成功対象を再利用し、文書検査と最終SHA照合だけ更新する。
