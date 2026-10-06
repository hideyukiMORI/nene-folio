# カテゴリの作成の確認記録（Issue #169 単位 A / FR-034 / ADR 0039・ADR 0022 の補正）

2026-10-06 の夜から 2026-10-07 の未明にかけて統合した単位 A（#172 / #176 / #174 / #177 / #180 / #182）で、何を守り、何を測り、何を見たかを分ける。
「実機で見た」「値として確かめた」「まだ見ていない」を混ぜない。**hide の目視はまだ 1 項目も無い**（設計席の絵はある）。

## 単体（ゲート内・`pwsh -NoProfile -File ./eng/check.ps1` が守るもの）

| Issue | PR | main | 守るもの | フルゲート |
| --- | --- | --- | --- | --- |
| #172 | #175 | `8f7894d` | `category_name_create` / `_text` / `_destroy`（`tests/unit/category_name_tests.c`: 拒否 20 例・受理 11 例ほか。結果は ACCEPTED / INVALID / OUT_OF_MEMORY・空も INVALID）・`category_ledger_inserted`・確保失敗の場面 | exit 0・分岐 2820/3020（93.38%）・実ツール反例 19 本 |
| #176 | #178 | `1a38919` | 結果の値 4 つ（`CATEGORY_NAME_INVALID` / `CATEGORY_NAME_TAKEN` / `CATEGORY_NOT_CREATED` / `CATEGORY_LEDGER_STALE`）と文言の 4 か所の表（`failure_lines[]`・`state_tests.c` の期待表・`inline_outcomes[]`・`ui_text` の 3 列。CNF-009 / CNF-011）。`FILTERED` の文言の 3 言語での一般化（`state_tests.c` 2 か所と `ui_text_tests.c` 1 か所を直した） | exit 0（席の HEAD `672140a` と、設計席の 2 回） |
| #174 | #179 | `03bded5` | 偽ポートの `category_create_outcome` / `category_creates` / `created_category`・adapter の `acquire_lock` と共有する `open_lock` と `journal_absent`（既存の起動時の経路を壊さないこと） | exit 0・分岐 2820/3020（93.38%） |
| #177 | #181 | `a6b1a0a` | `folio_state_create_category` の単体 5 場面（FILTERED → 同期 → 大小を畳んだ重複 → `reserve_category` → port → 台帳 → `adopt_category`）・確保失敗の場面 | exit 0・分岐 2855/3058（93.36%） |
| #180 | #183 | `872e340` | 名前入力面の種別 `NAME_PROMPT_NEW_CATEGORY`・閉じた述語 4 つ（`has_category_list` / `prompt_name_label` / `closes_on` / `holds_pending`）の全値 switch・文言 4 本・既存 3 種別の保留経路が現行のままであること | exit 0・分岐 2855/3058（93.36%） |
| #182 | #184 | `5bc5ac5` | `FOLIO_COMMAND_NEW_CATEGORY` の登録表（`newcategory` / `newcat`・件数 18→19・一覧 16→17）・`execute_new_category_command` | exit 0・分岐 2859/3062（93.37%）・CI は check / full-gate とも成功（full-gate 2 分 5 秒） |

描画・鍵・流し込みは測定の対象外で、下の「未確認」の項目が残る。English の 2 本の文言は既存の最長（`INVALID_NAME` 131 字）を超えたため 133 字・116 字へ縮めた（ADR 0039 補正 1）。

## 実アダプタの測定（ゲート外・`D:\NeNeFolio\design\2026-10-07\create-category-probe\`）

測った HEAD は `9f453f0`（`feat/174-create-category-port`）。probe exe の隣に専用の `data/` を場面ごとに作り（1 場面 1 プロセス・錠はプロセスが持つため）、実ファイルで測った。
製品コードは触っていない。環境は Windows 11 10.0.26300.0・D: は NTFS。**PASS 16 / FAIL 0。**
原本は `probe.c`（SHA-256 `6929e367307c3a3e316f1b4d05398143e039eb4fbfc94a6ad475b03f31762749`）、実行物は `probe.exe`（`3549ac56bdd71c5c95c0e0fdb2602543fcde7f3d48bddf7e204303e47b3285cf`）、
ログは `run.log` / `run8c.log`・`results.txt`。Sonnet 席（22 回の道具呼び出し・100K tokens・約 3 分）が測り、設計席が `results.txt` を照合して拒否 ACE が残っていないことを確かめた。

