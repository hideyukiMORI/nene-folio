# ADR 0032 — 表示言語は `ui_text` の表を 3 列にして `settings.json` の版 3 で選び、言語ごとの face は core の表から引き、訳し残しと列の欠落は機械で守る

- 状態: 受理（設計リナ 2026-09-23。ADR 0029 の単位 C。実測 `out/design/2026-09-23/language-probe/` に基づき、現行コード（main `2448987`）に照らした読み取り専用の批評（止める所見 7・直したい所見 12）を経て直した）
- 日付: 2026-09-23
- Issue: #76（親 #38・最後の単位。統合で #38 を閉じる）
- 規則: ARC-001 / ARC-002 / ARC-003 / ARC-004 / ARC-005 / ARC-007 / ARC-010、C-002 / C-003 / C-005 / C-012 / C-018、CNF-002 / CNF-006 / CNF-009 / CNF-010 / CNF-011（この単位で定義）、QLT-009 / QLT-010
- 関連: ADR 0016 決定 1（パレットの照合）、ADR 0025（設定の版）、ADR 0026 決定 6（最初に見える論理行の対）、ADR 0029 決定 4〜6・11、ADR 0030（`ui_text_line(id, language)`・`folio_language`・置換子・CNF-010）、ADR 0031（設定画面・`note_pane_recolor`・`note_pane_restore_selection`・`render_pane_document`）、#42（同梱フォント・後続）、#82（名前入力面のテーマ）

## 文脈

実測（`language-probe`・Windows 11 26200・RICHEDIT50W）:

- `ui_text` の表は 131 値。ヘルプは 19 行。**8 エントリは clang-format（`ColumnLimit: 100`）で隣接リテラル 2 つに割れている**（3 列にすればさらに増える）。`eng/conformance.py` の `string_literals()` はリテラル 1 つずつを返す。
- **EDIT は `WM_SETFONT` を配り直すだけで本文・選択・Undo・変更印が不変**（`EN_CHANGE` 0）。**RichEdit の既定書式の face は `tomSuspend` ＋ `SCF_DEFAULT` ＋ 変更印の退避復元で、本文・選択・Undo の段数・変更印が不変のまま変わる**（`SCF_ALL` は要らない）。
  face を変えると ASCII の多い本文で**再折り返し**が起き、表示行数と `EM_GETFIRSTVISIBLELINE` が動く（`EM_LINESCROLL` では同じ論理行に戻らない）。キャレットが画面の外にあると `EM_SETCHARFORMAT` はキャレットまでスクロールする（色でも face でも。#75 の再着色にも当てはまる）。
- 字形の欠け: **Microsoft YaHei UI は ja / zh とも 0 で唯一「1 行が混ざらない」**。Yu Gothic UI は zh 5 字だけリンク。**Segoe UI と Consolas は CJK を一切持たない**（Consolas は全字が 1 つのリンク先へ揃う）。RichEdit は face に関わらず簡体字を Microsoft YaHei へ束ねる。
  `CreateFontW` は無い face 名でも失敗せず、存在確認は `EnumFontFamiliesExW`（gdi32・宣言済み）でしかできない。Noto Sans SC はこの機械に無い。
