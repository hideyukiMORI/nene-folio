# 狭幅表の可読列幅と水平到達の成立性 — #456

2026-10-30。base main `ee65c6d`。非製品の固定3条件を診断し、3列では成立、63列2条件では幅設定が不成立だった。
TOM処理と表配置の結果を分け、実幅・原文状態・画像を照合して診断を受理した。製品方式・最小列幅は採用しない。
規則: ARC-001/004/008/011/012、C-008/012/013/014/016/017、QLT-008/010/012/013、GIT-001〜004。Waivers none。

## 調査と焦点

現行`note_pane_table.layout_row`はnative rowの字下げを引いた残幅を全列へ均等配分する。
最小列幅はなく、字下げ以上の幅が無い場合は0幅になる。ADR0072決定6はこの状態の可読性を保証していない。
callerのviewport幅は31679twipで上限化されるため、対応indent32767は現callerの全幅域で残幅0になる。
これは静的算術上の帰結であり、今回の動的不具合再現ではない。横barだけでは圧縮された列幅を広げられない。

次の判断に必要なのは、列幅を保ったnative表を最後のセルまで横移動でき、前後の通常段落はviewportで折り返す条件である。
単一RichEdit、既存Markdown→RTF、native行配置、原文・選択・dirty・Undoを保つ範囲で調べる。
表ごとの別viewer/独立scroll container、通常本文のnowrap化、保存schema変更は含めない。

SOLの静読報告は`D:/NeNeFolio/agents/quality-post452-tables/read-only-investigation.md`。
親は報告のsource/ADRと28入力hashを照合し、`agents/456-table-horizontal-feasibility/parent-research-acceptance.json`へ受理を記録した。
同じ継続作業の明示分担で、設計・受理・統合はAstra、非製品fixture/driverと対象検証はSOLが担う。

## 設計相談と未採用事項

Claude Codeのデザインリナへdesign-critique/design-handoffを読み取り専用で依頼した。
初回回答の96logical px相当/1440twipという幅は、フォント計測前の診断刺激に限って使う。製品の最小列幅は未採用。
縦barは既に外観だけを隠しているため、横barを可視にする案は新しい見た目/操作の判断を要する。

初回回答の31679をnative表総幅上限へ流用する案と、`accessibility_node`の横NoScrollを本文providerへ適用する説明を親が訂正し、リナも撤回を確認した。
31679はcallerのviewport入力capであり、native総extentの既知上限ではない。自前ScrollPatternはTREE/SURFACE_LIST限定で、本文RichEditのnative providerとは別である。
未確認の総幅で列幅0へ退行するfallbackや本文用の独自UIA経路は採用していない。旧回答も原本として保持する。
原本は`agents/quality-post452-tables/design-rina-result.json`と同followup。

## 限定診断の準備

専用worktreeは`D:/NeNeFolio/worktrees/456-table-horizontal-feasibility`、branch `test/456-table-horizontal-feasibility`。
補助source/判断/恒久原本は同名の`agents/`、新規run/runtimeは同名の`outputs/`へ置く。
現styleと既存public APIを使い、root少数列と通常段落、多列/深い字下げの3程度の固定caseを準備する。
正典metadataと#452恒久archiveの一貫したlibs/RC/runtimeを読取再利用できるか確認し、不要な製品全体buildを追加しない。

実行前にfixture/driver/固定oracleを親が受理する。全セル幅/indent、実DLL/DPI/zoom/style、client/format rect、
native range/page/pos、要求と実x、左右端固有tokenの座標を記録する。戻り値1だけを右端到達の証拠にしない。
通常段落の折返しと、文書全体のx移動により通常段落が一時的に隠れる範囲を分けて観測する。
raw/選択/dirty/非空Undo/Redoと代表画像を確認し、programmatic移動を物理pointer/keyboard/readerの成功へ広げない。
不成立なら原本を保持して次のstyle対照を別途判断し、同一runで設定を次々に試さない。

