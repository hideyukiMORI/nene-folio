# 行索引構築の正常本文比較 — 2026-11-05 / #651

## 結論

既存line_indexのCRを探す部分だけで、CRを含まない4単位をまとめて進むようにした。
同じRelease/ABBAの24条件480sampleで、16Mi UTF-16単位のASCII一行は7.67675→3.13045ms、
64単位ごとの通常行は9.9366→4.25205ms、日本語/絵文字の同じ行長は9.72315→4.34975msだった。
8単位ごとの短い行は16Mi単位で14.0219→14.0907ms（約0.49%増）。最大比の悪化は同分布64Ki単位で
31.490625→35.2625µs（約12%増、+3.771875µs）。この費用を明示して候補1を採用した。
これは索引構築/行数取得/破棄の費用で、RichEdit読取・描画・入力全体の改善率ではない。

## 正典経路と契約

[ADR0026](../adr/0026-line-numbers.md)のline_index ownerと、[ADR0023](../adr/0023-in-note-search.md)の
CR単位の行境界に従う。note_paneは既存のlines_staleによって本文変更後に索引を再作成する。
既存のcache/位置二分探索/行開始位置/桁数/公開API/確保・解放を変えない。

private unbroken_unitsは残長4単位以上でだけ8byteをmemcpyし、正本paragraph_breakの繰返し値とXORする。
16bit単位のzero判定で一致を含まない塊だけ進み、一致を含む塊と末尾は既存と同じ単位照合で先頭CRを確定する。
借りによる隣接laneへの影響を位置として使わない。内外の位置はcount以下、CR登録後は必ず1単位進む。
CRごとのreserve/確保順/昇順登録/失敗時の所有解放/成功時だけのout更新は不変。
UTF-16をdecodeする処理や新owner/cache/計装/外部依存/製品の試験用入口を増やさない。

## 比較方法

基点main8e74a91で正典Release core83actionsを構築し、library・元line_index.c・compile vectorを凍結した。
#649のmain13ce4eeへffした差分はnative test/docsとCTest登録2行だけ。src/eng/cmake不変とCMakeの追加2行を監査し、
coreの成功buildを再利用した。同じ/O2・/Ob2・/DNDEBUG・/MT・C23・/WXで元/候補を個別compileし、
明示line_index objectを同じcore archiveより前にlinkする。MAPでobserverが呼ぶ6公開関数がそのobject由来と確認した。

入力はflat-ascii、ordinary-64、日本語/絵文字のjapanese-64、猫/絵文字を含むshort-8の4分布。
各16/128/1024/65536/1048576/16777216 UTF-16単位、全24入力。64/8はCRまでの行長で、短い本文はそのprefix。
全入力は正常UTF-16LEで、完全な文字単位で作り、正確なunit数・独立decode後のCR行数・hashを確認する。
file bytesはunit数の2倍。ASCII一行以外の長いcaseは末尾CRを含み、最後の空行も1行と数える。

元→候補→候補→元（ABBA）で各case4process、各process warmup1/5sample、各版10sample、全480sample。
1sampleの反復はmax(1,min(65536,floor(1Mi単位/count)))。計時内はcreate/count/destroyと結果enum/行数の定数時間照合だけ。
元入力の読取、全行開始位置・行数・先頭/中央/末尾位置・桁数・範囲外out保持の照合は計時外で前後に行う。
全入力fileの終了時hash/全unitsも一致した。基準profile24条件120sampleは探索記録として別に保持する。

QPCとthread cyclesを記録。Intel Core i9-10850K/Windows11 Pro build26300、thread mask4、優先度不変。
親のbuild/archiveを計時中に重ねない。OS全体のidle/cache/CPU周波数は制御せず、cold start/最悪時間/実UI寄与率は示さない。
整形前後は標準includeの同じ集合と、include以外の空白を除いた全bodyが一致することを監査した。
includeの並べ替えと整形だけなので、実装不変の比較を工程のために反復しない。

## 実測

単位µs、各版10sample中央値。比は候補/元。raw QPC/cyclesと全再計算はmeasurement-audit.json。

