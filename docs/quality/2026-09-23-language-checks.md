# 表示言語（#76 / ADR 0032）の確認記録

2026-09-23。実装は `out/worktrees/76-language`（枝 `feat/76-language`）。
実測は `out/design/2026-09-23/language-ui-probe/`（`out/` は追跡しないのでリポジトリには入らない）。
設計の材料になった読み取り専用の調査は `out/design/2026-09-23/language-probe/`。

🔴 **簡体字の訳は母語話者の確認を得ていない。** 語彙・用語の統一・敬体は実装リナが揃えたが、
自然さと誤訳の有無は未検証である。後日 `src/core/ui_text.c` の**3 列目だけ**を直せる
（ID・置換子・閉じた語彙は変わらないので、単体と CNF-011 がその差し替えを守る）。

🔴 **描いた絵そのものは見ていない。** 豆腐・字形の混在・実際の読みやすさは
[統合チェックリスト](2026-09-22-visual-checklist.md)の実機目視が残る。

---

## 1. 用語の統一表

3 列の訳はこの対応で揃えた。**同じ概念に同じ語**を使い、揺れは作らない。

| 日本語 | English | 简体中文 |
| --- | --- | --- |
| ノート | note | 笔记 |
| カテゴリ | category | 分类 |
| 索引 | index | 索引 |
| 本文 | body | 正文 |
| 台帳 | ledger | 台账 |
| 閲覧 | View | 查看 |
| 編集 | Edit | 编辑 |
| 保存 | Save | 保存 |
| 別名で保存 | Save as | 另存为 |
| 名前を変更 | Rename | 重命名 |
| 検索（ノート内） | Find | 查找 |
| 検索（全ノート） | Search | 搜索 |
| 絞り込み | filter | 筛选 |
| 置換 | Replace | 替换 |
| パターン | pattern | 模式 |
| 設定 | Settings | 设置 |
| テーマ | Theme | 主题 |
| OS に従う | Follow the OS | 跟随系统 |
| ライト / ダーク | Light / Dark | 浅色 / 深色 |
| 言語 | Language | 语言 |
| 行番号 | line numbers | 行号 |
| 一覧（パレット） | List | 列表 |
| 操作 | command / Actions | 操作 |
| 欄 | box | 栏 |
| 記憶域が足りません | Out of memory | 内存不足 |
| 全区画 | Anywhere | 所有区域 |
| 並列の区切り `・` | `,` | `、` |

言語の名前（`日本語` / `English` / `简体中文`）は**各言語の自称**なので 3 列とも同じ文字列である。
製品名 `NeNe Folio` と `NENE FOLIO`、`{k}` などの置換子、`data/…` のパス、`.md` は翻訳しない。

## 2. 日本語の字面を変えた 2 件

ADR 0032 の決定 3 のとおり、**構文の見本だけを ASCII に揃えた**（括弧の外の日本語はそのまま）。

| ID | 変更前 | 変更後 |
| --- | --- | --- |
| `UI_TEXT_HELP_EX_SUBSTITUTE` | `Ex  :%s/パターン/置換/[g]（g なしは…）` | `Ex  :%s/pattern/replacement/[g]（g なしは…）` |
| `UI_TEXT_COMMAND_SUBSTITUTE` | `正規表現で置換（:%s/前/後/g）` | `正規表現で置換（:%s/pattern/replacement/g）` |

`tests/unit/ui_text_tests.c` の同一性の期待表もこの 2 件だけ更新した（ほかの 129 行は不変）。
この単位で新しく足した ID は 5 つ（`HELP_EX_SET_LANGUAGE`・`SETTINGS_LANGUAGE` と 3 つの自称）で、
表は 131 → 136 値になった。

測りながら英訳を 1 件短くした: `UI_TEXT_HELP_EX_SET_NUMBER` の en を
`(line numbers while editing)` → `(line numbers in edit)`（426px → 384px。箱は 428px）。

## 3. 帯幅（Win32 部品 probe・96 DPI）

欄の中に出る文言（`HELP_*`・`STATUS_*`・`COMMAND_*`・`SETTINGS_*`・`CHIP_*`・`ACTION_*`・
`inline_outcome` が真の `FAILURE_*` 14 値）を実際の書体で測った。

