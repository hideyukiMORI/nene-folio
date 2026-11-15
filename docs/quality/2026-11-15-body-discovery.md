# 本文検索の品質記録 — #825

中核/application/native UIの対象確認と外側計測まで。設計は[ADR0095](../adr/0095-body-discovery-and-source-position.md)。成功したと未実行を分け、全試行をここへ追記する。

## 入力と先行調査

- hideの停止まで継続する現行goalに従う段階3b。#823 / PR824のmain `c6c70e045438647815da01d3ba4de0f04a3d0008`からD:/NeNeFolio/worktrees/825-body-discoveryを作成した。
- 作業出力はD:/NeNeFolio/outputs/825-body-discovery、背景design原応答と記録はD:/NeNeFolio/agents/live-grep-design-20261115。
- 背景Claude designは読取専用、exit0、stderr空、実装/試験/アプリ操作なし。助言はclaude-advice.mdへ保存し、受理と実測を区別した。
- 18:13以降の調査でnote_metadataのprivate native_point/native_lengthを確認。previewの標準EDITはCRLF二単位、RichEditのGT_RAWTEXTは改行一単位なので、共有scalar/改行処理を先に設計した。
- 推測した不存在pathの読取は失敗した（src/ui/win32/note_pane_stream.c、src/core/rtf_edit.c、src/core/discovery_summary.h）。変更・試験の成功として数えず、rgで実際のnote_pane.c/markdown_rtf.c/application/discovery_summary.hを確認した。
- GitHub pluginのIssue作成は403 Resource not accessible by integration。認証済みgh CLIで同じbody-fileを使い#825を作成した。重複Issueは作っていない。
- 追加のpath確認ではsrc/core/CMakeLists.txt、tests/unit/main.cが存在せず読取失敗した。ビルド一覧はrootのCMakeLists.txt、試験入口はtests/unit/folio_tests.cを確認した。
- #823の最終CI・反映・保全/整理の確定結果を現在欄と既存品質記録へ反映した。本文検索の試験結果とは扱わない。

## 中核の一区切りと試行

18:26:51にADRの独立読取レビュー指摘を閉じ、編集不可/採用境界を追記して受理した。
共有note_source、BODY scopeと有限POSITION、最初の一致の座標/順位、共通note_excerptのcontext/一行抜粋、
保存prompt越しの不変discovery_targetを実装した。application/UIへは未接続。
2,058通りの独立byte/scalar oracle、mixed newlineの固定座標、200超/removed、80行context、60KiB一行、
不正range、編集不可、所有寿命と全新factory確保点の試験を用意した。現時点では未実行。

configure-01は子/記録0、エラーなし。core-build-01はnote_excerpt_snippetの認知複雑度12（上限10）で子1。
未生成folio_tests.exeの終了artifact収集も失敗し、記録終了125。実行前入力/raw出力/失敗情報は保全した。
前後へ有限byteを取ってscalar境界へ揃える処理を、contextとsnippetが共有するsource_beforeへまとめた。
ゲート閾値・除外・抑制は変更していない。次の試行は新しいrun-dirへ記録する。

core-build-02は成功。core-unit-01 `folio_tests --body-discovery`は2,058 oracleと境界を通過した。
names-regression-01 `--discovery`、tags-regression-01 `--tags`も成功。core-conformance-01は実CMake graph込み0違反。
全5runは子/記録0・記録エラーなし。通常buildの確保失敗注入0件は、確保失敗確認の成功とは数えない。

測定buildは正典eng.coverage.build_measurement、compile_commandsのsanitizer vector/link options、nullability/ASan注入証明を再利用する。
measurement-build-01は入力一覧のsanitizer-probe.pyという誤記でsnapshot失敗（子未起動、記録125）。
実在するeng/sanitizer_probe.pyへ入力一覧を修正し、新しいmeasurement-build-02へ記録している。前試行/入力一覧は保持した。

