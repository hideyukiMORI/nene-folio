# ADR 0014 — 枠なし窓は WM_NCACTIVATE と WM_NCPAINT も自分で答え、非アクティブ化で OS に枠を描かせない

- 状態: 受理（ADR 0011 の「`WM_NCACTIVATE` / `WM_NCPAINT` は触らない」を置き換える）
- 日付: 2026-09-11
- Issue: #22（再開）
- 影響する規則: ARC-002 / C-017 / QLT-013

## 文脈

ADR 0011 は `WM_NCCALCSIZE` の TRUE / FALSE とも 0 を返して client を窓の矩形全体にし、「client が全体なら OS は枠を描かない」として
`WM_NCACTIVATE` / `WM_NCPAINT` を触らなかった。施主の実機（120 DPI・3840×2160 の 125%）でその版を放置したところ、白い帯が再発した
（2026-09-11）。索引側には出ず右ペイン側にだけ出た。

触らずに測った結果（調査リナ・2026-09-11 00:07〜00:14）:

- 窓の矩形・client・`DWMWA_EXTENDED_FRAME_BOUNDS` は 4 辺とも一致（差 0）。`WM_NCCALCSIZE` の修正は効いたまま。角丸と縁の色（`#24272C`）も生きている
- 帯は窓の矩形の**内側** 7 px（`#FFFFFF` 1 px + `#F4F7FC` 6 px + `#A0A0A0` 1 px）で、色は `GetSysColor` の `COLOR_3DHIGHLIGHT` / `COLOR_INACTIVEBORDER` /
  `COLOR_BTNSHADOW` と完全に一致する。つまり **USER32 の既定のサイズ変更の縁の描画**
- 帯は x=500（ドロワー子窓の右端）から右にだけ出る。子窓が上に描く所だけ消えて見える。右ペイン固有ではない
- 放置中の OS のイベント（電源・表示・ロック・テーマ）は 0 件。変わったのは前面の窓だけ
- 同じ形の窓を自分で作って再現: 非アクティブにした瞬間 `WM_NCACTIVATE(wParam=0)` が届き、その場で帯が出る。`WM_NCPAINT` も
  `WM_ERASEBKGND` も `WM_PAINT` も届かないので client は無効化されず、**次に大きさが変わるまで帯が残る**。アクティブに戻すと
  色が `COLOR_ACTIVEBORDER`（`#B4B4B4`）に変わるだけで消えない（施主の「動かすと消えたり消えなかったり」と一致）
- 切り分け: `WM_NCACTIVATE` を TRUE で返すだけで帯は出ない（必要十分）。`WM_NCPAINT` を 0 で返すだけでは直らない。どちらでも角丸と縁の色は生きている

`DefWindowProcW` は `WM_NCCALCSIZE` の結果ではなく **`WS_THICKFRAME` の有無**で枠を描く。ADR 0011 の前提はここが誤りだった。

## 決定

**`window_procedure` の、窓の構造体を要らない先出しの処理（`WM_CREATE` / `WM_NCCALCSIZE` / `WM_GETMINMAXINFO`）に
`WM_NCACTIVATE` → TRUE と `WM_NCPAINT` → 0 を足し、既定処理に渡さない。**

- `WM_NCACTIVATE` に TRUE を返すのは「既定の処理を続けてよい」の意味で、アクティブ化そのものは止まらない。題字帯を持たない窓なので、
  失うものは無い
- `WM_NCPAINT` は `SetWindowPos(SWP_FRAMECHANGED)` / `RedrawWindow(RDW_FRAME)` の経路から届きうるので、同じ理由で 0 を返す
- ADR 0011 の決定 1〜5 はそのまま。「失う・残る」の「`WM_NCACTIVATE` / `WM_NCPAINT` は触らない」だけをこの ADR が置き換える

## 強制

- 実機の証拠（QLT-013）に **「窓を非アクティブにした直後に、窓の矩形の外周 8 px が地の色のまま（`#F4F7FC` / `#B4B4B4` / `#FFFFFF` が 0 px）」**
  を足す（`docs/quality/gate-proofs.md` 第 5-j 節の追補）。起動直後の差 0 だけでは今回の再発を捕まえられなかった
