# 主窓操作の常設入口と一覧退出時の復元 — #452

2026-10-30。候補v5の対象検証を受理し、PR #455の最終head必須CI成功後、mainへ統合・通常exe反映・安全整理まで完了した。controlsで見つかった設定面の復元不備も修復し、設定/テーマ2ケース15項目が一致した。
規則: ARC-001/002/004/011/012、C-002/008/012/014/016/017/018、QLT-008/010/012/013。Waivers none。

## 範囲と設計

最小化・最大化・元に戻すを共通headerの常設入口にする。Win32の既存WM_SYSCOMMANDへ合流し、OS状態を正本とする。
最小320幅でもshell操作を残し、移動24を保てない幅では表示3択だけを52logicalのchooserにまとめ、既存CommandLayer MENUを使う。
新たな保存state、擬似最大化、native popup、独立button描画系は作らない。

デザインリナの批評と独立静読を受理した。初回の320でshell操作を隠す案、560幅の16px算術誤差、native menu案は訂正済み。
static headerの可視・構造通知と、MENU行の世代・現在mode印を区別する。
原本は`D:/NeNeFolio/agents/452-window-controls/design-decision.md`と`independent-boundary-review.md`。

## 旧placementの観測準備

[ADR0047](../adr/0047-three-layout-modes.md)の一覧への入出契約を対象に、main`3ef671f`から専用作業木を準備した。
normal保存→DRAWER中にOS最大化→BOTH/NOTE、max保存→DRAWER→BOTH/NOTEの4freshケースを実行する。
後者には最後のOS restore対照を含む。前者では一覧の位置/高さを先に変更し、保存wide_placement全体へ巻き戻す修復と区別する。
geometry transactionを実行するprivate helperと実rootへのWM_SYSCOMMANDであり、物理mouse、新button、外側の本文focus処理の試験ではない。

親はfixture/計画のhash不変、既受理driverとの10派生補助関数のAST一致、#451通常反映を照合して実行を受理した。
configure/build/deriveは新しい452 source rootの実metadata/provenanceから行い、過去の全unitを繰り返していない。
compile/linkの完全Debug vector、C23/MT、ASan/UBSan/nullability/no-recover、RCと4runtimeを保持し、製品bytesを主TUへ一回だけ展開する。
元folio_window.c.objを重複抽出せず、実argv/map/展開bytesと入力snapshotを保持する。
通常dataを使わず、D側runtimeに合成settings/category/index/空本文とmanifest一致fontsを置く。

## 最初の観測は終了処理の不備で未受理

`configure-old-01`、`build-old-01`、`derive-old-01`はchild/wrapper0・記録error0。
deriveのlink.logにlldの`ignoring unknown symbol record kind 0x1176`が出たが、linkはexit0。警告を隠す設定は加えていない。

`observe-old-01`は4ケースを最後まで観測したが、専用desktopのCloseDesktopがerror170で失敗した。
最終rawはcompletedCases4、contractViolations6、technicalFailures1、exit3。0や2へ読み替えず、旧状態の正式完備受理を保留した。

診断rawの不一致6件はnormal保存からの退出2ケースに各3件だった。実120DPIで、一覧normalのscreen frameは`[90,80,490,605]`、
退出後は幅950へ戻る一方、IsZoomed1/showCmd3、screen frame`[0,0,950,2176]`になっていた。
max保存側は元の最大化とnormal位置を戻し、最後のOS restoreで初期normal frame`[50,50,1000,650]`へ到達した。
これらは失敗run内の診断事実であり、4ケース全体の成功や後始末の成功とは呼ばない。

計画中の「GetWindowPlacement.flagsは0」との記述に対し、max時の実rawでは2だった。凍結入力を書き換えず、この差をここで訂正する。
oracleは元からflagsを最大化の意味判定に使わず、IsZoomed/showCmdと同じ座標系内の矩形を使っている。
CloseDesktopは同processの他threadが使用中の場合等に失敗する契約だが、今回の所有threadの原因はこのrawだけでは確定していない。
[Microsoft CloseDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-closedesktop)を照合し、試験側の寿命を切り分ける。

