# ADR 0089 — 本文表示後に生成するnative Mermaid図

- 状態: 受理（2026-11-03、hideの実装依頼とフローチャート/シーケンス図の優先指定。製品編集前）
- 日付: 2026-11-03
- Issue: #539
- 規則: ARC-001〜012、C-001〜018、QLT-008/009/011/012/013

## 利用者との契約

`mermaid` fenceの閲覧表示にフローチャートとシーケンス図を追加する。原文と保存schemaは変えない。
本文を先に表示し、図は画像と同じ画面付近優先の遅延表示へ渡す。処理中は同じ通知行と進捗メーターを使う。
ノート/モード/テーマ変更後に古い結果を採用しない。表示幅/倍率の変更も同じnative画像再配置へ渡す。

今回は通常のノートで使う次の構文を対象にし、細かな対応表を実装・検証と揃えて利用案内へ記載する。

- フローチャート: flowchart/graph、TB/TD/BT/LR/RL、主要なノード形、ラベル付きの辺、
  実線/破線/太線/矢印、分岐/合流/戻り辺/自己辺、サブグラフ。
- シーケンス図: sequenceDiagram、participant/actorとalias、メッセージ、自己メッセージ、
  note、activation、autonumber、loop/alt/opt/par/critical/break等の枠。

不正構文・未対応機能・入力/配置/bitmapの上限超過は型のある失敗にする。要素を黙って省いた図を成功にしない。
失敗しても本文と編集画面の原文を使える。JavaScript、任意HTML、click callback、外部素材は実行/取得しない。
ほかの図の種類を今回の対応済みとして扱わない。

## 正典経路と所有者

1. 既存Markdown block/container parserだけがfenceを判定する。本文の走査中は内側の原文を収集するだけで、
   図の解析・配置・描画を行わない。画像参照と図の表示slotを一つのmarkdown_mediaへ集める。
   markdown_imagesの原文destination/書き出し責務を保ち、図のslotをファイル参照へ偽装しない。
2. applicationから既存image_portの型付き要求へ図原文を渡す。同じworker/ticket/取消/結果の受け渡しを使う。
   workerは原文・実効配色・書体を所有し、UI/applicationの可変状態やHWND/TOMを借りない。
3. coreのparserが不変モデルを作る。adapterがラベルの実文字幅を測り、不変の寸法列をcoreの配置へ渡す。
   coreはOS・時計・乱数・グローバル状態を使わない。同じ入力から同じ配置を生成する。
4. coreのsceneをadapterがOS同梱GDIで2倍のDIBへ写し、2×2平均で表示bitmapへ縮小する。
   同じfontと倍率でラベルを事前計測する。製品へJavaScript engine/WebView/DOM/SVG parserを足さない。
   完成bitmapを既存image_bitmapとRichEditの1文字差替えへ渡し、選択/読取位置/Undo/dirtyを同じ境界で保つ。
   図の塗りは本文背景へlink色を1/16（group）・2/16（node）・3/16（note）混ぜ、
   文字の4.5:1を保てない塗りは本文背景を使う。本文文字色自体が4.5:1未満なら図内だけ黒/白の高比率側を使う。
   枠のaccentはnode上で3:1以上を満たすlink色、満たさなければ図の文字色とする。
5. 内容・配色・書体の一致を条件に有界cacheを再利用する。保存Markdown/ZIPへcacheを含めない。
   native表示位置とviewport優先順は既存pane ownerだけが持つ。

モデル/sceneの構築中のcollectorはfactoryの同期呼出し内だけで所有し、完成後は不変のqueryだけを公開する。
未完成sceneや可変配列をapplication/UIへ保持させない。永続ASTや別のMarkdown parserは作らない。

