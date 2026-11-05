# 編集可能判定のASCII本文を短縮する — 2026-11-05 / #647

## 結論

既存の全文ASCII確認を共有し、編集可能判定だけを短縮した。固定Releaseの正常24入力で、
ASCII16MiBは中央値21.7084→2.08205ms、1MiBは1.2972→0.0864msだった。
日本語・混在文は概ね同程度。末尾だけ日本語の16MiBは21.69825→23.0468ms（約6.2%増）、
16byteの同分布は約7.3%増（約0.0015µs）を残る費用として採用する。
編集開始・ノート切替・履歴復元から呼ばれるが、これは判定単体の費用で、実UI全体の速度改善率ではない。

## 正典経路と契約

src/core/note_text.cの既存plain_asciiは変更せず、note_text_editabilityからも使う。
全文が非NUL ASCIIと確認できた場合だけ、正本editor_alteredのASCII要素を長さ付きmemchrで探す。
U+0007を別の製品定数として複製しない。非ASCIIがあれば従来どおり先頭から共通utf8_text_decodeと
editor_altersを使い、U+2028/U+2029を含む同じ3文字集合で判定する。

plain_asciiは残長を確認してmemcpyする既存経路。memchrは本文長内、intへの変換は0〜127だけ。
空本文・BOMを除いた本文・反復呼出しでも同じ結果。API/所有者/確保回数/本文bytes/保存schema/版/依存は不変。
本文生成時の検証・editor改行正規化は変更しない。新cache・新decoder・製品計装・ゲート緩和なし。

## 比較方法

基点mainfec31faで正典Release core83actionsを構築し、製品不変の#645統合後mainc1c6444へffした。
core/eng/CMake入力不変を照合し、library・元note_text・compile vectorを凍結。
同じ/O2・/Ob2・/DNDEBUG・/MT・C23・/WXで元/候補note_textを個別にcompileし、
明示objectを同じ正典core archiveの前へlinkした。MAPでobserverが使う7公開関数すべてがそのobject由来と確認。
製品の公開入口やCMakeへ観測のための入口を足さない。

入力はASCII/日本語/英数字+日本語+絵文字/末尾だけ日本語の4分布×16/128/1024/64KiB/1MiB/16MiB。
完全な文字単位でpatternを繰り返し、最後の余りはxで正確なbyte長にする。全24入力はUTF-8として正常で編集可能。
初版fixtures01では16byte日本語/混在のpatternが全体で収まらず全xだった。最初の分離profileはその実入力の記録として保持し、
比較前に文字単位の末尾充填へ訂正したfixtures02を別名生成した。すべての最終paired比較は02を使う。

各caseを元→候補→候補→元（ABBA）の4process、各process warmup1・5sampleで比較する。
1sampleの反復はmax(1,min(65536,floor(1MiB/bytes)))、各版10sample、全480sample。
note_text生成/破棄と全文bytes照合は計時外。公開editability呼出し、結果enumの定数時間比較、反復loopだけを計時する。
計時前後で結果EDI| ascii | 16 | 0.02120 | 0.01156 | 0.5453 |
| ascii | 128 | 0.17122 | 0.01743 | 0.1018 |
| ascii | 1024 | 1.28892 | 0.08228 | 0.0638 |
| ascii | 65536 | 81.91563 | 4.93125 | 0.0602 |
| ascii | 1048576 | 1297.20000 | 86.40000 | 0.0666 |
| ascii | 16777216 | 21708.40000 | 2082.05000 | 0.0959 |
| japanese | 16 | 0.02114 | 0.02176 | 1.0294 |
| japanese | 128 | 0.18684 | 0.19117 | 1.0232 |
| japanese | 1024 | 1.52534 | 1.52979 | 1.0029 |
| japanese | 65536 | 97.80938 | 98.25000 | 1.0045 |
| japanese | 1048576 | 1581.35000 | 1578.20000 | 0.9980 |
| japanese | 16777216 | 26014.10000 | 26216.00000 | 1.0078 |
| mixed | 16 | 0.02072 | 0.02196 | 1.0598 |
| mixed | 128 | 0.16455 | 0.16831 | 1.0228 |
| mixed | 1024 | 1.21514 | 1.21807 | 1.0024 |
| mixed | 65536 | 77.50938 | 77.82500 | 1.0041 |
| mixed | 1048576 | 1242.65000 | 1250.10000 | 1.0060 |
| mixed | 16777216 | 21098.20000 | 20817.75000 | 0.9867 |
| late-japanese | 16 | 0.02058 | 0.02209 | 1.0734 |
| late-japanese | 128 | 0.16768 | 0.17242 | 1.0283 |
| late-japanese | 1024 | 1.27202 | 1.34312 | 1.0559 |
| late-japanese | 65536 | 80.76250 | 84.07500 | 1.0410 |
| late-japanese | 1048576 | 1296.60000 | 1352.65000 | 1.0432 |
| late-japanese | 16777216 | 21698.25000 | 23046.80000 | 1.0622 |、本文長/終端/全bytesと入力file不変を確認する。

QPCとthread cyclesを記録。Intel Core i9-10850K、Windows11 Pro build26300、thread mask4、優先度不変。
開始時の既知build/test/observer processは0。親build/archive/hashを計時中に重ねない。
OS全体のidle・cache・CPU周波数は制御しない。同じ本文への反復で、cold start/最悪時間/実UI寄与率ではない。

