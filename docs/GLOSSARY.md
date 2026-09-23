# 用語集 — NeNe Folio

> Status: normative（規範）/ 2026-09-09 初版
> ここに載っている語は、コード・ドキュメント・Issue で**同じ意味**で使う。同義語を発明しない。

| 語 | 意味 | 型／場所 |
| --- | --- | --- |
| 結果（outcome） | 期待される成功・失敗を表す閉じた `enum` | `*_outcome` |
| 拒否理由（rejection） | 結果に添える閉じた理由の集合 | `*_rejection` |
| ポート（port） | 中核がプラットフォームを使うための型のある窓口（関数ポインタの束） | `*_port`（application） |
| ポートの束（folio ports） | `folio_state` を作るときに渡す 3 つのポートの束。引数の数が C-012 の 4 つで飽和したので、ポートが増える側はこの型で、`folio_state_create` の署名は変わらない。借りるだけで、呼び出しの間しか生きなくてよい | `struct folio_ports`（application）・ADR 0029 の決定 3 |
| アダプタ（adapter） | ポートの実装。プラットフォームに触れてよい唯一の場所 | `*_adapter`（adapters/win32） |
| 合成ルート | 依存を結ぶ唯一の場所。端末と終了コードを持つ | `src/app` |
| 反映（render） | 決まった値を UI 部品へ写す操作。UI 状態を変えてよい唯一の場所 | `render*`（UI） |
| 隔離区画 | 可変性を許した唯一の場所 | ARC-005 の表 |
| 規約検査（conformance） | NeNe Folio 固有の自作ゲート | `eng/conformance.py`（CNF-NNN） |
| waiver | 1 つの規則に対する期限付きの狭い例外 | `docs/waivers/WVR-NNNN-*.md` |
| 機械強制の状態 | active / planned / 不能 / 不採用 | `docs/QUALITY_GATES.md` の強制マトリクス |
| negative proof | ゲートが意図した規則で落ちることの実測 | `docs/quality/gate-proofs.md` |
| ノート（note） | 1 つの Markdown ファイル。`data/<カテゴリ>/<名前>.md` | `struct note`（core） |
| カテゴリ（category） | `data/` 直下の 1 ディレクトリ。色・表示順・展開状態を持つ | `struct category`（core） |
| 索引（index） | ドロワーに並ぶ「カテゴリ → ノート」の一覧。台帳の順序で並ぶ | `struct drawer_layout`（core） |
| 台帳（ledger） | 順序と色を永続化する json。`data/categories.json` と `data/<カテゴリ>/index.json` | adapters/win32 が読み書き・core が解釈 |
| ドロワー（drawer） | 左のペイン。検索窓と索引。自前描画・スクロールバー無し | `src/ui/win32` |
| ビュー（view）／編集（edit） | 右のペインの 2 つのモード。閉じた enum。ノートを切り替えても保たれ、閲覧へ戻すのは「閲覧」の札と窓を閉じる操作だけ | `enum pane_mode`（application）・ADR 0013 |
| 名前入力面（name prompt） | 初回保存・別名保存・改名で名前と保存先カテゴリを受けるモーダル。`dialog_theme` の塗りで、保存先カテゴリは常時開いた一覧（4 行） | `name_prompt_show`（ui/win32）・ADR 0035（補正 17〜21） |
| 失敗の箱（failure box） | 「知らせて戻る」だけのモーダル。application の `folio_state_failure_line` の 1 行を、名前入力面と同じ `dialog_theme` の塗りで見せる。警告音と三角の「!」の印（`ICON_PAINT_WARNING`）を持つ。答えで分岐しない。作れないときだけ `MessageBoxW` へ退避 | `failure_box_show`（ui/win32）・ADR 0035（補正 12〜16） |
| 区画（focus zone） | 鍵を受け取る側。**索引**（主窓）と**本文**（RichEdit）の 2 つで、Win32 のフォーカスそのものが正本。application は区画の状態を持たない | `GetFocus()`（ui/win32）・ADR 0013 の決定 1 |
| カーソル（cursor） | 索引で鍵が指している行。**ノート行**（そのときは選択そのもの）と、**見えるノート行を持たないカテゴリ行**（折り畳み・ノート 0 本）にだけ止まる。カテゴリ行にあるあいだ選択と右ペインは動かない。印は行の右端の角（カテゴリ行では**折畳の印**の左。印は字形ではなく 24 の viewBox の面のパスなので、空ける幅は定数） | `enum folio_cursor_kind`・`folio_state_cursor`（application）・`struct drawer_cursor`・`drawer_row.cursor`（core）・ADR 0015 |
| 歩み（step） | 索引のカーソルを動かす向きと行き先。次・前・最初・最後の閉じた集合で、辿り方（止まる行の列・端で止まる）は application が持つ | `enum folio_step`（application）・`folio_state_select_adjacent` |
| 止まる行（stop row） | 歩みが止まれる行の列。カテゴリごとに、見えるノート行があればそのノートを順に、無ければそのカテゴリ行 1 つ | `folio_state_select_adjacent`（application）・ADR 0015 の決定 2 |
| 寄せる（reveal） | カーソルの行が頭の帯から下端までに収まる最小のスクロール量にすること。鍵でカーソルを動かしたときだけ行い、クリックでは行わない | `drawer_layout_reveal`（core）・`folio_state_reveal_cursor`（application）・ADR 0013 の決定 6 |
| 札（chip） | 右ペインの頭の右端に並ぶ「閲覧」「編集」の切替。有効な側だけ面を塗る | `folio_window` の `chip_rect`・`chip_*` の色（`folio_palette`） |
| 改行の形（line ending） | 本文の改行が LF か CRLF か。読んだ本文の最初の改行で決まり、書き戻すときに揃える | `enum line_ending`（core） |
| ドロップ先（drop target） | ドラッグを離したときに落ちる場所。種類・**移動先の**カテゴリ・**移動後の番号**・挿入線の y を持つ | `struct drop_target` / `enum drop_kind`（core）・`drawer_layout_drop` |
| 塊（chunk） | カテゴリ行と、展開中ならそのノート行をひとまとめにした範囲。カテゴリの並び替えの候補はこの境界で、ノートの移動先はポインタのある塊（境界は隣り合う塊の間の中ほど） | `drawer_layout_drop`（core） |
| ノートの居場所（note ref） | カテゴリ番号とノート番号の組。移動の意図はこれを 2 つ受ける | `struct note_ref`（core） |
| 台帳が古い（ledger stale） | mdの作成・移動は済んだがindex.jsonを保存できない状態。表示はファイルに従い、再起動の照合で揃う。初回保存では次の保存・変更前に再試行し、未同期の終了を拒否する | `FOLIO_STATE_LEDGER_STALE`（application）・ADR0008/0020 |
| 無題（untitled） | まだファイル名を持たない編集中の文書。空でも未保存。本文とUndoはRichEdit、種類と文書カテゴリはapplicationが持つ | `folio_document_kind`・ADR0020 |
| 初回保存（first save） | 無題の本文を名前とカテゴリに結び、新しいmdを既存ファイルの置換なしで公開する。初回の履歴は作らない | `note_name`（core）・`folio_state_store_new`（application）・`create_note`ポート |
| スクロール量（scroll offset） | ドロワーを下へずらした画素。**要求量**は application が持つ値で、**有効量**はいまの配置の上限（0〜`bottom + bottom_padding − viewport_height`）へ丸めた量。表示座標は内部の座標から有効量を引いたもの | `folio_state_scroll_drawer`（application）・`drawer_layout_scroll` / `drawer_layout_scroll_limit`（core）・ADR 0009 |
| フェード（fade） | あふれている側の端に置く短い帯。端で地の色 100%・内側で 0% になるよう画素ごとに混ぜ、隠れた行があることを示す。要否は core の `drawer_layout_overflow_above` / `_below` が答える | `drawer_window` の `draw_fade`（ui/win32）・ADR 0009 |
| 挿入線（drop line） | ドラッグ中に落ちる位置を示す 1 本の線。掴んだ行のカテゴリ色で、その字下げから右の余白まで | `drawer_window` の `draw_drop_line`（ui/win32）。y は core が返す |
| 意図（intent） | UI が発行する操作。クリック・ドラッグ・ホイール・鍵・入力 | application の reducer 関数 `folio_state_*`（`folio_state_toggle_category` 等）。種類が増えたら `enum folio_intent` に束ねる |
| 表示値（view model） | application が作り UI が写す値 | `struct *_view`（application）・`struct drawer_row`（core） |
| 測定ビルド（measurement build） | 同じ中核ソースと単体テストを計装コンパイルして分岐を測る検証専用のビルド。第 2 の製品実装ではない | `eng/coverage.py` → `out/coverage/` |
| テーマ（theme） | 描画に使うライト／ダークの閉じた 2 値。選択と OS の値から `folio_theme_resolve` が決める | `enum folio_theme`・`folio_theme_resolve`（core） |
| テーマの選択（theme choice） | 利用者が設定で選ぶ 3 値（`system` / `light` / `dark`）。`system` だけが OS の値を通す | `enum folio_theme_choice`（core）・`folio_state_theme_choice`（application）・ADR 0031 |
| 設定画面（settings surface） | 頭の歯車・「操作」メニュー・パレットの「設定」が開く入力面。EDIT を持たずレイヤー自身がフォーカスと鍵を受ける | `COMMAND_SURFACE_SETTINGS`（ui/win32）・ADR 0031 の決定 7 |
| パレット（palette） | テーマごとの色の束。RTF 用と自前描画用の 2 つが正本 | `rtf_palette`（core）・`folio_palette`（ui/win32） |
| 操作 ID（command ID） | Ex・操作パレット・既存ショートカットが共通して実行する、表示名から独立した閉じた値 | `enum folio_command`（core） |
| 操作パレット（command palette） | Ctrl+P で開き、登録済みの操作を表示名または Ex 別名で絞り込んで実行する一時的な入力面 | `folio_command`（core）の登録表・`folio_window`（ui/win32）の入力面 |
| 編集の破棄（`:e!`） | 編集を破棄して保存済みの本文に戻す。ディスクは読み直さない（vim と違う）。別名 `:edit!`・パレットと「操作」メニューの「編集を破棄して読み直す」も同じ意図を通る | `folio_state_discard_edits`（application）・ADR 0016 の補正 4 |
| 履歴（history） | md を書き戻す**直前**にファイルにあった本文の写し。`data/.history/<カテゴリ>/<ノート>/1.md`（最新）〜 `5.md`（最古）の連番で、6 つ目は捨てる。時刻は名前にも中身にも使わない。残せなければ保存しない | `note_history_depth`（core）・`archive_note`（persistence_port）・ADR 0012 |
| 確保失敗の注入（allocation probe） | 測定ビルドでだけ中核の `malloc` / `calloc` / `realloc` をテストへ向け、n 回目の確保を失敗させる仕掛け | `tests/unit/allocation_probe.h` |
| キャレット（caret） | 本文の挿入位置と選択範囲。索引のカーソルとは別で、RichEdit自身が唯一の所有者。Ctrl＋hjklも部品の移動機能を呼ぶ | `note_pane`（ui/win32）・`ITextSelection`・ADR0019 |
| 別名保存（save as） | 現在の本文を新しいmdへ保存してそちらを開く。元のmd/履歴は保存済みの状態を保つ。閲覧では表示文字ではなくMarkdown原文を使う | `FOLIO_COMMAND_SAVE_AS`→`folio_state_store_new`・ADR0021 |
| 名前変更（rename） | 現在のノートのmdと履歴ディレクトリを、同じカテゴリの新しい名前へ移すこと。台帳の位置は変わらない | `FOLIO_COMMAND_RENAME`→`folio_state_rename_note`・ADR0022 |
| 改名の意図（rename intent） | カテゴリ・旧名・新名・完成後の台帳を持つ不透明な値。副作用の前に確保し、未完了なら application が 1 つだけ保持する | `struct note_rename`（core）・ADR0022 の決定 2 |
| 復旧記録（rename journal） | `data/.rename.json`。版 1 の改名の意図と、元 md・元履歴の 128bit 識別子を持つ。これがあるあいだ改名は未完了 | `struct rename_journal`（core）・ADR0022 の決定 5 |
| 錠（data lock） | `data/.nenefolio.lock` を共有なしで開いたまま持つハンドル。同じ `data/` を使うプロセスを 1 つに直列化する。ファイルは残ってよく、所有は OS のハンドルが決める | `persistence_adapter`（adapters/win32）・ADR0022 の決定 3 |
| ノート内検索（in-note search） | 現在表示中のノートの中だけを探し、一致を RichEdit の選択 1 つで示すこと。全ノートの本文で索引を絞り込む検索（FR-032 / #40）とは対象も語も別 | `note_search`（core）・`folio_state_search_term`（application）・ADR 0023 |
| 検索の向き（search direction） | 前方（`/`・「次へ」）か後方（`?`・「前へ」）の閉じた選択肢。application が「直前の方向」を 1 つ覚え、`N` の一回きりの逆向きでは変わらない | `enum search_direction`（core）・`folio_state_search_direction`（application） |
| anchor（探し始める位置） | いまの選択範囲。次へは選択の外側から、入力中は同じ一致の頭から数え直す。選択の正本は RichEdit で、core も application も持たない | `struct note_search_span`（core）・ADR 0023 の決定 1 |
| 復旧待ち（recovering） | 記録を公開したまま完了していない状態。パンくずは古い名前を実ファイル名として示さず、次の意図が先に同じ改名を再試行する | `pane_title_view.recovering`・`FOLIO_STATE_RENAME_PENDING`（application） |
| 絞り込み（index filter） | 全ノートの名前と本文に語が含まれるノートだけを索引に残すこと。一致を持つカテゴリは折り畳んでいても展開して並ぶ。ノート内検索（FR-011）とは対象も語も別 | `index_filter`（core）・`folio_state_set_index_filter`（application）・ADR 0024 |
| 論理行（logical line） | md 原文の改行で区切られた 1 行。RichEdit が折り返して作る**表示行**とは別で、折り返しの継続行には番号を出さない。表示中の平文では段落区切りが CR 1 つに揃っている | `line_index`（core）・`struct line_mark`・ADR 0026 の決定 2 |
| 番号の帯（gutter） | 編集中の本文の左の空きに、主窓が自前で描く論理行番号の列。RichEdit の中には置かない（スクロールで一緒に動いてしまうため） | `struct gutter_row`（ui/win32）・`note_pane_visible_rows`・ADR 0026 の決定 1 / 3 |
| 設定（settings） | `data/settings.json` の版 2（`number` と `theme`。版 1 は読めて版 2 へ移行する）。効果を実装したキーだけが入り、キーを足すたびに版を上げる。無いときだけ既定値で始め、読めなければ既定値で起動して**上書きしない** | `folio_settings`（core）・`folio_state_set_number` / `folio_state_set_theme`（application）・ADR 0025・ADR 0031 |
| 本文の写し（note corpus） | 絞り込みが読む、ノートの本文の複製。**空でない語が初めて来たときに 1 回だけ**ポートから読み、以後は保存・新規・改名・移動で該当の写しだけ差し替える。宛先はカテゴリ名とノート名（並び替えでは動かない）。写しが無いノートは一致しない | `note_corpus`（core）・ADR 0024 の決定 1 |
| 配置の入力（drawer source） | ドロワーの配置が読む、カテゴリ台帳・索引台帳の列・nullable の絞り込みの束。絞り込みの有無で配置の関数を分けないための 1 つの型 | `struct drawer_source`（core）・ADR 0024 の決定 3 |
| 置換の下見（replace preview） | 「このパターンで、この本文の、この一致を、この置換文字列で置き換える」を 1 つ持つ値。**UTF-16 の本文の写し**・一致の列・件数・宛先（カテゴリ名とノート名）・解析済みの置換文字列を所有する。位置が `EM_EXSETSEL` と 1 対 1 でなければならないので UTF-8 へ写さない（C-014 の名指しの例外） | `replace_preview`（core）・`folio_state_preview_replace`（application）・ADR 0028 の決定 6 |
| 置換の当て方（replace scope） | 1 件（anchor 以降で最初の一致）・すべて・各論理行の最初の一致（`g` の無い `:%s`）の閉じた 3 値。選別そのものは core が行う | `enum replace_scope`（core）・ADR 0028 の決定 6 / 8(b) |
| 置換文字列（replace template） | 利用者が打つ置換の書き方を解析した値。`&` と `\0`・`\1`〜`\9`・`\r` / `\n`・`\\` / `\&` / `\/` だけを認め、ICU の `$1` 形式は使わない | `replace_template`（core）・ADR 0028 の決定 5 |
| 置換の編集（replace edit） | 「本文のこの範囲を、この UTF-16 で置き換える」1 つ。1 件なら一致の範囲、すべてなら本文全体。UI は `EM_EXSETSEL` → `EM_REPLACESEL(TRUE)` の 1 回で当てるので Undo も 1 単位 | `replace_edit`（core）・`note_pane_replace`（ui/win32）・ADR 0028 の決定 7 |
| 古い下見（stale preview） | 渡された本文が下見の写しと違う、または宛先が今の文書と違う状態。下見を捨てる契機を列挙せず、照合だけで「同じ本文の別ノートへ当てる」を構造的に防ぐ | `FOLIO_STATE_REPLACE_STALE`（application）・ADR 0028 の決定 6 |
| 入力面の欄の集合（command input set） | 入力面が「自分のもの」と見なす EDIT の集合。集合の中でフォーカスが移るあいだは入力面を閉じない。置換の欄だけが 2 つ持つ | `command_owns`（ui/win32）・ADR 0016 の 2026-09-22 の補正 |
| 文言の表（ui_text） | 利用者に見える語・句・1 行の唯一の置き場所。`enum ui_text` の ID で引き、文言そのものは `src/core/ui_text.c` の指示付き初期化子の表だけが持つ。ファイル名・台帳のキー・Ex の別名・RTF の制御語・Win32 のクラス名と書体名は文言ではない | `ui_text_line`（core）・C-018 / CNF-010・ADR 0030 |
| 置換子（placeholder） | 句の中に語や数を差し込む綴り。`{k}` `{n}` `{offset}` `{name}` `{from}` `{to}` の 6 つだけで、埋めた内容は再走査しない（`{k}` という名前のノートが壊れない）。語や数を連結して句を作らない | `ui_text_format` / `struct ui_text_request`（core）・ADR 0030 の決定 3 |
| 表示に使う言語（folio_language） | 文言をどの列から引くかの閉じた選択肢。core は「いまの言語」を可変状態として持たず、呼ぶたびに引数で受ける。値は application が答える | `enum folio_language`（core）・`folio_state_language`（application）・ADR 0030 の決定 1 |

