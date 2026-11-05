# 借用Markdown行の参照費用を減らす — #624

#622の段落分布計測を根拠に、同じmarkdown_view_atの中で行の探索費用を減らした。
固定Releaseの1MiB入力では、79文字+LFのRTF構築が56.39725→27.20645ms、1文字+LFが722.01880→253.44850ms。
不均一な追加分布では約10%増える場合も残る。全入力の改善や実UIの速度を保証する変更ではない。

## 正典・契約

製品の基点はmain6107a26。文書だけの#622を取り込んで作業branchをmainbd8359eへffした。
`markdown_blocks` → `markdown_inline_tokens` → `markdown_view_at`と、render入口の`markdown_view_valid`を維持する。
borrowed view/spanの公開構造、所有者、検証境界、RTF/保存bytes、schema、版、依存、thread、確保・失敗順は変えない。
新cache/状態/計装/APIは加えない。[ADR0078](../adr/0078-lightweight-markdown-inline-rendering.md)の借用入力の内側の実装詳細で、新ADRは不要と判断した。

規則: ARC-001/003/004/007/008/012、C-004/008/012/014/016、QLT-008/009/010/012/013、GIT-001〜004。
追加waiver none。既存WVR-0001はWinHTTP callbackだけで、今回へ適用しない。gate・閾値・抑制・許可集合は不変。

`src/core/markdown_view.c`で、単一spanは従来のspan内参照へ進む。複数spanは先頭/末尾の実範囲を先に確認する。
32span以上で先頭・次・末尾直前の長さが揃う場合だけ、先頭strideから候補を求める。
これは費用を絞るheuristicであり、均一性や正しさの証明ではない。stride非ゼロ→推定index範囲→実offset/lengthの照合を必須にする。
private bool queryは採用時だけoutを更新し、不採用ならlow=0のまま既存と同じ順序付きspan探索へ戻す。
探索はremainingを毎回ceil半分へ減らす。low+remaining<=countを保ち、中点加算とspan参照は範囲内、remaining>1なら必ず進む。
奇数で不適合の中点を1件余分に残す場合があるが、正しい候補は失わない。仮想LFと入力外NULの返却は維持する。
無効viewは既存render入口で拒否する。重複offset等を直接atへ渡した際の偶発的な旧戻り値までの同値性は保証しない。

## 比較と採用

正典Release coreの#622 libraryをSHA一致で複製した。実compile_commandsからO2/Ob2/DNDEBUG/MT/C23・警告を維持し、
基点/各候補のview objectだけを別名で生成して同じarchiveより先にリンクする。mapでat/validが明示object由来と確認した。
archive自体と残りの製品sourceは不変。新しい製品窓口や別の製品build経路は作らず、外部observerで比較した。

| 候補 | 内容・判断 |
| --- | --- |
| 1 | 全体平均幅から毎回推定し、外れたら片側へ探索。不均一な直接lookupで約2倍など悪化。不採用 |
| 2 | 推定を外しremainingの二分探索だけを簡潔にする。1MiBの短行RTF739.34260→697.76410msだが小さいlookupに悪化が残り、比較候補に留めた |
| 3 | 先頭/末尾と標本行幅の推定を追加。大きい均等行は改善。日本語一行で約20%増、認知的複雑度超過の静読指摘があり、そのままは不採用 |
| 4 | 推定を意味のまとまったprivate queryへ分け、単一spanは公開入口の同じ参照へ保つ。64caseで効果と不利な分布を確認して採用 |

候補3のC-012指摘は静的見積もりであり、tidy失敗を実行したとは記録しない。候補4の実tidyは成功した。
候補4を整形後、製品sourceの空白除外token一致を確認し、計測を再実行しない。

#622の24fixtureに、先頭/末尾長・交互長短・空行混在・行をまたぐ記法を各1KiB/1MiBで加えた34入力。
初版fixtures-01は生成成功したが、小さい交互/不均一patternが全Xになる不足を静読で発見した。
未使用の初版を保持し、サイズ別patternを使うfixtures-02を新規生成して比較に使用した。
さらに標本行幅だけ揃い、中間が長い/交互となる4入力を追加し、候補3/4の推定が外れる費用も確認した。

基点A/候補BをcaseごとにA/B/B/A、各block5sample・各版10sampleで測る。RTFは1回warmup後、構築だけを計時。
note_text生成、破棄、全bytes照合、入力read/hash、RTF保存は計時外。全blockのRTFを基点と全bytes比較する。
直接lookupは各byteを原文と照合するcaller込みで、RTFへの寄与率ではない。1sampleの反復は1KiB256/16KiB16/256KiB2/1MiB1。
全caseに30秒process timeout、入力上限1MiB、出力上限64MiBを設ける。全processは正常終了した。