- 窓を PID で選ぶ（`FindWindowW` は同じ題字の窓が 2 つあるとどちらを返すか分からない。調査で実際に取り違えの危険があった）
- 施主の実機で放置後も帯が出ないこと（施主の確認）

## 結果

得る: 非アクティブ化・アクティブ化で OS が枠を描く経路が無くなり、帯の発生条件が消える。角丸と DWM の縁の色は変わらない。

失う・残る:

- アクティブ／非アクティブの見た目の差は完全に無い（いまも実質無い）
- `WS_THICKFRAME` は残すので、OS は「8 px の枠がある窓」として扱い続ける（`WINDOWINFO.cxWindowBorders = 8`）。スナップと最大化の既定動作を保つための選択
- 起動後の OS のライト／ダークの切り替え（`WM_SETTINGCHANGE` / `WM_THEMECHANGED`）には依然として追随しない（ADR 0005 のまま・別件）
- `build/NeNeFolio.exe` と `out/hide/NeNeFolio.exe` が同じ位置（`CW_USEDEFAULT`）に重なり、施主が別の窓を見る事故が起きうる（別件）

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `WM_NCACTIVATE` で client を無効化して既定処理へ渡す | 帯が一瞬出てから塗り潰される。活性の切り替えごとに全面の描き直しとちらつき。原因が残る |
| `WS_THICKFRAME` を外して `WM_NCHITTEST` だけでリサイズさせる | スナップ・最大化・`WM_GETMINMAXINFO`・DWM の影の扱いが変わり、ADR 0002 の枠なし窓の作りを丸ごと見直すことになる |
| `WM_NCPAINT` だけを止める | 実測で直らない。`DefWindowProcW(WM_NCACTIVATE)` は `WM_NCPAINT` を経由せずに描く |
| `DWMWA_NCRENDERING_POLICY = DISABLED` | DWM の描画ではないので効かない。角丸と縁の色を失うだけ |

## 2026-09-22 の補正（#91・決定本文は書き換えない）

1. **先出しの群に `WM_NCDESTROY` を足し、`self->handle` を消す場所をそこ 1 か所に揃えた。**
   この ADR の決定は `WM_NCACTIVATE` / `WM_NCPAINT` を「窓の構造体を要らないので先に答える」群へ
   入れたが、**窓の最後のメッセージ `WM_NCDESTROY` はその群に入っていなかった**。
   `window_destroyed`（`WM_DESTROY`）が `self->handle = nullptr` にした後なので、
   `WM_NCDESTROY` は `on_message` → `on_input_message` の `default` に落ちて
   **`DefWindowProcW(nullptr, …)`** を呼び、`ERROR_INVALID_WINDOW_HANDLE`(1400) で何もしていなかった
   （ADR 0033 の決定 10 が「別 Issue」として残したもの・Issue #91）。
   直した形は `window_procedure` の先出しの switch に `WM_NCDESTROY` を足し、`window_finalized` が
   **(a) 引数の HWND で `DefWindowProcW` を呼び → (b) `self->handle` を `nullptr` にし →
   (c) `GWLP_USERDATA` を 0 にする**。`window_destroyed` は `self->handle` を触らなくなり、
   **HWND の写しは窓が消えるその瞬間まで有効**になる（`WM_DESTROY` の後に届くメッセージが
   既定処理へ `nullptr` を渡す経路そのものが消える）。
   `WM_NCDESTROY` は Win32 の開いた集合の 1 値なので `default` はそのまま残る（C-017）。
   Win32 部品 probe（`out/design/2026-09-23/ncdestroy-probe/`・14 / 14 成功・
   [確認記録](../quality/2026-09-22-ncdestroy-checks.md)）で、直す前の `nullptr` と 1400 を再現し、
   直した後は HWND が有効・`GWLP_USERDATA` が外れる・**破棄の順（主窓の `WM_NCDESTROY` が最後）が
   1 つも変わらない**・GDI+ の `Shutdown`（`folio_window_destroy`・ADR 0033 の決定 4）の位置が
   変わらないことを測った。利用者に見える振る舞いの変化は無い。

## 参考

- [ADR 0011](0011-frameless-client-min-size-and-breadcrumb-ellipsis.md)（置き換える行）/ [ADR 0002](0002-plain-win32-no-ui-library.md)
- [WM_NCACTIVATE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-ncactivate)