- `TEXTMETRIC`: 96 DPI のヘルプ行 14px に YaHei UI（16px）が入らず、144 DPI の 21px には Yu Gothic UI / Segoe UI / YaHei UI（23px）のどれも入らない。Consolas（13 / 20px）だけが入る。操作行 32px・ドロワーの行（30 / 34px）・番号の桁幅（6px）はどの face でも収まる。
- 帯幅（仮訳・Consolas 11 の主窓）: ヘルプ 428px に英語 5 行があふれる。欄の中の失敗の 1 行（`inline_outcome` が真の 14 値）は en で短縮が要る。箱で出る値（ja の 3 件があふれるのは現状の欠陥）は `MessageBoxW` が折り返す。名前入力面の説明の STATIC は既に 60px 高（`SS_LEFT` は語単位に折り返す）。UTF-16 の最長は en 135 単位（上限 256）。
- ヘルプ 19 行を機械的に欄に分けると意味として正しく分かれるのは 9 行だけ。**英語の説明語は ASCII なので「ASCII の語の多重集合の一致」は原理的に成り立たない**（`次/前` → `next/previous` で `/` の数も変わる）。
- パレットの部分一致 `contains_span` は `memcmp` で大小を区別し、英語の label（先頭が大文字）は小文字で打つと当たらない。ASCII の大小無視の `folded` は `index_filter.c`（`char`）と `note_search.c`（`char16_t`）に static で 2 つある。
- 設定画面: `command_box_height(8, 0) = 400px` で 560×360（使える高さ 316）では **5 行**しか見えず、150% でも 5 行。`adopt_settings_row` は行の添字をそのまま `folio_theme_choice` へキャストしており、行が増えると表の外を読む。`settings_navigate` に中間の見出しを飛ばす分岐は無い。
- `failure_box_show(owner, outcome)` は `FOLIO_LANGUAGE_JA` を直書き（呼び出し 8 か所）。`markdown_rtf.c` の fonttbl は `document_head[]` の 1 本の定数で、`markdown_rtf_create` / `_empty` の呼び出しは `folio_state.c` の 7 か所（起動時の `adopt_theme_choice`・`refresh_theme`・ノートを開く／保存するなど）。`note_pane.c` の既定書式の face は `editor_face` の `sizeof` 写し。`name_prompt` はモーダルで、言語を切り替えられる瞬間には存在しない。
- 版 3 は `folio_settings.c` に約 68 行で C-012 に収まるが、`parse_version_N` と `create` が 4 引数（上限ちょうど）になる。`enum folio_language` を分岐する `switch` は 0 か所。CNF-009 は `[値] =` を数えるだけで列の数と空を見ない。CNF-010 は `ui_text.c` を走査ごと飛ばす。`enumeration_values()` は `enum folio_language : unsigned char` を読める。
- Issue #76 は「言語ごとの UI フォントを ui の表に」と書くが、`markdown_rtf`（core）が同じ face を要るので ADR 0029 決定 11 のとおり core に置く。

## 決定

1. **言語は 3 値、設定は版 3。** `enum folio_language` に `EN` と `ZH_HANS` を足し（`JA` / `EN` / `ZH_HANS`）、`folio_language.h` に `constexpr size_t folio_language_count = FOLIO_LANGUAGE_ZH_HANS + 1;`（正本は列挙。単体が一致を固定）。
   `settings.json` は `{"version":3,"number":…,"theme":…,"language":"ja"|"en"|"zh-Hans"}`（既定 `ja`・キー順込み）。版 1・2 には既定値を埋め、書くのは常に版 3。
   **先に独立の refactor コミット**で `folio_settings.c` の解析と生成を「`struct folio_settings` を直に埋める」形（`parse_version_N(reader, struct folio_settings *)`・`create(const struct folio_settings *, out)`）に整えてから `language` を足す（4 引数の上限に版 4 で当たらないため。`default` / `with_number` / `with_theme` / `parse` が同時に変わる）。
   `language_names[]` は指示付き初期化子で、`lineTables` に `src/core/folio_language.h` → `src/core/folio_settings.c`（prefix `FOLIO_LANGUAGE_`）。`folio_settings_language` / `folio_settings_with_language`（写してから 1 つ変える）。
2. **表は 3 列。** `ui_text.c` を `static const char *_Nonnull const lines[][folio_language_count] = { [UI_TEXT_X] = {"ja", "en", "zh-Hans"}, … }` にし、`ui_text_line(id, language)` が列で引く（署名は変えない）。翻訳しない項は 3 列に同じ文字列。`UI_TEXT_EMPTY` だけ 3 列とも空。
   **列の網羅は新しい字句検査 CNF-011 で守る**（対応する規則は C-018。`planned` で ADR のコミットに置き、実装のコミットで `active`・`gate-proofs.md` に証明行）:
   `eng/conformance.py` が `textCatalog` の表の各エントリ `[値] = { … }` について、**`{…}` の中を深さ 0 のカンマで区切った要素の数**が `enumeration_values(folio_language.h)` の数と一致し、各要素は**隣接リテラルを連結してから**空でないこと（`UI_TEXT_EMPTY` だけ例外・設定 `textCatalog.emptyAllowed`）を見る。
   `string_literals()` は文字列の取り出しにだけ使い、数える単位にはしない。`tests/conformance` に正例・反例、`eng/prove-gates.py` に反例（1 列を消すと落ち、戻すと通る）。閾値・除外は変えない。
