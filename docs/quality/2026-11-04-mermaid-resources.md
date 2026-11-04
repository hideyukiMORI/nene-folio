# 反復生成・取消後の描画資源 — #561

既存native試験は取消の返値とbitmap非公開を確認するが、反復後のGDI/USER object数とprocess handle数を
観測していなかった。既存Release libraryを外部診断から呼び、正常生成、途中取消、layout上限拒否、
workerの生成/破棄を分けて測定した。測定区間2,176回で各batch終了後の資源数に残存増加はなかった。
製品修正を追加する根拠は得られず、製品code/API/schema/版/依存/ゲートは変更しない。

## 正典経路と環境

作業先は`D:/NeNeFolio/worktrees/561-mermaid-resources`、出力は同名`outputs/`、恒久原本は同名`agents/`。
製品sourceはmain`87251022dc37434fa63f82676b16779c75e752a3`。#559の固定CMake Releaseから
core/application/adaptersの3libraryと同梱fonts7ファイルをコピーし、全hashを照合した。
src/tests/eng/fontsの各Git treeとCMakeLists.txtが同一であることを確認し、再buildはしない。
文書#559を統合したmain`ab3d187`の取込でも製品入力は不変で、成功済み測定は再利用する。
出典・コピー先・hash・bytesと入力一致の原本は`reused-libraries.json`。

外部`resources-native.c`は既存`diagram_render_tests.c`のfixture/要求生成を再利用し、
`diagram_render_read`、`diagram_canvas_measure`、`diagram_layout_create`、`image_adapter_create/destroy`を使う。
製品へ計測用APIや別の所有経路を追加しない。固定LLVM19.1.5で`/O2 /MT`、Windows11 build26300、
同梱Noto Sans JP regularの実font/GDIで測定した。実app/実ノートへ操作しない。

資源数は自processに対する
[GetGuiResources](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguiresources)の
GDI/USER現在数と、[GetProcessHandleCount](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesshandlecount)の
executive object handle数を使う。GUIの0は失敗と区別し、直前にLastErrorを0にして返値/LastErrorを確認する。
各観測前にGdiFlushを呼ぶ。pen作成でGDIがちょうど1増え、DeleteObject後に元へ戻ること、
event作成でhandleがちょうど1増え、CloseHandle後に元へ戻ることを最初に確認した。
USER objectについては増減の感度試験を追加していない。

## 条件と測定結果

各条件で8回のwarmup後に基準値を取り、16回ずつ8batch、計128回を測る。各batch終了後に
GDI/USER/handleのすべてがその条件の基準値と厳密に一致するかを記録した。許容幅は設けない。
process全体の最初にも8回のflow生成でGDI/font/workerの初期化を行う。

| 条件 | 入力 | 測定回数 | GDI / USER / handles | 結果 |
| --- | --- | ---: | --- | --- |
| 正常生成 | flow / sequence / multiline-shapes | 各128 | 1 / 1 / 107 | 全24batchで基準値一致 |
| parsing通知後の取消 | 同上 | 各128 | 1 / 1 / 107 | 全24batchで一致、bitmap非公開 |
| layout通知後の取消 | 同上 | 各128 | 1 / 1 / 107 | 全24batchで一致、bitmap非公開 |
| drawing通知後の最初の取消 | 同上 | 各128 | 1 / 1 / 107 | 全24batchで一致、bitmap非公開 |
| 縮小処理の最後の取消確認点 | 同上 | 各128 | 1 / 1 / 107 | 全24batchで一致、bitmap非公開 |
| layout座標上限の拒否 | 160ノードの独立した横並び | 128 | 1 / 1 / 101 | 全8batchで一致、bitmap非公開 |
| idle workerの生成・破棄 | 既存image_adapter | 128 | 1 / 1 / 97 | 全8batchで一致 |

flow/sequence/multiline-shapesは既存native fixtureで、日本語、group、循環、note/frame/activation、
複数行の円柱/円/diamond等を含む。正常生成ではbitmapが存在し、破棄後に計数する。
取消ではIMAGE_CANCELLEDとbitmapがNULLであることを毎回要求する。
drawing後半の取消は正常生成で得た同phaseのcallback回数（24/45/38）を使い、最後の確認点で発生させた。
現在のcanvasはprimitive描画後、128行ごとの縮小処理でも取消を確認するため、最後の点は縮小中に属する。

