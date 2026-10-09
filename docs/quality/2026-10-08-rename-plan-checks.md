# D1 #207 — 種類付き改名計画の確認

以下の最初の記録は10月8日の停止時点。10月9日の再開測定は末尾に記載する。
当時はhideの停止準備指示により、最終ゲートと未測定のOS場面を残して保存した。
**#207は未完成。Ready/mergeを行っておらず、完了を意味しない。**
基点mainは `204e21406613ed474ed68122c64287e50b8ddfb9`（C2と受理ADR0041統合後）。
規則: ARC-001/003/004/008/009/010/011、C-002/003/005/012/014、QLT-008/009/010/012/013、CNF-009/011。Waivers: none。

## 製品差分

- core: `note_rename`を`rename_plan`へ一括置換。NOTE/CATEGORYの閉じたkind、型付き台帳getter/take、全要素比較を持つ。
  `note_rename_target`はADR0041が指定する既存NOTE入力として保持した。旧opaque型・旧codec・旧portは残さない。
- category ledger: 合成`read`と最外parseの終端検査を分離。キー長も照合し、OOM/out不変を保持する。
  `renamed`は同じ位置の名前だけを変更する。既存の行コピーも同じmetadata転記関数へ集めた。
- journal: 新規はNOTE/CATEGORYとも版2だけ。版1はNOTEへ正規化して同じcodec/復旧へ渡す。
  未知版/kind、payload不一致、欠損/余剰キー、不正ID、旧名残存・新名不在/重複の完成台帳を拒否する。
- application/port/fake: `apply_rename`へ一括移行し、既存NOTEを接続。
  D1ではCATEGORY開始API/UIを公開しない。未接続CATEGORYをNOTE台帳として取得しないHALTED分岐を持つ。
- adapter: 種類別の道・本体directory・親guard・完成台帳を選び、ID/存在/置換なしハンドル移動/公開/分類/記録の読取と除去を共有する。
  CATEGORY親はdataと存在historyだけ。共通識別子検査は期待directory、name surrogate、NTFS、volume GUID照会を確認する。
- schemaの現在説明はGLOSSARYの2項目へ追随した。category/index自体の版は1。

## 実行済みの狭い確認

ログの正本は `D:/NeNeFolio/agents/207-rename-plan/`。

| 実行したコマンド・入口 | 結果 | 証跡 |
| --- | --- | --- |
| `cmake -S D:/NeNeFolio/worktrees/207-rename-plan -B D:/NeNeFolio/worktrees/207-rename-plan/build -G Ninja -DCMAKE_BUILD_TYPE=Debug` | exit0 | `configure.log` |
| `cmake --build D:/NeNeFolio/worktrees/207-rename-plan/build --target folio_tests nenefolio_adapters` | 最終対象build exit0、C23/clang-tidyを通過 | `build-target-4.log` |
| `build/folio_tests.exe --rename-d1` | exit0 / PASS | `unit-target.log` |
| 親静読のcopy統一修正後、`cmake --build .../build --target folio_tests` と同じ `--rename-d1` | exit0 / PASS | `build-review1.log` / `unit-review1.log` |
| toolchain環境で `python D:/NeNeFolio/agents/207-rename-plan/allocation-target.py` | 修正後exit0 / PASS。製品core/applicationに既存確保注入口とASanを結び、`--rename-d1`だけ実行 | `allocation-target.log` |
| `python eng/conformance.py` | 停止保存前exit0 / 0違反 | `conformance-stop.log` |
| `git diff --check` | 停止保存前exit0 | `diffcheck-stop.log` |

`--rename-d1`は既存台帳単体、改名codec/plan、既存NOTE開始/保存失敗/公開前拒否/保留/再開/強制終了/起動復旧の状態境界、改名の確保失敗だけを呼ぶ。
新規単体はCATEGORY全行の名前・順序・RGB・展開、same-index変更、型付きtake、同意図比較、長いカテゴリ名、合成readの停止位置、最外終端、v1 NOTEとv2両kindを確認した。
不正版/kind/payload/余剰・偽キー/ID/完成台帳と、失敗時out保持も対象とした。
確保注入ではNOTE/CATEGORY planとv1/v2 journalを順に失敗させ、OOMを不正へ丸めず、所有物を片付けることを確認した。

ASanランタイムは `interception_win: unhandled instruction ...` を出力した。
測定プロセスはexit0で対象テストはPASSだったが、このランタイム観測はPASSの主張と分けて残す。
今回の機能の不足と分かる根拠は見つかっておらず、無関係なランタイム調査や全再試行は行っていない。

## 製品adapterのOS測定

環境: Microsoft Windows NT `10.0.26300.0`、Dボリューム `NTFS` / label `DATA`。
製品のDebug静的ライブラリとsanitizerランタイムをリンクしたconsole probeを、
`D:/NeNeFolio/design/2026-10-08/rename-plan/fixtures/`の自作dataだけで実行した。
ユーザーdata/bin、ごみ箱、B3削除拒否済み領域は操作していない。

