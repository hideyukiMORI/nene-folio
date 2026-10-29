# #442 通常literal内部SPの解析領域

2026-10-29。base `591e5c4d585f17dc5abd32986d09c6ea387676ff`。
この記録はv2対象greenと最終static完了時点。初回green buildはC012失敗で停止し、改訂後の5段は全成功した。最終static後の結果欄更新を実行時snapshotと別hashで保存する。

## 問題と変更

#440の共通extentはSPを分離するため、空白区切りの文は単語/SPごとにtokenを持つ。
短phrase7bytesと長phrase65536bytesを同じ資源/厳密RTF oracleへ通し、旧sourceで短168bytes・長1835008bytesの最大calloc要求を実測した。
短は固定4096上限内、長だけが上限を超えた。両caseは先に正確RTF全bytesとfinishを通過している。
この値はallocatorへ渡す最大calloc要求で、sizeof(token)/全heap/peak/RSS/速度/latencyの測定ではない。

productionは`markdown_inline_tokens.c`の既存literal_boundaryと共通extentだけを変更した。
SP以外の普通文字から始まるliteralで内部SPを取り込み、LFで止まった時だけ末尾SPを戻して次のSP-start runへ渡す。
先頭SP run/escape/backtick/delimiter/括弧/LFの既存境界とpair/emit/append_rangeは維持する。
EOS前SPは保持する。view_at(N)のNULはLFではない。先頭非SPなのでrewindはstartを越えず、正長で次へ進む。
count/発行は同じextent、T<=Nとscratch=max(T,B+1)、既存scan前guard/二calloc/OOM契約を保つ。

公開API/schema/Markdown対応構文/ADR/waiverの変更はない。ARC-001/004/005/008/010/012、C-002/004/005/008/011/012/016、QLT-008/010/012/013。Waivers none。
既読7規約はf032adc→base差分0を照合して再利用し、C012を再読した。受理根拠はADR0070の短命inline owner、agents442 accepted-design/独立review/親red受理。

## 共用fixture

既存verify_inline_literal_storageへ非空固定pattern引数を足し、`a`と`abc def `を同じ観測へ渡す。
source/out作成・source finish後にprobe resetし、render→観測採取→reset→raw print→RTF全bytes/finish→資源上限という同じ経路を使う。
観測有効はcalls非zero、active時maximum非zeroかつ4096以内。strict selectorは未計装を成功扱いしない。
短7bytesは`abc def`、長65536bytesは8byte pattern反復でEOS末尾SP。両expectedはsource全bytes、末尾をtrimしない。

新phrase entryをphrase-only strict selectorと通常Markdown/既存storage suiteから共用する。
direct SP境界7case（先頭/内部/EOS、LF前1/2/3SP、backslash前SP、強調/link隣接、code trim/all-SP、image raw）と、
borrowed3case（空span含むvirtual LF、table image/code内部SPとescaped pipe）を既存expect helperへ追加した。
元allocatorと観測APIは変更しない。metadata2確保の既存buffer保持と、公開factory out sentinel保持は別の既存OOM確認を共用する。

## 実行済み記録

この表に列記した診断/検証childは新record-run/input-listへ保存し、raw stdout/stderrとchild/wrapper終了値を分けた。

| record | child/wrapper | inputs/artifacts | 結果 |
|---|---|---|---|
| configure-old-01 | 0/0 | 411/4 | 新worktree canonical Debug configure、File API query先置き |
| build-old-01 | 0/0 | 426/3 | folio_tests Debug/tidy、76/76 |
| measurement-old-01 | 0/0 | 455/6 | 正典derived3compile/link、proof reuse、新proof0 |
| red-inline-phrase-01 | 1/1 | 468/1 | 意図した長phrase資源assert、profile保存 |
| format-candidate-01 | 0/0 | 12/4 | 整形前copyをinput、live4source/testsをartifact |
| build-green-01 | 1/1 | 451/3 | extent複雑度12>10。3artifactは旧Debug cached exe/libs、新green生成物ではない |
| format-candidate-02 | 0/0 | 9/1 | v2変更productionだけ整形。不変tests3はformat01のhash一致で再利用 |
| build-green-02 | 0/0 | 452/3 | 改訂sourceの実Debug/tidy/link成功 |
| green-markdown-01 | 0/0 | 455/0 | canonical Markdown、計装inactiveを区別 |
| measurement-green-01 | 0/0 | 456/6 | 新source全compile/relink、actual3argv/cwd照合、proof再利用0new |
| green-inline-storage-01 | 0/0 | 461/1 | phrase/literal短長は最大calloc56、exact RTF/finish/追加境界成功 |
| green-inline-storage-oom-01 | 0/0 | 461/1 | metadata拒否2・factory拒否8を個別確認 |

全表記録のrecordingErrors0、input after不変。旧red rawは次のとおり。

```
INLINE_STORAGE,phrase-short,source_bytes=7,calloc_max_bytes=168,limit=4096,calls=2,active=1
INLINE_STORAGE,phrase-long,source_bytes=65536,calloc_max_bytes=1835008,limit=4096,calls=10,active=1
FAIL: literal inline metadata calloc request is observed and stays within 4096 bytes
```

旧赤はstrict selector末尾の計装require/成功printへ到達していない。一般storage境界/OOMはそのselectorの対象外で未実行。
旧source/tests/exe/rawと候補sourceを別hash/snapshotへ保全し、旧値を完成sourceの実測へ付け替えない。
raw stderrの既知`interception_win: unhandled instruction`一行を保持し、全sanitizer正常とは主張しない。

## 実vectorとproof再利用