3. **翻訳の書き方と機械の守り。**
   - **ヘルプ 19 行は 1 行 1 ID のまま**（欄分けはしない）。**訳してはいけない閉じた語彙**を単体（`ui_text_tests.c`）が持ち — Ex の別名（`folio_command` の `names[]` から機械的に）・`:set` の語（`options[]` から）・鍵の名前（`Ctrl+h/j/k/l` `Ctrl+P` `Ctrl+F` `Ctrl+Shift+F` `Ctrl+N` `Ctrl+S` `Ctrl+Shift+S` `Ctrl+Enter` `F1` `F2` `F3` `Shift+F3` `Shift+Enter` `PgUp` `PgDn` `Esc` `Enter` `Tab` `gg` `G` `j` `k` `h` `l` `n` `N` `i` `:` `/` `?` `↑` `↓` `←` `→` `↑↓`）— **ja の行に現れる語彙の語は en と zh-Hans の同じ行にも現れる**ことを全 HELP・`COMMAND_*`・`SETTINGS_GUIDANCE` で固定する（`:set number` や `Ctrl+P` を訳す事故が落ちる）。語彙の外の語順・`/` の数・`・` は自由。
   - **構文の見本の部分だけ ASCII に揃える**: `UI_TEXT_HELP_EX_SUBSTITUTE` の `:%s/パターン/置換/[g]` → `:%s/pattern/replacement/[g]`、`UI_TEXT_COMMAND_SUBSTITUTE` の `（:%s/前/後/g）` → `（:%s/pattern/replacement/g）`（括弧の外の日本語はそのまま）。日本語の字面が変わるのはこの **2 件**で、単位 A の同一性の期待表はこの 2 件だけ更新する（確認記録に明記）。
   - **収まりの規約**: 欄の中に出る文言（HELP・`STATUS_*`・`COMMAND_*`・`SETTINGS_*`・`CHIP_*`・`ACTION_*`・`PROMPT_*`・`inline_outcome` が真の `FAILURE_*` 14 値）は 96 DPI・Consolas 11（主窓）／面の face 14（名前入力面）の箱の幅に収まるように書く。幅は Win32 部品 probe が 3 言語 × 対象 ID で測り、あふれる ID を 0 にする（訳を短くする）。箱で出る `FAILURE_*` は折り返すので対象外（ja の 3 件は現状の欠陥で対象外）。
     名前入力面の説明（`PROMPT_PENDING_EXPLANATION`）は既存の 60px の STATIC が語単位で折り返すので、3 言語で 2 行に収まることを probe で確かめ、収まらなければ面の高さを増やす。
   - **翻訳の書き手**: 英語と简体中文は実装リナが `ui_text.c` に直接書く（中間ファイルは置かない）。別の Opus が読み取り専用で用語の統一（ノート=note=笔记・カテゴリ=category=分类・索引=index=索引・閲覧/編集=View/Edit=查看/编辑・テーマ=Theme=主题・OS に従う=Follow the OS=跟随系统・保存=Save=保存・置換=Replace=替换 …）・敬体の統一・語彙の一致・幅の probe を見る。**簡体字は母語話者の確認を得ていない**ことを確認記録と統合チェックリストに書く（後日 `ui_text.c` の 1 列だけ直せる）。
