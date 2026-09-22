# ADR 0030 — 利用者に見える文言は core の `ui_text` の表 1 か所から ID と言語で引き、句の中の語と数は置換子で埋め、置き場所を字句検査で守る

- 状態: 受理（設計リナ 2026-09-22。ADR 0029 の単位 A。棚卸し `out/design/2026-09-22/ui-text-inventory.md`（main `53acb20`）と、現行コードに照らした読み取り専用の批評（止める所見 7・直したい所見 9）を経て直した）
- 日付: 2026-09-22
- Issue: #74（親 #38・ADR 0029 決定 4〜6）
- 規則: ARC-001 / ARC-002 / ARC-003 / ARC-004 / ARC-005 / ARC-010 / ARC-011、C-002 / C-003 / C-005 / C-012 / C-018（この単位で定義）、CNF-002 / CNF-006 / CNF-009 / CNF-010（この単位で定義）、QLT-009 / QLT-010、GIT-004
- 関連: ADR 0027（`failure_lines[]` と CNF-009）、ADR 0028 決定 9（`BAD_PATTERN` の位置は UI が添える）、ADR 0029（分割・置換子・OOM の退避・検査の方針）、ADR 0016 決定 2（label の翻訳表の予告）、ADR 0017（パンくずの区切りは多角形で文言ではない）、ADR 0001

## 文脈

棚卸しと批評の実測（main `53acb20`）:

- 非 ASCII のバイトか `\x` `\u` `\U` のエスケープを含む文字列リテラルがあるのは **8 ファイル**: `folio_state.c` 49、`folio_window.c` 43、`name_prompt.c` 20、`folio_command.c` 15、
  `main.c` 10、`drawer_window.c` 2（`L"\x2212"` の折畳印）、**`file_bytes.c` 1 と `note_text.c` 1（UTF-8 の BOM `"\xEF\xBB\xBF"` を `memcmp` に渡すバイト列で、文言ではない）**。
  `folio_window.c` に文字リテラル `'\x1b'` がある。`u"…"` は 13 件あるがすべて空、`u8"…"` と `U"…"` は 0 件。畳んだ後の異なる文言は 116 種、ASCII だけで表示に使うのは 11。
- `failure_lines[]` 40 と `folio_command.c` の label 15 は outcome / command 名から ID が機械的に導ける。label は `folio_command_label` のほか `folio_command_matches`（部分一致）と
  `folio_command_listed_count` も読む。`folio_window.c` の操作ボタン（`draw_actions`）は `folio_command_label(NEW / SAVE / HELP)` を**意図的に共有**している。
- 全値の `switch` の後の**到達しない既定**が 5 か所。同字面の重複は `name_prompt.c` の 3 つだけで、`search_direction_label` の既定 `"このノート内を検索  "` と `main.c` の
  `L"実行ファイルの場所が取得できません。"` は**どの case とも字面が違う**（到達しないので表示されたことは無い）。
- 断片の連結は **5 か所**: `search_status`（向きの札 → k → `" / "` → n → `" 件"`）、`replace_status`（ノート名 → `" / "` → k → `" 件"`）、`failure_status`（失敗の 1 行 →
  `" 位置 "` → offset）、`name_prompt.c` の `fill_pending`（旧名 → `" → "` → 新名。`char line[600]`）、同 `show_pending_reason`（失敗の 1 行 → `L' '` → `pending_explanation`。
  `wchar_t line[512]`。変換失敗と長さ超過の退避 2 本はどちらも `pending_explanation` だけを出す）。`replace_status_capacity` は 512。ノート名の上限は `name_list_max_length = 255` バイト。
- W 系 API へ文言を渡す経路: `draw_utf8` / `measure_utf8`（`folio_window.c`・`drawer_window.c`。描画のたびに `utf16_text_create` で確保）、`drawer_window.c` の
  `GetTextExtentPoint32W(L"\x2212", 1)` と折畳印の描画（カテゴリ行ごと・再描画ごと）、`main.c` の `report()`（`L"窓を作れませんでした。"`・`L"記憶域が足りません。"` ×2・
  `adapter_failure()` の 5 文言）、`name_prompt.c` の `prompt_title` / `prompt_hint` / `prompt_accept` / `prompt_close` と `label(L"ノートの名前")` / `L"保存先カテゴリ"`。
  退避付きの `utf16_text_create` は `main.c` の `report_utf8` と `name_prompt.c` の `submit`。呼び出し側は `utf16_text_length` で書いた長さを使う。
