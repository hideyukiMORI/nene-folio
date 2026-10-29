# #440 — Markdown inlineの一時領域を実token数へ合わせる

## 対象と結果

2026-10-29。Issue #440、base `f032adc2aff1f25e8c7c491de041489a677fcd15`。
ADR0070の短命inline owner内で、通常literalを連続tokenへまとめ、既存tokenize/pair/emitを維持した。
公開API/保存形式/対応構文を変更せず、第二renderer・本文copy・grow fallbackは追加していない。
ARC-001/004/005/008/010/012、C-002/004/005/008/011/012/016、QLT-008/010/012/013。Waivers none。

旧sourceはN=view.lengthに対し二配列をN+1要素ずつcallocし、通常文字も1byte/tokenだった。
同じ共用fixtureの旧赤で、短literal7bytesは4096bytes上限を満たし、長65536bytesだけが超過した。
完成候補は同じfixtureで両方56bytesの最大calloc要求となり、厳密RTF出力・追加境界・対象OOMも成功した。
これはallocatorへ渡す最大calloc要求量で、sizeof(token)、総heap/peak/RSS、速度/入力latencyの実測ではない。

| 入力 | 旧source最大calloc要求 | 完成候補 | 固定上限 | 観測 |
|---|---:|---:|---:|---|
| literal7bytes | 448 | 56 | 4096 | 両方active1、RTF全bytes/finish成功 |
| literal65536bytes | 3,670,072 | 56 | 4096 | 旧は資源assertで失敗、候補成功 |

## 実装と所有

`src/core/markdown_inline_tokens.c`の共通extentを計数と発行の2passで使う。
escapeの2byte消費（backtick前だけ1byte）、delimiterの従来の1/2幅分割、backtick/SP runは維持する。
literal境界はbackslash/backtick/star/underscore/tilde/4括弧/SP/LF。
`!`はimage_markerが元sourceの直前byteを判定するので束ね、HT/CR/UTF-8等も既存emitのまま扱う。

T=実token数、B=最長backtick runとしてtoken配列はT、code索引/整合stack共用scratchはmax(T,B+1)。
code用の長さ索引にはB+1が必要で、coherent stackはTを超えない。
空view、invalid view、N==SIZE_MAXと従来の長さ上限拒否はscanより前に残した。
extentは正かつ残りbytes以下、発行数はextent以下なのでT<=N、scratch<=N+1で、従来の上限が乗算も制限する。
二つの確保はNullable localへ受け、全成功後だけNonnull ownerへcommitし、失敗はemit前に解放して返す。
pair/emit/append_rangeの意味と元source借用を変更していない。

## 共用fixtureと検証範囲

test allocatorへ最大calloc要求bytesの観測を追加。積overflowはSIZE_MAXへ飽和して観測し、fail_atでresetする。
allocatorの実動作や既存失敗順序を変えず、製品へtest seamを加えていない。
out作成後にprobeをresetしてdirect inlineを呼ぶため、metadata callocと出力reallocを分けて観測できる。
計装有効なら最大要求が非zeroかつ4096以下であることを要求し、malloc等へ替えて観測0をinactiveへ隠せないようにした。
canonical非計装版のcalls0はinactiveと明示する。strict selectorは未計装を成功と扱わない。

- 513 paired ticks＋514 unpaired ticks（T小/B大）、1025 delimiter原文、32段coherent stack。
- 構造記号の前後literal、画像/リンク/escape、SP/LF/odd-even backslash、HT/CR、UTF-8/RTF escape。
- 空spanを含む複数借用spanのvirtual LF、table_cellの画像raw/code内escape pipe。
- 空/invalid viewとSIZE_MAX/SIZE_MAX-1の既存早期拒否。巨大sourceの実読取は行わない。
- metadata2確保の個別OOMは既存out='kept'を保持。公開factoryは既存verify_render_allocationでout sentinel保持。
  計装対象ではmetadata拒否2、factory拒否8を個別assert/表示して成功した。

通常`--markdown-rtf`とCI全件は同entryを使う。
完成時はcanonical Debug `--markdown-rtf`と測定strict `--inline-storage` / `--inline-storage-oom`を実行し、
測定`--markdown-rtf`を追加して成功済みfixtureを重ねて走らせていない。
direct renderの出力buffer OOMと、metadata OOMでの既存buffer保持、公開factoryのout保持は区別した。

## 実コマンドと終了値

この表に列記した診断/検証childは`tools/record-run.py`＋input-listで新規recordへ保存し、raw stdout/stderrとchild/wrapper終了値を分けた。
Python -B/PYTHONDONTWRITEBYTECODE、TEMP/TMPとprofile出力はD440へ限定。
初回File API queryをconfigure前に作り、同じprocessで正典toolchainを読んだ。

```text
execute-old-plan.ps1 -Phase configure
execute-old-plan.ps1 -Phase build
execute-old-plan.ps1 -Phase measurement
execute-old-plan.ps1 -Phase red
format-candidate.ps1
execute-green-plan.ps1 -Phase build
execute-green-plan.ps1 -Phase canonical
execute-green-plan.ps1 -Phase measurement
execute-green-plan.ps1 -Phase storage
execute-green-plan.ps1 -Phase oom
```

各scriptの実体と実argv/環境識別/hashは`D:/NeNeFolio/agents/440-inline-storage/`と各recordに収載する。

