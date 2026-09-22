# #74 文言の表引きの確認記録（ADR 0030） — 2026-09-22

Issue [#74](https://github.com/hideyukiMORI/nene-folio/issues/74)（ADR 0030・利用者に見える文言を core の
`ui_text` の表 1 か所に集める）で実際に走らせた検査の記録。**この単位は字面を 1 字も変えない**ので、
確認の中心は「同一性」である。実機の目視は[統合チェックリスト](2026-09-22-visual-checklist.md)の 74-1（1 行）。

- 変更前の基準: main `ba0e538`（この枝の分岐元）
- 道具: `out/design/2026-09-22/ui-text-probe/`（git 管理外）
- 環境: Windows 11 Pro 10.0.26200 / x64、clang-cl 19.1.5、Windows SDK 10.0.26100.0

---

## 1. 同一性（2 本）

**変更前のソースから機械で写した表**（`generate_catalog.py` → `emit_sources.py` / `emit_test.py`）と、
**逆向きの照合**（`verify_identity.py`）の 2 本で挟む。方向が逆なので、片方の取りこぼしがもう片方に出る。

| # | 何を見るか | 経路 | 結果 |
| --- | --- | --- | --- |
| 1-1 | 表の 123 値が変更前の文言と 1 字も違わない | `tests/unit/ui_text_tests.c` の期待表（`emit_test.py` が変更前のソースから生成）を ctest で回す | 成功（CTest 2/2） |
| 1-2 | 表の各文言を置換子で切った断片が、変更前のソースの文字列リテラルに現れる | `python verify_identity.py <変更前の木>` | `catalog ids: 123` / `pre-change literals (folded): 195` / `every catalog piece appears in the pre-change literals: OK`（exit 0） |

### 消える文言は 3 つ（ADR 0030 の補正 5）

`verify_identity.py` の逆向きの照合が「表のどこにも現れない変更前の非 ASCII リテラル」として 3 件を挙げた。
3 件とも**到達しない既定**か**確保に失敗したときだけ**なので、通常の表示は変わらない。

| 消えた文言 | もとの場所 | いつ出ていたか |
| --- | --- | --- |
| `このノート内を検索  ` | `folio_window.c` の `search_direction_label` の既定 | 全値の `switch` の後。到達しない |
| `実行ファイルの場所が取得できません。` | `main.c` の `adapter_failure` の既定 | 全値の `switch` の後。到達しない |
| `エラー表示の記憶域が不足しています。入力は残っています。` | `name_prompt.c` の `submit` の退避 | UTF-16 への変換で確保に失敗したときだけ（決定 5 で経路ごと消えた） |

## 2. 直書きが残っていないこと

`out/design/2026-09-22/ui-text-inventory/extract_strings.py`（棚卸しに使った抽出器）を変更後の木で走らせ、
`src/core/ui_text.c` 以外の非 ASCII のリテラルを数える。

| 時点 | `src/core/ui_text.c` 以外の非 ASCII リテラル |
| --- | --- |
| 変更前（main `ba0e538`） | 73 行（`folio_window.c` 43・`name_prompt.c` 20・`main.c` 9・`folio_state.c` 1。ほかに `drawer_window.c` の `L"\x2212"` 2 件がエスケープ） |
| このコミット列のあと | **0 行** |

## 3. Win32 部品 probe（`ui_text_probe.exe`）

単体が固定するのは「表の 1 行」で、その先（組み立て・UTF-16 の写し・字形の幅）はここで測る。
core の `ui_text.c` / `utf16_text.c` / `utf8_text.c` を**そのまま**コンパイルし、ui / app は参照しない。
窓もファイルも作らず、メモリ DC だけを使う。

- ビルド: `pwsh -NoProfile -File ./out/design/2026-09-22/ui-text-probe/build.ps1 <作業木>`
- 実行: `./ui_text_probe.exe > ui-text-probe.log`
- 結果: **58 passed, 0 failed**（exit 0）

| 節 | 測ったこと | 結果 |
| --- | --- | --- |
| 1 | 組み立てに使う表の 17 行が変更前の字面（末尾の 2 空白・`◀` `▶` `▾` を含む） | 17/17 |
| 2 | 組み立て 5 か所が変更前の連結と同じ完成形（`3 / 12 件`・`仕事-01 / 7 件`・`無題（未保存） / 0 件`・` / 0 件`（対象名なし）・` 位置 5`・`正規表現の書き方が違います。 位置 12`・`古い名前 → 新しい名前`・改名の理由 ＋ 空白 ＋ 説明・`このノート内を検索 ／ 次へ  1 / 1 件`）と、`{k}` という名前のノートが壊れないこと | 11/11 |
| 3 | W 系 API へ渡す UTF-16 が変更前のワイドのリテラルと `wcscmp` で同じ（`report` 5・名前入力面 14・絞り込み欄・ドロワーの頭・折畳印 2） | 24/24 |
| 4 | 折畳印の幅（Consolas -14 のメモリ DC）が変更前と同じ | `L"\x2212"` = 表の U+2212 = **cx 8 / cy 17**、`+` も一致。4/4 |
| 5 | 全 123 値が `ui_text_unit_limit`（256）に収まって UTF-16 へ写せる | 最長 **64 単位**（上限の 1/4）。2/2 |

## 4. ゲート（CNF-010 / C-018）

| # | 何を見るか | 結果 |
| --- | --- | --- |
| 4-1 | 反例: `folio_window.c` の末尾に非 ASCII のリテラルを 1 つ足す | 実 `eng/conformance.py` が `CNF-010: src/ui/win32/folio_window.c: line N: display text belongs in src/core/ui_text.c` で非 0（`eng/prove-gates.py` の P22） |
| 4-2 | 復帰: 戻す | `Conformance: 0 violation(s)` で 0。実ツール反例は 15 → **16 本** |
| 4-3 | 正例・反例の単体 | `tests/conformance` に 10 件（非 ASCII・ワイドのエスケープ・ナローの BOM のバイト列・文字リテラル・注記・許可ファイル・`tests/`・逃がした `\` の `U`・第 2 の表が無いこと）。69 → **79 テスト** |
| 4-4 | CNF-009 の `lineTables` を 2 行足す（`ui_text.h` → `ui_text.c`・`persistence_adapter_outcome.h` → `main.c`） | 0 件 |

**実装中に検査が見つけたもの**: `escapeTokens` を部分文字列で数えた最初の版は、
`src/adapters/win32/persistence_adapter.c` の `L"\\\\?\\UNC\\"`（UNC のパス接頭辞）を「`\U` を含む」として
掛けた。逃がした `\` の次の `U` はエスケープではないので、**リテラルを左から走査して `\` の次の 1 字だけを数える**
形に直した（ADR 0030 の補正 8）。除外の設定は足していない。

## 5. 限界（見ていないこと）

- **描いた絵そのものは見ていない。** 字面と字形の幅は測ったが、実機で描いた画面は目視していない
  （統合チェックリストの 74-1）。
- 日本語以外の列はまだ無い（`enum folio_language` は 1 値）。翻訳・言語の切り替えは単位 B / C。
- `draw_utf8` / `measure_utf8` の上限（`draw_unit_limit` = 1024 UTF-16 単位）を**超える文字列は描かれない**。
  表の最長は 64 単位、ノート名は 255 バイト、状態の 1 行は 512 バイトなので現状は届かないが、
  上限そのものを超える入力は試していない。
- 名前入力面の `pending_line_capacity`（600 バイト）を超える「旧名 → 新名」は、変更前と同じく面の初期化を
  失敗させる。実際に 600 バイトを超える組み合わせは作っていない（ノート名は 255 バイトまで）。
