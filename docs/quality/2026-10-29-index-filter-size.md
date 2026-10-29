# index_filterの終端付き件数 — 2026-10-29

Issue: [#418](https://github.com/hideyukiMORI/nene-folio/issues/418)。基点main
`de2a53ac61b5db26932976e30a6df57c47b4b7d5`。
規則: ARC-001/004/008/010/012、C-002/005/008/011/016/017、QLT-001/007/008/012、GIT-001〜004。
既存OOM/out保持契約の回復であり、新ADR・API・enum・依存・schema・製品上限・ゲート変更なし。Waivers: none。

## 拒否不足と変更

`index_filter_create`は公開queryのcountから2本のsize_t配列を
`(count + 1) * sizeof(size_t)`で確保していた。加算/積の表現可能性を調べないため、
不整合な借用spanの宣言countで式がwrapし、過小確保後のentry走査へ進み得る。
通常UIの実ledgerから巨大countが自然に作られた再現ではない。
pointerの割当サイズとcountの整合性を一般に検証する修正でもない。

既存のNO_TERM→MALFORMEDの順を保ち、その直後に
`count >= SIZE_MAX / sizeof(size_t)`を既存OUT_OF_MEMORYで拒否する。
`stdint.h`を明示し、owner確保・加算・積・entry参照前に戻ってoutを保持する。
size_t配列2本の条件は同じ一箇所で守る。初期化/nullable所有/cleanup/一致順は変更しない。
headerへ結果優先順位・容量拒否・out不変を注記した。

配列容量の許可条件は`count + 1 <= floor(SIZE_MAX / sizeof(size_t))`なので、
上のguardは最大fitの一つ手前を拒むoff-by-oneではなく、終端1要素を含めた境界になる。
今回の64bit環境では最初の拒否countが2305843009213693951、最大fitは2305843009213693950。
`index_filter_removed`のcountはopaque owner内で元query count以下に保たれるため、
createの構築不変条件で同じ配列算術範囲に収まる。private ownerの偽造や公開テスト窓口を作らない。

## 安全な限定red

`eng/coverage.py`の`sanitizer_vector`で正典Debug compile_commandsの全計装対象から
一致するASan/UBSan/nullability・no-recover-all vectorを取得し、同じ`build_measurement`で作成した。
旧sourceのredはこの計装exeだけで実行し、冒頭で既存probe_activeをassertする。
未修正sourceに巨大countを渡すcanonical CRT実行は行っていない。

実entry1個と短い有効termを用意し、宣言countだけを最初の拒否値に変える。
`fail_at(1)`が小さなownerのcallocを拒否するため、旧sourceも巨大配列確保/entry参照へ進まない。
旧結果はOOM（値3）、確保呼出し1回・配列malloc要求0bytesだった。
期待する「owner確保前拒否/calls0」との差でselector child exit1となり、期待したassertionを保存した。
wrapper exit0はredの期待照合が成功した意味であり、子exit1を成功へ付け替えない。

監査にあったfail_at(2)案は採用していない。旧factoryは配列2本の確保後にNULL判定するため、
2回目だけを拒否しても3回目の巨大確保が走り得る。red/最大fitはともにfail_at(1)だけを使う。

## 完成候補の対象検証

新scenarioは既存`size_overflow_tests.c`のprobe/helperへ合流し、全体entryと限定selectorで共用する。
実在する非空filter（ref3/7）をout sentinelとして、pointer・count・note/category所属の保持を確認する。
SIZE_MAXとsize_t配列の最初のoverflowを同じ小さなentryで拒否し、
NO_TERM/MALFORMED＋SIZE_MAXの優先順位も確認する。
最大fitはprobe有効時だけ、最初の小さなownerを拒否しcalls1/malloc0でguard通過を観測する。
canonical CRTでは最大fitをskipし、実巨大確保を試みない。

既存`run_filter_allocation_tests`を計装selectorへ含め、小さいqueryの3失敗点・out保持・同じqueryのretryを再利用した。
通常empty/nonempty・ASCII fold・UTF-8・順序・removedとfilter使用配置の意味確認は既存scenarioを使う。
`run_index_filter_tests`という共通entryへ既存filter固有scenarioだけを分け、従来の`run_filter_tests`も同じentryを呼ぶ。
corpus固有scenarioを削除/複製せず、限定selectorには全corpus/全allocation回帰を含めない。

| 対象 | 実行と結果 |
| --- | --- |
| 修正前計装 | `run-phase.ps1 -Phase red`。同Debug vector、probe active、child1/wrapper0、calls1/malloc0 |
| 完成候補Debug | `run-phase.ps1 -Phase green`内のcanonical configureと`cmake --build ... --target folio_tests -j 4`。build/tidy成功 |
| canonical対象 | `folio_tests.exe --index-filter-size`。拒否count2件はcalls0、probe inactive、最大fit0件 |
| 計装対象 | `folio_coverage.exe --index-filter-size-allocation`。拒否count2件はcalls0、最大fit1件はcalls1/malloc0、通常filterの3確保失敗点から成功まで |
| binding proof | 同vectorの必須正常例exit0、反例exit1と`_Nonnull binding to null pointer`を確認 |
| 静的確認 | 変更C/Hのformat、実CMake File API graph付きconformance、diff空白確認。結果はstatic-result.json |

実終了値は各`*-result.json`、全生値/失敗log/実コマンドは
`D:/NeNeFolio/agents/418-index-filter-size/`へ保存する。red/green sourceとtestは
`inputs-red/`/`inputs-green/`の固定copy・hashで識別し、後のsourceへ旧結果を付け替えない。
build/runtime/profileは`D:/NeNeFolio/outputs/418-index-filter-size/`だけを使う。
最終結果manifestは`completion.json`、全入力/出力hashは`hashes.json`、コマンドは`commands.md`。

## 限界と引継ぎ

通常UIの巨大件数到達、applicationのtotal_notes集計/staging配列算術、全core確保監査、
検索idle化/性能、画像literal範囲は対象外。画像案は未採用のままにしている。
ASan runtimeの`interception_win: unhandled instruction`起動警告をred/green/proofログに保存した。
期待するbinding反例以外にUBSan/nullabilityエラーを通す運用へ変えていない。
local fullgate・全体coverage閾値・無関係UI/検索/OS回帰は実行しない。

対象入力不変ならレビュー/rebase/commitで成功を再実行しない。最終head必須CI、commit/push/PR、
main統合・通常exe反映と保全後の整理は親担当。通常data/Nib・削除拒否30箇所は操作していない。
未統合のworktree・出力・唯一の証拠は保持し、稼働参照と理由をcompletionへ渡す。
親が統合/反映/保全後に絶対path・取込・未保存/未追跡/ignored・唯一成果物・稼働参照・リンクを監査して整理する。