成功/失敗attempt、runtime、raw JSONL、実終了値、入力snapshotは`D:/NeNeFolio/outputs/452-window-controls/`に保持する。
後始末の不備と観測の受理条件を閉じた結果を、次節に記録する。

## controller/worker方式で旧動作の観測を完備

初回の個別thread所有者は断定せず、試験側のdesktop寿命を修復した。controllerは元のdesktopに留まり、
作った専用desktopを同exe workerの初期desktopとして指定する。workerは期待名一致をCOM/製品生成前に要求し、fallbackを持たない。
workerの全threadを含む実process終了を待ってからcontrollerがCloseDesktopする。内側20秒＋終了待ち5秒、外側30秒。
timeoutは所有process handleだけを対象に失敗扱いとし、worker/controllerのraw・stderr・実終了値を別々に保持する。

元の4case生成/oracleを含む17,978bytesは不変。固定prefixの改行変換を静的検査が検出して実行前に訂正し、bootstrap部分だけ整形した。
製品source/configと成功済みcanonical buildは再利用し、新たな製品buildは0。`derive-old-02`はchild/wrapper0・記録error0。
成功した正典sanitizer/vectorと単一TU経路を保持し、v1の失敗runを変更していない。

`observe-old-02`は実120DPI、**4ケース・24snapshot段階・30契約判定・6不一致・技術不備0**。
worker/controller/runner/記録wrapperは実exit2であり、製品の成功0へ変換しない。
controllerはDefault、workerは専用desktopに留まりbind0、実process終了→CloseDesktop成功、timeout無しを確認した。
不一致6件は初回と同じnormal保存側の2ケースに各3件。max保存側の2ケースは元のnormal位置へのOS restore対照まで一致した。

親は44代表snapshot hash、全24段階、状態/寸法の整数、worker/controller終了、desktop帰属/解放、展開bytes/TUを独立照合した。
`parent-old-placement-acceptance.json`で、旧動作の負側根拠として正式受理した。親による追加build/probeは0。
担当の読取監査utilityはmetadata-only行にokを要求した誤りとsanitizer token形式の誤想定を別attemptで訂正した。
実製品/fixture/vector/rawを変えて成功させたものではない。正典argvはaddress/undefined/nullabilityの3tokenとno-recoverを保持する。

## 実装判断

[ADR0075](../adr/0075-window-shell-controls-and-layout-chooser.md)を製品編集前に確定した。
normal保存かつ現在IsZoomedの一覧退出だけをOSの通常状態へ戻してから、一覧の現在normal位置/高さと保存wide幅を使う。
保存trueのwide_placement復元と、既存transaction/失敗rollbackは維持する。
通常の窓操作・新chooser・UIA/keyboard/MENUの完成候補はこの旧placement診断とは別に確認する。

## 候補の静的レビュー

18 source/testの候補v1を固定し、必要な独立静読を依頼した。親の途中レビューで、560以上の既存56/16配置を保持する条件、
offscreenとheader表現を混ぜないgeometry由来のHEADER値、既存metadataから組むchooserの説明を訂正した。
最小化/最大化/復元は共有shell判定から既存system commandへ合流し、現在mode印とMENU候補は別に導出する。

独立静読は新min/max/chooserの中央が旧hit_testでHTCAPTIONになる接続漏れを発見し、親もsourceを照合した。
描画・UIAには入口があっても、実pointerのclient配送条件が欠けていた。共通layout slotとshell矩形を同判定へ加える訂正を採用した。
候補v1のpatch/hashを保持し、訂正後は新しい候補として固定する。WM_NCHITTESTのclient判定を対象検証へ含め、
press helperへの合成入力だけを物理pointer成功とは呼ばない。この時点で候補build/test/probeは未実行。

原本は同taskの`parent-early-review-01.md`、`candidate-source-inputs-v1.json`、`candidate-source-v1.patch`。

訂正v2の18hashを親と独立レビューが照合し、R1のsource上の解消を確認した。追加の具体的不具合は0。
`independent-candidate-review-v1.md/json`はv1の発見とv2の解消を分け、動的成功を認定していない。