- 非 ASCII の最長は `FOLIO_STATE_RENAME_HALTED` の 64 UTF-16 単位（UTF-8 で最大 192 バイト）。英訳・中訳はまだ無い。
- `eng/conformance.py` の `c_code()` は**コメントと一緒に文字列リテラルも潰す**ので、そこから文字列は取り出せない。`line_table_checks` はファイルを直接読むので `src/app` も指せ、
  右辺が列挙値でも `[値] =` を数えるだけで通る。`document_checks` は規則の定義と強制マトリクスの状態一致だけを見て、証明行は `active` のときだけ要る。ID の空きは CNF-010・C-018。
  `eng/coverage-policy.json` の対象は `core` と `application` だけ。ARC-003 は core の `<stdio.h>` を拒む。`readability-function-size` は関数だけを数える。
- core には関数 `utf16_text_units()` がある。ARC-005 は可変状態を持てる区画を ui と adapters に限る。単位 C は表を `[UI_TEXT_X] = {"…","…","…"}` の 2 次元にする（ADR 0029 決定 4）。

## 決定

1. **ID は core の閉じた列挙 `enum ui_text`（`src/core/ui_text.h`）、言語は core の閉じた列挙 `enum folio_language`（`src/core/folio_language.h`・この単位では `FOLIO_LANGUAGE_JA` の 1 値）。**
   文言は `src/core/ui_text.c` の指示付き初期化子の表から `[[nodiscard]] const char *_Nonnull ui_text_line(enum ui_text, enum folio_language)` で引く。**言語を最初から引数に取る**のは、
   単位 C で表を 2 次元にしたとき全呼び出しを書き換えず、core に可変の「現在の言語」を持たせない（ARC-005）ため。言語の値は application の
   `[[nodiscard]] enum folio_language folio_state_language(const struct folio_state *)`（この単位では常に `JA`。単位 C で設定から返す）から UI が受けて渡す。
   UTF-8 の NUL 終端文字列を返し、長さは `strlen` で取る。表の外の値へは落ちない（ADR 0027 決定 3）。網羅は **CNF-009 の `lineTables` に `ui_text.h` → `ui_text.c` の行を足す**。
   ID の群: `UI_TEXT_FAILURE_<outcome>`（40）、`UI_TEXT_COMMAND_<command>`（15）、`UI_TEXT_HELP_<意味>`（18・意味で名付ける）、`UI_TEXT_STATUS_*`、`UI_TEXT_ACTION_*`
   （`◀ 前へ` `次へ ▶` `↑ 前` `↓ 次` `操作 ▾` `1 件` `すべて` など字形を含む 1 リテラルは 1 ID）、`UI_TEXT_CHIP_*`、`UI_TEXT_PROMPT_*`、`UI_TEXT_APP_*`、
   `UI_TEXT_GLYPH_MINUS` / `UI_TEXT_GLYPH_PLUS`（単独で描く字形は `−` と `+` の 2 つだけ）、`UI_TEXT_PLACEHOLDER_*`、`UI_TEXT_TITLE_UNTITLED`、`UI_TEXT_EMPTY`（`""`）。
   **共有と分離は現行コードを写す**: いま 1 つの経路を共有している所（`draw_actions` の `folio_command_label(NEW / SAVE / HELP)`）は共有のまま、いま別のリテラルである所
   （札「編集」「閲覧」と command の label、名前入力面のボタンと command の label）は別 ID。新たに統合も分離もしない。
   到達しない既定 5 か所は表引きで消える。**どこにも残らない文言が 2 つできる**: `"このノート内を検索  "`（`search_direction_label` の既定）と `L"実行ファイルの場所が取得できません。"`
   （`main.c` の既定）。どちらも到達しないので表示は変わらない。同一性の単体はこの 2 件を「消える」と明示する。
