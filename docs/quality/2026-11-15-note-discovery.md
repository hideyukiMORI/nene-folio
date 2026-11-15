# 名前検索と原文previewの実装記録

2026-11-15 / #823。開始mainは0851f66de66313586d68fc80bf480b8ce88a2091。
先行ADR0094は15:55に独立読取レビュー2点を反映して受理。所有表/構成/並行規則を先に更新し、
index_filter_buildのscope/Unicode scalar部分列/固定18 bucket/上位200と不変note_excerptを実装した。
開始時はapplication/UI・PRがなく、中核の対象selectorをfolio_tests --discoveryへ加えた。
以下は各段階の実行結果であり、未実行を成功とは記録しない。

追加領域はD:/NeNeFolio/worktrees/823-note-discoveryとD:/NeNeFolio/outputs/823-note-discovery。
design相談の原応答/stream/briefと恒久証跡はD:/NeNeFolio/agents/discovery-design-20261115。
背景Claudeの読取専用相談は終了0で完了済み。相談は実機確認とは扱わない。

前段#821/PR822は最終head c3b36f7d1230e2511c9e8195698294549e304339、必須CI43686022314成功、
main0851f66へ統合。51 CTest成功、branch9541/10500。通常exe1.3.2のSHA256は
316e208282bd485a853cbb6ad71f45ea82ee045f488ffde8c5615c2b51b9af33、反映前後の実data34件のbytes/hash/日時は一致した。
前段の全試行・整理結果はD:/NeNeFolio/agents/tag-management-20261115/completion.jsonとpost-integration.mdを正本とする。

静読中、index_query.h、name_text.h、command_surface.h、ADR0074の誤った仮名pathの参照が失敗した。
index_filter_query.h、note_name/category_name、既存command_surfaceのmode/metrics、ADR0074-linear-index-matchingを確認して継続した。
これらは試験成功とは扱わない。新matcher、別本文台帳、worker、ゲート変更、版上げ/配布/Store操作は行っていない。

## 中核の一区切り

configure-01は子/記録0、記録エラーなし。core-build-01は子/記録1、記録エラーなしで停止した。
index_filter_buildのallocate_build/removedがそれぞれ認知複雑度11、上限10へ違反したため、
容量条件と完成した順位情報のcopyをprivate helperへ分けた。閾値・除外・抑制は変更していない。
まだ単体試験は走っていない。raw出力と実行前inputは各run-dirへ保存した。

core-build-02/03、core-unit-01 `--discovery`、bounded-regression-01 `--bounded-index-matching`、
tags-regression-01 `--tags`は子/記録0・エラーなし。後の03は空一覧/SEALED借用解除/removed確保失敗/16KiBちょうどのfixture追加だけ。
measurement-build-01は正典eng.coverage.build_measurement、Debug sanitizer vector/link options/注入証明を共有して成功した。
measurement-unit-01 `--discovery`はbuilder5・removed5・excerpt2の確保失敗点を注入して成功。
measurement-legacy-01 `--bounded-index-matching-oom`も成功（既存factory5、初回準備30、保存copy9等）。
全件のlocal試験や無関係なnative試験は行っていない。

