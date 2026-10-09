# 通常書込のdata錠所有 — #223

- 日付: 2026-10-09。基点main `be5a191948e83d095dd570c1badf26fbae30ce27`（#218統合）。
- 規則: SPECIFICATION §4、ADR0022決定3/補正、ARC-001/003/007/010、C-005/008/012、QLT-008/013。Waivers: none。
- 対象: persistence_adapterの通常7mutating port → 既存writable_data。取得後journal拒否はlocal HANDLEを閉じ、成功後だけ採用。
- 読みのport、改名/ごみ箱の型付き境界、公開port/enum/schema/依存/ゲートは変更していない。
- OS測定時ソースSHA-256: `d38f93c615ec5d50f645dbfd321c4dd7f8f0c14c449a4fd62d8fbde87f3742e1`。
- 静読受理後のコメント1行補正（open_lockの共有範囲を「起動と書込」へ）のSHA-256: `ef77ced93ebc518661eb3573d6f40c37f0423fecbc24c8c20d81c3ea70e62e74`。振る舞い同一で対象OS成功を再利用。
- Windows 11 Pro 10.0.26300、D:ローカルNTFS、固定clang-cl/toolchain、実Debug ASan/UBSan/nullability libraryを使用。

## 元の問題の成功済み再現

親が#218と同じsourceの実adapter/application probeを用い、lockfileだけREADONLYの隔離fixtureでM→N保存を測定済み。
READY/archiveSTORED/write1、md=N/history1=M。`D:/NeNeFolio/agents/2026-10-09-autonomous/write-lock-repro.json` と
`reproduce-write-lock.py` を再利用し、同じ修正前試験を再実行していない。

## 修正後のOS対象結果

10fixtureがPASS。直接通常7portへの46呼出、カテゴリ作成7呼出（合計53）、application通常保存2呼出、NOTE/CATEGORY改名各1。
probeは実portの公開APIだけを呼び、runnerが結果と全fileのSHA/集合を照合する。

| fixture | 結果 |
| --- | --- |
| lockだけREADONLY | 起動/read_note/scan_categories/settings/両台帳read成功。通常7portとcategory作成すべてUNWRITABLE、全md/history/settings/台帳/移動元先/file集合不変 |
| lockが同名directory | 同じ閲覧成功/書込拒否/全file・directory集合不変 |
| READONLY lock + application M→N | HISTORY_FAILED/archiveUNWRITABLE/write0、mdM/既存historyH/EDIT/最後の保存済本文Mを保持 |
| directory lock + application M→N | 同じHISTORY_FAILED/write0/本文・履歴・状態保持 |
| 起動時から所有済み | 通常7port/category作成すべてSTORED。archiveM→1、旧H→2、本文N・新規N・moveV・両台帳/settingsを確認 |
| data無しsettings初回write | 起動はdata生成0、明示writeでdata/lock/settingsを作りSTORED、次のcategory作成もSTORED |
| READONLY解除・2adapterの所有交代 | 先に2adapterを錠無しで生成。解除後1番settingsSTORED、2番7port全部UNWRITABLE、第3起動DATA_IN_USE。1番destroy後2番settingsSTORED |
| 遅延取得後journal在り・再試行 | 自作markerを起動後に置き、通常7port/categoryを連続2周すべてUNWRITABLE。各周の独立共有なしopen成功で取得HANDLE解放を確認。marker除去後settings/categorySTORED |
| NOTE改名・内部台帳公開 | COMPLETED、note→renamed、indexの位置、historyH追随、journal消滅を確認 |
| CATEGORY改名・内部台帳公開 | COMPLETED、A→Renamed、categories台帳、historyH追随、journal消滅を確認 |

READONLY/directory拒否fixtureではapply_renameのRENAME_UNLOCKED、trash_noteのTRASH_FAILEDも確認した。
錠無しの最初の型付き拒否だけを呼び、Shell/実ごみ箱へは進んでいない。前タスクの改名全fixtureは再実行していない。
applicationの既存archive拒否→write0単体はsource不変の成功を再利用し、同じ失敗写像の単体を追加しない。