## 対象ビルドの準備中に見つかった記録用スクリプトの不備

既存Debug設定からnenefolio/folio_testsだけを作り、既存`--menu-surface`だけを実行する計画を受理した。
最初の`build-candidate-01`はCMake開始前に停止した。実cacheの`Debug\r\n`を末尾CR非対応の正規表現で誤拒否し、child1。
未生成のunit exe/tool identityのartifact記録2件も失敗してwrapper125。入力426は不変で、製品compile/tidyやunitの失敗ではない。
v1 script/record/rawを保持し、厳密なDebug値にLF/CRLFだけを認めるv2へ訂正した。
実cacheで旧match=false/新match=true、parser errors0を静的照合し、新しい`build-candidate-02`を受理した。
候補source18hash、対象、警告、sanitizer、既存設定は変更しない。後続unitは成功buildの全入力/実exeを照合してから実行する。

`build-candidate-02`は実compile/tidyで停止した。新しいchooser説明から既存append_textを宣言前に呼び、
accessible_visibleの認知複雑度12、folio_window_accessible_readの11が既存上限10を超えた。
child1/wrapper125（未生成unit exeのartifact記録1件）、入力426不変、Ninjaは12/49で停止。後続unitは未実行。
既存helperのprivate宣言と表示slotの分類への整理を採用した。空boundsの処理を移す案では、親がNOTICEのlive属性と
native inputのenabled積を迂回する影響を静読で見つけ、hidden layout要素だけへ条件を限定するよう訂正した。
v2/v3原本を保持し、上限・警告・sanitizerを変更せず次の候補を固定する。成功したcompileやUI動作とはまだ主張しない。

## 訂正v4のDebug/tidy・限定unit成功

hidden layout要素だけを空boundsへするv4を固定した。独立静読済みv2からの製品差分はfolio_window.cだけで、
既存append_textの宣言追加、表示slotの分類、layout限定の空bounds処理を親が照合した。他17source/testは不変。
`candidate-source-inputs-v4.json`、`parent-candidate-v4-build-plan-acceptance.json`に入力と判断を保持する。

- `cmake --build D:/NeNeFolio/outputs/452-window-controls/canonical-debug --target nenefolio folio_tests --parallel 2`:
  `build-candidate-03`で38actions完了、child/wrapper0・記録error0、入力426不変、成果物8。既存Debug/tidy・ASan/UBSan/nullability/no-recoverを保持した。
- `D:/NeNeFolio/outputs/452-window-controls/canonical-debug/folio_tests.exe --menu-surface`:
  `unit-menu-candidate-01`でchild/wrapper0・記録error0、入力430不変、stderr空。
  command catalog/shortcut/text/menu availabilityが成功。unit exeの作成で他test sourceもcompileしたが、実行したselectorはこれだけ。
- `python -B D:/NeNeFolio/agents/452-window-controls/parent-build-unit-audit.py`:
  exit0。両recordの入力/成果物snapshotと現file、候補18hash、使用tool5件、unitの実argv/stdout/stderrを独立照合しerror0。
  unique1306filesをhash確認した。製品の追加実行は0、原本は`parent-build-unit-acceptance.json`。

01のdriver失敗と02のcompile失敗は別原本として保持する。成功した候補入力を固定し、文書/レビュー/PRだけの変更で再実行しない。
新しいUI fixtureは、geometry/pointer/代表画像と、placement/keyboard/MENU/UIA/modalの直接境界に分けて静的準備中。
試験用の専用desktopで順番に実行し、通常data・共有cursor・他窓を操作しない。この時点で候補UI derive/probeは0。

## 同じ4ケースでplacement修復を受理

成功build03を使い、既受理のfixture/derive/run/record計5fileのhash不変を親が確認して再利用した。
`derive-candidate-placement-01`と`observe-candidate-placement-01`はchild/wrapper0・記録error0。
実120DPI、4ケース・24段階・30契約判定、**不一致0・技術不備0**。worker/controller/runnerも実0だった。

