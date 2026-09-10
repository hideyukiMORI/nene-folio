# ADR 0012 — md を書き戻す直前の本文を data/.history に連番で 5 版まで残し、残せなければ保存しない

- 状態: 受理
- 日付: 2026-09-10
- Issue: #23
- 影響する規則: ARC-003 / ARC-004 / ARC-007 / ARC-009 / ARC-010 / C-002 / C-005 / C-012 / CNF-002 / QLT-009 / QLT-013

## 文脈

md の保存は Ctrl+S・編集を抜ける操作・別ノートの選択・窓を閉じる操作で自動的に走り（ADR 0006）、破棄は無い。FR-018（Issue #24）で
索引の鍵の移動でも保存するようになると、意図しない保存で前の本文を失う経路が増える。施主は 2026-09-10 に「編集前の状態を数個ログとして
蓄積してローテートし、誤って自動保存されたら前の版に戻れる保険」を求め、保持数 5・置き場所 `data/.history`（`data/` ごと持ち運べる）で合意した。

現行の保存経路は application の `store_edited`（本文が同じなら何もしない → `markdown_rtf` を作る → `port.write_note` → 差し替え）で、
adapters の `write_note` は一時ファイルと `MoveFileExW` で原子的に置き換える。カテゴリの走査は `data/` 直下のディレクトリを
名前だけで拾い、`.` と `..` しか除外していない。時刻を読んでよいのは adapters だけ（ARC-007）で、履歴の名前に時刻を使うと
core / application のテストが決定性を失う。

## 決定

**application は本文が変わったと確定したあと、`write_note` の前に `port.archive_note` で「いまファイルにある本文」を履歴へ写させ、
写せたときだけ書き戻す。履歴は `data/.history/<カテゴリ>/<ノート>/1.md`（最新）〜 `5.md` の連番で、6 つ目は捨てる。
`.` で始まるディレクトリはカテゴリとして走査しない。**

具体的には:

1. **形。** `data/.history/<カテゴリ>/<ノート>/N.md`。`1.md` が最新、`5.md` が最古。保持数は core の `note_history.h` に
   `constexpr size_t note_history_depth = 5` として 1 か所に置く（戻す UI と adapters が同じ値を見る）。時刻は名前にも中身にも使わない
2. **順序。** `store_edited` は `note_text_equals` で同じなら何もしない（既存）。違えば `archive_note(adapter, category, note)` →
   `STORED` か `ABSENT`（元の md が無い）なら `write_note` へ進む。`archive_note` が `UNWRITABLE` なら新しい結果
   `FOLIO_STATE_HISTORY_FAILED`（文言:「履歴を書けなかったので保存していません。編集中の本文は残っています。」）で状態を変えない。
   `OUT_OF_MEMORY` は既存の値
3. **ローテーション。** adapters は `data/.history/<カテゴリ>/<ノート>/` を作り（無ければ）、`5.md` を消し、`4.md → 5.md` …
   `1.md → 2.md` を `MoveFileExW` で改名し、いまの md の中身を `file_bytes_store`（一時ファイル + `MoveFileExW`）で `1.md` へ書く。
   途中で落ちると `1.md` が欠けた履歴が残りうるが、md は無傷（書き戻しはこのあと）。欠けた番号は次の保存で詰まらず、
   戻す側は「あるものだけ」を見る
4. **走査。** adapters の `accept_entry` は名前が `.` で始まるディレクトリを拾わない（`.history` のため。`.git` 等も同じ扱い）。
   ファイル（ノート）は従来どおり。`name_list` の規則は変えない（`.` 始まりの名前は台帳としては妥当。走査で拾わないだけ）
5. **別カテゴリへの移動。** 履歴は**追随しない**。移動後の最初の保存で新しい場所に積み始め、古い履歴は元の場所に残る（孤立）。
   戻す UI（別 Issue）を入れるときに、移動時の追随を同じ ADR で決める
6. **ポート。** `persistence_port` に `archive_note(adapter, category, note)` を 1 本足す。テストの偽アダプタは呼び出しの順
   （`archive` → `write`）と回数を記録する
7. **戻す操作。** この縦切りでは持たない。利用者はエクスプローラで `data/.history` を開いて手で戻せる（それがこの保険の最低線）

## 強制

- adapters 以外がファイルを触らないこと、core / application が時刻を読まないことは ARC-003 / ARC-007 が **active**（`eng/symbols.py`）
- 順序（`archive` → `write`・archive の失敗で write 0 回・同じ本文で両方 0 回）は `tests/unit/state_tests.c` が正本
- 新しい結果の値は `folio_state_outcome.h` に足し、`folio_state_failure_line` の網羅は C-002 でコンパイルが守る
- 分岐 90% は QLT-009 が **active**
- 実機は `docs/quality/gate-proofs.md`（6 回保存して `1.md`〜`5.md` が新しい順・最古が消える・`.history` がカテゴリに出ない・
  `.history` を読み取り専用にして保存が止まり本文が残る・バイト列で LF / CRLF と BOM 無しが保たれる）

## 結果

得る: どの経路の自動保存でも、直前 5 版の本文が `data/` の中に残る。復元は手作業でもできる。名前が連番なので core / application の
テストは決定的なまま。`data/` を丸ごと写せば履歴も一緒に移る。

失う・残る:

- 履歴を書けない環境（`.history` が読み取り専用・容量不足）では保存もできない。保険を優先した判断で、文言で理由を示す
- ノートを別カテゴリへ移すと履歴は追随せず、元の場所に孤立する
- ローテーションは原子的でない。落ちれば番号が欠けることがあるが、md は無傷
- 履歴は md と同じ大きさで最大 5 倍のディスクを使う。圧縮しない
- 外部で md を書き換えた場合、その版も次の保存で `1.md` に入る（「ファイルにある本文」を写すため）。編集前の本文とは限らない
- 戻す UI は無い。`data/.history` を開く手作業だけ

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 時刻付きの名前（`20260910-2203.md`） | 時刻を読むのは adapters だけ（ARC-007）で、application のテストが決定的でなくなる。連番で足りる |
| 履歴を書けなくても保存する（警告だけ） | 保険が効かない状態で自動保存が走る。「書けなければ状態を変えない」の流儀を守る |
| 保存の直前ではなく編集を始めたときに写す | 編集中に外部で md が変わると差分が飛ぶ。書き戻す直前の「ファイルにある本文」を写す方が確実 |
| `<ノート>.md.1` のように md と同じディレクトリに置く | ノートの走査に紛れ、`data/<カテゴリ>/` が散らかる。`.history` に隔離する |
| ローテーションの計画を core の純関数にする | 順序は固定で判断が無い。深さの定数だけを core に置く |
| 移動時に履歴も移す | 移動先に古い履歴が残っていたときの扱いと失敗の伝え方が増える。戻す UI と一緒に決める |
| `name_list` で `.` 始まりを拒む | 台帳としては妥当な名前。走査で拾わないだけにして規則を増やさない |

## 参考

- [ADR 0006](0006-edit-mode-and-save-path.md)（保存の経路）/ [ADR 0008](0008-cross-category-note-move.md)（移動と台帳の追随）
- [MoveFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw)
