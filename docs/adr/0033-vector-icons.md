# ADR 0033 — 閉じる・設定・折畳・選択の印は 24 の viewBox の面のパスとして ui に持ち、OS 同梱の GDI+ を型定義の無い自前の宣言で結んで既存の DC に直接塗る

- 状態: 受理（設計リナ 2026-09-23。Issue #88。hide が画面案 `https://claude.ai/artifact/UiUEYVVnhqUpYNCXxV8Ry3` の流儀 C（面で塗る・角丸の板）を選んだ。実測 `out/design/2026-09-23/icon-probe/`（42 + 5 項目・幾何の突き合わせ 0 件差）と、現行コード（main `4dcd3c7`）に照らした読み取り専用の批評（止める所見 8・直したい所見 12。CNF-002 は検査器を実際に走らせて判定）を経て直した）
- 日付: 2026-09-23
- Issue: #88
- 規則: ARC-001 / ARC-002 / ARC-003 / ARC-004 / ARC-012、C-002 / C-003 / C-005 / C-006 / C-007 / C-008 / C-012 / C-017、CNF-002 / CNF-009 / CNF-010、QLT-009 / QLT-010
- 関連: ADR 0005（案2 堅）、ADR 0009（ドロワーの DIB とフェード）、ADR 0011 / 0016 決定 9（閉じる・設定の位置）、ADR 0013（Escape で閉じない）、ADR 0015 決定 7（カーソルの角の位置）、ADR 0030（`ui_text` の `GLYPH_*`）、ADR 0031 決定 7・8(a)（歯車と印は GDI の線）、ADR 0028 / 0031（`platformLibraries` の正典経路）

## 文脈

- 現行: 閉じる × は GDI の線 2 本（`draw_close`・pen は `scale(1, dpi)` で 1 → 2 → 2px と飛ぶ）、歯車は GDI の線（`draw_settings_icon`）、設定画面の選択の印は `current_text` の小円（`draw_settings_mark`・直径 `base_settings_mark = 8`）、折畳印はドロワーが字形 `−` `+`（`UI_TEXT_GLYPH_MINUS` / `PLUS`。`toggle_room` が字形の実測幅 ＋ `base_mark_gap`(6) で、カテゴリ行のカーソルの角は `mark.right - toggle_room` に置かれる。カテゴリ名の省略幅は `toggle_room` に依存しない）。
  主窓の描画面は `blit_pane` の `CreateCompatibleBitmap`（DDB・damage の大きさ・`SetViewportOrgEx` で切る。`draw_close` は damage との交差を見ずに毎回描く）。ドロワーは `create_surface` の 32bpp DIB で、`paint_surface` は `draw_rows` → `draw_header` → **既存の `GdiFlush()`** → `draw_fade` の順（`mix` は 16/8/0 ビットしか組まないのでフェード帯のアルファは 0 に戻る）。`draw_pane` は `SelectObject(mono_font)` / `SetBkMode` / `SetTextCharacterExtra` を当てたあとにアイコンを描き、続けて `draw_actions` と `draw_gutter` が文字を描く。
  主窓の `window_procedure` は `WM_NCCREATE` を扱わず（`self` は `WM_CREATE` の `on_create` で `GWLP_USERDATA` に入る）、`WM_DESTROY` の `window_destroyed` が `self->handle = nullptr` にするので、続く `WM_NCDESTROY` は既定処理を無効な HWND で呼んでいる（既存の欠陥・別 Issue）。`folio_window_destroy` はメッセージループを抜けた後に `DestroyWindow` → `free` の順で呼ばれ、生成の途中で失敗した経路も同じ関数を通る。
