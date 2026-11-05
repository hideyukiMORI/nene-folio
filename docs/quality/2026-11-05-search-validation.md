# ノート内検索のUTF-16先頭検証 — 2026-11-05（#693）

公開note_search_next/countの同一入力比較で、サロゲートを含まない先頭を4単位ずつ進める候補を採用する。
262144単位のASCII/日本語/末尾絵文字では、逆順確認の前方検索が約92〜93%、件数取得が約31〜36%短縮した。
先頭絵文字には費用が残る。65536単位の後方検索は+5.15%（約18.32µs）、262144単位の件数取得は+3.59%（約51.25µs）。
16単位の先頭絵文字前方は+10.79%だが差は約5.09ns。混在262144単位の件数取得も+2.17%（約28.28µs）。
通常BMP本文での改善とこの限定的な費用を併記する。一般的な倍率や実UIの応答時間は主張しない。

## 正典経路と安全境界

[ADR0023](../adr/0023-in-note-search.md)の公開next/count→private searchable→well_formed→encoded_unitsを維持する。
src/core/note_search.cのprivate scalar_prefixは残長が4char16_t以上の塊だけをmemcpyでuint64_tへ読む。
各16bit単位の上位5bitを0xD800と比べ、サロゲート全域D800〜DFFFが無ければ4単位進む。
減算によるzero検出は存在判定だけに使う。借りによるbitを位置として解釈せず、該当塊の先頭から既存encoded_unitsへ戻る。
末尾0〜3単位も既存処理で確認する。既に確認した先頭は再走査しない。

index<=lengthを維持して残長を先に比較する。8byte alignmentや終端文字を要求しない。
対象Win32の16bit char16_t/64bit uint64_tを使用し、入力変更・確保・新owner・decoder・matcher・APIを加えない。
BAD_SPAN→NO_TERM→語の整形式→本文の整形式、失敗out不変、NUL/CR、ASCII限定fold、重なり/巡回/ordinalを維持する。
Two-Way前処理と一致列には差分がない。保存schema/依存/版/品質ゲートは不変。

独立の読み取り専用レビューは凍結candidate01（SHA256 618e8e028ee89900967cf0df95d7671c79c35e1460e33fd08d1ef2eb0bdae7fa）を対象とし、必須修正0。
mask・存在判定・残長/加算・alignment・拒否順序・既存matcher/借用契約を静読した。生成コード・実行・親の追加testはそのレビューに含めない。

## 比較方法と全結果

基点maine17c178、D専用refactor/693-search-validation。正典Release core83actionsから同じcompile vector/archiveを凍結した。
/O2 /Ob2 /DNDEBUG /MT C23 /WXを保持し、明示note_search objectだけを差し替える。
MAPでnext/countが各明示object、ascii_fold_unitが同じ正典archive由来と照合した。製品への計測口はない。
#691のtest/docs統合mainbd2394fへFFしてもsrc/eng/cmake/CMakeは不変で、Release結果を再利用した。

5分布×16/1024/65536/262144 UTF-16単位、20有効本文/40file、4公開操作で80条件。
asciiは英数字文、japaneseは日本語BMP文、mixedは日本語/絵文字/英数字文、early_emoji/late_emojiはASCII本文の先頭/末尾だけに絵文字を置く。
語はab（大小無視）、無一致はzzZZ。Pythonのstrict UTF16LE往復と独立ASCII-fold単位走査で、全span/count/ordinalの固定期待値を生成した。
forwardはcaret0、backwardは本文末尾、countのordinal位置は本文中央、missingは無一致count。入力読込は計時外。
計時中も全呼出しのenum/span/count/ordinalを比較し、計時前後に同じ結果と入力全bytes不変を確認する。

profileは各条件5sample、比較はABBAで各版10sample、同じexeの確認はBAABで各版10sample、合計3600sample全結果一致。
基準反復はmax(1,min(16384,131072/units))、比較4倍、確認16倍、いずれも65536上限。
1sampleの値は公開API呼出し・固定結果照合・反復loopを含む。UTF-16検証だけの時間ではない。
単一threadだけaffinity mask4、priority不変、QPCとthread cyclesを記録し、cycleをCPU時間へ換算しない。
Core i9-10850K/20logical processors、Windows11 Pro build26300。親のbuild/archive/hashを計時中に重ねず、開始時の既知build/test/observerは0。
他process、CPU周波数、cache、全OS idleは制御しない。短文や単発の時間差には測定の揺れも含む。

