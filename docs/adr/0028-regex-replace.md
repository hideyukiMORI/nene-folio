# ADR 0028 — 正規表現置換は OS 同梱の ICU を adapters に閉じ、置換文字列の文法と組み立ては core が持つ

- 状態: 受理（設計リナ 2026-09-22。実装前に記録。実測は `out/design/2026-09-22/replace-probe/`（101 項目成功・ICU 72.1 / Unicode 15.1）。草案は現行コードに照らした読み取り専用の批評を経て直した）
- 日付: 2026-09-22
- Issue: #41（FR-023）。親計画 #36 の 5 節
- 規則: ARC-001 / ARC-002 / ARC-003 / ARC-004 / ARC-005 / ARC-007 / ARC-009 / ARC-010 / ARC-011、C-002 / C-003 / C-005 / C-006 / C-007 / C-008 / C-012 / C-013 / C-014 / C-016、QLT-009 / QLT-011 / QLT-013、CNF-002 / CNF-009

## 文脈

FR-023 は現在開いている md の**編集中の本文だけ**に正規表現置換を行い、対象名と一致件数を見せ、1 件置換／全置換を
editor へ Undo できる単位で反映し、保存は既存の履歴 → 原子的な書き戻しに渡す。Issue #41 は「エンジンは実測後の ADR で固定し、
未検証の自作実装を安易に追加しない」と求める。C には正規表現の標準ライブラリが無い。

実測（probe）で分かったこと:

- Windows 10 1903 以降には `icu.dll`（`icu.h`・SDK の `icu.lib`）があり、clang-cl から `uregex_*` を直接結べる。仕様の最低環境は
  2004 以降なので範囲内。`u_getVersion` は測定機で 72.1。配布物は増えない。
