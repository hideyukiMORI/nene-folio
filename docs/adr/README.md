# アーキテクチャ決定記録（ADR）

トレードオフのある判断を、**理由ごと**残す場所。正本ドキュメントが「いま何が規範か」を書くのに対し、
ADR は「なぜそう決めたか」「何を却下したか」を書く。

## 書き方

- 連番 4 桁 ＋ kebab の題名: `NNNN-short-kebab-title.md`
- `0000-template.md` を複製して書く
- 状態は `提案` / `受理` / `却下` / `置換（→ NNNN）`
- **却下した選択肢を必ず書く。** 再提案は、その理由への反論から始めること
- 受理された ADR を後から書き換えない。変えるときは新しい ADR で置き換える
- 文脈は可能な限り実データ（コード・履歴・計測・コンパイラの出力）で裏を取る

## 一覧

| ADR | 題名 | 状態 |
| --- | --- | --- |
| [0001](0001-strictness-is-mechanically-enforced.md) | 厳格さは機械で強制する | 受理 |
| [0002](0002-plain-win32-no-ui-library.md) | 素の Win32 で作り、UI ライブラリを入れない。ドロワーは自前描画にする | 受理 |
| [0003](0003-c23-clang-cl-foundation-and-measured-limits.md) | C23 / clang-cl の検査基盤と実測できた限界を固定する | 受理 |
| [0004](0004-first-drawer-slice.md) | 最初の縦切りで中核を生み、リンカ段と分岐の検査を結線する | 受理 |
| [0005](0005-rigid-design-and-os-theme.md) | 画面の見た目は「案2 堅」を採り、テーマは OS のアプリのモードに従う | 受理 |
| [0006](0006-edit-mode-and-save-path.md) | 編集中の本文は RichEdit だけが持ち、編集モードを抜けるときと Ctrl+S で元の形に揃えて書き戻す | 受理 |
| [0007](0007-drag-reorder-and-index-write-back.md) | ドロップ先は core が決め、並び替えの意図で台帳を作り直して categories.json / index.json を書き戻す | 受理 |
| [0008](0008-cross-category-note-move.md) | 別カテゴリへの移動は md の rename が先で、両方の index.json はそれに追随し、同名は拒む | 受理 |
| [0009](0009-drawer-scroll-and-overflow-fade.md) | スクロール量は application が要求量で持ち、上限と表示座標は core が決め、UI はあふれをフェードで示す | 受理 |
| [0010](0010-category-color-via-choose-color.md) | カテゴリの色は右クリックで OS の色の選択から選び、expanded と同じ経路で categories.json へ書き戻す | 受理 |
| [0011](0011-frameless-client-min-size-and-breadcrumb-ellipsis.md) | 枠なし窓の client は自分で決め、最小サイズを持ち、パンくずはノート名 → カテゴリ名の順に省略する | 受理 |
| [0012](0012-note-history-before-save.md) | md を書き戻す直前の本文を data/.history に連番で 5 版まで残し、残せなければ保存しない | 受理 |
| [0013](0013-index-focus-vim-keys-and-persistent-mode.md) | フォーカスは索引と本文の 2 区画、vim の鍵で選択を動かし、モードを保ったまま保存して次のノートを開く | 受理 |
| [0014](0014-frameless-window-answers-ncactivate.md) | 枠なし窓は WM_NCACTIVATE と WM_NCPAINT も自分で答え、非アクティブ化で OS に枠を描かせない | 受理 |
| [0015](0015-cursor-stops-on-collapsed-categories.md) | 索引のカーソルは、ノート行と「見えるノート行を持たないカテゴリ行」に止まる | 受理 |
| [0016](0016-command-catalog-and-shared-execution.md) | Ex・操作パレット・既存ショートカットは共通の操作へ変換し、一つの経路で実行する | 受理 |
