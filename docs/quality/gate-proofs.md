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
