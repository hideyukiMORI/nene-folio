# ADR 0035 — 名前入力面と失敗の箱は comctl32 v5 のまま `dialog_theme` の 1 本の塗りでテーマに従い、失敗の箱は `MessageBoxW` をやめて同じ塗りの自前のモーダルにする

- 状態: 受理（設計リナ 2026-09-23。Issue #82。実測 `out/design/2026-09-24/prompt-theme-probe/`（v5 / v6 の 2 本の塗りの表・失敗の箱の幅・13 か所の文脈・S6 / S7 / S8 の追試・Enter の直し方 V4〜V6）に基づき、現行コード（main `2a13318`）に照らした読み取り専用の批評（止める所見 8・直したい所見 12・追補 8）をすべて本文へ織り込んで受理した。草案は ADR 0034 の番号で書いたが、0034 は運用 ADR が取ったので 0035 になった）
- 日付: 2026-09-23
- Issue: #82（ADR 0031 決定 10 の積み残し）
- 影響する規則: ARC-001 / ARC-002 / ARC-004 / ARC-011 / ARC-012、C-002 / C-003 / C-005 / C-006 / C-007 / C-012 / C-017 / C-018、CNF-002 / CNF-009 / CNF-010、QLT-009 / QLT-010
- 関連: ADR 0010（`ChooseColorW` は OS の部品でライトのまま）、ADR 0020 / 0021（名前入力面）、ADR 0022（改名の経路。コンボは `EnableWindow(FALSE)`）、ADR 0030（文言は `ui_text`）、ADR 0031 決定 10（対象外にした分）・補正 D1（`inline_outcome`）、ADR 0032 決定 4（面の face は開く瞬間に決まる）、ADR 0033（`icon_paint`・DWM の呼び直し）

## 文脈

現行（main `2a13318`）:

- 名前入力面（`name_prompt.c`）は `DialogBoxIndirectParamW`（メモリ上の `DLGTEMPLATE`・EDIT・`COMBOBOX(CBS_DROPDOWNLIST)`（改名では `EnableWindow(FALSE)`）・STATIC・押し釦 2 本）で palette を見ない。
- 失敗の箱は `failure_box_show(owner, outcome, language)` → `MessageBoxW(MB_OK | MB_ICONWARNING)`（題 `NeNe Folio`・本文は `folio_state_failure_line`）。呼び出し 13 か所（ドロワー 5・主窓 8）は**すべて「知らせて戻る」だけ**で答えで分岐しない。`enum folio_state_outcome` 42 値のうち `inline_outcome` が真は 14 値、面が閉じているときは 40 値すべてが箱。
- `build/NeNeFolio.exe` に `Common-Controls` の manifest は無く、読み込まれる comctl32 は **v5.82**。ドロワーは主窓の `WS_CHILD`（`drawer_window.c`）で、ドロワーから箱を開くときの owner は `drawer->handle`。

実測（probe・DPI 120・OS ダーク・clang-cl 19.1.5）:

- **塗れる**（`WM_CTLCOLOR*` で palette のブラシを返す）: `WM_CTLCOLORDLG` の地・`WM_CTLCOLORSTATIC` の STATIC・`WM_CTLCOLOREDIT` の EDIT の中と**コンボの閉じた字の面**・`WM_CTLCOLORLISTBOX` のドロップダウンのリスト（本文と地 15.06:1・ライト 17.78:1）。
  **コンボは有効なとき `WM_CTLCOLOREDIT`、`EnableWindow(FALSE)` のとき `WM_CTLCOLORSTATIC`**（両方に答えないと改名のときだけ白く残る）。
