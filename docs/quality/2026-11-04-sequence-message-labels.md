# 背景線からメッセージ文字を保護する — #565

sequenceのmessage文字を透過描画していたため、離れた参加者を結ぶ文字領域を中間参加者の生命線やactivationが横切っていた。
実font/GDIの明暗6組・18ラベルで、対照6ラベルを除く12件に計2,212pxの差を再現した。
既存canvasの背景塗りを使う1行の修正後、18ラベルすべてが単独描画の参照と一致した。

## 変更と正典経路

製品編集前に[ADR0089](../adr/0089-native-lazy-mermaid.md)へ判断を追記した。
`diagram_sequence_layout.c`の`emit_message`が出す文字primitiveへ、`fill = DIAGRAM_INK_BACKGROUND`を指定する。
flowの辺ラベルで既存の塗りを再利用し、既存scene→canvasの塗り→文字で実測矩形内を保護する。
文字の位置・寸法・font・padding、矢印・生命線・activationの座標、primitive数や順序は変えない。
message以外の文字、新renderer/API/owner、schema・版・依存・上限・ゲートの変更はない。

## 画像の基準と対象

既存native試験へ`--message-labels`を追加し、引数なしの必須CIへも結線する。
fixtureでmessageとparticipantの文字列を分け、modelのmessage label IDに対応する実sceneの文字primitiveを抽出する。
同じfont/metrics・絶対座標・全体寸法で文字だけの参照画像を描き、全図とのBGRA差を数える。
座標を期待式として複写せず、実sceneの矩形を使う。境界1logical pxを除く内部を比較する。
各条件で全図/参照2枚を保存してから最後に集約assertするため、修正前の失敗でも全24画像が残る。

#563のpixel oracleは`heading_pixels`から`text_pixels`へ名称・診断文を一般化しただけで、比較式は維持した。
直接影響する枠見出しのselectorも対象実行して確認した。各条件の文字列IDによる抽出はこのfixtureの範囲であり、
任意の図の意味を診断する製品APIではない。

| 添字 | 条件 | ラベル数（各theme） | 修正前の内部差（各theme） |
| --- | --- | ---: | ---: |
| 0 | 3参加者、A→C | 1 | 22px |
| 1 | C→A、3行 | 1 | 68px |
| 2 | 中間Bのactivation | 1 | 243px |
| 3 | 5参加者・actor・autonumber・中間Cの二重activation・2行 | 1 | 729px |
| 4 | loop内のA→Cと返信 | 2 | 22pxずつ |
| 5 | 隣接参加者の往復と自己messageの対照 | 3 | 0px |

theme0が明、1が暗。`message-label-<添字>-<theme>.bmp`が全図、`-isolated.bmp`が参照。

## 試行と結果

作業先は`D:/NeNeFolio/worktrees/565-sequence-message-labels`、出力は同名`outputs/`、恒久原本は同名`agents/`。
sourceはmain`c063a60`。固定Debug/Ninja、clang-tidy、ASan/UBSan/nullabilityでbuildし、
`tools/record-run.py`で入力snapshot・argv・raw出力・終了値・生成exe/bitmapを保存した。

| run | コマンド・条件 | 結果 |
| --- | --- | --- |
| build-red-01 | 製品未変更、`cmake --build build --target diagram_render_tests folio_tests --parallel 2` | 子/記録側0 |
| red-01 | `diagram_render_tests.exe --message-labels <red-bitmaps>` | 12/18ラベル・計2,212pxの差、子/記録側1 |
| build-green-01 | 同じtarget、message塗りを1行追加 | 子/記録側0 |
| native-green-01 | `diagram_render_tests.exe --message-labels <green-bitmaps>` | 18ラベル全差0・350checks、子/記録側0 |
| core-green-01 | `folio_tests.exe --diagram` | 既存の図対象成功、子/記録側0 |
| headings-control-01 | `diagram_render_tests.exe --frame-headings` | 24見出し全差0・278checks、子/記録側0 |
| verify-01 | `clang-format --dry-run --Werror`、conformance、symbols core/application、`git diff --check` | 整形成功・違反0・2libraries違反0・diff成功、子/記録側0 |

全runのrecordingErrorsは空。red-01だけ意図したassert失敗をstderrに残し、build/green/control/verifyのstderrは空。
生産実装をなぞるfill値だけのcoreassertは追加せず、native画像と既存core図契約で対象を確認する（QLT-008）。
成功したcode/test入力はpush/レビュー/統合で再実行せず、最終headの必須CIを要求する（QLT-012）。

`compare-bitmaps.py`は全12図の寸法/文字矩形・全pixel・hashを照合した。
文字矩形の外は全pixel不変、参照12枚と対照2全図は前後の全SHA256が一致し、10全図だけ文字矩形内が変わった。
比較原本は`bitmap-comparison.json`。参照自体の不変により、文字を変えて差を消していないことも確認した。
修正前3全図と修正後6全図を視認し、複数行・明暗・activation・番号付き・loop・対照の表示を確認した。

## 自己レビュー・限界・完了条件

単体で実装・自己レビュー・対象実行を行った。既存塗りの指定1行で、追加の独立レビューは使わない。
規則: ARC-001/003/004/008/012、C-002/008/011/012/014/016、QLT-008/009/010/012/013、GIT-001〜004、ADR0089。
追加waiverはnone。既存WVR-0001はWinHTTP callbackだけで継続する。

境界1logical pxを除く今回の6組・実font/GDIの保証であり、全DPI/書体/任意図やhideの実機受理へは拡張しない。
実app/実ノートの試験操作、Store再提出/公開は行っていない。
最終headの必須CI、正典統合、通常反映/data照合、恒久保全と追加作業木/output整理の確定値は
`D:/NeNeFolio/agents/565-sequence-message-labels/integration-completion.json`へ収載する。