初回の末尾絵文字262144単位backwardは+17.66%だったが、同exeの逆順確認では-32.75%となった。
短いmixedのmissingも+25.41%から-8.44%へ変化した。初回の不利な値を捨てず、全80行を並記する。
差は候補/基準−1、負が短縮。raw全標本と全数値はagentsのmeasure-*.json/measurement-audit.jsonに保持する。

| 分布 | UTF-16単位 | 公開操作 | 確認: 基準µs | 確認: 候補µs | 初回時間差% | 確認時間差% | 確認cycle差% |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| ascii | 16 | forward | 0.045807 | 0.022336 | -2.62 | -51.24 | -51.23 |
| ascii | 16 | backward | 0.087070 | 0.065115 | -21.28 | -25.22 | -25.47 |
| ascii | 16 | count | 0.085935 | 0.066699 | -23.61 | -22.38 | -22.51 |
| ascii | 16 | missing | 0.090211 | 0.067906 | -23.98 | -24.73 | -24.67 |
| ascii | 1024 | forward | 2.142163 | 0.172070 | -92.19 | -91.97 | -91.94 |
| ascii | 1024 | backward | 5.544946 | 3.854419 | -35.48 | -30.49 | -30.70 |
| ascii | 1024 | count | 5.655371 | 3.860767 | -29.97 | -31.73 | -31.68 |
| ascii | 1024 | missing | 5.550537 | 3.589331 | -9.22 | -35.33 | -35.30 |
| ascii | 65536 | forward | 136.853125 | 9.467187 | -93.08 | -93.08 | -92.99 |
| ascii | 65536 | backward | 360.246875 | 244.540625 | -33.13 | -32.12 | -32.04 |
| ascii | 65536 | count | 358.959375 | 240.382812 | -33.10 | -33.03 | -32.49 |
| ascii | 65536 | missing | 342.428125 | 214.507812 | -38.43 | -37.36 | -37.00 |
| ascii | 262144 | forward | 542.965625 | 39.543750 | -92.96 | -92.72 | -92.68 |
| ascii | 262144 | backward | 1438.043750 | 975.243750 | -31.60 | -32.18 | -31.81 |
| ascii | 262144 | count | 1440.462500 | 989.318750 | -31.71 | -31.32 | -30.94 |
| ascii | 262144 | missing | 1371.018750 | 876.068750 | -37.86 | -36.10 | -36.37 |
| japanese | 16 | forward | 0.045918 | 0.022371 | -49.55 | -51.28 | -50.88 |
| japanese | 16 | backward | 0.086438 | 0.065832 | -19.14 | -23.84 | -23.74 |
| japanese | 16 | count | 0.086228 | 0.066183 | -24.61 | -23.25 | -23.32 |
| japanese | 16 | missing | 0.090214 | 0.067449 | -27.08 | -25.23 | -25.11 |
| japanese | 1024 | forward | 2.129810 | 0.170313 | -91.99 | -92.00 | -91.96 |
| japanese | 1024 | backward | 5.348096 | 3.468213 | -51.61 | -35.15 | -34.88 |
| japanese | 1024 | count | 5.371509 | 3.419214 | -36.94 | -36.35 | -35.99 |
| japanese | 1024 | missing | 5.324683 | 3.447925 | -36.86 | -35.25 | -35.28 |
| japanese | 65536 | forward | 134.470313 | 9.364063 | -93.35 | -93.04 | -93.01 |
| japanese | 65536 | backward | 342.629687 | 220.071875 | -36.08 | -35.77 | -35.58 |
| japanese | 65536 | count | 343.473437 | 227.198438 | -37.26 | -33.85 | -34.03 |
| japanese | 65536 | missing | 338.523438 | 220.923438 | -37.32 | -34.74 | -35.74 |
| japanese | 262144 | forward | 542.181250 | 42.515625 | -92.92 | -92.16 | -92.40 |
| japanese | 262144 | backward | 1378.065625 | 896.609375 | -35.72 | -34.94 | -34.98 |
| japanese | 262144 | count | 1367.206250 | 877.912500 | -34.78 | -35.79 | -35.74 |
| japanese | 262144 | missing | 1363.725000 | 863.228125 | -36.49 | -36.70 | -36.92 |
| mixed | 16 | forward | 0.042319 | 0.036577 | -14.88 | -13.57 | -13.55 |
| mixed | 16 | backward | 0.084319 | 0.079791 | -5.37 | -5.37 | -5.30 |
| mixed | 16 | count | 0.083599 | 0.079415 | -4.36 | -5.00 | -4.55 |
| mixed | 16 | missing | 0.086467 | 0.079172 | +25.41 | -8.44 | -8.55 |
| mixed | 1024 | forward | 1.892456 | 1.875171 | +1.35 | -0.91 | -0.82 |
| mixed | 1024 | backward | 5.177002 | 5.077979 | +4.29 | -1.91 | -0.85 |
| mixed | 1024 | count | 5.108594 | 5.093726 | -1.14 | -0.29 | -0.50 |
| mixed | 1024 | missing | 5.105835 | 5.173291 | -0.29 | +1.32 | +0.67 |
| mixed | 65536 | forward | 120.381250 | 119.889062 | -2.29 | -0.41 | -0.42 |
| mixed | 65536 | backward | 324.634375 | 329.765625 | +2.89 | +1.58 | +1.09 |
| mixed | 65536 | count | 326.204687 | 335.125000 | +8.00 | +2.73 | +3.12 |
| mixed | 65536 | missing | 323.915625 | 323.268750 | -0.46 | -0.20 | +0.05 |
| mixed | 262144 | forward | 485.175000 | 482.962500 | +0.57 | -0.46 | -0.10 |
| mixed | 262144 | backward | 1315.209375 | 1329.950000 | -0.59 | +1.12 | +0.91 |
| mixed | 262144 | count | 1306.125000 | 1334.409375 | -0.13 | +2.17 | +1.96 |
| mixed | 262144 | missing | 1309.131250 | 1326.768750 | +2.39 | +1.35 | +0.64 |
| early_emoji | 16 | forward | 0.047197 | 0.052291 | +6.21 | +10.79 | +10.75 |
| early_emoji | 16 | backward | 0.083037 | 0.087187 | +1.42 | +5.00 | +5.06 |
| early_emoji | 16 | count | 0.086262 | 0.087533 | +8.51 | +1.47 | +1.50 |
| early_emoji | 16 | missing | 0.090376 | 0.086926 | +6.01 | -3.82 | -3.79 |
| early_emoji | 1024 | forward | 2.130933 | 2.137939 | +4.45 | +0.33 | +0.30 |
| early_emoji | 1024 | backward | 5.532446 | 5.577124 | +3.09 | +0.81 | +0.65 |
| early_emoji | 1024 | count | 5.660034 | 5.680908 | +3.60 | +0.37 | +0.26 |
| early_emoji | 1024 | missing | 5.432642 | 5.336987 | -0.23 | -1.76 | -1.69 |
| early_emoji | 65536 | forward | 135.240625 | 136.101562 | -0.19 | +0.64 | +0.65 |
| early_emoji | 65536 | backward | 356.023438 | 374.342188 | +2.08 | +5.15 | +4.49 |
| early_emoji | 65536 | count | 358.987500 | 374.295313 | +2.57 | +4.26 | +3.03 |
| early_emoji | 65536 | missing | 346.140625 | 338.659375 | -2.17 | -2.16 | -1.27 |
| early_emoji | 262144 | forward | 554.396875 | 543.140625 | -0.07 | -2.03 | -2.10 |
| early_emoji | 262144 | backward | 1436.821875 | 1472.306250 | +1.85 | +2.47 | +2.49 |
| early_emoji | 262144 | count | 1429.593750 | 1480.846875 | +4.04 | +3.59 | +3.46 |
| early_emoji | 262144 | missing | 1370.321875 | 1376.837500 | +0.77 | +0.48 | +0.48 |
| late_emoji | 16 | forward | 0.043042 | 0.023651 | -47.19 | -45.05 | -45.04 |
| late_emoji | 16 | backward | 0.086045 | 0.065300 | -22.79 | -24.11 | -24.11 |
| late_emoji | 16 | count | 0.083820 | 0.065995 | -22.67 | -21.27 | -21.26 |
| late_emoji | 16 | missing | 0.087852 | 0.070894 | -16.81 | -19.30 | -19.43 |
| late_emoji | 1024 | forward | 2.119946 | 0.172412 | -91.87 | -91.87 | -91.86 |
| late_emoji | 1024 | backward | 5.541626 | 3.603003 | -14.96 | -34.98 | -34.98 |
| late_emoji | 1024 | count | 5.596899 | 3.788257 | -33.57 | -32.32 | -32.74 |
| late_emoji | 1024 | missing | 5.353906 | 3.398584 | -38.74 | -36.52 | -36.75 |
| late_emoji | 65536 | forward | 136.884375 | 9.265625 | -93.87 | -93.23 | -93.17 |
| late_emoji | 65536 | backward | 364.953125 | 237.643750 | -33.63 | -34.88 | -33.66 |
| late_emoji | 65536 | count | 356.960937 | 239.593750 | -28.82 | -32.88 | -33.06 |
| late_emoji | 65536 | missing | 349.621875 | 214.731250 | -42.19 | -38.58 | -38.39 |
| late_emoji | 262144 | forward | 551.956250 | 40.090625 | -93.02 | -92.74 | -92.74 |
| late_emoji | 262144 | backward | 1450.037500 | 975.112500 | +17.66 | -32.75 | -32.57 |
| late_emoji | 262144 | count | 1446.537500 | 1001.831250 | -31.25 | -30.74 | -30.57 |
| late_emoji | 262144 | missing | 1385.725000 | 864.528125 | -28.20 | -37.61 | -37.65 |