- **塗れない**: EDIT の 1px の縁（`#646464`）・コンボの矢印の釦（`#F0F0F0`）・ドロップダウンの選択の帯（OS のアクセント）とスクロールバー・押し釦（`WM_CTLCOLORBTN` は来るが無視される）。**押し釦は `BS_OWNERDRAW` ＋ `WM_DRAWITEM` なら塗れる。**
- **comctl32 v6 は後退する**: `SetWindowTheme` は v5 では完全に無効。v6（manifest）では `DarkMode_*` が効くが色は uxtheme の `#333333` 固定で palette にならず、**コンボの閉じた面が `WM_CTLCOLOREDIT` を無視して白に戻る**。`IsAppThemed()` は両方 1 で判別に使えない。
- 題の帯: `DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE)` ＋ `DWMWA_CAPTION_COLOR` / `DWMWA_TEXT_COLOR` で `palette.window` と字の色になる（`hr=0`。`decorate()` と同じ dwmapi で `platformLibraries` は増えない）。
- 塗ってもフォーカス・IME 文脈・Enter → `IDOK`・Esc → `IDCANCEL` → `EndDialog`・owner の無効化は 1 つも変わらない。
- **S6（owner-draw の釦と鍵）**: `BS_OWNERDRAW` の釦は `WM_GETDLGCODE` が **`DLGC_BUTTON` だけ**（既定釦の `DLGC_DEFPUSHBUTTON` / `DLGC_UNDEFPUSHBUTTON` を返さない）。そのため **Cancel にフォーカスを置いて Enter を押すと `IDOK`**（現行の `BS_PUSHBUTTON` なら `IDCANCEL`）。`ODS_DEFAULT` は **一度も立たない**（`DM_SETDEFID` を呼んでも・`BM_SETSTYLE` を重ねても）。EDIT で Enter → `IDOK`、Space → その釦、Esc → `IDCANCEL` は変わらない。
  直し方の追試: **V4**（釦を `GWLP_WNDPROC` で subclass して `WM_GETDLGCODE` に `DLGC_(UN)DEFPUSHBUTTON` を足す）は Enter を直すが、**既定釦が動くたびに対話管理が `BM_SETSTYLE` を送って釦の型を `BS_OWNERDRAW` から `BS_PUSHBUTTON` / `BS_DEFPUSHBUTTON` に書き換え、以後 `WM_DRAWITEM` が来ない**（自前の塗りが止まる）。
  **V5**（面の `WM_COMMAND` で `IDOK`・`BN_CLICKED` のとき `GetFocus()` がもう一方の釦ならその id として扱う）は型を変えず、(c) EDIT で Enter・(d) Cancel で Enter・(e) Space・(f) Esc・(i) OK のクリック・(j) Cancel にフォーカスのまま OK をクリック、の**すべてが現行の押し釦と同じ値**になる（クリックは `WM_COMMAND` の前にフォーカスを OK へ移すので誤って読み替えない）。V6（V4 ＋ V5）は二重には効かないが V4 の型書き換えを抱える。
- **S7（枠の対比）**: `chip_background` と面の地 `window` は **ダーク 5.47:1・ライト 8.80:1**（3:1 以上）。草案が枠に使おうとした `border` は 1.56 / 1.38:1 で **3:1 未満**。
- **S8（owner）**: ドロワーは主窓の `WS_CHILD` なので、owner に `drawer->handle` を渡しても `GetAncestor(GA_ROOT)` を渡しても**実際の `GW_OWNER` は主窓**で、モーダル中は主窓 = 無効・ドロワー = 有効（子は親の無効化に従う）、閉じた後は両方有効。`MessageBoxW` でも同じ。**どちらでも動きは変わらない**。
- **失敗の箱**: `MessageBoxW` は OS がダークでも白く、色を渡す手段が無い。最長の en（131 単位）は client 約 432×135（96 DPI）。自前で描くと Yu Gothic UI 14 で折り返さないと 805px、**幅 480 で最大 2 行（38px）**。高さは文言ごとに `DT_CALCRECT` で決める。
  費用の見積り: (a) `MessageBoxW` のまま 0 行・(b) 自前のモーダル 約 160 行・(c) `command_layer` の 1 行へ寄せる 60〜100 行 ＋ 面が閉じているときの状態の行の新設 ＋ ドロワーからの `folio_message_*` 1 本。

現行コードに照らした批評で当たった検査器: `dialog_theme.h` に列挙を同居させると **CNF-002**（1 ファイル 1 型）、`ui_text` に ID を足して表に列を欠くと **CNF-009 / CNF-010 / CNF-011**。草案の段階で `eng/conformance.py` を回して見つけた（#88 と同じ）。

## 決定

**名前入力面と失敗の箱は comctl32 v5 のまま、新規 `dialog_theme` の 1 本の塗り（`WM_CTLCOLOR*` ＋ owner-draw の釦 ＋ DWM の題の帯）でテーマに従い、失敗の箱は `MessageBoxW` をやめて同じ塗りの自前のモーダルにする。面のテーマは開く瞬間に固定する。**

### 決定 1 — comctl32 は v5 のまま

manifest も uxtheme も `SetWindowTheme` も足さない。v6 はコンボの塗りを後退させ、`DarkMode_*` の色は palette にならない。塗りは `WM_CTLCOLOR*`・`BS_OWNERDRAW`・`CBS_OWNERDRAWFIXED`・DWM だけで行う。`eng/architecture.json` の `platformLibraries` は変えない（dwmapi は `decorate()` が既に結んでいる）。

### 決定 2 — 塗りの経路は `dialog_theme` の 1 本