| 分布 | UTF-16単位 | 行数 | 元 | 候補1 | 比 |
| --- | ---: | ---: | ---: | ---: | ---: |
| flat-ascii | 16 | 1 | 0.04871 | 0.04422 | 0.9078 |
| flat-ascii | 128 | 1 | 0.09849 | 0.05984 | 0.6076 |
| flat-ascii | 1024 | 1 | 0.46528 | 0.19282 | 0.4144 |
| flat-ascii | 65536 | 1 | 26.96563 | 9.43750 | 0.3500 |
| flat-ascii | 1048576 | 1 | 431.55000 | 154.25000 | 0.3574 |
| flat-ascii | 16777216 | 1 | 7676.75000 | 3130.45000 | 0.4078 |
| ordinary-64 | 16 | 1 | 0.04734 | 0.04574 | 0.9663 |
| ordinary-64 | 128 | 3 | 0.12970 | 0.09754 | 0.7520 |
| ordinary-64 | 1024 | 17 | 0.61074 | 0.34033 | 0.5572 |
| ordinary-64 | 65536 | 1025 | 32.89375 | 12.19063 | 0.3706 |
| ordinary-64 | 1048576 | 16385 | 524.70000 | 185.60000 | 0.3537 |
| ordinary-64 | 16777216 | 262145 | 9936.60000 | 4252.05000 | 0.4279 |
| japanese-64 | 16 | 1 | 0.04695 | 0.04490 | 0.9565 |
| japanese-64 | 128 | 3 | 0.13746 | 0.09922 | 0.7219 |
| japanese-64 | 1024 | 17 | 0.60815 | 0.25791 | 0.4241 |
| japanese-64 | 65536 | 1025 | 32.92188 | 12.21250 | 0.3710 |
| japanese-64 | 1048576 | 16385 | 552.30000 | 187.05000 | 0.3387 |
| japanese-64 | 16777216 | 262145 | 9723.15000 | 4349.75000 | 0.4474 |
| short-8 | 16 | 3 | 0.08460 | 0.08268 | 0.9772 |
| short-8 | 128 | 17 | 0.14067 | 0.13459 | 0.9567 |
| short-8 | 1024 | 129 | 0.82061 | 0.85298 | 1.0395 |
| short-8 | 65536 | 8193 | 31.49062 | 35.26250 | 1.1198 |
| short-8 | 1048576 | 131073 | 655.10000 | 656.60000 | 1.0023 |
| short-8 | 16777216 | 2097153 | 14021.90000 | 14090.70000 | 1.0049 |

## 対象回帰と試行

既存line_index_testsを全suiteと--line-indexで共有し、元の空行・末尾CR・LF・範囲外・桁数の回帰を維持する。
18長さ×各CR位置またはCRなし×4alignmentの1572配置とsurrogate pair例を追加。
BMPの似たbyte（U+010D/U+0D00/U+010C）と、明示長の直後にCRを置く非終端入力でも、
全位置/全行先頭/行数を独立した単純な1単位走査と照合する。guard CRは物理的な保護pageではない。
所有と確保経路は不変なので追加のOOM sweepは行わず、既存line_index_scenarioを含む最終CIの測定を維持する。

| run | 結果 |
| --- | --- |
| configure-release-01 / build-core-01 | 正典Release core83actions成功 |
| fixtures-01 / reference-01 / compile-baseline-01 | 正常24入力生成、凍結source/archive/vector、observer構築成功 |
| measure-baseline-01 | 基準24条件120sample成功 |
| compile-candidate-01 / measure-candidate-01 | 同vector、24条件480sample、全行位置等一致、stderr空 |
| configure-debug-01 / build-debug-01 | 最終3Cのformat/tidy、正典Debug119actions成功 |
| contract-01 | folio_tests.exe --line-index成功、stderr空 |
| symbols-01 | eng/symbols.py --require core application、2library/0違反 |
| audit-01 | include抽出の空白patternが空行も含め、順変更前後の同集合を誤って拒否。子1/記録125、未生成artifact error1 |
| audit-02 | patternを横空白だけへ訂正。全raw/入力codec-hash/MAP6/source/archive/vectorと初回失敗実値を照合成功 |
| conformance-01 | 最終6docsの参照・git diff --check・source/build graphを確認する |

独立text_validation_reviewは現席の既定modelを継承し、3C差分だけを読取。編集/build/test/追加成果物なし、必須修正0。
製品SHA0293a0b7…bec0a9aとline_index_tests SHA3c267f86…97555ceを親が照合した。
selectorだけはレビュー後に同じblockを既存text系helperへ移し、全体関数の行数を保つ配置を親が自己レビューした。
追加waiver none、既存WVR-0001継続。規則ARC-001/003/004/007/008/012、C-008/012/014/016、QLT-008/010/012/013、GIT-001〜004。

原本D:/NeNeFolio/agents/651-line-index/。全実行はtools/record-run.pyで初回失敗も保持する。
補助readのWindows glob誤りは全体journalへ分離。通常app/実ノート/Storeは試験に使わず、#634は保留。
最終head必須CI eng/check.ps1、正典統合、必要な通常反映/data不変、全保全/hash確認と専用worktree/output整理は
同taskのintegration-completion.jsonで確定する。branch/commit保持。API/schema/版/ゲートは不変。