- **実測（`icon-probe`）**: Windows SDK の `gdiplus*.h` は C++ 専用（6 組を試して C から通る組が無い）。`windows.h` だけを読み、使う関数を無修飾の名前で宣言すれば `gdiplus.lib` に解決する（名前は装飾されていない・`GdiplusStartup` が 0 を返す）。
  画面案 C にベジェは無く `M/L/l/h/v/a/z` と `<rect rx>` だけ。点の表（閉じる 12 点・歯車の外周 32 点）は画面案の `d` と 1 点も違わず、塗った量は幾何の面積と 3% 以内。**`SmoothingModeAntiAlias` ＋ `PixelOffsetModeHalf`** で 96 / 144 / 192 DPI・16 / 24px の 20 通りすべてに中間色が出て黒白の縁は無い。
  `GdipCreateFromHDC` は `SetViewportOrgEx` / `IntersectClipRect` を引き継ぐ（DIB で実測。DDB と、GDI の状態（選択フォント・文字色・`SetBkMode`・`SetTextCharacterExtra`）が前後で不変かは未測定）。GDI+ は DIB のアルファ欄に `0xFF` を書く。色は `0xFF000000 | R<<16 | G<<8 | B`（`COLORREF` は `0x00BBGGRR`）。
  費用: `GdiplusStartup` 0.8〜1.5ms・1 回の描画（graphics を作る ＋ 4 形 ＋ 捨てる）0.022ms・`GdiplusShutdown` 約 5ms。静的に結んでも起動時間は変わらない。破棄の順は主窓の `WM_NCDESTROY` が最後。`Shutdown` 後の `Gdip*` は落ちずに `GdiplusNotInitialized` を返す。
  歯車は `FillModeAlternate` でないと穴が埋まり、`+` は `Alternate` だと中心が抜ける。**高さ 2 単位の板と 8 単位の印は箱が 20px 未満だと塗りが 50〜75% の灰色になる**（8px の箱では印の実寸が 2.67px）。96 DPI では面の × が現行の線より 1.53 倍濃い（腕 1.98 単位）。`close_rect` / `settings_rect` は `base_close_size = 24` なので 24 の viewBox と 1:1。
- **CNF-002 の実測**（批評）: `enum icon_paint_kind {…}` を `icon_paint.h` に置くと「filename does not match」で落ちる。`typedef float …;` / `typedef struct … {…} …;` を持つヘッダは「multiple top-level type definitions」で落ちる。`struct GpGraphics;`（不完全型の宣言）と `constexpr int` は型定義に数えられない。名前付き `struct icon_point {…}` を `icon_paint.c` に置くと落ち、無名 struct の配列（`folio_window.c` の `settings_teeth[]` と同じ形）は通る。
- `conformance.py` の `architecture_checks` は `src/**/*.c` が build に居ることを要求する。`platformHeaders` と `symbols.py` は core / application だけを見る。CNF-010 は文字列リテラルだけを見る（コメントは対象外）。`lineTables` は `ui_text.h` ↔ `ui_text.c` だけで、`ui_text_tests.c` の期待表は `expected_count == UI_TEXT_APP_NO_WINDOW + 1` とコンパイルが守る（`APP_NO_WINDOW` は列挙の最後）。`eng/coverage-policy.json` は ui を測らない。

## 決定