`pwsh -NoProfile -File D:/NeNeFolio/design/2026-10-08/rename-plan/run.ps1` と `run-stage2.ps1` はともにexit0。
**35 fixture場面PASS、54 probe呼出し（製品測定35＋復旧記録fixture作成19）**。
4つのPENDING測定は同プロセスで保持handleを閉じ、同意図RESUME→COMPLETEDも確認した。
結果の正本は同領域の `results.json` / `run-stage1.log` / `run-stage2.log` / 各場面log。

| 場面 | 数 | 結果 |
| --- | --- | --- |
| CATEGORY新規・NOTE新規の履歴なし/あり | 4 | COMPLETED |
| CATEGORY本体/履歴/完成台帳/記録を読取handleで競合させる | 4 | PENDING→同意図RESUME COMPLETED |
| CATEGORY記録公開直後・履歴移動後・本体移動後・完成台帳書込後の復旧 | 4 | COMPLETED |
| 版1 NOTE、履歴なし/ありの復旧 | 2 | COMPLETED |
| 既存記録に完全一致するSTART | 1 | COMPLETED |
| 別to・同from/toで別metadata・別kindのSTART | 3 | HALTED、記録hashと本体を保持 |
| 記録不在RESUME | 1 | HALTED、新規記録を作らない |
| 宛先本体/履歴のfile/directory衝突 | 4 | NAME_TAKEN、宛先のbytesを保持、未公開 |
| 元本体file・元履歴file・元本体不在 | 3 | UNSUPPORTED / IDENTITY_FAILED、未公開 |
| 本体/履歴の旧新両在・両無・ID不一致 | 6 | HALTED、記録hashを保持 |
| 空historyIdなのに旧/新履歴が後から現れる | 2 | HALTED、記録hashを保持 |
| 本体と履歴の移動後に実folio_stateを起動 | 1 | READY、走査前にCATEGORY復旧、カテゴリ順を保持 |

完成時はカテゴリ台帳の `A,new,C` と対象行の `#ABCDEF` / `expanded=false`、
カテゴリ内index、md、入れ子の任意file、履歴の内容を照合した。
本体読取競合のPENDINGでは履歴だけ新側、本体は旧側、記録は保持され、再試行で完了した。
台帳競合のPENDINGでは両実体は新側、記録は保持され、再試行で同じ完成台帳を確定した。
NOTEの共通ID検査変更は新規v2と既存v1復旧に直接当たるため、両方を測った。

最初の8場面の後に親静読で行コピーを同じ転記関数へ集めた。対象unit/OOMは修正後に再確認し、OS成功済み場面はその理由で再実行しなかった。
続く27場面は修正後の製品ライブラリへ再リンクしたprobeで実行した。

## 未実行・残る作業

- hide停止指示により、**最終 `eng/check.ps1`、全体coverage/negative proof/gate proofsは未実行**。数値を完了値として記載しない。
- 独立静読の最終受理と必要修正、未測定OS場面の確認、その後に最終productionでフルゲート1回。
- OS未測定: 二重錠/錠なし、junction/name surrogate、長いexe/data path、共有DELETEを許すdirectory reader、case-only、記録公開自体の失敗。
  これらの一部に対応するprobe補助はリンクbuildだけ済み、caseは開始していない。
- network/非NTFS/volume GUID照会失敗は未測定。外部shareへ接続していない。
  network shareにvolume GUIDが無いことは[Win32公式仕様](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew)の拒否根拠であり、実測成功とは扱わない。
- D2 #210の新規CATEGORY application、D3 #211のUI/文言は未着手。
- 電源断/OSクラッシュ耐久性、同期サービス競合、exeまでの祖先差替えはADR0041どおり未証明/対象外。

## 停止時の配置

worktree `D:/NeNeFolio/worktrees/207-rename-plan` / branch `feat/207-rename-plan` は未完なので残す。
ignoredは `build/`。確保測定build、probeソース/runner/log/35fixtureはDの担当agents/design領域に残す。
2026-10-08 02:32 JSTの読取では担当probe/folio_tests/allocationプロセスは0。
fixturesはfile498 / directory400 / reparse point0。削除・データ復元・既存B/Cの後片付けは行っていない。
再開時はreportのHEAD/PR状態、実行済みと未測定を確認し、成功済み場面を一括再実行しない。

日時: 2026-10-08 02:33:03 JST

## 2026-10-09 再開: 残るローカルOS境界

製品sourceは保存HEAD `4163a8e184b29a6f21befb58dd1b1ac1ea9cd208` と同一。
最新mainの停止文書のみを取り込んだ `121e8f8c208db50a7459e34f0ccd7dd6f8c6170e` について
`git diff 4163a8e HEAD -- src tests CMakeLists.txt` は空だった。静読レビューの最終sourceと同じため再利用する。
成功済み35fixture/対象単体/OOMを工程変更だけで繰り返していない。製品の追加修正は0。

同じWindows/D: NTFS環境で、製品Debugライブラリへリンクした自作probeを用い、
自作fixturesだけに**追加17場面/18呼出し（製品測定17＋記録fixture公開1）**を実行した。
合計は52場面/72呼出し（製品52＋公開20）。外部share、既存device、利用者data/binは操作していない。