2. **application の `failure_lines[]` は outcome → ID の表になる。** `static const enum ui_text failure_lines[] = { [FOLIO_STATE_X] = UI_TEXT_FAILURE_X, … }` で、
   `folio_state_failure_line(outcome, language)` は `ui_text_line(failure_lines[outcome], language)` を返す（引数が 1 つ増える。呼び出し側は `folio_state_language` の値を渡す）。
   `READY` と `CANCELLED` は `UI_TEXT_EMPTY` を共有する。CNF-009 の既存の 2 行はそのまま効き、`state_tests.c` の期待表は**変更前の文言そのまま**で残す。
   core の `folio_command.c` の `label` は `enum ui_text` の欄になり、`folio_command_label(command, language)`・`folio_command_matches(…, language)`（3 → 4 引数・C-012 の上限）・
   `folio_command_listed_count` が `ui_text_line` 経由で読む。
3. **句の中の語と数は置換子、句どうしの並置は許す。** 置換子は `{k}` `{n}` `{offset}` `{name}` `{from}` `{to}` の 6 つ。core の純関数
   `[[nodiscard]] enum ui_text_format_outcome ui_text_format(const struct ui_text_request *_Nonnull request, char *_Nonnull out, size_t capacity)`
   が埋める。`struct ui_text_request`（`src/core/ui_text_request.h`・完全型・全メンバーが独立に妥当）は `enum ui_text id; enum folio_language language; size_t k; size_t n;
   size_t offset; const char *_Nullable name; const char *_Nullable from; const char *_Nullable to;` で、`NULL` は空文字列。結果は `ui_text_format_outcome.h` の
   `UI_TEXT_FORMAT_READY` / `UI_TEXT_FORMAT_TOO_LONG`（値名は表の ID と接頭辞が重ならないようにする）。
   **`capacity` は UTF-8 のバイト数で終端を含む**（呼び出し側のバッファの大きさ。`replace_status` は 512・`fill_pending` は 600 を保つ）。`TOO_LONG` のときは `out` を空文字列にし、
   UI は現行と同じ振る舞い（`replace_status` は何も出さない・`fill_pending` は `false` を返して面の初期化を失敗させる）。`{name}` `{from}` `{to}` はノート名（最大 255 バイト）なので
   **`TOO_LONG` は実行時に起こりうる**（`{from} → {to}` の最大 516 バイトは 600 に収まる）。数は `append_number` と**同じ規則**（10 進・先頭のゼロ無し・桁区切り無し・0 は `"0"`）で
   手書きし（core は `<stdio.h>` を使えない・ARC-003）、単体で `append_number` と同じ出力を固定する。**左から 1 パスで書き、埋めた内容は再走査しない**（`{k}` という名前のノートが壊れない）。
   表の全 ID の置換子が 6 つの集合に含まれることは単体が全 ID を回して固定し、実行時には調べない。表の文言に `{` `}` は現在 0 件で、置換子以外の `{` は置かない。
   置き換える **5 か所**: `search_status` = 向きの札（3 ID・末尾の 2 スペース込み）＋結果の句（`UI_TEXT_STATUS_SEARCH_FOUND` = `{k} / {n} 件` ほか 1 句 1 ID）、
   `replace_status` = `UI_TEXT_STATUS_REPLACE_COUNT`（`{name} / {k} 件`）、`failure_status` = 失敗の 1 行 ＋ `UI_TEXT_STATUS_REPLACE_POSITION`（` 位置 {offset}`・先頭のスペース込み。
   添える条件は ADR 0028 決定 9 のとおり UI）、`fill_pending` = `UI_TEXT_PROMPT_PENDING_RENAME`（`{from} → {to}`）、`show_pending_reason` = 失敗の 1 行 ＋ `" "` ＋
   `UI_TEXT_PROMPT_PENDING_EXPLANATION` の並置（変換失敗の退避は決定 5 で消え、長さ超過の退避は現行どおり説明だけを出す）。「1 件」は固定の句。
   **単位 A の字面は変更前と同一**であることを単体と Win32 部品 probe で確かめる。
