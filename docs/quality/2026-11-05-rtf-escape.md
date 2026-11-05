# RTF記号の一括追加を比較し、採用を見送る — #673

記号escapingの2回appendを1回へ減らす3候補を比較した。記号を含む直接変換は改善したが、
長い記号なしMarkdownで約18%/10%/11%増えたため、全候補を不採用とした。
元sourceの全bytes/SHAへ戻し、製品/test/build/schemaを変更せず計測記録だけを収載する。

## 方法と対象

main `96a907936b76ef6147973f94f383047dc438512e`から正典Release coreを作り、rtf_text source/compile vector/archiveを凍結した。
途中で#671統合main `547d745a7c05010966559287578181087f3b14de`へFFし、src/eng/CMake不変を確認した。
各observerは同じRelease vectorの明示rtf_text objectと同corearchiveをlinkする。MAPで4symbolの所属を各版で照合した。
/O2、/Ob2、/DNDEBUG、-MT、C23、/WXを含むflagsは全版同一、保護設定・依存・抑制を変更しない。

通常のWindowsパス、波括弧を含むC風コード、日本語/絵文字を含むCSS風コード、記号無し対照を使う。
各snippetを16/512回並べ、直接rtf_text_appendとfenced Markdown全体を分けた16条件・32UTF8fileである。
入力codeは実行しない。直接escapingの期待は独立Python bytes置換（CR除去、backslashと左右braceのescape）で作る。
全RTFの期待は凍結baselineの出力を保存し、candidateが全bytesを維持するか比較する。後者は独立Markdown oracleとは呼ばない。

profileは1warmup/5samples、16条件80sample。batchが約0.69〜7.02msだったため、比較では両版の反復数を16倍にした。
各候補ごとにbaseline→candidate→candidate→baseline、条件順も交互に反転する。各版/条件10sample、3比較合計960sample。
同一thread affinity mask4、priority不変、計時中に親のbuild/archive無し。通常の生成/追加/解放と長さ確認を計時に含め、
完全bytes比較はwarmupと計時後に行う。QPCとthread cyclesの全rawを保持する。

## 候補と採否

- 候補1: 2byteのstack配列にescapeと元文字を作り、一度appendする。
- 候補2: 既知の3種類の文字列literalから選び、一度appendする。
- 候補3: 同じ3組を一つのstatic const二次元表に置き、組を選んで一度appendする。

どの版も出力は全一致した。assembly比較では候補1にstack cookie処理と末尾callの形の変化が見えたが、
これは観測した構造差であり、全遅延の原因を分離測定した結果ではない。候補2/3も対照全体で悪化が残った。
直接変換の改善だけで通常表示全体の改善と扱わず、全候補を不採用とした。
以下は各候補自身のABBAの中央値で、別比較のbaselineを混ぜない。単位は1operationあたりµs、差は候補/基準−1。

## 候補1 — stack pair（不採用）

| 条件 | 基準 µs | 候補 µs | QPC差 | cycles差 |
| --- | ---: | ---: | ---: | ---: |
| literal-paths-16 | 1.912 | 1.793 | -6.20% | -5.71% |
| markdown-paths-16 | 5.515 | 5.349 | -3.01% | -2.32% |
| literal-paths-512 | 55.836 | 49.645 | -11.09% | -10.54% |
| markdown-paths-512 | 111.084 | 106.282 | -4.32% | -4.88% |
| literal-braces-16 | 1.446 | 1.355 | -6.28% | -6.57% |
| markdown-braces-16 | 27.515 | 27.209 | -1.11% | -0.77% |
| literal-braces-512 | 37.893 | 36.134 | -4.64% | -4.22% |
| markdown-braces-512 | 813.364 | 801.085 | -1.51% | -1.58% |
| literal-mixed-16 | 2.188 | 1.980 | -9.53% | -9.79% |
| markdown-mixed-16 | 7.108 | 6.838 | -3.80% | -4.38% |
| literal-mixed-512 | 61.210 | 53.911 | -11.92% | -11.90% |
| markdown-mixed-512 | 156.633 | 152.505 | -2.64% | -2.90% |
| literal-plain-16 | 0.936 | 1.087 | +16.11% | +15.84% |
| markdown-plain-16 | 4.490 | 4.664 | +3.88% | +3.34% |
| literal-plain-512 | 25.643 | 30.668 | +19.60% | +19.47% |
| markdown-plain-512 | 84.601 | 99.866 | +18.04% | +18.09% |

## 候補2 — 固定literal（不採用）