measurement-build-02とmeasurement-body-01/names-01/tags-01は子/記録0、記録エラーなし。
正典ASan/UBSan/nullabilityとfno-sanitize-recover、リンクINCREMENTAL:NO/FUNCTIONPADMIN:6を照合した。
本文対象は2,058 oracleに加えtarget6・match excerpt2の確保失敗点を通過。名前対象はbuilder5・removed5・excerpt2、
applicationの既存session78確保点を通過した。タグ編集の共通化も対象selectorで通過した。
全件local実行は行っていない。これらは中核までの結果で、後続application/UI変更の成功には流用しない。

## applicationの一区切りと試行

19:01までに閉じたmodeと空語INPUT、新しいcorpus一巡の開始/観測再利用、snippetと一致周辺preview、
不変targetの生成、採用済みidentity/本文の照合を実装した。途中BODY→NAMES preview→BODY再開では
同じ一巡で読んだnoteを再読せず、明示的な新管理一巡では全観測をリセットする。
mode/語/条件の保持、未読/対応外metadata、保存改訂、所有寿命、編集不可と98確保点を対象とした。

- 調査の追加path誤り: src/adapters/win32/folio_window.c、src/ui/folio_window.c、accessible_surface_slot.hの読取失敗と、
  PowerShell上でui_text_*.cを明示pathに渡したrgのglob失敗。実在するsrc/ui/win32/folio_window.c、
  accessible_surface_button.h、src/core/ui_text.cをinventoryから確認した。編集や試験結果ではない。
- app-build-01の前の整形は推測したC:/Program Files/LLVM/bin/clang-format.exeが不存在で失敗。
  続く記録runはtoolchain未設定のためcmake子未起動、記録125（child_launch WinError2）。既存exeのartifactは旧版であり新build成功ではない。
- 正典eng/toolchain.ps1を初期化し整形したapp-build-02は、continue_discoveryの認知複雑度11（上限10）で子/記録1。
  観測配列準備と一巡開始をprepare_discovery_passへまとめたapp-build-03は子/記録0。基準・除外・抑制変更なし。
- app-body-01は追加snippet試験の入力に誤りがあり失敗（子/記録1）。note_text_createがfile先頭BOMを一つ消費する契約に対して、
  raw fileへ計算したmatchをbodyへ渡していた。file署名と本文U+FEFFを二つのBOMで区別し、matcherを実際のbodyへ当てた。
  検証したい期待値（本文U+FEFF保全、rowのcontrol/TAB/U+2028/U+2029だけの空白化）は維持した。
- app-build-04/app-body-02は成功。2,058独立oracle、最大256補助平面scalarの一致全体保持、長い前行を行境界で捨てるfallback、
  保存改訂/変更snapshot/同じ一巡/新一巡を通過。app-names-01とapp-management-01も成功し、app-conformance-01は0違反。
- app-measurement-build-01は正典coverage compiler/sanitizer証明でmeasurement-02を生成した。
  app-measurement-body-01はtarget6・excerpt2とsession98確保点、names-01はbuilder5・removed5・excerpt2・session78、
  management-01はupdate52・preparation154・application43の確保点を通過。すべて子/記録0、記録エラーなし。
  成功した各runのinputs/outputsとraw出力を保持し、ここでは全件local gateを実行していない。

19:00:05のapplication独立読取レビューはP1/P2なし。corpus観測世代とtargetの所有/採用本文照合を確認し、
UI/native確定は評価外と明記した。原本はD:/NeNeFolio/agents/live-grep-design-20261115/application-review.md。

## UI接続の一区切りと試行

名前/本文のmode、native一致選択と実際の行番号、3言語の状態/抜粋、確定時の既存文書切替と採用境界を接続した。
通常のEDIT/VIEW、別note、無題、外部変更、編集不可、native投入拒否、私用clipboardと画面matrixの対象試験を追加した。
初期patchはnoticeの文脈が一致せず全体未適用だったため、実際のanchorで適用し直した。

19:21の独立読取レビューはP2を1点指摘した。正常確定後の面終了がpane noticeを消し、
snapshot不一致または再読編集不可の理由が残らない。短命なnotice値を保持し、共通終了の後へ戻す修正を入れた。
レビューではCtrl+V/Shift+Insertの標準EDIT処理がWM_PASTEの検証へ入るか未確定だったため、
WM_PASTE・Ctrl+V・Shift+InsertでCR/LF/CRLFを渡すnative確認を追加した。読取だけで欠陥とは断定していない。

