# ADR 0011 — 枠なし窓の client は自分で決め、最小サイズを持ち、パンくずはノート名 → カテゴリ名の順に省略する

- 状態: 受理
- 日付: 2026-09-10
- Issue: #22
- 影響する規則: ARC-002 / ARC-011 / C-002 / C-017 / QLT-013

## 文脈

施主が 2026-09-10 に実機（Windows 11・96 DPI）で 3 点を見つけた。(1) 窓の 4 辺に太い白い帯が出ることがある。
(2) 大きさを変えると描き直されず表示が崩れる。(3) 小さくしすぎると右ペインの頭の札とパンくずが崩れる。

コードで確かめた原因:

- (1) `folio_window.c` の `window_procedure` は `GWLP_USERDATA` が結ばれる前のメッセージをすべて `DefWindowProcW` に流す。
  `WM_NCCALCSIZE`（`wParam == TRUE`）は `WM_NCCREATE` の直後・`WM_CREATE` より前に届くので、最初の client の計算は OS の既定になり、
  `WS_THICKFRAME` の枠ぶん client が縮む。その帯を OS が既定の枠で描く。大きさが変わる操作があれば自分の処理（0 を返す）を通って
  消えるが、動かすだけ・アクティブ化の切り替えでは再計算されない。施主の観察（起動時に出ていることが多い・動かすと消えたり
  消えなかったり）と一致する
- (2) `WM_SIZE` は `arrange`（子の移動）だけで `InvalidateRect` を呼ばず、クラスに `CS_HREDRAW | CS_VREDRAW` も無い。
  右ペインの頭（パンくず・札）は幅に依存する位置に描くので古い描画が残る
- (3) `WM_GETMINMAXINFO` を扱っておらず、`caption_rect` は幅から札のぶんを引くだけで、はみ出しの規則が無い

SPECIFICATION 第 2 節は「窓の初期サイズとドロワー幅 | 960×640・ドロワー 240 | 実機で見て決める」としており、最小サイズと
はみ出しの規則は未定だった。

## 決定

**`WM_NCCALCSIZE` は窓の構造体を要らないので結び付けの前でも自分で答え、`WM_SIZE` で描き直しを頼み、`WM_GETMINMAXINFO` で
最小サイズを持つ。パンくずは札 → 番号 → ノート名 → カテゴリ名の順に幅を確保し、足りなければノート名、次にカテゴリ名の末尾を省略する。**

具体的には:

1. **client の計算。** `window_procedure` は `WM_NCCALCSIZE` を `WM_CREATE` と同じく `self` の有無に関わらず処理する
   （`wParam == TRUE` なら 0 を返し、client を窓の矩形全体にする）。最大化中は枠ぶん画面からはみ出るので、
   `wParam == TRUE` かつ最大化（`IsZoomed`）なら `NCCALCSIZE_PARAMS.rgrc[0]` をモニタの作業領域（`MonitorFromWindow` +
   `GetMonitorInfoW`）に収める
2. **描き直し。** `WM_SIZE` で `arrange` のあとに主窓を `InvalidateRect`（子は `MoveWindow` の再描画で足りる）。
   クラスの `CS_HREDRAW | CS_VREDRAW` は使わない（描き直しの理由を 1 か所に見せる）
3. **最小サイズ。** `WM_GETMINMAXINFO` の `ptMinTrackSize` を 96 DPI で **幅 560・高さ 360**（DPI で拡大）にする。
   幅はドロワー 240 + パンくずの字下げ + 番号 + 札 2 つ + 余白が収まる値。SPECIFICATION 第 2 節に載せる
4. **パンくずの省略。** 右ペインの頭の幅を、右から札 2 つ（縮めない）→ 左から番号（縮めない）→ 残りを「カテゴリ / ノート」に割り当てる。
   残りに収まらなければ、まず**ノート名**を `DT_END_ELLIPSIS` で省略する（ノート名の最小幅は 96 DPI で 48 px）。それでも
   収まらなければ**カテゴリ名**も同じく省略する。番号・「/」・札は最後まで残す。幅の測定は `DrawTextW` の `DT_CALCRECT`。
   これは描画の割り当てであり判断を含まないので UI に置く（ARC-011。判断が要ると分かったら core へ）
5. **ドロワー側。** 変更なし。面は描くたびに client の大きさで作るので、`WM_SIZE` の `MoveWindow` で描き直る

## 強制

- ui が adapters を呼ばないこと、OS ライブラリが増えないことは ARC-002 / ARC-003 が **active**（user32 の関数だけを使う）
- 実機の確認は `docs/quality/gate-proofs.md` 第 5-j 節（QLT-013）: 起動直後に白い帯が無い・最大化で画面に収まる・リサイズ直後に残像が無い・
  最小サイズで止まる・幅を狭めたときにノート名 → カテゴリ名の順に省略され札と番号が残る（写真）・96 DPI と 120 DPI
- 施主の実機で白い帯が再現しないこと（施主の確認）

## 結果

得る: 枠なし窓の client が起動直後から自分の計算になり、白い帯の発生条件そのものが無くなる。大きさを変えても表示が追随し、
小さくしても頭が崩れない。

失う・残る:

- 最小サイズより小さい窓は作れない。狭い画面（作業領域が 560×360 未満）では最小サイズが作業領域を超える
- 省略記号はノート名とカテゴリ名だけ。番号と札が収まらない幅は最小サイズで防ぐ
- 最大化の扱いは `IsZoomed` と作業領域だけで、複数モニタでの作業領域の違い・スナップの半画面は実機で見ていない
- `WM_NCACTIVATE` / `WM_NCPAINT` は触らない（client が全体なら OS は枠を描かない）。触る必要が出たら事故駆動

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 作成直後に `SetWindowPos(SWP_FRAMECHANGED)` で再計算させる | 直るが、最初の計算が既定に流れる構造は残る。`WM_NCCALCSIZE` を先に処理すれば構造ごと直る |
| クラスに `CS_HREDRAW \| CS_VREDRAW` | 描き直しの理由が見えず、`WM_ERASEBKGND` を 1 で返している設計と噛み合わない。`WM_SIZE` の 1 行に置く |
| 最小サイズを持たずに札を隠す | 「動かない部品は描かない」（ADR 0005）とは違い、動く部品を隠すと操作を失う。最小サイズで防ぐ |
| パンくずの真ん中を省略する（`DT_PATH_ELLIPSIS` 風） | ドロワーの行は末尾を省略しており、流儀を揃える |
| カテゴリ名を先に省略する | いま見ているノートの名前の方が情報として重い |
| 省略の割り当てを core に置く | 画素の幅はフォントと DPI に依存し、core は測れない。割り当ては判断を含まない描画の都合 |

## 参考

- [ADR 0002](0002-plain-win32-no-ui-library.md)（枠なし窓）/ [ADR 0005](0005-rigid-design-and-os-theme.md)（右ペインの頭の寸法）
- [WM_NCCALCSIZE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-nccalcsize) /
  [WM_GETMINMAXINFO](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-getminmaxinfo)
