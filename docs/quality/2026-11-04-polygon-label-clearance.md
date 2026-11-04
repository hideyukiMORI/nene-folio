# 斜辺付きノードのラベルと中央の余白 — #553

2026-11-04、継続的な品質改善で、nativeの斜辺が幅に比例するのに文字の左右余白は固定であることを
確認した。実fontのbitmapではasymmetricの切込みが1行の文字へ重なり、複数行ではhexagonや
parallelogram、trapezoidの斜辺から文字がはみ出していた。

## 正典経路と変更

製品編集前に[ADR0089](../adr/0089-native-lazy-mermaid.md)へ判断を追記した。
斜辺の比率1/7を維持し、`diagram_style.h`の新しい`diagram_polygon_slope_divisor`を
native描画の2か所とcoreの幅予約で共用する。描画時のdevice座標の整数除算も維持する。

`diagram_geometry.c`の内部`sloped_width`は、左右16pxを含む文字幅Pに対して
中央帯がP以上となる幅`ceil(P×7/(7−2))`を予約する。従来のP+24より狭くせず、最小64×40を保つ。
対象はhexagon、左右parallelogram、上下trapezoid、asymmetricの6形状だけ。
subroutineは従来の追加幅へ分離し、stadium等へ新しい幅計算を広げない。

輪郭比率の変更、文字の縮小/切捨て、描画後の位置補正や別routerは追加しない。
既存geometry→layout→scene→native描画を使い、状態owner・公開関数・保存schema・版・依存・上限は不変。
新しいstyle定数は既存の寸法ownerに属する。長いラベルではnode幅が増える。

## 試行・入力・結果

作業木は`D:/NeNeFolio/worktrees/553-polygon-label`、出力は同名の`outputs/`、恒久先は同名の`agents/`。
最新main`fd793da`を取り込んでからnative試験だけを追加し、production未変更で再現した。
`tools/record-run.py`により入力snapshot・子argv・実行exe・同梱font・bitmap・raw出力・終了値を保存した。
固定toolchainのDebug/Ninja、clang-tidy、ASan/UBSan/nullabilityを使用した。

| 試行 | コマンド・内容 | 実結果 |
| --- | --- | --- |
| build-red-01 | `cmake --build build --target diagram_render_tests folio_tests --parallel 2`、製品未変更 | 子/記録側0 |
| red-01 | `diagram_render_tests.exe --polygons <red-bitmaps>` | 24条件中18条件が余白不足、子/記録側1 |
| stadium-control-01 | `diagram_render_tests.exe --stadium <stadium-control-bitmaps>` | 6条件94checks、子/記録側0 |
| build-green-01 | 斜辺定義と幅予約、core境界を追加した最終入力 | 子/記録側0 |
| native-green-01 | `diagram_render_tests.exe --polygons <green-bitmaps>` | 24条件476checks、子/記録側0 |
| core-green-01 | `folio_tests.exe --diagram` | 関連の解析・配置・遅延media対象成功、子/記録側0 |

全runのrecordingErrorsは空。対象build/試験のstderrも空。
実行前の入力保存と固定toolchainの初期化に時間がかかった。stdout/stderr未生成時の読み取り失敗は
検証の失敗/成功に数えず、終端とresultで判定した。推測した不存在の`diagram_path.c`とWindowsでの
globパス引数による検索も失敗として記録し、実在するflow layout/native shapeの読取で確認した。

native試験は既存`diagram_native_shape_draw`をBeginPath/EndPath内で呼び、PathToRegionで実際の輪郭を
領域へ変換する。図形の点列は試験へ複写しない。実fontで計測した文字矩形を1 logical pxずつ広げ、
RGN_DIFFが空かを確認する。これは線幅のための余裕を含む判定で、glyphの外側画素数の測定ではない。
各bitmapは判定前に保存し、全24条件の差分を出してから最後に成功/失敗を確定する。

日本語14字の1行は196×21、8行は196×164 logical px。形状の添字0〜5は下表の順、themeの0/1は明/暗。
bitmap名は`polygon-<形状添字>-<行数>-<theme>.bmp`で、元の12原文は恒久先の`.mmd`へ保存した。

| 形状 | 修正前1行の余白判定（明暗共通） | 修正前8行 | 修正後1行/8行 |
| --- | --- | --- | --- |
| hexagon | 空 | 不足 | 空 |
| parallelogram | 1 device pxの不足 | 不足 | 空 |
| reverse parallelogram | 空 | 不足 | 空 |
| trapezoid | 1 device pxの不足 | 不足 | 空 |
| reverse trapezoid | 空 | 不足 | 空 |
| asymmetric | 不足、切込みと文字が重なる | 不足 | 空 |

1行parallelogram/trapezoidの差分bboxは`(54,22,55,23)`で、拡張した余白だけの1 device pxの不足。
これを実glyphのはみ出しと取り違えない。asymmetricの1行はbbox`(54,34,72,57)`で、原bitmapでも重なりを確認した。
修正前のnode幅は252、修正後は320。高さは1行45/8行188、ラベル自体の寸法は不変。
修正後24条件はすべてbbox`(0,0,0,0)`・NULLREGION。1行のasymmetric/parallelogram/trapezoid、
8行の全6形状と暗色の代表、計14枚を視認し、文字の重なり解消と可読性を確認した。

共通のnative輪郭検査へ替えたstadiumも直接影響するため、同じ入口で一度確認した。
保存した6画像は#551の恒久manifestと全SHA256が一致し、照合結果を`stadium-reuse.json`へ保存した。
その後の製品修正でstadiumの寸法/描画経路は変わらないため、この成功を再利用する。

coreは6形状×12幅（0/1/14/28/29/30/31/32/33/196/200/4096）の72条件を確認する。
幅拡張の開始、整数丸め、許容metricsの上端で中央帯が文字と左右paddingを収めること、
小さいnodeの70×40とsubroutineの従来252×188を維持することを確かめた。

## 規則・限界

ARC-001/003/004/008/010/012、C-002/005/008/011/012/014/016、
QLT-008/009/010/012/013、GIT-001〜004、ADR0089。
主担当が単体で実装・自己レビュー・対象検証を行い、追加の独立レビューは使っていない。
追加waiverはnone、既存WVR-0001はWinHTTP callbackだけへ継続する。

長いラベルの幅増加で、巨大な図が従来のscene/bitmap上限へ達する可能性は残る。
横向きの辺が斜辺と離れる別の静的候補は#554へ分けた。この記録時点ではその実行再現は未実施。
実app/実ノートの操作、Store再提出/公開は行わない。実font/GDIの確認をhideによる通常版の受理や全DPIの証明とは扱わない。
整形・conformance・symbols・diffを確認し、最終headの必須CIを統合条件とする。
最終CI/通常反映/data照合/保全整理の確定原本は`D:/NeNeFolio/agents/553-polygon-label/integration-completion.json`へ。

GDIの経路はMicrosoftの[BeginPath](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-beginpath)、
[EndPath](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-endpath)、
[PathToRegion](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-pathtoregion)に従った。