4. **ヘルプ 18 行は 1 行 1 ID のまま移す。** 欄分けと非翻訳語の分離は単位 C（ADR 0032）。label の `SET` / `SUBSTITUTE` の構文もこの単位では字面のまま。
5. **UTF-16 への変換は確保しない固定長で行い、W 系 API へ表の文言を渡す全経路がそれを使う。** core の `utf16_text` に
   `[[nodiscard]] enum utf16_text_fill_outcome utf16_text_fill(const char *_Nonnull utf8, char16_t *_Nonnull out, size_t capacity, size_t *_Nonnull written)`
   （4 引数・`capacity` は終端を含む単位数・`written` は終端を除く書いた単位数・`UTF16_TEXT_FILL_READY` / `TOO_LONG` / `MALFORMED`。専用ヘッダ。既存の `utf16_text_outcome` と
   統合しないのは、`utf16_text_create` が決して返さない `TOO_LONG` を結果型に混ぜないため）を足す。
   表の 1 行の上限は core の定数 `ui_text_unit_limit = 256`（`ui_text.h`・`constexpr`。日本語の最長 64 の 4 倍）で、**表の全 ID が収まること**を単体が全 ID を回して固定する。
   表の文言をそのまま渡す経路（`main.c` の `report_utf8` / `report` の 3 か所 / `adapter_failure`、`name_prompt.c` の `prompt_title` / `prompt_hint` / `prompt_accept` /
   `prompt_close` / `label` / `button` / `submit` / `show_pending_reason`、`drawer_window.c` の折畳印と `NENE FOLIO`）はスタックの `char16_t[ui_text_unit_limit]` に写す。
   置換子で組んだ句やノート名を渡す経路（`draw_utf8` / `measure_utf8`・`fill_pending`）は呼び出し側の上限（既存の 600 / 512、描画は ui の定数）で大きさを決めたスタックのバッファに写し、
   **描画のたびの `utf16_text_create` の確保を無くす**。`TOO_LONG` / `MALFORMED` なら何も出さない（表の値は単体で収まりが固定される。ノート名由来の超過は現行と同じく出さない）。
   確保に失敗したときの `L"記憶域が足りません。"` 系の直書きとその退避の分岐は**経路ごと消える**（単位 A で唯一の振る舞いの変化で、確保失敗のときだけ。ui と app は分岐網羅の対象外なので数字は動かない）。
   `adapter_failure()` は `persistence_adapter_outcome` → `enum ui_text` の表（`main.c`）になり、`lineTables` に `persistence_adapter_outcome.h` → `main.c` の行を足す。
6. **文言の置き場所は字句検査 CNF-010 で守り、規約は C-018 として書く。** CNF-010: 「`src/` の C ソース（`.c` `.h`）の**文字列**リテラルで、(a) 非 ASCII のバイトを含むもの、
   (b) ワイド／`char16_t`／`char32_t` リテラル（`L"…"` `u"…"` `U"…"`）で `\x` `\u` `\U` のエスケープを含むもの、は `eng/conformance-rules.json` の `textCatalog`（`src/core/ui_text.c`）以外に
   置けない」。ナローの `"\xEF\xBB\xBF"`（BOM のバイト列）は (b) に含めないので掛からず、例外の設定は要らない。**文字リテラル（`'\x1b'`）は対象外**。コメントは対象外だが
   `c_code()` は文字列も潰すので使わず、コメントの選択肢を先に置いた 1 本の字句走査で文字列リテラルだけを取り出す別関数を書く。`tests/` `out/` `eng/` は対象外。
   結合や `#define` による回避は見えないので、第 2 の経路を**見つけやすくする**検査である。検出語（許可するファイル・拡張子）は設定に置く。
   `tests/conformance/test_conformance.py` に正例・反例、`eng/prove-gates.py` に反例（`folio_window.c` に非 ASCII のリテラルを 1 つ足すと落ち、戻すと通る）、`docs/quality/gate-proofs.md` に行、
   `docs/QUALITY_GATES.md` に CNF-010、`docs/CODING_RULES.md` に **C-018「表示文言を直書きしない。`ui_text` の ID を足す」**（機械強制 = CNF-010）。
   **ADR のコミットで QUALITY_GATES に CNF-010 を `planned`、C-018 を対にして置き、実装のコミットで `active` に変える**（ADR 0001。CNF-006 は同じコミットに定義があれば未定義 ID にしない）。
   フォント名は ASCII なので掛からず、この単位では触らない（単位 C・ADR 0029 決定 11）。
