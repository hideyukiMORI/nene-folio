# ADR 0006 — 編集中の本文は RichEdit だけが持ち、編集モードを抜けるときと Ctrl+S で元の形に揃えて書き戻す

- 状態: 受理
- 日付: 2026-09-09
- Issue: #11
- 影響する規則: ARC-003 / ARC-004 / ARC-005 / ARC-007 / ARC-008 / ARC-009 / ARC-010 / ARC-011 / C-002 / C-003 / C-005 / C-006 / C-014 / CNF-002 / QLT-009 / QLT-013

## 文脈

4 本の縦切り（#3 / #5 / #7 / #9）で閲覧まで通ったが、ノートを書けない。FR-006 は「編集モードで md を編集し、保存すると
同じファイルに書き戻す。同じ RichEdit をプレーンテキストで使う」と定め、ARC-004 は「編集中の本文は UI（RichEdit）が
所有し、保存の意図で application へ渡す」と定めている。この隔離区画（ARC-005）を初めて使うので、保存の時機・本文の
所有・改行の扱い・UI と application の境界の型を、実測で裏を取って固定する。

RichEdit（`RICHEDIT50W` / Msftedit.dll / Windows 11 10.0.26200）の実測（2026-09-09・Folio実装リナ）:

- `EM_STREAMIN`（`SF_TEXT|SF_UNICODE`）に LF / CRLF / CR を流すと、すべて 1 つの段落区切りに潰れる。内部表現は CR 1 個で、
  `EM_STREAMOUT` と `GT_USECRLF` は CRLF を返す。**LF は取り出せない**
- `EM_GETTEXTEX` の `CP_UTF8` は UTF-8 を直接返すが、孤立サロゲートを**黙って U+FFFD に置き換える**。
  `EM_STREAMOUT` の UTF-16 は孤立サロゲートをそのまま返す
- `EM_SETCHARFORMAT`（`SCF_ALL`）は既定の文字書式も更新し、その後の `SF_TEXT` の流し込みはこの既定で描かれる。
  `SF_RTF` の流し込みは本文の書式を RTF で上書きするが既定は壊さない。どちらの向きの流し込みも本文と書式を全置換する。
  `EM_SETBKGNDCOLOR` は流し込みで消えない
- `ES_READONLY` で作った控えは `EM_SETREADONLY` で往復できる。`EM_STREAMIN` は読み取り専用でも通る
- `ENM_KEYEVENTS` を立てると親の `WM_NOTIFY` に `EN_MSGFILTER` が `WM_KEYDOWN` / `WM_CHAR` / `WM_KEYUP` で届き、
  親が非 0 を返すと既定処理が止まる
- `EM_STREAMIN` は文字数上限を流し込んだ長さちょうどまでしか広げない。超える分は 1 文字も打てない（`EM_EXLIMITTEXT` が要る）
- `EM_STREAMIN` の先頭に BOM を付けると本文の U+FEFF になる
- 既存の `file_bytes_store`（一時ファイル＋`MoveFileExW`）は、読み取り専用の宛先・排他で開かれた宛先・無いディレクトリで
  `PERSISTENCE_UNWRITABLE` を返し、元のファイルも `.tmp` も残さない

## 決定

**編集中の本文は RichEdit だけが持つ。application は読んだ本文（`note_text`）と表示モード（`pane_mode`）を持ち、
「編集モードを抜ける」意図と Ctrl+S の意図で UTF-16 の本文を受け取り、core で UTF-8 に検証・変換し、元の本文の改行の形に
揃え、同じなら書かず、違えば `persistence_port.write_note` で原子的に書き戻す。**

具体的には:

1. **保存の時機は 2 つ。** 編集モードを抜けるとき（「閲覧」札・別ノートの選択・窓を閉じる）と Ctrl+S（抜けない）。
   破棄（保存せずに抜ける）は持たない。Escape は編集モードでは何もしない（破棄の意味を先取りしない）
2. **同じなら書かない。** application が読んだ本文と、受け取って正規化した本文を core の `note_text_equals` で比べる。
   dirty flag を application に持たない（本文を 2 か所に持つことになり ARC-004 に反する）
3. **改行の形は元の本文に合わせる。** core の `note_text_line_ending`（最初の改行で LF / CRLF を決める。無ければ LF）と
   `note_text_from_editor`（CR / CRLF / LF をその形に畳む）で正規化する。末尾改行の有無は触らない。**BOM は書かない**
   （読みで捨てている。BOM は本文ではなく、無いのが現在の既定）
4. **UI から application への本文は UTF-16 の単位列で渡す。** C-014 は「変換は ui / adapters の境界で」と書くが、
   この 1 本の意図に限り application が core の `utf8_text_create` を呼ぶ。孤立サロゲートは `FOLIO_STATE_NOTE_MALFORMED`
   として保存しない。理由は 2 つ: 壊れた本文をどうするかの判断を UI に置かない（ARC-011）、`EM_GETTEXTEX` の `CP_UTF8` は
   黙って直すので使わない（ARC-009）。変換の実装は引き続き core の 1 か所
