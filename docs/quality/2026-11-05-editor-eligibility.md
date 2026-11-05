# 編集可能判定のASCII本文を短縮する — 2026-11-05 / #647

## 結論

既存の全文ASCII確認を共有し、編集可能判定だけを短縮した。採用候補3の固定Release正常24入力で、
ASCII16MiBは中央値21.71145→2.49205ms、1MiBは1.29495→0.1268msだった。
日本語16MiBは26.04835→26.18070ms（約0.5%増）、混在は20.8986→20.7411ms、
末尾だけ日本語は21.6685→21.5292ms。最小16byte日本語は約7.36%増（約1.54ns）、混在は約3%増を残す。
編集開始・ノート切替・履歴復元から呼ばれるが、これは判定単体の費用で、実UI全体の速度改善率ではない。

## 正典経路と契約

src/core/note_text.cの既存plain_asciiは変更せず、note_text_editabilityからも使う。
全文が非NUL ASCIIと確認できた場合だけ、正本editor_alteredのASCII要素をprivate contains_asciiで探す。
U+0007を別の製品定数として複製しない。非ASCIIがあれば従来どおり先頭から共通utf8_text_decodeと
editor_altersを使い、U+2028/U+2029を含む同じ3文字集合で判定する。

plain_asciiは残長を確認してmemcpyする既存経路。contains_asciiも残長8byte以上の場合だけmemcpyし、
対象byteの繰返し値とのXORとzero-byte判定で存在を調べ、末尾は1byteずつ照合する。対象は0〜127だけ。
非ASCIIのcode_point初期化はloop前へ移す。生成時に検証済みで各decodeが必ず上書きするため、結果は同じ。
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

各版10sampleの中央値、単位µs。比は候補/元。採用候補3のraw QPC/cyclesと全再計算はmeasurement-audit03.jsonへ。

| 分布 | bytes | 元 | 候補3 | 比 |
| --- | ---: | ---: | ---: | ---: |
| ascii | 16 | 0.02266 | 0.00471 | 0.2081 |
| ascii | 128 | 0.17036 | 0.01678 | 0.0985 |
| ascii | 1024 | 1.26553 | 0.13140 | 0.1038 |
| ascii | 65536 | 80.73438 | 7.61250 | 0.0943 |
| ascii | 1048576 | 1294.95000 | 126.80000 | 0.0979 |
| ascii | 16777216 | 21711.45000 | 2492.05000 | 0.1148 |
| japanese | 16 | 0.02092 | 0.02246 | 1.0736 |
| japanese | 128 | 0.18771 | 0.18831 | 1.0032 |
| japanese | 1024 | 1.52666 | 1.52383 | 0.9981 |
| japanese | 65536 | 98.15000 | 97.67188 | 0.9951 |
| japanese | 1048576 | 1574.50000 | 1571.45000 | 0.9981 |
| japanese | 16777216 | 26048.35000 | 26180.70000 | 1.0051 |
| mixed | 16 | 0.02050 | 0.02112 | 1.0303 |
| mixed | 128 | 0.15749 | 0.15684 | 0.9959 |
| mixed | 1024 | 1.21533 | 1.20269 | 0.9896 |
| mixed | 65536 | 77.48438 | 76.80625 | 0.9912 |
| mixed | 1048576 | 1250.60000 | 1232.90000 | 0.9858 |
| mixed | 16777216 | 20898.60000 | 20741.10000 | 0.9925 |
| late-japanese | 16 | 0.02062 | 0.02115 | 1.0259 |
| late-japanese | 128 | 0.16753 | 0.16022 | 0.9564 |
| late-japanese | 1024 | 1.27173 | 1.22178 | 0.9607 |
| late-japanese | 65536 | 80.71875 | 77.35938 | 0.9584 |
| late-japanese | 1048576 | 1308.50000 | 1256.10000 | 0.9600 |
| late-japanese | 16777216 | 21668.50000 | 21529.20000 | 0.9936 |

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
| conformance-01 | 候補1の6docs/160参照、diff/source/build graph0違反 |
| CI41971675035 / merge-01 | 候補1のmemchrをARC-003未許可外部symbolとして拒否。自動再送run41972123004はcancel済み。Draft復帰後に所有merge子を停止し終了値4294967295を保存 |
| compile-candidate-02 / measure-candidate-02 / inspect-01 | 許可済みmemcpyによる候補2を比較。大きな混在文で約24%増のため不採用。assemblyも保全し、悪化原因は断定しない |
| compile-candidate-03 / measure-candidate-03 | decode出力初期化をloop前へ移した候補3。正常24入力/480sample、全結果・本文一致、stderr空 |
| build-debug-02 / contract-02 | 最終sourceのformat/tidy/3actions、--editor-eligibility成功、stderr空 |
| symbols-01 | 補助入力のlibrary名をfolio_と誤指定。子未起動/記録125、missing input2件の前後計4errorを保持 |
| symbols-02 | 実在するnenefolio_core/applicationへ訂正。2library、依存違反0。許可集合・ゲート不変 |
| audit-03 | 候補3の全24入力/480sample/7symbol/source/archive/vector/中央値・cycles照合成功。失敗試行も実終了値で集約 |
| conformance-02 | 最終6docsの参照・diff/source/build graphを確認する |

候補1はCIの依存違反で不採用。許可集合を拡張せず実装を直し、候補2の悪化も残した上で候補3を採用する。
初回監査の失敗は保持する。linkerが未使用関数を除去しただけで製品/比較結果の失敗ではない。
独立text_validation_reviewは読み取り専用、現席と同じ既定model、編集/build/test/追加copyなし。
候補1のsource SHA6867020d…da7d26へ必須修正0。候補3の追補はindependent-review-02.mdへ保存する。
親が実sourceと照合して受理し、実Debug/tidy/対象結果は親の記録で確認した。
整形前後は空白以外一致を照合して成功比較を再利用する。工程だけでは再実行しない。

規則: ARC-001/003/004/007/008/012、C-008/012/014/016、QLT-008/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。全対象実行はtools/record-run.pyで初回失敗も含め保持。
補助読取の不存在ADR名/過大出力は全体journalへ分離する。

原本D:/NeNeFolio/agents/647-editor-eligibility/、作業木は同worktrees/647-editor-eligibility/、
一時出力は同outputs/647-editor-eligibility/。全体journalはD:/NeNeFolio/agents/quality-20261105/journal.md。
最終head必須CIのeng/check.ps1、正典統合、通常版反映/data32file不変、全保全/hash照合後の専用領域整理は
同taskのintegration-completion.jsonで確定する。branch/commit保持。実app/実ノート/Storeを試験に使わず、#634は保留する。

## 14:30 統合・通常反映・保全整理完了

PR648 head5bb819d476b92c302fae8d15672de7d632e0433b、最終CI41973359728 full-gate/check成功。
main8e74a9122b2b335c553714dc09aaf884922ef3a8へ正典統合、Issue647 CLOSED。通常版再build/check成功、data32file不変。
最終1523fileのZIP全hashを再照合し、取込tree/clean-untracked/ignored/link/稼働参照の監査後、647専用worktree/outputを削除。
旧候補1ZIP/branch/commit保持。全失敗実値も含む確定原本は同taskのintegration-completion.json。通常app起動なし。