新規 `src/ui/win32/dialog_theme.{h,c}`（外部関数の接頭辞 `dialog_theme_`・CNF-002）。名前入力面と失敗の箱の**両方**がこれを呼ぶ。別々に書けば第 2 の経路（ARC-001）。

- 型は不完全型 **`struct dialog_theme;`** ＋ `dialog_theme_create(const struct folio_palette *)` / `dialog_theme_destroy` の対（C-003 / C-007）。中に palette の写しと `HBRUSH` 4 本（地・札・EDIT の中・リスト）を持つ。**ブラシは `DialogBoxIndirectParamW` が返った直後に `destroy` で捨てる**（`name_prompt` の font と同じ対）。
- 役割は**面が決めて渡す**: 専用ファイル `dialog_theme_surface.h` の `enum dialog_theme_surface { DIALOG_THEME_DIALOG, DIALOG_THEME_LABEL, DIALOG_THEME_FIELD, DIALOG_THEME_LIST }`。面は `WM_CTLCOLOR*` の `lParam` の HWND を自分の子と比べて役割を決める（**無効なコンボの `WM_CTLCOLORSTATIC` は `FIELD`**）。
- `HBRUSH dialog_theme_color(const struct dialog_theme *, enum dialog_theme_surface, HDC)` が `SetTextColor` / `SetBkColor` を当ててブラシを返す（`DIALOG` = `window`・`LABEL` = `window` に `current_text`・`FIELD` = `pane` に `editor_text`・`LIST` = `pane` に `editor_text`）。面は**そのブラシを `INT_PTR` で return する**（`SetWindowLongPtrW(DWLP_MSGRESULT)` は使わない）。
- `void dialog_theme_draw_button(const struct dialog_theme *, const DRAWITEMSTRUCT *, enum dialog_theme_button)` が押し釦を**右ペインの頭の札と同じ形**で描く。`enum dialog_theme_button { DIALOG_THEME_BUTTON_PRIMARY, DIALOG_THEME_BUTTON_SECONDARY }` は専用ファイル `dialog_theme_button.h`。**`ODS_DEFAULT` は立たない（実測）ので、既定かどうかは面が渡す。** 既定 = `chip_background` の面に `chip_text`、他 = **`chip_background` の 1px の枠**に `current_text`（`border` は 3:1 未満・S7）。`ODS_FOCUS` は 1px 内側の枠、`ODS_SELECTED` は面を `selected_background`、`ODS_DISABLED` は字を `border`。文言は **`GetWindowTextW` で釦から取る**（`ui_text` の ID を二重に持たない）。`DrawTextW` の旗は `draw_chip` と同じ。角丸の札の形が `draw_chip` と `dialog_theme_draw_button` の **2 か所**になることは**明記して受け入れる**（片方は `folio_window` の DC への直描き、片方は `DRAWITEMSTRUCT`。共通化はこの単位の外）。面は `WM_DRAWITEM` / `WM_MEASUREITEM` に TRUE を返す。
- `void dialog_theme_draw_item(const struct dialog_theme *, const DRAWITEMSTRUCT *, const wchar_t *)` がコンボの項目（閉じた面とリストの行）を描く。`ODS_SELECTED` は `selected_background` / `selected_text`、`ODS_COMBOBOXEDIT`（閉じた面）は `FIELD` と同じ色、`ODS_DISABLED` は字を `border`。
- `void dialog_theme_decorate(const struct dialog_theme *, HWND dialog)` が `DwmSetWindowAttribute` の 3 属性（immersive dark・caption・text）を当てる。
- `WM_CTLCOLOR*` の値は `windows.h` の定数を使い、数を直書きしない。

### 決定 3 — 名前入力面（`name_prompt.c`）

