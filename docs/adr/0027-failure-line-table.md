# ADR 0027 — 失敗の文言は値ごとの表から引き、表の網羅は字句検査（CNF-009）が守る

- 状態: 受理（設計リナ 2026-09-22。実装前に記録）
- 日付: 2026-09-22
- Issue: #65（#41 と #38 の前提）
- 規則: C-002 / C-012 / QLT-010 / CNF-006 / CNF-007、ADR 0001

## 文脈

`enum folio_state_outcome` は閉じた結果型で、失敗のたびに利用者へ見せる 1 行は `folio_state_failure_line` が返す。
いまは full-enum の switch 2 つ（`folio_state_failure_line` と `unfinished_failure_line`）に分けて文言を返しており、
どちらも C-012 の 60 行に対して 59 行で飽和した（#39 の独立レビュー A-8）。値を 1 つ足せば落ちる。
#41（置換）と #38（設定・テーマ・言語）は値を足す。

switch は `-Wswitch-enum` で「値が増えたらコンパイルが落ちる」を与えるが（C-002）、1 値につき最低 2 行（`case` と `return`）を使うので、
値の数 × 2 が 60 を超えたら関数を分ける以外に無く、分けた各関数も全値を列挙しなければならない（分割の効果は 1 値 1 行の委譲分だけ）。
値が 33 の時点で既に 2 関数に割れており、この形は伸びない。

## 決定

1. **文言は値ごとの表から引く。** `folio_state.c` に `static const char *_Nonnull const failure_lines[]` を指示付き初期化子
   （`[FOLIO_STATE_X] = "…"`）で置き、`folio_state_failure_line` は表を引いて返す。`unfinished_failure_line` は無くす。
   文言は 1 字も変えない。`READY` の行は空のまま。
2. **表が列挙の全値を持つことは `eng/conformance.py` の新しい検査 CNF-009 が守る。** 設定は `eng/conformance-rules.json` の
   `lineTables`（列挙のヘッダと表のファイルの対）で名指しする。検査は列挙の本体から `FOLIO_STATE_\w+` の値を字句で集め、
   表のファイルに `[値] =` が 1 回ずつ現れることを求める（無い値・重複した値は違反）。検査器のソースに検出語を直書きしない。
   `eng/test-conformance.py` に正例・反例、`eng/prove-gates.py` に「表から 1 値を消すと落ち、戻すと通る」の実ツールの反例を足す。
3. **表の外の値には落ちない。** 表の長さは列挙の最大値 + 1 で、CNF-009 が全値の存在を保証するので、添字の範囲検査は書かない
   （到達しない分岐を置かない）。`switch` の外の到達不能な `return` と同じ扱い。
4. `docs/QUALITY_GATES.md` に CNF-009 を **active** として書き、強制マトリクスを更新する（CNF-006 が行の欠落を拒む）。
   閾値・除外・重大度は触らない（QLT-010）。
5. 他の switch（UI の `command_failure` などの振り分け）は変えない。C-002 の網羅性はそこで従来どおり効く。

## 却下した選択肢

- 関数をさらに分ける（家族ごとの switch）: `-Wswitch-enum` の下では各関数が全値を列挙するので、1 値ごとに全関数が 1 行ずつ伸びる。値が 48 前後でまた飽和する。
- X-macro で列挙と表を同じ一覧から生成する: 生成漏れはコンパイルで落ちるが、ヘッダの列挙が読めなくなり、値ごとのコメントとエディタの補完を失う。
- 表 + 実行時の範囲検査 + 単体テスト: テストは列挙を数え上げられないので、末尾に足した値の欠落を機械では検出できず、人の記憶に頼る（ADR 0001 に反する）。
- `.clang-format` の `AllowShortCaseLabelsOnASingleLine` で switch を 1 値 1 行に詰める: 整形設定の変更で行数検査をすり抜けるのは QLT-010 の趣旨に反する。
- 文言をリソース（文字列表）ファイルへ外出しする: #38 の多言語化で改めて決める。この単位で経路を増やさない。

## 検証

単体: 全値の文言が変更前と同じ（変更前の値を固定した表との比較。`READY` は空）。
`python eng/test-conformance.py` の正例・反例、`python eng/prove-gates.py` の実ツール反例、`pwsh -NoProfile -File ./eng/check.ps1`。Waivers: none。

## 2026-10-06 の補正（#168）

決定 5 は「他の switch は変えない」としたが、ui の 2 か所を同じ形の表引きへ広げる。

1. **表引きを ui の 2 か所へ広げる。** `src/ui/win32/folio_window.c` の (a) コマンドの実行 `execute_command` は
   `[FOLIO_COMMAND_X] = 関数` の関数の表 `command_runs[]` を 1 回引き、(b) 失敗を欄の 1 行にするか箱にするかの
   `inline_outcome` は `[FOLIO_STATE_X] = true | false` の表 `inline_outcomes[]` を引く。(b) は `false` の値も省かず全値を書く。
   表の関数はすべて「窓と引数」の同じ形で受け、引数を使わない操作は引数を捨てる。行き先は表へ移す前の switch と 1 対 1 で同じ。
2. **理由。** `main` の `0a99ea5` で `execute_command` が 59 行・`inline_outcome` が 57 行・`folio_window_create` が 60 行と
   C-012 の 60 行に飽和しており（#168 の実測）、次の単位（ノートの削除・カテゴリの作成／削除／改名）はコマンドと結果の値を足す。
   switch は 1 値に 2〜3 行を使うので、文脈の節と同じ理由で伸びない。`folio_window_create` はアクセラレータの表を作る
   かたまりを関数へ出して空ける（初期化の順と失敗時の後始末は変えない）。
3. **網羅は CNF-009 が守る。** `eng/conformance-rules.json` の `lineTables` に `src/core/folio_command.h` →
   `src/ui/win32/folio_window.c`（`FOLIO_COMMAND_`）と `src/application/folio_state_outcome.h` → `src/ui/win32/folio_window.c`
   （`FOLIO_STATE_`）の 2 行を足す。switch を離れて失う `-Wswitch-enum` の網羅性（C-002）の代わりで、
   値を足して表の行を書き忘れると落ちる。2 表とも「1 値を消すと CNF-009 で落ち、戻すと通る」を実測した。
4. **表の外の値には落ちない。** 決定 3 と同じく、添字の範囲検査は書かない。
5. **振る舞いの変化は 0。** 18 のコマンドの行き先と 45 値の分類（欄の 1 行が 16・箱が 29）は変更前と同じで、
   文言・スキーマも変えない。閾値・除外・重大度は触らない（QLT-010）。