1. **GDI+ は OS 同梱ライブラリとして正典経路で結ぶ。** `eng/architecture.json` の `platformLibraries.ui_win32` に `gdiplus`、`CMakeLists.txt` に `nenefolio_system_link(nenefolio_window gdiplus)`（`imm32` の隣）と **`src/ui/win32/icon_paint.c` をソース一覧に**足す（ARC-002）。ARC-003 / ARC-007 のシンボル検査と `platformHeaders` は ui を見ないので触れない。
2. **宣言は自前の 1 ヘッダ `src/ui/win32/gdiplus_flat.h` で、型定義を 0 にする。** `windows.h` だけを読み、使う関数を SDK の `gdiplusflat.h` の署名の字面で **`WINAPI` 付き・無修飾の名前**で宣言する（`gdiplus.lib` の名前は装飾されていないので C から解決する）。
   対象は**不完全型の宣言**（`struct GpGraphics;` / `struct GpPath;` / `struct GpBrush;` / `struct GdiplusStartupInput;` / `struct GdiplusStartupOutput;`）、座標は `float`、Status / FillMode / SmoothingMode / PixelOffsetMode / MatrixOrder は使う値だけ `constexpr int`（`typedef` は置かない）。
   使う関数はちょうど次の **17 本**: `GdiplusStartup` / `GdiplusShutdown` / `GdipCreateFromHDC` / `GdipDeleteGraphics` / `GdipSetSmoothingMode` / `GdipSetPixelOffsetMode` / `GdipTranslateWorldTransform` / `GdipScaleWorldTransform` / `GdipResetWorldTransform` /
   `GdipCreatePath` / `GdipDeletePath` / `GdipAddPathLine` / `GdipAddPathArc` / `GdipAddPathEllipse` / `GdipClosePathFigure` / `GdipCreateSolidFill` / `GdipDeleteBrush` / `GdipFillPath`（`GdipAddPathBezier` と `GdipSetPathFillMode` は使わない・C-008）。
   `struct GdiplusStartupInput` の定義は専用ファイル **`src/ui/win32/gdiplus_startup_input.h`**（ファイル名 = 型名・CNF-002）に置き、`DebugEventCallback` は `void *` でなく **`void (WINAPI *)(void)`** の関数ポインタで宣言して常に `nullptr`（C-006。x64 のレイアウトは同じ）。`GdiplusStartupOutput` は背景スレッドを既定のままにするので **`nullptr` を渡し**、定義は置かない。
   ヘッダ冒頭に「SDK の `gdiplus*.h` は C++ 専用で C から読めないため、名前と署名は SDK の `gdiplusflat.h` を写した」と根拠を書く。署名の一致は Win32 部品 probe（`GdiplusStartup` が 0・5 形が塗れる）で固定し、`_Nonnull` を使う `icon_paint.c` から include した状態でコンパイルが通ることも確かめる。
3. **形は 24 の viewBox の点の表として `src/ui/win32/icon_paint.c` に持つ。** 列挙 `enum icon_paint_kind`（`CLOSE` / `SETTINGS` / `FOLD_COLLAPSE` / `FOLD_EXPAND` / `SELECTION`）は専用ファイル **`icon_paint_kind.h`**、関数は `icon_paint.h` の
   `void icon_paint_fill(HDC, RECT bounds, enum icon_paint_kind, COLORREF)`（4 引数・上限ちょうど）の 1 本。点の表は**無名 struct の配列**（`static const struct { float x; float y; } close_points[] = …`）で、値は画面案 C の `d` 属性を**相対を絶対に畳んで**写す（閉じる 12 点・歯車の外周 32 点・角丸の板 `rect rx`・角丸の四角 8×8 rx=2）。
   **画面案の値を変えない**（歯車の外周の +0.05〜0.10 単位のずれもそのまま）。
   写しの規則: (1) `path` は絶対座標の点列で、続く 2 点ごとに `GdipAddPathLine`、`z` は `GdipClosePathFigure`。(2) `rect rx` は直径 `2rx` の四分円 4 つ（左上 180° → 右上 270° → 右下 0° → 左下 90°・`sweep 90`）で直線は GDI+ が継ぐ。(3) 円の穴は `GdipAddPathEllipse`。
   (4) 箱への写像は `ResetWorldTransform` → `TranslateWorldTransform(left, top)` → `ScaleWorldTransform(size / 24)` → `FillPath` → `Reset`（倍率は `(right - left) / 24.0f` の 1 行）。**FillMode は `GdipCreatePath(mode, &path)` の引数だけで決める**（歯車は `Alternate`・他は `Winding`）。
   `icon_paint_fill` は呼ばれるたびに `GdipCreateFromHDC` → `SetSmoothingMode(AntiAlias)` → **`SetPixelOffsetMode(Half)`** → パスを作る → 塗る → 捨てる（0.022ms。使い回さない）。色は `COLORREF` → ARGB（`0xFF000000 | R<<16 | G<<8 | B`）の 1 関数。
   **形ごとの static 関数に割る**（全値 switch は `default` 無し・C-002。1 関数 60 行・複雑度 10・C-012）。`Gdip*` が 0 以外を返したら**何も描かず戻る**（描画関数は結果を持たない現行の `draw_*` と同じ）。