- `WM_INITDIALOG` で `dialog_theme_create`（palette は呼び出し側が渡す）と `dialog_theme_decorate`。`WM_CTLCOLORDLG` / `STATIC` / `EDIT` / `LISTBOX` の 4 つを役割に変えて `dialog_theme_color` へ。**新しい `case` は `composing`（IME の門）より前に置く**（門の後ろだと組成中に色が抜ける）。
- 押し釦 2 本を `BS_OWNERDRAW` にし、`WM_DRAWITEM` を `dialog_theme_draw_button` へ（既定 = 「保存」「変更」側）。**Enter の読み替え（V5）**: `WM_COMMAND` で `LOWORD(wParam) == IDOK && HIWORD(wParam) == BN_CLICKED` のとき `GetFocus()` がキャンセルの釦なら `IDCANCEL` として扱う。V4（subclass）は採らない（型の書き換えで塗りが止まる・実測）。
- **EDIT は `WS_BORDER` を外し**（`inputs()` で）、面が `WM_PAINT` で EDIT の矩形を `InflateRect(1, 1)` した外側に **`chip_background` の 1px の枠**を描く（塗れない `#646464` の縁を消す。絞り込み欄 `draw_filter_frame` と同じ流儀）。
- **コンボは `CBS_OWNERDRAWFIXED | CBS_HASSTRINGS`**（`fill_categories` の `CB_ADDSTRING` 直後の解放と `CB_GETLBTEXT` を保つ）。`WM_MEASUREITEM` は `inputs()` の中で来るので行の高さは `prompt->font` から測る。有効・無効の両方の `WM_CTLCOLOR*`（`EDIT` と `STATIC`）に答える。
- `WM_DPICHANGED` で `lParam` の矩形へ `SetWindowPos`。
- **残るもの（v5 の限界として明記・目視の対象）**: コンボの矢印の釦とドロップダウンのスクロールバーは OS の色のまま。フォーカス・IME・Enter・Esc・`EndDialog` の経路と `command_return_focus` の戻りは変えない（実測で不変）。face は ADR 0032 のとおり開く瞬間に決まる。

### 決定 4 — 失敗の箱は `MessageBoxW` をやめ、同じ塗りの自前のモーダルにする（案 (b)）

- `failure_box.c` を `DialogBoxIndirectParamW`（メモリ上の `DLGTEMPLATE`（`name_prompt` の template を写す）・STATIC 1 つ・`BS_OWNERDRAW` の押し釦「OK」1 本（既定）・題 `NeNe Folio`）に置き換え、決定 2 の塗りを通す。
- **署名は `failure_box_show(HWND owner, enum folio_state_outcome, const struct folio_state *)` の 3 引数に変える**（草案の「署名は不変」を撤回・S3）。13 か所は `folio_state_language(self->state)` → `self->state` の機械的な置換。箱の中で `folio_state_language` と `folio_palette_for(folio_state_theme(...))` を引く。
- 釦の文言は新しい `UI_TEXT_PROMPT_OK` を `ui_text.h` の列挙・`ui_text.c` の 3 列・`ui_text_tests.c` の期待表の **3 か所**に足す（CNF-010 / CNF-011）。**`UI_TEXT_APP_NO_WINDOW` は列挙の末尾に残す**（CNF-009 の `lineTables` を動かさない）。
- 本文の幅は **`min(480, 測った幅)` に下限（釦 ＋ 余白）**（96 DPI 基準・DPI でスケール）。`WM_INITDIALOG` で `GetDC` ＋ font の `DT_WORDBREAK | DT_CALCRECT` から高さを決め（最長でも 2 行・38px）、`AdjustWindowRectExForDpi` ＋ `MoveWindow` で面の高さを本文に合わせる。
- Enter / Esc / OK / × で閉じ、戻り値は使わない。owner の無効化とフォーカスの戻りは `DialogBox` の既定に任せる（`MessageBoxW` と同じ）。
- **変わる振る舞い**: `MB_ICONWARNING` のアイコンと警告音が消える（`MessageBeep` は呼ばない）。統合チェックリストの目視項目にする。
- (c)（欄の 1 行へ寄せる）は採らない: 面が閉じているときの状態の行が無く、ドロワーからの経路も要り、「知らせて止まる」というモーダルの意味が変わる。

### 決定 5 — 面のテーマは開く瞬間に固定する

開いている面の追随（草案の決定 5: `WM_SETTINGCHANGE` でブラシを作り直して `RedrawWindow`）は**やめる**。palette は開くときに `dialog_theme_create` へ写し、モーダル中は入口 3 つ（歯車・パレット・`:set`）が使えないので言語・テーマの意図は来ない。OS の切替だけがモーダル中に起きうるが、閉じて開き直せば新しい色になる（ADR 0032 決定 4 の face と同じ流儀）。

### 決定 6 — ドロワーからの owner

ドロワーからの 5 か所の owner は **`GetAncestor(self->handle, GA_ROOT)`（主窓）**に揃える。動きは `drawer->handle` のままと同じ（実測・S8）だが、「モーダルの owner はトップレベル」という意図をコードに明示する。

### 決定 7 — やらないこと

comctl32 v6 / uxtheme / `SetWindowTheme`、釦の subclass、`ChooseColorW` の窓（ADR 0010 のまま OS の部品）、コンボの矢印とスクロールバーの塗り、失敗の箱のアイコンと音、ハイコントラスト、開いている面のテーマ追随、`draw_chip` との共通化。

## 強制

