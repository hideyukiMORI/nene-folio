# 同じ文字単位の検索比較 — 2026-11-06（#763）

private same_unitでraw UTF-16単位が等しいときの二回のascii_fold_unit呼出しを省く候補を採用する。
BAAB確認の262144単位countはJapanese841.078→669.638µs（-20.38%）、emoji1013.753→797.547µs（-21.33%）、
repeat_exact1365.900→1114.116µs（-18.43%）、repeat_fold1400.213→1223.016µs（-12.65%）。同条件のABBAでも利益を確認した。
後方もJapanese-16.62%、emoji-24.76%、repeat_exact-20.91%、repeat_fold-10.95%となった。

費用もある。BAABのfolded_ascii262144countは+3.42%（31.72µs）、mixed_case同countは+3.59%（32.84µs）。
folded_ascii同前方は+8.36%（3.19µs）。16単位folded_ascii countは+20.06%だが差は12.75ns、mixed_case countは+12.35%/7.87ns。
Japanese/emoji/反復語の両順序の利益とこの費用を併記して採用し、全入力の非退行や実UI応答の改善を主張しない。

## 正典経路と意味

[ADR0023](../adr/0023-in-note-search.md)の単一Two-Way一致列の中で、same_unitの比較式1行だけを変更する。
left==rightなら同じ純粋なascii_fold_unitを適用した結果も必ず同じなのでtrue。異なる値は従来と同じ二つのfold結果を比較する。
[ADR0032](../adr/0032-languages.md)のASCII限定fold規則を複製せず、maximal_suffixのfold後の順序判定は変更しない。
split/period/memory/advance、比較の位置と順序、UTF-16検証、拒否順序/失敗out/NUL/CR/重複/巡回/ordinalはすべて不変。
借用契約/owner/確保/環境参照/API/schema/依存/CMake/版/ゲートに差分はない。

旧#448のfold呼出数/2は、raw短絡後の文字対比較回数ではなくなる。その計測値を新しい比較回数として流用しない。
今回、比較の前進制御と真偽の意味が不変である1行差分を照合し、既存の線形走査を維持する。新たな比較回数測定や一般実時間上限は主張しない。

## 同条件比較と全結果

base393c253b2dd934dea33dd9e67fd1066cf6be2a5cから正典Release core83actionsを生成し、source/archive/compile vectorを凍結。
/O2 /Ob2 /DNDEBUG /MT C23 /WXを保持。同archive/observer/linkvectorで明示note_search objectだけを変え、next/count/ascii_foldの6MAP行を照合した。
過去の測定値をbaselineへ流用せず、製品に計測口を加えていない。

exact_ascii（ab）、folded_ascii（ABをab検索）、mixed_case（Ab/aBをab検索）、japanese（猫と鳥）、emoji（😀猫🐾）、
repeat_exact（a反復）、repeat_fold（Aa反復をa列検索）の7分布。16/1024/65536/262144単位の28本文/語組、期待値も含む84file。
反復語長はmin(255,本文長/2)、他の語は2/3/5単位。forward/backward/count/missingの112条件を比較する。
各本文/語はPython strict UTF16LE往復を通し、独立ASCII-fold全単位走査で重複する全一致から前後span/count/中央ordinalを算出する。
missingだけは固定zzZZでcountし、全本文にzが無いことを独立確認した。本文/語のファイル読込は計時外。
全公開呼出しのenum/span/count/ordinalを照合し、計時前後に本文と語の全bytes不変を確認する。

ABBA/BAAB各4round×112条件×5sample、各2240、総計4480sample。各版は各順序10sample。
基準反復max(1,min(16384,131072/units))、ABBA4倍/BAAB16倍、上限65536。
公開呼出しと結果照合/loopを含み、比較式だけの時間ではない。単一thread affinity mask4、priority不変、QPC/thread cyclesを記録。
Core i9-10850K/20logical、Windows11 Pro26300。開始時既知compiler/test/observer0、計時中にbuild/archive/hashを重ねず、BAAB終了後にDebugを開始した。
他process/CPU周波数/cache/全OS idleは制御しない。小差には揺れも含む。
exact_ascii262144前方はABBA-13.37%からBAAB+0.01%、repeat_exact同前方は+6.40%から-1.07%となった。初回の値も捨てない。
時間差は候補/基準−1、負が短縮。全生値とcycleはagents/measure-*.json、再計算はmeasurement-audit.jsonに保持する。