静的v1は3case、PS3本/Pythonの構文検査と正典整形を完了し、親は9補助fileと377source/config、5archive成果物等の393hashを照合した。
親早期静読でnative列数の範囲、通常段落の実point、失敗cleanupを補い、幅/scroll契約の後だけseed済みYの実Redo→Undoを追加した。
復帰用のfixture選択操作を製品保持の証明へ混ぜず、native bar表示bitと固定styleも分けた。
独立静読では深い字下げ設定側の列数guard漏れと、tool/runtime pinの参照不足を確認した。
build04のinputsにはsanitizer runtime4本が無く、v1の参照は実行前に停止するため、実在する別の成功derive原本へ結び直す。
compiler/linkerは同buildのtool identities成果物を正しく参照する。v2で出所を訂正し、v3でsetter前のnative level非0も補った。
親と独立静読は最小差分を照合して未解消指摘0。旧v1/v2を保持し、受理済みv3で初回derive/probeへ進んだ。
静的監査utilityのCP932既定読取失敗も保存し、UTF-8へ訂正した。製品の不具合や動的結果とは数えない。

## 初回の限定実測と追加観測の理由

`table-derive-current-style-01`はchild/wrapper0・記録error0。入力804件、成果物7件の原本/snapshotを照合した。
source/config377件、archive libs/RC5件、実tool2本とversion、runtime4本の一致を確認し、map/linkは既存native/Markdown memberを選択した。
製品targetの再buildは0。fixture exeはSHA256 `ff5b37c510ca63e07a8d55a4d1ffa1ddc59cdb346d151a58ec02b6566c19bd75`。

`table-probe-current-style-01`はMSFTEDIT `10.0.26100.9949`、120DPI、実client320×600pxで実行した。
root3列/100%は各1440twipを保ち、左右tokenの先頭/末尾glyph-startはviewport内。29契約の不一致0で、
通常段落の相対改行profile、raw/選択/dirty/非空Undo/Redo、最後の実RedoY→UndoXも一致した。
親は右端画像の`EDGE_R`全文可視を確認した。通常段落の先頭xは1→-246となり、左clipも画像と整合する。
折返しが保たれることと、横移動中も通常段落全体が見えることは別である。横rangeの値0/100/page0も実座標へ読み替えない。

root63列/100%は90720twipの入力時、TOM guard付き既存`note_pane_table_layout`呼出しの複合条件がfalseとなった。
worker/controller/runner実exit3、technical1、契約不一致0。深い字下げ/300%のcase3には進まなかった。
record-runはchild3に加え、未生成case2/3画像のsnapshot error2を記録してwrapper125。成功扱いせず全原本を保持する。
TOM suspend/resumeとlayout結果の内訳、失敗直後の全列幅は初回rawに無いので、native総幅上限や原因は断定しない。
成功済みcase1を再実行せず、同じ幅/styleのcase2と未実施case3へ、結果内訳・失敗後readbackだけを追加するv4を準備する。
timeoutはなく、worker実終了後にdesktopを閉じた。通常data/実画面入力/DWM呈示の検証ではない。
原本は同taskの`derive-current-style-01-audit.json`、`probe-current-style-01-audit.json`、`parent-probe01-partial-observation.json`。

## 失敗後の追加観測と受理結果

v4は同じ幅/style/既存public APIのまま、TOM suspend/resume、型付きlayout結果、失敗後の全列幅を記録する。
成功済みcase1は再実行せず、case2と未実施case3だけを対象にした。失敗後は幅の期待値を判定せず、実readbackとして残す。
caseごとの完遂/失敗段階/cleanupも分けた。幅設定が失敗してもcleanupが成功すればfresh case3へ進み、
cleanup自体が失敗すれば参照を保持してcase3を開始しない。旧稿・初回失敗も保持する。

親が実差分と固定hashを受理後、`table-derive-current-style-02`はchild/wrapper0・記録error0、
804inputs不変・7artifacts/811snapshot照合に成功。新exeはSHA256 `2b9cfb7040257cd863579496e8db3f4ec8cac4f77c2cf22e430844718a9456a9`。
続く`table-probe-current-style-02`は実worker/controller/runner/record child/wrapperがすべて3、記録error0。
815inputs不変、9artifactsを保全した。タイムアウトはなく、両caseのpane/parent破棄とworker終了後のdesktop閉鎖に成功した。