normal保存側の2ケースは、一覧のscreen位置`[90,80]`と高さ525を保持し、保存wide幅950へ戻り、
normal状態・showCmd1・screen frame`[90,80,1040,605]`となった。max保存側の2ケースは元の最大化/normal位置を保持し、
最後のOS restoreで初期normal frameへ戻った。変更前の6不一致が、同じcase/oracle bytesで0になった。

controllerはDefault、workerは専用desktopに留まりbind0、worker実終了後にCloseDesktop成功。timeout無し、stderr空。
`parent-candidate-placement-audit.py`を実行してexit0。親は44代表snapshot hash、全case/段階/30判定、OS状態/寸法、
desktop寿命、同fixture hash、製品bytesの一回展開/TUを独立照合し、`parent-candidate-placement-acceptance.json`で受理した。
親の製品build/probe追加は0。これはgeometry transactionと合成system commandの対象結果で、新buttonの物理操作を実測したものではない。

## 配置と合成pointerの受理、代表画像の一部は保留

固定v4/成功build03を再利用し、`geometry-derive-candidate-01`と`geometry-probe-candidate-01`はchild/wrapper0・記録error0。
実120DPIで13幅・523契約判定・不一致0・技術不備0。ENは最小320と4閾値の直前/直上、JA/ZHは320/560を観測した。
独立したADR定数の期待矩形と、paint入力・client hit・WM_NCHITTEST・公開UIA viewのboundsが一致し、移動24logical以上を保持した。
外UP取消、normal/maxからの最小化とOS復帰、最大化/復元の実OS状態と元の寸法への復帰を記録した。
合成client入力前にHTCLIENTを要求しており、helper単体の成功をpointer配送成功と読み替えていない。物理mouse・DWM呈示は未確認。

親は`parent-geometry-audit-v2.py`を実行してexit0。67代表snapshot、全13幅/10pointer状態、専用desktopの実終了、
製品bytes一回展開、初期seed、終了workspace全5filesのZIP/hashを照合し、`parent-geometry-acceptance-v1.json`で受理した。
最初の親audit v1は固定SHA文字列の転記漏れでassert失敗し、v2でその文字列だけを訂正した。製品や試験の再実行は0。
専用dataでは言語設定を変更し空lock fileが追加された。可変workspaceを不変inputとは扱わず、通常dataは使っていない。

PrintWindow原本8枚は全て開いて確認したが、JA320は全面黒、EN560は直前の狭い描画と黒い右領域が残っていた。
API戻り値成功だけでは視覚合格にせず、この2枚の受理を保留した。他6枚は判読可能で、現在mode印と復元glyphを含む。
この時点では撮影時の更新と製品描画のどちらに原因があるか未確定。原本を保持して静的調査中で、523項目の再実行はしない。

## 2枚だけの撮影補助を修復して受理

旧EN560は700幅の末尾195列が黒く、直前に観測した505幅までの描画が残っていた。JA320は全画素が黒だった。
旧rawの現在client/geometryとは異なり、APIの戻り値だけで画像を受理できない事例として保持した。
製品描画の不具合やOS内部cacheが原因だとは断定していない。

同じv4/build03を使う新fixtureで、fresh EN560/BOTHとJA320/DRAWERの2枚だけを撮影した。
元のprocedureを一回呼ぶ観測用WNDPROCで自然なWM_PAINTを記録し、RedrawWindowを一回実行してからPrintWindowを一回呼ぶ。
戻り値、update region、現在client/DPI/language/mode、observer解除をrawへ残した。sleep/retryや代用描画は追加していない。
製品build、旧523項目、旧8画像は再実行していない。

`geometry-capture-derive-candidate-01`と`geometry-capture-probe-candidate-01`はchild/wrapper0・記録error0。
2case/2画像/2粗い画素判定、不一致0・技術不備0。両caseでRedrawWindow成功→WM_PAINT1回→update空を観測し、
PrintWindow中にもWM_PAINT1回を観測した（WM_PRINT/WM_PRINTCLIENTは0）。observer復帰とworker実終了後のdesktop解放も成功した。
これは今回の専用desktopでの観測であり、PrintWindow内部の一般契約や物理displayの呈示を証明するものではない。

