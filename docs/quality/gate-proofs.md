# ゲート発火の証明 — NeNe Folio

> Status: 記録 / 最終実測 2026-09-09（Issue #3・最初の縦切り。core / application の静的ライブラリと単体テストが生まれ、ARC-002 / ARC-003 / ARC-007 / QLT-009 を結線した）
> 根拠となる規則: QLT-007（カスタムゲートには negative proof が要る）

**検査は「落ちること」を見るまで信用しない。** 各ゲートについて、最小の違反を仕込んだ状態で
意図した規則 ID によって失敗すること、そして元に戻すと `pwsh -NoProfile -File ./eng/check.ps1` が緑に戻ることを実測する。
ゲートを変えたら、この記録も同じ変更で更新する。

🔴 **この文書に結果を先に書かない。** 雛形の段階で「失敗」「成功」と書いてあった結果は、
実測していないのに測ったように見える（NeNe Loupe ADR 0003 が雛形の欠陥として指摘・2026-09-06）。
結果の列は実測するまで **未実測** のままにする。

環境: Windows x64。clang-cl 19.1.5（LLVM、Visual Studio 同梱）、MSVC toolset 14.44.35207（リンカ・SDK・dumpbin）、CMake 3.31.6-msvc6、Ninja 1.12.1、Python 3.12.10。今後使う版の正本は `eng/tool-versions.json`。
CI: GitHub Actions `windows-2022`（PR の ready_for_review で起動。ローカルの実測はこの文書、CI の実行証拠は Issue #1 の PR で取る）

---

## 1. 実測結果

### 1-a. active の規則（規則 ID ごとの発火と復帰）

| 規則 | 最小の違反 | 検証経路 | 実測 2026-09-09 |
| --- | --- | --- | --- |
| ARC-002 | 宣言外の依存と OS ライブラリを CMake で結ぶ・宣言外モジュールへの相対 include・ビルドに無い翻訳単位 | eng/prove-gates.py（configure）/ tests/conformance の architecture_checks | configure が `ARC-002: forbidden dependency` / `forbidden platform library` で非 0（P2）。相対 include と File API の実グラフは tests/conformance で ARC-002。実 target 6 つ（core / application / adapters_win32 / ui_win32 / app / unit_tests）に対し `--build-dir build` が 0 件 |
| ARC-003 | 中核相当のオブジェクトで `CreateFileW` を呼ぶ・未定義の外部シンボル・必須モジュールの欠落 | eng/prove-gates.py（`eng/symbols.py --object` / `--require`）/ tests/conformance の test_symbols | `ARC-003: core: undeclared external symbol __imp_CreateFileW` で非 0（P17）。`--require application` を単一オブジェクトに対して要求すると `required module application has no static library` で非 0（P18）。実ライブラリは `2 libraries checked, 0 violation(s)` |
| ARC-007 | 中核相当のオブジェクトで `time()` / `GetTickCount()` を呼ぶ | eng/prove-gates.py（`eng/symbols.py --object`） | `ARC-007: core: non-deterministic input symbol _time64` / `__imp_GetTickCount` で非 0（P1）。`memcmp` だけの probe は 0 |
| QLT-002 | 未使用変数・プロトタイプ無し・lint 違反・整形違反 | eng/prove-gates.py / 実 CMake ビルド・clang-tidy・clang-format | `-Wunused-variable` / `-Wmissing-prototypes` / `readability-function-size` / `clang-format-violations` で非 0。各復帰は 0 |
| QLT-004 | 一行に詰めた main | eng/prove-gates.py / clang-format --dry-run --Werror | `clang-format-violations` で非 0。元の整形は 0 |
| QLT-009 | 台帳・配置・状態のテストを省いて実行（`--coverage-negative`） | eng/coverage.py / 同一 exe の別プロファイル | 21.51% で `QLT-009: branch coverage 21.51% < 90%`。全テストへ復帰すると 707/744 分岐＝95.03% で成功（P19） |
| CNF-006 | 未定義 ID・重複定義・状態不一致・証明行欠落・未置換値 | tests/conformance の document_checks 正例・反例 | 正例は指摘 0、反例は CNF-006。本文書を書いた直後に「active の証明行が無い」を 4 件検出し、この表を書かせた |
| CNF-008 | Issue 番号の無いタスクコメント | tests/conformance の configuration_checks 正例・反例 | 番号付きは指摘 0、番号なしは CNF-008 |

### 1-b. 反例の一覧（planned の部分証明を含む）

2026-09-09 に `pwsh -NoProfile -File ./eng/check.ps1` を通した結果。`eng/prove-gates.py` は毎回のゲートで下の実ツール反例を仕込み直し、復帰まで確かめる（`out/proofs/results.json`）。

| # | 規則 | 仕込む違反 | 実行するタスク | 結果 |
| --- | --- | --- | --- | --- |
| P1 | ARC-007 | 中核相当のオブジェクトで `time()` / `GetTickCount()` を呼ぶ | `eng/prove-gates.py`（clang-cl でコンパイル → `eng/symbols.py --object`） | `eng/symbols.py` が `ARC-007: core: non-deterministic input symbol _time64` で非 0。`memcmp` だけの probe は 0 |
| P2 | ARC-002 | CMake で宣言外の依存と OS ライブラリを結ぶ | `eng/prove-gates.py`（configure） | configure が `ARC-002: forbidden dependency verification -> verification` / `forbidden platform library verification -> user32` で非 0。戻すと configure・build とも 0 |
| P3 | C-002 | 分岐漏れ／網羅済みの `default` | `eng/prove-gates.py`（実ビルド） | `-Wswitch-enum` と `-Wcovered-switch-default` で非 0。元のソースは 0 |
| P4 | QLT-002 | 未使用変数・プロトタイプ無し | `eng/prove-gates.py`（実ビルド） | `-Wunused-variable` と `-Wmissing-prototypes` で非 0。元のソースは 0 |
| P5 | CNF-001 | 型を `note_helper` と命名する | `tests/conformance` | tests/conformance: `note_helper` / `NoteManager` / `utils/` を CNF-001。普通の名前は指摘 0 |
| P6 | CNF-003 | waiver 無しの抑制を書く | `tests/conformance` | tests/conformance: pragma・`NOLINT`・waiver 無しの `NOLINTNEXTLINE` を CNF-003。有効な行単位 waiver は通る |
| P7 | CNF-004 | 期限切れの waiver を置く | `tests/conformance` | tests/conformance: 期限切れ・必須項目欠落・不整合索引・範囲外 Scope を CNF-004。期限当日は通る |
| P8 | QLT-004 | 整形を崩す | `eng/prove-gates.py`（clang-format） | clang-format --dry-run --Werror が `clang-format-violations` で非 0。元の整形は 0 |
| P9 | CNF-005 | `baseline` を名に含む設定ファイル・`/WX-`・`-Wno-error` | `tests/conformance` | tests/conformance: `baseline.json`・`/WX-`・`-Wno-error`・`-w` を CNF-005。名指しの `-Wno-switch-default` は通る |
| P10 | CNF-006 | マトリクスに無い規則 ID・未置換値・証明行の欠落 | `tests/conformance` | tests/conformance: 未定義 ID・未置換値・証明行欠落・重複定義・状態不一致を CNF-006。正例は指摘 0 |
| P11 | CNF-007 | どこからも読み込まれない設定ファイルを置く | `tests/conformance` | tests/conformance: 参照欠落と余分な `.clang-tidy` を CNF-007。正常な参照は通る |
| P12 | GIT-003 | 形に合わないコミット件名 | `tests/conformance` | tests/conformance: Issue 番号無し・英語説明・`BREAKING CHANGE` フッタ欠落を GIT-003。正しい件名は通る |
| P13 | C-003 | `const` を剥がして代入する | `eng/prove-gates.py`（`-Wcast-qual`） | `-Wcast-qual` で非 0。元のソースは 0 |
| P14 | C-004 | `nullptr` を逆参照する | `eng/prove-gates.py`（clang-tidy） | clang-tidy `clang-analyzer-core.NullDereference` で非 0。元のソースは 0 |
| P15 | C-012 | 5 引数の関数 | `eng/prove-gates.py`（clang-tidy） | clang-tidy `readability-function-size` で非 0。元のソースは 0 |
| P16 | C-016 | 可変長配列 | `eng/prove-gates.py`（`-Werror=vla`） | clang-tidy `clang-diagnostic-vla`（コンパイルでは `-Werror=vla`）で非 0。元のソースは 0 |
| P17 | ARC-003 | 中核相当で `CreateFileW` を呼ぶ | `eng/prove-gates.py`（`eng/symbols.py`） | `eng/symbols.py` が `ARC-003: core: undeclared external symbol __imp_CreateFileW` で非 0 |
| P18 | ARC-003 | 必須モジュールの静的ライブラリが無い | `eng/prove-gates.py`（`eng/symbols.py --object … --require application`） | `required module application has no static library in the build` で非 0 |
| P19 | QLT-009 | テストを省いた実行で分岐を測る | `eng/coverage.py`（測定ビルド・`--coverage-negative`） | 21.51% で非 0。全テストで 707/744（95.03%）が 0。確保失敗の注入を切ると 86.29% で落ちることも 2026-09-09 に実測した（切った状態は残していない） |
| P20 | ARC-003 | 中核が自分のアーカイブ内・宣言済み依存の関数を呼ぶ | `tests/conformance/test_symbols.py` | アーカイブ内（`note_id_parse`）と application → core（`category_ledger_count`）は解決されて指摘 0。宣言外の core → application（`folio_state_create`）と `nenefolio_*` の接頭辞だけの外部シンボルは ARC-003 |

**復帰の確認**: 2026-09-09。P1〜P4・P8・P13〜P18 は `eng/prove-gates.py` が各反例の直後に元へ戻して build / configure / clang-format / symbols を再実行し、終了コード 0 を確かめた。P5〜P7・P9〜P12・P20 は正例テストが同じ suite にある（63 テスト）。P19 は `eng/coverage.py` が反例のあとに全テストの計測で 0 を確かめる。最後にフルゲート全体が終了コード 0 で `NeNe Folio full gate passed` を出した。

**除外側の確認**: 例外区画について「禁止が効いていること」と「唯一の窓口が通ること」の両方を見る。

