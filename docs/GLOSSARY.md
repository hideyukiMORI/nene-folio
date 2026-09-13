# 用語集 — NeNe Folio

> Status: normative（規範）/ 2026-09-09 初版
> ここに載っている語は、コード・ドキュメント・Issue で**同じ意味**で使う。同義語を発明しない。

| 語 | 意味 | 型／場所 |
| --- | --- | --- |
| 結果（outcome） | 期待される成功・失敗を表す閉じた `enum` | `*_outcome` |
| 拒否理由（rejection） | 結果に添える閉じた理由の集合 | `*_rejection` |
| ポート（port） | 中核がプラットフォームを使うための型のある窓口（関数ポインタの束） | `*_port`（application） |
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
| 区画（focus zone） | 鍵を受け取る側。**索引**（主窓）と**本文**（RichEdit）の 2 つで、Win32 のフォーカスそのものが正本。application は区画の状態を持たない | `GetFocus()`（ui/win32）・ADR 0013 の決定 1 |
| カーソル（cursor） | 索引で鍵が指している行。**ノート行**（そのときは選択そのもの）と、**見えるノート行を持たないカテゴリ行**（折り畳み・ノート 0 本）にだけ止まる。カテゴリ行にあるあいだ選択と右ペインは動かない。印は行の右端の角（カテゴリ行では `+` / `−` の左） | `enum folio_cursor_kind`・`folio_state_cursor`（application）・`struct drawer_cursor`・`drawer_row.cursor`（core）・ADR 0015 |
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
| テーマ（theme） | ライト／ダークの閉じた選択肢。OS のアプリのモードから起動時に決まる | `enum folio_theme`（core） |
| パレット（palette） | テーマごとの色の束。RTF 用と自前描画用の 2 つが正本 | `rtf_palette`（core）・`folio_palette`（ui/win32） |
| 操作 ID（command ID） | Ex・操作パレット・既存ショートカットが共通して実行する、表示名から独立した閉じた値 | `enum folio_command`（core） |
| 操作パレット（command palette） | Ctrl+P で開き、登録済みの操作を表示名または Ex 別名で絞り込んで実行する一時的な入力面 | `folio_command`（core）の登録表・`folio_window`（ui/win32）の入力面 |
| 履歴（history） | md を書き戻す**直前**にファイルにあった本文の写し。`data/.history/<カテゴリ>/<ノート>/1.md`（最新）〜 `5.md`（最古）の連番で、6 つ目は捨てる。時刻は名前にも中身にも使わない。残せなければ保存しない | `note_history_depth`（core）・`archive_note`（persistence_port）・ADR 0012 |
| 確保失敗の注入（allocation probe） | 測定ビルドでだけ中核の `malloc` / `calloc` / `realloc` をテストへ向け、n 回目の確保を失敗させる仕掛け | `tests/unit/allocation_probe.h` |
| キャレット（caret） | 本文の挿入位置と選択範囲。索引のカーソルとは別で、RichEdit自身が唯一の所有者。Ctrl＋hjklも部品の移動機能を呼ぶ | `note_pane`（ui/win32）・`ITextSelection`・ADR0019 |
| 別名保存（save as） | 現在の本文を新しいmdへ保存してそちらを開く。元のmd/履歴は保存済みの状態を保つ。閲覧では表示文字ではなくMarkdown原文を使う | `FOLIO_COMMAND_SAVE_AS`→`folio_state_store_new`・ADR0021 |

## 使ってはいけない語

`manager` / `helper` / `util` / `utils` / `common` を型名の語尾に使わない（C-010）。
役割を語る名前が思いつかないときは、その型が 2 つの責務を持っている可能性が高い。