画像と図を合わせて256表示slot、図原文は1件256KiB・文書合計4MiB。coreの配置寸法は各辺32768、
図bitmapは各辺8192・合計4M画素で制限する。既存画像decoderの長辺2048への縮小は維持し、
共通bitmap ownerだけは図の縦長出力を扱えるよう各辺8192・総64MiBの検証へ広げる。
flowの局所direction変更、ノード集合省略等の未対応構文は利用案内へ明示し、部分描画の成功にはしない。

## C-013の限定拡張

ADR0085の単一画像workerへ、コピーしたMermaid原文の解析・有限な配置・文字計測・bitmap生成を追加する。
処理量は原文bytes・ノード/辺/イベント・配置反復・bitmap寸法/総bytesで制限する。
UI threadへ長さ依存の図処理を置かず、通常メッセージを処理できることをnative検証する。
workerを増やさず、入れ子message pumpも作らない。取消と終了回収は既存adapterの同じ所有境界を使う。
この許可は任意のapplication処理やファイル処理の背景化へ広げない。

## 参考実装とライセンス

- Mermaid mermaid@12.2.0、commit 1fb7869866dd619168d840291c9c54b4f129758b。
  flowchart/sequence grammarとモデル分離を確認する。MIT、Knut Sveidqvist。
- Obsidian Pele、commit 3a2f075ee0c3d914f0fad3545fc0f193693975ac。
  DOMを要しない解析/配置/描画分離、有限回の階層配置を参考にする。MIT、kepano。

取り込んだ/翻案した部分の出典をソースと第三者通知へ記録し、必要な著作権/許諾文を配布物へ同梱する。
上流の実行時依存を追加するものではない。単純なC化だけで高速化を達成したとは主張しない。

## 検証

構文の正例/誤入力/未対応、方向/辺の対応/配置の有限性、原文保持、上限/確保失敗をcoreで固定する。
applicationの要求取消と古い結果拒否、nativeの画像混在/文字幅/テーマ/倍率/読取位置/進捗消去を対象検証する。
代表例の解析・配置・描画・cold/warmの測定範囲を区別して記録する。Mermaid.jsとの比較は同じ入力と
測定境界を明記し、異なる出力方式の数字を同一条件とは扱わない。
最終headの必須CI成功を統合条件とする。成功済みの不変入力は再実行しない。

追加waiver: none。既存WVR-0001はWinHTTP callbackだけへ継続する。

## 22:30の追加指定 — 部品と文字組み

