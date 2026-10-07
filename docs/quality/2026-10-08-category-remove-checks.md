# #202 C1 — 空カテゴリ削除の確認

Issue: #202（親 #169、FR-036）。正典: ADR 0039 決定 14・2026-10-08 補正 5。
規則: ARC-001 / ARC-003 / ARC-010 / ARC-011、C-002 / C-003 / C-005 / C-012 / C-018、
QLT-008 / QLT-009 / QLT-010、CNF-009 / CNF-011。Waivers: none。保存 schema・依存・ゲート変更なし。
UI 入口は C2 の範囲。

## 実装と対象単体

`category_ledger_removed` は元の ledger を保ち、残る名前・色・開閉を写す。
`folio_state_delete_category` は FILTERED → 範囲 → 既存同期 → 空／無題宛先の確認 → ledger と
null 番兵つき縮小配列の確保 → typed port → categories.json → 採用の順。
削除受理と不在だけ採用する。失敗した準備は借りた note ledger を壊さずに捨てる。
別の名前付き／無題の文書は保存も再読込もせず、添字だけ詰める。

port は `enum category_remove_outcome` の 5 値。application にはカテゴリ専用の失敗 4 値を追加し、
失敗文言 3 言語と UI の分類表を結ぶ。後段失敗は履歴や index だけ処理済みの可能性を伝える。
fake の全結果、空でない／無題宛先／filter／範囲／改名同期の拒否、ledger 書出し失敗時の採用、
前後の選択・無題宛先の詰替え、編集中の本文・RTF・下見の実適用・履歴の保持、
中間／末尾／最後の見出し cursor を対象単体で確認。確保失敗の注入は測定 build で port 呼出し 0 と状態保持を確認する。

## ハンドル API の先行測定を再利用

Windows 11 Pro 10.0.26300 / 固定 NTFS の先行 probe は 18 場面・108 checks、保持 20/20。
`D:/NeNeFolio/agents/202-remove/probe-report.md` と
`D:/NeNeFolio/design/2026-10-08/category-remove-probe/` に保存済み。同じ API 試験は再実行していない。
空 directory と index、late child の非空拒否、保持中 rename/replace 拒否、reader・readonly・name surrogate、
拡張長 path、削除共有 reader の pending 契約を根拠として使う。

## 実 adapter の新しい結線確認

製品 Debug `nenefolio_adapters.lib / nenefolio_application.lib / nenefolio_core.lib` をリンクし、
公開 create / port から呼ぶ。対象は `D:/NeNeFolio/design/2026-10-08/category-remove-adapter/sandbox/` の自作ダミーだけ。
絶対パスとハンドルの実体を検査し、外なら拒否する。OS 設定・userdata・既存ごみ箱項目を変更しない。

| 場面 | port 結果と観測 |
| --- | --- |
| 空、index のみ、履歴あり | REMOVED。履歴の全本文を自作ごみ箱項目と SHA256 照合 |
| カテゴリ不在、孤立履歴のみ | ABSENT。孤立履歴も専用 recycle_entry を通る |
| 外部 file／directory | HAS_FILES。index・履歴・外部内容を保持 |
| index が directory／readonly／file symlink | FAILED。履歴は動かさず link 先も保持 |
| category が directory symlink | FAILED。link 先と履歴を保持 |
| category／index の非削除共有 reader | FAILED。index と履歴を保持 |
| history が file／非削除共有 reader／既存 MTA | HISTORY_NOT_RECYCLED。index・履歴を保持 |
| category の削除共有 reader | REMOVED。自身の close 後の名前照会は access denied、reader は有効、最後の close 後に不在 |
| index の削除共有 reader | FAILED。履歴移動・index pending、category は残る。最後の reader close 後に index は不在 |
| adapter 作成後の改名記録 | FAILED。毎回の確認で index・履歴を保持 |
| data 不在で錠なし | FAILED。data を作らない |

結果: 20/20 場面、94 checks PASS。別の読み取り保持確認 37/37 PASS、manifest 90 項目。
履歴をごみ箱へ送った自作 4 項目は元パス・$I/$R を `residue.json` に記録して残す。
`REMOVED` は OS の disposition 受理と自己 close の契約であり、全 reader の終了や即時の物理不在を保証しない。
index／category は既存 `open_entry` の FILE_SHARE_READ と DELETE | FILE_READ_ATTRIBUTES を使い、
name surrogate／種類／index readonly を履歴処理より先に拒否する。
history は B1 の専用 RecycleItem 1 本。index と空 category だけ FileDispositionInfo を使う。
再帰削除、一般削除 fallback、rollback は無い。

コマンドと証跡:

- 対象 `cmake --build .../build --target folio_tests nenefolio_adapters`: exit 0、警告／clang-tidy 通過。
- `.../build/folio_tests.exe`: exit 0。新しい状態境界と全表を確認。
- probe `build.ps1 / run.ps1 / links.ps1 / preservation.ps1`: 各 exit 0。
- `D:/NeNeFolio/agents/202-remove/`: build / unit / probe / preservation log、report.md、gate.log。
- probe 専用領域: C/PS source、各場面 log、results.json、residue.json、manifest.json。
- 最終 `pwsh -NoProfile -File ./eng/check.ps1`: 実行結果を完了時に追記する。

## 限界

履歴と index と category は原子的でなく、後段失敗時は先行処理済みの可能性がある。
別 OS・媒体・ACL・祖先 directory の差替え・全競合・クラッシュは未測定。
長い category のハンドル API は先行測定済みだが、履歴は既存 Shell の場所・長さの制限に従う。
新 UI や Undo 自体の操作は C2 の範囲。簡体中文は母語話者の確認なし。