7. **`docs/ARCHITECTURE_CONSTITUTION.md` の ARC-004 の表に「利用者に見える文言 → core `ui_text`（置換子の組み立ても core・言語は application が渡す）」の行を足す。**
   `docs/GLOSSARY.md` に `ui_text`・置換子・`folio_language`。`SPECIFICATION.md` の文言は変わらないので触らない。
8. **翻訳・言語の切り替え・フォント・ヘルプの欄分けはこの単位では行わない**（単位 B・C）。永続化・スキーマ・ゲートの閾値・除外は変えない。

## 2026-09-22 の補正（決定本文は書き換えない）

実装（#74）で決定の字面と食い違った所を記録する。**決定 1〜8 の本文は書き換えない**（当時の判断の記録として残す）。

1. **`ui_text_format` は `ui_text.c` に置く。** 決定 3 は関数の置き場所を書いていないが、CNF-002 は「実装ファイルの
   外部関数はファイル名の接頭辞」を字句で数えるので、`ui_text_format` を別ファイルにすると `ui_text_format_` の
   接頭辞を要求される。表を引く関数と同じファイルに置き、補助の型は作らずに**数えてから書く 2 段**で実装した。
2. **`L":"` と `L"NeNe Folio"` は動かさない。** どちらも ASCII なので CNF-010 は掛からず、前者は Ex の
   接頭辞（文法の一部）、後者は窓の題（製品名）である。棚卸しの「表示に使う ASCII だけの 11 件」も同じ理由で動かさない。
3. **`UI_TEXT_TITLE_RECOVERING` は `TITLE_` の群に置く。** 決定 1 の群の並びには無かったが、パンくずが実ファイル名の
   代わりに出す語なので `UI_TEXT_TITLE_UNTITLED` と同じ群が自然である。
4. **`UI_TEXT_GLYPH_MINUS` は表では UTF-8 の実体（U+2212）で持つ。** 変更前は `L"\x2212"` のワイドのエスケープだったが、
   表の正本は UTF-8 の `char` なので、同じ符号位置をそのまま書く。同一性の検査はこの 1 件だけ綴りが違うことを明示する。
5. **どこにも残らない文言は 3 つ。** 決定 1 は 2 つ（`"このノート内を検索  "`・`L"実行ファイルの場所が取得できません。"`）と
   書いたが、決定 5 が消す OOM の退避のうち `name_prompt.c` の
   `L"エラー表示の記憶域が不足しています。入力は残っています。"` も表に入らないので 3 つになる。
   3 つとも「到達しない既定」か「確保に失敗したときだけ」なので、通常の表示は変わらない。
6. **`self` を持たない関数は言語を引数で受ける。** 決定 1 は「UI が `folio_state_language` から受けて渡す」としか
   書いていない。`breadcrumb_note` / `toggle_room` のように状態を持たない小さな関数は、
   呼び出し側が引いた `enum folio_language` をそのまま引数で受ける（core の `ui_text_line` と同じ形）。
7. **状態を作れていない経路だけは `FOLIO_LANGUAGE_JA` を直に渡す。** `main.c` で `folio_state_create` が失敗したときと
   `wWinMain` のアダプタ生成の失敗では、言語を尋ねる `struct folio_state` がまだ無い（`folio_state_language` は
   `_Nonnull` を受けるので `nullptr` を渡せない）。単位 C で設定から言語を読むようになっても、設定を読む前の
   この 2 経路は既定の言語で出すほかない。
