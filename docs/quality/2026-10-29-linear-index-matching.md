# #446 — prefix継続とtail選択による総比較workの線形化

整形後の製品2file/unit3fileを固定し、正典Debug/tidyと限定matchingが成功した。
測定compile/linkとstrict OOMも成功した。最終File API conformanceは0違反、diff初回が検出した親文書の末尾空行2件を訂正し、diff-final-02も成功した。新native/性能計時は行わない。
受理正本は[ADR0074](../adr/0074-linear-index-matching.md)。恒久原本は `D:/NeNeFolio/agents/446-linear-index-matching`、今回の実行出力は `D:/NeNeFolio/outputs/446-linear-index-matching`。

coreの唯一のbuilderで、q0だけ既存tail skipを使い、部分一致後はprefix表を一比較/一fallbackずつ継続する。
HEAD/FULLの先頭再比較を撤去し、公開13操作・単文字・65536政策・application/UI/I/Oは変えない。
prefixは一般語/count>0だけ構築中に所有し、SEALED時にprivate destroyで解放する。
create/removedは同じ構築helperとparts_destroyを使い、nullable localの全成功後だけmemberへ渡す。

旧挙動の根拠は[既存診断](2026-10-29-bounded-search-diagnostic.md)の同source/fixture/public work/raw hash対応を確認した原本を再利用する。
N4096/m17のW73442は新上限12290を超す。これは旧挙動の既測定証拠であり、新assertを旧exeで実行したredではない。
新matchingの実workとprefix OOMの実結果は下表のとおり。

| 対象 | 状態 |
| --- | --- |
| 候補format | format-candidate-03: 15入力/5生成物、子/wrapper0、記録エラー0 |
| 正典Debug configure | configure-green-01: 406入力/4生成物、子/wrapper0、記録エラー0。File API query先置き |
| 正典Debug build/tidy | build-green-01: 420入力/3生成物、76/76完了、子/wrapper0、stderr空、記録エラー0 |
| bounded matching集合/work境界 | matching-green-01: 429入力、子/wrapper0、stderr空、記録エラー0 |
| 同Debug sanitizer測定build/proof再利用 | measurement-green-01: 456入力/15生成物、子/wrapper0、3直接compile/link exit0、記録エラー0。旧proof3生成物を再利用、新proof実行0 |
| strict bounded OOM | oom-green-01: 478入力/1profile生成物、probe active、子/wrapper0、記録エラー0 |
| 最終File API conformance | conformance-final-01: 769入力、子/wrapper0、記録エラー0、0 violation(s) |
| 最終diff | diff-final-01: 同769入力、子/wrapper2、記録エラー0。親の日報/引継ぎ末尾空行2件を訂正。diff-final-02は773入力不変、子/wrapper0、記録エラー0 |

上記成功recordの入力afterは全て不変。親がformat/configure/build/matching/measurement/OOMの全現物/snapshot hashを独立照合した。
親は最終static両recordの769入力全hashとafter不変も照合した。diff初回の指摘はhandoffs:945とreports:1282のnew blank line at EOFで、CRLF置換warningは別stderrへ保持した。
親が文書の結果追記/末尾正規化と新diff recordを担当する。成功conformance/format/build/対象試験は再実行せず、旧final-source-before-staticとinput一覧も上書きしない。
この品質記録の最終欄は検査後の報告値追記で、参照や製品/規範内容を追加していない。
親は訂正後の773入力も全hash照合した。結果欄の確定後、最終stageで12filesの範囲・sourcehash・参照不変・差分を確認する。
73対象TUのC23・W4/WX・static CRTとASan/UBSan/nullability/no-recoverをDebug compile metadataから確認した。
linkは同じflagsを文字列で渡す形式ではなく、正典targets.cmakeがlld-linkへ4runtimeとASan WHOLEARCHIVEを明示する。
実build.ninjaの経路と4lib hashを確認した。実compile child個別argvを別追跡した主張ではない。

| 空name・入力entry1のcase | 新総work | 固定oracle | 完成count |
| --- | --- | --- | --- |
| N4096/m17 periodic | 8180 | <=12290 | 0 |
| N16384/m17 periodic | 32756 | <=49154 | 0 |
| N4096/m257 periodic | 7940 | <=12290 | 0 |
| N16384/m257 periodic | 32516 | <=49154 | 0 |
| single z / x^65536 | 65538 | exact65538 | 0 |
| x^65523＋末尾NeEdLe PhRaSe | 5057 | <=5058 | 1 |

1/2/不均等/4096/65536の5scheduleで集合と総workが一致した。新ab末尾、多段fallback、source非連結、fold-prefix aAaBB/aaaaBB、UTF8、既存filter/state/size境界を同じentryで確認した。
canonicalのallocation probe inactive表示は期待どおりであり、OOMを確認済みとは扱わない。
m257は公開core範囲で、現UIの127 UTF-16単位からの再現証拠ではない。workの低下をmsや倍率へ読み替えない。

