# stadiumの複数行ラベルと曲線の余白 — #551

2026-11-04、継続的な品質改善で、stadiumの寸法計算とnative RoundRectの曲線を照合した。
高さが増えてもラベルへの左右余白が28 logical pxのままなので、8行のラベルが輪郭を越える。
production未変更の実Noto Sans JPによるbitmapと、GDIの領域差分で再現した。

## 正典経路と判断

`diagram_geometry.c`の内部関数`stadium_width`が、従来の追加幅56とnode高さの大きい方を
ラベル幅へ加える。既存の`diagram_geometry_node`からだけ使い、高いラベルは左右の丸い部分を
文字の中央帯の外側へ確保する。短い1行の幅・高さ、最低64×40、上下12px、fontや折返しは維持する。
曲線と文字を別の場所で補正せず、同じgeometry→layout→scene→GDIの経路を使う。

半径から最小の余白を逆算する別の整数平方根処理は加えず、丸い両端を明確に予約する。
高いラベルでは図が横へ広がる。巨大な入力は従来と同じscene/bitmap上限へ到達し得る。
文字の縮小や切捨てで収めず、上限を超えた図は従来の型のある失敗にする。
公開API・保存schema・版・依存・上限は変更しない。CMakeは既に宣言された`win32_tests`の
`gdi32`を既存のnative試験targetへ結ぶだけで、製品のruntime依存を増やさない。

## 試行・入力・結果

main`7fef552`から`D:/NeNeFolio/worktrees/551-stadium-label`を作成した。
出力は同名の`D:/NeNeFolio/outputs/`、恒久原本は同名の`D:/NeNeFolio/agents/`。
`tools/record-run.py`で入力snapshot、子コマンド、生成exe、bitmap、raw出力と終了値を保存した。
固定toolchain、Debug/Ninja、clang-tidy、ASan/UBSan/nullabilityを使用した。

| 試行 | 内容 | 実結果 |
| --- | --- | --- |
| build-red-01 | production未変更、native再現試験追加 | 子/記録側0 |
| red-01 | `diagram_render_tests.exe --stadium <red-bitmaps>` | 8行が輪郭の内側からはみ出し、子/記録側1 |
| build-green-01 | 幅予約とcore境界を追加 | 複雑さ上限と試験のstdint include不足で1 |
| build-green-02 | 内部関数への分離とincludeを修正 | native試験の引数5個が上限4を越え、1 |
| build-green-03 | 既存request型を渡して引数数を修正 | 子/記録側0 |
| native-green-01 | `diagram_render_tests.exe --stadium <green-bitmaps>` | 82checks、子/記録側0 |
| core-green-01 | `folio_tests.exe --diagram` | 対象の解析・配置・遅延mediaが成功、子/記録側0 |

全runのrecordingErrorsは空。最終buildと対象試験のstderrも空。
build前に推測したLLVM既定pathが存在せず整形が未実行になった試行は、正典`eng/toolchain.ps1`を
読み込んで解消した。静的解析の閾値・抑制・除外は変更していない。

native試験は実fontの1行・8行・10行の細いラベルを明暗で描画する。既存の描画関数とfont登録を使い、
`--stadium`でこの境界だけを実行できる。通常CIも同じ関数を呼ぶ。
RoundRectの2倍rasterから枠線1 logical pxを除いた内側領域を作り、文字矩形との差を
`CombineRgn(..., RGN_DIFF)`で求める。修正前後ともbitmapをassertより先に保存する。

| 実fontの入力 | 文字寸法 | 修正後node寸法 | 内側との差 |
| --- | --- | --- | --- |
| 日本語14字×1行 | 196×21 | 252×45（従来寸法） | 空 |
| 日本語14字×8行 | 196×164 | 384×188 | 空 |
| 日本語1字×10行 | 14×205 | 243×229 | 空 |

8行の修正前nodeは252×188、文字矩形は`(28,12,196,164)`だった。
2倍rasterでの差分bboxは`(56,24,448,352)`、領域種別3（COMPLEXREGION）。
これは外側画素数ではなく、差分領域の外接矩形と種別である。
修正後の全6条件は差分bbox`(0,0,0,0)`、種別1（NULLREGION）。
保存した修正前1枚と修正後6枚を視認し、はみ出しの解消、短い1行の外観、明暗の可読性を確認した。

coreは幅0/1/196/200/4096、高さ1/18/32/33/42/164/4096の35組を確認する。
最低寸法と円弧の両端、奇数寸法と幅拡張の境界、受け入れるmetricsの上端を含む。
ラベル四隅の円弧中心からの距離を2倍整数座標で調べ、幅計算の式を期待値へ複写しない。
短い1行の既存252×42（固定metrics）も維持する。

## 規則・検証範囲

ARC-001/003/004/008/010/012、C-002/005/008/011/012/014/016、
QLT-008/009/010/012/013、GIT-001〜004、ADR0089。
実装と自己レビュー・対象実行は主担当が単体で行った。追加の独立レビューは使っていない。
実appや実ノートの操作・全図形の再設計・別図種の追加は行っていない。
このnative描画確認を、hideによる通常版の実機受理や全DPIの検証とは扱わない。

整形・conformance・symbols・diff確認は`verify-01`、最終headの必須CIを統合条件とする。
最終head/CI/main/通常反映/data照合/保全整理の確定原本は
`D:/NeNeFolio/agents/551-stadium-label/integration-completion.json`へ収載する。
追加waiverはnone、既存WVR-0001はWinHTTP callbackだけへ継続する。

GDIの領域演算はMicrosoftの
[CreateRoundRectRgn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createroundrectrgn)と
[CombineRgn](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-combinergn)に従った。
