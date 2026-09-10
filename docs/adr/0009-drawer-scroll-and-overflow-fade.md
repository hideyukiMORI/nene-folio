# ADR 0009 — スクロール量は application が要求量で持ち、上限と表示座標は core が決め、UI はあふれをフェードで示す

- 状態: 受理
- 日付: 2026-09-10
- Issue: #19
- 影響する規則: ARC-002 / ARC-003 / ARC-004 / ARC-005 / ARC-007 / ARC-011 / C-002 / C-003 / C-005 / C-012 / C-017 / CNF-002 / QLT-009 / QLT-013

## 文脈

ドロワーの配置（`drawer_layout`）は行を `top_padding` から下へ詰めて置くだけで、窓の下端を越えた行は描かれず、届く手段が無い。
FR-012 は「あふれたらホイールと ↑↓ / PgUp / PgDn でスクロールでき、**スクロールバーは表示しない**。上端・下端に短いフェードで
あふれを示す」と定め、SPECIFICATION 第 4 節は「スクロール上限は core の純関数」「スクロール量の所有者は application」と固定している。
ADR 0007 / 0008 は「スクロールが入っても `drawer_layout_drop` に渡す y をずらすだけ」と想定し、ADR 0005 は下端のフェードを
「msimg32 の `GradientFill` は許可リストに無く、自前のアルファ合成もまだ書いていない」として先送りした。

現行の UI は `drawer_window` が `WM_LBUTTONDOWN` / `WM_MOUSEMOVE` / `WM_LBUTTONUP` の生の y を core へ渡し、鍵は主窓の
`WM_KEYDOWN`（Escape）と RichEdit の `EN_MSGFILTER`（Ctrl+S）だけが受けている。ドロワーはフォーカスを取らず、編集中に索引を
触っても Ctrl+S が RichEdit に届く（ADR 0007 の決定 7 が拠って立つ性質）。描画はメモリ DC に完成させてから転送している（C-017）。

## 決定

**application が「要求したスクロール量（画素）」を持ち、core の配置が上限に丸めて表示座標へ写す。意図は有効量（丸めた量）から
数え直す。UI はホイールと鍵を画素の差分に変えて意図にし、あふれのある側だけに短いフェードを自前で混ぜる。**

具体的には:

1. **寸法。** `drawer_metrics` に `viewport_height`（ドロワーの client の高さ）と `bottom_padding`（最後の行の下に空ける余白）を足す。
   どちらも DPI に応じて UI が測る値であり、既存の 6 つと同じ性質（C-003 の完全型のまま）
2. **core の上限と表示座標。** `drawer_layout_scroll_limit(layout)` = `max(0, bottom + bottom_padding − viewport_height)`。
   `drawer_layout_scroll(layout, offset)` は `offset` を 0〜上限に丸めて配置に記憶する（`drawer_layout_select` と同じ「配置への印」）。
   以後 `drawer_layout_row` の `top`、`drawer_layout_hit` / `drawer_layout_drop` の y、`drop_target.line_y` は**表示座標**
   （内部の座標から丸めた量を引いたもの）で受け渡す。UI はこれまでどおり生の y を渡し、座標を計算しない（ARC-011）。
   `drawer_layout_overflow_above(layout)` = 丸めた量 > 0、`drawer_layout_overflow_below(layout)` = 丸めた量 < 上限。フェードの要否は core が答える
3. **application の所有。** `folio_state` がスクロール量（要求量・画素・0 以上）を持つ。`folio_state_drawer_layout` は配置を作って選択の印を
   付けたあと `drawer_layout_scroll` で要求量を適用する（状態は変えない）。意図 `folio_state_scroll_drawer(state, metrics, delta)` は
   配置を作り、**有効量 + delta** を上限で丸めて要求量にする。同じなら書かずに READY。配置が作れなければ OUT_OF_MEMORY で状態は変えない。
   選択・トグル・並び替え・移動・編集はスクロール量を触らない（見える位置へ寄せない）