| 言語 | あふれ | 最長 | その ID | 箱 | 余裕 |
| --- | --- | --- | --- | --- | --- |
| ja | **0** | 412px | `UI_TEXT_HELP_FILE_KEYS` | 428px | 16px |
| en | **0** | 420px | `UI_TEXT_FAILURE_SETTINGS_UNREADABLE` | 444px | 24px |
| zh-Hans | **0** | 406px | `UI_TEXT_HELP_FILE_KEYS` | 428px | 22px |

- 札（「閲覧」「編集」→ View / Edit・查看 / 编辑）は en で 96 → **104px** に太るが、
  560 幅でパンくずに残るのは 102px で、最小 48px（ADR 0011 の決定 4）を割らない。
- 名前入力面（言語ごとの face の 14px・幅 360px）は 3 言語ともあふれず、
  `PROMPT_PENDING_EXPLANATION` は **ja 2 行 / en 1 行 / zh-Hans 1 行**で既存の 60px に収まる。
- **箱で出る `FAILURE_*` は対象外**（`MessageBoxW` が折り返す）。ja の 3 件が 444px を超えている
  現状の欠陥（`LEDGER_STALE` ほか）はこの単位では直していない。

## 4. 書体の差し替え（Win32 部品 probe）

| 測ったこと | 結果 |
| --- | --- |
| 編集の face 差し替え（`tomSuspend` + `SCF_DEFAULT`）で本文・選択・Undo・変更印 | **すべて不変**。既定書式の face だけが変わる |
| ja ↔ zh の face でこの本文・この幅の折り返し | 変わらなかった（`display 30 → 30`） |
| 折り返しが本当に変わる対（Consolas 181 行 → Yu Gothic UI 121 行） | **RichEdit 自身が先頭の内容を保ち**、論理行は 11 のまま。復元は空振り |
| 再着色（`SCF_ALL` + `SCF_DEFAULT`）でキャレットが画面の外 | 論理行が **16 → 1 へ飛び**、同じ包みで **16 に戻った** |
| 閲覧（新しい fonttbl の RTF を流し直す） | 論理行 26 → 26・選択 40..48 が戻る |
| EDIT の `WM_SETFONT` | 本文・選択・Undo・変更印が不変 |
| face の実在（`EnumFontFamiliesExW`） | `Yu Gothic UI` / `Microsoft YaHei UI` はあり、無い face は 0 件で返る |

**論理行の包みが本当に効くのは再着色の側**だった（face の差し替えでは安全網）。
ADR 0031 の 2026-09-23 の補正 2 はこの実測にもとづく。

## 5. 設定画面 8 行（`geometry.py`）

| DPI | 箱が欲しい高さ | 使える高さ | 見える行 |
| --- | --- | --- | --- |
| 96（560×360） | 400px | 316px | **5** |
| 144（150%） | 600px | 474px | **5** |

8 行は最小寸法ではどちらも全部入らないので、一覧はスクロールで辿る（ADR 0032 の決定 7）。
`reveal_command_selection` は 6 つの選択肢のどれでも箱の中へ送り出し（行 7 で `first=3`）、
`settings_next_choice` は言語の見出し（行 4）を飛ばす（3 → 5・5 → 3）。端では動かない。

**probe は 69 項目すべて成功**（width 15・reface 16・geometry 38。`failures=0`）。

## 6. 機械が守るもの

| 何を | どこが |
| --- | --- |
| 全 ID × 3 列が埋まっている（訳し忘れ・空の列） | **CNF-011**（`eng/conformance.py`。実ツール反例 P23） |
| 言語の値と `language_names[]` / `faces[]` の対応 | CNF-009 の `lineTables` 2 行 |
| 閉じた語彙（Ex の別名・`:set` の語・鍵の名前）が訳されない | `ui_text_tests.c`（ja にある語彙は en と zh-Hans にも要る） |
| 表の外に文言を直書きしない | CNF-010 / C-018 |
| 設定画面の段が増えたら採用の分岐を書かせる | `enum settings_row_kind` の閉じた switch（C-002） |
| `folio_option` に語を足したら `execute_set_command` が落ちる | `-Wswitch-enum`（C-002） |

## 7. 実行した検証

| 何を | 結果 |
| --- | --- |
| `pwsh -NoProfile -File ./eng/check.ps1`（最終 HEAD） | #76 の PR を参照 |
| `ctest`（単体） | 2/2 成功 |
| `eng/conformance.py` / `eng/test-conformance.py` | 0 件 / 89 テスト成功 |
| `eng/prove-gates.py` | 17 本（CNF-011 の反例を含む） |
| Win32 部品 probe | 69 項目成功 |
| 実機の目視 | **未実施**（統合チェックリスト） |