core-conformance-01はCNF-002の4違反で失敗。index_filter_build.c/note_excerpt.cのprivate補助recordを
同一.cに置いたため、一ファイル一型と名前対応に違反した。それぞれindex_filter_rank.h/note_excerpt_span.hへ移し、
型名をファイルへ揃えた。意味・layout・ゲートは変更していない。
core-build-04は子/記録0、core-conformance-02は0違反かつ子/記録0で成功した。両方の記録エラーは空。
型の配置だけなので、直前の挙動/確保失敗の成功結果はその範囲で再利用する。後続application変更の対象試験は別に行う。
調査中のtests/core、tag_catalog_state.hの誤参照とPowerShell下のsrc/core/*.h glob失敗も読取失敗として残す。

## Applicationの接続と途中診断

16:24以降、既存のentry/iteratorへ名前scopeを接続し、folio_stateが独立した条件・query・順位結果・
選択の所有名・原文excerpt・session内の読取観測を持つようにした。別本文cacheは作っていない。
note_identityは検証済み台帳stemをそのまま所有し、共通switch_noteが保存promptの後に再解決する。
新規の入力拒否2種を3言語catalog/失敗対応表へ加えた。検索面の描画・操作はこの時点では未実装。

app-build-01は未定義のnenefolio_ui targetを指定したため子/記録1で停止した。CMakeの定義を確認し、
nenefolio_windowへ訂正したapp-build-02はfolio_tests/nenefolio_windowとも成功、子/記録0・記録エラーなし。
app-conformance-01も0違反、子/記録0で成功した。app-unit-01はsame-category reorderのfixtureで
末尾挿入番号2を誤指定し失敗した。既存契約どおり最終位置1へ訂正した。
app-build-03は追加fixtureのmalloc後にrequireだけを置いたため静的解析のNullDereferenceで失敗した。
明示的なnull分岐から既存noreturn test_failureへ進め、app-build-04は成功した。
ここまでの実行はすべて各run-dirへ入力/生出力/exitを保存し、失敗を成功へ書き換えていない。

独立読取レビューはapplicationの範囲でP2を一件確認した。prepare_corpus_entryはOOMでも位置を進め、
最初の失敗で一覧要求が取り消された後の失敗を、同sessionの語変更による再試行で飛ばせた。
app-unit-02は修正前に追加した再現条件「タグ条件→read OOM→語変更→read OOM→語変更→read成功」で
`retry cannot skip an unobserved matching source after read failure`へ到達して失敗した。
共有readerの次位置はREADY時だけ進めるように修正した。通常の読取不能は観測済みREADYなので消費する。
原文200000bytesから4bytesへの差替え中の旧matcher失効、保存世代、無題不変、条件の独立性も対象fixtureへ加えた。
準備/失敗件数は一件の観測時に更新し、summaryごとに全件を再走査しない。app-build-05から修正後を確認する。

この段階の読取失敗: src/adapters/win32/ui.c、ui_text_key.h、別ファイルと誤認したtag_*_state_tests.c、
record-run.ps1、request.json、PowerShellでのtests/unit/*state*tests.c glob。rgで実ファイルとrecord-run.py/run.jsonを確認して継続した。
一度のapply_patchは整形後の文脈不一致で適用前に停止し、実文脈を読んで再適用した。

app-build-05/06/07は子/記録0・エラーなし。app-unit-03は位置再開修正後の対象を通過した。
app-legacy-01 `--bounded-index-matching` とapp-tags-01 `--tag-management`も成功した。
measurement-build-02は正典測定buildに成功したが、measurement-unit-02は再開後の条件保持で失敗した。
同経路を通常の固定fixtureへ追加したapp-unit-04も、修正前に
`discovery read failure preserves independent list conditions`で失敗した。
原因はdiscoveryから呼ぶprepare_index_corpusの失敗後処理が独立した一覧要求/タグまで取り消す点だった。
共通のsync/次entry/readをprepare_corpus_nextへまとめ、一覧自身の失敗処理は従来wrapperに保った。
discoveryは同じ一件読込を使い、その失敗だけを自面へ返す。別reader/iteratorは増やしていない。
app-unit-05とmeasurement-build-03/measurement-unit-03は成功。後者はcoreの5/5/2点に加え、
discovery sessionの78確保境界を注入して同じownerからの再開、条件保持、非公開を確認した。
ASan/UBSan/nullabilityの非回復設定を正典測定経路で共有している。これらは実UIの検証ではない。

16:48以降は共通コマンド/操作メニュー、3言語catalog、discovery_textの表示関数、検索面のgeometryと
独立したreadonly native EDITの接続へ進んだ。この段階のUIは未完成で、build/native試験は未実行。
画面投入失敗では古いpreviewを表示せず、本文正本は既存note_excerpt/note_corpusに保つ。
読取の仮名utf16_text.h（uiではなくcore）、menu_tests.cの失敗と、UI patchの文脈/順序違いによる
適用前停止2回も記録する。いずれも実際のpath/文脈を確認して継続した。


### 17時以降 — 検索面の接続

command surfaceへDISCOVERYと専用readonly EDITを接続し、共通メニュー/パレットから開く。
入力、独立した星/タグ条件の解除、結果候補、原文/行番号/範囲、明示Open、取消、IME、UIAを追加した。
名前/原文プレビューは元のnative本文とUndoを変更せず、Openは共通switch_noteへ渡す設計である。
この段階は実装中で、native試験の成功はまだ記録していない。

ui-build-01はaccessible_item_hostの列挙追加で61行となり、C-012上限60を超えて失敗した。
同じ閉集合のhost対応を明示表へ整理し、CNF-009登録を追加した。除外・閾値・抑制は変更していない。
ui-build-02は追加UIの宣言引数不一致、stdio宣言欠落、8関数の複雑度/長さで失敗した。
操作/入力名/フォーカス/状態/readonly nativeの責務へ分けて修正した。
ui-build-03はwheelの候補移動分岐の複雑度11、ui-build-04はlive通知追加後のsurface_control_viewの複雑度で失敗した。
それぞれ候補移動の共通dispatchとstatus通知可否へ整理した。失敗したrunの子/記録exitは1/1、記録エラーなし。

独立読取レビューでvisible判定へ誤挿入されたaction分岐、native名cacheの3/4件不一致、低高でのpreview消失、
previewの共通shortcut欠落、候補行Spaceでの意図しないOpen、処理中/候補移動ごとの過剰live通知を確認した。
actionは実行側だけへ戻し、cache長を対象入力一覧から導出した。低高では見出し/範囲を1段・statusを1段にする。
readonly nativeは共通shortcut/characterを受け、矢印/scroll/copyをnativeへ残す。
Spaceはボタンfocusだけを起動し、候補はEnter/明示Openだけで確定する。
processingはlive=false、applicationのresults_revisionは集合の破棄/完成時だけ更新し、選択/preview改訂と分けた。
query/条件では古いUIA行を失効させ、同集合の候補移動ではSTATUS nodeを再生成しない。
レビューは修正後の読取で追加P1/P2なしを報告した。native通知の実測/読み上げ試験を行った報告ではない。

ui-conformance-01は0違反、ui-unit-01 `--discovery` とcommands-unit-01 `--commands-ui` は成功した。
いずれも子/記録exit0・記録エラーなし。通常buildの確保注入件数0を、前述の正典測定78点の再実行とは扱わない。
結果集合/preview世代を分離したunitもこの成功に含む。対象unitは同一binaryのlink完了確認後に実行した。
UIAの同集合世代保持/新query失効、本文/selection/Undo不変、合成IME、無題取消と保存後の宛先再解決、
最小560×360・3言語/3paletteのnative fixtureを追加し、実行前の段階である。

この区間の補助失敗: 仮名command.jsonとglob検索、存在しないui_text_numberの候補確認、
整形後の関数宣言の文脈不一致によるatomic edit停止、Python既定cp932でのUTF-8読取失敗。
後二者は書込前に停止し、明示UTF-8と実文脈へ訂正した。製品の成功検証へ数えていない。


ui-build-05は成功（子/記録0、記録エラーなし）。host-table-proof-01はコピーした表のCRLFをLFだけの
削除patternで探したためassertで失敗し、製品は変更なし。proof-02はCRLF/LFを認識し、正常→previewの
一行欠落でCNF-009一件→byte一致復元で正常を確認した。恒久登録した表検査は緩和していない。

native-discovery-01/02/03は同じpreview可視条件で失敗した。原文のUTF-16全体と36文字長は一致し、
実測DPI120、root1050×720、layer1010×625、native styleにWS_VISIBLEがあった。
原因はclipboard隔離用の非対話window stationを、このclipboard未使用のUI試験にも適用したことだった。
専用desktopは維持し、通常と同じwindow stationへ戻したnative-discovery-04は107 checksで成功した。
UIに合わせて期待値を弱めていない。プレビュー移動とquery/0件/取消で本文・selection・Undo/Redoを保持し、
UIA集合世代、合成IME、Spaceの非確定、明示Open、無題の取消と実名保存後の宛先再解決を確認した。
native-build-06/07/08は診断とfixture修正の対象buildとして成功。各rawと入力、全失敗を保存している。
物理IME・読み上げ・ユーザーclipboardは操作していない。nativeの実表示は専用desktopである。

native-visual-01は203 checks、3言語×light/dark/high contrastの最小サイズlist/preview計18BMPを採取し成功した。
実機DPI120ではWindowsの最小寸法制約で700×450px（560×360DIP）となる。PNGへ変換しJA list/EN preview/ZH HCを目視した。
英語の条件解除captionの窮屈さと、行番号が横scrollbarの高さにも描かれる点を修正対象にした。
ボタン幅を148DIPにし、英語星条件captionをClear favoritesへ短くする。gutterはnativeのEM_GETRECTの範囲へ揃える。
空の名前欄には既存のplaceholder描画経路から用途を示す。修正後のbuild/native visualは次のrunで確認する。
preview冒頭の---が見えにくい疑いは、原PNG該当領域に12dark pixelsが存在し、raw textも一致した。
現時点で描画欠落とは断定せず、無関係なpaint変更をしていない。

### 17:34以降 — 拒否境界、条件操作、native投影

ui-build-09とcommands-unit-02は成功。native-visual-02は202 checks・18BMPで成功し、
最小幅の英語解除caption、placeholder、scrollbarを除いた行番号をPNGで確認した。
後続では星と複数tagを同時に置いた最小/広幅、原文のcopyとnative失敗を追加確認する。

native-build-10〜15は成功（子exit0、記録エラーなし）。native-discovery-05は成功保存後の
期待文字列から既存blueの引用符を落としており失敗した。実ファイルは引用符を保持していたため
fixtureの期待値を訂正した。-06/-07はnative WM_SETTEXTをFALSEで拒否してもpreviewがvisible、
失敗statusが残らない点で失敗。-08の直接probeは、SetWindowTextWがこの環境ではhookのFALSEを
呼出元へ返さないことを確認した。実際のWM_SETTEXTは拒否hookへ届いていた。
-09はbuild-14のlink完了前に旧binaryを起動した診断手順の失敗で、修正後の検証へ数えない。
build-14の完了を確認してから-10を起動した。

原文の投影はSendMessageW(WM_SETTEXT)の返却値を直接判定するよう修正した。
[MicrosoftのWM_SETTEXT仕様](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-settext)は
EDITへの投入拒否をFALSEで返す。-10ではnative投入拒否→旧preview非表示/理由表示→候補再選択で復旧を確認し、
次のUIA focus試験で失敗した。共通accessible_nativeが本文/一覧入力だけをnativeとして扱っていたため、
query/置換2入力/原文previewを同じnative focus/point判定へ含めた。表示だけの別focus経路は追加していない。

native-discovery-11は220 checksで成功した。名前付きdirty本文の保存拒否時の本文/selection/Undo/履歴保持と
入口を閉じたままにすること、再試行の一回保存、Undoの保持、保存済み本文へ戻した再保存を確認した。
原文native拒否・復旧、read lockされた候補の名前一致/読取不能/開く拒否、513 UTF-16入力の上限拒否と訂正、
星+ALLの2tagsの写し、個別tag巡回/解除/全解除と一覧条件不変、UIAの原文focus・Tab退出・F1の共通helpを確認した。
既存の合成IME・本文非破壊・無題取消/保存後の所有宛先再解決も同じ実行に含む。
native-copy-01は41 checksで成功。新しい非対話window stationでclipboardを隔離し、原文全体のUnicode copyと
別描画の行番号がコピーへ混ざらないこと、scroll後もreadonly/非modifiedを確認した。ユーザーclipboardは未使用。
両runは子/記録exit0、記録エラーなし。成功後に診断用stderr出力を除き、queryのnative focusも固定する。

同区間の補助失敗: 存在しないdocs/SPEC.md（正本はrootのSPECIFICATION.md）、
終了済みclang-tidy PIDの読取と不要なeng/verify glob。これらを製品試験成功とは数えない。
FR-047、操作表、段階3a/3bの境界と#821統合済みのロードマップを更新した。保存schema/版は変更しない。

### 最終対象確認と外側計測

native-build-16〜18は対象buildに成功した。CMakeにdiscovery_native/discovery_copy_nativeを登録し、
同じprivate dataを使う既存native群とresource lockを共有する。native-ctest-01はtoolchain未読込により
ctestがPATH上に無く、child未起動・wrapper125で停止した。その結果JSONの補助読取も既定cp932で失敗した。
正典toolchainと明示UTF-8へ戻したnative-ctest-02は2/2成功、合計3.23秒（2.67/0.54秒）。
新版のquery native focusを含む。ソースの診断出力を除いた後の実行でstderrは空だった。

native-visual-03は330 checks・27BMPで成功した。最小560×360DIPのlist/previewと、1250×850pxの広幅を、
JA/EN/ZH×light/dark/high contrastで確認した。星+2tagsの個別操作領域は交差せず、広幅は結果と原文を同時表示した。
PNGで英語最小のtag名がcaption末尾に隠れる点を確認し、同じ3言語catalogで名前を先頭へ移した。
native-visual-04とcommands-unit-03は成功。英語末尾の括弧だけが狭いので不要な括弧を取り、
native-visual-05（330 checks・27BMP）とcommands-unit-04 `--commands-ui`は成功した。
英語最小の「red: remove 1/2」、JA dark preview、ZH high contrastの広幅を目視し、全画像を保存した。
文言変更後も保存/検索の実装は不変のため、既に成功したcore/application/注入/native境界結果を再利用する。
全runは子/記録exit0・記録エラーなし。存在しないeng/format.pyの確認失敗は、eng/check.ps1の正典clang-format呼出へ訂正した。

`python tools/measure-discovery.py build/verification-tags/tags_native_tests.exe`をnative-measure-01として実行し成功。
native fixtureはphase通知/ackとread件数のassertだけを持ち、時計は外側Pythonに置く。
3ノート、tag条件なし、新しい検索sessionを7回。名前完成時0read、初回preview後1、warm query2回後1、
新候補後2、取得済み候補後2を確認した。OS filesystem cacheは消しておらず、cold OS測定ではない。

| 区間（pipe・native UI・OS scheduling込み） | 最小 ms | 中央 ms | 最大 ms |
| --- | ---: | ---: | ---: |
| 面を開いて名前照合を完了 | 29.05 | 30.45 | 32.55 |
| 最初の本文取得と原文preview | 6.39 | 7.12 | 8.67 |
| 取得済みsourceを使う語変更2回 | 31.40 | 32.25 | 36.50 |
| 新候補の本文取得とpreview | 11.10 | 11.42 | 11.87 |
| 取得済み候補のpreview | 10.78 | 11.46 | 12.14 |

この計測は比較workの外の費用も含み、タイマーや全件workerを導入する根拠にはしていない。
名前照合の一idle65536 work、原文80行/16KiB、上位200表示は実時間/全保持メモリの上限とは異なる。
大量台帳のcount/確保/lookupや大本文の同期readの応答保証は未確認。

独立レビュー原本はcontract-review.md/application-review.md/ui-review.md、背景designはclaude-advice.mdとstream。
最終規約/format確認・head CI・通常exe反映・全証跡の保全/整理は後続原本へ記録する。
Issue/PRがCI・統合状態の正本、D:/NeNeFolio/agents/discovery-design-20261115/completion.jsonが反映/整理の確定原本。
物理IME、実読み上げ、全OS/DPI、一般YAML、外部同時変更の排他、cold OS cacheは確認外。
本文live grep/一致行は次の段階であり、現在の名前検索previewは本文一致座標を扱わない。
追加waiver none、既存WVR-0001継続。JSON台帳2/設定6と製品1.3.2は維持した。

final-conformance-01 `python eng/conformance.py --build-dir build`はCMake File API reply未生成で
ARC-002一件・子/記録1となった。eng/check.ps1と同じcodemodel-v2 queryを置き、configure-02で
既存Debug buildを再configureした。ゲートの変更はない。final-conformance-02は0違反で成功した。
final-format-01は変更したC/headerだけを正典clang-formatのdry-run/Werrorへ渡し、git diff --checkも成功した。
後三runは子/記録0、記録エラーなし。GitのCRLF→LF予告は差分/整形違反とは区別して記録した。
既読規則の自己確認では新ownerの範囲、閉集合分岐、唯一のmatcher/read/文書切替、入力と結果世代、
失敗の非公開、source/header配置、CNF-009表登録、ゲート無緩和を照合した。
最終headの必須CI（eng/check.ps1）はローカル対象結果とは別に、PRをReadyへ進めてから要求する。

commit前のhook確認ではpre-commit/pre-pushが存在せず、commit-msgだけが正典の件名検査を実行することを確認した。存在しない2pathの読取は失敗として記録し、hookを迂回していない。

### PR824の最初の必須CI

head b3d67b76d2747c3138a7edb253878d26dfe3820dのCI43697238758は、全targetのbuildと構成検査まで成功し、
ARC-003でapplicationの追加memchrが許可シンボルに無いとして停止した。CTest/coverage成功とは扱わない。
正典merge-01も子/記録1で停止し、統合していない。全CI log/jsonを恒久領域へ保存した。

許可表やゲートは変更せず、utf8_text_bytesの終端付き所有文字列契約を使い、strlenと所有lengthの不一致で
途中NULを拒否するよう修正した。先頭/途中/末尾の明示NUL入力3件を対象unitへ追加した。
ci-fix-build-01/02、ci-fix-symbols-01（core/application 2 library・0違反）、ci-fix-unit-01 `--discovery`は
すべて子/記録0・エラーなし。通常buildの確保注入0を、測定buildの78点の再実行とは扱わない。
語の検証だけの修正でnative投影・操作は不変。新headを確定し、必須CIを再度要求する。

### 最終CI・反映・整理（18:25確定）

最終head `a987aa3a3d40b8c26272856c46fa29c6809aa41d`のCI43697860397は成功。53 CTest、分岐9834/10828（90.820096%）。
正典merge-02は子/記録0・エラーなし。PR824はmain `c6c70e045438647815da01d3ba4de0f04a3d0008`へ統合し、C本体をff/cleanで確認した。
同headの先行Draft check赤はDraft表示で、最終Readyの成功と混同しない。

通常exeの正典反映reflection-01は子/記録0・エラーなし。反映前補助コマンドの一回目は34件snapshot後、
外部processを呼ばないPowerShell scriptにLASTEXITCODEを確認して誤停止した。反映子は未起動。snapshotを検査し補助判定を修正して反映した。
18:20:37の通常exeは1.3.2 / 1.3.2.0、SHA256 `485263b9495e9a27ea266353f2f9a890324b79d15361b2e420d09b406a45f606`。
前後の実data34件のpath/bytes/hash/作成・更新日時は一致し、起動していない。稼働中のStore版は触れていない。

全11,616ファイル・1,209,091,446 bytesを保全し、ZIP全entryを元fileへ再照合した。
preserved-task.zipのSHA256は `0ec7978d341b33e403b7e0cb347d9c8c4dd2103de3887efcd9a757ec274fabe8`。
絶対path、最終head/squash tree一致、未保存/未追跡なし、ignoredを含む全file保全、リンク/稼働参照なしを確認し、
18:24:39に823の追加worktree/outputsを整理した。branch/commitは保持。旧保持領域とPR810は未変更。
確定原本はD:/NeNeFolio/agents/discovery-design-20261115/completion.json、詳細はpost-integration.mdとcleanup-audit.json。