| 固定条件 | 実結果 | 読み取れる範囲 |
| --- | --- | --- |
| root3列、100%、各1440twip | 初回の29契約不一致0。左右token可視、通常段落wrapと実RedoY→UndoXも一致 | この条件のprogrammatic成立。通常段落も文書と一緒に横移動する |
| root63列、100%、入力90720twip | layout結果`NOTE_PANE_LOAD_FAILED`。各行は36列×61+27列×60=3816twipのまま | 可読幅への変更は不成立。横到達/最後の実Redo→Undoは未実施 |
| 63列、native indent32767、300%、入力123487twip | 圧縮配置は成功し各63列が0幅。拡張layoutは失敗し、両行のindent/0幅は不変 | 対応indentと可読幅の同時成立を示せない。Markdown由来indentの試験ではない |

case2/3のwide呼出しはともにTOM suspend/resumeのHRESULTが1（SUCCEEDED）、layout結果が2（FAILED）。
初回の複合失敗を、TOM失敗ではなく既存表配置の結果へ切り分けた。内部のSET/GETどちらが失敗したか、数値の総幅上限は未確定である。
各caseのbaseline/失敗後の10契約は不一致0で、全raw・選択2..7・dirty=-1・非空Undo/Redo・固定style/DPIを保持した。
ただし幅設定に失敗したので、両caseはtechnical1ずつ・全契約未完。20項目不一致0を「横移動まで成功」と表現しない。

親は2画像を確認した。case2は圧縮された63列の縦罫線、case3は300%通常段落の上部で表はviewport外。
case3の0幅はnative readbackの観測であり、画像から表の可読性を証明していない。再撮影やfallbackはしなかった。
原本は`derive-current-style-02-audit.json`、`results-current-style-final.md`、`parent-probe02-acceptance.json`。

結論として、現styleのprogrammatic横移動は少数列で成立したが、今回の可読列幅を63列/深い字下げへ適用する方式は採用しない。
横bar追加だけでは未成立の幅設定を解消できない。別の表示経路や0幅fallbackを追加せず、数値境界/内部失敗原因と
pointer/keyboard入力、本文の横移動、native UIAを次の設計判断として残す。製品source/config/schema・ゲート・版の変更は0。
文書は差分/参照/conformanceを対象検査し、統合は最終PR headの必須CIに従う。
統合・通常exe hash維持・全出力の恒久保全と安全整理の確定原本は同taskの`integration-completion.json`へ残す。

一次資料: [EM_SETSCROLLPOS](https://learn.microsoft.com/en-us/windows/win32/controls/em-setscrollpos)はpixel座標をnativeが補正する。
[EM_GETSCROLLPOS](https://learn.microsoft.com/en-us/windows/win32/controls/em-getscrollpos)には16bitの戻り座標制約がある。
[RichEdit styles](https://learn.microsoft.com/en-us/windows/win32/controls/rich-edit-control-styles)の横自動scrollと、表の可読幅は別の条件として扱う。
[EM_SETTABLEPARMS](https://learn.microsoft.com/en-us/windows/win32/controls/em-settableparms)のcCell条件は1〜63。
[TABLECELLPARMS](https://learn.microsoft.com/en-us/windows/win32/api/richedit/ns-richedit-tablecellparms)のdxWidthはLONGであるが、
確認したAPI資料は数値の総幅限界を明示していない。型の大きさや別のWord保存形式の制約からnative実行上限を推定していない。
参照確認は`parent-native-api-reference-review.json`へ記録した。

## 背景読込の並行静読

SOLが32入力を固定して、#438のheld reader中MoveFileExW失敗5→close後同保存成功のrawと、現`file_bytes.c`の完全一致を確認した。
現在は一idle一ノート、matching65536work、線形matcherとcorpus二分lookupが既にある。
残る一ノート同期I/O、検証/copy、確定filter保存後の同期修復、入力/結果反映のlayoutをsource位置付きで整理した。
報告は`D:/NeNeFolio/agents/quality-post452-read/read-investigation-v1.md`。新しい計時や背景化は行っていない。
共有flagだけの案は再採用せず、必要なら一ノートUI占有の分解計測を別の焦点とする。表の診断と同時に性能計時しない。
