# ADR 0031 — テーマは 3 値の選択を `settings.json` の版 2 に置き、OS の変更に主窓が追従し、編集中の再着色は TOM の Undo 停止と変更印の退避で本文・Undo・印を守る

- 状態: 受理（設計リナ 2026-09-23。ADR 0029 の単位 B。実測 `out/design/2026-09-22/theme-probe/`（再着色 7 案・RTF の流し直し・`WM_SETTINGCHANGE`・DWM・レイヤーのフォーカスと IME・配色のコントラスト）に基づき、現行コード（main `f881f16`・#74 統合後）に照らした読み取り専用の批評（止める所見 7・直したい所見 10）を経て直した）
- 日付: 2026-09-23
- Issue: #75（親 #38・ADR 0029 決定 2・3・7〜10）
- 規則: ARC-001 / ARC-002 / ARC-003 / ARC-004 / ARC-005 / ARC-007 / ARC-010 / ARC-011、C-002 / C-003 / C-005 / C-012 / C-018、CNF-002 / CNF-009 / CNF-010、QLT-009 / QLT-010
- 関連: ADR 0005（テーマ「案2 堅」・OS 変更への追従は先送り → この単位）、ADR 0011 決定 4（パンくずの最小幅）、ADR 0016（入力面・決定 9 の設定アイコン・補正の「自分の欄の集合」・補正 2 の Ctrl+S）、ADR 0018（パレットの骨格。#69 の補正で面ごとの行数を引数に取る）、ADR 0023（閲覧の検索の選択）、ADR 0025（設定の版 1・決定 1〜6）、ADR 0026 決定 3（`EN_CHANGE` で番号の表に印を付ける）、ADR 0029、ADR 0030（`ui_text_line(id, language)`・`folio_language`・CNF-010）

## 文脈

実測（`theme-probe`・RICHEDIT50W・clang-cl 19.1.5）:

- 編集中の再着色。1 打鍵して `EM_CANUNDO=1`・変更印 TRUE・選択 `[7,9]` の状態（と保存直後を模した変更印 FALSE の状態）で 7 案を別々の窓に当てた。
  Undo の段数（打つ前へ戻るまでの `EM_UNDO` の回数・基準 1）・変更印・`EN_CHANGE`・本文の色:
  (a1) `EM_SETBKGNDCOLOR` だけ → 1・不変・0・**本文の色は変わらない**。(a2) `SCF_DEFAULT` だけ → **2**・TRUE・1・変わる（ADR 0029 の文脈節が「既定書式は以後の文字だけ」と書いたのは誤り。補正で訂正する）。
  (a3) `SCF_ALL` → 2・TRUE・1。(a4) TOM `Undo(tomSuspend)` → `SCF_ALL` → `Undo(tomResume)` → **1**・**TRUE**・1。**(a5) (a4) ＋ `EM_GETMODIFY` の退避と `EM_SETMODIFY` の復元 → 1・不変・1・変わる。**
  (a6) TOM `ITextRange::SetForeColor`（suspend 内）＋退避復元 → (a5) と同じ。(a7) `ENM_CHANGE` を外して `SCF_ALL` → 2・TRUE・0。本文・選択・`EN_UPDATE=0` は全案で不変。
  **Undo を積ませない唯一の手は `tomSuspend`、変更印を守るのは退避・復元だけ、`EN_CHANGE` は `tomSuspend` の中でも 1 回出る。`SCF_ALL` と `SCF_DEFAULT` を 1 つの suspend の中で続けて当てる形は測っていない**（実装の probe で測る）。
- 閲覧の RTF を `EM_STREAMIN` で流し直す時間は 9KB で 0.5ms、100KB で 3ms、**1MB で 30ms**。**スクロール位置（`EM_GETFIRSTVISIBLELINE` / `EM_GETSCROLLPOS`）は保たれ、選択は `[0,0]` に消える**（`EM_EXSETSEL` だけを送った場合。`EM_SCROLLCARET` を重ねれば一致の位置へ飛ぶ）。
- `WM_SETTINGCHANGE`: レジストリを書くだけでは何も来ない（通知は切り替えた側の Settings アプリが broadcast する）。`HWND_BROADCAST` の `WM_SETTINGCHANGE`（`lParam` = `ImmersiveColorSet`）は**トップレベル窓にだけ 1 件**届き、`WS_CHILD` には来ない。
  `WM_THEMECHANGED` / `WM_SYSCOLORCHANGE` は来ない。Settings アプリで実際に切り替えたときの件数・遅れは測っていない。