Windows 11 Pro 10.0.26300、Core i9-10850K、20 logical processors。observer threadだけaffinity mask4、優先度不変。
比較前の既知build/probeは0。計測中に親のbuild/hash/archiveを重ねない。QPCとthread cyclesを別々に保持する。
OS全体idle・排他的CPU・周波数/cacheは保証しない。cycleを経過時間へ換算しない。計測APIの根拠と限界は[#622](2026-11-05-markdown-paragraph-scale.md)へ。

### 採用候補4のRTF結果

各版10sampleの中央値。構築だけの時間で、実UI/RichEdit投入/描画/I/O/peak heapは含まない。

| 1MiB入力 | 基点→候補の中央値ms | 比 |
| --- | ---: | ---: |
| ASCII・一行 | 10.67520→10.20675 | 0.956 |
| ASCII・79文字+LF | 56.39725→27.20645 | 0.482 |
| ASCII・1文字+LF | 722.01880→253.44850 | 0.351 |
| ASCII・空行段落 | 19.01530→19.31160 | 1.016 |
| 日本語・一行 | 11.20875→10.57655 | 0.944 |
| 日本語・81byte+LF | 55.52685→26.66985 | 0.480 |
| 長い先頭行 | 378.42065→362.65850 | 0.958 |
| 長い末尾行 | 383.55055→134.21020 | 0.350 |
| 交互の長短行 | 33.42735→30.03040 | 0.898 |
| 不均一・空行混在 | 23.41855→18.08885 | 0.772 |
| 行をまたぐ行内記法 | 210.64420→208.75555 | 0.991 |
| 中間だけ長い行 | 377.29095→286.67770 | 0.760 |
| 標本行幅だけ一致・中間交互 | 83.28635→91.52040 | 1.099 |

標本行幅だけ一致する中間交互入力はRTF約10%増、1KiBの直接lookupは約55%増となる。
一方、1MiBの直接lookupは79文字+LF12.02205→4.93870ms、1文字+LF45.50415→4.82025ms、長い末尾行30.59265→3.50020ms。
一般的な計算量の改善や一定の上限ではなく、特定分布の繰返し探索を減らした結果とする。
小さい入力/不採用案/全標本/range/cycleは各paired JSONへ全て保存する。

## 境界・検証・独立静読

`tests/unit/markdown_tests.c`の既存run_markdown_testsへ借用viewの検証を追加した。
span数0/1/2/3/4/5/7/8/9/16/31/32/33/64、全空・均等・不均一・先頭/末尾/中間長・中間空を使う。
各spanを正確な長さで確保し、独立した連結bytesに仮想LFを挿入する。全indexを正順と逆順で照合し、入力不変、終端、SIZE_MAXを確認する。
非ASCIIもbyteとして保持する。これは任意UTF8構文の検証ではない。最大長の2spanと32spanは巨大領域を実際には読まず、
仮想LFと小さな実spanだけを参照する。後者は推定範囲外→探索復帰の上限近傍を通る。

親が調査/実装/計測/検証、text_validation_reviewが同じ既定モデルで候補1〜4を独立静読した。
子による編集/build/test/追加copyは無い。採用候補の契約blocker0。C-012の分割提案と最大長の追加提案を反映した。
最終判断・試験は親が行い、詳細をindependent-review.mdへ保存する。

configure-debug-01、build-debug-01の119actionsが成功。レビュー提案の最大長fixture追加後、build-debug-02で対象2actionsのみ再buildした。
Debug/tidy/format/警告error、ASan/UBSan/nullabilityとrecover禁止を維持した。
markdown-01は既存folio_tests.exe --markdown-rtfで子/記録0・stderr空。既存構文/表/行内/原文/RTF契約も同じselectorで確認する。
この正典Debugはallocation injection inactiveであり、OOM sweep成功を主張しない。製品に新確保はなく、既存の計装検証は最終必須CIで維持する。

候補compile5件、通常比較4件、候補3追加比較1件は成功。候補4追加比較はmeasure-04内で順に実行した。
audit-01も子/記録0。全6pairedファイル、240組/4,800timed samples、38fixture、576RTFを再hashし、raw中央値/range/cycleを再計算。
全版のRTF同一と#622の24出力一致、明示view objectのmapを確認した。全成功runは記録error0、input終了時不変。
構文完全性、任意16MiB、実UIの受理を証明するものではない。

## 原本・統合

恒久原本は`D:/NeNeFolio/agents/624-markdown-view-lookup/`。各runの実argv/source/object/exe/input/output/raw/終了値をrecord-run.pyで保全する。
全体journalは`D:/NeNeFolio/agents/quality-20261105/journal.md`。
文書確認→最終head必須CIのeng/check.ps1→正典merge→通常反映/data不変→全保全/専用worktree/output整理の確定は
同taskのintegration-completion.jsonへ記録する。branch/commitは保持する。実ノート・通常app・Storeは試験に使わない。
