# D1 #207 — 種類付き改名計画の確認（未完成・停止時点）

hideの停止準備指示により、最終ゲートと未測定のOS場面を残して保存した。
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
