# ADR 0029 — 設定・テーマ・言語（#38）は 4 つの単位に分け、port を束ねてから文言の正本を 1 表に集め、版は効果のあるキーごとに上げる

- 状態: 受理（設計リナ 2026-09-22。読み取り専用の調査 `out/design/2026-09-22/settings-survey/` に基づき、現行コードに照らした読み取り専用の批評（止める所見 8・直したい所見 11）を経て直した）
- 日付: 2026-09-22
- Issue: #38（親）。子は #73（前提 0）/ #74（単位 A）/ #75（単位 B）/ #76（単位 C）。親計画 #36 の 4 節
- 規則: ARC-001 / ARC-002 / ARC-003 / ARC-004 / ARC-005 / ARC-007 / ARC-009 / ARC-011、C-002 / C-003 / C-006 / C-007 / C-012、CNF-002 / CNF-006 / CNF-009、QLT-009 / QLT-010、GIT-001 / GIT-004
- 関連: ADR 0001（`planned` を `active` と書かない）、ADR 0005（テーマ「案2 堅」・OS 変更への追従は先送り）、ADR 0016（入力面・決定 2 の翻訳表の予告・決定 9 の設定アイコンの予約・2026-09-22 補正の「自分の欄の集合」）、ADR 0025（設定の版 1・決定 1「効果のあるキーだけ」・決定 2「キーを足すたびに版を上げる」・決定 3「解析と直列化は core」・決定 6「読めない設定のセッションは変えない」）、ADR 0027（文言の表引きと CNF-009）、ADR 0028（`folio_state_create` は 4 引数で飽和・決定 9 の位置は UI が添える）

## 文脈

Issue #38 は「設定画面からテーマ（dark / light / system）と言語（日本語 / English / 简体中文）を選び、Ubuntu 風の配色を当て、
版付きの `data/settings.json` を唯一の永続化先にし、表示文字列は安定した message ID から選ぶ」ことを求める。

現状（調査・実測）:

- 表示文言は約 123 文言が 3 か所に分かれている。application の `failure_lines[]`（40・表引き・CNF-009 が網羅を守る）、
  core の `folio_command.c` の `label`（15・表引き）、ui と app に散在する 68（switch 直書き 6・関数引数の直書き 14・`name_prompt.c` 17・
  `command_shortcuts[]` 18・`main.c` 7 ほか）。ARC-004 の表に文言の所有者の行は無く、3 つ目が第 2 の経路になっている。
  非 ASCII を含む文字列リテラルは `src/` に 137 あり、**5 ファイルだけ**（`folio_state.c` 49 / `folio_window.c` 43 / `name_prompt.c` 20 / `folio_command.c` 15 / `main.c` 10）。
  `drawer_window.c` の折畳印 `L"\x2212"` はバイトが ASCII のエスケープで、字面の検査には掛からない。
- OOM の退避が 2 か所ある: `main.c` の `report_utf8()` と `name_prompt.c` の失敗表示は、UTF-8 → UTF-16 の変換（`utf16_text_create`・確保する）に
  失敗したとき `L"…"` の直書きを出す。`main.c` の `adapter_failure()` は `enum persistence_adapter_outcome` の 5 文言を `const wchar_t *` で返す。
  `name_prompt.c` は `" → "` の区切りに `sizeof` を取っている。
- 状態行は断片の連結で作る。`search_status` は向きの札（3 つの翻訳文）→ k → `" / "` → n → `" 件"`、`replace_status` はノート名 → `" / "` → k → `" 件"`、
  `failure_status` は失敗の 1 行に `REPLACE_BAD_PATTERN` かつ offset > 0 のときだけ `" 位置 "` → offset。英語 "3 of 12" と中文「第 3 / 共 12 项」は語順が違う。
- ヘルプ 18 行は「区画名␣␣鍵₁ 説明₁ / 鍵₂ 説明₂ / …」の形で、1 行に複数の（鍵・説明）の対と翻訳対象の区画名を持ち、鍵の欄に `↑↓←→` の非 ASCII があり、
  `:%s/パターン/置換/[g]` は構文の中に日本語がある。鍵の無い行も 1 行ある。