| 区画 | 適用しない禁止 | 呼んでいる禁止 API | 結果 |
| --- | --- | --- | --- |
| `src/adapters/win32` | 決定性・OS import | `__imp_GetModuleFileNameW` / `__imp_FindFirstFileExW` / `__imp_CreateFileW` / `__imp_ReadFile`（`persistence_adapter.c` / `file_bytes.c`） | 2026-09-09: `eng/symbols.py --build-dir build --require core application` は core / application の 2 ライブラリだけを検査して `0 violation(s)`。adapters_win32 のライブラリは対象外なので上の import を持ったまま通る（唯一の窓口が通ること） |
| `src/ui/win32` / `src/app` | OS import | `__imp_CreateWindowExW` / `__imp_GetDpiForWindow` / `__imp_MessageBoxW` | 同上。字句検査（ARC-007 の名前）は ui / app にも適用され、`GetTickCount` 等の名前は書けない |

---

## 2. 出力の抜粋（実行結果からの引用）

```
QLT-002: C:\Users\info\WORKS\NeNeFolio\out\proofs\build-jc2zb9_f\tests\build\toolchain_smoke.c(1,22): error: unused variable 'unused' [-Werror,-Wunused-variable]
C-002: C:\Users\info\WORKS\NeNeFolio\out\proofs\build-jc2zb9_f\tests\build\toolchain_smoke.c(2,40): error: enumeration value 'MODE_EDIT' not handled in switch [-Werror
C-003: C:\Users\info\WORKS\NeNeFolio\out\proofs\build-jc2zb9_f\tests\build\toolchain_smoke.c(2,59): error: cast from 'const struct note *' to 'struct note *' drops con
C-004: C:\Users\info\WORKS\NeNeFolio\out\proofs\build-jc2zb9_f\tests\build\toolchain_smoke.c:1:43: error: Dereference of null pointer (loaded from variable 'p') [clang
C-012: C:\Users\info\WORKS\NeNeFolio\out\proofs\build-jc2zb9_f\tests\build\toolchain_smoke.c:1:12: error: function 'pick' exceeds recommended size/complexity threshold
C-016: C:\Users\info\WORKS\NeNeFolio\out\proofs\build-jc2zb9_f\tests\build\toolchain_smoke.c:1:53: error: variable length array used [clang-diagnostic-vla]
QLT-004: C:\Users\info\WORKS\NeNeFolio\out\proofs\build-jc2zb9_f\tests\build\toolchain_smoke.c:1:15: error: code should be clang-formatted [-Wclang-format-violations]
```

フルゲートの構成で踏んだ 3 点（規則ではなく道具の癖。ADR 0003 に記録）:

- clang-tidy の cl ドライバは compile line の `/clang:-std=c23` を落とし、C23 の `constexpr` / `nullptr` を C17 として拒否した → `--extra-arg` で同じ変数を渡す
- `CMAKE_C_EXTENSIONS OFF` を書くと CMake が `/std:c17` を足し、clang-cl が `-Wunused-command-line-argument` で拒否した → 書かない
- clang-cl は `-Wall` を `/Wall`＝`-Weverything` と解釈し、`-Wc++98-compat` で C23 の `constexpr` を拒否した → `/W4` だけにする（K16）

---

## 3. 事故から生まれた検査

<!-- 設定だけあって効いていなかった、検査を足した直後に迂回された、などの事故を Issue 番号つきで残す。
     前例: NeNeClock Issue #26（設定ファイルが読み込まれていなかった）/ #42（文言検査の迂回） -->

（まだ無い）

---

## 4. リポジトリ設定（ruleset）

| 設定 | 値 | 確認日 |
| --- | --- | --- |
| PR 必須 | ruleset `main`（id 22558593・active）の `pull_request`。承認 0・スレッド解決必須・`allowed_merge_methods: [squash]` | 2026-09-09 |
| 必須 check | 同 ruleset の `required_status_checks`: context `check` | 2026-09-09 |
| strict up-to-date | 同 ruleset `strict_required_status_checks_policy: true` | 2026-09-09 |
| force push / ブランチ削除の禁止 | 同 ruleset の `non_fast_forward` と `deletion` | 2026-09-09 |
| squash のみ | リポジトリ設定 `allow_squash_merge` のみ true・`delete_branch_on_merge` true ＋ ruleset の `allowed_merge_methods` | 2026-09-09 |

読み戻し: `gh api repos/hideyukiMORI/nene-folio/rulesets/22558593`（2026-09-09）。必須 check が実際に PR を止めることは Issue #1 の PR で確認する。

🔴 **設定していないものを「必須になっている」と書かない。** 設定したら `gh api` で読み戻して記録する。

---

## 5. 環境依存の確認（QLT-013）

<!-- 表示・実機・実 OS 資源を伴う確認は、単体テストとは別にここに環境と手順を書く -->

### 5-a. 最初の縦切り（Issue #3・2026-09-09）

環境: Windows 11 Pro 10.0.26200・96 DPI・モニタ 1 枚・`build/NeNeFolio.exe`（Debug 構成＝ASan / UBSan / nullability 付き）。

手順:

1. `build/data/仕事/打ち合わせ.md` と `build/data/categories.json`（版 1・`仕事` を `#3D7EFF`・展開）を置く
2. `build/NeNeFolio.exe` を起動し、1.5 秒後に主窓の矩形を `GetWindowRect` で取って画面を写す
3. `categories.json` を `{"version": 2, "categories": []}` に書き換えて起動する

結果:

- 枠もタイトルバーも無い 960×640 の窓が (0, 0) に出た。左 240 px のドロワーに、青の帯付きの太字 `仕事` と、字下げした `打ち合わせ` が描かれた（[first-drawer.png](first-drawer.png)）。右ペインは白
- 版 2 の台帳では `NeNe Folio` を題にした MessageBox が出て、閉じると終了コード 1 で終わった。既定値へは落ちなかった（FR-015）
- 見ていないもの: DPI の切り替え・スナップ・縁でのリサイズ・高 DPI のフォント・複数モニタ。単体テストはこれらの証拠にならない

### 5-b. カテゴリ行のトグルと `expanded` の書き戻し（Issue #5・2026-09-09）

環境: 5-a と同じ。

手順:

1. 5-a と同じ `data/` で起動し、ドロワーの子ウィンドウ（クラス `NeNeFolioDrawer`）へ `WM_LBUTTONDOWN` / `WM_LBUTTONUP` を (40, 20)（カテゴリ行の中）に送る
2. 0.7 秒後に画面を写し、`data/categories.json` を読む
3. 同じ座標へもう一度送り、同様に写して読む

結果:

- 1 回目でノート行 `打ち合わせ` が消え（[category-collapsed.png](category-collapsed.png)）、`categories.json` の `expanded` が `false` になった。名前 `仕事` は UTF-8 のまま（E4 BB 95 E4 BA 8B）
- 2 回目でノート行が戻り、`expanded` は `true`。`categories.json.tmp` は残っていない
- 見ていないもの: 読み取り専用の `categories.json` での「書き戻せませんでした」の表示（reducer が状態を変えないことは単体テストで確認）

### 5-c. ノート行のクリックで右ペインに md を表示（Issue #7・2026-09-09）

環境: 5-a と同じ。`Msftedit.dll` は OS 同梱のもの。

手順:

1. `build/data/仕事/打ち合わせ.md` に見出し・太字・斜体・インラインコード・リンク・箇条書き・番号付き・引用・コードブロックを書く
2. 起動し、ドロワーへ `WM_LBUTTONDOWN` / `WM_LBUTTONUP` を (60, 50)（ノート行）に送る
3. 0.9 秒後に主窓を `PrintWindow` で写す

結果:

- 右ペインの `RICHEDIT50W` に、見出し（大きな太字）・太字・斜体・等幅のコード・下線のリンク文字・黒丸の箇条書き・`1.` `2.` の番号・字下げした斜体の引用・等幅のコードブロックが描かれた（[note-pane.png](note-pane.png)）
- 見ていないもの: 無い・読めない・UTF-8 でないノートを選んだときの 1 行の表示（reducer が表示値を変えないことは単体テスト）。長い本文のスクロール

### 5-d. 案2「堅」の見た目と OS に従うテーマ（Issue #9・2026-09-09）

環境: 5-a と同じ（Windows 11・96 DPI）。OS の「アプリのモード」は施主の設定がダーク。

手順:

1. `data/` に 3 カテゴリ（`#5B8DEF` / `#8B7CF6` / `#2FB8A6`、2 つ目は折り畳み）と 5 ノートを置く
2. `HKCU\…\Themes\Personalize\AppsUseLightTheme` を 0 にして起動 → ノート行をクリック → `PrintWindow` で写す → 値を元（0）に戻す
3. 同じ値を 1 にして同様に写し、元に戻す

結果:

- ダーク（[theme-dark.png](theme-dark.png)）: 地 #0A0B0D、頭に `NENE FOLIO` と件数 5、`01`〜`03` の番号がカテゴリ色、折り畳みは − / +、選択行は #16191D の面と右端の 6px の角。右ペインは #101214 に等幅のパンくず `01 仕事 / 打ち合わせ` と「閲覧」の札、本文は白い見出しと薄い引用
- ライト（[theme-light.png](theme-light.png)）: 同じ構成が #F4F5F7 / 白の上に描かれ、コードブロックだけ黒地
- 見ていないもの: 起動中の OS 設定の変更（追従しない・ADR 0005）、Windows 10 での角丸と縁、高 DPI

補足: 2 本目の縦切りの測定ビルド（ASan 付き）が、テスト補助関数がローカルのアダプタを指すポートを state に写していた寿命の誤り（stack-buffer-underflow）を捕まえた。
正典ビルドの CTest では到達しない経路（確保失敗の注入下でだけ走るトグル）だったので、測定ビルドにも ASan を付けた判断（ADR 0004）が効いた。

### 5-e. 編集モードと md の保存（Issue #11・2026-09-10）

環境: Windows 11 Pro 10.0.26200・`build/NeNeFolio.exe`（Debug 構成＝ASan / UBSan / nullability 付き）・
**主モニタ 150%（DPI 144）ではなく DPI 120 の側**で測った。窓は `SetWindowPos` で主モニタの (200, 120) へ寄せ、
`GetDpiForWindow` = **120**（client 1200×800）で座標を計算した。
🔴 補助スクリプトを DPI 非対応のまま走らせると `GetWindowRect` が 960×640 を返し、`PrintWindow` が窓の左上だけを写して
**右端の札が写真から消える**。撮影する側も Per-Monitor v2 にする（今回 1 度これで誤診した）。

手順（`FindWindowExW` で子を取り、`SendMessageW` / `PostMessageW` で操作。失敗の窓が出る操作だけ `PostMessageW`）:

1. `data/仕事/打ち合わせ.md` を **CRLF**（232 バイト・CR 15 / LF 15）に、`data/個人/買い物.md` を **LF** のままにする
2. ノート行をクリック → 右端の「編集」の札をクリック → `RICHEDIT50W` の `GWL_STYLE` から `ES_READONLY` を読む
3. `EM_SETSEL` で末尾へ移り、`WM_KEYDOWN`(VK_RETURN) → `WM_CHAR`(0x0D) と `WM_CHAR` で `ABC ` と U+65E5 U+672C U+8A9E を打つ
4. `WM_CHAR` の **0x13**（Ctrl+S の制御文字）を送り、md のバイト列を読む
5. 「閲覧」の札をクリックして写す
6. md に読み取り専用属性を付けて 3〜4 を繰り返し、`FindWindowW("#32770", "NeNe Folio")` を探す。属性は戻す
7. 編集中に別カテゴリのノート行をクリックする／編集中に `WM_CLOSE` を送る
8. `AppsUseLightTheme` を 1 にして 2 を繰り返し、**値を 0 に戻す**

結果:

- 「編集」の札で `ES_READONLY` が落ち、RichEdit に **md の原文**が平文（Yu Gothic UI 11pt・`editor_text`）で出た（[edit-mode.png](edit-mode.png)）。札は有効な側だけ面塗り
- Ctrl+S で md が **247 バイト・CR 16 / LF 16・孤立した LF は 0・先頭 3 バイトは `23 20 E6`（BOM 無し）**。
  末尾は `0A 0D 0A 41 42 43 20 E6 97 A5 E6 9C AC E8 AA 9E` = 改行 + `ABC 日本語`。**CRLF のファイルは CRLF のまま**
- LF の `買い物.md` に同じ操作をすると **CR 0 / LF 4**。RichEdit から出てくるのは常に CRLF だが、core が元の形へ畳んでいる
- Ctrl+S のあとも `ES_READONLY` は落ちたまま（編集モードのまま）。「閲覧」の札で読み取り専用に戻り、RTF で描き直された（[edit-saved.png](edit-saved.png)）
- 読み取り専用の md では「ノートを書き戻せませんでした。編集中の本文はそのままです。」の窓が出て、ファイルは 247 バイトのまま、
  RichEdit の本文（打った `XYZ`）も残った。その状態で「閲覧」の札を押しても同じ窓が出て**閲覧へ戻らない**（FR-015 / ADR 0006 の決定 6）
- 編集中に別ノートを選ぶと、先に保存されてから選択が切り替わった。編集中の `WM_CLOSE` も保存してから終了し、**終了コード 0**。`data/` に `.tmp` は残らない
- ライトでも札は同じ配置で、有効な側が #E9EBEF の面（[edit-mode-light.png](edit-mode-light.png)）。撮影後に `AppsUseLightTheme` は 0 へ戻した
- 施主の実機確認（2026-09-10 00:03・DPI 96）: `data/仕事/打ち合わせ.md`（CRLF 版）を「編集」の札で開き、IME で「あああああ」を
  入力して Ctrl+S。設計リナがバイト列を読み、**末尾に `E3 81 82` × 5・改行はすべて `0D 0A`・先頭 BOM 無し・`.tmp` 無し**を確認した
- 見ていないもの: **IME のかな漢字変換の未確定文字列が残ったままの Ctrl+S**（確定後の Ctrl+S だけを確認した）。
  ほかに 32767 文字を超える md での入力（`EM_EXLIMITTEXT` は流し込みで縮まないことだけ確かめた: 作成直後 32767 → 2147483647 のまま）、
  外部で書き換えられた md の上書き（仕様の非要件）、キャレット位置の往復での保存

### 5-f. ドラッグでの並び替えと `index.json` の書き戻し（Issue #13・2026-09-10）

環境: Windows 11 Pro 10.0.26200・`build/NeNeFolio.exe`（Debug 構成＝ASan / UBSan / nullability 付き）・
主窓を `SetWindowPos` で主モニタの (200, 120) へ寄せ、`GetDpiForWindow` = **120**（client 1200×800・ドロワー 300 幅）で
座標を計算した。撮影する側も Per-Monitor v2（5-e の教訓）。OS のアプリのモードはダーク（施主の設定・触っていない）。
`build/data/` は 3 カテゴリ（`仕事` #5B8DEF / `個人` #8B7CF6 / `読書` #2FB8A6）・5 ノートで、確認の前後で元へ戻した。

手順（`FindWindowExW` で子を取り、`SendMessageW` で `WM_LBUTTONDOWN` → `WM_MOUSEMOVE` ×8〜10 → `WM_LBUTTONUP` を送る。
失敗の窓が出る操作だけ離しを `PostMessageW`）。DPI 120 での行の位置は `top_padding` 55・カテゴリ行 42（上に間 7）・ノート行 38:

1. カテゴリ行 `仕事`（y=84）を y=250 まで運び、離す前に `PrintWindow` で写してから離す
2. `仕事`（y=170）を y=50 まで運んで離し、`categories.json` のバイト列を読む
3. ノート行 `打ち合わせ`（y=124）を同じカテゴリ内で y=170 まで運び、離す前に写してから離す
4. ノート行（y=124）を別カテゴリ `読書` の上（y=330）まで運んで離す
5. ノート行とカテゴリ行を**動かさずに** down/up する
6. `仕事\index.json` に読み取り専用属性を付けて 3 を繰り返し、`#32770` の窓を写す。属性は戻す
7. ノートを選んで「編集」の札を押し、`XYZ` を打った状態のままカテゴリ `仕事` を下へ運ぶ

結果:

- 1: 挿入線が `買い物` の下（`読書` のカテゴリ行の上・間の中ほど）に 1 本、**掴んだ `仕事` の青 #5B8DEF** で、
  カテゴリの字下げから右の余白まで引かれた（[drag-category.png](drag-category.png)）。離すと `categories.json` は
  **`個人` / `仕事` / `読書`**。`.tmp` は残らない
- 2: `仕事` が先頭へ戻り、バイト列は `7B 0A 20 20 22 76 65 72 73 69 6F 6E 22 3A 20 31 2C …`＝
  **キー順は version → categories → name → color → expanded のまま・UTF-8（`仕事` は E4 BB 95 E4 BA 8B）・BOM 無し**
- 3: 挿入線は `週報` の下に、**ノート行の字下げから**引かれた（[drag-note.png](drag-note.png)）。離すと
  `data/仕事/index.json` が**初めて作られ**、`{"version": 1, "notes": ["週報", "打ち合わせ"]}`
  （`22 E9 80 B1 E5 A0 B1 22` = `"週報"`）。表示も同じ順に変わった
- 4: `読書` の上で離しても**自分のカテゴリの端に寄り**、`仕事/index.json` は `打ち合わせ / 週報`。
  `読書/index.json` は**作られない**。`categories.json` も変わらない
- 5: 動かさずに離すと今までどおり、ノート行は選択（右ペインに md）・カテゴリ行はトグル（`個人` が `+` に畳まれ
  `expanded` が `false` で書き戻る）。クリックの確定が離すときへ移っても振る舞いは同じ
- 6: 「data/ の台帳（categories.json / index.json）に書き戻せませんでした。表示は変えていません。」の窓が出て、
  `index.json` はバイト単位で**不変**、ドロワーの並びも変わらず、`.tmp` も残らない
- 7: カテゴリを動かしても `ES_READONLY` は落ちたまま（編集モードのまま）で、RichEdit の長さは **161 → 161**（打った `XYZ` を含む未保存の本文がそのまま）。
  右ペインの頭の番号だけが `01` → `02` に変わり、カテゴリ名 `仕事` とノート名 `打ち合わせ` と選択行の印はそのまま
  （[drag-editing.png](drag-editing.png)）。並び替えは md を 1 度も書かない
- 見ていないもの: **96 DPI と高 DPI（144 以上）での挿入線の太さと位置**（今回は DPI 120 の 1 点だけ）。
  実際のマウスでのドラッグ（`SendMessageW` の合成入力だけを見た）。ドロワーの外へポインタを出したままの離し
  （`SetCapture` の経路は通っているが、`WM_CAPTURECHANGED` での取り消しは実機で起こしていない）。
  折り畳んだカテゴリ**だけ**が並ぶ配置での挿入線。カテゴリが 1 つだけのときのドラッグ

### 5-g. 別カテゴリへのノートの移動と両方の `index.json`（Issue #15・2026-09-10）

環境: Windows 11 Pro 10.0.26200・`build/NeNeFolio.exe`（Debug 構成＝ASan / UBSan / nullability 付き）・
主窓を `SetWindowPos` で主モニタの (200, 120) へ寄せ、`GetDpiForWindow` = **120**（client 1200×800・ドロワー 300 幅）で
座標を計算した。撮影する側も Per-Monitor v2（5-e の教訓）。OS のアプリのモードはダーク（施主の設定・触っていない）。
`build/data/` は 3 カテゴリ（`仕事` #5B8DEF: 打ち合わせ・週報 / `個人` #8B7CF6: 買い物 / `読書` #2FB8A6: 積読・抜き書き）で、
各シナリオの前に退避した内容へ戻し、確認の最後にも元へ戻した。DPI 120 の行は `top_padding` 55・カテゴリ行 43（上に間 8）・
ノート行 38（`MulDiv` の丸め）。塊の境界は次の塊のカテゴリ行の上端から間の半分（4 px）上。

手順（`FindWindowExW` で子を取り、`SendMessageW` で `WM_LBUTTONDOWN` → `WM_MOUSEMOVE` ×10 → `WM_LBUTTONUP`。
失敗の窓が出る操作だけ離しを `PostMessageW`）:

1. ノート行 `打ち合わせ`（y=125）を `読書` の `積読`（322..360）と `抜き書き`（360..398）の間（y=360）まで運び、
   離す前に `PrintWindow` で写してから離す
2. `categories.json` の `個人` を `expanded: false` にして起動し、`打ち合わせ`（y=125）を折り畳んだ `個人` の
   カテゴリ行（190..233）の上（y=212）で離す
3. `読書/週報.md` を置いて起動し、`仕事` の `週報`（y=163）を `読書` の中（y=398）で離す
4. `打ち合わせ` を選んで「編集」の札を押し、末尾に `XYZ` を打った状態のまま 1 と同じ移動をする。
   そのあと `WM_CHAR` の 0x13（Ctrl+S）を送る
