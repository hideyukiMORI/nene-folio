# #195 B1 — 専用 RecycleItem の port / adapter 確認

Issue: #195（親 #169）。規則: ARC-001 / ARC-002 / ARC-003 / ARC-007 / ARC-010、C-002 / C-005 / C-008 / C-011 / C-012、QLT-008 / QLT-010 / QLT-013。Waivers: none。
正典は ADR 0040 の 2026-10-08 の補正。UI と application の削除意図は B2 / B3 の範囲であり、B1 では呼び出し口だけを結ぶ。

## 実装の境界

- `trash_outcome` の 5 値と `persistence_port.trash_note(adapter, category, note)`。偽 port も同じ行に結線する。
- 錠なし、改名記録あり、記録の有無を照会できない場合は `TRASH_FAILED`。data/ を作る、錠を取得し直す、業務上の復旧を始める経路は無い。
- md の事前確認 → 履歴 → md。md が不在でも残った履歴を送り、その後 `TRASH_ABSENT`。1 対象を確認して送る `recycle_entry` は adapter 内の 1 本。
- 対象を `DELETE | FILE_READ_ATTRIBUTES`、共有なし、`OPEN_REPARSE_POINT | BACKUP_SEMANTICS` で開く。BACKUP_SEMANTICS は意図しないディレクトリも開いて種類を明示的に拒むために使う。
- md は file、履歴は directory。name surrogate と NTFS の既存判定を共有し、実体パスを取得する。UNC、固定ローカル NTFS 以外、260 文字以上の対象パス、照会不能を拒む。親 junction と subst は実体へ解く。
- Shell への移動は `ITransferSource::RecycleItem(TSF_NORMAL)` だけ。親から `BHID_Transfer / IID_ITransferSource` を得て、`FOLDERID_RecycleBinFolder` を宛先にする。
- 完了候補は `S_OK` と実測した `COPYENGINE_S_DONT_PROCESS_CHILDREN`、かつ移動後の item が存在する場合だけ。`S_FALSE`、未処理、保留、将来の通知値を成功に数えない。元の残存は HRESULT だけから断言しない。
- 初期 open の sharing violation だけを `BUSY` とする。履歴を送った後の md 失敗は `FAILED`。専用 API の明示的なごみ箱非対応は `UNAVAILABLE`。通常削除への代替は無い。
- COM は STA 初期化の `S_OK / S_FALSE` 分を、参照解放後に uninitialize。異なる apartment は `UNAVAILABLE`。shell32 / ole32 / uuid は adapters の `nenefolio_system_link` に閉じる。

## 実 adapter の隔離 probe

環境: Windows 11 Pro 10.0.26300、D: fixed NTFS、clang-cl 19.1.5。既存 OS 設定と既存 localhost D$ を使い、設定変更・共有作成は行っていない。
製品 `nenefolio_adapters.lib / nenefolio_application.lib / nenefolio_core.lib` をリンクし、公開 `persistence_adapter_create / persistence_adapter_port` から呼ぶ。
対象は `D:/NeNeFolio/design/2026-10-08/trash-adapter-probe/sandbox/` に作ったダミーだけ。絶対パスとハンドルの実体を検査して、範囲外なら操作しない。

| 場面 | 結果 | 観測 |
| --- | --- | --- |
| 通常 md | TRASHED | 自作ごみ箱項目と元パスを照合、本文一致 |
| md と履歴 2 版 | TRASHED | md と履歴の各項目を照合、全本文一致 |
| md 不在・履歴あり | ABSENT | 履歴が移り、全本文一致 |
| 両方不在 | ABSENT | ごみ箱の追加なし |
| md の他ハンドル | BUSY | md・履歴は残存し本文不変 |
| 履歴自身の他ハンドル | BUSY | md・履歴は残存し本文不変 |
| 履歴の子の他ハンドル | FAILED | md・履歴の全本文不変 |
| md と履歴の read-only 属性 | TRASHED | BUSY と取り違えず移動、全本文一致 |
| md が directory | UNAVAILABLE | その directory と履歴を保持 |
| 履歴が file | UNAVAILABLE | md と意図しない file の本文不変 |
| adapter 作成後に改名記録を公開 | FAILED | 毎回の確認で拒否、md・履歴不変 |
| 既存 MTA | UNAVAILABLE | md・履歴不変、外側の MTA を保持 |
| 既存 STA | TRASHED | 外側の STA を保持、md 本文一致 |
| 264 文字の md | UNAVAILABLE | md・履歴の本文不変 |
| 短い履歴の 400 文字の子 | TRASHED | 未知の長い子を含め全本文の SHA256 一致 |
| 親 category が junction | TRASHED | sandbox 内の実体 md が移り、本文一致 |
| 履歴自身が junction | UNAVAILABLE | link・link 先本文・md を保持 |
| md の位置が junction | UNAVAILABLE | link と履歴を保持 |
| UNC 配置から呼ぶ | UNAVAILABLE | md・履歴の本文不変 |
| subst 配置から呼ぶ | TRASHED | D: 実体の md と履歴が移り、本文一致 |
| data/ 不在で錠なし | FAILED | data/ を新設しない |

