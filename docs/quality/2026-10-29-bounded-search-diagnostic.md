# #444 bounded検索の比較work診断

2026-10-29。base main `89b58732a726cab6918b650cf352f6a9caae79cb`。
本記録のagents444は`D:/NeNeFolio/agents/444-bounded-search-diagnostic/`を指す。
製品を変更せず、#428後の同じcore matcherを合成入力で一回診断した。親受理は`agents444/parent-build-acceptance.json`と`parent-native-acceptance.json`。
ARC-001/004/005/007/010/012、C-002/003/004/007/012/013/016、QLT-008/012/013。Waivers none。公開API/schema/ADR/通常exeは変更しない。

## 結果

一回のload auditがREJECTになり、起動前にWORK_ONLYへ固定した。同じprobeを一回だけ実行し、6case×予算4096/65536の12rowsで既知集合、全stepの予算/進捗/readiness、総workの予算独立性が成功した。
native/supervisor/record wrapperは全て0、timeout/CAPPEDなし。記録input1306・artifact19、全after不変、recordingErrors0。
診断clockのcounterReads/frequencyReadsはともに0、frequency/各interval/step集計/maxCallはnull。**検索の時間値は取得も採用もしていない。**

| case | count | actual総work（両予算同じ） | core step calls:4096 | core step calls:65536 |
|---|---:|---:|---:|---:|
| single_absent、N65536 | 0 | 65538 | 17 | 2 |
| phrase_tail_hit、N65536 | 1 | 5058 | 2 | 1 |
| periodic、N4096/m17 | 0 | 73442 | 18 | 2 |
| periodic、N16384/m17 | 0 | 294626 | 72 | 5 |
| periodic、N4096/m257 | 0 | 990722 | 242 | 16 |
| periodic、N16384/m257 | 0 | 4161026 | 1016 | 64 |

全caseの入力entry数は1（結果countは表のとおり）、ref(0,0)、name空、生成済み不変body/termを全stepまで借用する。
単文字はbody=x^N/term=z。phraseは末尾13bytesだけNeEdLe PhRaSe、term=needle phrase。periodicはbody=a^N/term=a^(m−2)+b+aで全て非一致。
stepはMORE=>NOT_READY/残work0、最後のALL_DONE=>READYを確認した。READY後だけcount/refを読み、finish後filterのcount/note/categoryも照合した。各caseで両予算のWと集合は一致した。

periodicの読取式はentry開始1+name NUL1、各候補tail/head各1とfull m−1、最後は同tickでsettleするため`W=2+(N−m+1)(m+1)`。
今回4条件のactual Wはこの事前式と一致する。候補ごとのlate mismatchでO(Nm)の総比較workが残り得る構造と、各stepを有限workへ切る契約は別の問題である。
この表は現在の整数work証拠で、旧matching-probe候補の時間/速度値を流用していない。

## loadと時刻境界

auditは一回10.2671577秒。外部CPU合計2.765625秒、compute busy0、unknown16で、合計CPU政策上限（区間の10%）を超えてREJECTとなった。unknownも残し静穏とは呼ばない。
同じWin32_Process objectのCreationDate/KernelModeTime/UserModeTime rawを保存した。Units qualifierの100nsを根拠に換算し、本文のmilliseconds表記との不一致を踏まえraw uint64も残す。両CPU0/欠測/消失はUNKNOWN、Idle PID0は実行CPUではないため集計除外。2snapshot間だけで生起・消失するprocessは観測できない。[Win32_Process一次資料](https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-process)
WORK_ONLYではpost計時auditを追加せず、同じfixture/oracle/orderの単一nativeを実行した。追加poll/retry/mode変更はない。

時計TUはD側診断`source/adapters/win32/diag_clock.c`のみ。TIMEDならcreate_total（UTF8検証+確保+prepare）、各step、finishを挟む設計だが、今回は未計時。
この設計の値もwrapper/bookkeeping overheadを含むpublic-call周辺Debug観測で、各内部関数だけの厳密時間ではない。[QPC一次資料](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)
診断のclock0という主張はnative診断adapterに限り、外側audit/timeout管理やASan/CRT/OS内部の時刻利用へ広げない。