5. `読書/index.json` に読み取り専用属性を付けて 1 と同じ移動をし、`#32770` の窓を写す。属性を戻して起動し直す

結果:

- 1: 挿入線が `積読` と `抜き書き` の間に、**掴んだ `打ち合わせ` の青 #5B8DEF** で、ノート行の字下げから右の余白まで
  引かれた（[move-note.png](move-note.png)）。離すと `build/data/読書/打ち合わせ.md` ができ、`仕事/` から消えた。
  `読書/index.json` = **積読 / 打ち合わせ / 抜き書き**、`仕事/index.json` = **週報**。
  `読書/index.json` のバイト列は `7B 0A 20 20 22 76 65 72 73 69 6F 6E 22 3A 20 31 2C …`＝
  **キー順は version → notes・UTF-8（`積読` は E7 A9 8D E8 AA AD）・BOM 無し**。`.tmp` は残らない
- 2: 折り畳んだ `個人` のカテゴリ行の上で離すと**末尾**に入り、`個人/index.json` が**初めて作られて**
  `買い物 / 打ち合わせ`。`仕事/index.json` = `週報`
- 3: 「移動先に同じ名前のノートがあります。移していません。」の窓が出て、`data/` 以下のファイルは
  **名前もバイト数も 1 つ残らず不変**（`読書/index.json` = 積読 / 抜き書き、`仕事/index.json` = 打ち合わせ / 週報）。`.tmp` も無し
- 4: 移動しても `ES_READONLY` は落ちたまま（編集モードのまま）で、RichEdit の長さは **161 → 161**（打った `XYZ` を含む
  未保存の本文がそのまま）。右ペインの頭は `01 仕事 / 打ち合わせ` から **`03 読書 / 打ち合わせ`** に変わり、番号と選択行の印が
  `読書` の色 #2FB8A6 になった（写真は 1 の [move-note.png](move-note.png) で代表する）。
  そのあとの Ctrl+S は**移動先**の `読書/打ち合わせ.md`（220 バイト・末尾 `0A 60 60 60 0A 58 59 5A` = 改行 + ``` + `XYZ`）へ書いた
- 5: 「ノートは移しましたが、台帳（index.json）を書き戻せませんでした。次回の起動で揃います。」の窓が出て、
  **md は `読書/` へ移り**、`読書/index.json` はバイト単位で不変（積読 / 抜き書き）、`仕事/index.json` は `週報` に書き戻された。
  属性を戻して起動し直すと、ドロワーの `読書` は **積読 / 抜き書き / 打ち合わせ**（台帳の順のあとに走査で見つけた md が付く・
  FR-008 の自己修復）で、頭の件数は 5 のまま
- 見ていないもの: **96 DPI と高 DPI（144 以上）での挿入線**（今回も DPI 120 の 1 点だけ）。実際のマウスでのドラッグ。
  大文字小文字だけ違う名前への移動（台帳では別名・OS では同名なので「書き戻せなかった」側の文言になる・ADR 0008 の結果）。
  別ボリュームへの移動（`data/` は 1 つのディレクトリ木）。移動の途中で電源が落ちた場合の実測

### 5-h. ドロワーのスクロールとあふれのフェード（Issue #19・2026-09-10）

環境: Windows 11 Pro 10.0.26200・`build/NeNeFolio.exe`（Debug 構成＝ASan / UBSan / nullability 付き）・
主窓を `SetWindowPos` で主モニタの (200, 120) へ寄せ、`GetDpiForWindow` = **120**（client 1200×800・ドロワー 300×800）で
座標を計算した。撮影する側も Per-Monitor v2（5-e の教訓）。OS のアプリのモードはダーク（施主の設定）。
`build/data/` は確認の前に退避し、**4 カテゴリ（仕事 #5B8DEF 10 本 / 個人 #8B7CF6 9 本 / 読書 #2FB8A6 8 本 / 資料 #E4A11B 9 本）・
ノート 36 本・全展開**の索引に差し替え、終了後に元へ戻した。

DPI 120 の寸法（`MulDiv` の丸め）: `top_padding` 55・カテゴリ行 43（上に間 8）・ノート行 38・`bottom_padding` 20・フェード 30。
行の下端は **1627**、上限は 1627 + 20 − 800 = **847**。

手順（`FindWindowExW` で `NeNeFolioDrawer` / `RICHEDIT50W` の子を取り、`SendMessageW` で `WM_MOUSEWHEEL`（wParam の
上位ワードに ±120・lParam はスクリーン座標）・主窓へ `WM_KEYDOWN`（`VK_UP` / `VK_DOWN` / `VK_PRIOR` / `VK_NEXT`）・
`WM_LBUTTONDOWN` → `WM_MOUSEMOVE` ×8 → `WM_LBUTTONUP` を送る。撮影は `PrintWindow(hwnd, dc, 2)`）:

1. ノート行をクリックして選び、**選んだ行の面（#16191D）の上端 y** を写真の画素から読む。行の内部の座標は
   台帳から計算できるので、`内部の上端 − 面の上端` がそのときのスクロール量になる（この差でだけ量を測った）
2. ホイール 1 刻み・`VK_DOWN` 1 回・`VK_NEXT` 1 回をそれぞれ 0 から送り、量を読む
3. ホイールを 30 刻み回して下端まで行き、さらに 5 刻み回す
4. フェードは「地の色からの隔たりの最大値」を帯の 30 行ぶん並べて測る。**同じ内部の行**が帯の中に来る量と
   画面の真ん中に来る量の 2 つで撮り、並べて比べる
5. スクロールした位置でクリック（正しいノートが選ばれるか）・カテゴリ行のトグル・頭の帯のクリック
6. ノート行を掴んで 48 px 動かし、離す前にホイールを 1 刻み回してから離す
7. ノートを選んで「編集」の札を押し、末尾に `XYZ` を打った状態でドロワーをホイールと鍵でスクロールし、Ctrl+S を送る
8. `AppsUseLightTheme` を 1 にして 4 を繰り返し、**値を 0 に戻す**

結果:

- 1 回の移動量は **ホイール 1 刻み = 114 px（= ノート行 38 × 3）・`VK_DOWN` = 38 px・`VK_NEXT` = 762 px（= 800 − 38）**。
  いずれも設計どおりで、端数の蓄積は無い
- 下端は **847**（計算値と一致）で、そこからさらに 5 刻み回しても **847 のまま**動かない。`VK_UP` を 40 回送ると 0 に戻る
- フェード（ダーク・上端の帯 = 表示 55..85）: 隔たりは `0, 5, 10, 14, 19, …, 62, 63` と端から内側へ**線形に増える**。
  **同じ内部の行**を画面の真ん中（表示 321..351）で撮ると `142` が並ぶ。下端の帯（表示 770..800）は `48, 43, 38, …, 5, 0` で、
  同じ行を真ん中で見ると `142`。帯の外側の端で地の色 100%、内側で 0% に混ざっている
- フェードの有無: **上端にいるとき（量 0）は下だけ**（[scroll-top.png](scroll-top.png)）、**中間では両方**
  （[scroll-middle.png](scroll-middle.png)）、**下端（量 847）では上だけ**（[scroll-bottom.png](scroll-bottom.png)）。
  スクロールバーは出ない
- ライト（`AppsUseLightTheme` = 1）でも同じ規則で、地 #F4F5F7 へ向かって上端 `0, 5, 11, …, 68`（真ん中では `152`）・
  下端 `213, 205, 198, 190, 181, 173, 0, …`（[scroll-light.png](scroll-light.png)）。撮影後に値は 0 へ戻した
- スクロールした位置でのクリック: 量 0 で y=400 → `仕事-08`、量 762 で y=400 → `読書-07`、量 847 で y=600 → `資料-05`。
  いずれも「内部の上端 − 面の上端」がその時点の量と**画素まで一致**した（＝ヒットテストが表示座標で正しい）
- スクロールした位置でのトグル: 量 494 で `読書` のカテゴリ行（表示 393..436）を押すと `categories.json` の
  `読書` が `expanded: false` になり、もう一度押すと `true` に戻った
- 頭の帯: 量 494（`個人-01` が帯の下へ潜っている状態）で y=10 / 20 / 54 を押しても選択は `仕事-08` のまま変わらず、
  帯の下端そのもの（y=55 = `top_padding`）を押すとそこに描かれている `個人-01` が選ばれた。
  **スクロールで帯の下へ潜った行は掴めない**。この判断は core の `drawer_layout_hit`（`tests/unit/layout_tests.c` が正本）
  にあり、ここはその実機での確認
- ドラッグ中のホイール: `仕事-08` を掴んで y=448 まで運ぶと挿入線は **y=448**。離す前にホイールを 1 刻み回すと、
  同じポインタ位置の下の行が変わるので挿入線が **y=461**（`個人-01` と `個人-02` の間）へ**引き直された**
  （[scroll-drag-wheel.png](scroll-drag-wheel.png)）。そこで離すと `個人/index.json` が
  `個人-01 / 仕事-08 / 個人-02 / …` になり、`仕事/index.json` から `仕事-08` が消え、md も `個人/` へ移った
- 編集中のスクロール: 「編集」の札で `ES_READONLY` が落ち、末尾に `XYZ` を打つと長さは 38 → **41**。
  そのままホイール 3 刻みと `VK_DOWN` 2 回でドロワーをスクロールしても `ES_READONLY` は落ちたまま・長さは **41 のまま**。
  続けて Ctrl+S（`WM_CHAR` の 0x13）を送ると md は **51 バイト**になり、末尾は `20 74 77 6F 0A 58 59 5A` = `two` + 改行 + `XYZ`
  （**LF のまま・BOM 無し**）。「閲覧」の札で読み取り専用に戻り、そのあと主窓へ `VK_DOWN` を送るとドロワーが 38 px 動いた
  （＝閲覧へ戻ると鍵が主窓へ返る）
- 見ていないもの: **96 DPI と高 DPI（144 以上）でのフェードの高さと手触り**（今回も DPI 120 の 1 点だけ）。
  実際のマウスのホイールと高精度ホイール（合成した `WM_MOUSEWHEEL` だけを見た）。
  「非アクティブ ウィンドウをホバーしたときにスクロールする」を切った環境（ADR 0009 の「失う・残る」）。
  DPI を切り替えたあとの量のずれ。窓の高さを変えたときの上限の縮み。
  編集中のスクロールの写真は `PrintWindow` が RichEdit の本文を写さなかったので、長さの実測だけを証拠にした

### 5-i. カテゴリ行の右クリックで色を選ぶ（Issue #21・2026-09-10）

環境: Windows 11 Pro 10.0.26200・`build/NeNeFolio.exe`（Debug 構成＝ASan / UBSan / nullability 付き）・
主窓を `SetWindowPos` で主モニタの (200, 120) へ寄せ、`GetDpiForWindow` = **120**（client 1200×800・ドロワー 300×800）。
撮影する側も Per-Monitor v2（5-e の教訓）。OS のアプリのモードはダーク（`AppsUseLightTheme` = 0・施主の設定のまま変えていない）。
`build/data/` は確認の前に退避し、**3 カテゴリ（仕事 #5B8DEF 2 本 / 個人 #8B7CF6 1 本 / 読書 #2FB8A6 2 本）・ノート 5 本・全展開**の
索引で試して、終了後に元へ戻した。

DPI 120 の行の位置（`top_padding` 55・間 8・カテゴリ行 43・ノート行 38）: 仕事 63..106 / 週報 106..144 / 打ち合わせ 144..182 /
個人 190..233 / 買い物 233..271 / 読書 279..322 / 積読 322..360 / 抜き書き 360..398。

手順（`FindWindowExW` で `NeNeFolioDrawer` / `RICHEDIT50W` の子を取る。**右クリックは `ChooseColorW` がモーダルなので
`WM_RBUTTONDOWN` → `WM_RBUTTONUP` を必ず `PostMessageW`** で送る。色の選択の窓は `FindWindowW("#32770", "色の設定")` で取り、
`GetDlgItem` の R / G / B 欄（`COLOR_RED` = 706 / `COLOR_GREEN` = 707 / `COLOR_BLUE` = 708）へ `WM_SETTEXT` してから
対話箱へ `EN_UPDATE` の `WM_COMMAND` を送り、`IDOK` / `IDCANCEL` を `PostMessageW`。
**他プロセスのコントロールは `GetWindowTextW` では読めないので、読みは `WM_GETTEXT`**）:

1. ノートを選んでから `仕事` のカテゴリ行（y=84）を右クリックし、出た窓の初期値と「作成した色」を見る
2. R/G/B を 220/38/38（`#DC2626`）にして OK。`categories.json` のバイト列・行の番号・右ペインの頭の色を見る
3. `個人`（y=211）を右クリックして 1/2/3 に変えてから**キャンセル**
4. `仕事` を右クリックして**何も変えずに OK**
5. ノート行（y=125）と頭の帯（y=20）を右クリック
6. 「編集」の札で編集に入り `XYZ` を打ってから、別のカテゴリを右クリックして色を変える
7. `categories.json` に読み取り専用属性を付けて 2 を繰り返す。属性は戻す
8. 左ボタンを押したまま（`WM_LBUTTONDOWN` を送って捕捉させたまま）右クリックし、そのあと離してもう一度右クリック

