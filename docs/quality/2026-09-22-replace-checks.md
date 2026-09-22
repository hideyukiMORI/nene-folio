# 正規表現置換の確認記録（Issue #41 / ADR 0028）

2026-09-22。ゲート内の単体と、ゲート外の測定でそれぞれ何を守ったかを分ける。
「実機で見た」と「値として確かめた」を混ぜない。**実機の目視はまだ 1 項目も行っていない。**

測定機の版: **ICU 72.1.0.4 / Unicode 15.1**（`icu.dll` の `u_getVersion` / `u_getUnicodeVersion` と
ファイル版 `72, 1, 0, 4`）。**ICU の版は OS が決めるので固定できない**（QLT-011 は OS 同梱に適用しない・
ADR 0028 の決定 1）。Windows の更新で folding や Unicode 属性の挙動が動きうる。

## ゲート内（`pwsh -NoProfile -File ./eng/check.ps1` が守るもの）

### core（`tests/unit/replace_tests.c`）

- `replace_template` の全記号: `&` と `\0`（一致全体）、`\1`〜`\9`（群・参加しなかった群は空）、
  `\r` と `\n`（CR 1 つ）、`\\` / `\&` / `\/`、`$` は普通の文字。
- 拒む形: 未知の `\x`、末尾の単独の `\`（どちらも `MALFORMED`。黙って消さない）。
- `note_replace` の組み立て: 群の欠落・ゼロ幅の一致・末尾の一致・CR の挿入・必要長と `note_replace_limit`。
  数える → 呼び出し側が確保 → 書く、の 2 段が同じ長さになること。
- 論理行ごとの最初の一致の選別（`REPLACE_LINE_FIRST`）。
- `regex_match_limit`（1,000,000）を超えた走査は `TOO_MANY`。
- `folio_command_parse_substitute` の分解: `\/` を区切りにしない、`g` の有無、空のパターン、
  区切りの不足、`%s` の直後が英数字（別名の照合を壊していないこと）。

### application（偽 adapter・`tests/unit/replace_state_tests.c` / `state_tests.c` / `allocation_tests.c`）

- 編集中でない（`NOT_EDITING`）・空パターン（`REPLACE_NO_PATTERN`）。
- 走査と template の各失敗値の写像（`BAD_PATTERN` / `BAD_TEMPLATE` / `TIMED_OUT` / `TOO_COMPLEX` /
  `TOO_MANY` / `TOO_LARGE`）。失敗のときは**前の下見をそのまま保つ**。
- 件数（ゼロ幅の一致も 1 件）と `folio_state_replace_error_offset`（`BAD_PATTERN` のときだけ非 0）。
- `REPLACE_ONE` の anchor からの前進と、無ければ先頭からの巡回。反転した anchor は `REPLACE_BAD_SPAN`。
- `REPLACE_STALE`: 本文が違う場合と、宛先（カテゴリ名・ノート名）が違う場合の両方。
- 下見の入れ替え（新しい下見が古い下見を捨てる）。
- `folio_state_create` の 4 引数化（`main.c` と全テストの呼び出しが揃っていること）。
- `folio_state_outcome` の 9 値を `failure_lines[]` と `state_tests.c` の期待表の 2 か所に足した（CNF-009）。
- 確保失敗は `allocation_tests` の `replace_scenario` / `replace_under_probe` が、本文の写し・一致の列・
  template・組み立ての出力の確保を 1 回ずつ失敗させて `OUT_OF_MEMORY` で片付くことを確かめる。

### ui

- `note_pane_replace` と置換の欄はゲート内の単体を持たない（Win32 部品なので下の probe で測る）。
  `COMMAND_SURFACE_REPLACE` を足したことで `-Wswitch-enum` が `command_surface` の全 switch を
  網羅させ、漏れがあればコンパイルが落ちる（C-002）。

## ゲート外 1: 実アダプタ probe（`out/design/2026-09-22/replace-adapter-probe/`）

製品の `src/adapters/win32/regex_adapter.c` を**そのままコンパイル**して、ポート越しに測る。
`build.ps1` → `regex_adapter_probe.exe`。**32 項目すべて成功**（`regex_adapter_probe.log`）。

`llvm-nm --undefined-only regex_adapter.obj` が残す ICU の名は
`uregex_open` / `uregex_setText` / `uregex_setTimeLimit` / `uregex_findNext` / `uregex_start` /
`uregex_end` / `uregex_groupCount` / `uregex_close` の 8 つだけ（`eng/symbols.py` と同じ道具）。

| 項目 | 測った値 |
| --- | --- |
| 旗: `\q`（未知のエスケープ） | `BAD_PATTERN` と位置 2。`UREGEX_ERROR_ON_UNKNOWN_ESCAPES` が効いている |
| 旗: CR の行端 | `^` / `$` は `a\rb\rc` に 3 件ずつ。`.` は CR に一致しない（`UNIX_LINES` を付けていない） |
| 旗: 利用者の `\n` | `a\rb` に 0 件。`\r` は 1 件 |
| 旗: `(?i)` | インラインで効く。`Ａ` と `ａ` が一致する（full case folding） |
| サロゲート | `B` の位置は 4（UTF-16 コード単位）。`.` は絵文字 1 個に 1 件で、範囲はペア全体（割れない） |
| 群 | 10 個のパターンも受け、報告は 1〜9 の 9 個。群 9 は 8..9。参加しなかった群は `present` が偽 |
| 壊れたパターンの位置 | `(` → 1、`[` → 1、`*` → 1、`a{3,1}` → 6、`[z-a]` → 4、`\p{Nonexistent}` → 15（1 起算） |
| 成功した走査 | 位置は 0 に戻る（前の失敗の値を持ち越さない） |
| `TIMED_OUT` | `(a+)+b` on `a`×40 + `c` → `TIMED_OUT`（536.1 ms。2,000 steps） |
| `TOO_MANY` | `b*` on `a`×1,100,000 → `TOO_MANY`（73.1 ms。1,000,000 で打ち切る） |
| 数えるだけ | `capacity = 0` でも `count` に総数（4）。`capacity = 2` なら入るぶんだけ書いて `count` は総数 |
| 長さ 0 の本文 | 0 件で成功（`NULL` は渡していない） |
| 長さ 0 のパターン | **`BAD_PATTERN`**（下の「限界」参照） |
| ゼロ幅 | `x*` on `axxb` は 4 件 |
| 1MB（620,000 コード単位）の 1 回の scan（`uregex_open` 込み） | literal `ノート` 10,878 件 **4.32 ms** / `\w+` 108,773 件 **21.32 ms** / `(\w+)-(\w+)` 10,877 件 **84.75 ms** / `^` 10,878 件 **4.42 ms** |

## ゲート外 2: Win32 部品 probe（`out/design/2026-09-22/replace-ui-probe/`）

製品コードは参照せず、画面外 (-4000,-4000) の窓で `RICHEDIT50W` と `EDIT` の振る舞いだけを測る。
**32 項目すべて成功**（`replace-ui-probe.log`）。

| 項目 | 測った値 |
| --- | --- |
| `EM_EXSETSEL` → `EM_REPLACESEL(TRUE)` 1 回 | `EN_CHANGE` は **1 回**。`EM_CANUNDO` が真になり、`WM_UNDO` 1 回で置換前の本文へ戻る |
| 本文全体の差し替え（全置換） | `EN_CHANGE` は **1 回**で、`WM_UNDO` 1 回で戻る |
| 置換文字列の CR | CR 1 つを入れると表示本文（`GT_DEFAULT`）でも CR 1 つ・LF 0（位置が `EM_EXSETSEL` と 1 対 1）。CRLF を入れても CR 1 つに畳まれる |
| 置換後の選択 | `EM_REPLACESEL` の直後は挿入文字列の直後に潰れた選択（`2..2`）。製品が置き直す `span.start + 長さ` と同じ位置 |
| 全置換後の論理行 | スクロールして 78 行目 → 差し替え直後は 1 行目 → `EM_EXLINEFROMCHAR` + `EM_LINESCROLL` で **78 行目へ戻る** |
| 2 つの EDIT | control id 4 / 5 で作れる。開いた欄はパターンの側がフォーカスを受ける |
| `Tab` | パターン ⇄ 置換文字列を往復する（自前で処理し、EDIT には渡さない） |
| `WM_KILLFOCUS` の相手 | 集合の中のもう 1 つの欄。**`dismiss` の集合判定はここで閉じない**。本文の RichEdit は集合の外 |
| 素の `Enter` | `WM_CHAR` **0x0D**（1 件） |
| `Ctrl+Enter` | `WM_CHAR` **0x0A**（`GetKeyState` を読まずに見分けられる・ARC-007） |
| `Enter` / `Ctrl+Enter` の飲み込み | 欄の本文には入らない |
| `Esc` | 覚えた HWND へフォーカスを返せる |
| 96 DPI・560×360 のパレットの箱 | 使える高さ 316。ヘルプ 16 行でも 18 行でも箱の中に収まる（上端 60 → 32）。**どちらも操作の行は 0 行**。キー操作を閉じている既定では **5 行**見える |

## 実機の目視に残すもの（hide）

probe はすべて画面外の窓で値だけを測っており、**描いた絵は 1 度も見ていない**。次を実機で見てほしい。

1. **置換の欄の見た目**（状態の 1 行・2 つの欄の左右の揃い・「1 件」「すべて」の位置と当たり判定）。
2. 両テーマ（ライト／ダーク）での欄の色と、状態の 1 行が長いとき（失敗の 1 行 + 「位置 N」）の収まり。
3. **IME 変換中の Enter**（変換の確定に食われて置換が走らないこと）と、確定後に 1 回だけ数え直すこと。
4. **1MB のノートでの打鍵の体感**（1 打鍵ごとに `uregex_open` からやり直す。値では 4〜85 ms）。
5. 高 DPI（120 / 144 / 168）での 2 つの欄とボタンの寸法。
6. **最小寸法 560×360** で置換の欄が本文を隠しすぎないか（Ex より 1 行ぶん高い）。
7. パレットと「操作」メニューに「置換」が 1 つだけ出ること（`:%s` は出ないこと）。
8. Ex `:%s/…/…/` を打ったときに欄が開かず、成功なら黙って閉じ、0 件なら「対象名 / 0 件」が残ること。
9. 全置換の直後に**読んでいた場所が戻っている**こと（値では戻るが、見た目の跳ねは見ていない）。
10. #39 までの未確認項目（`docs/quality/2026-09-17-number-checks.md` の末尾 11 項目）も引き続き残っている。

## 限界（この単位では測っていない・やっていない）

- **`TOO_COMPLEX`（`U_REGEX_STACK_OVERFLOW`）は測っていない。** 既定の 8MB スタックと 2,000 steps では
  時間の上限が先に効く（ADR 0028 の決定 4(a) の予測どおり）。写像そのものは偽 adapter の単体で覆っている。
- **長さ 0 のパターンは adapter で `BAD_PATTERN` になる。** 設計 probe（`replace-probe`）は
  `uregex_open(pattern, -1, 旗 0)` で「文字数 + 1 件」だったが、製品の adapter は長さを明示して渡すので
  ICU が拒む。application が先に `REPLACE_NO_PATTERN` で断るので**製品では走らない経路**である。
- **steps の上限は 1 回の一致操作ごとに数え直される**ので、全走査の最悪は「一致の数 × 上限」まで膨らむ。
  測ったのは単発の暴走パターンだけで、「一致 1 件ごとに上限近くまで使うパターン」は測っていない。
- **ICU 内部の確保失敗は測れない**（`u_setMemoryFunctions` はプロセス全体で `u_init` の前にしか
  差し替えられない。QLT-009 の「置いていない層」）。`uregex_open` の確保失敗も測っていない。
- **描いた絵そのものを 1 度も検査していない。** 欄の字形・色・位置は実機の目視に依る。
- Win32 部品 probe は製品の `folio_window.c` ではなく**同じメッセージ列を自分で並べた模型**である。
  「製品が実際にその順で送っている」ことは、コードの読みとゲート内のコンパイルでしか確かめていない。
- **IME 変換中の振る舞いは測っていない**（`WM_IME_*` を probe で作っていない）。実機の目視に残す。
- **1MB の本文での「打鍵から画面が更新されるまで」は測っていない**（scan の時間だけを測った）。
- `:s` の行範囲・確認付き置換（`c`）・全ノートへの一括置換は**この単位の範囲外**（後続）。
- パターンと置換文字列の永続化は無い（起動のたびに空から始まる）。