5. **ドロワーは別ノートを選ぶ前に主窓へ同期メッセージで「編集中なら先に保存」を頼む。** UI 内の通知 1 つ
   （`folio_message_edit_flush`）で、主窓が本文を取り出して「抜ける」意図を出し、結果を返す。READY でなければ選択を進めず、
   失敗の 1 行を出す。`WM_CLOSE` も同じ手順で、失敗なら閉じない
6. **失敗は状態を変えない。** 書き戻せなければ `FOLIO_STATE_NOTE_STORE_FAILED` を返し、mode は EDIT のまま・読んだ本文も
   そのまま。RichEdit の本文は UI が持っているので消えない（FR-015）
7. **札は 2 つ描く。** 「閲覧」「編集」を頭の右端に並べ、有効な側だけ面塗り、無効な側は頭の文字色。矩形は 1 つの関数が
   描画と当たり判定の両方に与える。無効な側のクリックが意図になる。札の上は `HTCLIENT`

## 強制

- モジュール境界（core が `windows.h` を含まない、ui が adapters を呼ばない）は ARC-002 / ARC-003 が **active**
- 1 ファイル 1 型定義（`line_ending.h` / `pane_mode.h`）と外部関数の接頭辞は CNF-002 が **planned** だが、`eng/conformance.py` は
  走って落ちる（2026-09-09 実測）
- `write_note` の関数ポインタと文脈の不完全型は C-006 のレビュー事項。テストは `tests/unit/state_tests.c` の偽物で定義する
- 「application に UTF-16 を入れるのはこの意図だけ」はレビュー事項（C-014）。2 本目が要るなら新しい ADR
- 分岐 90% は QLT-009 が **active**。新しい確保経路は `tests/unit/allocation_tests.c` のシナリオで全部失敗させる
- 実機の確認は `docs/quality/gate-proofs.md` 第 5-e 節（QLT-013）。IME は無人で測れないので施主の実機確認事項

## 結果

得る: 閲覧と編集が同じ控えで往復し、保存は「抜けるとき」と Ctrl+S の 2 つに固定される。改行の形が保たれるので、
外部のエディタや git と併用しても差分が汚れない。壊れた入力は黙って直さず、型のある失敗で利用者に見える。

失う・残る:

- 🔴 BOM 付きの md を開いて保存すると BOM が落ちる。本文は変わらない
- 破棄が無いので、編集モードを抜ける操作はすべて保存になる。取り消したい変更は保存前に RichEdit の Undo（Ctrl+Z）で戻す
- IME（かな漢字変換の未確定文字列と Ctrl+S の競合）は実機でしか確かめられない
- 外部で変わった md は検知しない（仕様の非要件）。保存は最後に読んだ本文との比較なので、外部の変更は上書きされる
- 文字数上限は `EM_EXLIMITTEXT` で毎回広げる。`EM_STREAMIN` が上限を縮めるかは実装時に測る

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 打鍵ごとに自動保存する | 打鍵ごとにファイルを置き換えることになり、`.tmp` の作成と `MoveFileExW` が入力の遅延になる。保存の失敗も打鍵ごとに出る |
| 保存ボタンを置く | デザイン正本に無い。「抜けるとき保存」と Ctrl+S で足りる |
| Escape で編集を抜ける（保存する） | 利用者は Escape に「取り消し」を期待する。破棄を持たないこの縦切りでは Escape に意味を与えない |
| 破棄（保存せずに抜ける）を持つ | 確認ダイアログか二段の意図が要り、今回の縦切りより大きい。要るなら Escape と一緒に別 Issue |
| `EM_GETTEXTEX` の `CP_UTF8` で UTF-8 を直接取る | 孤立サロゲートを黙って U+FFFD に置き換える（実測）。ARC-009 に反する |
| UI で UTF-8 に変換してから application へ渡す | C-014 の字面には沿うが、変換に失敗したときの判断（保存しない・1 行を出す）が UI に落ちる（ARC-011） |
| application に dirty flag を持つ | 「変わったか」を本文とは別に持つと同じ事実を 2 か所に持つ（ARC-004）。本文の比較で足りる |
| 末尾改行の有無を元の本文に合わせる | RichEdit は末尾の段落記号を勝手に足したり落としたりしない（`EM_STREAMOUT` の実測）。利用者の意図した空行を消す規則になる |
| BOM を覚えて書き戻す | `note_text` が本文以外の事実を持つことになる。BOM は本文ではなく、無いのが現在の既定 |
| 編集用の RichEdit を別に作る | 実測で同じ控えの流し直しが本文と書式を全置換する。控えを 2 つ持つ理由が無い |
| ドロワーが `folio_state_pane_mode` を見て選択を拒む | 判断が UI に落ちる。本文を持つ主窓に頼むほうが正典経路が 1 つになる |

## 参考

- [ADR 0004](0004-first-drawer-slice.md)（ポートの文脈の不完全型・`_Nonnull`）/ [ADR 0005](0005-rigid-design-and-os-theme.md)（札の見た目）
- [EM_STREAMOUT](https://learn.microsoft.com/en-us/windows/win32/controls/em-streamout) /
  [EN_MSGFILTER](https://learn.microsoft.com/en-us/windows/win32/controls/en-msgfilter) /
  [EM_EXLIMITTEXT](https://learn.microsoft.com/en-us/windows/win32/controls/em-exlimittext)