| 区分 | 測ったこと | 結果 |
| --- | --- | --- |
| 作成 | `data/` 無しで `create_category` が STORED。`data/` と `data/<名>/` ができ、adapter が錠を持つ。続く 2 回目も STORED | 2/2 |
| 重複 | 同名・ASCII の大小違い（`abc` と `ABC`）・同名のファイルが在る、のどれも NAME_TAKEN で既存は変わらない | 3/3 |
| 記録・錠 | 生成後に外から `.rename.json` を置くと UNWRITABLE で作らない（錠は持ったまま）。別ハンドルが錠を保持していると UNWRITABLE | 2/2 |
| 書けない場所 | 実行フォルダへ書込拒否 ACE（`data/` 無し）と、`data/` への WD 拒否 ACE（錠を作れない）のどちらも UNWRITABLE で何も作られない。ACE は外した | 2/2 |
| 起動時から錠 | 起動時から錠を持つとき、後から置いた `.rename.json` は確認されず STORED（契約どおり。良し悪しは判断しない） | 1/1 |
| 名前の端・長いパス | 255 バイトの UTF-8（U+3042 × 85）と 255 文字の ASCII は STORED。`data/` までの全長が 260 超でも STORED | 3/3 |
| 他のメンバー | 作成後に `scan_categories` が新しい名前を返し、`read_note_ledger` は ABSENT。`categories.json` と `index.json` は作られない | 2/2 |

所要時間は 1 回目（`data/` と錠を作る）0.284 ms・2 回目 0.093 ms（各 1 回の計測）。

### 未測定

- ASCII の大小を区別するディレクトリの環境（`ABC` と `abc` の判定は NTFS の既定の大小無視に依存している）。
- 実行ファイル自体のパスが 260 を超える場所（`CreateProcess` が許さない）。実行場所は 222 文字に留め、`data/` までの全長だけ 260 超にした。
- 別プロセスからの錠の確認は、同一プロセスの別ハンドルで代替した。

## 設計席の実機確認（ゲート外・設計リナ 2026-10-07）

#184 の枝の exe（HEAD `1cb8fae`・3,017,728 B）を `D:\NeNeFolio\check\2026-10-07\empty\` へ写して実行し、`PostMessage` と `tools/capture-window.ps1 -TargetPid` で撮った。
画面は 125%・ダーク。絵は `D:\NeNeFolio\check\2026-10-07\shots\`。

| 見たこと | 絵 |
| --- | --- |
| `data/` が無い状態で起動 | `e00.png` |
| パレットで「新しいカテゴリ」 | `e01.png` |
| 名前入力面（題・見出し「カテゴリの名前」・欄・案内・「作成」「キャンセル」・一覧なし） | `e02.png` |
| 空の名前・`.git` → 失敗の 1 行「使えないカテゴリ名です。…」が 3 行で出て面が残る。`data/` は作られない | `e03.png` / `e04.png` |
| 「仕事」で作成 → 索引に「01 仕事」・カーソルが乗る・フォーカスは主窓。ディスクは `data/`・`data/仕事/`・`.nenefolio.lock`・`categories.json`（既定の色 `#7F8FA6`・展開）で `index.json` は無い | `e06.png` |
| `:enew` →「hello」→ Ctrl+S →「名前をつけて保存」の面の一覧に「仕事」（既存の面は変わらない）→「メモ」で `data/仕事/メモ.md`（5 B）と `index.json` | `e07.png` / `e08.png` |
| 編集中に未保存の字「XYZ」を足してからパレットで「読書」を作成 → 本文は「XYZhello」のまま・編集モードのまま・選択は「メモ」のまま・カーソルは「03 読書」。ディスクの md は「hello」のまま（保存は走っていない） | `e11.png` |
| `:newcategory 趣味`・`:newcat Shigoto` は作成。`:newcat shigoto`（大小だけ違う）→ 失敗の箱「同じ名前のカテゴリがあります。別の名前を指定してください。何も変えていません。」。箱を閉じた後も入力が効く | `e13.png` / `e14.png` |
| English（失敗の 1 行 133 字が 3 行で欄に収まる） | `f01-en.png` |
| 简体中文 | `f02-zh.png` |
| ライト（面の中で同名「仕事」→ 失敗の 1 行が出て面が残る／主窓） | `f03-light.png` / `f04-light-main.png` |

手順と期待はチェックリストの 182-1〜182-9（[統合チェックリスト](2026-09-22-visual-checklist.md)）。

## 未確認・hide の目視に残るもの

- **絞り込み中の拒否**: Ctrl+Shift+F を投げる手段が無かった。application の単体では固定済み（182-10）。
- **`CATEGORY_LEDGER_STALE` の失敗の箱**（182-11）。
- **物理のキーボードとマウス**（設計席は `PostMessage` で見た）（182-12）。
- **96 / 144 DPI**（設計席の絵は 125% だけ）（182-13）。
- English の失敗の箱（幅 480）で 2 行に収まるか: 既存の最長でしか測っていない。🔴 简体中文は母語話者の確認を得ていない。
- 右クリックメニューからの入口は FR-038 の単位で足す（まだ無い）。

## 見つかった既存の欠陥（#182 が持ち込んだものではない）

- **#185**: パレットから開いた名前入力面をキャンセルすると、パレットが開いたままフォーカスが主窓へ落ちる。「名前を変更」で再現（`g01-rename-cancel.png`・`GetGUIThreadInfo` のフォーカスが `NeNeFolioWindow`）。
- **#186**: パレットの行の別名（Ex の名前）が右端で切れる（`rename` → `renam`・`newcategory` → `newcategor`）。