| 追加場面 | 数 | 実測結果 |
| --- | --- | --- |
| NOTE/CATEGORY二重adapter | 2 | 2つ目はDATA_IN_USE、実体/台帳未変更 |
| NOTE/CATEGORY錠取得不可（錠名が自作directory） | 2 | 起動可、STARTはUNLOCKED、記録未公開/実体未変更 |
| CATEGORY記録あり錠取得不可 | 1 | adapter生成はRECOVERY_LOCKED/out null、記録SHA不変 |
| NOTE/CATEGORY共有DELETE読取handle保持 | 2 | COMPLETED、本文/内部台帳/任意入れ子/履歴bytesと台帳順/色/展開を保持 |
| NOTE/CATEGORY長いexe/data path | 2 | data root 334/330 UTF-16単位、COMPLETED、同じ保持照合 |
| NOTE/CATEGORY ASCII case-only | 2 | NAME_TAKEN、記録未公開/実体・台帳未変更 |
| CATEGORY本体/履歴/data/.history親のjunction | 4 | UNSUPPORTED、リンク先・台帳未変更、記録未公開 |
| NOTE/CATEGORY記録公開用一時名256候補を自作directoryで塞ぐ | 2 | JOURNAL_FAILED、記録未公開/実体・台帳未変更 |

入口は `pwsh -NoProfile -File D:/NeNeFolio/design/2026-10-08/rename-plan/run-stage3.ps1`。
初回3場面成功後、PowerShellが長いexeを起動できず停止。起動だけPython subprocessの明示lpApplicationNameへ変更した。
その次はprobe境界がGetModuleFileNameWのextended prefixを拒んだので、その境界だけ補正した。
製品処理へ到達前の測定補助失敗と成功を区別し、成功済み3場面を再試行していない。
残り15呼出しのrunnerはexit0。初回失敗のログも保持する。

証跡は同D領域の `run-stage3.log` / `run-stage3-resume*.log` / `results-stage3.json` / `results-stage3-first.json` / 各fixture log。
probe/build.ps1/common.ps1/launch-long.py/run-stage3.ps1を再現用に保持する。
長パスの起動制限は製品のShell上限ではなく起動補助の問題だった。
network/非NTFS/volume GUID照会不能は実測環境がなく未測定。既存volume/shareを変更せず、
networkの拒否根拠はADR0041の公式API仕様と静読として残す。電源断・同期競合・exe祖先差替えは従来どおり未証明/対象外。

最終gateは次に実行し、成功/失敗・数値を別途記載する。Draft→Ready/mergeは親担当。
Waivers: none。

## 再開の最終gate

`pwsh -NoProfile -File ./eng/check.ps1` を1回実行し、**exit0 / full gate passed**。
実行HEADは `121e8f8c208db50a7459e34f0ccd7dd6f8c6170e`（README/本品質記録だけ未commit）。
製品sourceは `4163a8e` と同一で、23ファイルの静読review hash（copy統一後を含む）も23/23一致。
対象単体/OOM・既存35OS成功記録・静読を再利用し、最終gateを本再開で初めて実行した。

- conformance: 0違反。検査自身のtest: 89/89。
- 全製品/単体Debug build: C23/clang-tidy/整形/モジュール/中核symbol検査PASS。
- CTest: 2/2。中核分岐: 3148/3362 = **93.634741%**。
- coverage反例: 7.50%を90%下限で拒否。復帰後の集計PASS。
- 実ツールgate proofs: **19**。違反を拒否し、各復帰を確認。
- whitespace: PASS。閾値/除外/抑制/依存/waiver変更なし。

ログ: `D:/NeNeFolio/agents/207-rename-plan/full-gate-resume.log` と同 `.exit.txt`。
source全SHA/静読比較は同 `source-sha256-resume.json` / `review-source-comparison.json`。
拒否fixtureの不変bytes再読取（製品操作の再実行ではない）は12場面×9内容=108件PASS、
同design領域の `verify-stage3-bytes.ps1` / `.log`。最初の検査補助はcase-insensitiveのOLDパスを
新名の存在と誤認したため、存在判定だけ修正した。本文の不変照合は同じ内容でPASS。

本D1の実装/ローカル検証は揃った。PR213はDraftのまま、親の技術受理/Ready/CI/統合が残る。
D2 #210とD3 #211は別単位であり、CATEGORY開始のapplication/UIを実装済みと扱わない。
network/非NTFS/GUID照会失敗等の未実測・耐久性の限界は上記のまま。Waivers: none。
worktree/ignored build,out/確保測定/probe fixturesは親の受理・main統合・証跡収載後に整理する。
追加4junctionは全て自作fixtures内を指し、exact paths/targetsは
`D:/NeNeFolio/agents/207-rename-plan/cleanup-links-resume.json` に保存。
branch/commitを保持する。以前削除拒否された生成物/既存C作業木は操作していない。

日時: 2026-10-09 20:03:29 JST