| 条件 | 基準 µs | 候補 µs | QPC差 | cycles差 |
| --- | ---: | ---: | ---: | ---: |
| literal-paths-16 | 1.986 | 1.539 | -22.51% | -22.16% |
| markdown-paths-16 | 5.410 | 5.065 | -6.39% | -6.43% |
| literal-paths-512 | 55.834 | 42.272 | -24.29% | -24.51% |
| markdown-paths-512 | 107.924 | 95.101 | -11.88% | -11.96% |
| literal-braces-16 | 1.422 | 1.209 | -14.96% | -14.87% |
| markdown-braces-16 | 27.521 | 27.691 | +0.62% | +0.41% |
| literal-braces-512 | 37.565 | 30.770 | -18.09% | -18.02% |
| markdown-braces-512 | 810.677 | 805.013 | -0.70% | -0.58% |
| literal-mixed-16 | 2.179 | 1.796 | -17.58% | -17.75% |
| markdown-mixed-16 | 7.042 | 6.565 | -6.77% | -6.80% |
| literal-mixed-512 | 60.638 | 49.389 | -18.55% | -18.47% |
| markdown-mixed-512 | 155.448 | 144.830 | -6.83% | -6.53% |
| literal-plain-16 | 0.928 | 0.902 | -2.80% | -2.85% |
| markdown-plain-16 | 4.444 | 4.430 | -0.31% | -0.26% |
| literal-plain-512 | 25.844 | 24.755 | -4.22% | -4.15% |
| markdown-plain-512 | 83.483 | 91.784 | +9.94% | +9.88% |

## 候補3 — 固定表（不採用）

| 条件 | 基準 µs | 候補 µs | QPC差 | cycles差 |
| --- | ---: | ---: | ---: | ---: |
| literal-paths-16 | 2.005 | 1.564 | -21.99% | -22.01% |
| markdown-paths-16 | 5.451 | 5.108 | -6.29% | -5.88% |
| literal-paths-512 | 54.127 | 43.168 | -20.25% | -20.17% |
| markdown-paths-512 | 109.150 | 99.109 | -9.20% | -9.58% |
| literal-braces-16 | 1.415 | 1.218 | -13.92% | -14.03% |
| markdown-braces-16 | 27.589 | 27.348 | -0.87% | -0.44% |
| literal-braces-512 | 37.318 | 30.600 | -18.00% | -18.32% |
| markdown-braces-512 | 818.082 | 798.648 | -2.38% | -2.33% |
| literal-mixed-16 | 2.152 | 1.758 | -18.34% | -18.40% |
| markdown-mixed-16 | 6.968 | 6.534 | -6.23% | -6.65% |
| literal-mixed-512 | 60.833 | 47.823 | -21.39% | -21.34% |
| markdown-mixed-512 | 156.716 | 144.651 | -7.70% | -7.67% |
| literal-plain-16 | 0.931 | 0.896 | -3.77% | -3.75% |
| markdown-plain-16 | 4.406 | 4.426 | +0.45% | +0.53% |
| literal-plain-512 | 25.549 | 25.062 | -1.91% | -1.43% |
| markdown-plain-512 | 83.477 | 92.598 | +10.93% | +10.84% |

## 検証と記録

configure-release-01/build-core-01（83actions）、fixtures-01/reference-01/compile-baseline-01/expected-01/profile-01、
compile-candidate-01〜03/compare-01〜03、assembly-01/audit-01は全て子/記録0、記録error0、入力終了時不変。
auditは32fixture全hash、UTF8/独立escaping期待、全208raw/1040sampleのQPC/cycles/中央値、16MAPbinding、
同flags/archive、FFの非製品差分、元sourceへ全bytes復帰を照合した。候補source/exe/MAP/rawも保全対象とする。

不採用で製品/test/eng/CMake差分0のため、採用時用のDebug/確保注入/通常buildは実行しない。
文書conformance/相対参照と自己レビューで最終文書SHAを照合し、最終PR head必須CIの正典eng/check.ps1を要求する。
一般UI性能・全入力の率・原因の断定はしない。比較した正常fixtureの範囲に限る。

規則: ARC-001/004/008/012、C-004/008/012/016、QLT-008/009/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。親単体、追加委任なし。通常app/実ノート/Store/#634保留試験は対象外。
原本D:/NeNeFolio/agents/673-rtf-escape/、全体journalは同quality-20261105/journal.md。
最終CI/統合/通常exe-data不変/全保全と専用worktree-output整理は同taskのintegration-completion.jsonへ。branch/commit保持。

## 18:35 統合・全保全整理完了

#673はPR674/head883fa105764901033aae70a9920f1c208559f093、最終CI42002490894 full-gate/check成功。
main85d664408fee423e3339fe0e31544f54d010881aへ正典統合、Issue673 CLOSED。全候補不採用で製品差分0、通常exe/data32file不変。
1357file/29525693bytes全保全ZIP再hash/取込tree/clean-untracked/ignored/link/稼働参照監査後、673専用worktree/outputのみ削除。
branch/commit保持、原本D:/NeNeFolio/agents/673-rtf-escape/integration-completion.json。