fixtureは周期4組、単文字65538、同phrase5058以下、5scheduleの集合/総work、末尾ab、重複/多段fallback、source非連結、fold/UTF8と既存明示長を共用した。
strict OOMでprefix4資源の全4失敗点、実nonnull sentinel不変/retry、partial SCAN destroy4回、prefix不要count0/SINGLE/removed各3alloc、多文字SEALED後term解放、finish retryを確認した。
通常factoryはprefixとwrapperを含む5失敗点後に成功。warm多文字prefixはcalls4/bytes16の個別失敗からpendingを停止し、APIは一回OUT_OF_MEMORY→次READY、同じ語の再要求でcount1へ復帰した。
cold preparation22失敗点、saved copy7、unchanged save loaded/unloaded6/6も既存共用entryで通過した。最大fitは小owner拒否のcalls1だけで、巨大mallocを実行していない。
固有LLVM_PROFILE_FILEとprofrawを保存した。stderrの `interception_win: unhandled instruction` 一行（PID32960）は保持し、全sanitizer正常とは扱わない。

準備中、親/独立レビューで既存bounded OOM→size→core factoryの間接呼出しを見落としていたことを再読で確認した。
未実行のfolio_tests直接追加を戻し、既存strict selectorを使う。新配線・二重fullrunは追加しない。
読取補助で不存在のCMakePresets.jsonとagents442側のexact-review名を推測したが、正典eng/check.ps1とagents440の実在原本へ訂正した。
診断/試験の失敗や成功へ読み替えない。

候補保全の初回record-runはrecords親directory未作成のため起動前validationで拒否された。
wrapper process exit1、prepare-candidate child未起動、run記録/候補snapshot未生成。outer原本を保持し、新名driver/recordで再準備を完了した。
format/build/testの失敗や旧redではない。

format起動前01はdot-sourceしたtoolchain内部$outputがdriverの出力rootを上書きし、wrapper125で非absolute input-listを拒否した。formatter child未起動。
02はtask専用変数へ改名した際にsplat参照を漏らし、PowerShell例外でwrapper自体未起動、wrapperExit null。両初稿/outer/rawとsource5不変を保存した。
定義/参照/splatをtask専用名へ揃えたv3で実formatterを一回だけ実行し成功した。

実Debug読取helperの初稿はUTF-8指定漏れによるcp932 decode失敗、v2はlinkにもcompile flagsがあるという誤oracleで停止した。
元helper/rawを保持し、UTF-8明示と正典4runtime/WHOLEARCHIVEの照合へ訂正したv3が成功した。sourceや成功buildを再実行していない。

測定案のread-only plan01は、format用manifestのclang-formatと旧proof manifestのlld-linkのtool集合差を拒否した。
plan02は専用6tool manifestでbinary path/hash/sizeが一致したが、versionのCRLF/LF表現差を拒否した。
いずれもchild1/wrapper125、proposal未生成artifact1件で、451/452入力のafterは不変。親が全hashを照合し原本を受理した。
親が発見したparse結果argumentsとargv配列の上書きはまだ未到達の静的補正としてoptions/command_argumentsへ分離した。
各compile/linkのbytes stdout/stderrと実終了値を成功失敗とも保存し、起動例外はexit nullへ分ける記録補正も同じv3へまとめた。

plan03では6tools全てのpath/hash/size、versionのCRLF→LF・末尾空白だけの表現正規化、OS表示prefix除去後の値、SDK/VC/runtime4、正典vector/73TU inventory、旧proof3artifactと旧6commandLineのguardが成功した。
clang-formatはmeasurement対象外なので元format manifestを変更せず、旧proof6toolsの専用manifestを使う。
このplan03時点では測定compile0、旧proofの新実行0、測定output directoryも未作成だった。proposal SHA256は `192CCA22EAC0DBDBC107B39AA1316D2A6056116E354F1735B6B626C0CD270FA4`。
その後親が実3argvを独立再導出し受理したmeasurement-green-01を一回実行した。実commandLine/cwdのexact一致、各終了0、raw stdout/stderrは順に0/0・0/0・213/0bytesで、失敗をdecodeで落とす経路を作らず保存した。
link stdout213bytesは親がcp932でlib/exp作成通知と再読した。表示decodeだけを訂正し、linkを再実行していない。
旧proof3copyのhash一致と同sanitizer vectorを確認した。exe SHA256は `205A1062545DD23289DD99BE010D1EDF0B47B0F663471AFEF60AF87AB0DABD50`、製品/test5fileは整形後hashのまま。
既存interception_win警告を旧proofの制約として保持し、全sanitizer正常とは主張しない。

比較workの線形上限と実時間は区別する。createの語検証/前処理・確保/解放・lookup・I/O・同期修復・UIは時間上限の外で、速度倍率や任意phraseの非退行を主張しない。
今回新native/性能計時/全件localgateは行わない。旧成功試験は関係入力の同一性が成立する範囲だけ再利用する。

規則: ARC-001/004/005/010/012、C-002/003/004/007/008/011/012/013/014/016、QLT-008/009/010/012/013。Waivers: none。
通常data/Nib/削除拒否44path不操作。新物は統合・通常反映・全hash保全とdirty/ignored/link/稼働参照の安全監査まで保持し、整理は親担当。branch/commit保持。