- テーマは起動時に `appearance_port.read_theme` を 1 回読むだけで、`folio_state` は `appearance` を保持しない（persistence と regex は**値で写して**持つ）。
  色の正本は `rtf_palette.c` と `folio_palette.c` で参照は 35 か所 / 24 関数。`WM_SETTINGCHANGE` は 0 件。`note_pane` は生成時に `EM_SETBKGNDCOLOR` と
  `EM_SETCHARFORMAT(SCF_DEFAULT)` を 1 回当てるだけで再着色の API が無い。
- **実測**（`theme_probe`。地の色と書式を**同時に**当てた）: 既存の本文の文字色を変える `SCF_ALL` は **Undo を 1 つ積み `EM_GETMODIFY` を立てる**。
  `EM_SETBKGNDCOLOR` 単独の影響は分離して測っていない。閲覧の RTF は色が文書に埋まるので流し直しが要るが、閲覧には失うものが無い。
- 設定は `folio_settings`（版 1・`number`）。`struct folio_settings` の定義は `folio_settings.c` に閉じ（C-003 / C-007）、`parse_document` は
  `{"version":…,"number":…}` をキーの順序込みで逐次に読む（26 行・分岐多数）。`folio_state_set_number` は先に書いてから採用し、`SETTINGS_UNREADABLE` の
  セッションでは変えない（ADR 0025 決定 6）。
- `folio_state_create` は 4 引数で C-012 に飽和し、呼び出しは 23 か所（`main.c` 1・`state_tests.c` 19・`replace_state_tests.c` 2・`allocation_tests.c` 1）。
- パレットの骨格のうち鍵（Esc / Enter / Tab / ↑↓ / F1）・IME の抑止（`command_composing`）・フォーカスが外れたときの消灯（`WM_KILLFOCUS` →
  `folio_message_command_focus_lost`）は **EDIT `command_input` のサブクラス**にあり、`command_layer_procedure` は描画とマウスしか扱わない。
  レイヤーは `WS_CHILD` の自作クラスなので `SetFocus` は受けられる。`eng/architecture.json` の `platformLibraries.ui_win32` に imm32 は無い。
- `:set` の語は `options[]`（完全一致の表・3 値の `enum folio_option`）で、`catalog[].names` の別名照合とは別の表。
- **実測**（`font_probe`）: Yu Gothic UI は簡体字のサンプル 8 字中 3 字を持たず、自前描画・EDIT・RichEdit の 3 経路すべてでフォントリンクが効いて豆腐は出ないが、
  1 行に日本語と簡体字の字形が混ざる。RichEdit は face に関わらず Microsoft YaHei へ束ねる。`markdown_rtf.c` の fonttbl は core が持つ。
- `docs/PROJECT_LAYOUT.md` 第 3 節の「保存形式の版と移行（ARC-009）も DTO も adapters に閉じる」は、#39 が ADR 0025 決定 3 で設定の解析を core に置いた時点で
  既に食い違っている。CNF-009 の `lineTables` は `tests/unit/state_tests.c` も対象に含めている。
- 規模: #41 は 60 ファイル / +3928 行。#38 を 1 PR にすると概算 55〜70 ファイル・約 369 文字列（123 × 3）・5000 行超。

## 決定

1. **4 つの単位に分け、Issue #38 は最後の単位で閉じる。順序は固定。**
   - **前提 0（#73）**: `struct folio_ports` の束ね。振る舞い・文言・スキーマの変化 0。`docs/PROJECT_LAYOUT.md` 第 3 節の食い違いの字面もここで直す。
   - **単位 A（#74）**: 文言の正本を core の `ui_text` 表 1 か所へ集める（日本語 1 列）。表示は 1 字も変えない（例外は決定 6 の OOM の退避だけ）。
   - **単位 B（#75）**: テーマ（`system` / `light` / `dark`・Ubuntu 風配色）と設定画面と `settings.json` **版 2（`theme` だけ）**。#69（最小寸法のパレット）は B の前に別単位で直す。
   - **単位 C（#76）**: 言語（`ja` / `en` / `zh-Hans`）と **版 3（`language`）**、ヘルプ行の欄分け、言語ごとのフォント。
   依存は一方向: 0 → A → B → C。A を B の前に置くのは、設定画面自身の文言を B が直書きすると第 2 の経路を一時的に作るから。B と C は独立だが、版の刻みのため順序は固定する。