4. **UI の単位変換。** ↑↓ はノート行 1 つ（`row_height`）、PgUp / PgDn は 1 画面から 1 行を引いた量（`viewport_height − row_height`）、
   ホイールは `WHEEL_DELTA` 1 刻みでノート行 3 つ（`MulDiv(delta, 3 × row_height, WHEEL_DELTA)`・端数は蓄積しない）。
   3 行は 96 DPI の寸法と同じ定数で、OS のホイール設定（`SPI_GETWHEELSCROLLLINES`）は読まない
5. **経路。** ホイールはポインタの下の窓へ: `drawer_window` は自分に届いた `WM_MOUSEWHEEL` を意図にし、主窓は自分に届いた
   `WM_MOUSEWHEEL` をポインタがドロワーの矩形にあればドロワーへ `SendMessageW` で転送する。鍵はフォーカスへ: 主窓の `WM_KEYDOWN` が
   `VK_UP` / `VK_DOWN` / `VK_PRIOR` / `VK_NEXT` を `drawer_window` の関数で渡す。**ドロワーはフォーカスを取らない**
   （編集中に索引を触っても Ctrl+S が RichEdit に届き続ける）。閲覧へ戻ったとき（`folio_state_end_edit` が READY）は主窓へ
   `SetFocus` し、Escape と鍵が効くようにする。読み取り専用の本文をクリックすれば RichEdit がフォーカスを取り、鍵は本文へ行く（最後にクリックした側）
6. **ドラッグ中のスクロール。** 捕捉中もホイールと鍵は届く。スクロールしたら最後のポインタ y（`WM_MOUSEMOVE` で覚える）で
   `drawer_layout_drop` を引き直し、挿入線を描き直す。自動スクロールは無い
7. **描画。** 面を 32 bit の DIB セクション（`CreateDIBSection`・gdi32）にし、行は頭の帯の下端から client の下端までにクリップして描く。
   そのあと `overflow_above` なら帯の直下に、`overflow_below` なら下端に、高さ 24 px（96 DPI・DPI で拡大）のフェードを、
   端で地の色 100%・内側で 0% の線形で画素ごとに混ぜる。msimg32 は結ばない。スクロールバーは出さない（`WS_VSCROLL` を付けない）
8. **失敗の写し。** 意図が READY 以外なら `failure_box` から 1 行（既存の経路）。READY なら `InvalidateRect` だけ

## 強制

- core が `windows.h` を含まないこと、ui が adapters を呼ばないこと、msimg32 を結ばないことは ARC-002 / ARC-003 が **active**
  （`eng/architecture.json` は変えない。結べば configure が落ちる）
- 新しい型は増やさない。`drawer_metrics` の追加メンバーは既存ファイル（CNF-002）
- 上限・丸め・表示座標・`overflow` の意味は `tests/unit/layout_tests.c` が正本。「有効量から数え直す」は `tests/unit/state_tests.c` が
  折り畳みで上限が縮んだ直後の 1 回で必ず動くことで固定する。UI が座標を計算していないことはレビュー事項（ARC-011）
- 分岐 90% は QLT-009 が **active**。`folio_state_scroll_drawer` の配置の確保は `tests/unit/allocation_tests.c` で失敗させる
- 実機の確認は `docs/quality/gate-proofs.md` 第 5-h 節（QLT-013）。フェードの見え方と 1 回の移動量は写真と数値で残す

## 結果

得る: ノートが増えても索引の全部に届き、ドラッグとホイールを組み合わせれば遠いカテゴリへも運べる。座標の変換が core に閉じ、
UI の `hit` / `drop` / 挿入線の経路は 1 行も変わらない。検索（FR-011）で行が減っても上限が縮むだけで済む。

失う・残る:

- **要求量は配置より長く残る。** 折り畳みで上限が縮むと表示は丸まるが要求量は残り、再展開すると元の位置へ戻る（意図した振る舞い）。
  そのあいだにホイールを 1 刻み回せば、有効量から数え直すので必ず動き、要求量は有効量 ± 差分に置き換わる
- DPI が変わってもスクロール量は画素のまま残り、位置の比率がずれる（丸めで壊れはしない。次の 1 刻みで揃う）
- 「非アクティブ ウィンドウをホバーしたときにスクロールする」を切った環境で RichEdit にフォーカスがあると、ドロワーの上のホイールが
  本文へ行く（Windows 10 / 11 の既定は on。事故駆動で `ENM_MOUSEEVENTS` を検討）
- 選択したノートの行を見える位置へ寄せない。別のカテゴリへ移したノートが画面の外にあってもそのまま
- 高精度ホイールの端数を蓄積しないので、非常に小さい刻みは切り捨てで 0 になりうる（`MulDiv` の丸めで ±30 は動く）
- フェードの高さと曲線、ホイール 1 刻みの手触りは 96 / 120 DPI でしか見ない
- 実装で足した判断（2026-09-10・PR の前に記録）: `drawer_layout_hit` は `top_padding` より上（頭の帯）を行として当てない。
  スクロールで帯の下へ潜った行は掴めない。`top_padding` を「最初の行の上の余白」と「帯の高さ」の両方に使っており、
  2 つをずらす日が来たら `drawer_metrics` に 1 つ足す。`drawer_layout_drop` には同じ検査を入れていない（帯の上は先頭の塊へ寄る）
- `CreateDIBSection` の出力引数は `void **` でしか受けられず、`WM_CREATE` の `lpCreateParams` に続く 2 か所目の Win32 の境界になった（C-006）

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| application が丸めた量を記憶する（`folio_state_drawer_layout` が状態を書く） | 配置の問い合わせが状態を変える。外から見える状態は意図でだけ変わる（ARC-004 / ARC-005）。有効量から数え直せば同じ手触りになる |
| スクロール量を行数で持つ | 部分行のスクロールができず、ホイールの手触りが荒い。画素で持ち、core が丸める |
| `drawer_layout_create` にスクロール量を渡す | 引数が 5 つになる（C-012）。`select` と同じ「作ってから印を付ける」形で足りる |
| UI が行の `top` から丸めた量を引く | ドロップ先と挿入線の座標変換が UI に散る（ARC-011）。core の表示座標 1 つに固定する |
| スクロールバーを出す | FR-012 が禁じる |
| msimg32 の `GradientFill` / `AlphaBlend` | 24 px の帯のために `platformLibraries` を増やす理由が弱い。DIB の 1 ループで足りる |
| 地の色の横線を並べてフェードにする | 文字の上で帯になる。画素ごとに混ぜる |
| ドロワーがフォーカスを取り、自分で `WM_KEYDOWN` を受ける | 編集中に索引を触ると Ctrl+S が RichEdit から離れる（ADR 0007 の決定 7 を壊す） |
| `SPI_GETWHEELSCROLLLINES` を読む | 環境の読み取りは adapters だけ（ARC-007）。ポートを 1 本増やす価値が無い。定数 3 |
| ドラッグ中の自動スクロール | 端に留めたときの速度と停止の判断が要る。ホイールと鍵で代用できるので別 Issue |
| 選択・移動のあとに見える位置へ寄せる | 検索（FR-011）で要るかもしれないが、いまは利用者の位置を勝手に動かさない |

## 参考

- [ADR 0002](0002-plain-win32-no-ui-library.md)（自前描画）/ [ADR 0005](0005-rigid-design-and-os-theme.md)（フェードの先送り）/
  [ADR 0007](0007-drag-reorder-and-index-write-back.md)（y を渡す側の想定・決定 7）
- [WM_MOUSEWHEEL](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousewheel) /
  [CreateDIBSection](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection)