- `DwmSetWindowAttribute` の 6 属性は起動中に呼び直せて全部 `S_OK`。縁の色は読み戻せない。画素での裏取りはできていない（調査環境の窓は画面取得に写らない）。
- `WS_CHILD` の自作クラスに `SetFocus` でき、別の子へ移ると `WM_KILLFOCUS` が 1 件来る。`WM_KEYDOWN` は ↑↓ Enter Esc PgUp PgDn Tab 英字の全部が手続きに届き、`TranslateMessage` は Enter / Esc / Tab / 英字にだけ `WM_CHAR` を作る（キューへ積んだ確認。OS の配達は未測定）。
  自作クラスにも既定で IME 文脈が付き、`ImmAssociateContext(hwnd, NULL)` で窓ごとに外せる。組成が始まらないことは未測定。
- 配色: Ubuntu の orange `#E95420` は白と **3.65:1**。WCAG 4.5:1 を満たす候補 2 組（ダーク・ライト）を作った（`contrast.py`。本文と地 15.06 / 17.78、選択 9.43 / 15.11、番号帯 6.33 / 6.10）。
  **3 対のうち「本文と地」は core の `rtf_palette.text` と ui の `folio_palette.pane` をまたぎ、「選択」と「番号帯」は ui の色だけで、core 単体では計算できない。** 相対輝度の計算には `pow` が要り、`eng/symbol-allowlist.json` の core に無い（`folio_palette_ink` が ui で使えるのは ui が検査対象外だから）。
  `⚙` `●` のような字形は CNF-010 により `ui_text.c` 以外に置けない。隣の閉じるアイコンは字形でなく GDI の線（`draw_close`）。

現行コード（main `f881f16`）:

- テーマは起動時に `appearance_port.read_theme` を 1 回読む。`folio_ports.h` の注記は「3 つのポートを複製して持つ」と言うが、`folio_state_create` は persistence と regex しか写していない。`main.c` は 3 つの adapter を `run()` → `folio_state_destroy` の後まで生かす。
- 閲覧の色は `markdown_rtf` に埋まり、その文書の所有者は application（`state->pane`。無題は `markdown_rtf_empty`）。UI は `folio_state_pane_rtf` を `render_pane` で流し込むだけで、`streaming` の印は `note_pane_render` の中で立つ。
- `note_pane_create` は `EM_SETBKGNDCOLOR` と `SCF_DEFAULT` を 1 回当て、`note_pane_edit` の平文の流し込みはその既定書式で描かれる。`note_pane_select` は `EM_EXSETSEL` の後に **`EM_SCROLLCARET`** を送る。`note_pane.c` は既に `<tom.h>` を include し、自前の `text_document_id` で `EM_GETOLEINTERFACE` → `QueryInterface` → `ITextDocument` を取っている（`prepare_selection`）。
  本文の `EN_CHANGE` を受けるのは `pane_notification` だけで、番号の表に印を付けて帯を描き直すだけ。`EM_GETMODIFY` を読む production コードは無い。
- 色の値は `rtf_palette.c`（6 役）と `folio_palette.c`（15 役）、`folio_window` / `drawer_window` / `folio_state` が palette を値で写して持ち、`command_brush` は `CreateSolidBrush` の 1 本、`decorate()` は生成時に 1 回。
- `command_surface` を見る箇所は 38 で、switch は 6 本、残りは `==` / `!=` の比較（コンパイルは落ちない）。#69 で `command_surface_rows` / `command_help_rows` / `command_box_height(rows, help_rows)` が面ごとの行数を引数に取り（`CLOSED` は `PALETTE` と同じ枝）、`command_box_height` は EDIT の高さと案内 2 行を常に数える。
  `open_command_surface` は `command_selection = 0` に固定し、`show_search_surface` が現在値を上書きする形の前例。Ctrl+S / Ctrl+N / Ctrl+P は `WM_CHAR` 経路（`command_character`）で、アクセラレータ表は Ctrl+Shift+S / F2 / Ctrl+F / Ctrl+Shift+F / F3 / Shift+F3 の 6 本。