2. **版は効果のあるキーごとに上げる（ADR 0025 決定 1・2 の帰結）。** 版 2 = `{"version":2,"number":<bool>,"theme":"system"|"light"|"dark"}`、
   版 3 = 版 2 + `"language":"ja"|"en"|"zh-Hans"`。同じ版に効かないキーを入れない（ADR 0001）。
   **移行は `folio_settings.c` の中の static な純関数**で行う（`struct folio_settings` の定義がそこに閉じているので別ファイルには書けない・C-003 / C-007）。
   解析は「版を読む → 版ごとの解析関数（`parse_version_1` / `parse_version_2` …。それぞれ C-012 の 60 行・複雑度 10 に収める）→ 足したキーは既定値で埋める」
   の形にし、書くのは常に現在の版。既定は `theme=system`・`language=ja`。単体は「版 1 の文書を読むと版 2 の既定値が入り、書き戻すと版 2 になる」を固定する。
   ADR 0025 決定 3（解析と直列化は core）を維持し、`docs/PROJECT_LAYOUT.md` 第 3 節を「台帳・md・改名記録のファイル形式と DTO はアダプタ、設定の解析と版は ADR 0025 に従い core」と前提 0 で直す。
3. **port は `struct folio_ports` に束ねる（前提 0）。** `src/application/folio_ports.h`（専用ファイル・CNF-002）に完全型
   `struct folio_ports { const struct persistence_port *_Nonnull persistence; const struct appearance_port *_Nonnull appearance; const struct regex_port *_Nonnull regex; }`
   を置き、`folio_state_create(const struct folio_ports *_Nonnull, struct folio_state *_Nullable *_Nonnull out)` の 2 引数にする。
   **state は今までどおり persistence と regex を値で写して持ち、`folio_ports` そのものは呼び出しの間しか参照しない。** `appearance` はいまは起動時に 1 回読むだけで
   保持しないが、**単位 B が OS テーマの再読（決定 7）のために `appearance_port` も値で写して保持する**。テストは `test_ports(...)` の補助 1 本で 23 か所を機械的に置き換える。
4. **文言の正本は core の `ui_text`（単位 A）。** ID は core の閉じた列挙 `enum ui_text`（専用ヘッダ `ui_text.h`）。文言は `src/core/ui_text.c` の
   指示付き初期化子の表 `[UI_TEXT_X] = "…"` から `ui_text_line(id)` で引く（CNF-002 の接頭辞 `ui_text_`）。長さは `strlen` で取り、`sizeof` に頼らない。
   application の `failure_lines[]` は **outcome → ID の表**になり（CNF-009 の字句検査は右辺が列挙値でも通る）、本文は `ui_text` の表 1 か所だけが持つ。
   core の `folio_command.c` の `label` も ID に置き換える（ADR 0016 決定 2 の予告どおり）。`main.c` の `adapter_failure()` は `persistence_adapter_outcome` → ID の表にし、
   `lineTables` に行を足して網羅を守る（`src/app` は CNF-002 の対象外だが `lineTables` は任意のファイルを指せる）。
   折畳印 `\x2212` や `◀` `▶` のような**描く字形も ID にする**（翻訳しないが正本は 1 か所）。単位 C では表を `[UI_TEXT_X] = {"…","…","…"}` の 2 次元配列にし
   （新しい型を作らない・CNF-002）、翻訳しない項は 3 列に同じ文字列を置く。
   **単位 A の受け入れは「変更前後で全文言が同一」**で、変更前の文言を機械的に抽出した単体の表で 1 字ずつ固定する（#65 と同じ手順）。
5. **句の中の語と数は置換子で埋め、句どうしの並置は許す（単位 A で形を作る）。** 文言は `{k}` `{n}` `{name}` `{offset}` の置換子を持てて、
   core の純関数 `ui_text_format(id, const struct ui_text_arguments *_Nonnull, char *_Nonnull out, size_t capacity)` が埋める
   （`ui_text_arguments` は専用ヘッダの完全型・C-012）。`search_status` は「向きの札（ID）」と「結果の句（ID・`{k}` `{n}`）」の 2 句の並置、
   `replace_status` は 1 句（`{name}` `{k}`）、`failure_status` は「失敗の 1 行」と「位置の句（ID・`{offset}`）」の並置で、位置の句を添える条件
   （`REPLACE_BAD_PATTERN` かつ offset > 0）は ADR 0028 決定 9 のとおり UI が持つ。**句の中で語や数を連結すること**（`" / "` `" 件"` `" 位置 "`）は無くす。
   **ヘルプ 18 行は単位 A では 1 行 1 ID のまま移す**（字面同一を保つため）。「区画名・（鍵・説明）の列・固定の区切り」への欄分けと、
   Ex の別名・`:set` の語・`:%s` の構文・置換文字列の記法・鍵の名前を訳文に埋めない形は**単位 C の ADR で決める**（鍵の欄に `↑↓←→` の非 ASCII があるので、
   鍵の欄も `ui_text` の翻訳しない項として持つ）。ファイル名・台帳のキー・`settings.json` のキー・利用者の md とノート名・カテゴリ名は翻訳しない。
