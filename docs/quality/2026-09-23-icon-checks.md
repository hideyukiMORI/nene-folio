# #88 の確認記録 — 閉じる・設定・折畳・選択の印を GDI+ の面のパスで描く

2026-09-23。ADR 0033 の検証節に対して**実際に走らせた**もの（と、走らせていないもの）をこの 1 枚に集める。
probe は `out/design/2026-09-23/icon-ui-probe/`（設計 probe は `out/design/2026-09-23/icon-probe/`）。

> **結果が空欄・「測っていない」と書いてあるものは、まだ見ていないという意味である。**
> 見ていないものを「確認済み」と書かない（ADR 0001 と同じ規律）。

## 1. 何が変わったか（字形から絵へ）

| 場所 | 変更前 | 変更後 | 箱 |
| --- | --- | --- | --- |
| 頭の閉じる × | GDI の線 2 本（`CreatePen(PS_SOLID, scale(1, dpi))`） | `ICON_PAINT_CLOSE` の面 | `close_rect`（24px・変えない） |
| 頭の設定の歯車 | GDI の線（小円 ＋ 8 本の放射線） | `ICON_PAINT_SETTINGS` の面 | `settings_rect`（24px・変えない） |
| 設定画面の選択の印 | `current_text` の小円（直径 8px） | `ICON_PAINT_SELECTION` の面・色は `chip_background` | 行の縦中央の 24px 正方形 `[row.left + 4, +28]` |
| ドロワーの折畳 − / + | **等幅フォントの字形**（`UI_TEXT_GLYPH_MINUS` / `_PLUS`） | `ICON_PAINT_FOLD_COLLAPSE` / `_EXPAND` の面 | カテゴリ行の縦中央の 24px 正方形 `[width - 16 - 24, width - 16]` |

`ui_text` から `UI_TEXT_GLYPH_MINUS` / `UI_TEXT_GLYPH_PLUS` を消した（**単独で描く字形は 1 つも残らない**）。
当たり判定の矩形（`close_rect` / `settings_rect` / カテゴリ行の折畳の当たり）は 1 つも変えていない。

## 2. Win32 部品 probe（`icon_ui_probe.exe` — **43 / 43 成功**）

production の `src/ui/win32/icon_paint.c` を**そのまま一緒にコンパイル**して呼んでいるので、
写しも失敗の扱いも出荷するコードそのものを測っている。画素を読むので窓は画面の上に出した
（`WS_EX_TOOLWINDOW` + `HWND_TOPMOST` + `SWP_NOACTIVATE`）。

| # | 測ったこと | 結果 |
| --- | --- | --- |
| 0-1 | host 窓が画面の上にある | PASS |
| 1-1 | `GdiplusStartup` が `Ok`（自前の宣言・`gdiplus.lib`・型定義 0 のヘッダ） | PASS（status=0） |
| 2-1〜2-5 | 5 形 × 箱 24 / 36 / 48px が**中間色**と完全な塗りの両方を持つ | 15 / 15 PASS |
| 3-1〜3-5 | 地 `#2C001E` → 字 `#9C8593` で、**黒や白の縁が 1 画素も出ない**（全画素走査） | 5 / 5 PASS |
| 4-1 | 歯車の穴が抜ける（`FillModeAlternate`・中心 = 0） | PASS |
| 4-2〜4-4 | `+` の中心・`−`・選択の印が埋まる（`FillModeWinding`・中心 = 1000） | 3 / 3 PASS |
| **5-1** | **GDI の状態が `icon_paint_fill` の前後で不変**（9 つ） | PASS |
| 5-2 | `SetViewportOrgEx` を引き継ぐ（client 座標のまま描ける） | PASS |
| **6-1** | **DDB のメモリ DC に描けて、damage の外のパスが 1 画素も出ない** | PASS |
| 6-2 | `IntersectClipRect` を引き継ぐ（ドロワーが行を切り詰める流儀） | PASS |
| 7-1 | ドロワーの DIB のアルファ欄に `0xFF` が入る | PASS |
| 7-2 | `mix()` は RGB だけを組むのでフェード帯でアルファが 0 に戻る | PASS |
| 7-3 | `paint_surface` の**既存の 1 本の `GdiFlush`** で画素が読める | PASS |
| 8-1 / 8-2 | 選択の印の箱 `[4,28]`・ink は `row.left + 12`（**従来と同じ位置**）・札 `row.left + 32` と重ならない | PASS |
| 8-3 / 8-4 | 折畳の箱 `[width-40, width-16]`・34px の行の縦中央（top +5・中心 17.0） | PASS |
| 8-5 | `toggle_room` が 96 DPI で 30px の定数になる | PASS |
| 8-6 | 箱が 96 / 144 / 192 DPI で 24 / 36 / 48px（1 単位 = 1.0 / 1.5 / 2.0px） | 3 / 3 PASS |
| 9-1 | `GdiplusShutdown` の後の `Gdip*` は `GdiplusNotInitialized`（18）で落ちない | PASS |
| 9-2 | GDI+ が落ちている状態の `icon_paint_fill` は**何も描かない**（落ちない） | PASS |