- ARC-002（`platformLibraries` 不変・宣言外の OS ライブラリは configure が拒む）: **active**。
- CNF-002（`dialog_theme_surface.h` / `dialog_theme_button.h` の分離・接頭辞）・CNF-009（`UI_TEXT_APP_NO_WINDOW` を末尾に残す）・CNF-010 / CNF-011（`UI_TEXT_PROMPT_OK` の 3 列）: **active**（`eng/conformance.py`）。
- 分岐網羅（QLT-009）: `ui/win32` は測定の対象外で、この単位は core / application に触れないので閾値は動かない。
- 塗りの色・Enter の読み替え・箱の高さ: **不能**（ゲートに Win32 の部品テストは無い）。実装の probe（画面上に出して `BitBlt`）と実機の目視で確かめる。

## 結果

- 得るもの: 名前入力面と失敗の箱が両テーマで palette の色になる（地・字・EDIT の中・コンボの閉じた面とリスト・押し釦・題の帯）。塗りは 1 本で、Enter / Esc / Space / クリックの意味は現行の押し釦と同じ値（実測 V5）。
- 失うもの: 失敗の箱のアイコンと警告音。角丸の札の描画が 2 か所。`failure_box_show` の署名が変わる（13 か所の機械的な置換）。
- 残る穴: コンボの矢印の釦とドロップダウンのスクロールバーは OS の色（v5 の限界）。IME の候補窓は IME 自身が描く。`WM_DPICHANGED` と塗りの組合せ・OS がライトのときの各部品の既定色は測っていない（この機械は OS がダーク）。実の鍵盤（`SendInput`）と Shift+Tab は測っていない。
- 緑であることが証明しないもの: フルゲートは色も Enter の読み替えも証明しない。証拠は実装の probe の実出力と目視。
- 移行: production の変更は `dialog_theme.{h,c}`（新規）・`dialog_theme_surface.h` / `dialog_theme_button.h`（新規）・`name_prompt.c`・`failure_box.{h,c}`・`ui_text.{h,c}` ＋ `ui_text_tests.c`・`folio_window.c` / `drawer_window.c`（13 か所の置換と owner 5 か所）・`CMakeLists.txt` のソース一覧。補正する文書: ADR 0031 決定 10・ADR 0010・ADR 0020 / 0021 / 0022 の「失敗の箱」の記述・GLOSSARY「失敗の箱」・統合チェックリスト。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| comctl32 v6 の manifest ＋ `SetWindowTheme(DarkMode_*)` | 色が uxtheme の灰色固定で palette にならず、コンボの閉じた面の塗りが後退する（実測）。production 全体への影響も測っていない |
| `MessageBoxW` のまま（案 (a)） | OS がダークでも白い（実測）。Issue #82 の目的そのもの |
| 失敗を `command_layer` の 1 行へ寄せる（案 (c)） | 面が閉じているときの状態の行が無く、ドロワーからの `folio_message_*` が要り、「知らせて止まる」意味が変わる |
| 押し釦を `WM_CTLCOLORBTN` で塗る | 無視される（実測）。`BS_OWNERDRAW` だけが塗れる |
| Enter の直しに釦の subclass（V4） | 既定釦の移動で対話管理が `BM_SETSTYLE` を送り、釦の型が `BS_OWNERDRAW` から外れて `WM_DRAWITEM` が止まる（実測）。`BM_SETSTYLE` の横取りまで抱えるより面の `WM_COMMAND` の 1 分岐（V5）が小さい |
| `ODS_DEFAULT` に頼って既定釦を描く | 一度も立たない（実測）。既定は面が渡す |
| 枠の色に `border` | 面の地と 1.56 / 1.38:1 で 3:1 未満（実測）。`chip_background` は 5.47 / 8.80:1 |
| 開いている面のテーマ追随 | モーダル中は入口が使えず実害が無い。palette をポインタで共有する形は面の寿命と絡む |
| コンボを EDIT ＋ 自前のリストに置き換える | 部品を作り直す量が大きく、この単位の目的（色）を超える。`CBS_OWNERDRAWFIXED` で項目だけ描く |
| EDIT の縁を `WM_NCPAINT` で塗る | EDIT のサブクラスが増える。`WS_BORDER` を外して面が枠を描く方が 1 か所 |
| 塗りを `name_prompt.c` と `failure_box.c` に別々に書く | 第 2 の経路（ARC-001）。`dialog_theme` の 1 本に寄せる |
| `dialog_theme.h` に列挙と関数を同居させる | CNF-002（1 ファイル 1 型）。役割と釦の列挙は専用ファイル |

## 検証