4. **言語ごとの face は core の表 1 か所。** 新規 `src/core/ui_font.{h,c}`（型定義 0・関数 1）に `[[nodiscard]] const char *_Nonnull ui_font_face(enum folio_language)` を**指示付き初期化子の表**で置き（`JA` / `EN` → `Yu Gothic UI`、`ZH_HANS` → `Microsoft YaHei UI`。ASCII で `{ } \ ;` を含まないことを単体で固定）、`lineTables` に `folio_language.h` → `ui_font.c`。
   `docs/ARCHITECTURE_CONSTITUTION.md` の ARC-004 の表に「UI の書体の face → core `ui_font`」の行を足す。Issue #76 の「ui の表」は `markdown_rtf`（core）が同じ face を要るため core にする（ADR 0029 決定 11）。
   **英語に Segoe UI は使わない**（CJK を持たず、ノート名・カテゴリ名・訳し残しが全部リンク先の字形になる。幅は Yu Gothic UI とほぼ同じ）。
   face を当てる先: ドロワーの文字（`refresh_fonts` の 2 本。`WM_CREATE` から `folio_state_language` を引ける）、名前入力面（`initialize` の `CreateFontW` 1 か所。**次に開くとき**に選ぶ。モーダルなので切り替え中には存在しない）、RichEdit の既定書式（`note_pane_create` と決定 6。`szFaceName` へは `LF_FACESIZE` の境界付きで写し、収まらなければ `Yu Gothic UI`）、`markdown_rtf` の fonttbl（決定 5）。
   **主窓の自前描画（パンくず・札・ヘルプ・状態行・番号帯・EDIT）は Consolas のまま**（いまも日本語はリンクで描いており、簡体字は全字が 1 つのリンク先へ揃うので混ざらない。zh の face は 96 DPI のヘルプ行に、どの face も 144 DPI の行に入らない）。
   ui は起動時と切り替え時に `EnumFontFamiliesExW` で face の存在を確かめ、無ければ `Yu Gothic UI` に落とす（ui の 1 関数）。#42 が同梱フォントを入れたら `ui_font.c` の表だけを差し替える。
5. **閲覧の RTF の face は全経路で言語に従う。** `markdown_rtf_create` / `markdown_rtf_empty` は face を引数で受け（`create` は 4 引数・上限ちょうど）、`document_head` を実行時に組む。application の `render_pane_document` は言語を受けて `ui_font_face(language)` を渡し、**`folio_state.c` の 7 か所の呼び出し全部**（起動時の `adopt_theme_choice`・`refresh_theme`・ノートを開く／保存する・無題）がそこを通る（`settings.json` に `zh-Hans` がある起動でも最初から YaHei）。
   `folio_state_language()` は設定の値を返す。`[[nodiscard]] enum folio_state_outcome folio_state_set_language(state, enum folio_language)`: `SETTINGS_UNREADABLE` は断る・同じ値は書かない・**新しい設定と新しい閲覧文書を先に作る → `write_settings` → 差し替え**（ADR 0031 決定 3 と同じ順。`set_theme` と共通の static に寄せる）・`synchronize` を通さない・確保失敗は `OUT_OF_MEMORY` で不変。
6. **UI の切り替え（言語の採用後）**: (a) 文言は `ui_text_line` から引き直すだけなので主窓・ドロワー・`command_layer` を全面 `InvalidateRect`（メニューは開くたびに作る）。(b) ドロワーの `refresh_fonts` を作り直して `WM_SETFONT`。(c) RichEdit は**モードごとに順が違う**:
   - 編集: 最初に見える論理行を `note_pane_first_visible_line` で退避 → `note_pane_reface(pane, face)`（`tomSuspend` ＋ 変更印の退避 → `SCF_DEFAULT`（`CFM_FACE`）→ 復元。`SCF_ALL` は当てない。`EN_CHANGE` で表に印が付く）→ `note_pane_scroll_to_line` で論理行を戻す。
   - 閲覧: 論理行と選択を退避 → 決定 5 の新しい RTF を `render_pane` で流し直す → `note_pane_restore_selection` → `note_pane_scroll_to_line`（`reface` は呼ばない。流し込みが既定書式を捨てるため）。
   `note_pane_reface` は `note_pane_recolor`（4 引数で飽和）と別の 1 本。**#75 の `recolor_pane` にも「論理行の退避 → （再着色または流し直し） → 選択 → 論理行の復元」の同じ包みを足す**（キャレットが画面外のとき `EM_SETCHARFORMAT` がスクロールする実測への補正。ADR 0031 の補正節に記す）。
