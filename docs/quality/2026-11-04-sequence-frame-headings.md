# 背景線に隠れないシーケンス枠見出し — #563

sequenceの枠見出しを生命線/activationより先に描くため、長い見出しを背景線や活性区間が上書きしていた。
製品未変更の実font/GDIで明暗12図・24見出しを確認し、対照2見出しを除く22見出しに合計1,880pxの差を再現した。
実bitmapでも生命線が文字を横切り、activationが見出しを覆うことを確認した。

## 変更と正典経路

既存`diagram_sequence_layout.c`で枠の外線/section線と見出しの出力を分け、見出しだけを
生命線/activationの後に出力する。枠線の出力順と座標は維持し、背景の塗りと文字が線の前面になる。
既存boundsの計算は同moduleの`frame_bounds`へ一度だけ移し、外線と見出しから共有する。
見出しの寸法・文字列・塗り・font・座標、message/note/参加者の配置、primitive数は変えない。

ADR0089で採用した枠タブと既存scene→native描画の内側で、文字の可読性を回復する実装詳細と判断した。
owner/API/依存方向・保存方針を変えないため新ADRは不要。新renderer、描画後の補線、公開計測APIは追加しない。
schema・版・依存・上限・ゲートも不変である。

## 回帰試験と独立した画像の基準

既存native試験に`--frame-headings`入口を加え、引数なしの必須CIでも同じ対象を確認する。
6組×明暗で、図全体のbitmapと、そのsceneから実際の見出しprimitiveだけを取り出して描いた参照bitmapを作る。
参照も同じfont/metrics、同じ絶対座標/全体寸法、同じGDI canvasを使う。配置式をテストへ複写しない。
bitmapのBGRAを比較し、見出し矩形の境界1logical pxを除いた内部が単独描画と一致することを要求する。
境界のstroke/丸めを内部文字の上書きへ混同しない。

| 添字 | 入力 | 見出し数（各theme） |
| --- | --- | ---: |
| 0 | 既存日本語sequence、actor/participant、alt/else、activation | 2 |
| 1 | 長いloop見出し、単独participant | 1 |
| 2 | activationをまたぐ複数行loop見出し | 1 |
| 3 | loopの内側のalt/else | 3 |
| 4 | par/andとcritical/option | 4 |
| 5 | 長い参加者名で生命線を右へ離した短いopt見出しの対照 | 1 |

`frame-heading-<添字>-<theme>.bmp`が全図、`-isolated.bmp`が参照。themeは0が明、1が暗。
各runで全24bitmapを保存してから集計assertするため、修正前の失敗でも全条件の画像と差が残る。
既存nativeのほかのselectorは維持し、製品へ試験用経路は追加しない。

## 試行と結果

作業先は`D:/NeNeFolio/worktrees/563-sequence-frame-headings`、出力は同名`outputs/`、恒久原本は同名`agents/`。
固定Debug/Ninja、clang-tidy、ASan/UBSan/nullabilityを使い、`tools/record-run.py`で入力・argv・raw・終了値を保存した。

| run | コマンド・条件 | 結果 |
| --- | --- | --- |
| build-red-01 | mainab3d187・製品未変更、`cmake --build build --target diagram_render_tests folio_tests --parallel 2` | 子/記録側0 |
| red-01 | `diagram_render_tests.exe --frame-headings <red-bitmaps>` | 22/24見出し、合計1,880px上書き、子/記録側1 |
| build-green-01 | 文書main43e725c取込後・製品修正 | emit_backgroundsの認知的複雑さ11、上限10で拒否、子/記録側1 |
| build-green-02 | 見出し出力を専用の内部関数へ分離 | 子/記録側0 |
| native-green-01 | `diagram_render_tests.exe --frame-headings <green-bitmaps>` | 24見出しすべて差0、386checks、子/記録側0 |
| core-green-01 | `folio_tests.exe --diagram` | 既存の図対象成功、子/記録側0 |
| verify-01 | `clang-format --dry-run --Werror`、`eng/conformance.py --build-dir build`、`eng/symbols.py --build-dir build --require core application`、`git diff --check` | 整形成功、conformance0、symbols2libraries/0、diff成功、子/記録側0 |

全runのrecordingErrorsは空。最終build/native/core/verifyのstderrは空。
build-green-01は関数を責務で分離して解消し、閾値・抑制・除外を変更していない。
red完了後のmain取込は文書5件だけでnative編集と重ならず、ff前後のnativeファイルhash一致を確認した。
測定結果・対象codeが不変のpush/レビュー/統合では成功済み結果を再利用する（QLT-012）。

修正前の全図3枚と参照1枚、修正後の全図6枚を視認した。明暗の複数行、activation、入れ子、sectionで
文字を横切る線が消え、対照の短い見出しは維持された。数値上は12全図の見出し領域外の変更が0px、
参照12枚と対照2枚は修正前後で全SHA256一致。10全図だけが見出しの範囲内で変わった。
`compare-bitmaps.py`はrawから座標/見出し数を照合し、BMP寸法・全pixel・各hashを検査する。
確定した比較原本は`bitmap-comparison.json`。参照自体が不変なので見出しの寸法/文字変更で差を消していない。

## 自己レビュー・限界・完了条件

主担当が単体で実装・自己レビュー・対象実行を行った。既存owner内の出力順変更で、追加の独立レビューは使わない。
frameのbackground線→生命線→activation→見出し→message/note→参加者の順序を確認した。
primitive数、失敗時scene非公開、boundsの一元化、coreのOS非依存と既存結果型を維持する。
規則: ARC-001/003/004/008/012、C-002/008/011/012/014/016、QLT-008/009/010/012/013、GIT-001〜004、ADR0089。
追加waiverはnone。既存WVR-0001はWinHTTP callbackだけで継続する。

見出し内部の比較と今回の明暗6組の保証であり、全DPI/書体/任意図の一致やhideの実機受理ではない。
実app/実ノートの試験操作、Store再提出/公開は行わない。
最終headの必須CI、正典統合、通常反映/data照合、恒久保全・追加作業木/output整理の確定値は
`D:/NeNeFolio/agents/563-sequence-frame-headings/integration-completion.json`へ収載する。
