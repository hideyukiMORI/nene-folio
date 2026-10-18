# #349 — 本文操作帯と保存状態・ヘルプのヘッダー移動（I-B）

2026-10-18、hideが明示した分担（設計=Fable席、実装/調査/テスト=Opus席）に基づく実装席の記録。
Issue: [#349](https://github.com/hideyukiMORI/nene-folio/issues/349)。設計の正本は [ADR0060](../adr/0060-ui-part-styles-and-state-rule.md) の I-B 補正節（決定8〜14）と、
作業中に設計席が確定して後で補正節に書く値（余白の原則・S 群の外枠なし・320 幅の縮め方・入力欄の内側余白）。
規則: ARC-001/004/011、C-002/008/012/017/018、QLT-008/011/012/013、GIT-001〜004。基点 main a7bf65d（I-A #348 統合後）。
恒久証跡: `D:/NeNeFolio/agents/349-action-bar/`（final/ が完成候補 36881ae の exe での対象確認、try1〜3 は途中診断、tools/ は駆動の道具）。
新しい theme トークン・pragma・waiver・閾値の変更なし。Waivers: none。

## 変更境界

- 保存の状態: application に `folio_state_unsaved`（未選択・閲覧は SAME、無題は CHANGED、編集中は note_changed と同じ正規化の比較を共有）と
  `folio_state_store_revision`（md を書いた時だけ進む）を足した。UI は入力の後の合流点（`folio_message_accessibility_changed`）の
  `settle_save_state` 1 か所で写しを取り直し、保存釦の様式（B/A）・パンくずの丸・UIA の Name / ItemStatus を同じ値から描く。
  保存の世代が進んで保存済みになったときだけ `UiaRaiseNotificationEvent`（ActionCompleted・MostRecent）を 1 回出す。窓の題名は不変。
- ヘルプ「?」は C 様式でヘッダー（設定の左）へ。UIA 親はヘッダー、Tab は 新規→表示3択→ヘルプ→設定→閉じる。
- 操作帯は [保存｜このノート内を検索｜操作▾] の A 群で、UIA の Group「ノートの操作」の子にした。操作▾の UIA 名は「操作」（▾は描く字だけ）。
- 新規の tooltip / HelpText は `new_note_destination` 1 本で表示のたびに組む（作成先は Ctrl+N と同じ `folio_state_current_category`）。
- 空状態: 文書が無いあいだ本文の RichEdit を隠し、操作帯・閲覧/編集・パンくずを描かず hit/UIA からも外し、案内（UIA は Text）を描く。
- 無題の題から「（未保存）」を外し、3 言語の期待表を更新。閲覧の札の英語は Read。`enum ui_text` は値が 256 を超えたので `unsigned short` にした。
- 寸法（設計席の確定値）: 窓の縁 16（`ui_part_edge`）、新規とブランドの間 16、表示見出し→S 群 8、S 群→「?」16、「?」→歯車 8、歯車→× 8、
  ペイン右端 16（閲覧/編集の右端は × の右端と同じ縦線）、ドロワーの検索欄と＋の左右 16。S 群は外枠と群の余白 4 を外した。
  A 部品の可変幅は論理 px の 4 の倍数へ切り上げ、閲覧/編集は 3 言語で 56 の固定幅。
- 入力欄（全ノート検索・ノート内検索・置換・パレット/Ex/テーマの欄・名前入力）は `place_field` / `ui_part_field_edit` の 1 本で置き、
  EDIT を字の高さで縦中央・字は箱の縁から 8、枠は箱の内側に静止 1px / focus 2px（太っても字は動かない）。パレット・Ex・置換・テーマの欄にも同じ枠を描く。

## 完成時の対象検証

- `pwsh -NoProfile -File ./eng/check.ps1`（作業木・36881ae）: exit 0、「NeNe Folio full gate passed」（`check-3.log`）。
  分岐網羅 93.21%（3542/3800）。1 回目は CNF-002（folio_window.c の型定義）、2 回目は QLT-004（注記の整形）で落ち、コードを直して通した。
- 単体: `ui_text_tests` に #349 の 12 ID と変更 2 件の 3 言語固定・組み立て（新規の tooltip、パンくずの未保存名）を追加。
  `state_tests` に `folio_state_unsaved`（未選択・編集入りだけ・変更・正規化できない本文・保存で SAME・閲覧・無題）と保存の世代（書かない保存では進まない）を追加。

## 隔離 Win32 fixture（`final/`・exe SHA256 C2F15D41…BE99）

専用デスクトップ `NeNeFolio-349-action-bar` で fixture の exe を起動し Path と SHA256 を照合。入力は PostMessage と UIA の Select、撮影は PrintWindow。
UIA の通知は native の `IUIAutomation5::AddNotificationEventHandler` を使う一時の道具（`tools/uia_notify.c`、repo に置かない）で受けた。
3 言語 × matrix / interact と、カテゴリ 0 件の 3 言語の計 9 回。無題を開いた interact の 3 回は終了時の保存確認で止まるので、自分で起動した PID を Path 照合のうえ停止した。他は WM_CLOSE で exit 0。

- 保存の遷移（3 言語とも同じ結果）: 閲覧 → 保存済み（A）。編集に入っただけ → 保存済み（A、橙なし）。1 字入力 → ItemStatus「未保存の変更があります」、
  パンくず Name「…／日本語の入力（未保存）」、釦の地 #E95420（chip_background）、丸あり。保存釦 → 保存済みへ戻り通知「保存しました」。
  入力 → 本文 Esc → 保存済み＋通知。入力 → ノート切替 → 切替先が保存済み、切替元の md 先頭に打った字が書かれ、通知。Ctrl+N の無題 → 開いた時点で未保存（橙・丸・「無題（未保存）」）。
  通知は 3 回（保存釦・Esc・切替）で、無題を開いたときは出ない。題名は全段で「NeNe Folio」。
- ヘルプ「?」: 120 DPI・1200 幅で x=1084（歯車の左 8）、親は「共通操作」、AcceleratorKey F1、tooltip「ヘルプとコマンド一覧（F1 / Ctrl+P）」。
  F6 で新規へ入り Tab×6 は 新規→両方→ヘルプ→設定を開く→閉じる→新規→両方。クリック・F1・Ctrl+P の面の画像は互いに identical。
- tooltip: 新規「新しいノート — 入力と検索の確認へ（Ctrl+N）」、操作▾「このノートの操作」、保存 Ctrl+S、検索 Ctrl+F。カテゴリ 0 件では新規の tooltip / HelpText /
  空状態の作成先の行が「カテゴリがありません。先にカテゴリを作ります」（en / zh も表どおり）。
- 空状態: 保存・閲覧・操作・検索・ノートの操作の ToolBar・Document の UIA 要素が 0、案内は Text「ノートが選ばれていません」（HelpText に本文・キー・作成先）。
  保存と閲覧の札があった位置を押しても capture なし・画像 identical・ダイアログなし。操作帯の tooltip の矩形は (0,0,0,0)。
- 配置（3DPI × 3 言語 × 両方/ノート/一覧 ＋ 560×360、計 36 枚）: 操作部品の UIA 高さは全条件 28。× の右端と閲覧/編集の右端は窓の右端から 16。
  一覧のみ（窓 320）は区画 40・群間 8 を採り、移動領域（新規の右端〜表示群の左端）は 3DPI × 3 言語とも 32。
  群間 16・区画 44 は 16+28+24+132+16+28+8+28+8+28+16=332、群間 8・区画 44 でも移動領域 20 で 24 に届かず、決めた順に区画 40 まで縮めた。
  ブランドと「表示」見出しは一覧では描かれない。560 幅の英語ではノート内検索が絵へ縮み、消えない。
- 入力欄: パレット・全ノート検索（静止/focus/入力）・名前の変更・ノート内検索の画像で、字が枠の内側 8 から始まり縦中央、focus の 2px が内側へ太っても字が動かないことを目視した（`field-*.png`）。

## 実デスクトップでの 1 回（設計席の依頼）

同じ exe を実デスクトップで 1 回だけ起動し、UIA の値を読んで WM_CLOSE で exit 0 にした（`final/real-desktop.json`）。hide の稼働プロセスには触れていない。
新しいノートの HelpText「新しいノート — 入力と検索の確認へ（Ctrl+N）」、保存の ItemStatus「保存済み」、Group は「カテゴリ」「表示」「ノートのモード」「ノートの操作」。
**全ノート検索の EDIT は実デスクトップでも native UIA（UIAutomationCore）が ControlType Pane・Name 空で返した。** 隔離デスクトップだけの現象ではない。
MSAA の名前は I-A の記録どおり入っている。原因の調査と修正は本 Issue では行っていない（設計席へ送付）。

## 独立レビュー後の修正（H1・M1・M2・L1・L3・L5）

設計席の独立レビューの指摘を同じ枝で直した。`check.ps1` は 55e9dd2 で exit 0（`check-4.log`、分岐網羅 93.23%）。
対象確認は `D:/NeNeFolio/agents/349-action-bar/review-final/`（製品の exe・SHA256 C291B9BB…5C72）と `review-probe*/`（一時の計装版）。成功済みで無関係な確認は再実行していない。

- H1: 未保存の合図は core の `theme_contrast_unsaved`（役割 `THEME_LINE_UNSAVED`、地は window / pane）が chip_background→selected_background の順に 3:1 以上の塗りを選び、
  どちらも届かないテーマでは同じ線の色で 2px の枠・字・丸を描く。`theme_tests` に全テーマの「合図 対 window / pane ≥ 3:1」「選んだ塗りと字 ≥ 4.5:1」「chip が届くテーマは chip のまま」
  「cobalt2 / tokyo-night-light は chip 以外」を足した。fixture では cobalt2 と tokyo-night-light の両方で保存釦が枠（cobalt2 は #FFFFFF、tokyo-night-light は #343B59）、丸も同じ色、既定テーマは従来どおり #E95420 の塗り。
  B の focus の地は選んだ塗り（塗らないテーマでは window / pane）にした。
- M1: 通知は md を書いた世代の前進だけで出し、本文を評価できない回は世代を持ち越す。名前付きの dirty のまま Ctrl+N で無題を開くと、離脱の保存で通知が 1 回出た（既定・cobalt2・tokyo-night-light）。
- M2: 本文の比較は EN_CHANGE の印・保存の世代・文書・mode の変化があるときだけ行う。一時の計装版（比較の回数を窓の property へ書く。ソースは戻し製品に含めない）で、
  編集に入った後の比較 1 回に対し、→×5・↓×1 の後も 1 回のまま、1 字入力で 2 回になった。
- L1: ノートのみで文書が無いとき、表示切替の復帰と root へ戻った focus の正規化の両方で新しいノートへ focus と印を置く。索引に focus がある状態からパレットの「ノートのみ表示」で切り替えると新しいノートに focus、F6 でも新しいノートに戻る。
- L3: 名前入力のカテゴリ一覧も箱の内側の枠（`dialog_theme_field`）にし、外側の枠の経路（`ui_part_frame_field` / `dialog_theme_frame`）を消した。対話は WS_CLIPCHILDREN を持たないので子の矩形を除いて塗る。
- L5: ToolBar の日本語名を「本文の操作帯」にした（Group「ノートの操作」と区別。英語・中国語は元から別名）。

## 未測定・限界

- 実カーソルの hover・物理キー・ナレーターの読み上げ（通知が実際に読まれるか）、96 DPI、ハイコントラスト実機は確かめていない。
- 選択中の項目に focus が乗ると下辺の棒が focus 枠の下辺と重なって見える。設計席が規則内として受理済み。
- 保存の状態は EN_CHANGE の後の合流点で名前付きの編集本文を写して比べる。巨大なノートでの打鍵の遅れは測っていない（設計席の指示で測定不要）。
- 変更の無い Ctrl+S（md を書かない保存）では通知を出さない（保存の世代が進まないため）。
- I-A で成功済みの押下・capture・focus 規則の対象確認は、入力の経路を変えていないので再実行していない。既存の保存・RTF・検索性能の試験も再実行していない。