結果:

- **色の選択の窓**（[color-dialog.png](color-dialog.png)）: `CC_FULLOPEN` のとおり**全開**で出て、owner は主窓（窓の位置は
  (204, 120)＝主窓の (200, 120) の隣）。`CC_RGBINIT` のとおり初期値は**その行の色**で、R/G/B 欄は `91/141/239` = `#5B8DEF`。
  `個人` を右クリックすれば `139/124/246` = `#8B7CF6`、`読書` なら `47/184/166` = `#2FB8A6` と、押した行の色が入る。
  **「作成した色」16 枠はすべて白**（枠の帯 x 15..300 / y 330..395 を数えると白が 10496 px で、残りは対話箱の地と枠の
  `#F0F0F0` / `#A0A0A0` / `#E3E3E3` / `#696969` の 4 色だけ＝**16 枠に白以外の色は 1 つも無い**）
- **OK**（[color-before.png](color-before.png) → [color-after.png](color-after.png)）: `categories.json` は 298 バイトのままで
  `仕事` の色だけが `#DC2626` に変わった。バイト列は
  `7B 0A … 22 63 6F 6C 6F 72 22 3A 20 22 23 44 43 32 36 32 36 22` = `"color": "#DC2626"` で、
  **大文字・キーの順は version → categories → name / color / expanded・先頭は `7B`（`{`）で BOM 無し・`.tmp` は 0 個**。
  写真では `仕事` の番号 `01`（ドロワー）・選択行の角の印・右ペインの頭のパンくずの `01` が**青から赤へ同時に変わり**、
  `個人`（`02`）と `読書`（`03`）・本文・札は変わらない。画素で数えると
  ドロワーの `#5B8DEF` 74 px → 0 px・`#DC2626` 0 px → 74 px、パンくずの `#5B8DEF` 10 px → 0 px・`#DC2626` 0 px → 10 px
  （`個人` の 12 px と `読書` の 14 px はそのまま）
- **キャンセル**: R/G/B を 1/2/3 に変えてからキャンセルすると、`categories.json` は**バイト・大きさ・mtime とも不変**で
  `個人` は `#8B7CF6` のまま。`.tmp` も残らない
- **同じ色で OK**: `#DC2626` の行を右クリックして何も変えずに OK すると、mtime は**同じまま**（＝書き込みが起きていない）
- **ノート行・頭の帯**: どちらを右クリックしても `#32770` の窓は現れない（`FindWindowW` が 4 回とも 0）。ファイルも不変
- **編集中の色変更**: 「編集」の札で `ES_READONLY` が落ち（長さ 19）、`XYZ` を打つと 22。そのまま別のカテゴリを
  右クリックして `#F472B6` にすると、`ES_READONLY` は**落ちたまま**・長さは **22 のまま**で、ドロワーのその番号だけが桃色になった
  （台帳には `#F472B6` が入る）。本文にも編集モードにも触っていない
- **読み取り専用の `categories.json`**: OK を押すと `NeNe Folio` を題にした窓が 1 つ出て、本文は
  **「data/ の台帳（categories.json / index.json）に書き戻せませんでした。表示は変えていません。」の 1 行**。
  閉じたあと `categories.json` は**バイト列も mtime も不変**で、ドロワーの色も変わらない
  （`#5B8DEF` は 74 px のまま・選ぼうとした `#10B981` は 0 px）。`.tmp` も残らない。属性は確認後に戻した
- **左ボタンを押している間の右クリック**: 捕捉中（`WM_LBUTTONDOWN` を送ったまま）に右クリックしても窓は出ない（0）。
  離すと今までどおりのクリックとして `仕事` が折り畳まれ、そのあと右クリックすればまた窓が出る（初期値もその行の色）
- **デザインからの逸脱（ADR 0010 の「失う・残る」の実測）**: OS のアプリのモードがダークでも
  `ChooseColorW` は**ライトのまま**出る（[color-dialog.png](color-dialog.png) の地は `#F0F0F0`）。案2「堅」には従わない
- 見ていないもの: **96 DPI と高 DPI（144 以上）**（今回も DPI 120 の 1 点だけ）。実際のマウスの右ボタン
  （合成した `WM_RBUTTONDOWN` / `WM_RBUTTONUP` だけを見た）。`WM_CONTEXTMENU`（Shift+F10 / Apps 鍵。ADR 0010 で
  初版は受けないと決めた）。カスタム色を作ってから閉じ直したときに消えること。
  ライトのアプリのモードでの見え方（`AppsUseLightTheme` は 0 のまま触っていない）。
  読み取り専用の窓の写真は `PrintWindow` が MessageBox の本文を写さず、`CopyFromScreen` も前面に出せなかったので、
  窓の本文の実測（`WM_GETTEXT`）だけを証拠にした（5-b / 5-f / 5-g と同じ扱い）

### 5-j. 枠なし窓の client・最小サイズ・パンくずの省略（Issue #22・2026-09-10）

環境: Windows 11 Pro 10.0.26200・`build/NeNeFolio.exe`（Debug 構成＝ASan / UBSan / nullability 付き）と、
比較のために**修正前の実行ファイル**を `build/NeNeFolio-before.exe` として並べて置いた（確認後に消した）。
「修正前」は 2 段階ある: パンくずと最小サイズの比較は main の `a860218`（= HEAD `85268bc` の production コード）、
`WM_NCCALCSIZE` の `wParam == FALSE` の比較は `030d2a5`（TRUE だけを自分で答えていた版）。撮影する側も Per-Monitor v2（5-e の教訓）。
OS のアプリのモードはダーク（`AppsUseLightTheme` = 0・施主の設定のまま触っていない）。
`build/data/` は確認の前に退避し、**長い名前**の索引（`プロジェクト運営と週次報告` #5B8DEF に
`2026年度第3四半期の振り返りと来期の計画メモ` と `週報`・`個人` #8B7CF6 に `買い物`）へ差し替えて、終了後に元へ戻した。
主モニタは `GetDpiForWindow` = **120**（作業領域 `0,0,3840,2100`）。

手順（窓は `FindWindowW("NeNeFolioWindow", "NeNe Folio")` で取り、`SetWindowPos` で動かす／大きさを変える。
最大化は `ShowWindow(SW_MAXIMIZE)` と `SW_RESTORE`。ノートの選択は `NeNeFolioDrawer` へ `WM_LBUTTONDOWN` /
`WM_LBUTTONUP`、札は主窓へ `WM_LBUTTONDOWN`。写真は `PrintWindow(hwnd, dc, 2)`。
**`CopyFromScreen` は AMSI に「悪意のあるスクリプト」として拒否されたので使えない**）:

1. 修正前・修正後をそれぞれ起動し、窓が現れた瞬間から 400 ms のあいだ `GetWindowRect` と `GetClientRect` を
   休まず読んで、値が変わった時刻と**何回目の観測か**を記録する（各 3 回）
2. 起動後・動かしただけ・リサイズ後・別モニタへ移したあとに、窓の外形と `ClientToScreen` した client を数値で比べ、
   縁 12 px の画素を並べる
3. `SW_MAXIMIZE` → 窓の矩形と client を作業領域と比べる → `SW_RESTORE`
4. `SetWindowPos` で 300×200 を頼む
5. 1200×800 → 820×560 にして、ドロワーの子窓の矩形と頭の札の位置を見る。`SetWindowPos` の直後に
   `GetUpdateRect` を 60 回読む
6. 長いノートを選び、幅を 1200 / 900 / 760 / 700 と狭めて、頭の帯（y 4..50）の画素を x ごとに
   「番号の青／カテゴリ名の暗い字 `#4A5059`／ノート名と有効な札の明るい字 `#C3C8CF`・`#E2E4E7`／
   「/」と札の面の淡い色」に分けて数える