| 分布 | 本文単位 | 操作 | BAAB基準µs | BAAB候補µs | ABBA時間差% | BAAB時間差% | BAABcycle差% |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| exact_ascii | 16 | forward | 0.023139 | 0.020232 | -13.23 | -12.56 | -12.73 |
| exact_ascii | 16 | backward | 0.064350 | 0.070332 | +9.61 | +9.30 | +8.84 |
| exact_ascii | 16 | count | 0.063541 | 0.069792 | +7.76 | +9.84 | +9.75 |
| exact_ascii | 16 | missing | 0.065534 | 0.070972 | +6.99 | +8.30 | +8.28 |
| exact_ascii | 1024 | forward | 0.170898 | 0.168115 | -1.54 | -1.63 | -1.57 |
| exact_ascii | 1024 | backward | 3.679736 | 3.651782 | -0.80 | -0.76 | -0.78 |
| exact_ascii | 1024 | count | 3.566724 | 3.592310 | +0.04 | +0.72 | +0.69 |
| exact_ascii | 1024 | missing | 3.448828 | 3.378442 | +0.28 | -2.04 | -1.99 |
| exact_ascii | 65536 | forward | 9.285938 | 9.262500 | +8.62 | -0.25 | -0.25 |
| exact_ascii | 65536 | backward | 231.029687 | 228.089063 | +1.38 | -1.27 | -1.35 |
| exact_ascii | 65536 | count | 232.776563 | 231.192187 | -0.66 | -0.68 | -0.76 |
| exact_ascii | 65536 | missing | 221.285937 | 213.553125 | +1.14 | -3.49 | -3.26 |
| exact_ascii | 262144 | forward | 38.300000 | 38.303125 | -13.37 | +0.01 | +0.06 |
| exact_ascii | 262144 | backward | 984.881250 | 920.237500 | +0.23 | -6.56 | -6.58 |
| exact_ascii | 262144 | count | 926.912500 | 919.303125 | -0.95 | -0.82 | -0.82 |
| exact_ascii | 262144 | missing | 858.509375 | 870.212500 | -0.53 | +1.36 | +1.08 |
| folded_ascii | 16 | forward | 0.023088 | 0.022374 | +11.17 | -3.09 | -3.00 |
| folded_ascii | 16 | backward | 0.063725 | 0.072671 | +14.86 | +14.04 | +13.29 |
| folded_ascii | 16 | count | 0.063535 | 0.076282 | +23.17 | +20.06 | +20.01 |
| folded_ascii | 16 | missing | 0.066703 | 0.071110 | +7.46 | +6.61 | +6.16 |
| folded_ascii | 1024 | forward | 0.170850 | 0.170557 | +17.57 | -0.17 | -0.23 |
| folded_ascii | 1024 | backward | 3.678345 | 3.638013 | +2.76 | -1.10 | -1.36 |
| folded_ascii | 1024 | count | 3.603125 | 3.640845 | +1.11 | +1.05 | +1.20 |
| folded_ascii | 1024 | missing | 3.337598 | 3.352417 | -0.49 | +0.44 | +0.41 |
| folded_ascii | 65536 | forward | 9.265625 | 9.271875 | -12.33 | +0.07 | +0.07 |
| folded_ascii | 65536 | backward | 230.917187 | 234.415625 | +0.00 | +1.52 | +0.70 |
| folded_ascii | 65536 | count | 232.445312 | 240.410937 | +2.01 | +3.43 | +3.49 |
| folded_ascii | 65536 | missing | 214.231250 | 212.801563 | -0.14 | -0.67 | -0.62 |
| folded_ascii | 262144 | forward | 38.171875 | 41.362500 | +2.22 | +8.36 | +8.13 |
| folded_ascii | 262144 | backward | 933.975000 | 961.459375 | +1.27 | +2.94 | +2.11 |
| folded_ascii | 262144 | count | 926.356250 | 958.071875 | +3.01 | +3.42 | +3.94 |
| folded_ascii | 262144 | missing | 876.250000 | 888.159375 | +0.45 | +1.36 | +0.51 |
| mixed_case | 16 | forward | 0.023006 | 0.021408 | -7.46 | -6.95 | -7.00 |
| mixed_case | 16 | backward | 0.064043 | 0.072116 | +15.83 | +12.60 | +12.60 |
| mixed_case | 16 | count | 0.063733 | 0.071603 | +13.39 | +12.35 | +12.30 |
| mixed_case | 16 | missing | 0.066245 | 0.070554 | +6.74 | +6.51 | +6.26 |
| mixed_case | 1024 | forward | 0.171191 | 0.169678 | +21.84 | -0.88 | -0.85 |
| mixed_case | 1024 | backward | 3.661304 | 3.602539 | +0.62 | -1.61 | -1.57 |
| mixed_case | 1024 | count | 3.588428 | 3.727588 | +3.79 | +3.88 | +4.01 |
| mixed_case | 1024 | missing | 3.389941 | 3.435425 | +24.83 | +1.34 | +0.60 |
| mixed_case | 65536 | forward | 9.268750 | 9.268750 | +14.13 | +0.00 | +0.22 |
| mixed_case | 65536 | backward | 227.059375 | 228.835937 | +2.46 | +0.78 | +0.78 |
| mixed_case | 65536 | count | 229.448438 | 237.628125 | +11.88 | +3.56 | +4.15 |
| mixed_case | 65536 | missing | 217.465625 | 215.146875 | +0.17 | -1.07 | -1.71 |
| mixed_case | 262144 | forward | 38.153125 | 38.137500 | -13.74 | -0.04 | -0.03 |
| mixed_case | 262144 | backward | 928.921875 | 936.318750 | -1.63 | +0.80 | +0.00 |
| mixed_case | 262144 | count | 915.021875 | 947.862500 | +2.82 | +3.59 | +3.00 |
| mixed_case | 262144 | missing | 863.021875 | 870.921875 | -0.30 | +0.92 | +0.99 |
| japanese | 16 | forward | 0.030891 | 0.027055 | -12.25 | -12.42 | -12.42 |
| japanese | 16 | backward | 0.071393 | 0.059869 | -14.61 | -16.14 | -16.51 |
| japanese | 16 | count | 0.069520 | 0.057992 | -16.81 | -16.58 | -16.59 |
| japanese | 16 | missing | 0.065489 | 0.070959 | -5.33 | +8.35 | +8.29 |
| japanese | 1024 | forward | 0.178882 | 0.174951 | -2.02 | -2.20 | -2.15 |
| japanese | 1024 | backward | 3.208984 | 2.637646 | -19.00 | -17.80 | -17.49 |
| japanese | 1024 | count | 3.155591 | 2.539331 | -18.07 | -19.53 | -19.10 |
| japanese | 1024 | missing | 3.441138 | 3.346021 | -0.07 | -2.76 | -2.33 |
| japanese | 65536 | forward | 9.296875 | 10.043750 | -6.65 | +8.03 | +7.54 |
| japanese | 65536 | backward | 201.043750 | 166.187500 | -17.89 | -17.34 | -17.32 |
| japanese | 65536 | count | 208.967187 | 165.678125 | -18.46 | -20.72 | -19.37 |
| japanese | 65536 | missing | 219.373438 | 213.751563 | -1.44 | -2.56 | -1.08 |
| japanese | 262144 | forward | 38.109375 | 38.537500 | -7.29 | +1.12 | +0.75 |
| japanese | 262144 | backward | 822.400000 | 685.731250 | -17.55 | -16.62 | -16.63 |
| japanese | 262144 | count | 841.078125 | 669.637500 | -18.25 | -20.38 | -20.51 |
| japanese | 262144 | missing | 891.003125 | 870.618750 | -0.73 | -2.29 | -2.66 |
| emoji | 16 | forward | 0.057902 | 0.048591 | -15.41 | -16.08 | -16.07 |
| emoji | 16 | backward | 0.095264 | 0.077299 | -17.45 | -18.86 | -18.56 |
| emoji | 16 | count | 0.094453 | 0.078537 | -18.09 | -16.85 | -17.14 |
| emoji | 16 | missing | 0.070659 | 0.078291 | +10.51 | +10.80 | +10.47 |
| emoji | 1024 | forward | 0.773047 | 0.765747 | -1.25 | -0.94 | -1.01 |
| emoji | 1024 | backward | 4.075635 | 3.293213 | -22.08 | -19.20 | -19.47 |
| emoji | 1024 | count | 4.142065 | 3.139185 | -22.52 | -24.21 | -24.10 |
| emoji | 1024 | missing | 3.948877 | 3.930542 | -0.56 | -0.46 | -0.28 |
| emoji | 65536 | forward | 45.800000 | 46.223438 | -14.25 | +0.92 | +3.70 |
| emoji | 65536 | backward | 254.315625 | 196.534375 | -21.82 | -22.72 | -23.30 |
| emoji | 65536 | count | 258.778125 | 197.490625 | -23.60 | -23.68 | -23.28 |
| emoji | 65536 | missing | 247.287500 | 248.860938 | -0.42 | +0.64 | +1.59 |
| emoji | 262144 | forward | 156.318750 | 156.437500 | -0.38 | +0.08 | +0.01 |
| emoji | 262144 | backward | 1035.034375 | 778.787500 | -23.71 | -24.76 | -24.44 |
| emoji | 262144 | count | 1013.753125 | 797.546875 | -24.74 | -21.33 | -21.31 |
| emoji | 262144 | missing | 995.587500 | 999.400000 | -3.22 | +0.38 | +0.00 |
| repeat_exact | 16 | forward | 0.060607 | 0.047171 | -22.75 | -22.17 | -22.01 |
| repeat_exact | 16 | backward | 0.106416 | 0.084357 | -20.42 | -20.73 | -20.73 |
| repeat_exact | 16 | count | 0.107175 | 0.080498 | -23.32 | -24.89 | -24.88 |
| repeat_exact | 16 | missing | 0.065483 | 0.072451 | +7.08 | +10.64 | +10.62 |
| repeat_exact | 1024 | forward | 1.774194 | 1.337915 | -24.24 | -24.59 | -24.36 |
| repeat_exact | 1024 | backward | 5.848267 | 4.576099 | -23.59 | -21.75 | -21.90 |
| repeat_exact | 1024 | count | 5.807300 | 4.500928 | -32.05 | -22.50 | -21.72 |
| repeat_exact | 1024 | missing | 3.390015 | 3.363770 | -1.80 | -0.77 | -0.57 |
| repeat_exact | 65536 | forward | 10.901563 | 10.506250 | -4.16 | -3.63 | -3.72 |
| repeat_exact | 65536 | backward | 355.942187 | 283.832812 | -19.51 | -20.26 | -20.46 |
| repeat_exact | 65536 | count | 344.904688 | 267.050000 | -20.51 | -22.57 | -22.51 |
| repeat_exact | 65536 | missing | 213.226562 | 217.156250 | -1.10 | +1.84 | +2.16 |
| repeat_exact | 262144 | forward | 40.071875 | 39.643750 | +6.40 | -1.07 | -0.95 |
| repeat_exact | 262144 | backward | 1429.878125 | 1130.918750 | -19.25 | -20.91 | -20.90 |
| repeat_exact | 262144 | count | 1365.900000 | 1114.115625 | -22.99 | -18.43 | -18.98 |
| repeat_exact | 262144 | missing | 873.887500 | 868.737500 | -0.93 | -0.59 | -0.29 |
| repeat_fold | 16 | forward | 0.063174 | 0.050926 | -17.38 | -19.39 | -19.32 |
| repeat_fold | 16 | backward | 0.105917 | 0.091370 | -12.39 | -13.73 | -13.75 |
| repeat_fold | 16 | count | 0.104910 | 0.088949 | -14.27 | -15.21 | -15.21 |
| repeat_fold | 16 | missing | 0.066135 | 0.072016 | +7.65 | +8.89 | +9.08 |
| repeat_fold | 1024 | forward | 1.763892 | 1.506836 | -15.52 | -14.57 | -15.09 |
| repeat_fold | 1024 | backward | 5.843799 | 5.245386 | -14.25 | -10.24 | -10.23 |
| repeat_fold | 1024 | count | 5.785645 | 5.019409 | -9.27 | -13.24 | -12.61 |
| repeat_fold | 1024 | missing | 3.371729 | 3.352979 | +1.38 | -0.56 | -0.44 |
| repeat_fold | 65536 | forward | 11.004687 | 10.628125 | -2.29 | -3.42 | -3.46 |
| repeat_fold | 65536 | backward | 355.209375 | 317.679688 | -6.86 | -10.57 | -10.90 |
| repeat_fold | 65536 | count | 348.231250 | 313.039062 | -9.09 | -10.11 | -10.16 |
| repeat_fold | 65536 | missing | 217.140625 | 216.723437 | -0.09 | -0.19 | -0.28 |
| repeat_fold | 262144 | forward | 39.837500 | 39.571875 | -0.69 | -0.67 | -0.65 |
| repeat_fold | 262144 | backward | 1448.725000 | 1290.078125 | -9.42 | -10.95 | -10.85 |
| repeat_fold | 262144 | count | 1400.212500 | 1223.015625 | -10.69 | -12.65 | -12.06 |
| repeat_fold | 262144 | missing | 894.165625 | 879.803125 | +0.49 | -1.61 | -0.58 |

