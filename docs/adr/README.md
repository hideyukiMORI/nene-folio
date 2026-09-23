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
| [0017](0017-powerline-breadcrumb.md) | パンくずをPowerline風の連続した色面で描く | 受理 |
| [0018](0018-japanese-operation-entry-points.md) | 日本語GUI・ヘルプ・閲覧からの編集開始を共通操作へ接続する | 受理 |
| [0019](0019-editor-caret-accelerators.md) | 編集本文のCtrl＋hjklをRichEdit自身の移動へ結ぶ | 受理 |
| [0020](0020-untitled-note-and-first-save.md) | 無題の本文を保持し、初回保存だけ新しいmdを公開する | 受理 |
| [0021](0021-save-as-preserves-original.md) | 別名保存は元を保存せず新しいmdを作る | 受理 |
| [0022](0022-note-rename-and-recovery.md) | 改名の意図を記録し履歴・md・索引を再開可能に移す | 受理 |
| [0023](0023-in-note-search.md) | 現在ノートの検索は表示中の本文を対象にし、判断はcore・語と方向はapplication | 受理 |
| [0024](0024-index-filter.md) | 全ノートの絞り込みは遅延して載せた本文をcoreで判定し、配置は同じdrawer_layoutに絞り込みを渡す | 受理 |
| [0025](0025-settings-file.md) | 設定はdata/settings.jsonの版1をnumberだけで始め、キーを足すたびに版を上げる | 受理 |
| [0026](0026-line-numbers.md) | 原文の行番号はcoreが論理行を数え、主窓が本文の左の空きに描く | 受理 |
| [0027](0027-failure-line-table.md) | 失敗の文言は値ごとの表から引き、表の網羅は字句検査（CNF-009）が守る | 受理 |
| [0028](0028-regex-replace.md) | 正規表現置換はOS同梱のICUをadaptersに閉じ、置換文字列の文法と組み立てはcoreが持つ | 受理 |
| [0029](0029-settings-theme-language-split.md) | 設定・テーマ・言語（#38）は4つの単位に分け、portを束ねてから文言の正本を1表に集め、版は効果のあるキーごとに上げる | 受理 |
| [0030](0030-ui-text-catalog.md) | 利用者に見える文言はcoreのui_textの表1か所からIDと言語で引き、句の中の語と数は置換子で埋め、置き場所を字句検査（CNF-010）で守る | 受理 |
| [0031](0031-theme-and-settings-surface.md) | テーマは3値の選択をsettings.jsonの版2に置き、OSの変更に主窓が追従し、編集中の再着色はTOMのUndo停止と変更印の退避で守る | 受理 |
| [0032](0032-languages.md) | 表示言語はui_textの表を3列にしてsettings.jsonの版3で選び、言語ごとのfaceはcoreの表から引き、訳し残しと列の欠落は機械で守る | 受理 |
| [0033](0033-vector-icons.md) | 閉じる・設定・折畳・選択の印は24のviewBoxの面のパスとしてuiに持ち、OS同梱のGDI+を型定義の無い自前の宣言で結んで既存のDCに直接塗る | 受理 |
| [0034](0034-agent-seats-by-job.md) | 背景席は仕事の種類でモデルを選び、Opusの実装席は1席1仕事で道具出力を小さくし、繰り返しの下ごしらえはスクリプトにする（運用） | 受理 |
| [0035](0035-dialog-theme.md) | 名前入力面と失敗の箱はcomctl32 v5のままdialog_themeの1本の塗りでテーマに従い、失敗の箱はMessageBoxWをやめて同じ塗りの自前のモーダルにする | 受理 |
| [0036](0036-bundled-fonts.md) | 日本語・简体中文・欧文の書体を固定版でfonts/に同梱し、exeの隣からFR_PRIVATEで登録してui_fontの表で選ぶ | 受理 |
| [0037](0037-required-check-verdict.md) | 必須checkは判定jobにし、フルゲートが成功していないhead（Draftのskipped・cancelled）をfailureで止める | 受理 |