ui-build-01は認知複雑度6関数と試験の長さ2関数で失敗。未生成native exeのartifact収集も失敗し記録125。
mode移動、UIA focus、tab操作、入力処理とfixture準備を意味ごとのhelperへ分けた。閾値・除外・抑制は変更していない。
ui-build-02へ記録している。追加調査のPython標準出力はcp932の補助平面文字で途中失敗したためUTF-8を明示した。
存在しないui-build01/command.jsonの読取は失敗し、実際のui-build-01/run.jsonと記録toolの仕様へ戻った。

ui-build-02とui-body-unit-01は子/記録0、記録エラーなし。ui-body-native-01は選択/変更flagの複合assertで失敗した。
ui-body-copy-01は151確認に成功。keyboard試験の入力focusを明示し、失敗箇所へ実測値を足したui-build-03は成功。
ui-body-copy-02は152確認に成功し、WM_PASTE・Ctrl+V・Shift+InsertのCR/LF/CRLF拒否を確認した。
このWindowsの標準EDITではkeyboard入口も同じ検証へ入り、追加の製品key処理は不要だった。

ui-body-native-02も同じassertで失敗したが、独立GT_RAWTEXT上の期待選択31..37と実際31..37は一致した。
差は変更flagで、試験のUndo準備後から-1だったものを、assertが常に0と要求していた。
同じEDITの契約はnative再投入せず変更flag/Undoを保持することなので、同一文書は開始前flagとの一致、
新しいEDIT投入は0を要求するよう試験を修正した。選択範囲と実際のRedo/Undo検証は維持した。ui-build-04へ記録する。

ui-build-04は成功。ui-body-native-03は待機が続き、専用exe/引数/PIDを照合してその子processだけを終了した。
子/記録4294967295、記録エラーなし。buffer内の途中表示は残らず、どのassertまで通過したかの証拠にはしない。
native投入拒否後は通常の失敗dialogが出る契約だが、追加fixtureにその終了処理がなかった。
既存begin_expected_failureで所有するdialogだけを確認/閉鎖し、独立選択値のstdoutをflushするui-build-05は成功した。

ui-body-native-04は同一EDIT31..37、同一VIEW→EDIT31..43、別note7..19を通過し、復旧assertで失敗した。
追加出力だけのui-build-06は成功、native-05で復旧後選択0..0/undo0/modified-1を記録した。
原因はfixtureが既に選択中のEDITへSelectionItem.Selectを送り、既存の再試行Invokeを呼んでいなかったことだった。
既存管理試験と同じInvokeを使い、本文の再投入、modify0、undo0、古い一致を選ばないことを確認するui-build-07へ進めた。
これらのfixture修正で製品の保護/復旧処理は変更していない。

ui-build-07とui-body-native-06は成功、260確認。無題promptの取消/保存、外部変更/消失/lock、
編集不可の事前拒否/再読VIEW保護、両採用境界のnative拒否と既存Invokeによる復旧も通過した。
UI読取レビューのP2は19:39:52の限定再読で解消、他のP1/P2なし。

ui-body-visual-01は入力欄の内側とradio外枠のbottomを同じ値としていたassertで失敗し、画像27件は未生成だった。
実際のfieldは枠内へinsetされるため、同じ行の内側に収まりradioと重ならないことを確認するassertへ変更した。
ui-build-08は追加試験の62行（上限60）で失敗。geometry検査を独立helperへ分けたui-build-09は成功した。
ui-body-visual-02は441確認と27画像出力を通過し、ui-conformance-01は実graph込み0違反だった。

その画像の目視で製品の不具合を確認した。一致範囲の数値は正しいが、原文は23行目から表示され、43行目の一致が
最小幅でも広幅でも画面外だった。load時のEM_SCROLLCARETがgeometry/表示前に送られていた。
新しい抜粋を投入した時だけreveal要求を持ち、実際に表示/配置した後に一度scrollする修正を入れた。
通常のscroll/copy中に繰り返し一致位置へ戻さない。native文字位置とfont高さを使う表示内assertをmatrixへ追加した。
ui-build-10は追加試験のGET_X_LPARAM/GET_Y_LPARAM定義不足を検出した。失敗runも保持する。