外側はspawnしたPopenの元handleだけを保持し、GetProcessTimesの実creation FILETIMEとlaunch観測時刻を分離した。今回PID3896・creation FILETIME134377525027669904、正常終了でkillは行っていない。[GetProcessTimes一次資料](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes)
30秒外側timeoutや5M work/row、TIMED安全capはCAPPED/partialであり、正しさ/性能合否へ写さない。kill時の未flush進行row/sampleは回収保証せず、保存fixture/完結JSON行の範囲をpartialとする。追加native/per-step I/Oはしない。

## buildと同一性

恒久agents442/output-archiveのDebug coreを再利用した。395repo入力のsnapshot/main/444worktree対応、core48TU/正典sanitizer_vector、tool6/runtime4/proof3 artifactをread/hashで照合した。
core SHA256は`156087F94FA950A2E56BD2F623426F6176443506FC080B24BD7C817026484C90`、新core/proof/configure/unit再実行0。
新診断4TUだけを正典DebugのC23/MT/ASan/UBSan/nullability/no-recover/warnings/tidy条件でcompileし、保存Debug linkflags/runtimeへlinkした。source/include/Fo/Fd/config-rootだけの導出を保全。
Pythonが直接捕捉したのはCMake __run_co_compile wrapper4＋lld-link1のactual commandLine/cwdで、CMake内部clang/tidy child個別の直接観測とは呼ばない。

| record | child/wrapper | inputs/artifacts | 結果 |
|---|---|---|---|
| source-format-01 | 0/0 | 18/10 | 整形前copyをinput、live10sourceをartifact |
| build-01 | 0/0 | 1275/16 | 5direct launches各0、個別raw10空、PDB含むlink成功 |
| native-01 | 0/0 | 1306/19 | 一audit→固定WORK_ONLY一native、12rows成功 |

上表はrecordingErrors0・全input after不変。exe SHA256は`D06B632F15A9014842A6E1B337C9C8D909A0D797CDF899FA7BA710F2C321E729`。
生のnative stdout21eventsは6fixtureの実bytes hex、12rows、clock2、suite1。外側validatorが実bytes/hashと事前構成、集合/work/予算順/clock数/nullを独立照合した。汎用matcherを別実装していない。
stderrの既知`interception_win: unhandled instruction`一行を原本保持する。成功は明示oracle/flags/既存proofの範囲だけで、全sanitizer正常/一般memory安全性の証明とはしない。

## 限界・補助訂正・保持

- 時間値/Release比較/入力応答改善率/最悪ms/UI message latencyは未取得。語変換、O(categories)初期集計、lookup、同期修復、OS read、reveal/layoutはこのcore診断の外。
- m257は公開core APIのASCII入力で、現UIのcapacity128・EM_LIMITTEXT127から同じ257 ASCII文字を再現した証拠ではない。m17も今回はUI操作していない。
- source/core/API/通常exeの変更や、新しいmatching最適化の採用はない。SDK/system libraries全bytesは保全していない。
- 親のread helper初回はconformance import用sys.path不足でModuleNotFoundError1、初稿/rawを保存してrepo/eng追加の読取0へ訂正。自己apply_patchは同一path Delete/Addの一括指定を検証拒否（変更なし）、旧draft保持後に別操作へ訂正した。compile/native失敗へ混同しない。
- plan-v2に残る未compile/未実施欄は当初時点であり、実状態はbuild/nativeの受理へ置く。計画、各受理、source versions/移設/formatを別hashで保持し、過去値へ新sourceを付け替えない。

source/evidenceはagents444、build/runはoutputs444。build原本1295files/64322604bytes、native原本1329files/66102832bytesを全copyhash照合し恒久保全した。
品質初稿とfinal facts/commands/results/hashを親へ渡し停止する。追加compile/proof/unit/audit/nativeは行わない。
通常data/Nib/拒否43paths不操作。親が最終文書検査/CI/統合/通常hash維持/全hash保全/dirtyignored/link/稼働参照監査後に新taskだけ安全整理し、branch/commitを保持する。