- 実装の probe（`out/design/2026-09-24/dialog-theme-probe/`・production の `dialog_theme.c` を一緒にコンパイル・画面上に出して `BitBlt`）: 名前入力面の地・STATIC・EDIT の中と枠・コンボの閉じた面（有効・無効）・リストの行と選択の帯・押し釦 2 本（既定と非既定・フォーカスの印）・題の帯が両テーマで palette の色で本文 4.5:1 以上・枠 3:1 以上／失敗の箱の最長 en が 480px で 2 行に収まり高さが合う／Enter（EDIT・OK・Cancel の各フォーカス）・Esc・Space・クリックが現行の押し釦と同じ値／IME 文脈が不変／`MessageBeep` を呼ばない／13 か所の呼び出しが 3 引数で揃う。
- ゲート: `platformLibraries` 不変・CNF-002 / 009 / 010 / 011・分岐網羅が下がらない。
- 実機の目視（hide）: 名前入力面と失敗の箱の見え方（両テーマ・コンボの矢印とスクロールバーが OS の色で残ること・アイコンと音が消えること）・DPI 96↔144・IME の候補窓。統合チェックリストに 82-* を足す。Waivers: none。


## 2026-09-23 の補正（実装・独立レビュー・Win32 部品 probe の後・決定本文は書き換えない）

実装（席 A / B）・独立レビュー（止める 2・直したい 6）・probe（`out/design/2026-09-24/dialog-theme-probe/`・production の `dialog_theme.c` を一緒にコンパイル・PASS 93 / FAIL 2）で分かったことと、設計リナの判断。

1. **`dialog_theme_create` の署名**は `[[nodiscard]] enum dialog_theme_outcome dialog_theme_create(const struct folio_palette *, struct dialog_theme **out)`
   で、結果は専用ファイル `dialog_theme_outcome.h` の `READY` / `NO_MEMORY` / `NO_BRUSH`（ARC-010 / C-005。`note_pane_create` と同じ流儀）。
   決定 2 が戻り値の型を書いていなかったのを補う。移行のファイル一覧に `dialog_theme_outcome.h` を足す。
2. **名前入力面は塗りを作れなければ開かない**（`NO_MEMORY` / `NO_BRUSH` は書体の失敗と同じ `FOLIO_STATE_OUT_OF_MEMORY` の経路。決定 3 に 1 句補う）。
3. **失敗の箱は塗り・書体・子窓を作れないときだけ `MessageBoxW` へ落として知らせる**（レビュー S1）。黙って戻れば FR-015「失敗の 1 行を利用者に見せる唯一の場所」が破れる。
   `MessageBoxW` はこの退避経路にだけ残り（`MB_OK` だけ・アイコンも音も無し）、正典の経路は自前のモーダル 1 本のまま（ARC-001 の「第 2 の経路」ではなく劣化時の退避）。
   内部の結果は専用ファイル `failure_box_outcome.h` の `SHOWN` / `FALLBACK`（公開の署名は変えない）。退避の経路は GDI や子窓の失敗を起こす手段が無く**一度も走らせていない**。
4. **面の色はすべて `dialog_theme` の写しから取る**（レビュー S2）。EDIT の枠は新しい `dialog_theme_frame(theme, HDC, RECT)` が描き、
   `name_prompt` は主窓の palette へのポインタを持たず、`dialog_theme_create` は `name_prompt_show` が `DialogBoxIndirectParamW` の**直前**に呼ぶ（決定 5「開く瞬間に固定」を構造で守る。塗りを作れなければ面を開かず `FOLIO_STATE_OUT_OF_MEMORY`）。
5. **フォーカスの印は `ODS_FOCUS` かつ `ODS_NOFOCUSRECT` でないときだけ**描く（開いた直後は印が出ない・OS の押し釦と同じ）。
   **閉じたコンボの面にもフォーカスの印を描く**（決定 2 に無かった追加。付けないとコンボにフォーカスがあることが見えない）。