## journal照会不能の境界

実在markerの連続拒否/HANDLE解放は上記の実OS結果。GetFileAttributes失敗は `journal_absent` がfalseになる既存のstatic確認。
自作予備fixtureでjournal自身READ_ATTRIBUTES拒否、親data READ/EXECUTE拒否を測ったが、GetFileAttributesは成功しopen_lockも成功した。
自作ACLは復元した。Delete pending journalの属性照会はERROR_FILE_NOT_FOUNDだったため、不在の試験にしかならなかった。
これらを照会不能の成功試験と呼ばない。親の判断で不安定なOS/fault注入や製品テスト窓口は追加せず、この分岐は未実測と明記する。

## 実行と再現source/log

恒久参照 `D:/NeNeFolio/agents/223-write-lock/`。probe.c、application-probe.c、build-probe.ps1、build-application-probe.ps1、run-probe.py、results.json、probe.log。
予備測定はobserve-acl.py/json/log、observe-pending-delete.py/json/log。利用者data/app、旧C作業木、過去拒否対象は操作していない。
新probeの初回compileは結果enumの綴りを誤り失敗、外部probeだけ直して再link成功。製品target buildは初回で成功した。

```powershell
. ./eng/toolchain.ps1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target nenefolio_adapters
& D:/NeNeFolio/agents/223-write-lock/build-probe.ps1
& D:/NeNeFolio/agents/223-write-lock/build-application-probe.ps1
python D:/NeNeFolio/agents/223-write-lock/run-probe.py
```

target build/clang-tidy、修正済みprobe link、10fixtureのrunnerはexit0。完了済みcaseはresults.jsonから再利用し、現fixtureを上書きしない。
再現には新しいD専用rootを使い、probeのfixture prefixとrunner ROOT、buildscriptのtaskRoot/taskProbeをそのrootへ揃える。
静読受理・コメント補正のあと、親が統合した#219のmain `9422b2403fe141277e7139adb49d1696dff70b98` へ未公開own3commitをrebaseした。
旧コミットは `chore/223-before-219-rebase` で保持。測定したadapter/core/CMake/eng/.githubに取込差分0、adapterのコメント補正後SHAも一致。
applicationは検索用cache保留を追加したが、今回の拒否2fixtureはcorpus未準備・copy_pending=falseで追加分岐を踏まず、
archive_before_store/store_editedの失敗経路も不変。実取込diffを静読してOS成功を再利用し、一律再実行していない。

最終checkpoint `4990b81dbf7237671e776a1098e8fc118361aea3` で `pwsh -NoProfile -File ./eng/check.ps1` を1回実行してexit0。
実時刻は `fullgate-meta.json`、全文 `fullgate.log`。自己試験89/89、CTest2/2、分岐3222/3438=93.717277486911%、7.33%の負例拒否、実ツールproof19。
ゲート後src/tests単体240件のSHA全一致。このあと結果の文書追記だけをcommitし、同一gate/OSを再実行しない。
coverage3JSON/proofs1JSONを `gate-evidence/` へcopyし、4/4hash一致を保存した。

## 残るもの

錠は書込可能性全体の保証ではない。取得後のfile固有権限/容量等の失敗は既存経路で扱う。
取得時journal照会失敗のOS再現、network/nonNTFS、RichEdit物理操作、電源断/容量不足は未測定。
所有済錠下の外部journal作成の検出、履歴transaction化、16MiB問題は対象外。data無しカテゴリ台帳初回writeの差分はADRへ明記し、
OSではsettings初回writeと同helper、所有済通常category台帳writeを測った。実ごみ箱を操作していない。
#218 cleanupの削除は自動承認レビューで拒否されたため保留。223は独立WT/agentsでbuildし、218build/probeへの稼働参照を持たない。