親は2枚を元解像度で開き、EN560の字付き新規/表示3択/shell、JA320のchooser/shellと黒領域の解消を確認した。
`parent-geometry-capture-audit-v1.py`はexit0。61代表snapshot、全16paint段階、2observer復帰、展開bytes/TU、
seed4fileと終了workspace全5filesのZIP/hash、desktop寿命を独立照合し、`parent-geometry-capture-acceptance-v1.json`で受理した。
初回で判読可能だった6枚と合わせ、代表8構成の画像を確認した。親の製品実行追加は0。

撮影補助の静的保全は、親が更新したquality文書を旧hash固定と誤認して一度停止し、旧snapshotと現在のproseを分離した。
実行後の監査では広いprocess正規表現が監査scriptを生成する自身のshellを誤認し、別の単独監査で照合した。
両失敗原本を保持し、製品derive/probeの再試行や旧証拠の変更にはしていない。

## 残るcontrols fixtureの独立静読

keyboard/静的provider、chooserと既存MENU、child設定面、実native名前dialogの4fresh caseを準備した。
通知は試験用の非UI MTA threadで実UIAutomation clientを使い、HEADER StructureChangedと最大化buttonのNameだけを購読する。
SDKの定数定義を第二TUへ複製せず、exact SDKのconst-only blockを検査した宣言用importと既存libraryを使う。

必要な独立静読で、未実行v1 fixtureのhandler最終Release後にsharedを参照するUAF可能性R1を発見し、親も確認した。
v2はSetEventを最後のshared参照解放前へ移し、以後sharedへ触れない。旧4fileを保持し、訂正後の未解消要修正0を確認した。
単一cleanup期限、同MTAでの登録/解除、UI message pump、部分登録失敗、実DialogBoxからのnested shell/close寿命も静読した。
原本は`independent-controls-fixture-review-v1.md/json`と`controls-fourfile-inputs-v2.json`。compile/実配送/実native成功はまだ未確認。

StructureChangedは方向payloadがなく、遅れて届く旧通知が新しいlive幅を読む可能性がある。
受理する観測は「各action marker後に同targetの実callbackを受け、その時点の幅が一致する部分順序」までとし、
通知の生成元actionの一意同定や遅延duplicate排除とは呼ばない。Nameは実callbackのnewValueとの対応を確認する。
実reader発話、通知の総数/厳密全順序、物理monitor移動の一般保証は対象外。

## controls派生時のcompile/link不備

親が最終plan/43入力hash/controller同値を照合して実行を受理した。
`derive-controls-candidate-01`はSDKの`interface`マクロと試験側member名の衝突、headerのHWND/REFIID/HANDLE注釈と末尾改行でcompileに失敗した。
child1/wrapper125、入力458不変、未生成artifact等の記録error9。実native probeは起動していない。
SDKマクロをundefせずmemberをproviderへ改名し、注釈/改行だけをv3へ訂正した。R1の解放順・通知条件・期限・製品18fileは不変。

`derive-controls-candidate-02`はcompile成功後、`UIA_ScrollPatternNoScroll`の二重定義でlinkに失敗した。
child1/wrapper125、入力458不変、記録error7。生成objとlink.logを保持し、exe/map/実probeの成功とは扱わない。
既に対象としたClient.hの391個のconst longとは別に、Core.hの独立guardにconst double一個の定義があった。
親は実Core.hのhashとguard内がその定義/コメントだけであることを照合し、同じtest-only宣言importの方針をこのguardにも適用すると決めた。
数値/ABIを複製せず、製品node.objの単一所有を392定義で確認する予定。flags/library/TU/製品sourceを変えず、新生成物の静的受理後に再開する。

## controls実観測は4ケース完備、child復元の1項目を調査中

Core定数guardだけを追加したv4 fixtureと、31helper/18製品入力hashを親が受理した。
`derive-controls-candidate-03`はchild/wrapper0・記録error0、入力459不変、成果物12。
実nmと最終mapで391個のlongと1個のdoubleがcanonical accessibility_node.objだけの定義であることを確認した。
SDK探索は実toolchainのINCLUDE/既存-I順とexact path/hashの静的照合であり、compiler include traceとは呼ばない。

