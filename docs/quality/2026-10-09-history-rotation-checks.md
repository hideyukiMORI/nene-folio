# 履歴回転の失敗伝播 — #218

- 日付: 2026-10-09。基点main `4330745fc1384c256613b6c882779c62c3e1fc54`。
- 規則: FR-017、ADR0012決定2/3、ARC-001/003/007/010、C-005/008/012、QLT-008/013。Waivers: none。
- 対象: `src/adapters/win32/persistence_adapter.c` の回転結果とstore_historyへの伝播だけ。
  port/enum/永続schema/依存/ゲートは変更していない。
- 修正ソースSHA-256: `88d68df7a964ed504487dccc02aeaf73aad8bb57eccb160df114179d74577472`。
- 環境: Windows、ローカルD: NTFS、固定clang-cl/toolchain、Debug ASan/UBSan/nullability実ライブラリ。
  自作fixtureだけをD側で操作。利用者app/data、旧C作業木、過去の削除拒否対象は操作していない。

## 再現と固定後の結果

自作md=M、history1=A/2=B/3=C/4=D/5=E。2.mdをGENERIC_READ・FILE_SHARE_READ（DELETE共有なし）で保持して通常保存M→N。
修正前はarchive STORED・application READY・write_note 1回、md=Nとなり、historyは1=M/2=B/3欠番/4=C/5=D。
最古Eに加えて旧1=Aを失った。最古5.md保持でも途中失敗を無視してREADY/write1になった。

修正後は次の9fixture・16回の保存呼出がPASS。修正前2回の再現、API error分類7例は別集計。

| fixture | 結果 |
| --- | --- |
| 中間2.md保持 | HISTORY_FAILED / archive UNWRITABLE / write0。md=M、旧1=A、pending=M。3欠番/4=C/5=Dは部分回転として残る |
| 最古5.md保持 | HISTORY_FAILED / UNWRITABLE / write0。md=M、履歴1〜5全部不変、pending=M |
| 中間2.md保持→解除→同session再試行 | 初回write0、再試行READY/write1/md=N、history1=M/2=A/3=B/4欠番/5=C |
| 最古5.md保持→解除→同session再試行 | 初回write0、再試行READY/write1/md=N、history1=M/2=A/3=B/4=C/5=D |
| 最古5.mdだけREADONLY | Delete拒否を伝播。write0/md=M/履歴全部不変、pending=M |
| history0件 | READY/write1、1=M、pendingなし |
| 中間2欠番 | READY/write1、1=M/2=A/3欠番/4=C/5=D |
| 最古5欠番 | READY/write1、1=M/2=A/3=B/4=C/5=D |
| 0件から通常6回保存 M→N→O→P→Q→R→S | 6回すべてREADY、md=S、5版だけ1=R/2=Q/3=P/4=O/5=N、pendingなし |

各失敗呼出はEDITモードと最後の保存済み本文Mも保持した。渡した編集本文Nはprobe内で同じ値を再試行に用いた。
RichEditの物理操作は行っていない。applicationのarchive拒否→write0の単体正本は
`tests/unit/state_tests.c:1747 verify_history_failures`で、source不変の既存成功を再利用し、同じ内容の単体を追加しない。

## OSエラー分類と試験側の補正

同じ自作NTFS fixtureのAPI直接測定で、Delete/Moveの欠けたfile=2 (`ERROR_FILE_NOT_FOUND`)、
欠けた親=3 (`ERROR_PATH_NOT_FOUND`)、DELETE共有なしのfile=32 (`ERROR_SHARING_VIOLATION`)、
READONLYのfileをDelete=5 (`ERROR_ACCESS_DENIED`)を確認した。よって2だけを欠番として許す。
中間2.mdのREADONLY属性でMove失敗を期待した試験案はOSと不一致で、source fileのREADONLYは改名を拒まなかった。
この探索呼出はREADY/write1で正しい回転となった。試験期待の誤りとしてログを保持し、製品不具合や修正後の失敗とは扱わない。
その後、成功済み4fixtureを再実行せず、最古READONLYと残りだけを続測した。

パス生成失敗はsource静読で即UNWRITABLEを確認。pendingの名前が各番号名より長いため、現公開経路から
同じdirectoryの番号名だけを長さ超過にする安定したOS fixtureは作らず、テスト専用窓口も追加していない。

## 実行したコマンドと証跡

恒久参照: `D:/NeNeFolio/agents/218-history-rotation/`。
`probe.c`は実adapterの公開APIだけでstateを作り、archive結果を記録、write_noteを実delegateへ転送して回数を数える。
`run-probe.py`は自作入力とJSON/file内容を照合し、完了済みcaseを結果JSONから再利用する。
`api-errors.py`はWin32エラー番号を測る。追加コピーはDだけ。以下は担当worktreeで実行した。

```powershell
. ./eng/toolchain.ps1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target nenefolio_adapters
& D:/NeNeFolio/agents/218-history-rotation/build-probe.ps1
python D:/NeNeFolio/agents/218-history-rotation/run-probe.py baseline
# 修正・対象build・probe再リンクのあと
python D:/NeNeFolio/agents/218-history-rotation/run-probe.py fixed
python D:/NeNeFolio/agents/218-history-rotation/api-errors.py
```

baseline/fixed target buildとprobe linkはexit0。baseline-results.json=2再現、fixed-results.json=9fixture、api-errors.json=7例。
`baseline-probe.log`、`fixed-probe.log`（READONLY中間案の期待誤りを含む）、`fixed-probe-continued.log`、`api-errors.log`に全文がある。
固定後sourceでの最終正典gateは静読受理後に1回実施し、結果を本節へ追記する。

## 残る範囲

回転は非transaction、途中回転の巻戻し/新journal/tmp掃除は追加していない。再試行で古い版が追加で落ちる限界を実測で示した。
network/nonNTFS、電源断、容量不足、ACLだけで「削除可・Move不可」、回転中の親directory消失、パス生成失敗のfault注入、
RichEdit物理操作は未測定。外部変更検知非保証はADR0006通り。別監査の錠未取得書込・16MiB不一致はこの枝で直していない。
最新改名probeの成功はsource不変なら再実行しない。
