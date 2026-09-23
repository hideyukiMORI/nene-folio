# ADR 0038 — 現在のノートの履歴（1.md〜5.md）を一覧から選び、選んだ版の本文を編集中の本文へ流し込む（ファイルは触らず、保存は既存の経路）

- 状態: 受理（設計リナ 2026-09-23。hide の 2026-09-23 の指示「対応を進めて」で、設計席の提案の順の 2 番目。現物調査 `out/agents/hist-probe/memo.md`）
- 日付: 2026-09-23
- Issue: #154
- 影響する規則: ARC-001 / ARC-003 / ARC-007 / ARC-009 / ARC-010 / ARC-011、C-002 / C-003 / C-005 / C-006 / C-008 / C-012 / C-018、CNF-002 / CNF-009 / CNF-010 / CNF-011、QLT-009
- 関連: ADR 0012（履歴の書き方・「戻す UI は別 Issue」）、ADR 0016 / 0018（操作の登録表と入力面）、ADR 0028（`note_pane_replace` = Undo 1 単位）、ADR 0031（一覧の面の作りと決定 7 の switch の列挙）、ADR 0022（改名で履歴が移る）、ADR 0008（移動で履歴が孤立する）

## 文脈

FR-017 / ADR 0012 は md を書き戻す直前の本文を `data/.history/<カテゴリ>/<ノート>/1.md`（最新）〜`5.md` に残す。
しかし現行（main `db546da`）には**読む経路が 1 本も無い**: adapters の `.history` を扱う関数は書く・回す・消すだけ、
`persistence_port` に読みの口は無く、application にも UI にも一覧が無い（現物調査 §1〜§2）。
「誤った自動保存の保険」は、利用者が戻せてはじめて保険になる。

既にある部品:

- 本文を丸ごと差し替える経路は `note_pane_replace`（`EM_EXSETSEL` → `EM_REPLACESEL` 1 回・**Undo 1 単位**・ADR 0028）と、
  編集モードへ入る `enter_edit`（`folio_state_begin_edit` → `note_pane_edit`）の 2 つ（§3）。
- 「未保存」は RichEdit の印ではなく application の `note_text_equals(body, edited)` の比較で決まる（§3）。
- 一覧の面は 2 種類: 設定画面（コンパイル時固定の `settings_rows[]`）とパレット（実行時に数える可変長・`command_first` によるスクロール）。
  履歴は 0〜5 本でノートごとに違うので**パレット型**が近い（§4）。
- 操作を 1 つ足す経路は `FOLIO_COMMAND_SETTINGS` と同じ（登録表 `catalog[]`・全値 switch 2 本・`ui_text`・`execute_command`・面を足すなら ADR 0031 決定 7 の switch 群）（§5）。
- 時刻は adapters しか読めない（ARC-007）。履歴のファイル名にも中身にも時刻は無い（ADR 0012 決定 1）。

## 決定

**履歴の一覧を開き、選んだ版の本文を「編集中の本文」として流し込む。ファイルは触らず、保存は既存の経路に任せる。**

### 決定 1 — 戻すとは「未保存の編集にする」ことである

1. 「戻す」は、選んだ版の本文で編集モードの本文を**丸ごと置き換える**未保存の変更である。md も履歴も書かない。
2. 置き換えは `note_pane_replace` の全文 span 1 回（Undo 1 単位）。閲覧中なら先に `enter_edit` で編集モードへ入る（既存の 1 本）。
   置き換えの後、カーソルは先頭・スクロールは 0。未保存の印は既存の比較で自然に付く。
3. 確認の箱は出さない。Enter で即採用・Esc で戻るの流儀（ADR 0016 / 0031）を守り、取り消しは Ctrl+Z 1 回。
4. 保存すると、そのとき md にある本文（＝戻す前に保存されていた本文）が 1.md に積まれる（ADR 0012 決定 2 のまま）。
   だから戻しても失われるものは無く、5 版の枠をひとつ使うだけである。**戻すことで履歴が動くのは保存の瞬間だけ**。

### 決定 2 — 読みの口は port に 1 本

5. `persistence_port` に `read_history(adapter, category, note, version, out)` を 1 本足す。`version` は 1〜`note_history_depth`。
   無ければ `PERSISTENCE_ABSENT`、UTF-8 でなければ `MALFORMED`、読めなければ `UNREADABLE`（`read_note` と同じ意味論）。
   一覧の専用関数（列挙）は作らない。application が 1〜5 を順に読み、`ABSENT` を飛ばす（最大 5 回のファイル読み・ローカル NTFS）。
6. adapters は `compose_version` の同じパス組みで読む（書く側と同じ 1 か所）。`.history` の中の走査は要らない。