結果: 21 / 21 PASS。成功項目は今回追加された `$I` の元パスで同定し、対応する `$R` の本文を SHA256 で照合した。
21 場面の呼び出し中の隠し窓 WM_APP / WM_TIMER は各 0 回。別の Windows 版や将来の配送が無いという保証にはしない。
Shell を呼んだ後に implicit MTA（APTTYPE_MTA / APTTYPEQUALIFIER_IMPLICIT_MTA）が見える場面があった。
そのため「呼び出し後は COM 未初期化のはず」という probe の仮定は撤回し、外側の STA / MTA の維持と adapter 内の初期化・参照解放・uninitialize の対応を確認した。

コマンドと証跡:

- `pwsh -NoProfile -File D:/NeNeFolio/design/2026-10-08/trash-adapter-probe/build.ps1`: 対象 Debug ビルドと probe コンパイル、exit 0。
- `pwsh -NoProfile -File D:/NeNeFolio/design/2026-10-08/trash-adapter-probe/run.ps1`: 21 / 21 PASS、exit 0。成功済みの場面は結果を保存して再利用。
- `verify-refusals.ps1`: 拒否したダミーの残存・本文・link と錠なし data/ 不在を読み取りで確認、33 / 33 PASS、exit 0。
- `pwsh -NoProfile -File ./eng/check.ps1`: exit 0。conformance 0、symbols 2 libraries / 0 violations、ctest 2 / 2、分岐 2859 / 3062 = 93.3703%、実ツールの反例 19 件。
- `D:/NeNeFolio/agents/195-adapter/`: build.log / probe-build.log / probe.log / gate.log。
- `D:/NeNeFolio/design/2026-10-08/trash-adapter-probe/`: source / build.ps1 / run.ps1 / 各場面 log / results.json / residue.json / report.md。

probe 初回の COM 未初期化仮定、PS の配列表現、長いパスを照会する probe 側の拡張パス、錠なし場面の probe スタック容量を修正した。製品の失敗を期待値変更で成功にしていない。
初回の md が実際に移った項目も回復照合し、同じ sandbox の元パスと内容を検査して残骸一覧へ含めた。
`cleanup.ps1` は絶対パス・実体・リンク先・証跡保存・非稼働を事前検査してから、sandbox の自作 98 項目と object を非再帰の個別操作で整理した（exit 0）。
source、script、log、report、hash、cleanup manifest を保存。今回の自作ごみ箱 13 項目は `residue.json` に元パスと `$I / $R` を保存して残した。既存ごみ箱項目は変更していない。

## 未測定と限界

OS ごみ箱無効設定・容量超過・別 Windows 版・removable / 他形式媒体・ACL による照会拒否・md 本体の file symlink・処理中の対象差し替えは未測定。
md の位置の junction は directory と link の両境界、履歴自身の junction は directory の name surrogate 拒否を測っている。
事前ハンドルを閉じてから Shell が開くまでの競合、2 対象の非原子性、履歴成功後の md 失敗、別々の復元、トップレベルの長いパス拒否は ADR 0040 の受理済みの限界。
通常削除 API を呼ばない構造は確認した。すべての OS 条件で元の残存を測定したとは扱わない。

参照: [RecycleItem の戻り値と移動後 item](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itransfersource-recycleitem)。