8. **CNF-010 のエスケープは字面ではなく実際のエスケープで数える。** 決定 6 は「`\x` `\u` `\U` のエスケープを含む」と
   書いているが、部分文字列で数えると `persistence_adapter.c` の `L"\\\\?\\UNC\\"`（UNC のパス接頭辞）が
   「`\U` を含む」として掛かる。リテラルを左から走査し、`\` の**次の 1 字**が `x` / `u` / `U` のときだけ数える。

9. **取り込んだ #69 のページ送りは置換子へ移す。** main の `d238c01`（#69）がヘルプのページ送り「1/2」を
   `append_number` + `"/"` + `append_number` の連結で足していた。この単位で active にする C-018 は
   「語や数を連結して句を作らない」なので、連結のまま残さず `UI_TEXT_STATUS_PALETTE_PAGES = "{k}/{n}"` を
   表に足して `ui_text_format` に通した（表は 123 → 124 値）。**描かれる字面は「1/2」のまま**で、
   これは #74 の前の木には無い唯一の新しい ID である。

## 却下した選択肢

- 文字列キーの ID・原文をキーにする: 打ち間違いがコンパイルで落ちない。原文を直すたびに参照が壊れる。
- `ui_text_line(id)` の 1 引数で始め、単位 C で言語を足す: 全呼び出し（5 ファイル・約 130 か所）の書き換えになるか、core に可変の「現在の言語」を持つ誘惑を生む（ARC-005）。
- `wchar_t` の表: 正本は UTF-8 の `char`（RTF や失敗の 1 行も UTF-8 を要る）。
- `ui_text_format` に可変長引数: 型が無く検査が届かない。要求を構造体 1 つに束ねる。
- 置換子を実行時に検証: 表は静的なので単体で全 ID を回せば足りる。
- OOM の退避文言を検査の例外にする・ASCII にする: ADR 0029 で却下済み。
- `utf16_text_fill` が収まらないとき切り詰めて出す: 途中で切れた文言を見せない。
- `utf16_text_fill` の結果を `utf16_text_outcome` に統合: `utf16_text_create` が返さない値が混ざる。
- エスケープ条項をナローのリテラルにも掛ける: BOM のバイト列 2 件を巻き込み、例外の設定が要る。
- ヘルプ行を単位 A で欄に分ける: 字面同一を保てない（ADR 0029）。
- 同字面のコマンド名と札を 1 ID に統合する、または `draw_actions` の共有を分離する: 現行の経路を動かさない。
- 字形 8 つを個別の ID にする: 単独で描く字形は 2 つだけで、他は語と同じ 1 リテラル。

## 検証

core: `ui_text_line` の全 ID が空でない（`UI_TEXT_EMPTY` 以外）・全 ID の置換子が 6 つの集合に含まれる・全 ID が `ui_text_unit_limit` に収まる・`ui_text_format` の境界
（`NULL` の name・`TOO_LONG` で空文字列・置換子なし・同じ置換子が 2 回・埋めた内容の `{k}` を再走査しない・数の書式が `append_number` と同じ）・`utf16_text_fill` の境界
（空・ちょうど・1 単位超・サロゲート・壊れた UTF-8・`written`）。`folio_language` は 1 値。
application: `failure_lines[]` の全値が変更前の文言と同一（`state_tests.c` の期待表はそのまま）・`folio_command_label` の 15 値が同一・`folio_state_language` が `JA`。
ui / app: 状態行 5 か所の組み立てが変更前と同じ字面（Win32 部品 probe・単体）、`report` と `adapter_failure` と名前入力面の題・案内・ボタンの字面が同じ、折畳印の幅が同じ。
ゲート: CNF-010 の正例・反例（`tests/conformance`・`prove-gates.py`）、`lineTables` の 2 行の追加、分岐網羅が 90% を下回らない（core に増える `ui_text_format` /
`utf16_text_fill` の分岐を単体で通す。ui / app の退避が消えても数字は動かない）。
文言の同一性: 変更前の文言を機械的に抽出した表（`out/design/2026-09-22/ui-text-inventory/strings.tsv`）と変更後の `ui_text` の表を 1 字ずつ突き合わせる単体（消える 2 件を明示）。
実機の目視: 字面が変わらないので hide の新しい項目は無い（統合チェックリストに「検索・置換の状態行と名前入力面が以前と同じに見える」を 1 行だけ足す）。Waivers: none。