7. **入口**: 設定画面に 2 段目「言語」（見出し 1 ＋ `日本語` / `English` / `简体中文`。名前は各言語の自称で 3 列とも同じ）。`command_surface_rows(SETTINGS) = 8` で、**560×360 と 150% では 5 行しか見えないので一覧はスクロールで辿る**（`command_first` / `reveal_command_selection` は #75 で入っている。パレットと同じ）。
   行は **`settings_rows[]` の表 `{ID, 種別（`enum settings_row_kind`: `HEADING` / `THEME` / `LANGUAGE`）, 値}`** にし、採用は種別の閉じた `switch`（段が増えたらコンパイルが落ちる）、`settings_navigate` と `click_settings_surface` は `HEADING` を飛ばす。添字を `folio_theme_choice` へキャストする現在の形は無くす。
   Ex `:set language=ja` / `language=en` / `language=zh-Hans`（`options[]` に 3 語・`enum folio_option` 6 → 9・`execute_set_command` に `apply_language`）。**失敗の箱も言語を受ける**: `failure_box_show(owner, outcome, language)`（3 引数）にし、8 か所の呼び出しが `folio_state_language` を渡す（`FOLIO_LANGUAGE_JA` の直書きを消す）。窓の題と箱の題 `NeNe Folio` は製品名で翻訳しない。
   Ex の別名・`:set` の語・`settings.json` のキー・ファイル名・md は翻訳しない（ADR 0029 決定 5）。
8. **パレットの部分一致は ASCII の英字だけ大小を無視する。** `folded` を新規 `src/core/ascii_fold.{h,c}`（`ascii_fold_byte(char)` / `ascii_fold_unit(char16_t)`）に 1 本化し、`index_filter.c` と `note_search.c` の static を置き換え（ARC-001）、`contains_span` もそれを使う。ADR 0016 決定 1 の「大文字小文字は区別し」を**パレットの label と別名の部分一致に限って**補正する（Ex の完全一致 `folio_command_parse` は区別したまま）。ADR 0016 に補正節。
9. **やらないこと**: OS の表示言語への追従（初期は `ja`）、右から左の言語、md 本文と `SPECIFICATION.md` の翻訳、簡体字の母語話者による校閲（後日・`ui_text.c` の 1 列）、主窓の描画 face の変更、#42 の同梱フォント、`name_prompt` のテーマ（#82）。
10. **#38 は #76 の統合で閉じる。**

## 却下した選択肢

- ヘルプ行を「区画名・（鍵・説明）の列」に分ける: 19 行のうち 10 行が意味として分かれない（実測）。
- 「ASCII の語の多重集合が 3 言語で一致する」単体: 英語の説明語が ASCII なので原理的に成り立たない。閉じた語彙の包含に限る。
- 英語に Segoe UI: CJK を 1 字も持たず、ノート名が全部リンク先の字形になる。
- 主窓の自前描画を言語ごとの face にする: zh の face は 96 DPI のヘルプ行 14px に、どの face も 144 DPI の 21px に入らず、#69 の式を変える必要がある。Consolas のリンクで簡体字は 1 つの face へ揃う。
- 既存の本文の face を `SCF_ALL` で変える: `SCF_DEFAULT` だけで変わる（実測）。
- 再折り返し後の位置を `EM_LINESCROLL` で戻す: 同じ論理行に戻らない。
- 閲覧で `reface` してから流し直す: 流し込みが既定書式を捨てるので無駄で、位置がずれる。
- 設定画面の見出しを外して行を詰める・言語を別の面にする: 見出し無しの 6 行でも 5 行に入らず、面を増やすと入口が 2 つになる。一覧のスクロールはパレットで既に使っている形。
- 中間の翻訳ファイル（`.po` / JSON）: 正本が 2 つになる。
- 列の網羅を C-002 で守る: 添字で引く表には `switch` が無い。字句検査だけが守る。
- リテラルの数で列を数える: clang-format が隣接リテラルに割る。要素の数を数える。
- あふれる文言を `DT_END_ELLIPSIS` のまま許す: 意味が切れる。欄の中は収まるように書く。
- `folded` を `folio_command.c` に 3 つ目として書く: 第 2 の経路（ARC-001）。1 本化する。
- 大小無視を Ex の完全一致にも入れる: ADR 0016 決定 1 の決定本文を変える。
- face を ui の表に置く（Issue #76 の字面）: `markdown_rtf`（core）が同じ face を要る（ADR 0029 決定 11）。

