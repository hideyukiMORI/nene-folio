# 本文検証の出力変数を走査中に再利用する — #464

2026-10-30。基点main`d137e212486351355ddec9ee127b17aba68677c2`。
規則: ARC-001/008/012、C-004/008/012/014/016、QLT-008/010/012/013、GIT-001〜004。ADR0044。Waivers none。

## 根拠と最小変更

[#460](2026-10-30-ascii-decode.md)後の固定16MiB Large一回値は、UTF-8/NUL検証18.7385ms、Large準備30.4197msだった。
`note_text.c`のprivate validateはscalarごとにcode_pointを0へ初期化し、共通utf8_text_decodeへ渡す。
既存の計装付きRelease objectでは、zero storeのoffset0xb0へloop backが戻ることを確認した。
この当初の根拠は計装objectの構造であり、未計装製品objectの確認やCPU busyの内訳ではない。後続の実測は下記へ分ける。

採る変更は`uint32_t code_point = 0;`を同じvalidateのfor直前へ移す一箇所だけ。
成功decodeはASCII/非ASCIIとも必ず出力を上書きし、失敗0ではpointを読む前にINVALID_UTF8を返す。
初期化は残し、空入力・成功/失敗・旧反復の値の扱いを変えない。NULを見ても残りをdecodeするため、UTF8→NUL→sizeの優先を保つ。
BOM、editor改行正規化、16MiB境界、失敗out不変、確保/所有順、公開API/保存形式/版は維持する。
他consumer/editabilityを変更せず、ASCII別decoderやSIMD、header inline/LTO、新APIを加えない。

## 静読の受理

SOLがdecoderの5TU/8callsite、現validateと旧生成bodyの一致、受理済みprofile/objectを静読した。
親も22入力と追加7入力の現hash/size、元source・計装objectのrelocationとbackedge・187385ticks/10MHzを照合して受理した。
初読18:49:28 JSTと最初のhash checkpoint18:55:23 JSTは別であり、初読からの全入力不変を遡って保証しない。
終了19:03:50 JSTまで固定入力に変更検出0。調査中mainは#462の文書統合で3dbed2aからd137e212へ進んだが、製品sourceは不変だった。
原本は`D:/NeNeFolio/agents/quality-post462-validation/read-only-investigation.md`、`input-hashes.json`、`parent-research-acceptance-v1.json`。
具体的な機能不具合0で、局所的な冗長初期化の削減候補を扱う。

## 検証範囲と現在地

候補の自己レビュー後、正典Debug sanitizer/tidyと既存`--text-validation`の文字/NUL/size境界を一回確認する。
Releaseの実vector/未計装note_text objectで、初期化位置と既存decoder呼出しを確認する。
性能判断は旧#460固定Bの原本を比較根拠とし、今回Bの生成source/vector/入力・終了管理を親受理してから一回だけ観測する。
古い全source一致検査を弱めず、成功済みbaselineや無関係な回帰を再実行しない。
一箇所の移動、Debug対象検証、Release objectと固定B一回の計測・同raw監査を親受理した。最終head必須CIと統合・通常反映へ進む。

branch `refactor/464-validation-output`。作業木・原本・一時出力はD側の同task専用領域へ置く。
文書は親、局所実装・検証はSOLが担当する。通常data/save・削除拒否50pathsは操作しない。
最終head必須CI・統合・必要な通常反映・全保全・安全整理を別に確認し、確定原本を同taskの`integration-completion.json`へ収載する。

## 局所実装とDebug対象検証

候補はnote_text.cの初期化一行をloop前へ移しただけで、他の全bytesを維持した。
元source SHA256 `4b2193222afa6f1161768703e530563b1097224c6e207a9493521a418523fa9b`、
候補SHA256 `86394a1b5e18407ce2b98aa966b1ef4d6cd6e4c8d0de0577c093d88c5a6cb040`。
親は元bytesからこの単一置換だけで候補bytesが一致することと、成功/失敗の出力利用順序を確認した。
新規テスト・test selector・公開API・config変更は加えていない。
準備scriptのCRLF前提assertは書込前に止まり、実LFを維持する置換へ訂正した。製品や実buildの失敗ではない。

既存正典driverをtask path/実tool pinsへ適応し、source inventory404件と実tools/SDK/RCを固定した。
正典DebugはC23・警告error・tidy・ASan/UBSan/nullability・no-recoverを保ち、以下を各一回実行した。

| stage | 入力 / 成果物 | 結果 |
| --- | --- | --- |
| Configure configure-debug-01 | 419 / 3 | child/wrapper0・記録error0 |
| Build build-debug-01、folio_tests target | 448 / 6 | child/wrapper0・記録error0 |
| Unit unit-text-validation-01、--text-validation | 458 / 1 | text validation tests passed、stderr空、child/wrapper0・記録error0 |

全入力after不変、current/snapshot hash-size一致を親も照合した。Config metadata19entry、Build21entryとtool/SDK/RC一致を記録した。
親受理原本は`parent-candidate-review-v1.json`、`parent-debug-acceptance-v1.json`。成功済み対象を再実行せずRelease静的確認へ進む。


## Release objectと測定前の受理

正典Release Configure/Buildと静的Inspectは各一回child/wrapper0・記録error0。
入力/成果物は419/3、444/9、461/7で、全after不変・current/snapshot hash-size一致だった。
実tool/SDK/RCとmetadata19entryを照合し、Debug/Releaseのsource/config404件は旧#460からnote_text.c一件だけが異なった。
Release buildは81actions。静的Inspect内ではobjdump/nm/ninjaのversion・読取を各一回、全6callがexit0/stderr空だった。
`ninja -t commands NeNeFolio.exe`は81本のcommandを読むだけで、compiler/linkerや製品実行は無い。

未計装note_text objectは6457bytes、SHA256 `a5975dd0972e9090b0fc6e2bd7f88db9ddd530b3261b34d3d4b2dbaa37542102`。
note_text_createのout-slot rsp+0x24はoffset0x2eで一回初期化され、0x100–0x128のloopにはzero storeが無い。
0x128の72d6は0x100へ戻り、0x10dのcallは0x10eのrelocationで既存utf8_text_decodeを指す。
旧#460の計装text.objとは種類が異なるため、未計装同士の前後比較とは扱わない。新速度はまだ測っていない。
親はInspect468組と報告/静的案99identity、実disassemblyを照合して`parent-inspect-acceptance-v1.json`へ受理した。

Inspect driver v1は正典toolchainのversion用output変数とlocalが衝突し、record-run前に停止した。
v2でtaskOutputへ改名しself参照を実行中scriptへ直した。完全patch/parseを保存し、正典toolchainと成功5段を変更していない。
Inspect成功後の補助auditはNinjaとcompileDBの全文一致を誤って仮定してassert停止した。
実差は正典NinjaのMSVC依存取得用/showIncludes一個で、他引数は一致した。失敗記録を保持し、成功Inspectは再実行していない。
最終報告は`candidate-verification-report-v1.md`、入力索引は`candidate-verification-inputs-v1.json`。

次段は旧#460固定B/seedを再利用する。今回の404sourceを新たに固定し、5TU/16forward・元body/main全bytesと実vectorの由来を確認する。
生成・派生compile・nativeは静的packet受理後に進める。13契約・one-read/65536workと終了管理を保持し、baseline再実行や一般速度の保証は行わない。


## 固定Bの生成・派生compile・seed照合

旧#460から15helperを適応し、4fileは完全bytes同一、11fileはtask path・desktop名・Issue名だけを置換した。
427入力（現404source/config＋23参照）、native26入力、accounting5入力を固定した。
PS5/Python3のparse0、474identityのpacketを親が照合した。成功済みsource検査を弱めず、旧baselineの再実行は無い。

| stage | 入力 / 成果物 | 結果 |
| --- | --- | --- |
| source-generate-01 | 474 / 6 | child/wrapper0・記録error0 |
| vector-inspect-01 | 482 / 2 | child/wrapper0・記録error0 |
| compile-profile-01 | 486 / 29 | 実5compile/link/nm0、child/wrapper0・記録error0 |
| inspect-profile-text-object-01 | 6 / 0 | 固定objdump読取0、child/wrapper0・記録error0 |
| seed-copy-b-01 | 12 / 7 | exact copy照合0、child/wrapper0・記録error0 |

全current/snapshot hash-size・input after不変を親も確認した。5TU/16base bodyとmain全bytesを保持し、
旧生成5TUとの差はtask pathと受理済みtext初期化移動だけだった。現実vector447入力を固定し、
5compileは正典compileDBからsource/Fo/Fdだけを替えた。link argvも旧版からtask path以外は一致した。
実mapはwindow/state/file/text元4memberを採用せず、6つの確認対象exportは各単一owner、decoderはcanonical coreのutf8_text.c.obj一つだった。
計測exeは689664bytes、SHA256 `f95013a9e9f86d109745dfef1bc33b2708cd98bc5d39fa00c9490307390ce111`。
計装text.objも初期化0x3d一回、loop0xb0–0xd8の72d6が0xb0へ戻り、反復内zero storeは無い。

旧460恒久archiveのseed-B-v1を新taskへそのままコピーし、manifestを含む6fileの実集合と全hash/size一致を確認した。
Anchor128bytesとLarge16777216bytesの2ノートで、query qxalphaの期待IDは[1]。
コピーhelperは旧版のtask root/source archive参照だけを適応し、完全patchとparseを保持した。seedの再生成は行っていない。
親受理は`parent-static-packet-acceptance-v1.json`、`parent-generation-vector-review-v1.json`、
`parent-vector-acceptance-v1.json`、`parent-compile-record-review-v1.json`、`parent-compile-acceptance-v1.json`。
この準備時点ではnative起動・計時0。下記の最終入力を受理してからB一回を観測した。


## 固定B一回の観測・原本監査

最終570identityは親が既照合536件を再利用し新34件を照合した。7fonts・MSFTEDIT10.0.26100.9949・OS・20processor・compile環境は旧Bと一致した。
親専用slotで他のbuild/test/probe/hash/archiveを止め、未変更runnerのpreflightも既知blocker0を確認した。
`measurement-probe-record-v2.ps1 -Stage Observe -Case B -Attempt measure-b-01 -RuntimeLabel b-v1 -CompileAttempt compile-profile-01`を一回実行した。
Observeは527入力/19成果物、worker/controller/runner/記録child・wrapper0、記録error0・timeout0・所有process0・desktop終了。
原記録へ`measurement-audit-record-v2.ps1 -Case B -Attempt audit-b-01 -RuntimeLabel b-v1 -ObserveAttempt measure-b-01`を一回実行した。
Auditは549入力/1成果物、child/wrapper0・記録error0、errors0・semanticFailures0・timingAccepted=true。
親は両段の全current/snapshot hash-sizeと全after不変、実runtime/終了とraw QPC内訳を照合した。

| 固定B・ms | 旧#460受理値 | 今回#464一回値 |
| --- | ---: | ---: |
| query wall（元loop復帰まで） | 49.3964 | 47.9622 |
| Large一件の同期準備 | 30.4197 | 29.9140 |
| うちUTF-8/NUL検証 | 18.7385 | 18.0793 |
| うちOS API scope | 4.7005 | 4.7157 |
| うち複製/確保scope | 5.9178 | 5.8415 |
| うち上記以外 | 1.0629 | 1.2775 |
| 最長UI continue | 30.4207 | 29.9150 |

Largeの非重複4内訳だけを足すと一件準備になる。親のraw再計算と同raw監査が一致した。
54continue・2saved reads・matching work3355490と最終ID[1]を保持し、13契約の不一致は0だった。
未保存Anchor本文・document mode・選択・modified・実Undo/Redo、one-read/65536work、途中結果未公開と完成一回採用を維持した。
LargeのMarkdown描画0、固定seed/保存dataの照合も通り、実runtimeの7fonts/MSFTEDIT/OS/processorも旧Bと一致した。

毎反復のzero storeが無いことは実機械語で確認した。一回値は小さくなったが、速度改善幅や一般速度は確定しない。
計装付きASCII中心標本で、QPCは待機/preemptionを含み、OS cache・全OS idle・物理入力・DWM呈示完了・非ASCII速度を保証しない。
一件の同期準備は今回も約29.91ms残る。ms/倍率を合否条件にせず、旧baselineや成功済み対象検証を再実行していない。
受理原本は`parent-native-b-slot-acceptance-v1.json`、`parent-observe-b01-review-v1.json`、`parent-native-b01-acceptance-v1.json`。

## 統合へ渡す状態

製品変更はnote_text.c一箇所、文書は本記録・#462完了補記・日報・引き継ぎ・currentの5file。
公開API・schema・製品版・依存・selector・テストの追加変更0、Waivers none。
局所初期化の削減は受理したが、背景I/O・一件準備の時間上限・極狭幅表を解決済みとは扱わない。
最終headの必須`pwsh -NoProfile -File ./eng/check.ps1`成功後に統合し、通常exeの必要反映と全成果物保全・安全整理を行う。
確定head/CI/main・通常exe hash・archive/整理結果は同taskの`integration-completion.json`を正本とする。