6. **文言の網羅と単一性は機械に守らせる（単位 A でゲートを足す。QLT-010 の ADR 相当の判断はこの ADR）。**
   - `ui_text` の表の網羅は **CNF-009 の `lineTables` に行を足す**だけで守る（`ui_text.h` → `ui_text.c`。新しい規則 ID は立てない）。
     単位 C で「各エントリに言語の数だけ空でない文字列がある」を新しい規則（**文言の列の網羅**。規則 ID は単位 C が `docs/QUALITY_GATES.md` に定義するときに採番する）として足す。
   - **文言の置き場所の検査**（規則 ID は単位 A が `docs/QUALITY_GATES.md` に定義するときに採番する）: `src/` の C ソースで**非 ASCII を含む文字列リテラル、および `\x` `\u` `\U` のエスケープを含む文字列リテラル**を置けるのは `src/core/ui_text.c` だけ
     （`tests/` と `out/` は対象外。`L"…"` も対象。固有名詞 `NeNe Folio` と `NENE FOLIO`、フォント名は ASCII なので影響しない）。
     コメントは対象外。結合や `#define` による回避は字句検査では見えないので、ARC-004 の第 2 の経路を**見つけやすくする**検査であって、絶対の封じではない。
   - **OOM の退避を無くす**: core の `utf16_text` に**確保しない固定長の変換** `utf16_text_fill(const char *utf8, char16_t *out, size_t capacity)`（閉じた結果値・収まらなければ断る）
     を足し、`report_utf8()` と `name_prompt.c` の失敗表示はそれで表から引く。確保に失敗したときの `L"記憶域が足りません。"` 系の直書き 2 か所は**経路ごと消える**
     （単位 A で唯一の振る舞いの変化で、確保失敗のときだけ。分岐網羅の測定ビルドで同じ経路を通す必要が無くなる）。
   - 新しい規則は `eng/prove-gates.py` に反例を載せ、`docs/quality/gate-proofs.md` に行を足し（CNF-006 が「active なのに証明行が無い」を拒む）、
     `docs/QUALITY_GATES.md` に **active** で記載する（実装が済んだコミットでだけ active にする・ADR 0001）。
   - `docs/ARCHITECTURE_CONSTITUTION.md` の ARC-004 の表に「利用者に見える文言 → core `ui_text`」の行を足す。
7. **テーマの型は 2 つに分ける（単位 B）。** 利用者の選択は core の新しい閉じた列挙 `folio_theme_choice`（`SYSTEM` / `LIGHT` / `DARK`）、
   解決した結果は既存の 2 値 `folio_theme` のまま。解決は core の純関数 `folio_theme_resolve(choice, os_theme)`。`rtf_palette_for` / `folio_palette_for` は
   `folio_theme` を受け続け、`SYSTEM` を知らない。OS のテーマを読むのは adapters（`appearance_port.read_theme`）だけで、`choice == SYSTEM` のときだけ
   `WM_SETTINGCHANGE`（`lParam` が `ImmersiveColorSet`）で読み直す（ADR 0005 が先送りした「次」はこの単位）。契機は ui が受け、意図は application の 1 本
   （`folio_state_refresh_theme`）へ渡る。**`SETTINGS_UNREADABLE` のセッション**では設定画面は開くが、Enter と `:set theme=` は `folio_state_set_number` と同じ値で断り、
   結果の 1 行を設定画面に出す（ADR 0025 決定 6）。