- `:set` の語は `options[]`（完全一致・`enum folio_option` 3 値）で、`wanted_number` が `bool` を返す switch、`execute_set_command` がそれを `apply_number` に渡す。`folio_command.c` の `catalog[]` は `lineTables` に入っていない。
- `folio_settings` は版 1（`number`）で `parse_document`（29 行・逐次）がキー順込みで読み、公開関数は 6 本。ADR 0025 決定 5: 設定の意図は `synchronize()`（改名の再開）を通さない。決定 6: 読めない設定のセッション（`SETTINGS_UNREADABLE`）は変えず上書きもしない。
- `close_rect` を配置に使うのは `chip_rect` の 1 行だけで、`breadcrumb_cells` は `chip_rect(VIEW).left` から引く。560 幅ではパンくずの帯が 120px 弱で、ADR 0011 決定 4 の最小 48px を `breadcrumb_room` が守る。
- `eng/architecture.json` の `platformLibraries.ui_win32` に imm32 は無い。`nenefolio_system_link` は無いライブラリで configure を落とす。

## 決定

1. **設定は版 2**: `{"version":2,"number":<bool>,"theme":"system"|"light"|"dark"}`（キーの順序込み・既定 `system`）。`folio_settings.c` の解析は「版を読む」「`parse_version_1`」「`parse_version_2`」の 3 本の static（各 60 行以内）に割り、
   版 1 には `theme` の既定値を埋める。書くのは常に版 2（`folio_settings_write` が `settings_version = 2` で `theme` を 1 つ足す）。未知の版・壊れた値（`"theme":"blue"`・型違い・重複・順序違い）は `MALFORMED` / `UNSUPPORTED_VERSION` で
   ADR 0025 決定 5・6 のとおり。公開関数は `folio_settings_theme(settings)` と `folio_settings_with_theme(settings, choice, out)`（写してから 1 つ変える）の 2 本を足して 8 本。
2. **型は 2 つ**: core の `enum folio_theme_choice`（`folio_theme_choice.h`・`SYSTEM` / `LIGHT` / `DARK`）と既存の `enum folio_theme`（`LIGHT` / `DARK`）。解決は core の純関数
   `[[nodiscard]] enum folio_theme folio_theme_resolve(enum folio_theme_choice, enum folio_theme os_theme)` を **新規 `src/core/folio_theme.c`（型定義 0・関数 1）** に置く（CNF-002 の接頭辞 `folio_theme_`。`folio_theme_choice.c` には置けない）。
   `rtf_palette_for` / `folio_palette_for` は `folio_theme` のまま。
3. **application は選択と OS の値を別に持ち、解決した値を出す。** `folio_state_create` は `appearance_port` も**値で写して保持**する（`folio_ports.h` の注記に実装を合わせる。adapter の寿命は `main.c` が `folio_state_destroy` の後まで保証する）。
   `theme_choice`（設定から）と `os_theme`（port から）を持ち、`folio_state_theme()` は `folio_theme_resolve(theme_choice, os_theme)`。`folio_state_theme_choice()` の問い合わせ 1 本。意図は 2 本で、**どちらも `synchronize()` を通さない**（ADR 0025 決定 5）:
   - `[[nodiscard]] enum folio_state_outcome folio_state_set_theme(state, enum folio_theme_choice)`: `SETTINGS_UNREADABLE` のセッションは `folio_state_set_number` と同じ値で断る。同じ選択なら書かない。
     **順序は「新しい設定（`folio_settings_with_theme`）と新しい閲覧文書（現在の文書と同じ作り方: 本文があれば `markdown_rtf_create(body, 新しい palette)`、無題は `markdown_rtf_empty`）を両方先に作る → `write_settings` → 成功したら設定・`rtf_palette` の写し・`state->pane` を差し替える」**。
     作り直しの確保に失敗したら `OUT_OF_MEMORY` で**ファイルも状態も変えない**。書き込みの失敗は `SETTINGS_STORE_FAILED` で状態を変えない。編集モードでも `state->pane` は作り直す（閲覧へ戻ったとき古い色にならない）。
   - `[[nodiscard]] enum folio_state_outcome folio_state_refresh_theme(state)`: port の `read_theme` を読み直して `os_theme` を更新し、解決値が変わったときだけ閲覧文書を作り直して差し替える（確保失敗は `OUT_OF_MEMORY` で不変）。書き込みは無い。
     解決値が変わったかは UI が `folio_state_theme()` を前後で比べる。
4. **OS の変更への追従（ADR 0005 の「次」）**: 主窓の `window_procedure` に `WM_SETTINGCHANGE` の 1 case を足し、`lParam` が `L"ImmersiveColorSet"` のときだけ `folio_state_refresh_theme` を呼び、解決値が変わっていれば決定 6 の再着色を行う。
   `command_layer` には届かないので主窓 1 か所。`WM_THEMECHANGED` / `WM_SYSCOLORCHANGE` は見ない。レジストリの読みは adapters の `appearance_adapter`（開きっぱなしの鍵を `RegGetValueW` で読み直す）だけ（ARC-007）。
