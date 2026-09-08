# ADR 0002 — 素の Win32 で作り、UI ライブラリを入れない。ドロワーは自前描画にする

- 状態: 受理
- 日付: 2026-09-09
- Issue: #1
- 影響する規則: ARC-003 / ARC-005 / ARC-011 / C-013 / C-014 / C-017 / QLT-011

## 文脈

NeNe Folio は枠の無いデスクトップの Markdown 帳面である（SPECIFICATION.md）。左のドロワーはカテゴリごとの索引で、
アコーディオンの展開・ドラッグでの並び替え・右クリックの色変更・スクロールバー無しのスクロールを持つ。右は Markdown の
ビューと編集で、日本語入力（IME）が使えなければ成立しない。

候補は 3 つあった。素の Win32（C の GUI として最も直接）、GTK4（C の代表的ツールキット・X11 でも動く）、
SDL2 ＋ 即時モード GUI（枠なしは容易）。施主は「Windows のみでも X11 でもよい」「できれば C で」と指定した（2026-09-08）。

## 決定

**素の Win32 API だけで作る。ドロワーは共通コントロールを使わず自前描画の 1 ウィンドウクラスにし、ビュー／編集は RichEdit に委ねる。**

- 枠なしは `WS_POPUP` ＋ `WM_NCCALCSIZE` を 0 で返す ＋ `WM_NCHITTEST` の自前判定。DWM の影とスナップは残す
- ドロワーは自前描画（`WM_PAINT` にメモリ DC・`WM_MOUSEWHEEL`・`SetCapture` によるドラッグ・`WM_RBUTTONUP` のメニュー）。
  ListView / TreeView は使わない。理由は下の却下表
- レイアウト計算（行の y 座標・ヒットテスト・ドロップ先・スクロール上限・展開状態）は **core の純関数**に置き、UI は描くだけにする
- ビュー／編集は RichEdit（`Msftedit.dll` の `RICHEDIT50W`）。ビューは core が md → RTF に変換した文字列を `EM_STREAMIN` で流し、
  編集は同じコントロールをプレーンテキストで使う。IME・複数行編集・書式表示を OS に委ねる
- 内部文字列は UTF-8。Win32 の `W` 系 API へ渡す境界でだけ UTF-16 に変換する（C-014）
- 実行時依存は 0。C ランタイムは `/MT` で静的に結ぶ（R1 で import が KERNEL32 だけであることを実測済み）

## 強制

- ARC-002 / ARC-003: `eng/architecture.json` の `platformLibraries` に無い OS ライブラリは configure で落ちる（planned → 製品 target が生まれてから active）
- 実行時依存 0: `runtimeDependencies` が空でないと `eng/conformance.py` が QLT-011 で落とす
- RichEdit と共通コントロールの区別は字句では見ない。ドロワーに `SysListView32` / `SysTreeView32` を作らないことはレビュー事項（planned）

## 結果

- 得る: 依存が OS だけで閉じ、公開物は exe ＋ `data/` になる。Phase 4 の同梱一覧が短い。IME・書式表示を自前で書かない
- 失う: Windows 専用になる。ドロワーの描画・ヒットテスト・ドラッグを自前で持つ（core にテストを置くことで相殺する）
- md → RTF はサブセット（見出し・強調・コード・リスト・リンク・引用）に限る。CommonMark 全部は目標にしない

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| GTK4（Linux/X11 を主、Windows は MSYS2） | Windows 配布に 30MB 前後の DLL 群が付き、Phase 4 の「固定の同梱一覧」と `/MT` の静的リンクが成り立たない。GObject のマクロと動的型は C-006（汎用データバッグ）と衝突する |
| 素の Xlib / XCB | 複数行テキスト編集と XIM / IBus の IME 連携を手で書くことになり、本体より大きくなる。日本語の帳面としては致命的 |
| SDL2 ＋ Nuklear / microui | 即時モード GUI の複数行テキスト編集と IME が弱い。枠なし以外の利点が無い |
| ListView（グループ折り畳み＋カスタムドロー） | 折り畳み（`LVGS_COLLAPSIBLE`）・グループをまたぐドラッグ・スクロールバー非表示を同時に載せると、コントロールとの戦いが本体を超える。`ShowScrollBar(FALSE)` はレイアウトで復活する |
| WebView2 で Markdown を描く | 実行時依存（Evergreen ランタイム）が増え、ARC-003 の「依存は OS だけ」と QLT-011 に反する |
| cmark を vendoring して AST 経由で描く | ビューが RichEdit なので RTF を直接出す方が短い。サブセットで足りる。全文法が要る計測が出たら再提案 |

## 参考

- NeNe Loupe ADR 0002（素の Win32・UI ライブラリ無し）
