# ADR 0001 — 厳格さは機械で強制する

- 状態: 受理
- 日付: 2026-09-09
- Issue: #1
- 影響する規則: すべて

## 文脈

本リポジトリの規約は、AYANE 厳格規約ポリシー（`_work/reports/ayane-strict-repo-policy/POLICY.md`）と、
先行する 5 リポジトリ（NENE-PIXEL＝Kotlin / nene-recall＝Go / xi-tools＝Rust / NeNeCommander＝C# / NeNeClock＝Java）で
確立された考え方を C (C23, clang-cl) へ持ち込んだものである。共通する中核は 1 つに要約できる。

> **一つのことを実現する方法を 1 つに固定し、そのことを人の記憶ではなく機械に守らせる。**

規約は、破ったときに何も起きなければ、時間とともに必ず腐る。

### C (C23, clang-cl) で実測したこと

2026-09-09 に MSVC 19.44.35228 と clang-cl 19.1.5（同じ Visual Studio に同梱）で 69 ケースを実測した。
再現は `pwsh -NoProfile -File ./eng/measure-language.ps1`、全記録は [phase0-results.json](../quality/phase0-results.json)。

```
M1-exhaustive (cl /we4061 /we4062): exit 2 / C4062  列挙子 'MODE_EDIT' はハンドルされません
K3-strict-missing-case (clang-cl -Wswitch-enum): exit 1 / enumeration value 'MODE_EDIT' not handled in switch
K3-strict-covered-default (clang-cl): exit 1 / default label in switch which covers all enumeration values
M2-opaque (cl): exit 2 / C2037  'note' の左側は未定義の struct/union を指定しています
K13-const-cast (clang-cl -Wcast-qual): exit 1 / cast from 'const struct note *' drops const qualifier
M2-const-launder (clang-cl -Wcast-qual): exit 0   ← uintptr_t 経由の洗浄は通る
M4-private / M4-copy (cl): exit 2 / C2079 不完全型の変数・コピーは作れない
K14-opaque-forge-hole (clang-cl): exit 0   ← malloc＋キャストで偽造できる
K5-null-deref-compiles-hole: exit 0 → T2 clang-analyzer-core.NullDereference: exit 1
K7-uninitialized: exit 1 / variable 'x' is uninitialized
K2-vla-werror (-Werror=vla): exit 1 / K2-vla-plain-hole (-Wvla): exit 0
K4-pragma-bypass-hole: exit 0   ← #pragma clang diagnostic ignored が -Werror を抑制
M5-time / M6-win32: exit 0 → L1 llvm-nm: U _time64 / L2 llvm-nm: U __imp_GetTickCount
L1-dumpbin-grep-time-misses: dumpbin を 'time' で grep しても見つからない（写像先は _time64）
C10-c23-nullptr-msvc-hole (cl /std:clatest): exit 2 / C2065 'nullptr': 定義されていない識別子
K1-c23-nullptr / bool-typeof / constexpr-enum-fixed / embed (clang-cl /clang:-std=c23): exit 0
A1-asan (cl・clang-cl とも): AddressSanitizer: heap-buffer-overflow / A2-ubsan (clang-cl): signed integer overflow
R1: /MT の exe は KERNEL32.dll しか import しない
```

| 先行事例の規則 | C (C23, clang-cl)（実測） |
| --- | --- |
| 不正状態を表現不能に・網羅性 | 不十分。`-Wswitch-enum` と `-Wcovered-switch-default` で分岐漏れと網羅済み `default` を拒否。範囲外 enum はキャストで作れる（K12） |
| 公開状態は不変 | 不十分だが C++ より届く。不完全型は外から触れない（M2-opaque）。`const` 剥がしは `-Wcast-qual`（K13）。`uintptr_t` 洗浄は通る |
| `null` の意味は一つ（ゼロ値・未初期化の穴） | 不十分。未初期化は拒否（K7）。`nullptr` 逆参照は clang-analyzer が拒否（T2）。`{0}` のゼロ値は通る（K15） |
| 非公開コンストラクタ＋唯一のファクトリ（迂回経路） | ある（opaque に限る）。直接構築もコピーも不能（M4）。`malloc`＋キャストの偽造は通る（K14） |
| 中核の決定性を構文的に塞げるか | 言語では無い（M5）。リンカ段の `llvm-nm` で `_time64` / `__imp_GetTickCount` を捕まえる（L1 / L2）。名前でなくシンボルを見る |
| 依存方向をビルドが拒むか（標準同梱の枠組みの漏れ） | 言語では無い（M6）。CMake の宣言グラフ＋File API＋`llvm-nm` の `__imp_*` 検査で補う |
| 抑制を言語で封じられるか | 無い。`#pragma clang diagnostic ignored` で `-Werror` を抑制できる（K4）。waiver 台帳を持つ |
| baseline を作る経路が塞がっているか | 言語では無い。ファイル先頭の pragma で全面抑制できる（M8-file-suppression）。CNF-003 / CNF-005 で補う |

**そして、いま実装が空である。** 規則を後から被せる場合に必要になる baseline が、今は要らない。
違反ゼロから始められる時点は今しかない。

