# ADR 0003 — C23 / clang-cl の検査基盤と実測できた限界を固定する

- 状態: 受理
- 日付: 2026-09-09
- Issue: #1
- 影響する規則: ARC-002 / ARC-003 / ARC-007 / C-002 / C-003 / C-004 / C-005 / C-012 / C-015 / C-016 / QLT-001 / QLT-007 / QLT-011 / CNF-001 / CNF-006

## 文脈

Phase 0 の実測（MSVC 19.44.35228 / LLVM 19.1.5 / 69 ケース）は `eng/measure-language.ps1` と `eng/probes/language.json`
から再現し、記録は `docs/quality/phase0-results.json`。要点は 4 つ。

1. **MSVC `cl` は C23 を話さない。** `/std:clatest` でも `nullptr` / `typeof` / `bool` が C2065 / C2061 で落ちる
   （C10-c23-nullptr-msvc-hole・C10-c23-bool-typeof-msvc-hole）。同じ Visual Studio に同梱の clang-cl 19.1.5 は
   `/clang:-std=c23` で `nullptr` / `typeof` / `constexpr` / 固定底の enum / `[[nodiscard]]` / `#embed` を全部通した（K1-*）
2. **C で塞げるものは clang の名指しの診断で塞ぐ。** `-Wswitch-enum` / `-Wcovered-switch-default`（K3）、`-Wcast-qual`（K13）、
   `-Wnullability-completeness`（M3-nullability-clang）、`-Werror=vla`（K2）、`-Wmissing-prototypes`、`-Wconversion` / `-Wsign-conversion`（C12）、
   `-Wimplicit-fallthrough`（C8）。`-Wvla` だけでは C23 で通る（K2-vla-plain-hole）
3. **決定性と依存方向はリンカ段で見る。** `time()` は CRT ヘッダで `_time64` に写像されるので、ソース上の名前で `dumpbin` を grep しても
   見つからない（L1-dumpbin）。`llvm-nm --undefined-only` は `_time64` と `__imp_GetTickCount` をそのまま出す（L1 / L2）。
   中核の静的ライブラリの未定義シンボルを `eng/symbol-allowlist.json` と完全一致で照合すれば、時刻・乱数・環境・OS import・
   宣言外の依存を 1 つの検査で拒否できる。これは C++（Loupe）の字句検査より強い
4. **言語で塞げない穴。** 範囲外 enum（K12）、`malloc`＋キャストの偽造（K14）、`{0}` のゼロ値（K15）、`uintptr_t` 経由の `const` 洗浄
   （M2-const-launder）、`#pragma clang diagnostic ignored` による `-Werror` の抑制（K4）、ファイル先頭の全面抑制（M8）、
   そしてメモリ安全そのもの。ASan は両コンパイラで、UBSan は clang-cl で発火を実測した（A1 / A2）が、検出は防止ではない

## 決定

**clang-cl（LLVM 19.1.5、Visual Studio 同梱）を唯一の C コンパイラとし、MSVC の toolset はリンカ・SDK・`dumpbin` のためだけに入れる。
警告集合は `eng/targets.cmake` の 1 か所に書き、中核の外部シンボルを `llvm-nm` で許可リストと照合する。**

- 言語は C23（`/clang:-std=c23`。CMake 3.31 は clang-cl の `CMAKE_C_STANDARD 23` を知らず configure が落ちる。
  clang-tidy の cl ドライバは compile line の `/clang:` 引数を落とすので、同じ変数 `NENEFOLIO_C_STANDARD_FLAG` を `--extra-arg` でも渡す。
  `CMAKE_C_EXTENSIONS OFF` を書くと CMake が `/std:c17` を足し、clang-cl が `-Wunused-command-line-argument` で拒否するので書かない。
  3 点とも 2026-09-09 のフルゲートで実測した）。`-Wno-pre-c23-compat` は互換性警告であって安全性の警告ではないので外す。`-Wno-switch-default` は
  本規約と逆向きの規則（`default` の強制）なので外し、`-Wcovered-switch-default` を使う。名指しの `-Wno-<診断名>` は CNF-005 の無力化検査の対象外
- 整形の正本は `.clang-format`、lint の正本は `.clang-tidy`。clang-analyzer の null 逆参照・未初期化・`unix.Malloc`・`insecureAPI`、
  `bugprone` の 2 つ、`readability` の複雑度 10・長さ 60 行・ネスト 3・引数 4 を名指しで入れる。一括の lint 群は採用しない
- モジュールの許可グラフは `eng/architecture.json`。実ターゲットは具体的な責務とソースがある時だけ作る。今回作る C ターゲットは
  C23 基盤のスモークテストだけで、製品モジュールは作らない
