# ADR 0010 — カテゴリの色は右クリックで OS の色の選択から選び、expanded と同じ経路で categories.json へ書き戻す

- 状態: 受理
- 日付: 2026-09-10
- Issue: #21
- 影響する規則: ARC-001 / ARC-002 / ARC-004 / ARC-005 / ARC-007 / ARC-009 / ARC-011 / C-002 / C-003 / C-005 / C-012 / C-014 / C-017 / QLT-009 / QLT-013

## 文脈

FR-010 は「カテゴリ行の右クリックで色を変えられる。`ChooseColor`。ノート行のメニューは初版では無し」と定める。
`categories.json` の `color`（`#RRGGBB`）は core の `rgb_color` が読み書きし、台帳の変更は「変更した新しい台帳を作り、
書き戻せたら差し替える」形（`category_ledger_toggled` / `moved`・ADR 0004 / 0007）で揃っている。カテゴリ色を使う場所は
ドロワーの行（番号・選択行の角・挿入線）と右ペインの頭のパンくず（`pane_title_view.color`）の 2 か所で、どちらも台帳から
描くたびに引く。RTF 本文はテーマの色（`rtf_palette`）だけを使い、カテゴリ色を含まない（ADR 0005）。

`ChooseColorW` は comdlg32 にあり、ui_win32 の `platformLibraries` は user32 / gdi32 / dwmapi だけである（ARC-002・active）。
現行のドロワーは右ボタンを扱っていない。

## 決定

**色の選択は OS の `ChooseColorW` に任せ、UI は `COLORREF` と `rgb_color` の変換だけをして意図 1 本を出す。
core は色だけを変えた新しい台帳を作り、application は `expanded` と同じ経路で `categories.json` を原子的に書き戻せたときだけ差し替える。**

具体的には:

1. **core。** `category_ledger_recolored(ledger, index, color, &out)` は index の色だけを変えた新しい台帳を作る（`toggled` と同じ形。
   台帳は可変にしない・ARC-005）。`rgb_color` は完全型なので値で渡す（C-012 の 4 引数に収まる）
2. **application。** 意図 `folio_state_recolor_category(state, index, color)`。index が範囲外なら `NO_SUCH_CATEGORY`、
   いまと同じ色なら書かずに READY、書き戻せなければ `STORE_FAILED`（既存の値・文言は台帳の種類を区別しない）で状態は変えない。
   選択・編集モード・スクロール量は触らない。`pane_title_view.color` は台帳から引くので自動的に追随する
3. **UI の操作。** `drawer_window` が `WM_RBUTTONUP` で core のヒットテストを引き、カテゴリ行なら `ChooseColorW` を出す。
   ノート行・頭の帯・行の外では何もしない。左ボタンを押している間（`pressed`）は右クリックを無視する（捕捉中の再入を避ける）
4. **色の選択の形。** `CC_RGBINIT | CC_FULLOPEN`（現在の色を初期値に、全開で出す）。owner は主窓（`failure_box` と同じ）。
   カスタム色 16 個は `drawer_window` が実行中だけ保持し、保存しない。キャンセルは何もしない
5. **変換は境界で。** `COLORREF` → `rgb_color` は `drawer_window` の 1 関数で行う（C-014）。`rgb_color` → `COLORREF` は既存の `to_colorref`。
   UI は色の妥当性や「同じ色か」を判断しない（ARC-011）
6. **結果の写し。** READY ならドロワーと主窓を `InvalidateRect`（本文は触らない。編集中でも本文と編集モードは保たれる）。
   それ以外は `failure_box` から 1 行
7. **comdlg32。** `eng/architecture.json` の `platformLibraries.ui_win32` に `comdlg32` を足し、`CMakeLists.txt` で
   `nenefolio_system_link(nenefolio_window comdlg32)` を結ぶ。他のモジュールが結べば configure が落ちる（ADR 0005 の advapi32 と同じ手順）

## 強制

- ui が adapters を呼ばないこと、comdlg32 が ui_win32 以外に結ばれないことは ARC-002 / ARC-003 が **active**
- `recolored` の意味は `tests/unit/ledger_tests.c`、意図の意味は `tests/unit/state_tests.c`（偽アダプタが書かれた台帳の色を記録する）が正本
- 分岐 90% は QLT-009 が **active**。`recolored` の確保経路は `tests/unit/allocation_tests.c` で失敗させる
- 色の直書きが `folio_palette.c` / `rtf_palette.c` の外に増えないことはレビュー事項（ARC-001）。`ChooseColorW` に渡す初期値は台帳の色
- 実機の確認は `docs/quality/gate-proofs.md` 第 5-i 節（QLT-013）

## 結果

得る: 色の変更が `expanded` と同じ書き戻し経路に載り、新しい保存形式も ADR 相当の判断も要らない。色の UI を自前で描かないので、
案2「堅」のデザインに新しい部品を足さない。

失う・残る:

- `ChooseColorW` の見た目は OS のもので、ダークテーマに従わない（Windows 11 でもライトのまま）。デザインからの逸脱として記録する
- カスタム色は再起動で消える
- 色の変更に Undo は無い。同じ操作でもう一度選び直す
- 色のコントラスト（地の色に対して読めるか）は検査しない。利用者の選択に任せる
- 右クリックの「メニュー」は出さない。項目が 1 つしか無いので直接ダイアログを出す。項目が増えたら（改名・削除）メニューにする判断が要る
- 実機は DPI 120 の合成入力でだけ見る。`ChooseColorW` の中は撮らない（OS の部品）

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 自前のパレット（案2 の 6 色から選ぶ）を描く | FR-010 が `ChooseColor` と定めている。任意の色を選べる方が要件に近く、部品を増やさない |
| 右クリックでメニューを出し、その中の「色を変える」でダイアログへ | 項目が 1 つでは段が 1 つ増えるだけ。増えたときに ADR で変える |
| `WM_RBUTTONDOWN` で開く | 押した時点で開くと離しが別の窓へ届く。左と同じく離したときに確定する |
| `WM_CONTEXTMENU` で受ける | 鍵（Shift+F10 / Apps）からも来るがポインタの位置が無いことがあり、行の特定に分岐が要る。初版は右ボタンだけ |
| カスタム色を `categories.json` か設定ファイルに保存する | 保存形式が増える（ARC-009）。色そのものは台帳に残るので足りる |
| 同じ色でも書き戻す | 書き込みの理由が無い。`toggled` と違い「変わらない」がありうるので、application が比べて書かない |
| ダークテーマ用に `ChooseColorW` をフック（`CCHOOKPROC`）して塗る | OS の部品の中を塗るのは壊れやすい。逸脱として記録する方を選ぶ |

## 参考

- [ADR 0004](0004-first-drawer-slice.md)（台帳の複製と `toggled`）/ [ADR 0005](0005-rigid-design-and-os-theme.md)（advapi32 の追加手順・色の正本）/
  [ADR 0007](0007-drag-reorder-and-index-write-back.md)（意図と書き戻しの形）
- [ChooseColorW](https://learn.microsoft.com/en-us/windows/win32/api/commdlg/nf-commdlg-choosecolorw) /
  [CHOOSECOLORW](https://learn.microsoft.com/en-us/windows/win32/api/commdlg/ns-commdlg-choosecolorw)
