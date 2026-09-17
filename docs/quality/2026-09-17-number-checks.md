# 原文の行番号と設定ファイルの確認記録（Issue #39 / ADR 0025・ADR 0026）

2026-09-17。ゲート内の単体と、ゲート外の測定でそれぞれ何を守ったかを分ける。
「実機で見た」と「値として確かめた」を混ぜない。実機の目視はまだ 1 項目も行っていない。

## ゲート内（`pwsh -NoProfile -File ./eng/check.ps1` が守るもの）

### core `folio_settings`（`tests/unit/settings_tests.c`）

- 版 1（`{"version": 1, "number": <bool>}`）の**往復**。書いた文書を読み直して同じ値になる。
  `true` と `false` の両方で確かめる。
- 既定値は `number = false`（ファイルが無いときだけ使う）。
- 拒む形 13 種を 1 件ずつ: 未知の版（2 / 0 → `UNSUPPORTED_VERSION`）、欠けたキー（`number` / `version`）、
  未知のキー（`theme`）、**重複したキー**、型の違う値（`1` / `"1"`）、キーの順序違い、空ファイル、
  閉じていない文書、末尾の余計な文字、配列。いずれも `MALFORMED` か `UNSUPPORTED_VERSION` で、
  **既定値へ落ちず、出力も触らない**。
- 確保失敗は `allocation_tests` の `settings_scenario` が既定値・複製・書き出し・読み直しの確保を
  1 回ずつ失敗させ、すべて `OUT_OF_MEMORY` で片付くことを確かめる。

### core `line_index`（`tests/unit/line_index_tests.c`）

- 空の本文は 1 行。CR の無い本文も 1 行。**末尾が CR なら空の最終行も 1 行**と数える。
- 空行を含む `a\rb\r\rc` は 4 行で、位置 → `{番号, 先頭か}` と番号 → 先頭位置が両方向で一致する。
- CR そのものの位置は前の行に属し、先頭ではない。本文の長さと等しい位置は「中」である。
- 3000 単位の長い行は、折り返しに依らず 1 論理行のまま（中の位置も同じ番号）。
- 範囲外（本文の長さ超え・番号 0・番号 > 行数）は `LINE_INDEX_OUT_OF_RANGE` で**出力を触らない**。
- 桁数は行数だけで決まり、最小 3 桁: 9 → 3・10 → 3・999 → 3・1000 → 4・9999 → 4・10000 → 5。
- 確保失敗は `allocation_tests` の `line_index_scenario`（構造体と CR の位置の列の伸長）が通す。

### core `folio_command`（`tests/unit/command_tests.c`）

- 登録表は 13 件になり、順序は安定（`SET` と `TOGGLE_NUMBER` を末尾に足した）。
- `:set number` / `:se nu!` / 引数なしの `:set` が `FOLIO_COMMAND_SET` に解ける
  （語の可否は `folio_command_parse_option` が別に決める）。
- 語の全別名: `number` / `nu` → 表示、`nonumber` / `nonu` → 非表示、
  `number!` / `nu!` / `invnumber` / `invnu` → 切替。末尾の空白は落ちる。
- 拒む語 9 種（空・空白だけ・`numbers`・`NUMBER`・`no`・`number no`・`nu nu`・`!number`・`invisible`）は
  false で**出力を触らない**。
- `folio_command_listed` は `SET` だけが偽で、ほかの 12 件はすべて真。
- `TOGGLE_NUMBER` は別名 0 個・日本語の表示名「行番号の表示を切り替える」で、パレットの語で見つかる。

### application `folio_state`（`tests/unit/state_tests.c` / `allocation_tests.c`）

- 設定は起動時に**ちょうど 1 回**読む。無いファイルでは既定値で始め、**起動時には書かない**。
- 変更は**先に書いてから採用する**。同じ値では書かない（書き込み回数で確かめる）。
  表示と保存が食い違わないことを、書き込み失敗（`PERSISTENCE_UNWRITABLE`）で確かめる:
  `FOLIO_STATE_SETTINGS_STORE_FAILED` を返し、**値は変わらない**。
