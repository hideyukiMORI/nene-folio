# #93 の確認記録 — 絞り込み欄の × も `icon_paint_fill(CLOSE)` で描く

2026-09-23。ADR 0033 の補正 8 が「× を描く経路が 2 つ残る」と書いたものを 1 つにした。
**実際に走らせた**もの（と、走らせていないもの）をこの 1 枚に集める。
probe は `out/design/2026-09-23/filter-close-probe/`。

> **結果が空欄・「測っていない」と書いてあるものは、まだ見ていないという意味である。**
> 見ていないものを「確認済み」と書かない（ADR 0001 と同じ規律）。

## 1. 何が変わったか

| 場所 | 変更前 | 変更後 | 箱 |
| --- | --- | --- | --- |
| 絞り込み欄の × | GDI の線 2 本（`CreatePen(PS_SOLID, scale(1, dpi))` ＋ `base_filter_clear_inset`(6) の内側から） | `icon_paint_fill(CLOSE, header_text)` の面 | `filter_clear_rect`（20px・**変えない**） |

`base_filter_clear_inset` は要らなくなったので消した（面のパスは 24 の viewBox を箱へ 20/24 で写す）。
当たり判定 `filter_clear_pressed` と `EM_SETMARGINS(EC_RIGHTMARGIN, 20)` は 1 文字も変えていない。
**× を描く経路はこれで `icon_paint.c` の 1 本になった**（頭の `draw_close` と同じ関数・同じ点の表・ARC-001 / ARC-012）。

## 2. Win32 部品 probe（`filter_close_probe.exe` — **16 / 16 成功**）

production の `src/ui/win32/icon_paint.c` を**そのまま一緒にコンパイル**して呼んでいる。
画素を読むので窓は画面の上に出した（`WS_EX_TOOLWINDOW` + `HWND_TOPMOST` + `SWP_NOACTIVATE`）。

| # | 測ったこと | 結果 |
| --- | --- | --- |
| 1-1 | `GdiplusStartup` が `Ok` | PASS（status=0） |
| 2-1〜2-3 | 20 / 30 / 40px（96 / 144 / 192 DPI）で**中間色と solid の画素の両方**を持つ | 3 / 3 PASS |
| 3-1 | 地 `#1A0011` → 字 `#9C8593` で**黒や白の縁が 1 画素も出ない**（20px の全画素走査） | PASS |
| 4-1 | 塗りが `filter_clear_rect` の箱から 1 画素もはみ出さない | PASS（inside=60 / outside=0） |
| 5-0〜5-2 | 実物の `EDIT` の DC に production と同じ順で描けて、箱の中だけに画素が出る | 3 / 3 PASS |
| 6-1 | `EM_SETMARGINS` の右余白が **20px のまま** | PASS |
| 6-2 / 6-3 | × が本文の矩形（右端 216）の外・当たり判定の中心は内・左端は外 | 2 / 2 PASS |
| 7-1〜7-3 | 押下で呼ぶ `SetWindowTextW(L"")` が `EN_CHANGE` を **1 件**出して欄が空になる | 3 / 3 PASS |
| 8-1 | 20px の塗りの量が 24px の箱の面積比 `(20/24)^2` と ±10% で釣り合う | PASS（ratio=1.017） |

### 20px でも灰色に潰れない（この Issue の見込みの答え）

ADR 0033 の実測は「**高さ 2 単位の板と 8 単位の印**は箱が 20px 未満だと塗りが 50〜75% の灰色になる」だった。
× は板ではないので 20px でも濃さが出る、というのが Issue の見込みで、実測はそのとおりだった。

```
2-1 close at 20px  touched=60  solid=13  partial=47  ink=33326
2-3 close at 24px（節 8 の比較）           ink=47176 → 面積比で 32761（実測は 1.017 倍）
```

**20px でも完全な塗りの画素が 13 ある。** 頭の 24px の × と同じ濃さの絵である。

### 見えている絵と押せる範囲が一致する

旧実装は箱の 6px 内側から線を引いていたので、絵は箱より小さく当たり判定だけが広かった。
面のパスは 24 の viewBox を箱へ写すので、`filter_clear_rect`（`(216,4)-(236,24)`）の
**外には 1 画素も出ず、中には 60 画素出る**（4-1 / 5-2）。当たり判定は同じ矩形のままである。

## 3. ゲート

| 検査 | 結果 |
| --- | --- |
| `python eng/conformance.py` | 0 件 |
| `clang-format --dry-run --Werror` | 0 件 |
| `cmake --build build`（`/W4 /WX` ＋ clang-tidy） | exit 0 |
| `ctest` | 2 / 2 |
| `pwsh -NoProfile -File ./eng/check.ps1` | **この記録を書いた時点の結果は #93 の PR に記す** |

閾値・除外・重大度は 1 つも触っていない。ui は `eng/coverage-policy.json` の測定対象外なので分岐網羅は動かない。
`architecture.json` / `CMakeLists.txt` / `conformance-rules.json` は変えていない
（`icon_paint.c` と `gdiplus` は #88 で既に正典経路に入っている）。

## 4. 測っていないもの（限界）

1. **描いた絵そのもの。** この環境では画面取得が写らないので画素の数だけを測った。
   実機の目視は[統合チェックリスト](2026-09-22-visual-checklist.md)の **93-1**。
2. 本物の `paint_filter_clear`（`static` なので probe から呼べない）。呼ぶ順と箱の式・色を
   probe 側へ字面どおり写して測っている。
3. 実際の `WM_PAINT` の総時間（GDI+ の 1 描画は設計 probe で 0.022ms）。
4. `WM_DPICHANGED` を実際に起こしたときの見え方（倍率は毎回 `bounds` から出すので手当ては要らない）。