- 位置は UTF-16 のコード単位で、RichEdit の表示中の平文（ADR 0023: CR 1 つ区切り）の `EM_EXSETSEL` と 1 対 1。サロゲートペアは `.` に 1 件で一致し、置換で割れない。正規化はしない。
- 既定の行端の扱いで CR は行端になる（`UREGEX_MULTILINE` の `^` / `$` が CR の前後に一致、`.` は CR に一致しない）。`UREGEX_UNIX_LINES` を付けると壊れる。利用者が打つ `\n` は CR に一致しない（`\r` / `\R` / `\s` は一致する）。
- 大小無視（`(?i)`）は ICU の full case folding で、全角英字 `ａ`/`Ａ` や `ß`/`SS` も一致する。既存の検索（ASCII の英字だけ）とは意味が違う。インラインの `(?i)` `(?m)` `(?s)` が効く。
- 壊れたパターンは `UErrorCode` の名と `UParseError.offset`（1 起算のコード単位）で位置が分かる。既定では未知のエスケープ（`\q`）が黙って通る（`UREGEX_ERROR_ON_UNKNOWN_ESCAPES` を付けた後の挙動はまだ測っていない。実装の adapter probe で測る）。
- **ICU の置換文字列の文法は利用者に見せられない**: `\n` が `n` になる、`$` の後に数字が無いとエラー（`$100` が打てない）、`&` と `\1` に意味が無い、末尾の `\` が黙って消える。
- ゼロ幅一致で無限ループしない。件数は Java / ICU 流儀（`x*` on `axxb` は 4 件）。空パターンは文字数 + 1 件。
- 上限は `uregex_setTimeLimit` の「steps」で、ICU は時刻を読まずに数える。測定機で 1 step ≒ 0.15ms、`(a+)+b` は 2 文字ごとに 4 倍。stack limit は既定 8MB で足りる。
  **上限は 1 回の一致操作ごとに数え直される**ので、全一致の走査の総量は「一致の数 × 上限」まで膨らみうる（単発の `find` でしか測っていない）。
- 1.18MiB / 1 万行で literal の全一致 1.5ms、`\w+` 9ms、`(\w+)-(\w+)` 46ms。`uregex_setText` は本文を写さない（呼び出し側が本文を保つ）。
- `uregex_*` は `eng/symbol-allowlist.json` の非決定名に無く、`eng/symbols.py` は core と application だけを検査するので、adapters では使え、core / application から呼ぶと ARC-003 で落ちる。
  `eng/conformance.py` の platform ヘッダ検査と非決定名の字句検査も adapters を対象外にしている。

現行コードとの噛み合わせ（批評で分かったこと）:

- C-012 は引数 4 つまで。ADR 0023 は 7 引数で書いて実装で `struct note_search_query` に束ねた。同じ失敗を繰り返さない。
- `folio_command_parse` は空白で切った 1 語を別名と完全一致で照合する。`%s/a/b/g` は 1 語になりどの別名にも一致しない。
- 入力面の EDIT は `command_input` 1 つで、フォーカスが同じ窓内の別の子へ移ると入力面を閉じる（`dismiss_command_if_focus_moved`）。
- `folio_state_create` は `(persistence, appearance, out)` の 3 引数。
- UTF-16 の入口は 7 本（ADR 0024 決定 7 まで）で、すべて即座に UTF-8 へ写して所有する。`folio_state` は `char16_t` を所有していない。
- `folio_state` に「破棄」の意図は無い（`:q!` は UI が窓を壊すだけ）。
- `docs/CODING_RULES.md` 第 5 節の表は「実行ファイルは kernel32 以外の DLL を import しない（R1）」と書くが、既に user32 / gdi32 / dwmapi / comdlg32 / shell32 / advapi32 を
  `platformLibraries` 経由で import している。記述が現状と合っていない。

## 決定

1. **エンジンは OS 同梱の ICU（`icu.dll`）で、`src/adapters/win32` にだけ現れる。** `eng/architecture.json` の `platformLibraries.adapters_win32` に `icu` を足し、
   `CMakeLists.txt` で `nenefolio_system_link(nenefolio_adapters icu)` と結ぶ（ARC-002 の正典経路。`eng/targets.cmake` / `eng/symbol-allowlist.json` / `eng/conformance-rules.json` は変えない）。
   暗黙リンクなので `icu.dll` が無い環境では起動しない。`SPECIFICATION.md` の対応環境に「`icu.dll`（Windows 10 1903 以降に同梱）」を注記する。
   **OS 同梱ライブラリは `platformLibraries` が正本で、`runtimeDependencies`（第三者の配布物）には入れない。** `docs/CODING_RULES.md` 第 5 節の R1 の記述は
   「kernel32 と `eng/architecture.json` の `platformLibraries` に列挙した OS 同梱 DLL 以外を import しない」に改める（現状の user32 等と同じ扱い）。
   QLT-011 の版の固定は OS 同梱には適用できない: ICU の版は OS が決める。確認記録に測定時の版（72.1 / Unicode 15.1）を残し、Windows の更新で folding や属性の挙動が動きうることをリスクとして書く。
2. **application が port を宣言し、adapters が定義する。** `src/application/regex_port.h` に `struct regex_adapter;`（不完全型・C-006）と関数ポインタ 1 本の
   `struct regex_port` を置く。実装は `src/adapters/win32/regex_adapter.c`（`regex_adapter_port()`。`persistence_adapter` / `appearance_adapter` と同じ形）。
   操作は **1 回の呼び出しで閉じる粗い形**にし、`URegularExpression *` も本文のポインタも application に持たせない（`setText` が本文を写さない危険を構造で消す）:
   `scan(adapter, const struct regex_request *request, struct regex_matches *matches, struct regex_pattern_error *error)` → `enum regex_scan_outcome`
   （`READY` / `BAD_PATTERN` / `TIMED_OUT` / `TOO_COMPLEX` / `TOO_MANY` / `OUT_OF_MEMORY`）。
   `struct regex_request`（core・完全型・借りるだけ）は本文とパターンとその長さ。本文は長さ 0 でも実体のある番地（ICU は `NULL` を拒む）。
   `struct regex_matches`（core・完全型）は `matches` 配列・`capacity`・`count` の出入りで、`capacity` を超える一致は数えるだけ、`count` は総数。
   `struct regex_pattern_error`（application・完全型）は `offset`（1 起算のコード単位、不明なら 0）で、`BAD_PATTERN` のときだけ意味を持つ。ICU の `UErrorCode` は adapter が閉じた値へ写し、生の数値を上へ出さない。
   一致は **core** の `regex_match.h`（完全型・全メンバーが独立に妥当。`note_search_span` と同じ例外）: 全体の `{start, end}` と群 1〜9 の `{start, end, present}`。
   10 個以上の群はパターンとしては受け、10 番目以降を報告しない（`\1`〜`\9` だけが参照できる）。
   テストの偽物は `tests/unit` に同名の `struct regex_adapter` を定義し、台本どおりの一致を返す（自分では確保しない）。
   `folio_state_create` は `(persistence, appearance, regex, out)` の 4 引数になり **C-012 で飽和する**。次にポートを足す単位（#38）は `struct folio_ports` に束ねる。
3. **パターンの旗は固定**: `UREGEX_MULTILINE | UREGEX_ERROR_ON_UNKNOWN_ESCAPES`。`UREGEX_UNIX_LINES` は付けない。大小無視は `(?i)` に任せ、チェックボックスを作らない。
   検索（ADR 0023）の「ASCII の英字だけ大小無視」とは意味が違うことを操作表に書く（正規表現は Unicode の規則で、全角英字も畳む）。
4. **上限は時計を読まず、閉じた値で断る。** (a) 1 回の一致操作あたりの steps は **adapter の定数** `regex_adapter.c` に置き（`uregex_setTimeLimit`。2,000 steps・測定機で約 300ms）、
   core / application は `TIMED_OUT` という値しか知らない。`U_REGEX_STACK_OVERFLOW` は `TOO_COMPLEX`。stack limit は既定のまま。
   (b) 一致の総数の上限は core の定数 `regex_match_limit = 1,000,000` で、超えたら `TOO_MANY`（走査を打ち切る）。
   (c) 出力の上限は core の定数 `note_replace_limit = 4,000,000 コード単位`（約 8MB）で、組み立ての前に必要長を数えて超えれば `TOO_LARGE` で何も変えない。
   記録: (a) は操作ごとに数え直されるので、全走査の最悪は「一致の数 × 上限」まで膨らむ。一致 1 件ごとに上限近くまで使うパターンは実用では見ないので、この単位では受け入れる。
5. **置換文字列の文法は自前で、組み立ては core の純関数。** ICU の `replaceAll` / `appendReplacement` は使わず、必要長の preflight も使わない（数えるのは core）。
   `src/core/replace_template`（不完全型・create/destroy）が置換文字列を解析し、`src/core/note_replace` が本文と一致の列と template から新しい本文を組み立てる
   （必要長を数える → 呼び出し側が確保 → 書く、の 2 段）。文法は Vim 寄りの最小: `&` と `\0` = 一致全体、`\1`〜`\9` = 群（無い群は空）、
   `\r` と `\n` = CR（RichEdit の段落区切り）、`\\` = `\`、`\&` = `&`、`\/` = `/`。`$` は普通の文字。それ以外の `\x` と末尾の単独 `\` は `BAD_TEMPLATE` で拒む（黙って消さない）。
6. **application は「置換の下見（preview）」を 1 つ所有し、宛先と本文で照合する。** 下見は core の `struct replace_preview`（不完全型・create/destroy の対・C-016）で、
   **UTF-16 の本文の写し**・一致の列・件数・宛先（文書の種類とカテゴリ名とノート名。`note_corpus` と同じく名前で持つ）を持つ。
   これは **C-014 の名指しの例外**である: 位置が `EM_EXSETSEL` と 1 対 1 でなければならないので UTF-8 へ写さない。`folio_state` が `char16_t` を所有するのはこの 1 か所だけ。
   意図は 2 本で、どちらも UTF-16 の入口（**8 本目・9 本目**。ADR 0024 決定 7 の 7 本目に続く）:
   - `folio_state_preview_replace(state, const struct replace_request *request)`（本文・パターン・置換文字列とその長さを束ねる）。編集中でなければ `NOT_EDITING`、
     パターンが空なら `REPLACE_NO_PATTERN`、scan と template の結果を閉じた値へ写す（`REPLACE_BAD_PATTERN` / `REPLACE_BAD_TEMPLATE` / `REPLACE_TIMED_OUT` / `REPLACE_TOO_COMPLEX` /
     `REPLACE_TOO_MANY` / `REPLACE_TOO_LARGE`。写像は 1 関数に置く）。成功なら古い下見を捨てて新しい下見を持つ。件数はゼロ幅一致も 1 件。
   - `folio_state_apply_replace(state, const struct replace_apply *apply, struct replace_edit *_Nullable *out)`。`replace_apply` は現在の本文・その長さ・anchor（選択。`note_search_span`）・
     `REPLACE_ONE` か `REPLACE_ALL`。**渡された本文が下見の写しと同じでない、または宛先が今の文書と違えば `REPLACE_STALE`** で何も変えない（同じ本文の別ノートへ適用しない）。
     反転した anchor は `REPLACE_BAD_SPAN`。`REPLACE_ONE` は anchor の開始以降で最初の一致（無ければ先頭から巡回）を 1 件、`REPLACE_ALL` は全部。
     結果 `struct replace_edit`（core・不完全型）は `{start, end, 置き換える文字列}` で、1 件なら一致の範囲、全部なら本文全体（0〜長さ）と新しい本文。
   下見を「捨てる契機」は列挙しない。宛先と本文の照合で古い下見は必ず `REPLACE_STALE` になるので、下見は次の下見か `folio_state_destroy` まで残してよい。
   application は RichEdit に触らず、選択も持たない（ADR 0023 決定 3 と同じ境界）。
7. **UI は結果を RichEdit へ 1 つの Undo 単位で反映する。** `note_pane_replace(pane, start, end, text, length)` を 1 つ足し、`EM_EXSETSEL` → `EM_REPLACESEL(TRUE)` で行う
   （`EM_REPLACESEL` 1 回 = Undo 1 単位。`EN_CHANGE` が出て番号の表の印は既存の経路で付く。`streaming` の印も `EM_SETMODIFY` も触らない）。
   1 件のあとの選択は挿入した文字列の直後に置き、同じ欄で下見をやり直す（Vim の `n` → 置換の繰り返しと同じ手触り）。
   全部のあとはキャレットを本文の先頭に置き、**最初に見えていた論理行を戻す**（ADR 0026 決定 6 と同じ `note_pane_first_visible_line` / `note_pane_scroll_to_line` の対。
   行数が桁を跨げば帯の幅は既存の `gutter_resized` の経路で追随する）。そのあとも下見をやり直し、件数（普通は 0 件）を出して欄は開いたまま。
   保存は既存の Ctrl+S / 編集を抜ける経路で、置換そのものは md を書かない。
8. **入口は 3 つで、同じ意図へ渡る。**
   (a) 右ペインの「操作」メニューとパレットの「置換」（登録表に `FOLIO_COMMAND_REPLACE`。`folio_command_listed` は真）は**置換の欄**を開く。
   欄は ADR 0016 の入力面で、検索欄と同じ場所に、パターン・置換文字列の **2 つの EDIT** と、自前描画の「1 件」「すべて」のボタン（検索欄の「前へ」「次へ」と同じ形）、
   右に状態の 1 行。**入力面の「自分の欄」を 1 つから集合へ広げる**: `on_command` と `dismiss_command_if_focus_moved` は集合に属する HWND を自分と見なし、
   Tab は `command_key_down` が自前で 2 欄を行き来させる（control id は 4 と 5）。打つたびに下見を取り直す。Enter = 1 件、Ctrl+Enter = すべて、Esc で閉じて元のフォーカスへ戻す。
   状態の 1 行は「失敗の 1 行（`BAD_PATTERN` なら末尾に「位置 N」）」が「対象名 / k 件」より優先する。
   (b) Ex `:%s/パターン/置換/[g]`（`FOLIO_COMMAND_SUBSTITUTE`。`listed` は偽・引数は `FOLIO_ARGUMENT_SUBSTITUTE`）は欄を開かず直接適用する。
   `g` があれば全部、無ければ**各論理行の最初の一致だけ**（Vim と同じ。選別は core）。
   解析: `folio_command_parse` は別名の照合の**前**に「トークンが `%s` で始まり、その直後が英数字でない」ときだけ `SUBSTITUTE` と判定し、`argument` を区切り文字の位置にする
   （他の別名の照合規則は変えない。引数も増やさない）。区切りは `/` だけで、`folio_command_parse_substitute(text, length, out)`（`folio_command_parse_option` と同じ流儀・結果は `ex_substitute.h`）が
   エスケープされていない `/` でパターン・置換・旗に分ける。パターンの `\/` はそのまま渡す（ICU で `/`）。置換の `\/` は決定 5 が `/` にする。旗は `g` だけで、他の旗・区切りの不足・空のパターンは未知のコマンドと同じ 1 行。
   **`:s`（`%` なし・行の範囲）はこの単位では意図して未対応で、未知のコマンドと同じ 1 行になる**（別名に無いので自然にそうなる。後続で `:s` を足すときはここを変える）。
   (c) `FOLIO_COMMAND_REPLACE` を閲覧中に開くと `NOT_EDITING` の 1 行を欄に出し、本文は変えない（開くだけではモードを変えない・ADR 0016 決定 4）。
   Ctrl+H は割り当てない（操作表の既存の決定）。
9. **失敗の文言は `folio_state_outcome` の表に足す**（ADR 0027 の経路。`failure_lines[]` と `state_tests.c` の表の 2 か所に同じコミットで足す。CNF-009 が網羅を守る）。
   `BAD_PATTERN` の位置は文言に埋め込まず、`folio_state_replace_error_offset()`（直前の下見が `REPLACE_BAD_PATTERN` のときだけ有効、それ以外は 0）から UI が得て添える。
10. 永続化・md・台帳のスキーマは変えない。パターンと置換文字列は保存しない。ゲートの閾値・除外は変えない。
    確保失敗は core / application の確保（写し・一致の列・template・組み立ての出力）を `eng/coverage.py` の注入で通す。
    **adapter の中（`uregex_open` と ICU 内部）の確保失敗は測っていない**（`u_setMemoryFunctions` はプロセス全体で `u_init` の前にしか差し替えられない。QLT-009 の「置いていない層」）。

## 2026-09-22 の補正（決定本文は書き換えない）

実装（core `557ceb9` / adapters `cb659f1` / application `802e2de` / ui）で決めたことと、決定の列挙との差を記録する。
受理してからの追加決定も含む。**決定 1〜10 の本文は書き換えない。**

### 実装で足した・変えた形

1. **`replace_scope` は 3 値**（`REPLACE_ONE` / `REPLACE_ALL` / `REPLACE_LINE_FIRST`）。決定 6 は 2 値で書いたが、
   決定 8(b) の「`g` が無ければ各論理行の最初の一致」を同じ `note_replace` の経路で通すために 3 値目を足した。選別は core。
2. **下見は解析済みの置換文字列（`replace_template`）も所有する。** 決定 6 は「本文の写し・一致の列・件数・宛先」だけを挙げていたが、
   置換文字列も下見の一部でなければ「打った置換文字列とは違うものを当てる」が起こりうる。
3. **宛先に文書の種類（`folio_document_kind`）は持たない。** core は application の列挙を見られないので、
   宛先はカテゴリ名とノート名だけ（無題はノート名が空文字列）。同じカテゴリに無題は 1 つしかないので区別は付く。
4. **`folio_state` が走査の入れ物（`found` / `found_capacity`）を持ち、1〜2 周走査する。** `regex_matches` の `capacity` を
   超えたら数えるだけ（1 周目）→ 入れ物を広げて書く（2 周目）。決定 2 は入れ物の所有者を書いていなかった。
5. **`folio_state_replace_count` を足した。** 決定 8(a) の「対象名 / k 件」を UI が読むための値で、決定 9 の
   `folio_state_replace_error_offset` と対になる。
6. **`note_pane_replace` は 4 引数ではなく 3 引数。** 決定 7 は `(pane, start, end, text, length)` の 5 引数で書いたが
   C-012（引数 4 つ）を超える。`struct note_search_span` へ束ね、長さは `EM_REPLACESEL` が終端で決めるので落とした
   （`(pane, span, units)`）。ADR の署名を書く段階で束ねる型を決める、という運用の注意（引き継ぎ 2026-09-22）の再発である。

### 追加決定（設計リナ・実装時）

7. **Ex `:%s` が 1 件以上置き換えたら、他の Ex と同じく黙って閉じる。0 件なら Ex を閉じず、
   `draw_command_status` の場所に置換の欄と同じ形の「対象名 / 0 件」を出す。**
   `folio_state_outcome` に値は足さない（「一致が無い」は失敗ではない）。ui が `command_unknown` と同程度の印
   （`command_no_match`）を 1 つ持ち、欄に打てば消える。
8. **置換の失敗と `NOT_EDITING` は `inline_outcome` に入れ、モーダルの箱ではなく欄の中の 1 行にする。**
   打ち間違いに箱を出さない。記憶域不足と「本文を取り出せない」は従来どおり箱。
   `inline_outcome` は **`enum folio_state_outcome` の全 42 値を並べた `switch`** で、`case` を積んで
   `return true`（12 値）と `return false`（30 値）の 2 群に分ける。`default` は書かない（C-002）。
   値を足すと `-Werror,-Wswitch` がここを落とすので、**新しい結果ごとに「箱か 1 行か」を必ず決めさせる**。
   `||` の並びで書くと第 2 の表になり、追随を忘れても通ってしまうので採らない（2026-09-22 の独立レビュー前の修正）。
9. **ボタンの字面は「1 件」「すべて」**（検索欄の「◀ 前へ」「次へ ▶」と同じ形・矢印は付けない）。矩形は検索欄と共有する
   （`surface_button_rect`。下端の行の右端に 2 つ）。
10. **Ctrl+Enter は `WM_CHAR` の 0x0A、素の Enter は 0x0D で見分ける。** `GetKeyState` を読まない（ARC-007）。
    `WM_KEYDOWN` の `VK_RETURN` は置換の欄では飲み込むだけにして、判断は `WM_CHAR` に寄せた（部品 probe §5 で実測）。
11. **対象名はパンくずと同じ `folio_state_pane_title().note`。** 無題では「無題（未保存）」になる（第 2 の文言を作らない・ARC-004）。
12. **`FOLIO_COMMAND_REPLACE` を文書が無いときに実行すると、欄を開かず「ノートを選んでください。」を出す**
    （`begin_search` と同じ）。決定 8(c) の「閲覧中は欄を開いて `NOT_EDITING`」はそのまま。
13. **置換の欄が覚えている文字は欄そのものが持つ**（application も core も持たない）。開き直すと前のパターンが残り、
    プロセスを終えれば消える。決定 10 の「保存しない」と矛盾しない。

### 実測で分かったこと（確認記録 `docs/quality/2026-09-22-replace-checks.md`）

14. **長さ 0 のパターンは adapter で `BAD_PATTERN` になる。** 設計 probe は `uregex_open(pattern, -1, 旗 0)` で
    「文字数 + 1 件」だったが、製品の adapter は長さを明示して渡すので ICU が拒む。
    application が先に `REPLACE_NO_PATTERN` で断るので、この経路は製品では走らない（決定 6 のとおり）。
15. **`TOO_COMPLEX`（`U_REGEX_STACK_OVERFLOW`）は測っていない。** 既定の 8MB スタックと 2,000 steps では
    時間の上限が先に効く（決定 4(a) の予測どおり）。写像そのものは偽 adapter の単体で覆っている。
16. **96 DPI・最小寸法 560×360 では、パレットの「キー操作を表示」を開くと操作の行が 0 行になる。**
    ヘルプ 16 行の時点で既にそうで、#41 の 2 行（置換の欄・`:%s`）を足しても変わらない
    （箱は `command_palette_rect` が使える高さへ丸め、ヘルプは下端から上へ積むので箱の外へはみ出さない）。
    既定（キー操作は閉じている）では最小寸法でも操作の行が 5 行見える。

### 独立レビュー（2026-09-22）で直したこと

17. **決定 6 の補足: 失敗した下見は前の下見を捨てる。** `replace_preview_holds()` は本文と宛先しか
    比べないので、「前の入力の下見」を残すと、パターンを空にして `REPLACE_NO_PATTERN` が出ている欄で
    Enter を押したときに 1 つ前の置換が本文に当たった。`folio_state_preview_replace` が READY 以外を
    返す経路をすべて 1 か所で受け、下見を捨てる。下見が無い適用は既存の `REPLACE_STALE` で何も変えない
    （結果の値は足さない）。**UI が下見の成否を見るのは表示のため**（出ている失敗の 1 行を STALE で
    上書きしない）であって、安全の正本は application 側である。
18. **`REPLACE_STALE` の文言を「本文か入力が変わったので、この置換は当てられません。もう一度入力してください。」に変えた。**
    本文の変化だけでなく「直前の入力が失敗して下見が無い」も同じ値になるため。検索ではなく入力を促す。
19. **`note_replace_build()` は `matches->count > matches->capacity` を `NOTE_REPLACE_PARTIAL_MATCHES` で断る。**
    決定 2 は「`capacity` を超える一致は数えるだけ」と書いたが、組み立てでそれを「入っているぶんだけ当てる」と
    読むと黙って部分適用になる。走査をやり直すのは呼び出し側の仕事なので、core は組み立てずに断る。
    application の `from_replace` は `REPLACE_TOO_MANY` へ写す（入れ物に収まらない一致 = 多すぎる、の意味）。
20. **ゼロ幅の一致を空文字列で置き換える「1 件」は、本文も選択も変えない。** 特別規則は足さない
    （Vim の `:s/x*//` も同じ）。操作表に 1 行書き、そのときは「すべて」を使うと案内する。
    あわせて「Enter（1 件）が探し始めるのは**いまの選択の開始以降**で、ノート内検索の `/` が選択の**次**から
    探すのとは 1 文字ぶん違う」ことも操作表に書いた（同じ一致をもう一度置き換えられる）。
