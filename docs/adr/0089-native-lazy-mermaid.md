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