| Record | child/wrapper | recordingErrors | 意味 |
|---|---|---:|---|
| configure-old-01 | 0/0 | 0 | 旧sourceの正典Debug configure |
| build-old-01 | 0/0 | 0 | folio_testsのDebug/tidy、旧sourcehash固定 |
| measurement-old-01 | 0/0 | 0 | canonical builder、49source/24test、binding正0/負1 |
| red-inline-storage-01 | 1/1 | 0 | active1の長literal資源assert、意図した旧赤 |
| format-candidate-01 | 0/0 | 0 | 整形前不変snapshot→live source artifact、6files |
| build-green-01 | 0/0 | 0 | 完成候補のDebug/tidy、stderr空 |
| green-markdown-01 | 0/0 | 0 | canonical Markdown、資源/OOM非計装はinactive、stderr空 |
| measurement-green-01 | 0/0 | 0 | canonical-derived3compile/link、既存bindingproof再利用 |
| green-inline-storage-01 | 0/0 | 0 | 短/長最大要求56、厳密RTF/境界成功 |
| green-inline-storage-oom-01 | 0/0 | 0 | metadata2/factory8の拒否と保持成功 |

各runで記録inputのafterは不変。旧赤では長caseで停止したので、当時の後続overflow/reset caseは未実行だった。
そのcaseと追加green fixtureは完成候補の成功へ別に記録し、旧rawへ付け替えていない。

## 同vector・proof再利用と記録上の訂正

測定はDebug compile_commands由来のASan/UBSan/nullability/no-recover vectorを使う。
旧測定は既存`eng.coverage.sanitizer_vector/build_measurement`とbinding正負proofを一度実行した。
greenはbuilder源/在庫/版/compiler/runtime/vector/旧proof source・exe・results hashが同一であることを確認し、
旧canonical builderからの実観測とexact対応が確定したcore/test/link3argvを使った。
変更は新出力先の/Fo・/Fe・link object pathだけで、独自flagsを組まない。
既存`coverage.run`で現候補をcompile/linkし、旧proof3artifactをhash照合copyして再利用と明記した。
greenのbinding再実行は0件。成功configureもCMake/設定不変のため再利用した。

旧audit observerはWindows Popenのargs文字列を文字配列としてargvへ保存し、Noneも文字列へ変えていた。
原本は保持し、lossless連結によるcommandLineと、canonical由来expected argvのlist2cmdlineが6件とも一致することを
別reviewで機械確認した。実行コマンドの変更/再試行ではない。
green observerはstr/listとNoneをそのまま区別し、実3command lineもexact照合した。

未実行helper初稿で全.libを不変比較する案は、greenで更新するcore/appまで拒否すると静読で判明し、
clangResourceDirectory/lib/windows直下の旧4runtimeだけへ限定した。製品core/app/exeは新green inputとして保存した。
この訂正は実行失敗やgate緩和ではない。

## 限界と未実施

旧red、旧bindingproof、完成測定strict2runには既知の`interception_win: unhandled instruction`がある。
rawを保持し、対応する明示assert/vector/正負proofの範囲だけで成功を述べる。全sanitizerが一般的に正常だとは言わない。
最大calloc要求は各要求の指標であり、全heap/同時生存量/OS working setの証明ではない。
単一巨大backtickはscratchがBに比例し、構文密入力はtoken数も増える。出力RTFや既存span領域も残る。
追加2passの時間費用や速度改善率を測定していない。

native/UI、通常exe再build/起動、検索、性能計時、全local gateは実施していない。
通常data/Nibと削除拒否41pathsは不操作。公開schema/対応Markdown/API/ADR/waiver変更なし。
最終head CI/統合/通常反映/保全後の安全整理は親担当。

## 最終static

親状態docs確定後に一度まとめる。成功したbuild/unit/OOMをこの報告更新だけで再実行しない。

| 確認 | 結果 |
|---|---|
| format | format-candidate-01成功を再利用、6file hash一致。最終dry-runはQLT-012に従い省略 |
| File API conformance | conformance-final-01: child0/wrapper0、0 violations、768 inputs不変、recordingErrors0 |
| git diff --check | diff-final-01: child0/wrapper0、768 inputs不変、recordingErrors0。親handoff/reportのCRLF→LF予告warningのみ |

実行結果欄: 対象green5段と最終static完了・source/test凍結。最終docsを含む参照/UTF-8検査はFile API conformanceで成功した。ここでの結果欄だけの報告更新は実行時snapshotと別hashで保存し、成功確認を再実行しない。

## 成果物と保持

恒久原本/commands/vector/入力hash/raw/実source snapshotsは`D:/NeNeFolio/agents/440-inline-storage/`、
build/runは`D:/NeNeFolio/outputs/440-inline-storage/`。
旧core SHA256は`CC4CA7EEA492D8178D9DB67977009F823DCF00C62EFCA028AF00A7DF33029819`、
完成候補は`D488E771FFF3E1826887D30A06FF6AEE592F6804D396560B1C8AA8B032D6EDE0`。
受理候補manifestは`3634A0C0BFCC7FA792C281ED7BE2B6CE312463F64E118A91801BF0E38D313EFE`。
旧成功/失敗runと新sourceの対応を保持し、実測を新sourceへ付け替えない。
追加worktree/outputは統合・必要反映・恒久archive全hash・未保存/ignored/link/稼働参照の安全監査が済むまで保持する。
担当は削除/commit/push/PRを行わず、親が新作業物だけ整理しbranch/commitを保持する。