7. 幅 820 で「編集」の札（client 740..800）と「閲覧」の札（670..730）を押し、md のバイト数と mtime を見る
8. DPI 144 と 168 のモニタへ移して 2 / 3 / 4 を繰り返す

結果:

- **窓の外形と client**: 修正後は `GetWindowRect` と `GetClientRect` + `ClientToScreen` の差が
  上下左右とも **0**（起動 1.5 秒後・動かしただけ・リサイズ後・DPI 144 / 168 のモニタへ移したあと）。
  縁 12 px は左が `#0A0B0D`（ドロワーの地）・上が `#101214`（右ペインの地）で、枠の色は出ない
  （[frame-after.png](frame-after.png)）。修正前の写真も同じ絵になる（[frame-before.png](frame-before.png)）＝
  **帯は写真ではなく矩形の数値で見る**（次の項）
- **作成直後の client（白い帯の発生条件・ADR 0011 の決定 1）**: `wParam == FALSE` を `DefWindowProcW` に流していた版
  （`030d2a5`）では、窓が現れてから **37 / 42 / 45 ms**（3 回）のあいだ client が **1184×784**（窓は 1200×800・**四方 8 px 小さい**）で、
  そのあと 1200×800 になる。この 8 px がまさに `WS_THICKFRAME` の枠で、OS がそこを既定の枠の色で描ける領域である。
  **`FALSE` も自分で答える版では、3 回とも「1 回目の観測」から差が 0 px** で、8 px の期間そのものが無い。

  | 版 | 試行 1 | 試行 2 | 試行 3 |
  | --- | --- | --- | --- |
  | `030d2a5`（FALSE を既定処理へ） | 11 ms まで 8 px | 37 ms まで 8 px | 42 ms まで 8 px |
  | この縦切り（FALSE も 0 を返す） | 1 回目の観測から 0 px | 1 回目の観測から 0 px | 1 回目の観測から 0 px |

  当初の見立て（「`GWLP_USERDATA` を結ぶ前の `TRUE` が既定に流れる」）は、修正前と修正後で差が出なかったこの実測で退けた。
  `TRUE` は結び付けの後にしか届かず、作成時に必ず届くのは `FALSE` である（ADR 0011 の「文脈」(1) はこの実測に合わせて直した）。
  なお 1 度だけ、`a860218` の版で client が内側のまま 1 回描かせたあと、1.5 秒後の写真の縁 8 px が
  **左端・上端とも `#E3E3E3` `#FFFFFF` `#F4F7FC`×6**（＝白い帯）になり、`GetClientRect` が 1200×800 を返しているのに
  帯が残るのを観測した。帯の色の実測はこの 1 回だけで、写真として繰り返し撮る手順は見つかっていない
  （`PrintWindow` は窓を描き直させるので、内側だった瞬間の画素は普通は残らない）
- **最大化**: 窓の矩形は修正前・修正後とも `-8,-8,3848,2168`（OS が枠ぶん外へ出す）。**client は修正前が
  `-8,-8,3848,2168`（3856×2176＝作業領域を四方 8 px と タスクバーぶんはみ出す）、修正後は `0,0,3840,2100` で
  作業領域と画素まで一致**する。DPI 144 のモニタでも窓 `-3850,-10,10,2170` に対して client は
  `-3840,0,0,2160`＝そのモニタの作業領域。`SW_RESTORE` で元の矩形に戻る。
  `FALSE` も自分で答えるようにしたあとも同じ値である（作業領域への収め方を矩形 1 つを受ける
  `fit_work_area` に括り、TRUE の `rgrc[0]` と FALSE の `RECT` の両方に当てているため）
- **最小サイズ**: 300×200 を頼むと、修正前は **300×200 になる**。修正後は **DPI 120 で 700×450・DPI 144 で 840×540** で
  止まる（96 DPI の 560×360 の 1.25 倍 / 1.5 倍・`MulDiv` の丸めどおり）。`FALSE` の変更のあとも同じ値
- **リサイズとドロワーの高さ**: 1200×800 → 820×560 にすると、ドロワーの子窓は `300×800` → `300×560` に追随し、
  右ペインの頭の札は新しい右端（client 820 − 20 = 800）に描かれる
  （[resize-wide.png](resize-wide.png) → [resize-narrow.png](resize-narrow.png)）。
  **残像そのものは写真に残せなかった**: `PrintWindow` は窓を描き直させるので、`InvalidateRect` の有無に関わらず
  新しい幅で描いた絵が返る。`GetUpdateRect` も `SetWindowPos` の直後に 60 回読んで修正前・修正後とも空だった
  （アプリ側が先に描き終えている）。決定 2 の効き目は施主の実機（実際のマウスで縁を掴む）での確認に残る
- **パンくずの省略**（DPI 120・x は窓の左端からの画素）:

  | 窓の幅 | 番号 | カテゴリ名 | 「/」 | ノート名 | 閲覧の札 | 編集の札 |
  | --- | --- | --- | --- | --- | --- | --- |
  | 1200 | 345..360 | 379..545（全部） | 565..570 | 592..890（全部） | 面 1050..1108 / 字 1065..1093 | 字 1135..1163 |
  | 900 | 345..360 | 379..545（全部） | 565..570 | 592..700 + 点 710 / 720 | 面 750..808 / 字 765..793 | 字 835..863 |
  | 760 | 345..360 | 379..466 + 点 475 / 485 | 507..512 | 534..572 + 点 581 / 590 | 面 610..668 / 字 625..653 | 字 695..723 |
  | 700（最小） | 345..360 | 379..395 + 点 404 / 414 | 447..452 | 474..512 + 点 521 / 530 | 面 550..608 / 字 565..593 | 字 635..663 |

  幅 1200 は省略なし（[breadcrumb-wide.png](breadcrumb-wide.png)）。幅 900 では**ノート名だけ**が省略され
  （[breadcrumb-note.png](breadcrumb-note.png)）、幅 760 と 700 では**カテゴリ名も**省略される
  （[breadcrumb-both.png](breadcrumb-both.png)）。**番号・「/」・札 2 つはどの幅でも残り**、ノート名は
  最小幅（96 DPI の 48 px＝DPI 120 で 60 px）を割らない（幅 700 で 474..530 = 57 px）。
  修正前の同じ幅 820 では、ノート名が**札の下をくぐって窓の右端（819）まで走り、「編集」の札が文字に潰される**
  （[breadcrumb-before.png](breadcrumb-before.png)）。修正前の幅 900 / 760 / 700 でも明るい字は 890 / 759 / 699＝
  いずれも窓の右端まで届いていた
- **札の当たり判定**: 幅 820 で client (770, 27) を押すと札の面が 740..798 へ移り「閲覧」が暗い字（685..713）になった
  ＝**新しい位置で「編集」の札が効く**（[breadcrumb-chip.png](breadcrumb-chip.png)）。(700, 27) を押すと閲覧へ戻り、
  md は **102 バイト・mtime とも不変**（本文を変えていないので書いていない）
- **DPI**: 主モニタ 120 のほか、**144** と **168** のモニタへ移した。移すと `WM_DPICHANGED` で OS の勧める矩形へ移り
  （120 → 144 で 960×640 が 1152×768）、そのあとも client と窓の差は 0 で、最小サイズも DPI に追随する
- 見ていないもの: **96 DPI**（この機械の 4 枚のモニタは 120 / 144 / 168 / 168 で 100% のモニタが無い。施主に依頼）。
  実際のマウスで縁を掴んだリサイズとスナップ（Win + ←→）・ドラッグ中の連続した `WM_SIZE`。
  **白い帯が施主の実機で消えたかどうか**（開発機では帯は一瞬しか出ず、消えたことの証拠は「作成直後から差 0 px」という
  矩形の数値だけである。帯そのものの写真は 1 度しか撮れていない）。リサイズ直後の残像（`PrintWindow` では写らない）。
  ライトのアプリのモードでの見え方（`AppsUseLightTheme` は 0 のまま触っていない）。
  作業領域が 560×360 より狭い画面。複数のモニタにまたがる最大化とスナップの半画面

**追補（2026-09-11・Issue #22 の再開・ADR 0014）**: 施主の実機で放置後に白い帯が再発した（索引側には出ず右ペイン側だけ）。
触らずに測ると、窓の矩形・client・`DWMWA_EXTENDED_FRAME_BOUNDS` は 4 辺とも一致（差 0）のまま、帯は窓の矩形の**内側** 7 px で、色は
`GetSysColor` の `COLOR_3DHIGHLIGHT` / `COLOR_INACTIVEBORDER`（`#F4F7FC`）/ `COLOR_BTNSHADOW` と一致した。帯は x=500（ドロワー子窓の
右端）から右にだけ出る＝子窓が上に描く所だけ消えて見える。放置中の OS のイベントは 0 件で、変わったのは前面の窓だけ。
同じ形の窓で再現すると、非アクティブ化の `WM_NCACTIVATE(wParam=0)` の既定処理がその場で枠を描き、`WM_NCPAINT` も `WM_PAINT` も届かない。
`WM_NCACTIVATE` → TRUE だけで帯は出ない（必要十分。`WM_NCPAINT` → 0 だけでは直らない）。

証拠（DPI 120・窓を PID で選び、`SetWindowPos` で (2000, 120) の最前面へ寄せ、自分の探針の窓を前面に出して非アクティブにし、
画面の画素を窓の矩形の外周から内側へ 12 px 読む。`PrintWindow` は使わない）:

| 版 | 状態 | 上辺 / 右辺 / 下辺（x = 3/4） | 左辺（ドロワー） |
| --- | --- | --- | --- |
| 修正前（main `20a03cb`） | アクティブ | `#24272C` `#FFFFFF`/`#A0A0A0` `#B4B4B4`×6 `#101214`… | `#24272C` `#FFFFFF` `#B4B4B4`×6 `#0A0B0D`… |
| 修正前 | 非アクティブ | `#24272C` `#FFFFFF`/`#A0A0A0` **`#F4F7FC`×6** `#101214`… | `#24272C` `#FFFFFF` **`#F4F7FC`×6** `#0A0B0D`… |
| 修正前 | 再アクティブ | `#B4B4B4`×6 に戻るだけで帯は消えない | 同じ |
| **修正後** | アクティブ | `#24272C` `#101214`×11 | `#24272C` `#0A0B0D`×11 |
| **修正後** | 非アクティブ | `#24272C` `#101214`×11 | `#24272C` `#0A0B0D`×11 |
| **修正後** | 再アクティブ | `#24272C` `#101214`×11 | `#24272C` `#0A0B0D`×11 |