`observe-controls-candidate-03`は4freshケース完備、**86契約判定・不一致1・技術不備0、実exit2**。
worker/controller/runner/記録wrapperの2を成功0へ読み替えない。入力475不変、記録error0、worker実終了後のdesktop解放成功、timeout無し。
Tab一slot巡回、静的providerの可視/拒否、chooserと既存MENUの寿命、実native DialogBox中のnested shell/closeは一致した。
非UI MTAでHEADER構造通知の2phaseと最大化/復元Nameの2phaseを実受信した。NameのnewValueはRestore/Maximize、
cleanupはjoin済み・threadExit0・両remove0・handler0・UI所有参照1。構造通知の原因帰属限界は前節のまま。

不一致は「child settings normal/minimize/restore keeps surface state」の1件。
元の複合判定はIsIconic/IsZoomed/command_surfaceの個別値を記録しておらず、どの条件が異なるかはまだ確定していない。
静読ではcommand_layerのWM_KILLFOCUSから遅延focus_lostを経て設定面を閉じる経路を候補として特定した。
製品原因と試験の操作順を区別するため、このchildケースだけに操作前後とfocus/messageの非介入観測を追加する。
成功したMTA等の他3ケースや製品buildは繰り返さない。旧原本とoracleを保持し、未受理の製品修正は行っていない。

同run後data全一覧は既存seed4fileの変更/削除0、追加は空`.nenefolio.lock`だけで、終了5fileを保全した。
親の`parent-controls-audit-v1.py`はexit0。1442個のsnapshot/現file hash、全raw、4phase実受信、MTA/native寿命、
392定義の単一所有、data全5fileを独立照合した。`parent-controls-audit-v1.json`は全体未受理と実exit2を保持する。
追加の製品実行0。Waivers none。

## child限定診断で遅延focus_lostの経路を特定

診断fixtureは非介入の同thread observerと単一sequenceを加え、元のchild操作順/複合oracleだけを実行した。
静的v1のsequence pointer不整合/不要な全体整形/不正確なafter_drain名を実行前にv2へ直し、旧稿は保持した。
SetWindowSubclass/DefSubclassProcは既存comctl32 library内で使い、各messageを元chainへ一回渡す。focus補正はしない。
`derive-controls-child-diagnostic-01`は0/0/error0、`observe-controls-child-diagnostic-01`は1case/10契約/1不一致/技術不備0、実2/2。

50行の時系列で、最小化後seq32はiconic1/zoomed0/surface5・focus無し。
復元のWM_SETFOCUS(seq39)はrootへ戻り、seq44はiconic0/zoomed0/showCmd1・surface5を維持していた。
queue drainのseq46→47でfolio_message_command_focus_lostを処理するとsurface5→0、子面可視1→0になった。
したがって今回の不一致はOSの通常状態への復帰失敗ではなく、既存focus喪失処理がrootへの復帰を面外操作と扱う経路である。
観測の追加がtimingへ影響し得る限界は残す。物理的なtaskbar操作の計測とは呼ばない。

親の`parent-child-diagnostic-audit-v1.py`はexit0。54代表snapshot、全50段階、同じ不一致、実終了/desktop解放、
展開bytes、seed4file不変と追加empty lockを照合し、`parent-child-diagnostic-acceptance-v1.json`で診断を受理した。
製品の成功とはせず、ADR0075へ実root focusの既存正規化入口を補正する方針を先行記録した。
設定/テーマの有効可視入力だけへ戻し、新しいfocus状態・強制activation・通知無視は加えない。実装/修復検証は次の段階。

## v5のfocus修復と対象検証を受理

製品差分はfolio_window.cの既存WM_SETFOCUS正規化だけ。設定/テーマ・native深さ0・非最小化・実rootfocus・
rootと既存targetの有効可視を条件にfocus_command_inputを使う。他17source/testのhashはv4から不変。
通常NOTE、native、他HWND、最小化/無効/非表示の条件は静読で除外を確認した。新たな動的負側テストを実行したとは呼ばない。
原本は`candidate-source-inputs-v5.json`、`parent-candidate-v5-source-acceptance.json`、`candidate-v5-verification-reuse-boundary-v1.md`。

