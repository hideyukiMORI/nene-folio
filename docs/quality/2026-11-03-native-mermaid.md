# 本文表示後に生成するnative Mermaid — #539

## 変更と契約

Markdownの`mermaid` fenceを、本文表示後にフローチャート/シーケンス図へ変換する。
既存block parserで原文を収集し、画像と同じworker・ticket・取消・cache・進捗通知・RichEdit差替えを使う。
Cのparser/不変model/実フォント計測/有限layout/GDI描画が唯一の経路で、JavaScript実行時依存を増やさない。
原文、保存schema、製品版1.2.0は維持する。利用可能な構文と上限は[利用案内](../MARKDOWN.md)、
所有境界と判断は[ADR0089](../adr/0089-native-lazy-mermaid.md)を参照する。
今回の実装は、既に準備したStore提出候補やGitHub公開版を更新しない。

規則: ARC-001〜012、C-001〜018、QLT-008/009/011/012/013、GIT-001〜004。
追加waiverはnone。既存WVR-0001はWinHTTP callbackの範囲で継続する。

## 対象検証の原本

`tools/record-run.py`で入力snapshot、raw stdout/stderr、子/記録側終了値、生成exeを記録する。
一時原本は`D:/NeNeFolio/outputs/539-lazy-mermaid/`、恒久記録は
`D:/NeNeFolio/agents/mermaid-20261103/`。以下の成功runは子/記録側終了値0、recordingErrorsなし。
最終headの必須CI・main・通常反映・data照合・保全整理は、完了時のintegration-completion.jsonで確定する。

- build-03/05/06/07: 固定toolchainのDebug対象build。parser/layoutと実アプリ、対象nativeテストを生成。
- core-03: `build/folio_tests.exe --mermaid`。構文・方向・形状・ラベル・戻り辺・自己辺・入れ子枠、
  原文/画像混在、実効配色、テーマ/言語/ノート/編集変更時の取消と要求dispatchを確認。
- build-08 / diagram-bounds-01: 追加した辺/イベント/入れ子/group/ラベル/表示slot/原文bytesの上限を
  `build/folio_tests.exe --diagram`で確認。過大なfenceは本文を止めず、図slotへ型付き失敗を残す。
- native-render-02: 実Noto Sans JPとGDIで明暗のflow/sequenceを生成。3処理段階、各段階の取消、
  workerへの原文/書体/配色コピー、cacheの完全一致と各入力変更による失効を61確認。
- native-display-01: 画像1件と図2件の混在を実RichEditへ挿入。選択/読取位置/Undo/dirty、
  幅と倍率の再配置、ミニマップ、編集原文を194確認。
- native-status-01: `image_status_tests.exe --mermaid`。本文先行、3段階の日本語通知、編集による取消、
  再要求、完了後の通知消去と常設バーを31確認。
- release-build-01: CMake File APIに基づくconformanceと固定Release buildが成功。
- allocation-target-01: canonical測定buildの確保失敗注入付き`folio_coverage.exe --mermaid`が成功。
  parser/layoutの各確保失敗と変更したMarkdown/application境界を確認。stderrにASanの
  `interception_win: unhandled instruction`通知があり、空stderrや完全なinterceptionを主張しない。
- allocation-target-02: `folio_coverage.exe --diagram`。fenceの通常終端・container終端・EOFでの
  原文収集の全確保失敗を追加。閉じ処理の失敗が次の本文で上書きされる問題を修正し、失敗の保持を確認した。
  同じASan interception通知がある。Debugの製品buildには確保失敗注入を入れない。
- build-11 / diagram-design-01 / native-render-03: B案反映後の固定Debug build、`folio_tests.exe --diagram`、
  `diagram_render_tests.exe <新規出力先>`が成功。全方向のgroup見出し余白、共用文字列の14/13px役割、
  多行円柱と円の余白、全themeの補助線contrastを確認。nativeは82確認、stderrなし。
- release-build-02 / native-release-01: B案の固定Release buildと同じnative 82確認が成功。
  明暗のflow/sequence/多行部品6枚のBMPは、検査済みDebug出力とSHA256がすべて一致した。
- allocation-target-03: B案反映後の確保失敗注入付き`folio_coverage.exe --diagram`が成功。
  同じ原文収集・解析・役割別metrics・配置の全確保点を対象にし、無関係な製品テストは実行していない。
- 初回CI（run 41653947418）は全16テスト成功後、分岐網羅率88.85%が90%未満として失敗した。
  統合ツールが始めた同一headの再試行41654913727は取消し、PRをDraftへ戻して不足条件を追補した。
- contracts-target-02 / allocation-target-04: `folio_tests.exe --diagram`と確保失敗注入付き同じ対象が成功。
  公開後モデル/sceneの不変性、不正な参照/寸法/範囲外座標、文字表/primitive上限、低contrast配色のfallback、
  全描画色によるcache失効、comment/引用の区別、空group、誤ったsequence枠/note/番号指定を確認する。
  対象16翻訳単位は1433/1578分岐を通過した。これは対象測定であり、全体ゲート成功とは扱わない。
  `contracts-target-01`は有効な末尾ハイフン付きIDを誤入力としたfixtureで失敗したため、
  IDと辺を空白で分けて意図した不正な辺を検証した。製品コードと閾値はこの追補で変更していない。
  確保失敗測定には従来と同じASan interception通知がある。最終headのCIで全体の90%を再確認する。