修正前は**アクティブ化でも**帯が出る（`COLOR_ACTIVEBORDER` の `#B4B4B4`）。修正後は 3 状態とも地の色と DWM の縁だけ
（[frame-inactive-before.png](frame-inactive-before.png) / [frame-inactive-after.png](frame-inactive-after.png) /
[frame-reactivated-after.png](frame-reactivated-after.png)）。左辺の帯はこの測定では出ている（ドロワー子窓の**外**の縁）が、施主の実機では
子窓が縁まで届いているので見えなかった。角丸と縁の色 `#24272C` は修正後も生きている。

### 5-k. 保存の直前の本文を `data/.history` に 5 版まで残す（Issue #23・2026-09-10）

環境: Windows 11 Pro 10.0.26200・`build/NeNeFolio.exe`（Debug 構成＝ASan / UBSan / nullability 付き）・
`GetDpiForWindow` = **120**（client 1200×800）。OS のアプリのモードはダーク（`AppsUseLightTheme` は 0 のまま触っていない）。
`build/data/` は確認の前に `build/data.backup` へ退避し、終了後に戻した。**エクスプローラは撮らず、ディレクトリの中身は
すべてテキスト（名前・バイト数・CR / LF の数・先頭 3 バイト・mtime・本文）で読んだ。**
🔴 補助スクリプトは窓を `FindWindowW` ではなく **`build\NeNeFolio.exe` のプロセスの `MainWindowHandle`** で選ぶ。
施主が `out/hide/NeNeFolio.exe` を同時に起動しているので、クラス名と題名だけで探すと**施主の窓を掴む**（今回 1 度これで誤診した）。
失敗の窓が出る操作は `PostMessageW`（`SendMessageW` だと自分が待ち、相手のモーダルと相互に固まる）。

手順（`FindWindowExW` で `NeNeFolioDrawer` / `RICHEDIT50W` の子を取り、行のクリックは `WM_LBUTTONDOWN` /
`WM_LBUTTONUP`、札は主窓の client (1149, 27)、入力は `WM_CHAR`、Ctrl+S は `WM_CHAR` の 0x13。
撮影は `PrintWindow(hwnd, dc, 2)` の直前に `RedrawWindow`（RDW_ALLCHILDREN）を送る）:

1. `data/仕事/打ち合わせ.md`（**LF**・217 バイト）を選び「編集」の札で編集にして、末尾に `[v1]`…`[v6]` を 1 つずつ足しながら **6 回 Ctrl+S**
2. 本文を変えずにもう 1 回 Ctrl+S して、md と履歴 5 本の mtime とバイト数を突き合わせる
3. `data/仕事/週報.md` を **CRLF**（37 バイト・CR 3 / LF 3）にしてからアプリを**起動し直し**、ドロワーを撮る
4. 週報を編集して 1 回 Ctrl+S し、`1.md` を元の md とバイト単位で比べる
5. `data/.history/仕事/週報/` に**書き込みと削除を拒否する ACE**（`icacls … /deny "<user>:(D,W)"`）を付けて Ctrl+S。
   失敗の窓・md・RichEdit の本文・`ES_READONLY`・履歴の中身を見る。ACE は `/remove:d` で外す
6. 週報を `個人` へドラッグして移し、そのあと保存する
7. 5 版まで積んでから拒否を `(W)` だけ（削除は許す）にして保存し、履歴 5 本の mtime とバイト数を突き合わせる。
   さらに `DeleteFileW` と `MoveFileExW` を補助スクリプトから直接呼び、権限ごとの成否と `lastError` を測る
8. `1.md.tmp` を手で置いてから保存し、控えが残らないことと版のずれ方を見る

結果:

- **6 回の保存**: `data/.history/仕事/打ち合わせ/` に **`1.md`〜`5.md` の 5 本だけ**ができた。
  `1.md` = `[v1][v2][v3][v4][v5]`（237 バイト・直前の版）、`2.md` = `…[v4]`（233）、`3.md` = `…[v3]`（229）、
  `4.md` = `…[v2]`（225）、`5.md` = `[v1]`（221・最古）。**最初の版（217 バイト）は 6 回目で捨てられて残っていない**。
  どの版も CR 0 / LF 15・先頭 3 バイトは `23 20 E6`（BOM 無し）で、md と同じ改行の形のままである
- **同じ本文**: 変えずに Ctrl+S しても md と `1.md`〜`5.md` の mtime・バイト数が**1 つも動かない**（履歴は 5 本のまま）。
  `note_text_equals` で止まるので `archive_note` も `write_note` も呼ばれない（ADR 0012 の決定 2）

  | | md | 1.md | 2.md | 3.md | 4.md | 5.md |
  | --- | --- | --- | --- | --- | --- | --- |
  | Ctrl+S の前 | 23:11:12.631 / 241 | 23:11:12.630 / 237 | 23:11:12.068 / 233 | 23:11:11.518 / 229 | 23:11:10.970 / 225 | 23:11:10.411 / 221 |
  | Ctrl+S の後 | 同じ | 同じ | 同じ | 同じ | 同じ | 同じ |

- **走査**: `data/.history` がある状態で起動し直しても、ドロワーは `仕事` / `個人` / `読書` の 3 つだけで
  **`.history` はカテゴリとして出ない**（頭の数え上げも 5 本のまま・[history-drawer.png](history-drawer.png)）。
  `.` で始まるディレクトリを `accept_entry` が落としている（ADR 0012 の決定 4）
- **バイト列**: CRLF の `週報.md`（37 バイト・CR 3 / LF 3・先頭 `23 20 E9`）を 1 回保存すると、`1.md` が
  **保存前の md と 37 バイト全部が一致**した（CR / LF の数も先頭 3 バイトも同じ。BOM は付かない）。
  履歴は core を通さずファイルのバイト列をそのまま写すので、改行の形も BOM の無さも保たれる
- **履歴を書けないとき**: `(D,W)` を拒否した状態で Ctrl+S すると
  **「履歴を書けなかったので保存していません。編集中の本文は残っています。」**の窓が出た（[history-failed.png](history-failed.png)）。
  md は **43 バイト・mtime とも不変**、履歴も `1.md` 1 本が mtime・バイト数ともそのまま、`.tmp` も残らない。
  RichEdit には打った `[XYZ][ABC]` が**残り**、`ES_READONLY` は**落ちたまま**（編集モードのまま・[history-kept.png](history-kept.png)）
- **別カテゴリへの移動**: 週報を `個人` へ移したあとに保存すると、**`data/.history/個人/週報/1.md`** が新しくでき、
  **`data/.history/仕事/週報/1.md` は消えずに残った**（孤立する・ADR 0012 の決定 5）。移動そのものでは履歴のディレクトリは 1 つも動かない

  ```text
  移動の直後（保存の前）        保存の後
  .history\仕事\週報\1.md       .history\仕事\週報\1.md      ← 37 バイト・元の場所に残る
                                .history\個人\週報\1.md      ← 43 バイト・移動先に積み始める
  ```

- **書けない環境では履歴を 1 つも触らない**（ADR 0012 の決定 3・改訂後）。5 版まで積んだ状態で
  拒否を `(W)` だけ（**削除は許す**）にして Ctrl+S すると、失敗の 1 行（`(D,W)` のときと同じ窓）が出て
  **`1.md`〜`5.md` は 5 本とも mtime とバイト数がそのまま**・md も不変・`1.md.tmp` も残らない。
  RichEdit の本文と `ES_READONLY` の落ちたままも `(D,W)` のときと同じである。

  | | 1.md | 2.md | 3.md | 4.md | 5.md | md |
  | --- | --- | --- | --- | --- | --- | --- |
  | Ctrl+S の前 | 23:34:33.363 / 237 | 23:34:32.800 / 233 | 23:34:32.242 / 229 | 23:34:31.692 / 225 | 23:34:31.128 / 221 | 23:34:33.370 / 241 |
  | Ctrl+S の後 | 同じ | 同じ | 同じ | 同じ | 同じ | 同じ |

  新しい版を先に `1.md.tmp` へ書き切る順序にしたので、書けない環境では**その 1 手目で止まる**。
  順序を変える前（削除 → 改名 → 書く）は同じ操作で **5 本が 4 本へ減っていた**。理由は Win32 の権限の分かれ方で、直接測った:

  | 操作 | `(W)` を拒否したディレクトリで | 意味 |
  | --- | --- | --- |
  | `CreateFileW(1.md.tmp.tmp, CREATE_ALWAYS)` | **失敗** | 1 手目で止まる＝履歴も md も動かない（現在） |
  | `DeleteFileW(5.md)` | **成功**（lastError 0） | `(W)`＝`FILE_GENERIC_WRITE` に `DELETE` は入らない。旧い順序ではここで最古の版が消えていた |
  | `MoveFileExW(4.md → 5.md, REPLACE_EXISTING)` | **失敗**（lastError **5** = ACCESS_DENIED） | 改名は `FILE_ADD_FILE` を要るので、置き換えの旗でも通らない |

- **ローテーションが原子的でないこと**（決定 3 に残る）: 落ちうるのは 2〜4 手目で、そのときは `2.md`〜`5.md` だけが残りうる。
  改名に `MOVEFILE_REPLACE_EXISTING` を付けてあるので、相手が残っていても連鎖は止まらない。この旗の効き目も直接測った
  （普通のディレクトリで `5.md` を置いたまま `4.md → 5.md`）:

  | 旗 | 結果 |
  | --- | --- |
  | `MOVEFILE_WRITE_THROUGH` だけ | **失敗**（lastError **183** = ALREADY_EXISTS）。以降の `3 → 4` も相手が空かないので**連鎖が全部止まる** |
  | `MOVEFILE_REPLACE_EXISTING` を足す（現在） | **成功**。`5.md` の中身が `4.md` のものへ替わり、連鎖が最後まで走る |

- **取り残された控えの上書き**: 前に落ちた体で `1.md.tmp`（13 バイトの別物）を先に置いてから保存すると、
  **`1.md.tmp` は残らず**、履歴は 5 本のまま正しく 1 つずつずれ（`1.md` = 直前の版・前の `1.md` が `2.md` へ）、
  置いた中身はどの版にも混ざらなかった