5. **配色は Ubuntu 風の 2 組で、値は `rtf_palette.c` と `folio_palette.c` の 2 か所だけ**（ARC-004 の現状の正本のまま）。初期値は実測の候補 1（ダーク: 地 `#2C001E` 系の aubergine・選択 `#772953`・札 `#E95420` に地の色の字）と
   候補 2（ライト: 地 `#F7F7F7` / `#FFFFFF`・文字 `#241318`・選択 `#F0E4EC`・札 `#772953` に白字）。**orange の面に白字を載せない**（3.65:1）。
   **本文と地・選択・番号帯の 3 対が 4.5:1 以上であることは Win32 部品 probe で固定する**（core 単体では計算できず、`pow` は core で使えない）。確認記録に比を書く。
   **値そのものは hide の実機目視で動かしてよい**（画面案 `https://claude.ai/artifact/SHmNscVHWZjBmL3XLsqSXZ`。変える場所は 2 ファイルだけ）。`folio_palette_ink`（ADR 0017）はそのまま。
6. **切り替え時の再着色で本文・キャレット・Undo・変更印・モード・スクロール位置を失わない。**
   - 主窓・ドロワー・`folio_state` の palette の写しを差し替え、`command_brush` を作り直し、`decorate()` を呼び直し（何度でも可・現在値は呼ぶ側が持つ）、主窓とドロワーを全面 `InvalidateRect`。
   - `note_pane` に `note_pane_recolor(pane, background, text)` を 1 本足し、**閲覧・編集の両モードで呼ぶ**（地の色は RTF に入らない）。実測 (a5) の形: `EM_SETBKGNDCOLOR` → 変更印を `EM_GETMODIFY` で退避 → 既存の `text_document_id` で `ITextDocument` を取り
     `Undo(tomSuspend)` → **`EM_SETCHARFORMAT(SCF_ALL, CFM_COLOR)` と `EM_SETCHARFORMAT(SCF_DEFAULT, CFM_COLOR)` の 2 回**（既定書式を古い色のまま残さない。`SCF_DEFAULT` も Undo を積むので必ず suspend の内側）→ `Undo(tomResume)` → `EM_SETMODIFY` で復元。
     **この形（1 つの suspend で 2 回当てる）は未測定なので、実装の Win32 部品 probe で Undo の段数・変更印・選択・本文の不変を取り直す。** `ITextDocument` が取れなかったときは地の色だけ変えて本文の色は次の流し込みまで古いまま（Undo を汚さない方を取る）。
     `EN_CHANGE` は 1 回出るが、受け手は `pane_notification`（番号の表に印を付けるだけ）で、`EM_GETMODIFY` を読む production コードは無いので **`ENM_CHANGE` は外さない**。変更印の退避・復元は Issue #75 の受け入れ条件（`EM_GETMODIFY` 不変）のためのもので、(a4) へ簡略化しない。
     `streaming` の印は触らない。
   - **閲覧（RTF）**はさらに、決定 3 で作り直した `state->pane` を既存の `render_pane` で流し直す（1MB で 30ms・`streaming` の印は `note_pane_render` が立てる）。スクロール位置は保たれるので退避しない。
     **選択は消えるので `EM_EXGETSEL` で退避し、新しい `note_pane_restore_selection(pane, span)`（`EM_EXSETSEL` だけ・`EM_SCROLLCARET` を送らない）で戻す**（`note_pane_select` は流用しない。ADR 0023 の検索の一致の選択を保つ）。
   - 番号の帯の色は palette の差し替えで次の描画から変わる。
