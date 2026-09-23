# 履歴から戻すの確認記録（Issue #154 / ADR 0038）

2026-09-23。ゲート内の単体と、ゲート外の測定でそれぞれ何を守ったかを分ける。
「実機で見た」と「値として確かめた」を混ぜない。**実機の目視と Win32 部品の絵はまだ 1 項目も無い**（設計席が撮る）。

## ゲート内（`pwsh -NoProfile -File ./eng/check.ps1` が守るもの）

### core（`tests/unit/text_tests.c` / `command_tests.c` / `ui_text_tests.c`）

- `note_text_first_line`（`verify_first_line`・12 場面）: 空の本文・先頭の空行・空白とタブだけの行を飛ばす・
  行の頭尾の空白とタブを除く・CRLF / LF / CR の区切り・末尾に改行の無い 1 行・全行が空なら長さ 0。
- 登録表: `FOLIO_COMMAND_HISTORY` は末尾に 1 つ、別名は `history` / `hist`（`:history` と `:hist` を parse で確かめる）、
  引数なし、パレットと「操作」に出る（出す操作は 16）。
- 文言: `UI_TEXT_COMMAND_HISTORY`・`UI_TEXT_HISTORY_GUIDANCE`・`UI_TEXT_HISTORY_ROW`・`UI_TEXT_HISTORY_ROW_UNREADABLE` の
  日本語の字面を期待表で固定。3 列の網羅は CNF-011、置換子（`{n}` `{name}`）の妥当性と閉じた語彙（`Enter` `Esc` `↑↓`）は
  `ui_text_tests` の全 ID の検査が守る。

### application（偽 adapter・`tests/unit/state_tests.c` / `allocation_tests.c`）

- `verify_history_all`: 5 版すべて・行（版番号・最初の行・読めるか）・本文・範囲外の行は false・
  閲覧中の `restore` が EDIT へ移って本文を返す・編集中の `restore`・開き直し・`close` の後に `count` が 0・
  別のノートへ移ると 0・保存で 0・開いたまま `destroy`。
- `verify_history_gaps`: 1 と 3 だけ（歯抜けの番号のまま）・0 版は `HISTORY_EMPTY`・`MALFORMED` / `UNREADABLE` の版は
  本文の無い行として残る・その行の `restore` は `HISTORY_UNREADABLE` でモードを変えない・範囲外・
  確保失敗で 0 行・無題は `NAME_REQUIRED`・未選択は `NOTHING_SELECTED`。
- 新しい結果値 2 つ（`HISTORY_EMPTY` / `HISTORY_UNREADABLE`）は `failure_lines[]` と `state_tests.c` の期待表の 2 か所（CNF-009）。
- 確保失敗は `allocation_tests` の `history_scenario` が `open_history` の各確保を 1 回ずつ失敗させ、
  持っている版が 0 のまま `OUT_OF_MEMORY` で片付くことを確かめる（QLT-009 の測定ビルド）。

### ui

- 面 `COMMAND_SURFACE_HISTORY` と操作 `FOLIO_COMMAND_HISTORY` の全値 switch に枝があること（`-Wswitch-enum`・C-002）。
  描画・鍵・流し込みは測定の対象外で、下の「未」の項目が残る。

## ゲート外 1: 実アダプタ probe（`out/agents/154-probe/`・worktree `154-history` の HEAD `5781cd2`）

production の `nenefolio_adapters.lib` / `nenefolio_application.lib` / `nenefolio_core.lib` をそのままリンクした
`history_probe.c` で `read_history` を測った（clang-cl 19.1.5・Windows 11 Pro 10.0.26200）。19/19 PASS。

| # | シナリオ | 期待 | 測った値 |
| --- | --- | --- | --- |
| 1 | 歯抜け（版 1・3 のみ）: 版 1〜5 | LOADED / ABSENT / LOADED / ABSENT / ABSENT | LOADED(body1) / ABSENT / LOADED(body3) / ABSENT / ABSENT |
| 2 | 版 0・版 6（範囲外） | 両方 MALFORMED | 両方 MALFORMED（ファイルに触れる前に断る） |
| 3 | UTF-8 でないバイト列 `0xFF 0xFE 0x00 0x01` | MALFORMED | MALFORMED |
| 4 | BOM 付き UTF-8 | `read_note` と同じ扱い | 両方 LOADED・BOM が落ちて "hello"・結果が一致 |
| 5 | icacls で読み取り拒否 → 復元 | UNREADABLE | UNREADABLE（復元後の ACL は正常） |
| 6 | CRLF の本文・LF の本文 | バイト列そのまま | 両方バイト完全一致（`memcmp`） |
| 7 | `.history/<category>/<note>/` が無い | ABSENT・ディレクトリを作らない | ABSENT・カテゴリ／ノートのディレクトリとも作られない |
| 8 | 日本語のカテゴリ「メモ」・ノート「テスト」 | LOADED | LOADED |

`read_history` は `read_note` と同じ `read_text` を通るので、BOM を含めて挙動の差は無い。

## ゲート外 2: Win32 部品（設計席の絵）

| 項目 | 状態 |
| --- | --- |
| 一覧の 5 行（版番号と最初の行・1 が最新） | 確認（4 版で。設計リナ 2026-09-23・枝 `00248ae`・`out/design/2026-09-23/history-restore/shots/h2.png` / `h7.png`。先頭の空行と空白行を飛ばした最初の行、CRLF の版も並ぶ。履歴の無いノートは `h5.png` の 1 行「履歴はありません。」） |
| 歯抜け（番号が飛んだまま） | 未（単体では確認済み） |
| Enter で本文が置き換わり、未保存の印が出る（閲覧中からは編集モードへ入る） | 確認（`h3.png`。閲覧中から編集モードへ入り、本文が版の中身・カーソルは先頭） |
| Ctrl+Z 1 回で戻す前の本文へ戻る | 未（`post-keys.ps1` に Ctrl+Z が無い。hide の目視） |
| 保存後に戻す前の本文が `1.md` にある | 確認（ディスクの `cat`。md が版の中身・旧本文が `1.md`・旧 `1.md`〜`3.md` が `2.md`〜`4.md`） |
| 560×360 で案内と 3 行以上が見える | 未 |

実機の目視の手順は[統合チェックリスト](2026-09-22-visual-checklist.md)の「154 —」の節。

## 限界

- 版の見出しは版番号と最初の行だけで、いつの版かは分からない（時刻を残さない設計の帰結・ADR 0038 の結果）。
- 版の本文に CRLF があるとき、`EM_REPLACESEL` は CR 1 つに畳む（#41 の Win32 部品 probe で置換文字列について測った挙動）。
  履歴の本文そのものでは実機で測っていない。
- 改名の意図が未完了のあいだは旧名で読むので、既に移った履歴は「履歴はありません」に見える（単位 A の報告）。