- 規約検査は `eng/conformance.py`（Python 標準ライブラリのみ）。検出設定は `eng/conformance-rules.json`。C ソースの字句検査は
  マクロ展開・別名解決の代わりではないので、決定性と依存の正は `eng/symbols.py` に置き、字句検査は補助とする
- `eng/symbols.py` は `--build-dir` で core / application の静的ライブラリを CMake File API から見つけて検査する。ライブラリが 0 個の
  間は「0 librar(ies) checked」で通る＝**何も守っていない**。中核が生まれたら `--require core application` を結線して初めて active
- C は抑制を言語で封じられないため waiver 台帳を持つ。行単位の `NOLINTNEXTLINE` は直前の有効な waiver と明示的な検査名が必要。
  ファイル単位の pragma / `NOLINTBEGIN` は認めない
- 中核の型は不完全型で公開する（C-003 / C-007）。`{0}` のゼロ値と直接コピーを書けなくする唯一の言語機能だから
- 単体テストは ASan / UBSan 付きでビルドし `-fno-sanitize-recover=all` で必ず落とす（結線は最初の中核実装の Issue で）
- 中核の分岐カバレッジ目標は 90%。LLVM の計装と `llvm-cov` が C で動くことは実測した（V1）が、測る対象がまだ無いので QLT-009 は planned
- UI と application の状態遷移は単一の UI スレッドで行う。背景スレッドの追加には新しい ADR が必要
- C ランタイムは `/MT` で静的に結ぶ。`/MT` の exe が KERNEL32 しか import しないことは実測した（R1）。Phase 4 の成果物検査の根拠

## 強制

規則ごとの正本は `docs/QUALITY_GATES.md`。自作検査は正例・反例の自動テスト（`tests/conformance`）をゲートに結び、
実ツールの発火は `eng/prove-gates.py` が毎回確かめ、証拠を `docs/quality/gate-proofs.md` に残す。CI / ruleset は実際に反映・確認するまで planned。

## 結果

- Windows の道具と既定ロケール・環境に触れる検証スクリプトは開発時の道具であり、製品の ARC-007 の区画には含めない。検査器自身は
  日付を引数でも受け、期限のテストを固定できる
- 検査スクリプトの依存は Python / PowerShell 標準機能のみ。実行時依存は 0
- 緑は検査基盤の正常性を示し、Folio が動作することは示さない
- 🔴 メモリ安全・範囲外 enum・偽造・洗浄・pragma 抑制は言語では塞げない。対応する規則は「検出層で守る」「書き方で狭める」と書き、
  塞げたとは書かない

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| MSVC `cl`（Loupe と同じ） | C23 が通らない（C10-*-msvc-hole）。`-Wcast-qual` / `-Wswitch-enum` / nullability / UBSan 相当が無い |
| `cl` と `clang-cl` の二本立て（`/analyze` を静的解析層に） | 「どちらのコンパイルが正か」が揺れる。clang-analyzer が C6011 相当の null 逆参照を拒否する（T2） |
| `dumpbin` をソース上の API 名で grep する | `time` → `_time64` の写像で空振りする（L1-dumpbin）。シンボル名の完全一致に変える |
| `-Wvla` だけで VLA を禁じる | C23 モードで通った（K2-vla-plain-hole）。`-Werror=vla` |
| clang-cl に `-Wall` を渡す | clang-cl は `-Wall` を `/Wall`＝`-Weverything` として扱い、`-Wc++98-compat` や `-Wpre-c23-compat` まで有効になって C23 の `constexpr` が落ちる（K16-clang-cl-Wall-is-Weverything-hole）。`/W4`（＝`-Wall -Wextra`）を使い、群は入れない |
| `-Wswitch-default` を残す | 網羅済みの `default` を禁じる C-002 と逆向き。両立しない |
| 全 lint の有効化 | 規則ごとの責務と採用理由が失われる |
| 未実装の層を空のライブラリで作る | 具体的な責務がない将来用モジュールになる。`symbols.py` の「0 個で通る」を active と偽ることになる |
| カバレッジ 90% を active と記録する | 中核の分岐も測定対象もまだ存在しない |

## 参考

- [clang diagnostic flags](https://clang.llvm.org/docs/DiagnosticsReference.html)
- [clang-tidy function-size](https://clang.llvm.org/extra/clang-tidy/checks/readability/function-size.html)
- [MSVC C language conformance](https://learn.microsoft.com/en-us/cpp/build/reference/std-specify-language-standard-version?view=msvc-170)
- NeNe Loupe ADR 0003（C++ の実測。19 ケース）