### 決定 3 — application が一覧を持ち、UI は描くだけ

7. `folio_state_open_history(state)`: 現在のノート（無題は `FOLIO_STATE_NO_DOCUMENT` 相当で断る）の 1〜5 を読み、ある版を**版番号順**に持つ。
   1 本も無ければ `FOLIO_STATE_HISTORY_EMPTY`（新しい結果値・文言は `failure_lines[]` に 1 行）。読めない版（`MALFORMED` / `UNREADABLE`）は
   **黙って落とさず**、本文の無い行として持つ（ARC-009）。確保に失敗すれば `OUT_OF_MEMORY` で何も持たない。
8. 問い合わせ: `folio_state_history_count`、`folio_state_history_row(index, &version, &first_line, &readable)`
   （`first_line` は本文の最初の空でない論理行・UTF-8・core の純関数 `note_text_first_line` で切る。時刻は出さない）、
   `folio_state_history_body(index)`（`const struct note_text *`）。
9. `folio_state_restore_history(state, index)`: 閲覧中なら `begin_edit` と同じ遷移を行い、本文を返す。
   読めない行なら `FOLIO_STATE_HISTORY_UNREADABLE` で何も変えない。UI はこの本文を `note_pane_replace` で流し込む。
10. `folio_state_close_history(state)`: 持っている版を捨てる。面を閉じるとき・別のノートへ移るとき・保存のときは必ず呼ぶ（古い版を持ち越さない）。

### 決定 4 — 入口と面

11. 操作は `FOLIO_COMMAND_HISTORY`（表示名「履歴から戻す」/ "Restore from history" / "从历史恢复"・Ex `history` / `hist`）。
    「操作」メニュー・パレット・Ex の 3 系統から同じ意図へ入る（ADR 0016）。鍵は割り当てない。
    `folio_command_listed` は現在のノートがあるときだけ true（無題では出さない）。
12. 面は新しい `COMMAND_SURFACE_HISTORY`。**パレット型**（実行時可変長・`command_first` のスクロール・行の高さと箱の高さは #69 の 1 本）で、
    行は「`{n}  {line}`」（`n` は版番号・1 が最新・`line` は決定 8 の最初の行。読めない版は「`{n}  （読めません）`」で Enter は何もしない）。
    案内の 1 行は「1 が最新 / ↑↓ 選択 / Enter 戻す / Esc 戻る」。文言はすべて `ui_text` の表（C-018）。
13. `HISTORY_EMPTY` と `NO_DOCUMENT` は面を開かず、既存の失敗の 1 行に出す（ADR 0031 の D1 と同じ形）。
14. 面のフォーカスと戻り先は ADR 0016 の入力面と同じ（開く前の区画へ Esc で返す）。Enter で採用したら面を閉じ、フォーカスは本文（編集モード）へ。

### 決定 5 — 変えないもの

15. 履歴の書き方（ADR 0012）、版の数 `note_history_depth = 5`、改名で移り移動で孤立する扱い（ADR 0022 / 0008）。孤立した旧履歴は出さない。
16. 時刻を表示しない（ARC-007。ファイルにも無い）。差分の表示・履歴の削除・全ノートの横断はしない。

## 強制

- 新しい結果値 `FOLIO_STATE_HISTORY_EMPTY` / `FOLIO_STATE_HISTORY_UNREADABLE` は `failure_lines[]` に行を足す（CNF-009 が落とす）。
- 文言は `ui_text` の 3 列（CNF-010 / CNF-011）。操作の全値 switch（C-002）。port の文脈は不完全型（C-006）。
- 確保失敗の分岐は `allocation_tests.c` で 1 回ずつ（QLT-009 の測定ビルド）。
- 一覧の面の switch 群（ADR 0031 決定 7 の列挙）に `HISTORY` の枝が要り、`-Wswitch-enum` が漏れを落とす。

## 結果

- 得るもの: FR-017 の保険が使える。ファイルを触らないので、戻す操作そのものが履歴を壊さず、取り消しは Ctrl+Z。
- 失うもの: 版の見出しは版番号と最初の行だけで、いつの版かは分からない（時刻を残さない設計の帰結）。面を 1 つ足すぶん switch が増える。
- 前提: `note_pane_replace` の全文 span が Undo 1 単位で効くこと（#41 で実測済み）。RichEdit の本文の上限（`EM_EXLIMITTEXT`）は既存のまま。

## 却下した選択肢

