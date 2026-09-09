# ADR 0007 — ドロップ先は core が決め、並び替えの意図で台帳を作り直して categories.json / index.json を書き戻す

- 状態: 受理
- 日付: 2026-09-10
- Issue: #13
- 影響する規則: ARC-002 / ARC-003 / ARC-004 / ARC-005 / ARC-007 / ARC-009 / ARC-010 / ARC-011 / C-002 / C-003 / C-005 / C-006 / C-012 / CNF-002 / QLT-009 / QLT-013

## 文脈

FR-009 は「索引はドラッグで表示順を変えられる（カテゴリ行同士・同じカテゴリ内のノート同士）」と定め、別カテゴリへの移動は
初版では扱わない（SPECIFICATION 第 2 節）。FR-008 の `index.json` はこれまで読むだけで、書き戻しは「保存を持つ縦切りで」と
先送りしてきた（ADR 0004）。SPECIFICATION 第 4 節は「ヒットテストとドロップ先は core の純関数」、ARC-004 は「順序は application の
reducer が変え、ドラッグ中の一時状態は ui/win32 が持つ」と定めている。

ドラッグの表現はデザイン正本（キャンバス「採用: 案2 堅」）に無い。現行のドロワーは `WM_LBUTTONDOWN` でクリックを確定しており、
押下と離しを区別していない。台帳は `category_ledger_toggled` のように「変更した新しい台帳を作り、書き戻せたら差し替える」形で
扱っている（#5）。

## 決定

**ドロップ先は core の `drawer_layout_drop` が配置・掴んだ行・ポインタの y から決め、UI は挿入線を描いて離したときに
「動かす」意図を出すだけにする。application は順序だけを変えた新しい台帳を作り、カテゴリなら `categories.json`、ノートなら
`index.json` を原子的に書き戻せたときだけ差し替える。**

具体的には:

1. **ドロップ先の意味。** 掴んだ行がカテゴリ行なら、候補はカテゴリの塊（カテゴリ行と展開中のノート行）の境界で、ポインタの y が
   塊の中点より上にある塊の数を挿入位置とする。掴んだ行がノート行なら、候補は**同じカテゴリ**のノート行の境界で、同じく中点で
   数え、範囲の外は端に寄せる（別カテゴリの上で離しても自分のカテゴリの端へ行く）。結果は「移動後の番号」で返し、掴んだ行と
   同じ番号なら「変わらない」を意味する。挿入線の y も core が返す（UI は座標を計算しない）
2. **台帳は作り直す。** `category_ledger_moved(ledger, from, to)` / `note_ledger_moved(ledger, from, to)` は順序だけを変えた新しい
   台帳を作る。既存の `toggled` と同じ形で、台帳を可変にしない（ARC-005）
3. **意図は 2 本。** `folio_state_move_category(from, to)` / `folio_state_move_note(category, from, to)`。`from == to` は書かずに
   READY。書き戻せなければ `FOLIO_STATE_STORE_FAILED`（既存の値。文言は台帳の種類を区別しない）で状態を変えない。
   選択中のノートと編集中の本文は移動後も同じノートを指すよう、application が索引の番号を付け替える
4. **`index.json` はノートの並び替えでだけ書く。** 起動時には書かない（ADR 0004 の却下案のまま）。FR-008 の「json に無い md は
   末尾に付け、json にあって無い md は次の保存で消す」は、起動時の照合結果（メモリ上）をそのまま書くことで満たす
5. **ポートに `write_note_ledger(adapter, category, const struct note_ledger *)` を足す。** adapters は `data\<category>\index.json`
   を `file_bytes_store`（一時ファイル＋`MoveFileExW`）で置き換える
6. **UI のドラッグ。** 押下で行を覚えて `SetCapture`。縦の移動が `SM_CYDRAG`（`GetSystemMetricsForDpi`）を超えたらドラッグに入り、
   動くたびに `drawer_layout_drop` を引いて挿入線（2px・掴んだ行のカテゴリ色・字下げから右の余白まで）を描く。離したら
   ドラッグ中なら意図、そうでなければ今までどおりのクリック（トグル・選択）。`WM_CAPTURECHANGED` で取り消す。
   クリックの確定は `WM_LBUTTONDOWN` から `WM_LBUTTONUP` へ移る
7. **編集中でも並び替えられる。** 本文は RichEdit が持ち、application は番号を付け替えるだけなので、保存の要求（ADR 0006 の
   `folio_message_edit_flush`）は要らない

## 強制

- core が `windows.h` を含まないこと、ui が adapters を呼ばないことは ARC-002 / ARC-003 が **active**
- 新しい型（`drop_kind` / `drop_target`）は 1 ファイル 1 型（CNF-002・`eng/conformance.py` が落とす）
- `drawer_layout_drop` の意味は `tests/unit/layout_tests.c` の境界のテストが正本。UI がドロップ先を計算していないことはレビュー事項（ARC-011）
- 分岐 90% は QLT-009 が **active**。新しい確保経路は `tests/unit/allocation_tests.c` で全部失敗させる
- 実機の確認は `docs/quality/gate-proofs.md` 第 5-f 節（QLT-013）

## 結果

得る: 並び替えが 2 つの台帳の書き戻しに載り、`index.json` に初めて書く経路ができる。ドロップ先の意味が core のテストで固定され、
UI は線を引くだけになる。スクロール（FR-012）が入っても `drawer_layout_drop` に y を渡す側が変わるだけで済む。

失う・残る:

- 別カテゴリへは移せない。ノートを他のカテゴリの上で離しても自分のカテゴリの端に寄る（線で見える）
- ドラッグ中の自動スクロールは無い（ドロワーのスクロール自体がまだ無い）
- クリックの確定が離したときになるので、押しっぱなしでは何も起きない
- `index.json` はノートを並び替えるまで作られない。外から md を足しただけのカテゴリは、並び替えるまで走査順のまま
- 挿入線の太さと位置は 96 DPI でしか見ていない

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| UI がドロップ先を計算する | SPECIFICATION 第 4 節と ARC-011 に反する。境界の判定はテストで固定したい |
| ドロップ先を「挿入位置（0〜count）」で返す | 掴んだ行の前後で番号の読み替えが要り、application と UI の両方に判断が散る。「移動後の番号」1 つに固定する |
| 別カテゴリの上で離したノートは何もしない | 線が出ないと「落とせない」ことが伝わらない。端に寄せて線で見せるほうが説明が要らない |
| 起動時に `index.json` を書き戻す | ADR 0004 の却下案のまま。読むだけの起動で書き込み経路を開かない |
| 台帳を可変にして並び替える | `toggled` と形が揃わず、失敗時の巻き戻しが要る（ARC-005） |
| ドラッグ中の行を半透明で追従させる | 案2 のデザインに無い。線 1 本で足りる。要るなら別 Issue |
| 編集中の並び替えを禁じる（先に保存を要求する） | 本文は UI が持ち、application は番号を付け替えるだけ。保存を挟む理由が無い |
| `SetCapture` を使わず `WM_MOUSEMOVE` だけで追う | ポインタがドロワーの外へ出た瞬間に離しを取りこぼす。ADR 0002 のとおり `SetCapture` |

## 参考

- [ADR 0002](0002-plain-win32-no-ui-library.md)（自前描画と `SetCapture`）/ [ADR 0004](0004-first-drawer-slice.md)（台帳の照合と `toggled`）/
  [ADR 0006](0006-edit-mode-and-save-path.md)（編集中の本文の所有）
- [SetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture) /
  [SM_CYDRAG](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getsystemmetrics)
