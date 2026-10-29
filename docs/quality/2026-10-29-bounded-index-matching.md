# 全ノート検索の有限work継続 — 2026-10-29

Issue: [#428](https://github.com/hideyukiMORI/nene-folio/issues/428)。base main `9d611cf8be04b0d679a6d608b033b6a351e1ac0b`。
規則: ARC-001/004/005/007/010/011/012、C-002/003/004/005/007/008/011/012/013/016/017、QLT-008/009/010/012。
Waivers: none。[ADR0073](../adr/0073-bounded-index-matching.md)と先行規範更新に従う。

## 修正と範囲

warm入力と初回corpus末尾で全件を同期照合していた処理を、一つの`index_filter_build` matcherへ移した。
applicationのidleは一回65536 workまで、確定filterの保存/改名修復は同じentry解決・step helperを同期drainする。
旧`index_filter_create`/query header/全entry借用配列/独立contains実装は同じ変更で撤去した。
新core sourceのCMake列挙とAPI注記を更新し、UI/loop、port、保存schema、依存やgate閾値は変えていない。

opaque buildだけがWORKINGのphase/source・比較位置・番号列を変更し、完成wrapperがSEALED buildを所有する。
13公開操作でreadinessとREADY前提count/refを分け、partial結果は公開しない。
SEALED時にterm borrowを解除し、name/body/entry pointerはstep間に持たない。finishは失敗時owner/outを保持し、成功時だけ移譲する。
removedは旧SEALEDを変更せず新しい番号写像を作り、同じprivate wrapper化へ渡す。約2KiBのscratch残存とwrapper1確保を受け入れる。

workはentry開始、名前のNULを含む長さ確認、single/tail/head/fullの各byte比較。
無料phaseは同callで進め、正work/MOREは消費し、最後のunitでentryが完了しても同callでcursorを進める。
空category越えだけapplicationで1work消費する。最初のmatching継続でcount/build作成とworkを行い、count0は偽entryなしでfinishする。

有効語を受理するとwarmでも「準備中」の全索引となり、完成時だけ採用する。不正語/語確保OOMは先の状態を保つ。
受理後のbuilder/finish OOMは要求を停止し、全索引のまま一回通知する。旧Fへ戻すsnapshotは加えない。
mutationはrequestを残してmatching進捗を破棄し、corpus完成済みなら再readしない。新語/clear/destroyはbuild→term、完成採用はdetach→adoptの順。
#219の確定F同期修復/copy_pending/scroll・失敗保持と削除前removed予約は維持する。

## 旧source redと最初のbuild失敗

親受理の旧APIだけの`--bounded-index-warm`を一度実行した。one一件・bodyは`a`131089 bytes。
語aでcorpusを完成させread1を確認してclear後、語zのset直後pending/read1を要求した。
旧Debug configure/build/tidyはchild/wrapper0。red-warmはchild/wrapper1で、唯一のFAILは
`warm set returns pending before matching the giant single body`。後半のcontinue/clearは最初の失敗により未実行だった。
新core APIのunitを旧binaryで実行したとは扱わない。

最初のgreen buildは追加unitの`malloc`後の確認を`require`に置いたため、clang-tidyが非nullを推論できず停止した。
child1、exe未生成のartifact欠落によりwrapper125を記録した。unitはこの段階では未実行。
テスト2箇所だけを明示的な`if (term == nullptr) test_failure(...)`へ直し、noreturnを解析可能にした。productionは不変。
新しい`build-green-retry` recordで成功済みobjectを再利用し、残りのbuild/tidy/linkはchild/wrapper0となった。
旧red/失敗build/source/exeを後の試行で上書きしていない。

## 対象unitと計装OOM

全体runと新selectorは既存fixture/操作helperを共用する。新しいtest matcherや製品test seamを作っていない。

- coreは既存の具体的な一致集合、ASCII/UTF-8、名前優先、body欠損、明示長、ordered ref、removedを同じ新engineへ移行。
- work 0/1/2/不均等/65536、正MOREの消費、名前NUL、tail/head/fullの中断、entry完了残0、count0。
- entry1＋name N/NUL2＋name比較1＋body65532＝65536と一超過を手計数し、appの最初のmatching callを確認。
  先頭empty categoryを追加した場合の1workも別の期待で確認する。
- 実nonnull sentinelへのNOT_READY保持、term解放後のSEALED step/query/removed/finish、wrapper OOMから同ownerの再試行。
- **warm MORE後**の新語/不正語/clear、同長small→zmall保存、改名、削除のref移動、新規公開のcount変更。
  cold準備中の既存mutationケースだけでactive buildを確認済みとはしない。
- warm語OOM後は2回継続で消費済進捗を保持することを区別し、受理後のbuild3点/finish1点OOMは全索引へ停止/再試行する。
- ownerと番号2列の全確保、removed bridge、wrapper、partial cleanupのout不変を確認。
  #418の容量は空語→不正UTF-8→容量拒否を維持し、最大fitはprobe activeでfail_at(1)の小さいownerだけを拒否する。
  巨大実mallocや巨大ownerの偽造は行わない。#219の既存対象fixtureを共用する。

| 対象 | run / 結果 |
| --- | --- |
| 旧Debug / red | `configure-red`、`build-red-record`は0。`red-warm`は意図したchild/wrapper1 |
| 完成Debug build/tidy | `configure-green`は0。`build-green-record`はchild1/wrapper125。テスト確認分岐の修正後、`build-green-retry`は0 |
| 完成unit | `build-green/folio_tests.exe --bounded-index-matching`、`green-matching` child/wrapper0、stderrなし |
| 同vector測定build | `measurement-build-green` child/wrapper0。既存`eng.coverage.sanitizer_vector/build_measurement`を直接使用 |
| binding proof | 正例exit0、反例exit1と`_Nonnull binding to null pointer`診断。生成source/exe/command/resultsを保全 |
| 対象OOM | `folio_coverage.exe --bounded-index-matching-oom`、`oom-green` child/wrapper0。factory4点、preparation21点、saved-copy6点、unchanged-save6/6を出力 |
| 容量 | overflow2例はcalls0/malloc0、最大fit2305843009213693950はcalls1/malloc0。過大配列確保前に拒否 |
| 最終format / File API conformance | `format-final`と`conformance-final`はchild/wrapper0。16 C/Hのdry-runとFile APIを含むconformanceは0 violations |
| 最終diff | `diff-final`はchild/wrapper2（handoff末尾の新規空行）。親が当該末尾だけ修正後、`diff-final-retry`はchild/wrapper0。原本を分離保存 |

測定vectorはDebug compile_commandsから一致必須で導いた
`-fsanitize=address -fsanitize=undefined -fsanitize=nullability -fno-sanitize-recover=all`。
独自ASan-only fallback、flag無効化/除外/抑制の追加はない。
OOM stderrには既知形式`interception_win: unhandled instruction`が1行あり、原本を保持する。
対象assertの成功、同vector一致、binding正負例の実診断が今回の成功根拠であり、一般的な検出能力を広げて主張しない。

clang-cl/clang-tidy 19.1.5、CMake 3.31.6-msvc6、Ninja 1.12.1、Python3.12.10、MSVC toolset14.44.35207。
Pythonは`-B`/正典toolchainのbytecode抑止を使った。全試行は新`tools/record-run.py`でinput/artifact snapshot、raw stdout/stderr、child/wrapper exitを分離する。
OOM profileは新runtime directoryの`LLVM_PROFILE_FILE`へ出し、既存profileを上書きしない。

最終staticは親のnative受理/状態文書確定後に実行した。diffのEOF空行修正後はdiffだけを再確認し、
成功済みformat/conformance/unit/OOM/nativeを再実行していない。CRLF→LFのGit warningもraw stderrに残す。
この最終結果欄の追記だけでは成功チェックを繰り返さず、各recordと恒久agentsのresults/completionを実値の正本とする。

## native・静的確認と未実施範囲

LUNAが4caseを実行し、親が生eventと入力/生成物hashを読み取り受理した。
正典Debugのcompile/archive/linkからmain/application/adapters/windowの観測対象だけを診断コピーへ置換し、
実loop・refresh・reportとcoreを維持した。同じ保存済みexeをcaseごとの新directoryへcopyし、fontsとともにhashを照合した。
各exe隣の新dataには`a`131089 bytesの合成一件だけを作り、語aでread1を確認してから実EN_CHANGEでclear/zへ移した。
positive-work MOREの直後に制御messageをPostし、整数event順を確認した。時間/Sleep/timer/物理入力による成立判定はない。
専用desktopを使い、窓/state/adapter解放、元thread desktopへの復帰、新desktop解放まで終了条件に含めた。

| case / 実終了JST | 結果と観測 |
| --- | --- |
| normal-dispatch / 14:32:35 | 子/wrapper0、19 events。control33832の後、実WM_SIZE=5のseq7/8→marker33833のseq9/10→次step seq11。間にmatchingなし、最終count0 |
| clear / 14:33:07 | 子/wrapper0、13 events。実EN_CHANGE→cleared→直接guard確認でstep不変。以後step/READYなし |
| close / 14:33:36 | 子/wrapper0、12 events。実WM_CLOSE=16→window closed→存在するbuilder→termの順で解放→teardown。以後step/READYなし |
| native-modal / 14:34:04 | 子/wrapper0、25 events。native guard内のdialog loopへprobeを配送、idle抑止と直接guardのstep不変→実dialog close/破棄→guard解除→同entry継続→count0 |

4caseはtimeoutなし・stderr0 bytes・readCount全eventで1。clearの直接guard診断は通常scheduler呼出しとは区別した。
modalのcontrollerはUI thread・caption・class・実ownerが一致するdialogだけを閉じた。
親は72指定入力と28生成物の現物/snapshot全hash一致を確認した。`native/parent-case-record-acceptance.json`が根拠。
外側runnerの30秒timeoutは当該子processのhang保護だけで、成功判定や性能計測には使わない。

診断の初回依存buildはDevShell未読込によるRC1015で子1/wrapper125となり、正典toolchainを同process内で読む修正後のincremental実行は0だった。
初回診断compileはローカル名のshadow警告で子1/wrapper125。診断側の局所名だけを直して新stageへ生成した。
次の診断buildは子0/実exe生成だったが、未生成のcompiler PDB4件をartifactへ誤宣言したためwrapper125となった。
25指定入力と11実在artifactの全hashを親が照合し、そのexeを4caseへ使った。元エラーは残し、再buildや偽PDB作成をしていない。
`native/parent-build-record-acceptance.json`を参照する。未指定依存/環境を含む完全記録成功とは主張しない。
生成前レビューの補正・全失敗・実時刻とrun名の差は[日報](../reports/2026-10-29.md)へ残した。

local full gate、無関係全件、性能計時、物理入力は行わない。65536は政策上限であり、最適値やms保証ではない。
語変換/語前処理・O(categories)集計・確保・lookup・OS read・同期修復・reveal経由の一時drawer layout/UIA/描画費用は残る。
入力全体の同期構築が無くなった、warmの待ちが一定時間以内になった、現在headで何倍速くなったとは主張しない。

証跡は`D:/NeNeFolio/agents/428-bounded-index-matching/`のrecords、source snapshots、commands/results/hash/binding/completion。
試行原本は`D:/NeNeFolio/outputs/428-bounded-index-matching/`。未統合のsource/build/全試行を保持する。
最終headのCI・commit/PR/統合/通常反映、archive全hash照合とdirty/untracked/ignored/唯一成果物/link/稼働参照監査後の整理は親担当。
通常data/Nibと削除拒否35pathを操作せず、cleanupは再試行しない。