## 使ってはいけない語

`manager` / `helper` / `util` / `utils` / `common` を型名の語尾に使わない（C-010）。
役割を語る名前が思いつかないときは、その型が 2 つの責務を持っている可能性が高い。

## #76（表示言語）で足した語

| 語 | 意味 |
| --- | --- |
| `folio_language` | 表示に使う言語の閉じた列挙（`JA` / `EN` / `ZH_HANS`）。core が持つが「いまの言語」は持たず、値の所有者は application の設定である |
| `ui_font` | 言語ごとの書体の face 名を持つ core の表。同梱（Noto Sans JP / SC。English は Noto Sans JP）を `ui_font_face(language)` が、OS の退避（Yu Gothic UI / Microsoft YaHei UI）を `ui_font_fallback_face(language)` が引く（ADR 0036 の決定 4） |
| `ui_face` | その face を Win32 へ渡せる UTF-16 にし、`EnumFontFamiliesExW` で**実在を確かめて**無ければ日本語の face に落とす ui の 1 関数 |
| `ascii_fold` | 「ASCII の英字だけ大小を無視する」畳み込み。索引の絞り込み・ノート内検索・パレットの部分一致の 3 か所が引く core の 1 か所 |
| 閉じた語彙 | 訳してはいけない語（Ex の別名・`:set` の語・鍵の名前）。ja の行にあれば en と zh-Hans の同じ行にも無ければならない（単体が固定する） |
| 論理行の包み | 「最初に見える論理行を退避 → 当てる → 選択を戻す → 論理行を戻す」の手順。face の差し替えと再着色が共有する（ADR 0032 の決定 6） |