windows.hという不存在include行へのpatchは未適用だった。実際のinclude列へwindowsx.hを追加したui-build-11は成功。
ui-body-visual-03は477確認/27画像に成功した。native選択の先頭位置は最小幅(4,17)・広幅(4,289)、
行高17pxで、それぞれ高さ34px/340pxの実テキスト矩形内へ収まった。最小画像は700×450px、広幅1250×850px。
3言語の明暗/高コントラストの最小preview9枚と広幅の代表画像を目視し、一致43行が選択表示されることを確認した。
修正前visual-02の画像もそのrunのartifactsに保全されている。既存のsystem scrollbar/文字fallbackの見た目は維持している。

ui-ctest-01は名前検索の件数assert失敗と本文の30秒timeoutだった。本文の追加fixtureが保存した
target/body-discovery-saved.mdを残し、次回の件数と同名保存へ影響していた。製品の検索結果が古いという失敗ではない。
作成した宛先を既存identity resolve/trashへ渡してそのnoteだけを除く終了処理をfixtureへ追加した。
専用exe process不在・正確な絶対path・リンクなしを確認し、失敗時dataをui-ctest-01-stale-dataへ移して保全した。
ui-build-12は成功。ui-ctest-02 `ctest --test-dir build -R '^(body_discovery_native|discovery_native)$' --output-on-failure`は
2/2成功、6.22秒。続く3ノート測定も成功し、前試行の保存noteが残らない状態を確認した。
共有RadioButton/SelectionItem経路のui-palette-01 `--palette`は156確認に成功した。

## 外側の時間計測

ui-body-timing-01は`python tools/measure-discovery.py <tags_native_tests.exe> --body`、7回、保存済み3ノート。
fixture内で実clockを読まず、外側のpipe往復・UI・OS scheduling込みで区間を分けた。子/記録0・記録エラーなし。
各回新しい検索sessionを開くが、OS cacheは冷やしていない。Debug/sanitizerのnative buildである。

| 区間 | 最小 ms | 中央 ms | 最大 ms |
| --- | ---: | ---: | ---: |
| 面の開始と新しい全本文一巡 | 42.895 | 44.237 | 190.451 |
| 本文照合と一致位置の完成 | 6.777 | 7.079 | 10.247 |
| 最初の一致周辺preview | 6.669 | 7.146 | 10.588 |
| 同じ本文による語変更2回 | 40.035 | 41.296 | 51.836 |
| 別候補の一致周辺preview | 12.508 | 13.613 | 16.796 |

有限workは実時間上限ではない。特に初回source読込、allocation、UI投影、snapshot copyは同期処理であり、
3ノートの値から大量corpus/16MiB本文の応答時間を一般化しない。全7sampleはraw stdoutに保持した。

## 規則・完了確認

自己レビューでは唯一のmatcher/read/corpus/文書切替、所有target、閉集合分岐、確保失敗の非公開、
最新世代、座標射影、UIA/paint/hitの共通geometry、ゲート無緩和を確認した。
追加schema/版/配布はない。FR-048、ADR0095、操作表/所有境界/現在欄/ロードマップ/日報/引き継ぎを更新する。
成功したcore/application測定とclipboardの不変境界は再利用し、ローカル全件gateは行っていない。
最終headの必須CIと統合・通常exe反映・保全/整理はまだ別の工程である。
final-symbols-01はcore/applicationの2 library・0違反、ui-conformance-02は実graph込み0違反。
final-format-01は変更C/headerのclang-formatとgit diff --checkを通過した。いずれも子/記録0・記録エラーなし。

## 未確認と残る制限

最終CI・統合・通常exe反映は未完了。製品の本文モードを完了済みとは扱わない。
実読み上げ、物理IME、全OS/DPI、大量corpusの応答保証、同時外部変更の排除は未確認。
追加waiver none、既存WVR-0001継続。
