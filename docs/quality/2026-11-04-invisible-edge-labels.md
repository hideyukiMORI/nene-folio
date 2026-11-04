# 非表示辺のラベルを残す — #577

`A ~~~|文字| B`は受理されmodelにラベルが残るが、既存flow layoutが線と同時に文字も省いていた。
6入力×4方向×明暗の48図で、辺ラベル56件中、非表示辺の40件がsceneへ渡らないことを再現した。
同じlayoutの幅/段間予約とrouteを通してラベル座標を求め、pathのemitだけを省く修正で、欠落/上書きは0になった。

## 根拠と正典経路

最初の合成入力は`flowchart TB / A[Start] ~~~|VISIBLE LABEL| B[End]`。
#573固定Releaseで明暗ともIMAGE_READY・257×234だったが、文字は描かれていなかった。
この初期観測は`D:/NeNeFolio/outputs/invisible-labels-20261104/observe-01`、原文と上流参照は同名agentsに保存した。
参照版12.2.0の[flow grammar](https://github.com/mermaid-js/mermaid/blob/1fb7869866dd619168d840291c9c54b4f129758b/packages/mermaid/src/diagrams/flowchart/parser/flow.jison)は、
非表示のLINKにもpipeラベルを許す。[edge renderer](https://github.com/mermaid-js/mermaid/blob/1fb7869866dd619168d840291c9c54b4f129758b/packages/mermaid/src/rendering-util/rendering-elements/edges.js)では
ラベル生成と線のinvisible thicknessの扱いが分かれている。これはsource読取であり、今回Mermaid.jsを実行した比較ではない。
webのedges raw取得はcache missで失敗し、同じ固定URLをInvoke-WebRequestで取得して本文を確認した。

既存diagram_flow_layoutのemit_link/emit_link_labelがstrokeだけで早期returnすることを確認した。
製品編集前にADR0089へ、線の非表示と文字の保持を同じowner内で分ける判断を追記した。
作業木はmain`6deb9b6`へ同期後に試験を追加し、対象成功後の`6e11fb9`取込は5文書だけで製品/test入力は不変。

## 実装と自己レビュー

- `has_edge_content`が「見える線、またはcaptionの実測幅がある文字」を一度だけ定義する。
- forwardのtrack予約、戻り/自己/並行辺のchannel予約、route計算は同じ条件を使う。
- routeから得たlabel座標を既存workspaceへ保存し、非表示ならpath emit前にreturnする。
- label passは線種に関係なく、従来と同じ実測幅/背景/paddingで文字を出す。

線/矢頭のprimitiveを非表示に保ち、無ラベル非表示辺は従来の配置制約だけを維持する。
通常の辺は既存の順序/接続点/幅予約/文字位置を保つ。ラベルを持つ非表示辺だけ、必要な段間/外側channelと文字primitiveが増える。
最大1024辺の既存配列と既存の有界loop内に閉じ、owner、公開API、追加確保、第二のrenderer/routerを作らない。
sceneの上限判定とnativeの寸法/画素上限は維持する。保存schema/版/依存/gateを変更しない。
ARC-001/003/004/008/012、C-002/008/011/012/014/016、QLT-008/010/012/013を対象に自己レビューした。
既存owner内の小さな条件修正であり、追加の独立reviewは行っていない。

## 対象試験

nativeの`--invisible-labels`を追加し、通常CIの既定実行にも含めた。
6入力は、複数行forward、通常辺と戻りの非表示辺、非表示の自己辺、非表示2本と通常1本の並行辺、
無ラベル非表示辺、通常のラベル付き辺。各TB/BT/LR/RL×明暗で48bitmapを保存する。

期待値はmodelに残るラベルを基準に、対応するsceneの文字primitiveが1つあることを要求する。
実GDIで生成した全図と、sceneの文字だけを同じcanvasへ描いた参照の文字矩形内pixelを比較する。
sceneのpath数は可視線のmodel数と一致し、DIAGRAM_INVISIBLEのpath primitiveを出していないことも確認する。
6入力中のラベルは一意で、同じlabel値を持つ複数辺一般の重複検査へは拡張しない。

| run | コマンドと結果 |
| --- | --- |
| build-red-01 | 固定Debugのbuild-target.ps1でdiagram_render_tests/folio_testsをbuild。子/記録側0 |
| red-01 | `diagram_render_tests.exe --invisible-labels <red-bitmaps>`。48画像保存後、40ラベル欠落で子/記録側1。既存文字の上書き0 |
| build-green-01 | 同じbuild-target.ps1。変更製品だけを再build/link、子/記録側0 |
| native-green-01 | 同じ対象selectorで48画像。1112checks、全56ラベル欠落0/上書き0、子/記録側0 |
| core-green-01 | `folio_tests.exe --diagram`。既存の図解析/配置/deferred-media対象成功、子/記録側0 |
| verify-01 | verify-target.ps1。clang-format、conformance0、symbols2library/違反0、git diff --check成功。子/記録側0 |

全runをtools/record-run.pyで入力snapshot・raw stdout/stderr・終了値とともに記録した。recordingErrorsは全空。
成功runのstderrはverify-01のCRLF→LF案内だけ。他の成功runとredの診断以外にstderrはない。
修正前2枚、修正後のforward/並行/戻り/自己の4枚を視認した。
compare-bitmaps.pyで全48組をhash照合し、無ラベル非表示辺/通常辺の対照16枚は全一致、修正対象32枚だけ変化した。
座標が変わる修正なので全画像の同寸pixel差分不変は要求していない。

## 文書・証跡・限界

製品1file、native試験1file、ADR/品質記録/gate-proofs/日報/引き継ぎ/currentの6文書を変更。
恒久原本は`D:/NeNeFolio/agents/577-invisible-edge-labels`。初期observeの別出力も同じ保全ZIPへ含め、全hash照合後に整理する。
GIT-001〜004、QLT-008/012に従い、成功済みの不変入力はpush/統合だけの理由で再実行しない。
最終headの必須CIと正典統合、通常exe反映、data内容/hash/作成更新日時照合、保全整理の確定値は
`D:/NeNeFolio/agents/577-invisible-edge-labels/integration-completion.json`へ収載する。
追加waiverはnone、既存WVR-0001はWinHTTP callbackだけで継続。
任意図の文字同士の衝突、全書体/DPI、実UIでの受理は未保証。今回の追加ラベルの性能は測定しておらず、速度向上を主張しない。
実app/実ノートを試験へ使わず、Store再提出/公開は行わない。
