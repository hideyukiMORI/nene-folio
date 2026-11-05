# 通常のUTF-8・UTF-16変換の費用 — 2026-11-05 / #643

## 結論

正典Release coreの変換単体を39条件/195sampleで測定し、期待する全bytes/unitsと一致した。
1MiB前後のUTF-8→UTF-16所有変換は中央値1.4319〜1.5775ms、逆変換は0.7312〜0.8174ms。
16byte前後の文言では所有変換約0.09µs、固定bufferへのfill約0.04µsだった。
fillは長さ確認と書込みの二走査を持ち、128/1024byteでは今回の所有変換より時間がかかった。
ただし所有変換とfillはメモリ所有・失敗時の書込み契約が異なり、この比較から相互へ置き換えない。

製品欠陥やUI全体のボトルネックを認定する結果ではないため、今回は計測と文書だけを残す。
製品/API/所有者/確保経路/schema/版/依存/ゲート変更なし。#616の本文検証、#620の改行正規化とは別の費用である。

## 正典経路と入力

utf16_text_create/utf8_text_createは所有変換、utf16_text_fillは呼出側bufferへの変換という既存経路を使う。
UI文言・名前・path・diagram label、本文復元などの呼出しがあるが、今回のobserverは変換単体だけを呼ぶ。
実UI・GDI・font・RichEdit・保存I/O・実app/実ノート/Storeは測定に含めない。

基点main6d10a21で正典Release core83actions成功。文書/testのみの#641統合後mainbfc9ab7へffし、
変換sourceとarchive/vector不変を確認した。eng/toolchain.ps1とCMakeの実compile vectorを使い、
/O2・/Ob2・/DNDEBUG・/MT・C23・/WXを維持。外部observerを正典archiveへ直接linkする。
MAPで実使用9関数が正典core由来と確認し、別objectで製品関数を置換していない。

15正常入力はASCII、日本語、英数字+日本語+絵文字の3固定patternと、
UTF-8目標16/128/1024/65536/1048576byteの組合せ。完全な文字単位で止め、末尾不足は0〜3byte。
Pythonの標準UTF-8/UTF-16LE codecで30入力fileと期待値を生成し、再encode/hashを独立に照合した。
埋め込みNULや不正列は含めない。create両方向は全15入力、fillは最初の3size/9入力だけ。

## 計時方法

1条件につき1process、全出力/終端/長さを検査するwarmup1回、5sample、最後に全出力をもう一度照合する。
反復数=max(1,min(32768,floor(1048576/UTF-8 bytes)))。
createは確保・変換・長さ/終端/最後の単位の定数時間確認・解放と反復loopを含む。
fillは事前確保した正確なunits+1容量への変換と同じ定数時間確認。内部strlenも含む。
入力read・独立期待bytesとの全体比較・入力不変照合・結果出力は計時外、phase選択は反復loopの外に置く。

QPCとthread cyclesを記録し、同じthreadをmask4へ固定、優先度不変。
Intel Core i9-10850K（10core/20thread）、Windows11 Pro build26300。開始時の既知build/observer processは0。
親のbuild/archive/hashを各計時中に重ねていない。family/sizeは表の順、phaseはto16/to8/fillの固定順。
OS全体のidle、cache、CPU周波数、他processの仕事は制御しない。cold startや最悪時間ではない。
別のphaseの中央値を加算して実UIの応答時間や因果寄与率へ読み替えない。

## 実測

各値は5sampleの中央値、単位µs。raw QPC/cycle、反復数、全標本はmeasure.jsonに保持する。

| 分布 | UTF-8 bytes | UTF-16 units | UTF-8→16 create | UTF-16→8 create | UTF-8→16 fill |
| --- | ---: | ---: | ---: | ---: | ---: |
| ascii | 16 | 16 | 0.09603 | 0.08027 | 0.04450 |
| ascii | 128 | 128 | 0.23286 | 0.15293 | 0.33949 |
| ascii | 1024 | 1024 | 1.34238 | 0.73691 | 2.59014 |
| ascii | 65536 | 65536 | 119.75625 | 40.59375 | 対象外 |
| ascii | 1048576 | 1048576 | 1577.50000 | 817.40000 | 対象外 |
| japanese | 15 | 5 | 0.09362 | 0.08216 | 0.04246 |
| japanese | 126 | 42 | 0.26113 | 0.14153 | 0.33772 |
| japanese | 1023 | 341 | 1.42098 | 0.64332 | 2.59278 |
| japanese | 65535 | 21845 | 85.57500 | 54.51250 | 対象外 |
| japanese | 1048575 | 349525 | 1431.90000 | 731.20000 | 対象外 |
| mixed | 15 | 9 | 0.09185 | 0.07882 | 0.04157 |
| mixed | 125 | 75 | 0.24900 | 0.14847 | 0.32643 |
| mixed | 1022 | 614 | 1.44347 | 0.68363 | 2.49240 |
| mixed | 65535 | 39321 | 81.08750 | 39.19375 | 対象外 |
| mixed | 1048575 | 629145 | 1470.20000 | 771.30000 | 対象外 |

固定入力内でもUTF-8 bytesとUTF-16 unitsは分布ごとに異なる。異なる文字数の列を同一workと扱わない。
短い入力には呼出し/確認/確保の固定費用も含まれる。この測定だけでは新cacheやASCII特別経路を追加しない。

## 試行・検証・原本

全対象実行はtools/record-run.py。原本D:/NeNeFolio/agents/643-text-conversion/、
作業木は同worktrees/643-text-conversion/、一時出力は同outputs/643-text-conversion/。
全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。

| run | 結果 |
| --- | --- |
| configure-release-01 / build-core-01 | 正典Release core83actions成功、File API query付き |
| fixtures-01 / reference-01 / compile-observer-01 | 15正常入力・独立期待値生成、library/source/vector固定、直接link成功 |
| measure-01 | 39条件/195sample成功、stderr空、全出力一致、入力終了時不変 |
| audit-01 | 30入力hash/codec、全raw統計、9正典symbol、source/archive/vector、全run入力不変を再照合 |
| conformance-01 | 6docsの相対参照・git diff --check・source/build graphを確認する |

最初の補助input-list準備は旧Debug driver名を持ち込み、存在確認で停止した。child/run-dirはまだ作っていない。
repo内638source/configと各run固有driverを分けてからconfigureした。不存在utf16_tests.cの読取もjournalへ分離した。
全record-runは子/記録0、記録error0。新たな製品テストを追加し、成功済み機能試験を反復する作業ではない。

規則: ARC-001/003/004/007/008/012、C-008/014/016、QLT-008/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。最終head必須CIのeng/check.ps1、正典統合、通常exe/data32file不変、
全保全/hash照合と専用領域整理は同taskのintegration-completion.jsonで確定する。branch/commit保持。
#634は保留を維持する。

## 13:40 統合・保全整理完了

PR644 head8c46fff8b74d04babbbc15c2fb28ebc090079cacの必須CI41968246196 full-gate/check成功、
mainfec31fa9eb5a46187c2d52678ff5609e7ec2f78aへ正典統合、Issue643 CLOSED。
通常exe/data32fileは#635後のbytes/hash/作成更新日時と一致。製品不変で通常build/app起動なし。
1329file全保全ZIPを再hash照合、取込tree/clean/ignored/link/稼働参照を監査後、専用worktree/outputを削除。
branch/commit保持。確定原本は同taskのintegration-completion.json。