build-01はtarget名、build-02は9関数の複雑度、build-04はnativeテストのCOM宣言不足で失敗した。
target名修正、意味を分けたhelper化、必要header追加で解消し、ゲートを弱めていない。
conformance-01のFile API reply不足はrelease-build-01のquery生成と再configureで解消した。
設計反映時のbuild-09は描画関数の引数数規約で失敗し、既存scene primitiveを渡す責務へ整理した。
この回はartifact指定先にも誤りがあり、記録側終了値125で成功扱いにしない。
build-10は新しい公開宣言に必要なHDCのnullability注釈を追加し、build-11で解消した。
verify-final-01は中核の未宣言symbol `strchr`/`strstr`を検出した。文字集合の判定を既存の
diagram_textへ集め、既知の矢印tokenの破線判定を添字で行う。依存許可表は変更しない。
この修正後の対象build・構文/表示検証・規約/symbol検査の結果は、恒久記録のparser-final.jsonを参照する。
独立した読み取り専用レビューの4指摘（矢印の意味、横向きgroup見出し、重複辺ラベル、自己activation到着点）を
修正し、対応する回帰テストと再レビューで解消を確認した。

## 外観と実機

初回bitmapで明色の文字contrast不足を発見した。本文背景とlink色から塗りを導出し、全themeで文字4.5:1、
node枠3:1を保つ検証を追加した。修正後の明暗flow/sequence実bitmapを確認した。
専用dataのRelease候補で本文先行→日本語flow/sequence、scroll、完了後の常設バー、未対応pieの通知、
編集画面の原文を実際に確認した。22:33にhideのEscで画面操作が停止した後は、追加の画面操作を行わない。
通常版のdataを実機検証へ流用していない。

22:30のhide指定を受け、背景Claude（既定Claude Fable 5.1）でdesign:design-critique、design:design-system、
design:design-handoffを実際にinvokeし、[canvas](https://claude.ai/artifact/CxHhi1aBQuF2WGa9PAxnbe)のB案を採用した。
デザイン席は比較・寸法表・引き継ぎを担当し、製品実装/検証は主担当が行った。
原本・全canvasソース・実preview・tokens.jsonは`D:/NeNeFolio/design/539-native-mermaid/`。
14pxの基本文字、辺/枠見出し13px、折返し200、補助線の10/16混色、形状ごとのpaddingと矢頭、
subgraphの方向に依存しない上余白、sequenceの文字と線4pxを取り込んだ。
多行cylinderの胴が狭くなる式を追加相談し、cap深さの最大11pxを採用した。
寸法はdiagram_style.h、文字の役割別実測はdiagram_metrics、配置は既存の二つのlayout、描画は同じGDI adapterが持つ。

実Noto Sans JPの行高はbody21/caption19、長い日本語の計測は196×41となり、200px内で折り返した。
実bitmapで明暗の多行円柱/円/diamond、補助文字、矢頭と枠を確認した。
canvasのB/Cは手配置であり、その284px/480px等の図全体寸法を自動配置の達成値として扱わない。

## 性能の測定境界

同じ3入力を固定Mermaid 12.2.0と比較した。nativeはReleaseの解析/font生成/実測/layout/GDI 2倍描画/
平均縮小/BMP所有まで、JavaScriptは`mermaid.render`とDOM `getBBox`までで、SVGのraster化を含まない。
process/browser起動、script読込、font登録/読込、worker queue、UI timerは両方の数字から除外する。
同じ同梱Noto Sans JP regularを使い、各入力の初回を分けた後、cacheなし20回の中央値を採る。
別途、同一bitmap cacheコピーの20回中央値を測った。Windows 11 build 26300、Chrome 156.0.8078.79。

Claude設計前は`benchmark-before-design.json`、採用後は`benchmark-after-design.json`を原本とする。
採用後はnativeがbody14/caption13px、Mermaid.jsはbase14px。JS側の入力/configは不変なのでjs-03の結果を再利用した。

| 入力 | native初回ms | native中央値ms | cacheコピー中央値ms | Mermaid.js SVG中央値ms |
| --- | ---: | ---: | ---: | ---: |
| flow-small.mmd | 11.778 | 2.410 | 0.032 | 40.25 |
| flow-group.mmd | 5.558 | 5.613 | 0.276 | 52.05 |
| sequence.mmd | 11.706 | 5.574 | 0.226 | 23.85 |

nativeの出力寸法は309×541、1228×423、520×724。設計前の中央値2.836/6.407/7.120msに対して
今回の3例では生成時間が増えなかった。配置、役割別文字サイズ、出力方式/寸法が異なるため、
同一画像の比較や一般的な速度倍率とは扱わない。worker待ち時間とUI通知周期は表に含まない。
Release測定のlink-02/native-release-02は子/記録側終了値0、recordingErrorsなし、stderrなし。

## 残る条件と限界

最終headの必須CI・main統合・通常反映・保全整理の確定状態は、上記integration-completion.jsonを参照する。
flowchart/sequenceの対応表にない記法は明示的な未対応とし、Mermaid全体との完全互換を主張しない。
bitmapの幅縮小で大きな図の文字も縮小する。全OS/DPI/書体/任意の大規模グラフの視認性を保証する検証ではない。
groupの帯と外側経路によって、単純な手配置より横幅が大きくなる場合がある。今回canvasの配置を別経路として移植しない。
独立レビューは静的な読み取りであり、実行検証の代わりとして扱わない。
