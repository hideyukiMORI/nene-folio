# 検索用写しの同期と再試行の確認記録（#219）

Issue: [#219](https://github.com/hideyukiMORI/nene-folio/issues/219)。FR-032、ADR0024/0041の2026-10-09補正。
規則: ARC-001/003/004/009/010/011、C-002/003/005/008/012/014/017、QLT-003/004/009/010/012。Waivers: none。
基点main `4330745fc1384c256613b6c882779c62c3e1fc54`、専用branch `fix/219-index-cache-retry`、source checkpoint `6dfd325eb9648c568af4d0ffb5a46fcea0b05496`。

## 問題と修正

保存後に本文の検索用写しを複製/一致集合を再構築する確保が失敗すると、mdと所有本文は既に新本文になっている。
以前は同本文再保存がREADYのno-opとなり、旧写しを修復しなかった。語を入れ直してもloaded=trueのため旧本文が検索された。
またNOTE改名がmd移動後に保留になったとき、初回絞り込みは旧名を読み、対象の写しを欠落したままloaded=trueにした。
その後改名が完了しても写しが無いままなので、新名による検索にも対象が出なかった。

`folio_state`内の`copy_pending`1つで保存済み現文書の写し修復を保持する。
trueの条件はNAMEDかつcorpus_loaded。本文の正本は所有済みstate.body、対象は現在文書の保存先で、別の本文/名前/添字は所有しない。
既存refresh_copyが処理前に保留を立て、note_corpus_putとrefresh_filterが両方READYのときだけ解除する。
共通同期はindex修復→写し修復→改名再開。文書/保存先/台帳の添字を変える既存の意図は、この同期成功後にだけ進む。
同本文保存のequality分岐は無変更で、保留時だけ先行同期が修復する。通常の同本文保存へcache確保を増やさない。
非空filterはNOTE/CATEGORYの改名保留または写し保留のとき、語/scroll/集合変更より先に共通同期を通す。
空解除は従来どおり実行でき、解除だけで写し保留を解消したことにしない。

core/API/public enum/port/schema/UI/ゲートは変更しない。headerはset_index_filter/note_changedの説明だけ整合させた。
ADR0024/0041に契約補正を記録した。daily/handoff/current/CLAUDEは親所有で、この枝では編集していない。

## 修正前の再現（失敗を保存）

productionは基点4330745のまま、専用の小さい試験を追加して実行した。

| 実行 | 結果と観測 | 全文ログ（D:/NeNeFolio/agents/219-index-cache-retry/） |
| --- | --- | --- |
| `build/folio_tests.exe --index-cache-retry` | exit1。NOTE保留中の初回filter→完了→新名検索が0件のまま。`completed NOTE remains searchable after first filter during pending`で失敗 | unit-before.log |
| `python D:/NeNeFolio/agents/219-index-cache-retry/allocation-target.py`（core/applicationにallocation probe注入、同exe `--index-cache-retry-oom`） | exit1。write済み本文/所有本文は新内容、same-save READY・write1/archive1の後、cache一致数0。`same-body retry repairs the saved copy and match set`で失敗 | allocation-before.log |

最初の通常buildはselector追加に伴うmainのsize/complexity超過をclang-tidyが拒否した。
selectorをhelperへ集約して解消（build-before.log→build-before-2.log）。閾値/抑制は変更していない。
旧commands-ui/rename-d1/rename-d2は同じ引数・同じ試験群・同じ成功文言へ渡ることを差分静読した。旧成功試験は反復しない。

## 修正後の対象検証

| 実行 | 結果 | 全文ログ（同agents領域） |
| --- | --- | --- |
| `cmake -S D:/NeNeFolio/worktrees/219-index-cache-retry -B D:/NeNeFolio/worktrees/219-index-cache-retry/build -G Ninja -DCMAKE_BUILD_TYPE=Debug` | configure成功。固定toolchain経路を使用 | configure.log |
| `cmake --build D:/NeNeFolio/worktrees/219-index-cache-retry/build --target folio_tests` | clang-cl/clang-tidy成功。変更の対象だけをビルド | build-after-3.log |
| `build/folio_tests.exe --index-cache-retry` | exit0。NOTE初回filter、PENDING/HALTEDの語/行/scroll保持・旧名read0、空解除、filterから再開して新名一致 | unit-final.log |
| allocation-target.py→`allocation.exe --index-cache-retry-oom` | exit0。公開後のput/filter **6確保点**を1つずつ失敗させて再試行。通常same-save確保数はloaded/unloaded **6/6** | allocation-final.log |
| `git diff --check` | exit0 | checkpoint作成前の実行記録 |

確保失敗の直接確認:

- md公開後のput/filterの各失敗で保存済み本文/文書/modeを保持し、さらに修復OOMを起こしても同じ保留を保持。回復後same-saveで一致1、追加archive/write/read0。
- 空解除→非空要求はcopy修復を先に実行。再OOMなら空の語/集合を保持。回復後は所有済み本文から一致し、全corpusを再読込しない。
- 元の非空語/行/scrollを持つ場合も、copy修復失敗では新語へ置き換えない。成功後のみscroll0と新集合へ進む。
- 初回保存とEDIT別名保存は、md公開後index/cache両方の失敗を保持。UNSYNCED→index回復→copy再OOM→成功の順を測る。new md再作成・元本文write/archiveなし。
- VIEW別名保存のcache失敗は:q相当のnote_changedでも先に修復。失敗時は結果outを触らずmode維持、成功はSAME。原文を使い入力の正規化/再作成/元write/archiveを行わない。
- select/new/reorder/trashは修復再OOMでbody置換・添字変更・trash portへ進まない。回復後は古い対象の写しを直してから元の意図を実行し、検索の宛先を取り違えない。
- 正常same-saveをcacheあり/なしの両方で確保を順に失敗させ、編集本文の正規化に必要な数だけで6/6。同じ本文へ不要なcache複製を足していない。

ASanの測定exeは従来D2測定と同じ非致命の`interception_win: unhandled instruction ...`を出した。
これは観測としてログに残し、終了0と試験PASSとは別に記録する。無関係なruntime調査/全面再試行はしない。

## 最終単一ゲート

対象unit/OOM成功checkpointを親へ提出済み。親静読の受理後、最終production sourceで`pwsh -NoProfile -File ./eng/check.ps1`を1回実行し、この節へ結果を追記する。
現時点で最終ゲートは未実行。成功したsource不変の試験は、push/PR/mergeという工程だけでは反復しない。

## 境界の静読と残る範囲

body採用のrender_note/new_note/create_edited/store_edited、保存先/添字を変えるrename/move/trash/category増減の前置synchronizeを点検した。
設定/言語/テーマは派生paneを作るだけでbody/保存先を変えない。mode/cursor/本文だけの検索置換/history所有写し照会も正本を入れ替えない。
copy_pendingとrenameが併存する正常経路は無い（必要保存cache失敗ならSTART前に拒否）。既存NOTE完了時のcorpus rekey OOMの制約まで広げて直していない。

この修正は新しいWin32の面/物理入力を変更しない。adapterのOS改名途中段階、CATEGORY改名の既存成功probe、無関係な全件試験を反復しない。
外部md変更の非検知、pending NOTE履歴の旧名読込、移動履歴の孤立、U+0000のRichEdit制約、電源断/OSクラッシュ耐久性の非保証は従来どおり。
利用者app/data、旧C作業木、削除拒否対象は未操作。Waivers: none。

日時: 2026-10-09 21:27:49 JST
