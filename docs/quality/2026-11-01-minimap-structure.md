# ミニマップの余白と文書構造 — #506

日付: 2026-11-01。Issue [#506](https://github.com/hideyukiMORI/nene-folio/issues/506)。
設定直後の応答停止は#504で修正済み。ここではhideの右余白・h1〜4/hr/リスト/表の依頼だけを扱う。

## 変更と正典経路

- `folio_window`の共通矩形で、表示中の右余白を56から12 logical px、本文との間隔を12から8へ縮小。
  非表示時の56、幅64、本文最小幅320を保持する。描画/hit/UIAは同じ矩形を使う。
- `note_pane`所有の既存ITextDocumentから独立rangeを一つ作り、`note_pane_sample`が最大16表示行の
  先頭1 UTF-16単位、字体・サイズ・色・段落字下げ・前余白を読む。選択は変更しない。
- h1〜4はnativeの字体/サイズ/色/前余白から線の太さ・長さを描く。水平線はnativeの小さい空段落と背景、
  リストはhanging indentと段落先頭位置、表はnative U+FFF9と既存`note_pane_table`の行取得を使う。
  折り返しの継続行にはリストの印を重ねない。表は限られた幅で識別できる列数へ格子を縮約する。
- `note_minimap`は最大256個のscalar sampleを完成時だけ公開。16/idle・120ms・非表示/IME/modal guardと
  失敗時の有限終了を維持し、本文・AST・RTFの複製、第二parser、背景thread、COM参照のcacheを作らない。
  高コントラストと現在範囲は実効paletteで描く。編集はnativeの平文表示に従う。

規則: ARC-001/002/004/007/008/009/011/012、C-002/008/012/013/016/017/018、
QLT-008/012/013、GIT-001〜004。ADR0084を補正。保存schema・版・依存・申請済みMSIXは不変。Waivers none。

## 実行した対象検証

全原本は`D:/NeNeFolio/agents/minimap-shape-20261101/`、一時出力は同名`outputs/`。
`tools/record-run.py`の入力snapshot・raw出力・終了値を保持する。

| 実行 | 結果と範囲 |
| --- | --- |
| `build.ps1` (`debug-07`) | conformance、正典Debug configure/build、clang-tidy成功。ASan/UBSan/nullabilityの正典vector |
| `contract.ps1` (`contract-05`) | CTest `minimap_structure`、実RichEditの81checks成功。h1〜4、hr、5リスト、3表行、折り返し、16上限、範囲外拒否、編集のnative平文、本文/選択/scroll/Undo/dirty保持 |
| `paint.ps1 -Stage run -Label paint-behavior-02 -Case behavior` | 実folio_window 232checks成功。256sample/16slice、1slice一つのdocument取得、全文readなし、click/drag/keyboard、狭幅/非表示復帰、IME/modal、入力集約、dirty/Undo/本文保持、可変行高の末尾到達 |
| `paint.ps1 -Stage run -Label paint-03 -Case structure` | 実folio_window 43checks成功。右12/間隔8/非表示56、line/table取得失敗の有限終了と復帰、明暗/高コントラスト、狭幅、96/144/192 DPIでthumb矩形 |

主窓fixtureは製品`folio_window.c`と同じcompile/link vectorに結び、同じobjectの二重linkをmapで拒否した。
合成ノートだけを専用desktopで実行。`paint-03/minimap-{0,1,high,narrow}.png`を目視し、
見出しの段階、hr、リストの字下げ/印、表格子、余白と狭幅非表示を確認した。
描画は実行環境の120 DPI、96/144/192は矩形検証であり、物理モニター間移動ではない。

## 途中の検出と修正

- 最初のbuildで描画関数の認知的複雑度15を拒否。文字strokeと形の選択へ分け、閾値は変更しない。
  `debug-01`は終了前の入力修正も記録されており成功扱いしない。
- native契約はリスト印0で失敗。TOM StartOfの移動量には非空rangeの縮小も含まれるため、
  開始位置と取得後の段落開始位置を比較する形へ修正。折り返しで印が重複しない回帰を加えた。
- 画像fixtureの「ノート単独400pxなら非表示」は実際の最小窓幅700と余白縮小に合わず失敗。
  両ペイン700pxの実際に本文幅が足りない条件へ修正。製品の最小本文幅や非表示条件は弱めていない。
- TOM2 GetRowは表でもE_FAILだったため採用せず、既存のnative行取得を再利用した。

## 限界・完了証跡

最大256行の等間隔sampleなので、飛ばした見出しや長い表の全セル・全折り返しは再現しない。
文字やpixelの厳密な縮小ではなくnative表示の概形。問い合わせ数の上限はRichEdit内部処理の実時間保証ではない。
物理IME・reader・異なるDPI間の実画面呈示は今回の合成試験とは区別する。
最終headの必須CI `eng/check.ps1`、main、通常exe反映/起動、全hash保全と終了整理の確定値は
同agentsの`integration-completion.json`を参照する。起動中の通常版を強制終了しない。
