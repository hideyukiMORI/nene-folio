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
| ビュー（view）／編集（edit） | 右のペインの 2 つのモード。閉じた enum | `enum pane_mode`（application） |
| 札（chip） | 右ペインの頭の右端に並ぶ「閲覧」「編集」の切替。有効な側だけ面を塗る | `folio_window` の `chip_rect`・`chip_*` の色（`folio_palette`） |
| 改行の形（line ending） | 本文の改行が LF か CRLF か。読んだ本文の最初の改行で決まり、書き戻すときに揃える | `enum line_ending`（core） |
| ドロップ先（drop target） | ドラッグを離したときに落ちる場所。種類・**移動先の**カテゴリ・**移動後の番号**・挿入線の y を持つ | `struct drop_target` / `enum drop_kind`（core）・`drawer_layout_drop` |
| 塊（chunk） | カテゴリ行と、展開中ならそのノート行をひとまとめにした範囲。カテゴリの並び替えの候補はこの境界で、ノートの移動先はポインタのある塊（境界は隣り合う塊の間の中ほど） | `drawer_layout_drop`（core） |
| ノートの居場所（note ref） | カテゴリ番号とノート番号の組。移動の意図はこれを 2 つ受ける | `struct note_ref`（core） |
| 台帳が古い（ledger stale） | md は移ったが `index.json` を書き戻せなかった状態。表示はファイルに従い、次回の起動の照合（FR-008）で揃う | `FOLIO_STATE_LEDGER_STALE`（application）・ADR 0008 |
| 挿入線（drop line） | ドラッグ中に落ちる位置を示す 1 本の線。掴んだ行のカテゴリ色で、その字下げから右の余白まで | `drawer_window` の `draw_drop_line`（ui/win32）。y は core が返す |
| 意図（intent） | UI が発行する操作。クリック・ドラッグ・ホイール・入力 | application の reducer 関数 `folio_state_*`（`folio_state_toggle_category` 等）。種類が増えたら `enum folio_intent` に束ねる |
| 表示値（view model） | application が作り UI が写す値 | `struct *_view`（application）・`struct drawer_row`（core） |
| 測定ビルド（measurement build） | 同じ中核ソースと単体テストを計装コンパイルして分岐を測る検証専用のビルド。第 2 の製品実装ではない | `eng/coverage.py` → `out/coverage/` |
| テーマ（theme） | ライト／ダークの閉じた選択肢。OS のアプリのモードから起動時に決まる | `enum folio_theme`（core） |
| パレット（palette） | テーマごとの色の束。RTF 用と自前描画用の 2 つが正本 | `rtf_palette`（core）・`folio_palette`（ui/win32） |
| 確保失敗の注入（allocation probe） | 測定ビルドでだけ中核の `malloc` / `calloc` / `realloc` をテストへ向け、n 回目の確保を失敗させる仕掛け | `tests/unit/allocation_probe.h` |

## 使ってはいけない語

`manager` / `helper` / `util` / `utils` / `common` を型名の語尾に使わない（C-010）。
役割を語る名前が思いつかないときは、その型が 2 つの責務を持っている可能性が高い。
