# ゲート発火の証明 — NeNe Folio

> Status: 記録 / 最終実測 2026-09-09（Issue #1・Phase 1 の足場に対するフルゲート。製品コードは 0 行）
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
| QLT-002 | 未使用変数・プロトタイプ無し・lint 違反・整形違反 | eng/prove-gates.py / 実 CMake ビルド・clang-tidy・clang-format | `-Wunused-variable` / `-Wmissing-prototypes` / `readability-function-size` / `clang-format-violations` で非 0。各復帰は 0 |
| QLT-004 | 一行に詰めた main | eng/prove-gates.py / clang-format --dry-run --Werror | `clang-format-violations` で非 0。元の整形は 0 |
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

**復帰の確認**: 2026-09-09。P1〜P4・P8・P13〜P17 は `eng/prove-gates.py` が各反例の直後に元へ戻して build / configure / clang-format / symbols を再実行し、終了コード 0 を確かめた（13 件）。P5〜P7・P9〜P12 は正例テストが同じ suite にある（58 テスト）。最後にフルゲート全体が終了コード 0 で `NeNe Folio full gate passed` を出した。

**除外側の確認**: 例外区画について「禁止が効いていること」と「唯一の窓口が通ること」の両方を見る。

| 区画 | 適用しない禁止 | 呼んでいる禁止 API | 結果 |
| --- | --- | --- | --- |
| `src/adapters/win32` | 決定性 | （中核も adapters もまだ無い） | 未実測。`eng/symbols.py` は 0 ライブラリで通っており、**いまは何も守っていない**（ADR 0003） |

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

（まだ無い）