4. **起動と終了は窓手続きの外に置く。** `GdiplusStartup` は `folio_window_create` の `CreateWindowExW` の**前**、`GdiplusShutdown` は `folio_window_destroy` の `DestroyWindow` の**後・`free` の前**（メッセージループを抜けた後・子窓の `WM_NCDESTROY` はすべて終わっている・生成の途中で失敗した経路でも 1 対 1）。token は `struct folio_window` が持つ。
   **`GdiplusStartup` が失敗したら窓を作らず `folio_window_create` を失敗にする**（枠なし窓で × が描けないと閉じる入口が見えなくなる・ADR 0013。`main.c` の既存の「窓を作れませんでした。」の経路に乗る）。
   `WM_NCCREATE` / `WM_NCDESTROY` に case は足さない（`WM_NCCREATE` では `self` が無く、`WM_NCDESTROY` は既存の欠陥の上に乗る。その欠陥（`window_destroyed` が `self->handle` を消した後の `WM_NCDESTROY` が無効な HWND で既定処理を呼ぶ）は**別 Issue** にする）。
5. **描く先は既存の DC。** 主窓は `blit_pane` の DDB のメモリ DC に client 座標のまま（`SetViewportOrgEx` を引き継ぐ。damage との交差は見ずに毎回描く現行どおり）、ドロワーは `create_surface` の DIB に直接。ドロワーの `GdiFlush()` は **`paint_surface` の既存の 1 本**（`draw_fade` の直前）で足りる。
   `drawer_window.c` の「画素は `0x00RRGGBB`」のコメントを「GDI+ が塗った画素はアルファ欄が `0xFF`。`mix` は RGB だけを組むのでフェード帯では 0 に戻り、`BitBlt(SRCCOPY)` はアルファを見ない」に直す（ADR 0009 の本文には無い文なので ADR は触らない）。
   **GDI+ が HDC の GDI の状態（選択フォント・文字色・`SetBkMode`・`SetTextCharacterExtra`・ビューポート原点）を変えないこと**と、**DDB のメモリ DC と damage の外に置いたパスのクリップ**を Win32 部品 probe で固定する（未測定の 2 点）。
6. **置き換える 5 か所と箱（96 DPI の値・DPI に比例）**:
   - 閉じる: `draw_close` → `icon_paint_fill(CLOSE, header_text)`・箱は `close_rect`（24px）のまま。
   - 設定: `draw_settings_icon` → `SETTINGS`・箱は `settings_rect`（24px）のまま。
   - 選択の印（設定画面）: `draw_settings_mark` → `SELECTION`・**箱は 24px の正方形 `[row.left + 4, row.left + 28]`** を行（32px）の縦中央に（印の ink は箱の左端から 8px なので現行の ink 位置 `row.left + 12` を保つ。選択肢の札は `row.left + 32` から始まるので重ならない）。
     **色は `current_text` から `chip_background` に変える**（画面案どおり。ADR 0031 決定 7 の補正として記す。暗テーマでは `selected_background` の面に橙の印が乗る。hide の目視で決める）。
   - 折畳（ドロワーの `draw_category`）: `FOLD_COLLAPSE` / `FOLD_EXPAND`・`header_text`・**箱は 24px の正方形 `[width - base_right_inset - 24, width - base_right_inset]`** をカテゴリ行（34px）の縦中央に（板の ink は箱の x = 6..18。20px 未満では灰色）。
     `toggle_room` は字形の幅を測るのをやめ **`scale(24, dpi) + base_mark_gap`（96 DPI で 30px）** の定数にする。**カーソルの角の x は `mark.right - toggle_room` のままなので現行より約 15px 左へ動く**（ADR 0015 決定 7 の補正として記す。カテゴリ名の省略幅は変わらない）。
   当たり判定の矩形（`close_rect` / `settings_rect` / カテゴリ行の折畳の当たり）は変えない。`WM_DPICHANGED` の手当ては要らない（倍率は毎回 `bounds` から出す）。