## 対象検証と全試行

全実行をtools/record-run.pyの新run-dirへ記録し、入力snapshot/hash/raw/終了値を保持した。
configureRelease01/buildCore01、fixtures01、compileBaseline01/compileCandidate01、profile01/compare01/confirm01は成功。
audit01は40file/80条件/3600sample/全中央値/6MAP行/同vectorとarchive/最終製品sourceを照合して成功した。

Debug準備補助の最初の生成ではtarget名の一括置換がformatter対象pathにも及び、自己assertで停止した。
configure/build/試験はまだ起動していなかった。初稿を保持し、--targetだけの置換へ直した。製品/規則の変更はない。
configureDebug01/buildDebug01（119actions）、初回--note-search、symbols01（2library/違反0）は成功、stderr空。
自己レビューで既存の短い不正UTF16例は4単位処理へ入らないと確認し、固定拒否例を追加した。
buildDebug02は変更したunitの2actions、contract02の最終--note-searchは成功、stderr空。製品・測定exe・library不変なので性能比較/symbolsを再実行しない。

既存tests/unit/search_tests.cへ、offset0〜3×長さ1〜32の終端無し・正確な確保長を使う2112caseを加えた。
128caseはNUL/CRとBMP境界値を含む正常列、1984caseはその各位置に有効surrogate対を置く。
同じ全本文を語にした前後検索/件数・ordinalと入力全bytes不変を、ASan/UBSan/nullability付きの正典Debugで確認する。
さらに8単位の固定5配列（4laneの孤立high/lowと末尾high）を本文/語の各側へ置き、30公開呼出しのMALFORMEDと全out不変を確認する。
既存4719短query oracle、重なり・巡回・拒否優先などはそのまま維持する。新selector/CTestはない。
新たなJSON変異試験や保留#634の再試行は行わず、性能比較の入力はすべて正常なUTF16である。