## 対象検証と試行

configureRelease01/buildCore01/fixtures01/compileBaseline01/compileCandidate01/compare01/confirm01は成功。
audit01は84file/112条件/4480sample/全rawと中央値/6MAP行/同vector/archive/最終製品1行差分を照合して成功。
configureDebug01/buildDebug01（119actions）後、contract01のfolio_tests.exe --note-search成功、stderr空。
ASan/UBSan/nullabilityを含む正典Debugで既存4719短query oracle/2112exact extent/#761の44fixture/216呼出しと拒否契約を確認する。
既存テストでraw同値/ASCII大小違い/非ASCII/反復/重なり/巡回が覆えるため、同義のtest/selector/CTestは増やしていない。
symbols01はcore/application2library・違反0。最終文書と一緒にformat/conformance実graph/diff・相対参照と全112行を確認する。
記録した検証試行はchild/wrapper0、入力不変、記録エラーなし。
docs01の完了前に補助self-reviewを読ませた一回はresult.json未作成でFileNotFoundErrorとなった。docs01自体は成功し、対象試験を再実行せず完了後の結果で照合する。
この手順上の失敗を追記した最終文書はdocs02で確認する。成功source/test/libraryが不変ならpush/統合で対象試験を反復しない。
単体自己レビュー。最終head必須CIのeng/check.ps1成功が統合条件。

統合後は正典prepare-real-machineで通常版へ必要反映し、実data全bytes/hash/作成更新日時の前後を照合する。
通常app/実ノート/Storeは品質試験に使わない。保留634のJSON変異は再試行しない。
全task file保全、絶対path/同取込tree/clean/ignored/untracked/link/稼働参照監査後にD専用worktree/outputを整理、branch/commit保持。
確定CI/統合/通常反映/全保全整理はD:/NeNeFolio/agents/763-search-equality/integration-completion.json。
ADR0023/0032/0044、ARC-001/003/004/007/008/010/012、C-002/004/008/012/014/016、QLT-008/009/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。[日報](../reports/2026-11-06.md)・[引き継ぎ](../handoffs/2026-11-06.md)。