| 選択肢 | 却下した理由 |
| --- | --- |
| 戻すと同時に md へ保存する | 保存の経路が 2 本になる（ARC-001）。誤って戻したときの取り消しが Undo でなく「もう一度戻す」になり、5 版の枠を 2 つ使う |
| 確認の箱を出す | Enter で即採用・Esc で戻る流儀（ADR 0016 / 0031）に反する。Undo 1 回で戻るので箱の価値が無い |
| port に列挙の関数（どの版があるか）を別に足す | 5 回の `read_history` で足りる。列挙と読みの 2 本は結果の整合（列挙にあるが読めない）を UI に持ち込む |
| ファイルの mtime を版の見出しに出す | 時刻は adapters しか読めず（ARC-007）、ADR 0012 が時刻を使わないと決めた。core へ時刻の型を持ち込む理由が無い |
| 設定画面型（固定行）で作る | 版は 0〜5 で可変。固定配列では表せない（現物調査 §4） |
| 読めない版を一覧から黙って落とす | ARC-009。行として見せ、選べないことを示す |

## 検証

1. 単体（偽 adapter）: 5 版すべて・歯抜け（1 と 3 だけ）・0 版（`HISTORY_EMPTY`）・読めない版（行はあるが `restore` は `HISTORY_UNREADABLE`）・無題（`NO_DOCUMENT`）・
   閲覧中の `restore` が編集モードへ遷移して本文を返す・`close` の後に `count` が 0。
2. 確保失敗: `open_history` の各確保を 1 回ずつ失敗させ、持っている版が 0 のまま `OUT_OF_MEMORY`（`allocation_tests.c`）。
3. core: `note_text_first_line` の空行・先頭の空行・CRLF / LF / CR・末尾に改行の無い 1 行。
4. 実アダプタ probe（Sonnet）: `read_history` の ABSENT / MALFORMED / 正常、版 0 と 6 の拒否。
5. Win32 部品（設計席の絵）: 一覧の 5 行・歯抜け・Enter で本文が置き換わり未保存の印が出る・Ctrl+Z 1 回で戻る・保存後に旧本文が 1.md にある・
   560×360 で案内と 3 行以上が見える。

## 移行

無し。`settings.json` も台帳も変えない。単位 A（port・adapters・core・application・tests）→ 単位 B（登録表・面・docs）の 2 つの Draft PR で入れる。

## 2026-09-23 の補正（単位 A / B の実装と設計席の絵の後・決定本文は書き換えない）

1. **port の引数は宛先を束ねた 3 つ**（決定 5 の 5 引数だと C-012 の引数 4 の上限に触れる）: `read_history(adapter, const struct history_version *which, out)`。
   `history_version`（category・note・version）は全メンバーが独立に妥当なので完全型で公開する（C-003 の例外・#158）。
2. **無題は既存の `NAME_REQUIRED`、未選択は `NOTHING_SELECTED` を流用**した（決定 7 の「NO_DOCUMENT 相当」）。新しい結果値は `HISTORY_EMPTY` と `HISTORY_UNREADABLE` の 2 つで、
   どちらも `inline_outcome()` の「欄の 1 行」の群（決定 13）。面が閉じている入口（「操作」メニュー）からの失敗は設定画面と同じく失敗の箱に出る。
3. 一覧の行の置換子は `{line}` ではなく既存の **`{name}`**（`ui_text_request.h` の置換子の集合は 6 つで閉じており、単体が数を固定している）。行は「`{n}  {name}`」、読めない版は「`{n}  （読めません）`」。
   最初の行は 255 バイトで UTF-8 の境目に切ってから埋め、はみ出しは描画の省略記号に任せる。
4. 採用の順は **写しを作る → `restore_history` → 閲覧中なら `note_pane_edit`（保存済みの本文）→ `note_pane_replace`（全文 span）→ カーソル先頭 → 面を閉じる（ここで `close_history`）→ 本文へフォーカス**。
   `enter_edit` は呼ばない（EDIT のときは何もせず、読めない版でも編集モードへ入ってしまうため）。画面から Ctrl+S で保存して一覧が 0 本になったら面を閉じる。
5. ヘルプの鍵の行は足していない（設定画面と同じく、面の案内 1 行を鍵の説明とする）。
6. 実測（設計席の絵・`out/design/2026-09-23/history-restore/shots/`）: 一覧・Ex `:hist`・履歴の無いノートの 1 行・Enter での置き換え・保存後の `1.md` の繰り上がりを確認した。
   **Ctrl+Z 1 回で戻ること・歯抜けの絵・560×360 は未**（[確認記録](../quality/2026-09-23-history-checks.md)）。
7. 残る既知の隙: 改名の意図が未完了（`RENAME_PENDING`）のあいだ `open_history` は旧名で読む。別カテゴリへの移動と改名の経路では一覧を捨てないが、面が開いたまま移動する経路は無い。
   版の本文に U+0000 があると `EM_REPLACESEL` はそこで切れる（`note_pane_replace` の契約どおり）。