6. **無効な EDIT（保留中の名前欄）の `WM_CTLCOLORSTATIC` も `FIELD`**（無効なコンボと同じ。決定 2 の役割の表に補う）。
7. **題の帯を暗くするかは `folio_palette_ink(window)` が白かで決める**（`dialog_theme_decorate` は palette しか受けない）。
8. **`failure_box.h` は `struct folio_state;` の前方宣言だけを公開し、`folio_state.h` は .c が読む**（C-003 / C-007。レビュー D5）。
9. **owner-draw の描画は `item->hDC` へ直接描く**（C-017 のメモリ DC は、ちらつきが目視で出たら直す。レビュー D6・目視項目 82-6）。
10. probe の実測（DPI 120・OS ダーク）: 画素の色は両テーマ 34 項目のうち **32 が palette と完全一致**、残り 2（ダークの非既定の釦の面・ライトの既定の釦の面）は
    **釦の字の行だけ R または G が 1 段ずれる**（ClearType の `DrawTextW` の丸めと推定・字の行の外は完全一致）。対比 18 件は全部 4.5:1 / 3:1 以上（最小 5.11:1）。
    最長の en の箱は本文 600×50（= 480×1.25・**2 行**）で client 650×155、短い文言は下限 160px が効く。鍵とクリック (c)〜(j) はすべて現行の押し釦と同じ値（V5）。
    **測っていないもの**: `dialog_theme_create` が `READY` 以外を返す経路・production の `name_prompt_show` / `failure_box_show` の実物（probe は同じ形の面）・IME・DPI 96 / 144・OS がライトのとき・v6。
11. 移行の「補正する文書」は **ADR 0031 決定 10（補正 3）・GLOSSARY「失敗の箱」・統合チェックリスト 82-1〜82-10** に絞る（ADR 0010 / 0020 / 0021 / 0022 は失敗の箱の見た目に触れていない）。

## 2026-09-23 の補正（実機目視の後・#119・決定本文は書き換えない）

hide が 2026-09-23 に失敗の箱を実機で見て、**「音は欲しい。印も欲しい」**と判断した（統合チェックリスト 82-7）。決定 4 の「アイコンは描かず `MessageBeep` も呼ばない」と、
決定 7「やらないこと」の「失敗の箱のアイコンと音」、「結果 / 失うもの」の「失敗の箱のアイコンと警告音」、検証節の「`MessageBeep` を呼ばない」「アイコンと音が消えること」を、
次のとおり**逆向きに補正**する（現物調査は `out/agents/119-probe/memo.md`）。

12. **警告音を戻す。** 箱が組み上がって見える直前（`initialize()` で書体と配置の両方が成功した所）に `MessageBeep(MB_ICONWARNING)` を 1 回呼ぶ。OS のサウンド設定に従い、
    戻り値は見ない（鳴らなくても箱の機能は変わらない）。**退避経路の `MessageBoxW` も `MB_OK | MB_ICONWARNING`** にして、正典の箱と同じ「音と印」を持たせる（303 行のコメントも直す）。
13. **三角の「!」の印を描く。** 意味は以前の `MB_ICONWARNING` と同じ「操作はしていない・本文はそのまま・やり直しが要る」で、40 値すべてに同じ印を出す（値で印を変えない）。
    描き方は ADR 0033 の流儀 C: `enum icon_paint_kind` に **`ICON_PAINT_WARNING`** を足し、`icon_paint.c` の点の表は **外周の三角 ＋ 縦棒の穴 ＋ 点の穴の 3 図形**を同じ path に入れて
    `gdiplus_fill_alternate` で抜く（歯車と同じ形）。24 の viewBox での目安は、三角 (12, 2.5) (22.5, 20.5) (1.5, 20.5)、縦棒 x 10.9〜13.1 / y 8〜14.5、点は中心 (12, 17.6) 半径 1.35。
    実装席は画素の丸めで ±0.5 まで動かしてよく、最終の値を probe の README に残す。色は **`chip_background`**（穴は箱の地 `window` が透ける。対比はライト 8.80:1 / ダーク 5.47:1 で 3:1 も 4.5:1 も超える）。
    OS のアイコン（`LoadIcon`）は借りない（テーマと DPI で浮く）。
14. **置き場と寸法。** 印の箱は他の印と同じ **24px（96 DPI）の正方形**で、本文の左に `box_gap` を空けて置き、**本文の 1 行目の高さの中央**に揃える。本文の折り返し幅 `box_line_limit`（480）は
    印の幅と隙間のぶん狭くなる（`measure_body` の 1 か所）。描くのは `failure_box.c` に **`WM_PAINT` を新設して `icon_paint_fill` を直接呼ぶ**（`folio_window.c` と同じ流儀。
    owner-draw の子窓を足して `WM_DRAWITEM` を CtlID で割る形は採らない＝「釦は 1 本」の前提を保つ）。
15. **確かめること。** production の `failure_box` の構造を写した probe（`out/design/2026-09-24/dialog-theme-probe/` の足場に `icon_paint.c` を結ぶ）で、両テーマの印の画素が
    `chip_background`・穴が `window` であること、最長の en（131 単位）が狭くなった幅でも **2 行以内**か（3 行なら箱の高さが `DT_CALCRECT` で伸びるだけで欠陥ではないが記録する）、
    DPI 96 / 120 / 144 で印と 1 行目の中央が揃うこと。実機の目視は 82-7 / 82-8 を「音が鳴り、三角の印が出る」に書き直して行う（記録の単位で）。