- 見ていないもの: 2〜4 手目で落ちる経路（`2.md`〜`5.md` だけが残る状態）を実機で作ること・容量不足での失敗・
  `data/` ごと別の機械へ写したときの履歴・外部で md を書き換えてからの保存・
  ライトのアプリのモードでの失敗の窓・96 DPI（施主に依頼）・戻す操作（この縦切りには無い）

### 5-l. 索引と本文の 2 区画・vim の鍵・モードの持続（Issue #24・2026-09-11）

環境: Windows 11 Pro 10.0.26200・`build/NeNeFolio.exe`（Debug 構成＝ASan / UBSan / nullability 付き）・
主窓を `SetWindowPos` で主モニタの (200, 120) へ寄せ、`GetDpiForWindow` = **120**（client 1200×800・ドロワー 300×800）。
撮影する側も Per-Monitor v2（5-e の教訓）。OS のアプリのモードはダーク（`AppsUseLightTheme` は 0 のまま触っていない）。
補助スクリプトは窓を `build\NeNeFolio.exe` のプロセスの `MainWindowHandle` で選ぶ（5-k の教訓。施主が `out/hide` を同時に起動している）。
`build/data` は確認の前に `build/data.backup` へ退避し、**4 カテゴリ（仕事 #5B8DEF 10 本・改行 LF / 個人 #8B7CF6 9 本 /
読書 #2FB8A6 8 本 / 資料 #E4A11B 9 本・仕事以外は CRLF）・ノート 36 本・全展開**に差し替えて、終了後に元へ戻した。

DPI 120 の寸法（`MulDiv` の丸め）: `top_padding` 55・カテゴリ行 43（上に間 8）・ノート行 38・`bottom_padding` 20・フェード 30。
内部の座標は 仕事 63..106 / 仕事-01〜10 が 106..486、個人 494..537 / 個人-01〜09 が 537..879、読書 887..930 / 930..1234、
資料 1242..1285 / 1285..1627。下端 **1627**・スクロール上限 **847**。

手順（`FindWindowExW` で `NeNeFolioDrawer` / `RICHEDIT50W` の子を取る）:

1. 索引の鍵は主窓へ `WM_KEYDOWN` を `PostMessageW` で 1 つだけ投げる。メッセージループの `TranslateMessage` が
   実際の打鍵と同じ `WM_CHAR` を作るので、**1 打 = 1 回**になる。
   🔴 `WM_KEYUP` を `lParam` の bit31 を立てずに投げると `TranslateMessage` が**もう 1 つ `WM_CHAR` を作り、1 打が 2 回**になる
   （今回 1 度これで誤診した）。`G` だけは Shift の状態を合成できないので `WM_CHAR`（0x47）を直に送る
2. 本文へは RichEdit へ `WM_KEYDOWN`（`VK_ESCAPE`）と `WM_CHAR`。札は主窓の client (1149, 27)、行のクリックは
   `WM_LBUTTONDOWN` / `WM_LBUTTONUP`
3. フォーカスは**窓を前へ出してから** `GetGUIThreadInfo` の `hwndFocus` で読む（活性でないスレッドの `hwndFocus` は 0 になる。
   Windows は非活性化の直前のフォーカスを覚えていて活性化で戻すので、前へ出してから読めばその時点の区画が読める）
4. スクロール量は「内部の上端 − 面（#16191D）の上端」。上端の帯（表示 55..85）と下端（770..800）はフェードが混ざって
   面の色と一致しないので、上端が帯に入る位置では**面の下端から行の高さ 38 を引いて**求めた
5. 撮影は `PrintWindow(hwnd, dc, 2)` の直前に `RedrawWindow`（RDW_ALLCHILDREN）

結果:

- **起動直後**（何も選んでいない・`paneLen` 0・フォーカスは索引）に `j` を 1 回で **仕事-01** が選ばれ（面の上端 106 = 内部 106・量 0）、
  右ペインに RTF が出て（長さ 20・本文の頭は `仕事-01\r\n` = 見出しの `# ` が消えている）、**フォーカスは索引のまま**
- **`j` を 10 回**で 仕事-10（面 448 = 106 + 38×9）、**11 回目**でカテゴリ行を跳び越えて 個人-01（面 537 = 内部 537）
- **`G`** で 資料-09（内部 1589）。**ドロワーがスクロールして**面の上端が **762** に来た（＝量 **827** = 1627 − 800。
  行の下端がちょうど viewport の下端）。そこでさらに `j` を送っても **762 のまま**動かない（[focus-reveal.png](focus-reveal.png)）
- **`gg`** で 仕事-01 に戻り、面の上端が **55**（＝量 51 = 106 − 55。行の上端がちょうど頭の帯の下）。そこで `k` を送っても **55 のまま**
- **`G` `g` `k` `g`** の順に送ると 資料-08（内部 1551・面 724）で止まり、**`gg` にならない**（`k` の `WM_KEYDOWN` が 1 打目の `g` を忘れさせる）
- **`h` / `l`**: 仕事-01 を選んだ状態で `h` を送ると `categories.json` の `仕事` が `expanded: false` になり（mtime が動く）、
  選択行は行が無くなるので画面から消える。`l` で `true` に戻って面が 55 へ戻る。**もう一度 `l` を送っても
  `categories.json` の mtime は 1 ミリ秒も動かない**（同じなら書かない）。もう一度 `h` で畳んでから `j` を送ると
  **折り畳んだ 仕事 の中を飛ばして** 個人-01（内部 157・面 106）へ行き、そこで `k` を送っても**前が折り畳みなので止まる**
- **`Enter` と `Esc`（閲覧）**: `Enter` でフォーカスが `RICHEDIT50W` へ移り、RichEdit へ `Esc` を送ると索引へ戻る。
  `ES_READONLY` は落ちない（閲覧のまま）
- **主窓の Escape**: 2 回送っても**プロセスは生きたまま**で、選択も変わらない（窓を閉じるのは Alt+F4 と閉じる操作だけ）
- **フォーカスの印**（選択行の右端の角・8×8 画素を読んだ）:

  | 区画 | x 272..279 の 8 行 |
  | --- | --- |
  | 索引（塗り） | 8 行すべてが `8B7CF6`（個人の色）で埋まる（[focus-index.png](focus-index.png)） |
  | 本文（枠 1px） | 外周 1 画素だけが `5B8DEF`、内側 6×6 は選択面の `16191D`（[focus-pane.png](focus-pane.png)） |

- **「編集」の札 → 入力 → `Esc`**: 札で `ES_READONLY` が落ち、フォーカスが RichEdit へ移り、本文が**平文**（頭が `# 仕事-01`）になった。
  末尾に `ABC` を打って RichEdit へ `Esc` を送ると、**フォーカスは索引へ戻り、`ES_READONLY` は落ちたまま**（編集モードのまま）。
  `仕事-01.md` は 31 → **34 バイト・CR 0 / LF 3**（LF のまま）・先頭 `23 20 E4`（BOM 無し）・末尾 `… 2D 30 31 0A 41 42 43`。
  `data/.history/仕事/仕事-01/1.md` に**保存前の 31 バイト**が積まれた
- **編集のまま `j`**: 選択が 仕事-02 へ動き、**`ES_READONLY` は落ちたまま・本文は平文**（頭が `# 仕事-02`）・**フォーカスは索引**
  （[focus-edit.png](focus-edit.png)）。直前の 仕事-01 は保存の直後で本文が変わっていないので **mtime が動かない**（同じなら書かない）
- **切り替えが保存すること**: 索引にフォーカスを置いたまま RichEdit へ `PQR` を足し（md は 38 バイトのまま）、`j` を送ると
  **仕事-02.md が 41 バイト**（末尾 `… 44 45 46 47 48 49 6A 50 51 52`）になり、履歴が 1 つずれた
  （`1.md` = 38 バイト = 直前の版・`2.md` = 34・`3.md` = 31）。**保存のたびに履歴が積まれる**（ADR 0012）
- **編集のまま `k`**: 次の 仕事-03 で `MNO` を足して `k` を送ると、**仕事-03.md が 34 バイト**になり
  `data/.history/仕事/仕事-03/1.md`（31 バイト）ができ、選択は 仕事-02（面 144）へ戻る。`ES_READONLY` は落ちたまま
- **本文の `j` はただの文字**: フォーカスが本文にあるとき `j` を打つと RichEdit の長さが 1 増えるだけで、選択は動かない
- **保存に失敗する状態**: `仕事-01.md` に読み取り専用属性を付け、編集のまま本文を足して `j` を送ると
  **「ノートを書き戻せませんでした。編集中の本文はそのままです。」**の窓が出て（[focus-failed.png](focus-failed.png)）、
  **選択は 106 のまま動かず**、RichEdit の本文（長さ 32）も `ES_READONLY` が落ちたままの状態も残り、md は 34 バイトのまま。属性は戻した
- **ノート行のクリック**: 編集のまま行をクリックしても `ES_READONLY` は落ちたままで、本文は平文（頭が `# 仕事-08`）に置き換わり、
  フォーカスは索引。鍵と同じ経路（`folio_message_select_note` → `switch_note`）を通っている
- **索引の区画での Ctrl+S**: 編集のまま本文に `ABC` を打ち、ドロワーの頭の帯をクリックして**区画だけを索引へ移す**と
  （行ではないので切り替えも保存も起きず md は 31 バイトのまま・`ES_READONLY` は落ちたまま・RichEdit の長さ 29）、
  そこで `x` を打っても本文は伸びない（29 のまま）が、**主窓へ `WM_CHAR` の 0x13（Ctrl+S）を送ると 34 バイトになり**
  `data/.history/仕事/仕事-01/1.md` に保存前の 31 バイトが積まれた。フォーカスも編集モードもそのまま。
  「閲覧」の札で閲覧へ戻してから同じ 0x13 を送ると、**失敗の窓は出ず md の mtime も動かない**（閲覧では何もしない）
- 見ていないもの: **IME が ON のときの `j`**（`WM_CHAR` に届かないことは ADR 0013 の「失う・残る」に記録。対策しない）・
  96 DPI と高 DPI（144 以上）での寄せ方と印の見え方・**実際のキーボードでの打鍵**（合成した鍵だけを見た。
  `keybd_event` でも同じ結果になることは別に確かめたが、記録は合成の方に統一した）・
  ライトのアプリのモードでの印の見え方・鍵の連打で保存が追いつかないときの手触り・
  マウスで本文をクリックして区画を取る経路（`Enter` と札だけを見た）