最終source/test/docsの差分・hash・文書リンクとconformanceを照合し、最終PR head必須CIでeng/check.ps1を要求する。
通常app/実ノート/Storeを試験には使わない。統合後は通常buildを反映し、実data32fileの全bytes/hash/作成更新日時不変を確認する。
全fileのZIP/hashと取込tree/未保存・untracked・ignored/link/稼働参照を監査後、追加worktree/outputだけを整理する。branch/commitと全rawを保持する。
原本D:/NeNeFolio/agents/693-search-validation/、最終CI/統合/通常反映/全保全整理は同integration-completion.json。

ARC-001/007/008/010/012、C-004/005/008/012/014/016、QLT-008/010/012/013、GIT-001〜004。
主担当単体で実装/計測/試験し、必要なbit判定の独立静読だけを委任。追加waiver none、既存WVR-0001継続。
[日報](../reports/2026-11-05.md)・[引き継ぎ](../handoffs/2026-11-05.md)。

## 21:30〜21:32 統合・通常反映・全保全整理完了

#693はPR694/head6109a3779c4876be4ed4cdc9a4923e9df5465e61、最終CI42026620126 full-gate/check成功、mainb898c79413b578bbfdb96c615ee0528ddae62feeへ正典統合。Issue693 CLOSED。
通常版を同mainへ反映し、exeSHA256は80633f7b61241e75577f2c4095fe87914a291c45e6b2027e63a3d98904f9bd8f。実data32fileの全bytes/hash/作成更新日時不変、通常app起動なし。
1529file/41081086bytesの全保全ZIP再hash、取込tree/未保存・untracked・ignored/link/稼働参照監査後、693専用worktree/outputだけ削除。
branch/commitと全rawを保持。原本D:/NeNeFolio/agents/693-search-validation/integration-completion.json。