21. **置換の直後はフォーカスが欄にあるので Ctrl+Z は欄の取り消しになる。** 本文を戻すには Esc で本文へ
    戻ってから Ctrl+Z（Undo 1 単位という決定 7 はそのまま）。操作表に 1 行書いた。
22. **記録（未解決）: `struct regex_match` は 200 バイトを超える**（全体の span + 群 9 個の span と present）。
    `regex_match_limit = 1,000,000`（決定 4(b)）の直下では、`folio_state` の `found` と下見の写しに
    **それぞれ約 200MB を 1 打鍵ごとに確保する**ことになり、出力の上限 8MB（決定 4(c)）と釣り合っていない。
    この単位では直さない。後続で上限の見直しか、`found` を下見へ move して二重に持たない形にする。

## 却下した選択肢

- PCRE2 の同梱: 新しい依存（QLT-011）で、第三者の C ソースが `-Werror` / clang-tidy / CNF-002 / C-002 / 分岐 90% と衝突し、通すには除外か閾値の変更（QLT-010）が要る。
- 自作エンジン: Issue #41 が戒める。Unicode 属性・folding・上限・ゼロ幅の前進規則の作り込みが大きく、検証済みの ICU が OS にある。
- ICU の `replaceAll` / `appendReplacement` に置換文字列を渡す: `\n` が `n`、`$100` がエラー、末尾の `\` が消える、群数で `$10` の意味が変わる。利用者の期待（Vim / sed）と食い違う。
- 置換を core で行わず adapter で完結させる: 置換文字列の翻訳は字句処理でありエンジンではない。core に置けば純関数として網羅でき、偽の adapter でも同じ組み立てを検証できる。
- 下見の写しを持たず、適用のたびに数え直す: 「下見と違う本文に適用する」を防げない（Issue の照合の要求）。写しは 1MB 級で、比較は memcmp 1 回。
- 下見を捨てる契機を列挙する（保存・切替・改名・移動・新規…）: 現行コードで 10 か所以上に散り、漏れる。宛先と本文の照合で構造的に防ぐ。
- `URegularExpression *` を application が持って `setText` を使い回す: 本文の所有権が契約になり、RichEdit の差し替えで宙に浮く。
- 大小無視のチェックボックス: `(?i)` で足り、検索の「ASCII だけ」と別の意味の真偽を UI に増やさない。
- 全ノートへの一括置換・`:s` の行範囲・確認付き置換（`c`）: この単位の範囲外（計画 5 節。`:s` は後続）。
- 時間の上限を ms で持つ・steps を core の定数にする: 時計を読む場所が増え（ARC-007）、環境依存の主張になる（QLT-013）。steps は ICU の実装の単位なので adapter が持つ。
- 置換の欄を検索欄と共用（欄を 1 つ増やして切り替える）: 検索は Enter で進み閉じない、置換は Enter で適用する。同じ欄で意味が変わるより、別の面にする。
- `runtimeDependencies` に ICU を書く: あの一覧は第三者の配布物（版を固定できるもの）のためで、OS 同梱の DLL は user32 等と同じく `platformLibraries` が正本。二重に書かない。

## 検証

core: template の全記号と拒否（未知の `\x`・末尾の `\`）、組み立て（群の欠落・ゼロ幅・末尾の一致・CR の挿入・必要長と上限）、
`:%s` の分解（`\/`・`g` の有無・空のパターン・区切りの不足・`%s` の直後が英数字）、行ごとの最初の一致の選別、`regex_match_limit`。
application（偽 adapter）: 編集中でない・空パターン・各失敗値の写像・件数・`REPLACE_ONE` の anchor からの前進と巡回・反転 anchor・`REPLACE_STALE`（本文の差・宛先の差）・
下見の入れ替え・`folio_state_create` の 4 引数・確保失敗。
adapters（実 ICU probe）: 旗（`\q` が `BAD_PATTERN` になること）・CR の行端・サロゲート・群 9 個と 10 個目の切り捨て・`BAD_PATTERN` の offset・`TIMED_OUT`・`TOO_COMPLEX`・`TOO_MANY`・
count だけの呼び出し・長さ 0 の本文・毎打鍵の `uregex_open` を含めた 1 回の scan の時間（1MB）。
UI（Win32 部品 probe）: `EM_REPLACESEL` 1 回が Undo 1 単位で本文が戻ること、template が入れた CR が `note_pane_display_text` で CR 1 つに戻ること、置換後の選択位置、
全体置換後の最初に見える論理行、2 欄の Tab / Enter / Ctrl+Enter / Esc とフォーカスの復帰、`dismiss` が欄の集合で閉じないこと。
実機の目視（hide）: 欄の見た目と最小寸法（ヘルプが 1 行増える）、IME 変換中の Enter、1MB のノートでの打鍵の体感。Waivers: none。