7. **× の濃さは画面案の幾何をそのまま採り、定数 1 つで調整できる形にする。** 96 DPI で現行より 1.53 倍濃くなるのは hide が選んだ絵の性質で、実機で見て太いと感じたら `close_points[]`（腕の幅 1.98 単位）だけを細らせる（1.6 単位で現行と同じ濃さ）。ADR は値を固定しない。
8. **`ui_text` から `UI_TEXT_GLYPH_MINUS` / `UI_TEXT_GLYPH_PLUS` を消す**: 触るのは `src/core/ui_text.h` の列挙（と冒頭の群の一覧コメントから `GLYPH` を外す）・`src/core/ui_text.c` の表・`tests/unit/ui_text_tests.c` の期待表の **3 か所**。`APP_NO_WINDOW` が列挙の最後なので `expected_count` の不変は保たれ、CNF-009 の `lineTables` は設定を変えずに整合する。確認記録に「字形から絵へ」と書く。
9. **補正を足す文書**: ADR 0031 決定 7（小円 → 面の印・色）・決定 8(a)（線の歯車 → 面）・却下の選択肢（「GDI の線で描く」は覆るが「字形は使わない」理由は有効）、ADR 0030 決定 1 の群の一覧と補正 4（`GLYPH_MINUS` の U+2212）、ADR 0015 決定 7（角の x）、`docs/GLOSSARY.md` の「`+` / `−` の左」の言い回し。`docs/KEY_BINDINGS.md` は更新不要。
10. **やらないこと**: SVG ファイルの実行時読み込み、Direct2D、第三者ライブラリ、`SuppressBackgroundThread`、ホバーの表現、他の記号（`◀` `▶` `▾` `↑` `↓`）の絵化（文言の一部なので `ui_text` のまま）、ハイコントラスト（palette は暗・明の 2 組のまま）、`WM_NCDESTROY` の既存の欠陥の修正（別 Issue）。

## 却下した選択肢

- SDK の `gdiplus.h` / `gdiplusflat.h` を読む: C++ 専用で C から通る組が無い（6 組を実測）。
- GDI の `Polygon` ＋ `FillPath`: 中間色 0 画素で、座標が整数（実測）。Direct2D: Issue の「やらないこと」。
- 自前ヘッダに `typedef` や `struct` の定義を置く: CNF-002 が「複数の型定義」「ファイル名と不一致」で落ちる（実測）。型定義 0 にし、`GdiplusStartupInput` だけ専用ファイル。
- `enum icon_paint_kind` を `icon_paint.h` に置く・点の表を名前付き `struct` にする: CNF-002 が落ちる（実測）。
- graphics やパスを使い回す: 1 回の描画が 0.022ms で、使い回しの利得は 0.006ms。
- 起動・終了を `WM_NCCREATE` / `WM_NCDESTROY` に置く: `WM_NCCREATE` の時点で `self` が無く（`GWLP_USERDATA` は `WM_CREATE`）、`WM_NCDESTROY` は `self->handle` が消えた後の既存の欠陥の上に乗る。`folio_window_create` / `folio_window_destroy` なら順序と寿命が自明。
- `GdiplusShutdown` を `WM_DESTROY` に置く: ドロワーの破棄の間 GDI+ が落ちた状態になる（実測の順序）。
- `GdiplusStartup` の失敗で × だけ GDI の線に落とす: 第 2 の経路（ARC-001 / ARC-012）。窓を作らない方を取る。
- 遅延読み込み: 静的に結んでも起動時間は変わらない（実測）。
- 折畳の板や選択の印を 16px 以下の箱に置く: 塗りが灰色になる（実測。8px の箱では印が 2.67px）。
- × の腕を先に細らせる: hide が選んだ絵を先に変えない。実機で見てから定数 1 つで直す。
- 全形を `Winding` に揃えて歯車の穴を逆回りで足す: 動くが、SVG の `evenodd` を `Alternate` で写す方が画面案との対応が読める。
- 折畳印の字形を残す: 絵と字形の 2 経路になる。
- `GdipSetPathFillMode` と `GdipCreatePath(mode)` の両方: 二重。生成時の引数だけ。

## 検証