layout拒否は、160ノードのparseと実font計測が成功し、diagram_layout_createがDIAGRAM_TOO_LARGEを返して
sceneを公開しないことを別途1回確認した。同じ入力をrender経路で136回（warmup8+測定128）拒否させた。
workerはidleの生成とdestroyによるjoin/event/thread解放を確認し、任意の通信や実行中の競合へ拡張しない。
表のhandle107/101/97は別processまたは所有状態の異なる基準値で、相互の差を漏れとは解釈しない。

## 初回診断の失敗と対象結果の再利用

`resources-01`は最初の15条件を最後のbatchまで終え、全135計数行と2感度確認行が一致した後、
上限拒否用の20ノードで期待結果のassertに失敗した。子/記録側は1であり、run全体を成功扱いにしない。
原因は診断入力の想定で、現行の座標上限32,768を20ノードでは超えていなかった。
追加診断で20ノードはIMAGE_READY、実bitmap5,232×133pxと確認した。

拒否fixtureを160ノードへ変更し、実際にlayout上限へ到達することを確認した。
初回の失敗時には実返値の詳細を印字していなかったため、以後の不一致時は期待/実際のenumとbitmap有無を出す。
未完条件だけを動かす`--remaining`を外部harnessへ加え、成功済み15条件は再実行しない。
link-02後の自己レビューで診断bitmapをNULL確認より先に参照する箇所を見つけ、
実行前にguardを追加してlink-03とした。link-02のexeは実行していない。製品コードや期待される上限は不変。

`resources-02 --remaining`は元20ノードの診断、160ノード拒否、worker条件を完了し、子/記録側0。
残る2条件18計数行もすべて一致、counterMismatches0、674checksだった。
元runのsource/exeはsnapshotに保持し、集計は完了済み15条件のraw行を明示的に再利用する。
全runのrecordingErrorsは空。link3回とresources-02のstderrは空、resources-01は期待結果assertの1行だけ。

| run | コマンド・用途 | 結果 |
| --- | --- | --- |
| link-01 | `link-resources.ps1`、外部harnessと再利用Release library | 子/記録側0 |
| resources-01 | `diagram_resources.exe` | 15条件の計数完了後に20ノードの期待結果で停止、子/記録側1 |
| link-02 | 拒否入力/診断/未完条件入口を更新 | 子/記録側0、exe未実行 |
| link-03 | 診断bitmapのNULL guardを追加 | 子/記録側0 |
| resources-02 | `diagram_resources.exe --remaining` | 上限拒否/workerの全計数一致、子/記録側0 |

各runは`tools/record-run.py`でharness、実exe、再利用library、同梱font、raw出力と終了値を記録した。
集計`summary.json`は初回の終了1と既知のstderrも要求し、19group（15条件+2条件+感度確認2件）の
batch数/全計数行/最終結果を検査する。失敗を新しい成功記録で上書きしない。

## 判断と限界

GDI/USER/handleの残存増加は観測せず、製品修正や追加の許容値・gate変更は行わない。
同じ資源数が保たれることは全heapの漏れなしを意味せず、増減が相殺される可能性も排除しない。
有限の反復と限定3入力、単一OS/font条件の観測であり、任意図、全DPI、長期間の保証やhideの実機受理ではない。
実app/実ノート、Store版、ネットワークdownloadへ操作していない。

主担当が単体で設計・診断・自己レビュー・文書化し、追加の独立レビューは使わない。
規則: ARC-001/003/004/008/012、C-008/014/016、QLT-008/010/011/012/013、GIT-001〜004、ADR0089。
追加waiverはnone、既存WVR-0001はWinHTTP callback限定で継続する。
最終head必須CI、正典統合、恒久保全・追加作業木/output整理の確定原本は
`D:/NeNeFolio/agents/561-mermaid-resources/integration-completion.json`へ収載する。
文書だけの変更で製品入力が不変なら通常exeは#557の反映を再利用する。
