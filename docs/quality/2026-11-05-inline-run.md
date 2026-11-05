# 行内記号runの開始文字再利用 — 2026-11-05（#687、不採用）

markdown_inline_tokensのextentが読んだ開始文字を内部runへ渡し、同じ位置のmarkdown_view_atを反復しない候補を比較した。
短い強調/コード/引用には短縮が見られたが、長い通常ASCII/日本語は両順序で約2〜3.5%増となったため不採用とした。
**最終製品は元sourceの全bytes/SHAへ復帰し、src/tests/eng/CMakeの差分は0。文書だけを収載する。**

## 比較方法

main17708db（#683統合済み）から正典Release core83actionsを作り、同compile vector/同archiveと
markdown_inline_tokensの明示objectを使って基準/候補の観測exeを作成した。MAPでinline_tokens_renderの明示objectと
markdown_rtf_create/markdown_view_at/rtf_text_appendの正典archive束縛、計8項目を照合した。
候補の差分はrunのchar引数、比較式、呼出元の3箇所だけ。構文/借用view/所有/確保/公開APIの意味は変えない。

正常UTF-8のASCII/日本語/強調/コード/通常2space改行/引用/表/混合8分布×短長、16文書を使用する。
短は各文書片4回、長は256回。コード表記は文字列であり実行しない。通常段落の対照も含め、入力とRTF全32fileをhash固定する。
全文RTF期待は基準Release生成の全bytes。独立の意味期待に代わるものではなく、既存の恒久unit期待を変更しない。

各processはCPU affinity4、warmup1、5sample、priority変更なし。計時前後で全文bytes、計時内でも毎回長さを確認する。
profile80sample、ABBA320sample、同exeの逆順BAAB/反復4倍で320sample、計720sampleが全出力一致。
QPCとthread cycleのraw/中央値、32filehash、同vector/archive、3箇所差分をaudit-01で照合した。
OS scheduling/cache/frequencyは固定していない。固定文書の計測で、一般UI性能の保証やcode layoutを原因とする断定はしない。

## 全結果（候補を不採用）

時間は全文RTF1回あたりmicroseconds。初回と確認はそれぞれの基準から差分を計算し、負数が短縮を示す。

| 条件 | 初回時間差 | 確認基準µs | 確認候補µs | 確認時間差 | 確認cycle差 |
| --- | ---: | ---: | ---: | ---: | ---: |
| markdown-ascii-short | +2.48% | 6.875 | 6.911 | +0.52% | -0.55% |
| markdown-japanese-short | +2.36% | 8.698 | 8.687 | -0.12% | -0.32% |
| markdown-emphasis-short | -4.39% | 14.920 | 14.371 | -3.68% | -3.65% |
| markdown-code-short | -0.90% | 14.712 | 13.489 | -8.31% | -8.38% |
| markdown-hardbreak-short | -0.25% | 9.665 | 9.259 | -4.20% | -4.41% |
| markdown-quote-short | -3.87% | 21.941 | 20.634 | -5.95% | -5.86% |
| markdown-table-short | +8.34% | 22.842 | 21.513 | -5.82% | -6.13% |
| markdown-mixed-short | +1.07% | 26.064 | 26.557 | +1.89% | +1.80% |
| markdown-ascii-long | +3.29% | 444.766 | 455.416 | +2.39% | +3.53% |
| markdown-japanese-long | +2.17% | 582.167 | 602.294 | +3.46% | +4.01% |
| markdown-emphasis-long | -1.12% | 1186.123 | 1179.309 | -0.57% | -0.68% |
| markdown-code-long | +3.52% | 1188.734 | 1181.293 | -0.63% | -0.64% |
| markdown-hardbreak-long | +3.14% | 597.971 | 597.690 | -0.05% | -0.02% |
| markdown-quote-long | +0.13% | 1469.847 | 1449.980 | -1.35% | -1.34% |
| markdown-table-long | +0.65% | 1219.652 | 1223.811 | +0.34% | +0.40% |
| markdown-mixed-long | +1.27% | 1599.248 | 1605.267 | +0.38% | +0.23% |

長いASCIIは初回+3.29%/確認+2.39%、長い日本語は+2.17%/+3.46%。短いコードの確認-8.31%など利点も残すが、
普段の本文処理に採用する根拠が足りない。短い表は初回+8.34%/確認-5.82%へ振れたため一回の結果だけで採否を決めない。
候補source/exe/MAP/rawは保全し、製品sourceは元bytesへ復帰した。性能を理由に新APIや別実装経路を増やさない。

## 実行・全試行の記録

作業木D:/NeNeFolio/worktrees/687-inline-run、出力D:/NeNeFolio/outputs/687-inline-run。
全実行原本はD:/NeNeFolio/agents/687-inline-run。tools/record-run.pyで入力snapshot/hash/raw/終了値を残す。
fixtures/configure-release/build-core/reference/compile-baseline/expected/profile/compile-candidate/compare/confirm/auditは子/記録0、記録error0。
準備補助は旧taskにないprepare-expected.py参照で一度exit1、直後のconfigure/build要求もinput-list未生成で入口exit1。
子build/testは起動していない。実在するhelperと今回のobserver/module名へ直し、全経緯をjournalへ保持する。

#685統合main2c5d5fcへFFした差分は既存native回帰と2行のCTest登録/文書のみ。測定の製品src/eng/compile vector/archiveは不変。
不採用で製品変更がないためDebug/確保失敗注入/ゲームやnative試験を追加反復しない。文書/source/buildgraph conformanceと
最終SHA/数値表/全runの自己レビュー、最終PR headの必須CIで正典eng/check.ps1を要求する。
統合後の通常exe/data不変/全保全/取込・未保存・ignored・link・稼働参照監査/専用整理は同agents/integration-completion.jsonへ。
branch/commitと全候補/試行を保持する。

ARC-001/004/008/012、C-004/008/012/014/016、QLT-008/009/010/012/013、GIT-001〜004。
API/schema/版/所有/依存/gate不変、追加waiver none、既存WVR-0001継続。
主担当単体、追加委任なし。実ノート/通常app/Storeを試験に使わず、#634保留を再開しない。
[日報](../reports/2026-11-05.md)・[引き継ぎ](../handoffs/2026-11-05.md)。
