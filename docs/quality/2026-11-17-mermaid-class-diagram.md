# Mermaidのクラス図の品質記録 — #890

2026-11-17。開始main `9504488`（#888統合後）。先行ADR0107。
作業木E:/NeNeFolio/worktrees/890-class-diagram（branch feat/890-class-diagram）、対象実行の原本E:/NeNeFolio/outputs/890-class-diagram、画像D:/NeNeFolio/agents/class-diagram-20261117。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 試行

| 試行 | 結果 |
| --- | --- |
| build-01 | 失敗: clang-tidyの認知的複雑度（`mermaid_class.c`の`relation`が13）。線と印の読み取りを`line_between`へ分けた |
| build-02 | 失敗: 同じ検査（adapters/win32の`diagram_native_shape_arrows`が14）。端の三角を作る所と、白抜きの印を描く所を分けた |
| build-03 / unit-01 | 成功。模型・13通りの関係・`--order`の区別・20通りの拒否・上限 |
| 描画1 | 563 checks成功。目で見て2点: ひし形が細すぎる（幅が矢印の半分）。同じクラスから出るコンポジションと集約の印が同じ点に重なり、塗りのひし形が白抜きに隠れて集約に見える |
| ひし形 | 幅を矢印と同じ、長さを2倍にした |
| 描画2 | 失敗（手順）: 同じ出力先へ取り直すと、描画の試験が「new bitmap artifact」で止まった（既存の画像を上書きしない作り）。出力先を新しいフォルダにして取り直した |
| 印の並び | 同じクラスの同じ辺にUMLの印（三角・ひし形）が複数付くとき、配置が印を横に並べるようにした（`marker_shift`）。フローチャートの既存の矢印は1点のまま。単体で、並ぶこと・ふつうの矢印は並ばないことを確かめた |
| 描画3 | 563 checks成功。`visual-03/class-{light,dark}.png`で、塗りのひし形・白抜きのひし形・2つの白抜きの三角・開いた矢印・破線を明暗で目で確かめた |
| 見た目で残る点 | 関係のラベルが、短い折れ線の角に重なることがある（フローチャートのラベルの置き方の既存の性質）。属性と操作の間の区切り線は無い（ADR0107の却下した案） |

## 最終の対象検証

原本は`v1-*.log`（`E:/NeNeFolio/temp/890/verify.ps1 -Tag v1`）。build（clang-tidy込み）・`eng/coverage.py`（分岐90.72%、閾値・除外不変。`mermaid_class.c` 84.18%、下限は全体）・
`eng/symbols.py`・`eng/conformance.py`（source/`--build-dir`）・`git diff --check`が成功し、`ctest`は62件すべて成功（135.0秒）。