- 読めない設定（`MALFORMED` / `UNREADABLE` / 未知の版 / 未知のキー）では `folio_state_create` は
  READY のまま返り、`folio_state_settings_notice` が `FOLIO_STATE_SETTINGS_UNREADABLE` を持つ。
  既定値で始まり、そのセッションの変更は断られ、**書き込みは 1 回も起きない**（上書きしない）。
  同じ状態でノートの閲覧・編集・保存は成功する。
- 記憶不足の読み取りだけは起動の失敗（`FOLIO_STATE_OUT_OF_MEMORY`）。
- `verify_failure_lines` が 2 つの新しい結果にも 1 行があることを確かめる。
- 確保失敗は `state_scenario` の鎖に `set_number` を足して通す。

### 分岐網羅

`python eng/coverage.py` は **92.99%（2331 / 2507 分岐）**、閾値 90%（QLT-009）。
`src/core/folio_settings.c` は 100%。閾値・除外・`eng/*.json` は触っていない。

## ゲート外の測定 1: 実アダプタの `data/settings.json`

`out/design/2026-09-17/settings_probe.c`（計装なしの `clang-cl /clang:-std=c23 /MT /utf-8` で
製品ソースを直接コンパイル）。専用の `data/` を毎回作り直し、利用者の `data/` には触れない。
結果は `out/design/2026-09-17/settings-probe.log`。**24 項目すべて成功、失敗 0、exit 0**。

- 無いファイルは `PERSISTENCE_ABSENT`。**読んでもファイルは作られない**。
- 最初の変更で `data/settings.json` が公開され、中身は版 1 の整形（`"version": 1` / `"number": true`）。
- **原子的な置き換えで `.tmp` を残さない**（`data/` の項目は `.` `..` と錠と `settings.json` の 4 つだけ）。
  2 回目の変更も同じファイルを置き換え、項目は増えない。
- 書いたものを同じアダプタが読み戻して同じ値になる。
- 壊れた文書・未知の版・未知のキーは `PERSISTENCE_MALFORMED` で、**ファイルはバイト単位でそのまま残る**。
- 読み取り専用の `settings.json` への書き込みは `PERSISTENCE_UNWRITABLE` で、保存された値は変わらない。

## ゲート外の測定 2: Windows 部品（番号の帯と操作）

`out/design/2026-09-17/number_window_probe.c`。A 部は製品の `note_pane` を自前の宿主窓（画面外）に作って
番号の行の幾何を測り、B 部は製品の `folio_window` を画面外に作って操作と `settings.json` を測る。
結果は `out/design/2026-09-17/number-window-probe.log`。**57 項目すべて成功、失敗 0、exit 0**。

### A 部（`note_pane` の `gutter_row`）

- 折り返しを含む本文（ASCII 行・空行・日本語の行・3000 単位の長い行・末尾の空行）で、
  **表示行 41 に対し番号の行は 4**。**継続行には番号が出ない**。
- 見えている番号は 1..N で、飛びも重なりも無い。各行は正の高さを持つ。
- **すべての行の y が、その論理行の先頭の文字の `EM_POSFROMCHAR` の y と一致する**（主窓の client 座標へ直した値）。
- **行の高さは一定でない**（ASCII 20px・日本語 26px）。日本語の行の番号はその行自身の y に乗る。
- 末尾の CR のあとの**空の最終行にも番号が出る**。
- 折り返しの途中だけが見えているときは、番号の行が**1 つも無い**のが正しい状態である。
- `EM_LINESCROLL`・キャレット移動（`VK_UP` × 20）・検索の選択（`EM_EXSETSEL` + `EM_SCROLLCARET`）の
  あとも番号は本文と一致する。`EM_LINESCROLL` は親へ `EN_VSCROLL` として届く。
- 入力（`EM_REPLACESEL`）・貼り付け・`EM_UNDO` はいずれも親へ `EN_CHANGE` として届き、
  そのあと番号は作り直されて一致する。
- **`EM_STREAMIN`（`note_pane_edit`）は `EN_CHANGE` を 1 回出す。**
  だから `EN_CHANGE` の扱いは「印と無効化だけ」で、そこで本文へ問い合わせない（ADR 0026 の決定 4 のとおり）。