7. **設定画面は `command_layer` の自前描画で、レイヤーがフォーカスを持つ（ADR 0029 決定 9）。** `command_surface_mode` に `SETTINGS` を足す。行は「テーマ」の見出し行と `OS に従う` / `ライト` / `ダーク` の 3 行（`command_surface_rows(SETTINGS) = 4`・`command_help_rows(SETTINGS) = 0`・`command_box_height` の式はそのまま）。
   現在の選択の行の左に **GDI の塗った小円**（× と同じく線で描く。字形は使わない）。開いたとき現在値の行にカーソル（`show_search_surface` と同じ形の `show_settings_surface` が `command_selection` を上書きする）。↑↓ で選択肢を移り（見出し行は飛ばす）、
   **Enter で採用**（決定 3 の `folio_state_set_theme`）し、欄は開いたまま結果を状態の行に出す（失敗なら `folio_state_failure_line`）。Esc で閉じて元のフォーカスへ戻す（`command_return_focus` の規則は変えない）。マウスのクリックとホイールはパレットと同じ。
   案内 2 行は 1 行目が `UI_TEXT_SETTINGS_GUIDANCE`（↑↓ で選び Enter で採用・Esc で戻る）、2 行目は `SETTINGS_UNREADABLE` のセッションなら**開いた瞬間から**その理由の 1 行（印は既定値 `system` に付く）、そうでなければ空。
   文言は `ui_text` の ID（`UI_TEXT_SETTINGS_THEME` / `_THEME_SYSTEM` / `_THEME_LIGHT` / `_THEME_DARK` / `_GUIDANCE` / `UI_TEXT_COMMAND_SETTINGS`）。単位 C は「言語」の見出しと 3 行を同じ形で足す。
   **レイヤーの手続き**に `WM_SETFOCUS`・`WM_KILLFOCUS`（EDIT と同じ `folio_message_command_focus_lost` を post）・`WM_KEYDOWN`（既存の `command_key_down` / `command_navigate` を共有）・**`WM_CHAR`（既存の `command_character` を共有。Ctrl+S / Ctrl+N / Ctrl+P は `WM_CHAR` 経路。Enter の `'\r'` と Esc の 0x1B は飲む）**を足し、
   第 2 の鍵の経路を書かない。**`command_surface` の比較で `SETTINGS` の枝が要る関数**: `command_owns` / `command_focus_target` / `arrange_command_input`（EDIT を隠す）/ `command_input_rect`（帯の矩形を返さない）/ `command_return`（`execute_command_input` でなく採用）/
   `command_navigate` / `command_wheel` / `command_tab`（PALETTE と同じく効く・Tab は何もしない）/ `click_command_surface`（隠した EDIT へフォーカスを移さない）/ `draw_command_guidance`（パレットの文言を出さない）。switch の 6 本は列挙の追加で落ちる。
   IME は `ImmAssociateContext(layer, NULL)` で外す（生成時に 1 回。`eng/architecture.json` の `platformLibraries.ui_win32` に `imm32`、`CMakeLists.txt` に `nenefolio_system_link(nenefolio_window imm32)`・ARC-002）。
   `command_owns` に入る結果、Ctrl+S（#67 と同じ・保存して欄は開いたまま）と表のアクセラレータ 6 本が設定画面でも届く。意図どおり。
8. **入口は 3 つ**: (a) 右ペインの頭の**閉じるアイコンの左隣**の設定アイコン（**× と同じく GDI の線で描く歯車**: 小円と 8 本の短い放射線・色は `header_text`。`settings_rect` を `close_rect` の左に置き、`chip_rect` の `right` をその左へ（配置に効くのはこの 1 行）、
   `hit_test` に `HTCLIENT`、`click_caption` で開く。560 幅でもパンくずの最小 48px を割らないことを probe で確かめる）、(b) 「操作」メニューとパレットの「設定」（`FOLIO_COMMAND_SETTINGS`・listed 真。**`folio_command.c` の `catalog[]` を `lineTables` に足す**
   （`src/core/folio_command.h` → `src/core/folio_command.c`・prefix `FOLIO_COMMAND_`）ので値の追加漏れが落ちる）、(c) Ex `:set theme=system` / `theme=light` / `theme=dark`（`options[]` に 3 語。`enum folio_option` 3 → 6。`wanted_number` は `bool` を返すので、
   `execute_set_command` を「語 → `apply_number` か `apply_theme` へ振り分ける」形に組み直す）。どれも application の同じ意図へ渡る。閲覧中でも文書が無くても開ける。
9. **ゲートと表**: `folio_state_outcome` に値は足さない（`SETTINGS_STORE_FAILED` / `SETTINGS_UNREADABLE` / `OUT_OF_MEMORY` で足りる）。`ui_text` の ID と `catalog[]` の行は `lineTables` が守る。閾値・除外は変えない。
   確保失敗は `folio_settings_with_theme`・閲覧文書の作り直し（`set_theme` と `refresh_theme` の両経路）を `allocation_tests.c` で通す。