8. **テーマの切り替えで本文・キャレット・Undo・変更印・モードを失わない（単位 B。方法は probe で決める）。** `SCF_ALL` はそのまま当てない。
   候補は (a) TOM `ITextDocument::Undo(tomSuspend)` → `SCF_ALL` → `Undo(tomResume)` と変更印の退避・復元、(b) 編集中は地の色（`EM_SETBKGNDCOLOR`）と既定書式だけ変え、
   本文の文字色は次の流し込みで揃える。**probe で (a) と、(b) の `EM_SETBKGNDCOLOR` 単独の影響を分離して測り**、Undo の段数と `EM_GETMODIFY` が不変な方を ADR 0031 で固定する。
   閲覧の RTF は `markdown_rtf` を新しい palette で作り直して流し直す。`note_pane` に再着色の API を 1 本足す。窓の縁（`DwmSetWindowAttribute`）・`command_brush`・
   3 か所の palette の値コピーは差し替える。描画中に作って捨てるブラシは自動で追随する。
9. **設定画面の器は `command_layer` の自前描画で、レイヤー自身がフォーカスを持つ（単位 B）。** `command_surface_mode` に `SETTINGS` を足し（全 switch が落ちる）、
   パレットの行の描画・追従・クリック・ホイールを流用する。行は「設定名: 値の選択肢」で、開いたとき現在値にカーソルがある。Enter で採用（先に書いてから採用）し、
   欄は開いたまま結果の 1 行を出す。Esc で閉じて元のフォーカスへ戻す（ADR 0016 の戻り先の規則は変えない）。
   **文字入力が無い面では EDIT を使わず、`command_layer` がフォーカスを取る。** そのために (a) レイヤーの手続きに `WM_KEYDOWN` を足して既存の `command_key_down` を**共有**する
   （第 2 の鍵の経路を書かない）、(b) `WM_KILLFOCUS` で EDIT と同じ `folio_message_command_focus_lost` を post する、(c) `command_owns` / `command_focus_target` /
   `arrange_command_input` に `SETTINGS` の分岐を足す、(d) IME を持ち込まないためレイヤーに `ImmAssociateContext(hwnd, NULL)` を当て、
   `eng/architecture.json` の `platformLibraries.ui_win32` に **imm32** を足して `nenefolio_system_link` で結ぶ（ARC-002 の正典経路）。
   `command_owns` に入る結果、Ctrl+S などのアクセラレータが設定画面でも届く。これは #67（入力面の Ctrl+S は保存して欄は開いたまま）と同じ振る舞いで、意図どおり。
10. **入口は 3 つで同じ意図へ（単位 B）。** (a) 右ペインの頭の**閉じるアイコンの左隣**の設定アイコン（ADR 0016 決定 9 の並び。`close_rect` ← 設定 ← `chip_rect` の
    連鎖を 1 か所で決め、`hit_test` に `HTCLIENT` の分岐を足す）、(b) 「操作」メニューとパレットの「設定」（`FOLIO_COMMAND_SETTINGS`・listed 真）、
    (c) Ex `:set theme=system|light|dark`。**`options[]` に `theme=system` 等の語をそのまま足す**（完全一致の表なので parser は広げない。
    `enum folio_option` が増えて `:set` を受ける switch が全部落ちるのは意図どおり・C-002）。単位 C は同じ形で `:set language=ja|en|zh-Hans` と設定画面の 2 段目を足す。
11. **簡体字の字形と言語ごとのフォント（単位 C）**: 言語ごとの UI フォントの face は **core の表 1 か所**（`markdown_rtf` の fonttbl が core にあるため。core → ui の依存は作らない・ARC-002）
    に置き、ui と `markdown_rtf` と RichEdit の既定書式が同じ表から引く（`ja` → Yu Gothic UI、`zh-Hans` → Microsoft YaHei UI、`en` → 単位 C の ADR で決める）。
    #42（同梱フォント）が来たらこの表だけを差し替える。フォントリンクで表示自体は成立する（実測）ので #42 は前提にしない。
12. **単位ごとに ADR を持つ。** 前提 0 はこの ADR で足りる。単位 A は ADR 0030（`ui_text` の形・置換子・`utf16_text_fill`・文言の置き場所の検査の定義と例外の無いこと）、
    単位 B は ADR 0031（配色の値・再着色の方法・設定画面の寸法と鍵・`WM_SETTINGCHANGE`・imm32）、単位 C は ADR 0032（翻訳の列・ヘルプの欄分け・文言の列の網羅の検査・フォントの表・帯幅の実測）。
    各 ADR は probe → 草案 → 批評 → 受理の順。