Win32 部品 probe（`out/design/2026-09-23/icon-ui-probe/`・メモリ DC の画素・`WS_EX_TOOLWINDOW` + `HWND_TOPMOST` で画面上に出す）: `GdiplusStartup` が 0・5 形が 96 / 144 / 192 DPI で中間色を持ち黒白の縁が無い・歯車の穴と `+` の中心（FillMode）・塗った量が幾何の面積と 3% 以内・点の表が画面案の `d` と一致（`geometry.py`）・
**GDI の状態（フォント・文字色・`SetBkMode`・`SetTextCharacterExtra`・ビューポート原点）が `icon_paint_fill` の前後で不変**・**DDB のメモリ DC に描けて damage の外のパスがクリップされる**・主窓の `SetViewportOrgEx` 下で位置が合う・ドロワーの DIB のアルファ欄と既存の `GdiFlush` 後の `draw_fade`・選択の印の箱 `[row.left + 4, +28]` と札の非重なり・折畳の箱とカーソルの角の x（数で）・
`Startup` 失敗時に窓が作られない（偽の失敗を注入できなければ「測っていない」と書く）・`folio_window_destroy` の後に `Gdip*` が `GdiplusNotInitialized`。
ゲート: `architecture.json` の `gdiplus`・ソース一覧・`lineTables` の整合（`GLYPH_*` を消した後）・`_Nonnull` の TU から include して警告 0・分岐網羅が下がらない（ui は対象外）。
実機の目視（hide）: 96 DPI の × の濃さ・24px の歯車が読めるか・折畳の板の太さと左右位置・選択の印の位置と色（暗テーマの橙 on 紫）・両テーマ・DPI 96↔144・設定画面の開閉で印が正しい行・絞り込み中のカテゴリ行の折畳印。統合チェックリストに足す。Waivers: none。

## 2026-09-23 の補正（実装と Win32 部品 probe の後・決定本文は書き換えない）

実装の probe は `out/design/2026-09-23/icon-ui-probe/`（43 項目 ＋ 幾何の突き合わせ 0 件差）、
結果のまとめは[確認記録](../quality/2026-09-23-icon-checks.md)。

1. **決定 2 の「ちょうど 17 本」は数え違いで、列挙されているのは 18 本。** 宣言したのは列挙どおりの
   18 本（`GdiplusStartup` / `GdiplusShutdown` / `GdipCreateFromHDC` / `GdipDeleteGraphics` /
   `GdipSetSmoothingMode` / `GdipSetPixelOffsetMode` / `GdipTranslateWorldTransform` /
   `GdipScaleWorldTransform` / `GdipResetWorldTransform` / `GdipCreatePath` / `GdipDeletePath` /
   `GdipAddPathLine` / `GdipAddPathArc` / `GdipAddPathEllipse` / `GdipClosePathFigure` /
   `GdipCreateSolidFill` / `GdipDeleteBrush` / `GdipFillPath`）で、**足しても引いてもいない**。
   設計 probe が使っていた `GdipStartPathFigure` / `GdipResetPath` / `GdipSetPathFillMode` /
   `GdipAddPathBezier` は要らない（`GdipClosePathFigure` の次の `Add*` が新しい図形を始める）。
2. **型の名前は SDK の綴りではなく、CNF-002 が要求する綴りにした。** `struct GdiplusStartupInput` を
   `gdiplus_startup_input.h` に置くと「filename does not match」で落ちるので、型名は
   **`struct gdiplus_startup_input`**（メンバーも `version` / `debug_event_callback` /
   `suppress_background_thread` / `suppress_external_codecs` の snake_case）にした。
   x64 のレイアウトが SDK の `GdiplusStartupInput` と一致していることが署名の根拠で、
   `GdiplusStartup` が `Ok` を返すことを probe（1-1）で固定している。
   第 3 引数の `GdiplusStartupOutput` も同じ理由で **`struct gdiplus_startup_output;`**（不完全型のまま）。
3. **`GdiplusStartup` は `folio_window_create` の中の `start_gdiplus()` という小さな static に割った。**
   本体に直に書くと `folio_window_create` が 63 行になり `readability-function-size`（C-012 の 60 行）で落ちる。
   置き場所（`CreateWindowExW` の前）と失敗の扱いは決定 4 のとおり。
