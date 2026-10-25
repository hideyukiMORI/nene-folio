# 設定・nativeモーダルの応答と主窓操作 — 2026-10-25（#373）

Issue: [#373](https://github.com/hideyukiMORI/nene-folio/issues/373)。基点main `60a564a`。
hideが前件#368の実機試用後も「毎回」設定/モーダルが重く、モーダル中の窓移動と主窓×が使えないと報告した。
判断は[ADR0063](../adr/0063-derived-ui-metrics-and-surface-arrangement.md)と[ADR0064](../adr/0064-content-modal-and-window-shell.md)。
規則: ARC-001/004/005/007/010/011/012、C-002/005/008/012/013/016/017、QLT-008/012/013、GIT-001〜004。

## 原因と変更境界

- 固定操作ラベル幅が描画・hit・tooltip・UIAで繰り返し計測され、面の初期化時の同期EN_CHANGEが途中の配置を増やしていた。
  同じGDI計測を明示した入力条件付きで保持し、面・選択・初期入力を確定してから一度配置する。
- 名前入力は全理由の最大高を開くたびに計測していた。主窓所有の`name_prompt_metrics`へ派生値だけを保持し、実dialog・入力・結果は毎回の同期呼出しが所有する。
- 設定/テーマの暗幕とhit判定、UIAの一律無効化、native DialogBoxのroot ownerが、内容操作と窓管理を一緒に止めていた。
  暗幕をヘッダー下へ限定し、内容を止めたまま窓移動・resize・主窓×を通す。nativeはroot所有の非表示`modal_owner`を使い、OSの同期loopを維持する。
- 主窓×で最内nativeを取り消した後、native深さと最外の同期業務dispatchの双方が戻ってから通常終了へ渡す。
  終了中の再close、無題の保存/保存しない/取消、保存失敗、未確定テーマ、IME拒否は既存契約を維持する。

保存schema・実行時依存・ゲート閾値/除外は不変。Waivers: none。

## 同条件の性能比較

恒久証跡は `D:/NeNeFolio/agents/gui-responsiveness-20261025/`。
`performance/inputs.json`等の入力と生CSV/集計、`native-metrics/`の独立計測fixture、`modal-input/`のowner診断を保持する。
データ本文を複製せず、3カテゴリ・6合成ノート・合計347bytes、日本語・行番号ON・monalisaという形状だけを揃えた。

| 操作（Debug・各8回中央値） | 基点 | 修正後 |
| --- | ---: | ---: |
| 閲覧中の設定open | 21.05ms | 11.93ms |
| 閲覧中の設定close | 16.35ms | 8.98ms |
| 閲覧中のテーマopen | 14.07ms | 7.43ms |
| Help open | 35.64ms | 23.27ms |
| Helpキー説明へ切替 | 19.91ms | 16.31ms |
| Help再描画 | 17.40ms | 14.22ms |
| native名前入力のactivation | 25.60ms | 7.59ms |
| native名前入力の初回paint | 40.24ms | 17.09ms |
| native失敗箱のactivation | 6.16ms | 5.23ms |
| native失敗箱の初回paint | 22.92ms | 16.93ms |

閲覧中の設定openは直接dispatch約4.05→1.86ms、後続paint/UIA drain約16.95→10.08ms。排出後のpending paintは両候補とも0。
配置3→1回。固定ラベル幅の参照60回を保ち、取得時間は約5.44→0.010msになった。
native名前入力は66件の理由計測に約19.19msを使っていた。63固定理由は8回の開閉を通して1回だけ計測し、種別案内3件は各回測る。
nativeの意図的なtimer待ちを除いたTOTALは名前27.91→10.54ms、失敗箱9.44→7.72ms。FUNCは描画と復帰後のqueue排出を含むため、TOTALと和を比較しない。
独立した統計値の和を総時間にしない。
専用非入力desktopのCPU/message/paint測定であり、物理inputからDWMの実display呈示までの時間ではない。
前件#368とはfixture/データ条件が異なり、数値を連続した改善率として扱わない。

起動後の初回設定は81.73→81.08msで改善していない。名前入力の初回TOTALも335.22→281.07ms残る。
この初回値を反復中央値と混ぜず、初回待ちの解消や一般的な体感性能を保証しない。
比較の正本は`performance/final-performance-comparison.json`、生標本は`Debug-ja-profile.csv`と`after-Debug-ja-profile.csv`。
性能測定中はcompiler・他のnative fixture・visibleアプリを起動せず、入力source/library/exeのhashを実行前に照合した。

## 実visible観測

computer-useで旧通常exeと同じhashの検証用コピーを操作した。記録は`visible-before-observation.json`、入力は`performance/visible-before-inputs.json`。
設定/名前入力でヘッダーまで暗くなり、主窓×と共通toolbarがUIAで無効となることを確認した。Escで元focusへ戻り、検証用アプリは主窓×で正常終了した。
設定中のdrag後、WGC合成画像から設定面が欠落したがUIAには残っていた。画像だけから製品描画の欠陥と断定せず、captureの限界として分離する。
物理input→presentのms値、物理IME、実reader、物理DPI跨ぎは未測定。

修正版の通常設定・名前入力でも実画面を確認した。root/header/主CloseはUIAで有効、内容の子は無効で、暗幕はヘッダーを避ける。
両面から主窓×を実クリックし、対話取消とアプリの正常終了を確認した。名前入力のEsc後は呼出元の新規カテゴリ操作へfocusが戻った。
header drag入力後も面のUIA/対話は存続したが、CU APIは絶対座標の移動量を返さないため、その量の実測とはしない。
設定のWGC画像欠落は修正前後で同じだった。記録は`visible-after-observation.json`、入力は`visible-after-inputs.json`。
終了後も全16入力ファイルのhashは不変。元の利用者dataや利用者の稼働processは変更していない。

## 対象検証とレビュー

| コマンド/fixture | 結果 |
| --- | --- |
| 共通`after-debug/build`の`cmake --build ... --target nenefolio` | Debug compile/clang-tidy/link成功。最終入力は`modal-input/geometry-resume/candidate-geometry-inputs.json`、同所`build-geometry.log` |
| `performance/final-fixtures.ps1 -Action Compile` | 外部性能/直接境界fixtureのcompile/link成功。`final-fixture-build-inputs.json` |
| 同`-Action RunPerformance` | exit 0、`RESULT,success`。同条件各8回の比較 |
| 同`-Action RunContracts` | 固定ラベル/初期化124、Help101、帯geometry95、設定scroll48、共通幅3。計371 checks/0 failures、全process exit 0 |
| `native-metrics/run-fixture.ps1` | 22 checks/0 failures、compile/link/run exit 0。旧全理由計測との独立oracleで288条件が一致 |
| `modal-input/run-native.ps1 -SkipBuild -Mode <mode>` | child78、テーマ終了取消7、名前30、UIA無題35、失敗箱22、入れ子20、色選択22、MessageBox退避22、実保存失敗25。計261 checks/0 failures、全process exit 0 |
| `geometry-resume/run-geometry.ps1 -SkipBuild -Mode minimum -RunLabel green` | 188 checks/0 failures。3言語×320/560幅×render DPI120/144/168、最小高360、選択全体可視・設定全選択到達・最長理由/0件/高コントラスト・高さ単調・通常矩形一致 |
| `python eng/conformance.py --build-dir <after-debug/build>` / `clang-format --dry-run --Werror <変更20C/H>` / `git diff --check` | 規約違反0、format/空白成功。最終source/exe/libraryのmanifest全10hash一致 |

名前理由高の同値は3言語×3字体サイズ×通常/最小幅×実HDC字間0/1×8案内で確認した。
GetObject/GetDC/GetTextCharacterExtra/SelectObject/DrawTextの失敗値を成功cacheへ固定せず、復旧後に再計測することを確認した。
対象試験のASan既知`interception_win`警告をraw stderrへ保持し、ASan/UBSan障害報告はない。

read-only独立レビューで、通常終了を取り消した後のfocus復帰欠落をP2として1件修正した。
native深さ0で背景HWNDを有効へ戻し、終了phaseの業務入力guardを維持したまま、既存local focusの復帰を成立させた。
戻り先の実top-levelもactivateし、hidden ownerへのactivation残留を避ける。修正版の再レビューでは追加の確定指摘0件。
UIA hostの同期寿命・親containerの可否・Close通知1回・private message route・移動先monitor・font失効も確認した。
修正前sourceをhash一致で再構成すると、テーマ終了取消とUIA無題のfocus検査が各1件失敗し、修正版で成功した。
保存失敗は実ファイルの書込み共有を拒む条件で作り、失敗中に旧保存bytesと本文・focus・active面が保たれ、解除後の通常終了で新本文が保存されることを確認した。
nativeの結果は`modal-input/native-results.json`、入力は`candidate-focus-inputs.json`。UIAは製品host/viewの境界を検査しており、別processのnative UIA clientや実readerの代替ではない。

## 最小窓の追加修正と画像

画像確認で、英語320×360logicalのテーマ候補が0行になる問題を見つけた。実OS120DPIで1行40pxに対して33pxしか残らなかった。
修正前は35 checks中3件失敗。候補の入力後/脚前gapを通常8から下限4logicalまで不足分だけ縮め、固定理由・案内・件数・戻る・面高は保った。
修正版は上表188 checksに成功。実OSのDPIは120で、144/168は製品の同じ書体/geometryへ明示したrender DPIであり、実モニター跨ぎではない。
追加証跡は`modal-input/geometry-resume/geometry-results.json`と同所の入力manifest・原ログ・画像に収載した。
通常高640の矩形は旧式と一致し、変更は短いテーマ面の間隔だけなので、既存の性能・名前計測・保存/終了の成功結果を再利用した。

名前入力のresize/最大化/復元後はPrintWindowの画像に題と子部品の欠落が残ったが、resize前のPrintWindowと同時点のWM_PRINT描画には存在した。
完成画像は実製品のWM_PRINT（PRF_CLIENT/CHILDREN/ERASEBKGND）で取得し、別のUIを描いた合成画像ではない。
設計席は最小テーマ・最長理由・0件・名前入力の4PNGを実見し、候補1行・通知全文・入力/保存先/取消の到達を確認した。
このcapture診断で製品の名前入力を変更せず、PrintWindowの欠落を製品表示の失敗とも、逆に物理displayの成功とも扱わない。
ClaudeCode CLIのデザインリナも同4枚を実読し、最終批評で確定した修正必須事項なしと判定した（`design-final-result.json`、exit 0）。
英語画像の批評であり、他言語・DPI・コントラスト数値・実reader/IME/性能を画像だけで合格にしていない。resize後の実displayの完全性もWM_PRINT画像だけでは保証しない。

## 統合候補の確認状態

native統合fixtureと最小窓の追加検証は上記範囲で成功。[PR #374](https://github.com/hideyukiMORI/nene-folio/pull/374)の最終headの必須CIで`eng/check.ps1`を通してから統合する。
hideの並行作業へ干渉しないため、再開後はComputer Useを使わず、専用非入力desktopと保存画像で対象境界だけを確認する。
通常ビルドは`tools/prepare-real-machine.ps1`で反映し、自動起動しない。統合・通常exe・整理の確定結果はIssue/PRと恒久証跡の`completion.json`へ記録する。