16. 移行で触る文書は **この補正・GLOSSARY「失敗の箱」（印と音の 1 句）・統合チェックリスト 82-7 / 82-8**（チェックリストは次の記録の単位）。ADR 0031 決定 10 は触れない。

## 2026-09-23 の補正（実機目視の後・#118・決定本文は書き換えない）

hide が 2026-09-23 に名前入力面を実機で見て、**保存先カテゴリのコンボだけが他の GUI と統一感を欠く**と判断した。絵で確かめた内訳は、決定 3 が容認した矢印の釦とスクロールバーに加えて、
**コンボの閉じた面の外周とドロップダウンのリストの外周の 1px の枠が OS の色のまま**で、すぐ上の名前欄（`dialog_theme_frame` の枠）と揃わないことである。hide の判断は「自前の部品にする（A）」。
現物調査（`out/agents/118-probe/memo.md`）で、主窓の「操作 ▾」の一覧は自前描画ではなく OS の `TrackPopupMenu` であり、面の中に浮く一覧を出す前例がコードに無いことが分かったので、
**開閉するプルダウンをやめ、面の中に常時開いた一覧を置く形（提案 E）**を probe（`out/design/2026-09-25/category-list-probe/`・PASS 47 / FAIL 0）で確かめてから決めた。

17. **保存先カテゴリは owner-draw の LISTBOX を常時開いて置く。** スタイルは `WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL`、
    `WS_BORDER` は付けず外周は名前欄と同じ `dialog_theme_frame`（`chip_background`）で面が描く。行は既存の `dialog_theme_draw_item`（`ODT_LISTBOX`。probe で地・選択の帯・字・無効時の `border` 化まで palette と一致）で描き、
    `WM_MEASUREITEM` の行高は現行のコンボと同じ式。項目の入れ方（`LB_ADDSTRING`・添字＝カテゴリの添字）と読み出し（`LB_GETCURSEL`）はコンボの `CB_*` を `LB_*` に置き換えるだけで、
    改名の経路の `EnableWindow(FALSE)` はそのまま（行の字が `border` に落ち、地は変わらない）。決定 3 の `CBS_OWNERDRAWFIXED | CBS_HASSTRINGS` はこれで置き換わる。
18. **見える行は 4 行に固定**（`LBS_NOINTEGRALHEIGHT`。項目が少なければ余白は `pane` の地、5 件以上なら OS のスクロールバーが出る）。
    面の配置は、一覧が占めるぶん（4 行 − 閉じたコンボの高さ）だけ**ヒントの 1 行・失敗の 2 行・釦の帯を下へずらし、面の高さを同じ量だけ伸ばす**。余白の値は変えない。
    数値は実装で `name_prompt.c` の既存の定数から決め、probe の README に 96 / 120 / 144 DPI の表で残す。
19. **鍵とマウス。** 一覧は普通の `WS_TABSTOP` の子なので Tab 順（名前欄 → 一覧 → 保存 → キャンセル）に乗り、↑↓ で選択が動く。一覧にフォーカスがあるときの Enter は
    `DLGC_WANTALLKEYS` を持たないので既定の釦（保存）に落ち、Esc はキャンセル、Space は何もしない（probe (a)〜(e)）。V5 の `reads_as_cancel` は無改修で成立する。
    **行のダブルクリック（`LBN_DBLCLK`）は「保存」に読み替える**（面の `WM_COMMAND` で `IDOK` と同じ経路へ。第 2 の保存経路は作らない）。単一選択なので選択行＝フォーカス行で、フォーカス専用の飾りは描かない。
20. **残る OS の部品はスクロールバーだけ**（5 件以上のとき。幅 `SM_CXVSCROLL`・OS の 2 色）。矢印の釦と 2 つの外周の枠は消える。決定 3 の「残るもの」をこの 1 つに書き換える。
    IME・`WM_DPICHANGED`（面は開いた瞬間の DPI で固定）・comctl32 v5 のままであることは変えない。
21. **確かめること。** `category-list-probe` を最終の配置の数値に合わせて更新し（両テーマ × 3 件 / 8 件の画素・無効化・鍵 7 場面・3 DPI の配置表）、実機では名前入力面（保存・別名保存・改名）の
    ダーク／ライトの絵を撮る（統合チェックリスト 82-1 / 82-3 / 82-4 を「一覧」の文言に書き直すのは次の記録の単位）。移行で触る文書は **この補正・GLOSSARY「名前入力面」（コンボ → 一覧の 1 句）**。