`cmake --build D:/NeNeFolio/outputs/452-window-controls/canonical-debug --target nenefolio --parallel 2`は
`build-candidate-04`でchild/wrapper0・記録error0。window.c compile/tidy→window.lib→exeの3actions、入力426不変、成果物8。
既存folio_testsはprovenanceとして記録しただけで、このrunでは再作成/再実行していない。
親の`parent-build-v5-audit.py`はexit0。868個のsnapshot/現file、製品18file、tool5件、実argv/stdout/stderrを照合した。

旧設定caseの操作/oracle bytesを維持し、新たにテーマの既存EDIT targetへ戻る1caseだけを加えた。
最初の`derive-controls-child-focus-01`は試験側local変数名が展開製品のstatic名を隠す-Wshadowでcompile失敗した。
そのlocal名と参照2箇所だけを訂正し、旧v1/rawを保持。製品v5、判定、期限、flags、library、TUは変更していない。

`derive-controls-child-focus-02`と`observe-controls-child-focus-02`はともにchild/wrapper0・記録error0。
derive入力459不変/成果物12、probe入力475不変/成果物6。**設定/テーマ2freshケース・15契約・不一致0・技術不備0**。
設定は復元後seq56でsurface5とlayer focusを維持し、遅延focus_lost後のseq62でも変わらない。
テーマは復元後seq29/queue drain後seq34でsurface6と元のEDIT focusを維持した。候補/保存theme/paletteも不変。
worker/controller/runnerは実0、timeout無し、workerの全process終了後にdesktopを解放した。

親の`parent-child-focus-audit-v1.py`はexit0。56代表snapshot、全104focus/message段階、15契約、
実終了/desktop寿命、展開bytes/単一TU、392定義の所有記録、終了data全5fileを照合して受理した。
元seed4fileの変更/削除は0、追加は空lockだけ。原本は`parent-child-focus-acceptance-v1.json`。
旧4caseの実2と診断の実2を成功へ書き換えていない。成功済みのmenu unit・placement・geometry/画像・MTA/nativeの
直接経路は、このsettings限定の差分で変わらないため再利用した。製品source/config/hashと適用条件を照合し、工程だけで繰り返さない。

## 統合条件と残る限界

最終文書参照/空白/製品hash/正典conformanceを`parent-final-record-draft.ps1`で記録した。
初回parent-final-01は旧品質文書末尾の余分な空行を検出し失敗。空行だけを訂正したparent-final-02は
30変更file/132参照/18製品hashを照合し、conformance違反0、child/wrapper0・記録error0だった。旧失敗は保持した。
最終head`5f42f88272333f22f81f523111b056720a34634f`の必須CI`40990655449`で
`pwsh -NoProfile -File ./eng/check.ps1`が成功し、PR #455をmain`ee65c6d23f78276a7f83358a59627abc4a02a17d`へ統合した。
正規`tools/prepare-real-machine.ps1`の通常反映もchild/wrapper0・記録error0。通常exeは11:23:10 JST更新、5,559,296bytes、
SHA256 `0ADACB5BE97FB89E51B3FE092F3258897E7DE7A3AAFBC6B0E21A567229FD7DAA`。更新後は起動していない。
13977files/2,187,035,109bytesを恒久archiveへ全hash照合して保全し、dirty/untracked/ignored・link・稼働参照0を確認した。
追加worktreeと一時出力を削除、branch/commitを保持した。旧削除拒否47箇所は操作していない。
確定値は`D:/NeNeFolio/agents/452-window-controls/integration-completion.json`。runの旧output pathは同taskの`output-archive`から辿れる。
保存schema・依存・ゲートの変更なし。Waivers none。

専用desktopの合成入力・UIA受信・PrintWindowの結果であり、物理mouse/IME、reader発話、複数monitor跨ぎ、
DWM実画面呈示を測定したものではない。StructureChangedの生成元action一意同定/遅延duplicate排除も保証しない。
Snap hover/Win+Zの完全対応、永続窓位置、Store/installerは対象外。通常dataは試験に使っていない。