新Debug compile_commandsの49source/24testから既存eng.coverage.sanitizer_vectorを用い、
address/undefined/nullabilityとfno-sanitize-recover=allの同一vectorを確認した。C23/MT/tidyは実commands/build.ninjaへ照合。
agents440恒久archiveの正典builder/config/tool/runtime/proof/source/vector hashと、新root相対inventoryをguardし、
既存canonical-derived3argvのsource/include/fresh出力pathだけを変換した。実3 commandLineはlist2cmdline期待およびcwdと完全一致、変更path数53/28/74。
proofのsource/exe/results3copyは旧archive hash一致、正0/反例nonzeroと_Nonnull診断を確認。copyを新proof実行と呼ばない。
通常のold440 worktree/outputは参照せず、旧path文字列は変換/照合用にだけ使った。

読取review helperの初回は`/MT`のみを期待し、実CMakeの同option `-MT`でassert失敗した。
初稿・tool-return stderr/exit1を保存し、両表記を受ける読取検査へ限定訂正。製品/flags/compileは不変、再build無し。
この読取/hash/copy helperの2試行はrecord-run外で、取得方法/原本をattempt JSONへ明記した。訂正後の読取は0。

親の失敗archive照合helperもmanifestキーをrecordsと誤ってKeyError exit1、実filesキーへ訂正し458files全hash照合したと受領した。
v2保全helper初回はsnapshot/diff保存後に既存v1 manifestをopen(x)してFileExistsError exit1。v1は上書きされず保持された。
初稿/raw/部分v2を保存し、v2新名manifestへ訂正、既存snapshot/diffは同hash/同bytesだけを再利用してretry0で確定した。
これらは読取/保全補助の失敗で、compile/test/資源赤と混同しない。品質初稿snapshot後の事実はcandidate-preservation-correction.mdと本更新で時点を区別した。

## 初回green失敗とv2整理

初稿はSP例外ORとLF if内のrewindを共通extentへ置き、実tidyが複雑度12（上限10）で拒否した。
自己reviewで既存escape内ternaryの+2を見落とした。失敗raw/sourceと451inputs/3cached artifactsを458files/37369323bytesで保全し、後続を実行しなかった。
親判断でC012を再読し、閾値だけのhelper分割を採らず、literal_boundaryを「通常literalの停止文字」としてSPを除外した。run_characterはSPを含む従来のままなので先頭SP runは維持される。
extentの走査条件を元の単純なboundary否定へ戻し、LF以外の早期return後にだけ末尾SPを戻す。共通字句経路/正長/期待/pair/emit/capacityは不変、推算複雑度10の候補はbuild-green-02の実tidyで通過した。通過時の個別scoreは出力されていない。
candidate-v1/旧helpers/失敗recordを上書きせず、candidate-v2/別v2helpers/format02/build-green-02で区別する。

## 完成greenと最終static

| 対象 | 結果 |
|---|---|
| 完成Debug/tidy | 初回C012失敗を保持、v2 build-green-02成功 |
| canonical --markdown-rtf | green-markdown-01成功、calls0はinactiveと区別 |
| same-vector measurement build | measurement-green-01成功、新proof0のreuse guard維持 |
| strict --inline-storage | 成功。phrase7/65536は最大calloc56、limit4096/active1、厳密RTF/finish一致 |
| strict --inline-storage-oom | 成功。metadata拒否2/factory拒否8を個別表示 |
| format | format-candidate-02のproduction1file成功＋format01の不変tests3hashを再利用 |
| File API conformance | conformance-final-01成功、0 violations、child/wrapper0/errors0、772 inputs不変 |
| git diff --check HEAD | diff-final-01成功、child/wrapper0/errors0、772 inputs不変。親handoff/reportのCRLF→LF予告warningのみ、書換無し |

最終staticは未追跡品質記録を含むrepository inventoryとFile API metadata、source4/docs4の最終8filesを入力に一回実行した。
format01のtests3とformat02のsource1はartifact/config/tool hash一致で成功を再利用した。ここでの結果欄だけの報告更新のために成功確認を繰り返さない。

canonicalとmeasurementはstderr空、両strictは既知interception_win一行ずつを原本保持した。成功は明示assert/vector/既存正負proofの範囲に限る。
成功済み旧configure/proofを繰り返さず、新sourceに直接関係する完成対象を一回まとめた。追加phrase-only成功run/測定Markdown重複/full localgate/native/UI/性能は実施していない。
最大backtickに比例するscratchは残る。資源改善を全heap削減率や無条件入力latency保証に広げない。

## 保全と保持

原本はagents442、build/runはoutputs442。旧source SHA256 `D488E771FFF3E1826887D30A06FF6AEE592F6804D396560B1C8AA8B032D6EDE0`、v2候補は`2CC6F01804BDB1ACC7688D25B867B554B0B63C457D17981D00128FDD0B816293`。
失敗したv1の`6C2F7FA72C50CE9915EE2B2FA318D1A17869C366B552F549754E74E99EE4F0BC`も別snapshotへ保持する。
旧debug2recordは852files/33570462bytes、旧measurementは465files/44909643bytes、旧redは473files/45420477bytesを全copyhash照合した。
詳細source/test/driver/input/vector/artifact/raw/exitは同task manifestsと各recordにある。SDK/system libsは環境依存で全bytesを保全していない。
通常data/Nib/拒否42paths不操作。担当はcommit/push/PR/通常反映/cleanupを行わない。
統合・必要反映・全hash保全・dirty/ignored/link/稼働参照監査までは新作業物を保持し、親が安全整理、branch/commit保持を担当する。