4. **`add_round_rect` は角丸の表の添字を受ける。** 決定 3 は写しの規則 (2) を 1 本に保てと言うが、
   `{x, y, width, height, radius}` を引数で渡すと 6 引数になって C-012 の 4 を超える。
   無名 struct のままでは型名が書けない（名前を付けると CNF-002 が落ちる）ので、
   `round_rects[]` の 1 表に 3 枚を入れて `constexpr size_t` の添字で選ぶ形にした。
   点の表（`close_points` / `settings_points`）は決定どおり無名 struct の配列で、
   ループは形ごとの static 関数がそれぞれ持つ。
5. **カーソルの角の x は「約 15px」ではなく 18px 左へ動く。** 96 DPI の Consolas 11px で
   U+2212 の実測幅は **6px** なので、旧 `toggle_room` は 6 + 6 = 12px、新しい定数は 24 + 6 = 30px、
   差は **18px** である（probe 8-5）。ADR 0015 決定 7 の補正にもこの数で書く。
6. **未測定だった 2 点はどちらも測れて、どちらも決定のとおりだった。**
   GDI の状態は 9 つ（フォント・文字色・`GetBkMode`・`GetTextCharacterExtra`・ビューポート原点・
   ウィンドウ原点・`GetMapMode`・ブラシ・ペン）とも不変（5-1）。
   DDB のメモリ DC には描けて、damage の外に置いたパスは 1 画素も出ない（6-1）。
7. **`GdiplusStartup` の失敗で窓を作らないことは測っていない。** 偽の失敗を注入する手立てが無い
   （`gdiplus.dll` の差し替えか API のフックになる）。コードの形を読んで確かめただけである。
   代わりに、**GDI+ が落ちている状態で `icon_paint_fill` を呼んでも落ちず何も描かない**ことは測った（9-2）。
8. **絞り込みの欄の `×`（`paint_filter_clear`・#86）は GDI の線のまま残した。** 決定 6 が挙げる
   5 種に入っていないので触っていないが、**× を描く経路が 2 つ残る**（ARC-001 / ARC-012 の観点で
   望ましくない）。箱が 20px で `base_filter_clear_inset` の刻み方も頭の × と別なので、
   同じ面のパスへ寄せるなら箱の数から決め直す必要がある。設計リナの判断で別 Issue にするか次の単位で扱う。

## 2026-09-23 の補正（#93・決定本文は書き換えない）

9. **補正 8 の「× を描く経路が 2 つ」は Issue #93 で 1 つにした。** `paint_filter_clear` の GDI の線 2 本を
   `icon_paint_fill(device, filter_clear_rect(self, window), ICON_PAINT_CLOSE, header_text)` に置き換え、
   **× を描く経路は `icon_paint.c` の `close_points[]` 1 本だけ**になった（ARC-001 / ARC-012）。
   置き換えたのは描き方だけで、**箱（`filter_clear_rect` の 20px の正方形）・当たり判定
   （`filter_clear_pressed`）・`EM_SETMARGINS(EC_RIGHTMARGIN, 20)` は 1 つも変えていない**。
   線の内側の刻みだった `base_filter_clear_inset`(6) は、24 の viewBox を箱へ 20/24 で写す形になって
   要らなくなったので消した。決定 6 の一覧はこれで 6 か所になるが、**箱を新たに決める必要はなかった**
   （既にある 20px の正方形をそのまま使う）ので決定 6 の本文は書き換えない。
   **20px でも灰色に潰れない**ことは Win32 部品 probe（`out/design/2026-09-23/filter-close-probe/`・
   16 / 16 成功・[確認記録](../quality/2026-09-23-filter-close-checks.md)）で測った。
   20px の箱の塗りは完全な画素 13・中間色 47 で、24px の箱の塗りを面積比 `(20/24)^2` で写した値の
   **1.017 倍**である。ADR 本文が「20px 未満だと灰色になる」と言っているのは**高さ 2 単位の板と
   8 単位の印**についてで、腕が 1.98 単位ある × には当てはまらなかった。
   絵は箱から 1 画素もはみ出さないので、**見えている × と押せる範囲が一致する**（旧実装は
   箱の 6px 内側から線を引いていたので絵の方が小さかった）。実機の目視は統合チェックリストの 93-1。
