# #91 の確認記録 — 主窓が `WM_NCDESTROY` を自分で受ける

2026-09-23。ADR 0033 の批評で見つかった既存の欠陥を直した。
**実際に走らせた**もの（と、走らせていないもの）をこの 1 枚に集める。
probe は `out/design/2026-09-23/ncdestroy-probe/`。

> **結果が空欄・「測っていない」と書いてあるものは、まだ見ていないという意味である。**
> 見ていないものを「確認済み」と書かない（ADR 0001 と同じ規律）。

## 1. 何が変わったか

| | 直す前 | 直した後 |
| --- | --- | --- |
| `self->handle` を消す場所 | `window_destroyed`（`WM_DESTROY`） | `window_finalized`（`WM_NCDESTROY`）**の 1 か所だけ** |
| `WM_NCDESTROY` の扱い | `window_procedure` に case が無く、`on_message` → `on_input_message` の `default` へ落ちる | `window_procedure` の**先出しの switch**（窓の構造体を要らない群）で受ける |
| 既定処理に渡す HWND | `self->handle`（＝ `nullptr`） | **引数の `window`**（有効な HWND） |
| `GWLP_USERDATA` | 外していなかった（窓と一緒に消える） | 既定処理の**あと**で明示的に 0 にする |

`WM_NCACTIVATE` / `WM_NCPAINT` と同じく「窓の構造体を要らないので先に答える」群に入れた（ADR 0014）。
`WM_NCDESTROY` は開いた集合の 1 値なので `default` はそのまま残る（C-017）。

## 2. Win32 部品 probe（`ncdestroy_probe.exe` — **14 / 14 成功**）

production の `window_procedure` は `static` なので probe からは呼べない。
**直す前の形**と**直した後の形**を 2 つの窓クラスへ写し、同じ手順（主窓 ＋ 子窓 2 つ →
`DestroyWindow`）で壊して比べた。写したのは手続きの骨だけで、判断は production の字面どおりである。

| # | 測ったこと | 結果 |
| --- | --- | --- |
| 0-1 | `GdiplusStartup` を `CreateWindowExW` の前に呼ぶ（ADR 0033 の決定 4） | PASS |
| 1-1 | **直す前**は `WM_NCDESTROY` の既定処理に `nullptr` が渡っていた | PASS（**欠陥の再現**） |
| 1-2 | その `DefWindowProcW(nullptr, …)` が `ERROR_INVALID_WINDOW_HANDLE`(1400) を立てる | PASS |
| 1-3 | `WM_DESTROY` と `WM_NCDESTROY` のあいだに主窓へ届く他のメッセージ | PASS（**0 件**） |
| 1-4 | 破棄の順（主窓 `WM_DESTROY` → 子の `WM_DESTROY` / `WM_NCDESTROY` → 主窓 `WM_NCDESTROY`） | PASS |
| 1-5 | `PostQuitMessage` の `WM_QUIT` が 1 件 | PASS |
| 2-1 | **直した後**は `WM_NCDESTROY` の既定処理に渡る HWND が**有効**（`IsWindow` が TRUE） | PASS |
| 2-2 | 既定処理の**あと**で `GWLP_USERDATA` が外れる（0 になる） | PASS |
| 2-3 | `self->handle` を消すのは `WM_NCDESTROY` の 1 か所だけ | PASS |
| 2-4 | **破棄の順が直す前と 1 つも変わらない**（7 件の並びが同一） | PASS |
| 2-5 | `WM_QUIT` が 1 件（変わらない） | PASS |
| 2-6 | 二重 `DestroyWindow` の守り（`handle` が `nullptr` なら壊さない）が効く | PASS |
| 3-1 | GDI+ の `Shutdown` は主窓の `WM_NCDESTROY` と `DestroyWindow` の戻りより**後** | PASS |
| 3-2 | `Shutdown` の後は `Gdip*` が `GdiplusNotInitialized`(18) | PASS |

### 欠陥はここにあった

直す前の形で `WM_NCDESTROY` に入った時点の `self->handle` は既に `nullptr` で、
`DefWindowProcW(nullptr, WM_NCDESTROY, …)` は `ERROR_INVALID_WINDOW_HANDLE`(1400) を立てて
何もしない。**窓の最後の既定の後始末が走らないままだった**（1-1 / 1-2）。

### 順序は 1 つも動いていない

直す前も後も並びは同じ 7 件だった。

```
main:DESTROY child:DESTROY child:DESTROY child:NCDESTROY child:NCDESTROY main:NCDESTROY destroy:returned
```

**主窓の `WM_NCDESTROY` が最後**で、`DestroyWindow` はそのあとに返る。
`folio_window_destroy` が `DestroyWindow` の後に呼ぶ `GdiplusShutdown` の位置は変わらない
（ADR 0033 の決定 4）。二重破棄の守り（`folio_window_destroy` が `handle != nullptr` のときだけ
`DestroyWindow` する）も、`DestroyWindow` が同期で `WM_NCDESTROY` まで回すので効き方は同じである（2-6）。

## 3. ゲート

| 検査 | 結果 |
| --- | --- |
| `python eng/conformance.py` | 0 件 |
| `clang-format --dry-run --Werror` | 0 件 |
| `cmake --build build`（`/W4 /WX` ＋ clang-tidy） | exit 0 |
| `ctest` | 2 / 2 |
| `pwsh -NoProfile -File ./eng/check.ps1` | **この記録を書いた時点の結果は #91 の PR に記す** |

閾値・除外・重大度は 1 つも触っていない。ui は `eng/coverage-policy.json` の測定対象外なので分岐網羅は動かない。

## 4. 測っていないもの（限界）

1. **production の `window_procedure` そのもの**（`static` なので probe から呼べない）。
   形を写して測っている。写しが字面と食い違えば probe は気付かない。
2. **production の子を全部持つ窓での `WM_DESTROY`〜`WM_NCDESTROY` 間のメッセージ。**
   probe の子は素の 2 つだけで、そこでは 0 件だった。production の主窓はドロワー・RichEdit・
   入力面 4 つ・command layer を持つので、同じとは測っていない。
   `self->handle` が `WM_NCDESTROY` まで生きているので、仮に届いても既定処理は有効な HWND を受ける。
3. **実機での目視。** この修正は目に見える変化を持たない（統合チェックリストに項目を足していない）。
4. 直す前の欠陥が実際に何を壊していたか。`WM_NCDESTROY` の既定処理が省かれても
   USER32 は窓を解放するので、観測できる害は見つけていない（Issue も「いまは害が見えない」と書いている）。