## 検証

core: 版 2 → 3 の移行と往復・版 3 の解析（3 値・壊れた値・未知の版）・構造体を直に埋める refactor の前後で `settings_tests` の期待値が同一・`folio_language_count` の一致・`ui_font_face` の 3 値と記号の無さ・全 ID × 3 列が空でない（`UI_TEXT_EMPTY` 以外）・閉じた語彙の包含（HELP・`COMMAND_*`・`SETTINGS_GUIDANCE`）・全 ID × 3 列の置換子と `ui_text_unit_limit`・単位 A の同一性の期待表（ja 列・変えた 2 件を更新）・`ascii_fold` と `contains_span` の大小無視・`markdown_rtf` の fonttbl に face が入る・確保失敗。
application: `folio_state_set_language`（`UNREADABLE`・同じ値・先に作って書いてから差し替え・書けなければ不変・作れなければ不変・`synchronize` を通さない）・`folio_state_language` が設定を返す・起動時と `refresh_theme` とノートの切替で RTF の face が言語に従う・確保失敗（両経路）。
ゲート: CNF-011 の正例・反例（隣接リテラルに割れたエントリの正例・列の欠落と空の反例・`tests/conformance`・`prove-gates.py`）・`lineTables` の 2 行・`gate-proofs.md` の行。
Win32 部品 probe（`out/design/2026-09-23/language-ui-probe/`）: 3 言語 × 欄の中の ID の帯幅であふれ 0・face の差し替えで本文・選択・Undo・変更印が不変で既定書式の face が変わる・再折り返し後に最初に見える論理行が戻る（編集・閲覧）・`recolor_pane` の論理行の復元・EDIT の `WM_SETFONT`・face の存在確認と代替・設定画面 8 行が 560×360 と 150% で 5 行見えて選択行が常に見える・札が en で +8px でもパンくずの最小幅を割らない・`:set language=` の 3 語・名前入力面の説明が 3 言語で 2 行に収まる・失敗の箱の題と本文の言語。
実機の目視（hide）: 3 言語の見え方（字形の混在・豆腐・幅）・簡体字の訳の妥当性（母語話者の確認は別）・言語切替中の本文と行番号・設定画面のスクロール。統合チェックリストに足す。Waivers: none。

## 2026-09-23 の補正（実装と Win32 部品 probe の後・決定本文は書き換えない）

実装の probe は `out/design/2026-09-23/language-ui-probe/`、
結果のまとめは[確認記録](../quality/2026-09-23-language-checks.md)。

1. **新しい `ui_text` の ID は単位 3（core）のコミットに置いた。** 決定 7 の
   `UI_TEXT_SETTINGS_LANGUAGE` と 3 つの自称、`command_shortcuts[]` の
   `UI_TEXT_HELP_EX_SET_LANGUAGE` はコミットの順では単位 4 / 5 に書いたが、
   `ui_text.h` と `ui_text.c` は core のファイルで、CNF-009 が列挙と表の対を
   **同じコミットで**要求する。表を 3 列にするコミットに 5 つとも入れた。
2. **`folio_option` の 3 語と `options[]` は単位 5（ui）のコミットに置いた。**
   値を足すと `execute_set_command` の全値 switch が落ちるので、行き先
   （`apply_language`）が無い段階では仮の分岐を置くことになる。ADR 0031 の補正 2 と同じ理由で、
   値と UI を同じコミットに置いた。
3. **`markdown_rtf` の `build` は本文を nullable の `struct note_text *` で受ける。**
   決定 5 は `create` が 4 引数になることだけを書いているが、内部の `build` は
   `(out, palette, face, bytes, length)` で 5 引数になる。本文の有無を `build` の中で見て
   4 引数に収めた（`markdown_rtf_empty` は `nullptr` を渡す）。