10. **やらないこと**: 言語（単位 C）、`name_prompt` と失敗箱のテーマ追随（OS 既定色のまま。別 Issue）、カスタム配色、起動中の DPI 変更との組合せの実測、`ChooseColorW` の窓の色。
11. **ADR 0029 の補正**: 文脈節の「`SCF_DEFAULT` は以後の文字だけ」を「`SCF_DEFAULT` も既存の本文の色を変え Undo を積む」に訂正する（決定本文は変えない）。

## 却下した選択肢

- 編集中は `EM_SETBKGNDCOLOR` と `SCF_DEFAULT` だけ当てる（(b)）: `SCF_DEFAULT` も Undo を積む（実測）。`EM_SETBKGNDCOLOR` だけでは本文の色が変わらない。
- `ENM_CHANGE` を外して当てる（(a7)）: Undo は止まらない。受け手も番号の帯だけなので外す理由が無い。
- TOM の `ITextRange::SetForeColor`（(a6)）: (a5) と同じ結果で COM が 1 段深い。既存の `CHARFORMAT2W` の形で足りる。
- `SCF_ALL` だけ当てる: 既定書式が古い色のまま残り、次のノートの平文が古い色で描かれる。
- `note_pane` を作り直す: 本文と Undo を失う（ADR 0029）。
- 閲覧の選択を `note_pane_select` で戻す: `EM_SCROLLCARET` でスクロールが動く。
- 閲覧の流し直しでスクロール位置を退避する: 実測で保たれる。
- `write_settings` の後に RTF を作り直す: 確保失敗でファイルと状態が食い違う。先に両方作る。
- コントラスト比を core の単体で固定する: 3 対の色が ui にあり、`pow` が core で使えない。
- `WM_THEMECHANGED` / `WM_SYSCOLORCHANGE` を契機にする: 来ない（実測）。レジストリを周期的に読む: 時計を読む場所が増える（ARC-007）。
- `folio_theme` に `SYSTEM` を足す・設定アイコンを操作行に置く・`:set` に `語=値` の文法を足す: ADR 0029 で却下済み。
- `folio_theme_resolve` を `folio_theme_choice.c` に置く: CNF-002 の接頭辞で落ちる。
- 設定アイコンと選択の印を字形（`⚙` `●`）で描く: CNF-010 で `ui_text.c` 行きになり、caption で初めてフォント依存（大きさ・ベースライン）を持ち込む。× と同じ GDI の線で描く。
- orange を本文や選択の面に使う: 白字と 3.65:1。札の面（短い語・地の色を字にする）に限る。
- 設定画面を EDIT を持ったまま作る: ADR 0029 決定 9 で却下済み。

## 検証

core: 版 1 → 2 の移行と往復・版 2 の解析（既定・3 値・壊れた値・未知の版・キー順・重複）・`folio_theme_resolve` の 6 通り・確保失敗。
application: `folio_state_set_theme`（`UNREADABLE` で断る・同じ値は書かない・先に作って書いてから差し替える・書けなければ不変・作れなければ不変・編集モードでも `pane` が新しい色）・`folio_state_refresh_theme`（`SYSTEM` のときだけ解決値が動く・確保失敗で不変）・
`folio_state_theme` の解決・偽 `appearance` の再読・`synchronize` を通さない・確保失敗（両経路）。
Win32 部品 probe（`out/design/2026-09-23/theme-ui-probe/`）: 1 つの suspend で `SCF_ALL` と `SCF_DEFAULT` を当てても本文・選択・Undo の段数・変更印が不変で本文と既定書式の色が変わる・閲覧の流し直しで選択が戻りスクロールが動かない・`WM_SETTINGCHANGE` で主窓が追従する・
設定面の ↑↓ Enter Esc・`WM_CHAR` の Ctrl+S が届く・`WM_KILLFOCUS` で閉じる・IME 文脈が NULL・設定アイコンの矩形と `hit_test`・`chip_rect` が重ならず 560 幅でパンくず 48px を割らない・560×360 と 150% で設定面が収まる（#69 の式）・
`folio_palette` と `rtf_palette` の 2 組の 3 対が 4.5:1 以上・`:set theme=` の 3 語・`catalog[]` から 1 行消すと `lineTables` で落ちる（`prove-gates.py` には足さない。既存の CNF-009 の反例で足りる）。
実機の目視（hide）: 2 組の配色の見え方（値は動かしてよい）・テーマ切替で窓の縁の色が変わる・Settings アプリで OS を切り替えたときの追従・IME ON で設定面に組成が始まらない・歯車と印の大きさと縦位置。統合チェックリストに足す。Waivers: none。