## 却下した選択肢

- #38 を 1 PR にする: 概算 55〜70 ファイル・5000 行超で、レビューが機能しない。
- 版 2 に `theme` と `language` を同時に入れ、言語は後で効かせる: 効果の無いキーを受理する（ADR 0025 決定 1・ADR 0001 の違反）。
- 単位 B を A の前に置く: 設定画面の文言を日本語で直書きしてから C で置き換えることになり、第 2 の経路を一時的に作る。
- 移行を新規 `settings_migration.{c,h}` に置く: `struct folio_settings` の定義が `folio_settings.c` に閉じている（C-003 / C-007）ので、別ファイルからメンバーに触れない。
- 移行をアダプタに置く: ADR 0025 決定 3 と矛盾する。
- OOM の退避文言を置き場所の検査の名指しの例外にする、または退避を ASCII にする: 例外は第 2 の経路を残し、ASCII 化は文言を変える。確保しない変換で経路ごと消す方が閉じる。
- 文言の表を application に置く: core の `folio_command.c` の label が既に core にあり、置換子の組み立ては純関数なので core が自然。文言は純粋なデータで、core の純粋性（ARC-003）を壊さない。
- `failure_lines[]` を廃して `ui_text` に直接 outcome を引かせる: outcome → ID の表を残す方が CNF-009 の網羅がそのまま効き、同じ 1 行を複数の outcome が使える。
- 文字列キー（`"search.count"`）の message ID: 打ち間違いがコンパイルで落ちない。閉じた列挙なら `-Wswitch` と `lineTables` で守れる。
- 単位 A でヘルプ行を（鍵・説明）の欄に分ける: 1 行に複数の対と区画名があり、字面同一を保てない。欄分けは翻訳と同じ単位 C で行う。
- 断片連結を残して言語ごとに連結順を分岐する: 語順の規則を ui に埋め、言語を足すたびに分岐が増える。
- `folio_theme` に `SYSTEM` を足す: `rtf_palette_for` / `folio_palette_for` が解決できない値を受けることになる。
- 設定画面をモーダルダイアログ（`name_prompt` 流）や `TrackPopupMenuEx` で作る: OS 既定色で浮く・フォーカスの戻り先を持たない。
- 設定画面でも EDIT を隠さずに置いて鍵を拾う: 文字入力の無い面に IME と文字入力の経路を残す。レイヤーが鍵を受け、`command_key_down` を共有する方が 1 経路。
- 設定アイコンを操作行の 6 個目に置く: 改修は 3 か所と少ないが、ADR 0016 決定 9 の「頭の右端」の並びと食い違う。
- `note_pane` を作り直してテーマを当てる: 本文と Undo を失う。
- `folio_command_parse_option` を `語=値` の文法に広げる: `options[]` は完全一致の表なので語を足せば足り、parser の変更は不要。
- フォントの表を ui に置く: `markdown_rtf`（core）が同じ face を要るので core → ui の依存になる（ARC-002）。

## 検証

前提 0: 全テストが同じ期待値で通る・3 引数以上の `folio_state_create` が残らない・分岐網羅が下がらない。
単位 A: 変更前後の全文言の同一性（機械抽出の表で 1 字ずつ）・`lineTables` の行と置き場所の検査の反例（`prove-gates.py`・`gate-proofs.md`）・状態行の字面が同じ（Win32 部品 probe）・
`utf16_text_fill` の境界（収まらない・空・サロゲート）。
単位 B: 版 1 → 2 の移行と往復・壊れた値・未知の版・書けない場合・`SETTINGS_UNREADABLE` のセッションで断る・編集中の切り替えで `EM_GETMODIFY` と Undo の段数が不変（probe）・
閲覧の流し直し・`WM_SETTINGCHANGE` の追従・IME を持ち込まない（レイヤーに IME の窓が出ない）・560×360 と高 DPI で設定画面が収まる（#69 を先に直す）・確保失敗。
単位 C: 全 ID × 3 言語の網羅（文言の列の網羅の検査）・版 2 → 3・切り替えで本文とフォーカスが不変・英語と中国語の状態行とヘルプの帯幅（probe）。
実機の目視（hide）: 配色・設定画面・言語ごとの見え方は #39〜#41 の統合チェックリストに続けて 1 表にする。Waivers: none。