## 決定

**すべての規範規則に「機械強制の状態」を持たせ、その状態を文書の側で機械検査する。**

1. 規則の正本を `docs/ARCHITECTURE_CONSTITUTION.md` / `docs/CODING_RULES.md` / `docs/QUALITY_GATES.md` に置く。
   すべての規則に ID を与え、状態を **active / planned / 不能 / 不採用** で明示する。**planned を active と書かない**
2. `pwsh -NoProfile -File ./eng/check.ps1` を唯一の入口とする。CI は同じコマンドを呼ぶだけにし、CI 側にしか無い検査を作らない
3. baseline を持たない。例外は期限付きの狭い waiver だけ
4. 抑制には理由と規則 ID を要求する（台帳を持つ。C は抑制を言語で封じられない＝K4）
5. ゲートを弱める変更は ADR を要する
6. 道具の版は eng/tool-versions.json だけが決める。2 か所に書かない
7. **ゲートは意図的な違反で発火することを証明してから active と書く**（`docs/quality/gate-proofs.md`）
8. 規約検査（`eng/conformance.py`）は依存ゼロで書く

## 強制

- **planned**: CNF-006（文書整合）・CNF-005（baseline 禁止）・CNF-004（waiver 期限）。実装後に active へ
- **不能**: 「`planned` を勝手に `active` と書き換えないこと」そのもの。マトリクスの行と実装の対応は、いまは人が見るしかない

## 結果

得られるもの:

- 規約が自分について嘘をつく経路が塞がる
- 新しい貢献者（人でも AI でも）が「何がいま守られているか」を 1 か所で読める
- 規約を緩めるコストが上がる

失うもの:

- 文書のメンテナンスコスト。規則を足すたびに 3 か所（本文・マトリクス・実装）が同期を要求される
- 検査の実装そのものの保守
- 書き味の制約: 中核の型を全部不完全型にするので、呼び出し側は値をコピーできず、必ずポインタと関数を経由する。Win32 の生のハンドルとメッセージを閉じた意図と表示値へ変換する境界が要る

**正直に記録しておくこと:**

- 🔴 メモリ安全（境界外アクセス・二重解放・解放後使用）は言語では塞げない。ASan / UBSan の**検出**を単体テストに常時付けるが、検出は防止ではない（C-016）。範囲外 enum・`malloc` 偽造・`uintptr_t` 洗浄・`{0}` ゼロ値・pragma による抑制も言語では塞げず、字句検査と規約で狭めるだけである
- **本 ADR の厳格さは、実装がまだ空である今だからこそ無傷で導入できた。** 緑であることは、規則が良いことの証明ではない。
  検査対象がまだ小さいことの結果でもある。実装で規則が邪魔になったとき、緩めるのではなく **ADR で判断を残すこと**が本 ADR の眼目である

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 散文の規約だけを置く | 守られているかを確かめる手段が無く、時間とともに必ず腐る。**本 ADR の出発点そのもの** |
| 既製 lint の既定セットだけで済ませる | 既製ルールは「C (C23, clang-cl) として危ういこと」を見るが、「NeNe Folio として守るべきこと」は見られない。規則と lint の対応が付かず「なぜ有効か」を説明できない |
| 全 lint の一括有効化 | 相互に矛盾する lint が同時に入り、規則ではなく道具の機嫌に従うことになる |
| baseline を作って既存違反を後回しにする | 新規リポジトリなので既存違反が無い。ここで baseline を許すと、以後の違反も同じ入口から入ってくる |
| 実装が入ってから導入する | 既存違反を凍結する baseline が必要になる。今なら違反ゼロで始められる |
| 規約を `CLAUDE.md` にだけ書く | 人間と AI の遵守に依存する。前例（nene-recall）で実証済みの失敗——規約は CLAUDE.md にあったが、守っていたのはテスト 10 ケースだけで、新しく書かれるコードには及んでいなかった |
| 規則 ID を付けず散文で参照する | 「どの規則の話か」がレビューのたびに揺れる。ID があると tooling・ADR・PR・waiver が同じ語を指せる |
| MSVC `cl` を C のコンパイラにする | `/std:clatest` でも C23（`nullptr` / `typeof` / `bool`）が通らない（C10-*-msvc-hole）。`-Wcast-qual` / `-Wswitch-enum` / nullability / UBSan 相当が無い。同じ VS に同梱の clang-cl 19.1.5 を採る（ADR 0003） |
| `dumpbin` の出力をソース上の名前で grep して決定性を検査する | `time()` は CRT ヘッダで `_time64` に写像され、`time` では見つからない（L1-dumpbin）。`llvm-nm --undefined-only` のシンボル名を許可リストと完全一致で照合する |

## 関連

- 先行: NENE-PIXEL `docs/ADR`、nene-recall ADR 0010、xi-tools ADR 0001、NeNeCommander ADR-0001、NeNeClock ADR 0001
- ポリシー: `/home/xi/docker/_work/reports/ayane-strict-repo-policy/`
- 道具の選定と実測できた限界の詳細: [ADR 0003](0003-c23-clang-cl-foundation-and-measured-limits.md)