4. **`note_pane_create` は face を受けない。** 決定 4 は「RichEdit の既定書式（`note_pane_create` と
   決定 6）」と書いているが、`note_pane_create` は既に 4 引数で飽和している。
   既定書式から `CFM_FACE` を外し（`note_pane.c` から face の直書きが消えた）、
   主窓が**作った直後に `note_pane_reface` を 1 回呼ぶ**形にした。face の正本は core の表だけになる。
5. **起動の最初の空文書だけは `FOLIO_LANGUAGE_JA` の face で作る。** `folio_state_create` は
   設定を読む前に `markdown_rtf_empty` で空の閲覧文書を作る（`state->settings` はまだ無い）。
   設定を読んだ直後の `adopt_theme_choice` が同じ経路で作り直すので、
   `settings.json` に `zh-Hans` がある起動でも**見えるのは最初から YaHei の文書**である（決定 5 のとおり）。
6. **face の実在を確かめる関数は ui の新しいファイル `ui_face` に置いた。** 決定 4 は「ui の 1 関数」
   としか書いていないが、引くのは主窓・ドロワー・名前入力面の 3 か所なので、
   CNF-002（実装ファイルの外部関数はファイル名の接頭辞）に合わせて `ui_face_for` の 1 本にした。
7. **決定 6 の「論理行の復元」が本当に効くのは再着色の側だった。** 実測では、
   face の差し替えで表示行数が 181 → 121 に変わっても **RichEdit 自身が先頭の内容を保ち**、
   最初に見える論理行は動かなかった（復元は空振り）。一方、**キャレットが画面の外にある再着色**では
   論理行が 16 → 1 へ飛び、同じ包みで 16 に戻った。包みは両方に付けたまま（face 側は安全網）で、
   ADR 0031 の 2026-09-23 の補正 2 はこの実測にもとづく。
8. **`failure_box_show` の呼び出しは 8 か所ではなく 13 か所だった**（ドロワー 5・主窓 8）。
   全部が `folio_state_language` を渡す。
9. **英訳を 1 件だけ短くした。** probe が `UI_TEXT_HELP_EX_SET_NUMBER` の en を 426px（箱 428px）と
   測ったので `(line numbers while editing)` → `(line numbers in edit)` にした。
   決定 3 の「欄の中は収まるように書く」の適用で、閉じた語彙は変えていない。

### 独立レビュー（設計リナ）で直した所

10. **S1: 既定書式の face はモードによらず当てる。** 決定 6 は「編集は `note_pane_reface`・
    閲覧は流し直し」と書き、補正 4 は「作った直後に `note_pane_reface` を 1 回呼ぶ」と書いたが、
    実装の `reface_pane` がモードで分岐していたため、**起動直後（閲覧）と閲覧中の言語切替では
    一度も当たらなかった**。`note_pane_create` から `CFM_FACE` を外してあるので、そのまま
    `i` で編集へ入った本文は RichEdit の既定 face（実測で `Segoe UI`）で描かれる（補正 4 の不履行）。
    `reface_pane` を「**常に** `note_pane_reface` → 閲覧ならそのあと `restream_pane`」へ直した。
    `SCF_DEFAULT` は既存の run を塗らないので閲覧の見た目は変わらない（`note_pane_recolor` が
    閲覧でも `SCF_DEFAULT` を無条件に当てているのと同じ理屈・ADR 0031 の補正 9）。
    probe に「作成 → reface → RTF → 閲覧のまま言語切替 → `i` で編集」の配線を並べた 4 項目を足した。
11. **訳を 14 件直した**（独立レビュー）。用語の揺れ・中国語の引用符・語の選び直し・
    英語の `shape` → `format` の 2 件。とくに**ノート内検索は Find / 查找、全ノート検索は
    Search / 搜索**で、`UI_TEXT_HELP_SEARCH_FIELD` だけがこの規則から外れていた。
    確認記録 §1 の用語表を正本とする。訳を変えた ID は帯幅の probe を流し直し、
    3 言語ともあふれ 0 のままであることを確かめた。