### 5-1 の中身（ADR が「未測定」としていた 1 つ目）

選択フォント・文字色・`GetBkMode`・`GetTextCharacterExtra`・ビューポート原点・ウィンドウ原点・
`GetMapMode`・選択ブラシ・選択ペンの **9 つとも不変**。
`draw_pane` が書体と字間を当てたあとにアイコンを描き、続けて `draw_actions` / `draw_gutter` が
文字を描く現行の順のままでよい。

### 6-1 の中身（ADR が「未測定」としていた 2 つ目）

`blit_pane` と同じ形（実窓の DC から `CreateCompatibleBitmap` で damage の大きさの **DDB**・
`SetViewportOrgEx(-damage.left, -damage.top)`）で、damage の中の箱は出て、
damage の外へ置いた箱は **1 画素も出ない**。DDB の大きさそのものがクリップになるので、
`draw_close` が damage との交差を見ずに毎回描く現行の流儀を変えなくてよい。

## 3. 幾何の突き合わせ（`geometry.py` — **失敗 0 件**）

画面案 C の `d` 属性と `<rect rx>` を独立に解釈して、**production の表**と比べた。

- `close_points` 12 点・`settings_points` 32 点が**1 点も違わない**
- `round_rects[]` の 3 枚（`{6,11,12,2,1}` / `{11,6,2,12,1}` / `{8,8,8,8,2}`）が一致
- 歯車の外接は x `3.0..21.1`（中心 12.05）・y `3.0..21.2`（中心 12.10）＝ viewBox の中心から
  **+0.05 / +0.10 単位**ずれている。**画面案そのものの性質で、値は 1 つも変えていない**

幾何の面積（靴紐の公式・角丸は `wh − (4−π)r²`）と実測の塗り（15 通り）の差は
**−2.18% 〜 +0.68%**（全部 3% 以内・写し違いも FillMode の取り違えも無い）。

| 形 | 箱 24px の幾何 | 測った塗り | 差 |
| --- | ---: | ---: | ---: |
| close | 47.600 | 47.176 | −0.89% |
| settings（穴を引いた） | 183.876 | 184.884 | +0.55% |
| fold-collapse | 23.142 | 22.808 | −1.44% |
| fold-expand | 42.283 | 41.616 | −1.58% |
| selection | 60.566 | 59.248 | −2.18% |

## 4. ゲート

| 検査 | 結果 |
| --- | --- |
| `python eng/conformance.py` | 0 件（`gdiplus_flat.h` / `gdiplus_startup_input.h` / `icon_paint_kind.h` / `icon_paint.h` の型定義の数と接頭辞を含む） |
| `clang-format --dry-run --Werror` | 0 件 |
| `cmake --build build`（`/W4 /WX` ＋ clang-tidy） | exit 0。`_Nonnull` を使う `icon_paint.c` から `gdiplus_flat.h` を読んで**警告 0** |
| `ctest` | 2 / 2 |
| `pwsh -NoProfile -File ./eng/check.ps1` | **この記録を書いた時点の結果は #88 の PR に記す** |

`eng/architecture.json` の `platformLibraries.ui_win32` に `gdiplus` を足し、
`CMakeLists.txt` の `nenefolio_system_link(nenefolio_window gdiplus)` とソース一覧
（`src/ui/win32/icon_paint.c`）を揃えた（ARC-002）。
`lineTables` は `GLYPH_*` を消したあとも設定を変えずに整合する（`UI_TEXT_APP_NO_WINDOW` が列挙の最後のまま）。
閾値・除外・重大度は 1 つも触っていない。ui は `eng/coverage-policy.json` の測定対象外なので分岐網羅は動かない。

## 5. 測っていないもの（限界）

1. **`GdiplusStartup` が失敗したときに窓が作られないこと。** 偽の失敗を注入する手立てが無い
   （`gdiplus.dll` の差し替えか API のフックになる）。**測っていない。**
   コードの形（`start_gdiplus` が偽 → `folio_window_destroy` → `FOLIO_WINDOW_NOT_CREATED`）を読んだだけである。
2. **描いた絵そのもの。** この環境では画面取得が写らないので、画素の数だけを測った。
   実機の目視は[統合チェックリスト](2026-09-22-visual-checklist.md)の 88-1〜88-8。
3. 実際の `WM_PAINT` の総時間。GDI+ の部分だけを設計 probe が 0.022ms と測っている。
4. `WM_DPICHANGED` を実際に起こしたときの振る舞い（倍率は毎回 `bounds` から出す）。
5. **絞り込みの欄の `×`（`paint_filter_clear`・#86）は GDI の線のまま。** ADR 0033 の決定 6 は
   5 種だけを挙げていて、この `×` は挙げていない（箱は 20px で、`base_filter_clear_inset` の
   刻み方も頭の × とは別）。**× を描く経路が 2 つ残る**ので、設計リナの判断で別 Issue にするか、
   この単位に足すかを決めてほしい。