- ホイールは `EN_VSCROLL` を出さないが、サブクラスの後処理が親へ `folio_message_pane_scrolled` を 1 通送る。
- 本文の矩形を動かしても（帯の幅の変化に相当）、**本文はバイト単位で同じ・`EM_CANUNDO` は保たれ・選択も保たれ**、
  `note_pane_scroll_to_line` で**最初に見える論理行が元に戻る**。
- 1 万行（10001 論理行）で桁数は 5。**本文が変わった直後の最初の組み立て 0.19 ms、
  スクロール中の 1 回の描き直し 0.19〜0.24 ms**（20 回の平均・最適化なしのビルド）。

### B 部（製品の `folio_window`）

- 本文の RichEdit は `note_pane_control_id` を持ち、絞り込みの欄と区別して振り分けられる。
- 起動直後は `data/settings.json` が無く、既定で番号は出ない。
- `:set number` で値が真になり、**`data/settings.json` に `"number": true` が書かれる**。
- `:set nonumber` / `:set nu!` / `:set invnu` がそれぞれ期待どおりに効き、そのつど書かれる。
- **`:set bogus` は何も変えない**（値もファイルも前のまま）。
- **10001 行（5 桁）を編集しているあいだだけ本文の左端が右へ 2px 動く**（240 + 36 = 276 → 278）。
  等幅書体の数字 1 文字は 6px なので、5 桁は 5×6 + 8 = 38 > 36 で空きを超える。
- **1001 行（4 桁）では本文は動かない**（4×6 + 8 = 32 ≤ 36）。
- **閲覧へ戻すと帯が消えて本文の左端も戻る**（5 桁の長いノートでも 276）。
- パレットで「行番号」を選ぶと切替が走る。**「設定を変える」（`:set` の表示名）ではパレットは何も実行しない**
  （`folio_command_listed` が偽なので候補に出ない）。
- **#40 の絞り込みの欄の `EN_CHANGE` は壊れていない**（欄に語を入れると索引が 1 本に減り、空にすると解ける）。

## 実機の目視（hide に依頼する項目・まだ 1 つも確認していない）

probe はすべて画面外の窓で値だけを測っており、描いた絵は 1 度も見ていない。次を実機で見てほしい。

1. **実際のマウスホイール**で本文を転がしたときに、番号がずれずに追随するか（`EN_VSCROLL` の来ない経路）。
2. **ちらつき**（番号の帯だけを無効化しているが、スクロールで即時に描き直している）。
3. **IME の変換中**に番号が動かないか、確定後に正しく追随するか。
4. **高 DPI**（120 / 144 / 168）での帯の幅と番号の位置。DPI を変えた直後の桁の大きさ。
5. **両テーマの色**（ライト `#6B727C` / ダーク `#7C838D`）。ClearType での見え方と、本文より薄く見えるか。
6. **番号の字の大きさ**（等幅 11px と本文 11pt Yu Gothic UI の釣り合い）。上揃えで良いか。
7. **4 桁が空きに収まって見えるか**（値としては収まるが、見た目に窮屈でないか）。
8. **ヘルプが 1 行増えた最小寸法 560×360** で、一覧とキー説明が破綻しないか。
9. **閲覧では出ない・無題の編集では出る**こと（値では確かめていない。probe は本文の矩形でしか見ていない）。
10. `data/settings.json` を手で壊したときの、起動時の 1 行と「そのセッションは設定を変えない」振る舞い。

## 限界（この単位では測っていない・やっていない）

- **描いた絵そのものを 1 度も検査していない**。番号の字形・色・位置は実機の目視に依る。
- 閲覧（RTF）で帯を出さない判断は、`gutter_visible` の材料（`number` と `pane_mode` と文書の有無）と
  本文の矩形の戻りでしか確かめていない。
- 無題の編集で帯が出ることは、桁数が 3 で本文が動かないため外から観測できていない。
- 実機の DPI 変更そのものは測っていない。
- 1 万行を超える本文での体感は測っていない（1 回の組み立ては行数に比例して伸びる）。
- 設定は `number` の 1 キーだけで、`theme` / `language` は #38 が版 2 で足す（ADR 0025 の決定 2）。