hideの指定で背景Claudeがdesign:design-critique / design:design-system / design:design-handoffを使い、
[実canvas](https://claude.ai/artifact/CxHhi1aBQuF2WGa9PAxnbe)で現行A・推奨B・広めCを比較した。
部品/文字/padding、明暗、本文幅600/420の比較画像を確認し、Bを採用する（製品の設計変更前）。
原本は`D:/NeNeFolio/design/539-native-mermaid/`。canvasの手配置を自動layoutの結果とは扱わない。

- 基本文字14 logical px、辺ラベルと枠見出し13px、折返し幅200px。各役割で同じ実fontを計測/描画する。
  fontの実行高を優先し、canvasの21/19pxを強制して文字を切らない。文字列の共用が異なる役割の計測を混同しないよう、
  不変metricsへ両役割の寸法を持たせ、sceneが文字の役割を選ぶ。
- 意味を運ぶ線と背景線を区別する。背景線はpaneへ図の文字色を10/16混ぜ、全ての図の塗りで3:1以上を要求する。
  不足時は文字色へ戻す。文字と1/16・2/16・3/16の塗りのcontrast規則は維持する。
- nodeの基本paddingは左右16/上下12、最小64×40。rounded半径6、group半径8。
  circleは実測文字の対角に安全余白を足し、cylinderは文字を楕円から離れた胴へ置く。
  追加相談でcylinderのcap深さを`min(高さ/6, 11)`へ制限し、多行でも胴の上下7px以上を保つ。
  subgraphは左右/下16、上は8+実測見出し+8とし、方向変換後も見出しが物理的な上側に残る。
- flowの基本rank間隔56、外余白16。sequenceの文字とmessage線は4、線から次の文字まで24、
  noteは左右12/上下8、枠タブは左右8/上下4。actorは28×48と名前まで8、activationの入れ子間隔6。
- 通常矢頭10×10、太線矢頭12×12、線幅1/3、破線6/4。寸法はcoreのstyle定数から導出し、
  GDIのdevice座標へ渡す箇所だけで2倍する。計測/配置/描画へ別々の値を書かない。

取り込み後に実bitmapの多行ラベル・形状・明暗と対象境界を確認し、同じ入力のRelease生成時間を再測定する。

## 2026-11-04 — 斜辺付きノードの文字領域（#553、製品編集前）

実fontのbitmapでasymmetricの切込みが1行の文字に重なり、複数行ではhexagon・parallelogram・
trapezoidの斜辺から文字がはみ出すことを確認した。実native描画pathと文字矩形に1pxの余裕を
足した領域の差分は、1行/8行・明暗の24条件中18条件で空にならなかった。
1px余白だけの不足と、実際の文字領域のはみ出しは区別して品質記録へ残す。

斜辺の張出し比率1/7は維持し、除数を既存coreの`diagram_style.h`で一度だけ定義する。
nativeの2か所の整数除算とcoreの幅予約が同じ定義を使う。描画のdevice座標での丸めは維持する。
幅Wの中央帯は少なくともW×(7−2)/7あるので、文字幅と左右16pxを含む幅Pに対して
`ceil(P×7/(7−2))`を予約する。従来のP+24より狭くはせず、最低64×40も維持する。
既存のgeometry→layout→scene→native描画へ収め、新しい状態ownerや配置経路は加えない。

輪郭の比率を変えて細く見せたり、文字を縮小/切捨てしたりせず、長い文字のnodeを広げる。
subroutine、stadium等の別形状はこの幅予約へ混ぜない。保存schema・版・公開関数の変更はない。
新しいstyle定数は既存の寸法ownerの中に置き、図形点列を試験側へ複写しない。
GDIのBeginPath/EndPath/PathToRegionで実描画pathを取得し、文字との包含を独立に検査する。

## 2026-11-04 — 横向きの辺と斜辺の接続（#554、製品編集前）

製品未変更の実font/native描画で、6形状・4方向・明暗の48条件を確認した。
順方向/戻り/自己辺の288端点中108端点で、1logical px近傍に輪郭の内側が無かった。
bitmapでもparallelogramの斜辺とasymmetricの左切込みから辺が離れることを確認した。

既存geometryへ、layoutが作った正の矩形の辺中央を実形状の接続点へ写す純粋関数を置く。
flow layoutは方向変換後の物理矩形・端点を渡し、同じpathの先頭/末尾だけを補正する。
上下の接続と、中央が矩形の外縁に達する別形状は従来の位置を保つ。
斜辺は#553の除数7から導き、端点の実際のy位置で線形補間する。奇数の高さでは中央が
半pixelずれるため、単に幅/14・幅/7を足すのではなく、両斜辺/左切込みの実位置へ合わせる。
積は64bit、最後に最も近いlogical pxへ丸め、nativeのdevice丸めとの差を1px以内で検査する。

公開するのは製品layoutから使う座標queryだけで、新たなowner・router・描画後補線は作らない。
描画の点列や輪郭比率、辺label/矢頭/線種/非表示辺、scene上限、保存schemaは変えない。
GDIの実描画pathを独立oracleにし、規則を複写した期待値だけで成功にしない。

## 2026-11-04 — sequenceメッセージ文字の背景（#565、製品編集前）

実font/GDIの明暗6組で、離れた参加者を結ぶmessageの文字領域を、中間参加者の生命線やactivationが
横切ることを確認した。同じsceneのmessage文字だけを描いた参照と比較し、18ラベル中12件で計2,212pxの差が出た。
隣接参加者と自己messageの対照6ラベルは差0だった。実bitmapでも複数行の文字と線の重なりを確認した。

既存sequence layoutが出すmessageの文字primitiveへ、flowの辺ラベルでも使う`DIAGRAM_INK_BACKGROUND`を指定する。
既存canvasの塗り→文字の経路で、文字の実測矩形内だけ生命線/activationを背景色で隠す。
新しい意匠・padding・座標・寸法・矢印・生命線・activationの配置は加えず、sceneの数や順序も維持する。
message以外の文字、保存schema、版、依存、公開API、上限、ゲートは変えない。
文字矩形の外の全pixelと隣接/自己message対照のbitmapを前後比較し、意図した範囲だけの変化を確認する。

## 2026-11-04 — flowの辺ラベルを保護する描画順（#567、製品編集前）

4方向・明暗24図の実font/GDIで、辺ごとにpath→labelを出すと、後続pathが先行labelを上書きすることを確認した。
48ラベル中16件、計4,950pxの差を実sceneの文字単独描画との比較で再現し、実bitmapも確認した。
単一辺の対照8ラベルと、後続pathが重ならない24ラベルは差0だった。

既存flow layoutで、pathを構築する時に計算済みの物理label座標を、同ownerの一時workspaceへ辺ごとに保持する。
すべての辺pathをemitした後、同じ座標・metrics・塗り・paddingで全labelをemitし、その後にnodeを描く。
workspaceの追加は上限1024辺の座標配列だけで、sceneの完成時に従来どおり破棄する。確保回数や公開APIは増やさない。
経路・接続点・矢頭・線種・非表示辺・node/group・文字の位置/寸法とprimitive数は維持する。
別router、経路の再計算、sceneの並替API、描画後の補線は加えず、schema/版/依存/上限/ゲートも変えない。
文字矩形外の全pixelと単一辺対照のbitmap、文字だけの参照画像の不変を確認する。

## 2026-11-04 — subgraph見出しと接続辺（#571、製品編集前）

実font/GDIの4入力×4方向×明暗で、48見出し中14件・計2186pxが後続の辺描画に上書きされた。
group背景と見出しを同時にemitしていたため、後から描く内外の辺が透過文字を横切っていた。
最初に全path後・辺label前へ移したところ、12件の線による差は解消したが、入れ子LRの2件は
辺labelの背景余白が46px重なって残った。既存flow ownerで背景と見出しのemitを分け、
全pathと辺labelの後・nodeの前に見出しを出す。辺labelの実文字矩形も同じfontで単独描画して確認する。
見出しの塗りを既存のgroup色とし、幅は既存metricsの実文字幅に合わせる。native描画が既に使う
実文字位置・幅・高さを維持し、空白の見出し帯全体を塗って辺を隠さない。
group/node/path/labelの配置や矢頭・primitive数・公開API・ownerは変更しない。
任意の見出しと辺label同士の衝突回避は別の性質であり、今回の線による上書きと混同しない。

## 2026-11-04 — 活性区間へ接続するメッセージ（#573、製品編集前）

実font/GDIの7入力・明暗で76端点中54件が、表示されるactivationの外縁へ届かず生命線の中心に留まった。
16段の自己messageでは戻り線が活性棒の中に埋まることもbitmapで確認した。棒の位置・深さ・期間は既存place_eventsが確定している。

既存sequence layout内で、その確定済みy/end/depthからactivationの描画矩形を作る関数を一度だけ定義し、棒のemitと接続判定で共有する。
矩形のxは生命線からの相対位置、yは配置済み位置とする。未閉鎖時のbottom、最小高8、開始/終了境界を含む既存の描画を維持する。
messageの送信yと到着y（自己messageでは異なる）で、同じ参加者の棒が重なる範囲の外側を接続先とする。
左右方向は参加者の並び、自己messageは右側を使う。該当する棒がなければ相対x=0で生命線へ接続する。
activationのstackを再実行せず、既存の確定情報を照合する。活性イベントがない図は照合を省く。

計算した送信/到着の相対xを同workspaceの上限2048件×2のint配列へ一度だけ保持し、幅予約とemitが同じ値を使う。
自己messageは両端の大きいxから既存の横幅40を取り、文字も同じ基準から従来の24を空ける。右幅へその相対xを加えて範囲外を防ぐ。
参加者間messageは端点移動で減る距離を既存widen_spanへ加え、実測文字幅+32の間隔を保つ。文字の中心は実際の両端の中心にする。
矢頭/線種、y、活性棒自体、見出し/枠/注記の寸法と期間、primitive数、公開API・owner・rendererは変えない。
横方向の配置と文字位置は必要な幅に従って変わる。無活性対照の全bitmap不変を確認する。

照合は既存上限内のイベント走査であり、最大で各messageの2端点×イベント数に制限される。実測文字計測/配置の同じ処理段階へ閉じる。
固定Releaseの旧/現行で、通常の活性と多イベントの対象入力について生成時間を確認し、範囲・全sample・負荷を残す。
第二のactivation状態機械、UI処理、新しいworker、時刻参照、schema/版/依存/ゲートの変更は加えない。
## 2026-11-04 — 非表示辺のラベルを残す（#577、製品編集前）

6入力×4方向×明暗のnative描画で、モデルにある辺ラベル56件中、非表示辺に付いた40件がsceneへ渡らず欠落した。
線だけを隠す構文として受理しているため、文字も黙って省く現状を修正する。上流12.2.0の固定grammarは
非表示LINKにもpipe文字を許し、edge label生成と線のinvisible thicknessを別に扱っている。
参照原本は`D:/NeNeFolio/agents/invisible-labels-20261104/upstream-references.json`。

既存flow layout内で、見える線または実測幅を持つ文字がある辺を同じ経路へ配置する。
forwardの段間予約、戻り/自己/並行辺の外側channel予約、ラベル座標の計算をその条件で揃える。
線の非表示はpath primitiveをemitする直前に適用し、ラベルは通常辺と同じ文字passで出す。
ラベルのない非表示辺は従来どおり配置を拘束するだけで、path/文字を出さない。
既存のmeasure/route/scene/native経路と一時workspaceを使い、第二のrouterや描画後の補足は加えない。

非表示辺のラベルを置くために必要な図の幅/段間とラベルprimitive数は増え得る。
線/矢頭を表示したりラベルを縮小したりせず、通常辺だけと無ラベル非表示辺だけの対照画像は不変を確認する。
API/schema/版/依存/上限/gateは変更しない。既存modelを基準に各ラベルのsceneへの到達と実文字の非上書きを検査する。

## 2026-11-04 — 破線から独立した端点記号の輪郭（#587、製品編集前）

実bitmapで開矢頭と×印の切れ目を確認し、4方向・両端・明暗のnative比較でも、同じ座標の実線と
破線でshaft外の記号画素が異なることを再現した。線本体のdash/gap判定は成立している。
参照版Mermaid 12.2.0のcrossheadも破線を解除している。取得原本は
`D:/NeNeFolio/agents/diagram-marker-observe-20261104/upstream-references.json`。

既存canvasのpen生成を色と線種で受け、破線pathを描いた後、その端点記号に限って同色・同幅の
実線penを選ぶ。記号の形・位置・寸法・塗りは同じnative_shape関数を使い、第二の描画器を作らない。
記号のないpathと実線/太線は追加penを作らず、現行の描画を維持する。追加penは局所所有し、
選択状態を復元して破棄する。確保失敗はIMAGE_OUT_OF_MEMORYで返し、途中bitmapを公開しない。

scene・layout・構文・文字・配色・公開API・schema・版・依存・上限・gateは変えない。
実線/太線/無記号の全画像不変、変更画素の端点範囲、線本体のdash/gap、GDI資源数の復帰を確認する。