## 実測

各版10sampleの中央値、単位µs。比は候補/元。raw QPC/cyclesと全再計算はmeasurement-audit.jsonへ。

| 分布 | bytes | 元 | 候補 | 比 |
| --- | ---: | ---: | ---: | ---: |
| ascii | 16 | 0.02120 | 0.01156 | 0.5453 |
| ascii | 128 | 0.17122 | 0.01743 | 0.1018 |
| ascii | 1024 | 1.28892 | 0.08228 | 0.0638 |
| ascii | 65536 | 81.91563 | 4.93125 | 0.0602 |
| ascii | 1048576 | 1297.20000 | 86.40000 | 0.0666 |
| ascii | 16777216 | 21708.40000 | 2082.05000 | 0.0959 |
| japanese | 16 | 0.02114 | 0.02176 | 1.0294 |
| japanese | 128 | 0.18684 | 0.19117 | 1.0232 |
| japanese | 1024 | 1.52534 | 1.52979 | 1.0029 |
| japanese | 65536 | 97.80938 | 98.25000 | 1.0045 |
| japanese | 1048576 | 1581.35000 | 1578.20000 | 0.9980 |
| japanese | 16777216 | 26014.10000 | 26216.00000 | 1.0078 |
| mixed | 16 | 0.02072 | 0.02196 | 1.0598 |
| mixed | 128 | 0.16455 | 0.16831 | 1.0228 |
| mixed | 1024 | 1.21514 | 1.21807 | 1.0024 |
| mixed | 65536 | 77.50938 | 77.82500 | 1.0041 |
| mixed | 1048576 | 1242.65000 | 1250.10000 | 1.0060 |
| mixed | 16777216 | 21098.20000 | 20817.75000 | 0.9867 |
| late-japanese | 16 | 0.02058 | 0.02209 | 1.0734 |
| late-japanese | 128 | 0.16768 | 0.17242 | 1.0283 |
| late-japanese | 1024 | 1.27202 | 1.34312 | 1.0559 |
| late-japanese | 65536 | 80.76250 | 84.07500 | 1.0410 |
| late-japanese | 1048576 | 1296.60000 | 1352.65000 | 1.0432 |
| late-japanese | 16777216 | 21698.25000 | 23046.80000 | 1.0622 |

## 対象回帰・レビュー・全試行

既存のcore editability回帰とapplicationの編集開始/履歴復元/切替/通知回帰を、
--editor-eligibilityと全suiteから同じ関数で呼ぶ。全suiteから削らない。
追加は11長さのASCIIでbell/tab/普通の文字の先頭・中央・末尾、Unicode separatorと日本語の7位置、
日本語の後のbell。反復判定後の所有本文pointer/長さ/bytes不変も確認する。
本文は正常UTF-8であり、UIで変質する3文字を含む場合は従来の編集拒否を期待する。
確保を追加しないため新たなOOM sweepは行わない。

| run | 結果 |
| --- | --- |
| configure-release-01 / build-core-01 | 正典Release core83actions成功 |
| fixtures-01 / reference-01 / compile-baseline-01 / measure-baseline-01 | 探索profile24条件成功。最小日本語/混在fixtureの上記限界を保持 |
| fixtures-02 / compile-candidate-01 / measure-candidate-01 | 正常24入力/480sample、全結果・本文一致、stderr空 |
| configure-debug-01 / build-debug-01 | 正典Debug、5変更C/headerのformat、tidy/119actions成功 |
| contract-01 | folio_tests.exe --editor-eligibility成功、stderr空、子/記録0 |
| audit-01 | MAPに未使用の全公開関数も残るという補助監査の前提が誤り、note_text_copy不存在で停止。子1/記録125、未生成成果物error1 |
| audit-02 | 実observerが使用する7関数を照合。全raw再計算/24入力hash・codec/source/archive/入力不変成功 |
| conformance-01 | 6docsの参照・git diff --check・source/build graphを確認する |

初回監査の失敗は保持する。linkerが未使用関数を除去しただけで製品/比較結果の失敗ではない。
独立text_validation_reviewは読み取り専用、現席と同じ既定model、編集/build/test/追加copyなし。
対象source SHA6867020d…da7d26に必須修正0、範囲・集合正本・非ASCII復帰・C-012を静読。
親が実sourceと照合して受理し、実Debug/tidy/対象結果は親の記録で確認した。
整形前後は空白以外一致を照合して成功比較を再利用する。工程だけでは再実行しない。

規則: ARC-001/003/004/007/008/012、C-008/012/014/016、QLT-008/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。全対象実行はtools/record-run.pyで初回失敗も含め保持。
補助読取の不存在ADR名/過大出力は全体journalへ分離する。

原本D:/NeNeFolio/agents/647-editor-eligibility/、作業木は同worktrees/647-editor-eligibility/、
一時出力は同outputs/647-editor-eligibility/。全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。
最終head必須CIのeng/check.ps1、正典統合、通常版反映/data32file不変、全保全/hash照合後の専用領域整理は
同taskのintegration-completion.jsonで確定する。branch/commit保持。実app/実ノート/Storeを試験に使わず、#634は保留する。
