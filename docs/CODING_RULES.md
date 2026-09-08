# C (C23, clang-cl) コーディング規約 — NeNe Folio

> Status: normative（規範）/ 2026-09-09 初版
> 判断の根拠は [ADR 0001](adr/0001-strictness-is-mechanically-enforced.md)。
> 本書は C (C23, clang-cl) を本リポジトリで承認された部分集合に狭めるためのものであり、
> ここが沈黙している領域は公式の C (C23, clang-cl) 標準に従う。

読み方（**active / planned / 不能 / 不採用**）と強制の層は
[ARCHITECTURE_CONSTITUTION.md](ARCHITECTURE_CONSTITUTION.md) 第 0 節と同じ。

---

## 1. 型と状態（C-0xx）

### C-001 — 境界でのプリミティブ執着を禁じる

識別子・寸法・版番号のように**単位や不変条件を持つ値**は専用の型にする。生の値を深いところまで運ばない。検証は生成時に行う。

- 機械強制: **planned**（「専用型にすべき値かどうか」はレビュー事項）
- 補助: C-007（唯一のファクトリ）により、型を作れば必ず検証される

### C-002 — 閉じた選択肢は閉じた型で表し、網羅性検査を殺さない

モードや状態機械を boolean の組み合わせ・マジック整数・裸の文字列で表さない。
分岐に `default` / `else` / `_` を書かない。**書いた瞬間に検査は死ぬ。**

- 機械強制: **planned** → `-Wswitch-enum`（分岐漏れ）と `-Wcovered-switch-default`（網羅済みの `default`）を `-Werror` で（K3・実測 2026-09-09）。範囲外の enum 値はキャストで作れる（K12）ので、enum は境界で範囲検査してから受け取る（C-007）。if/else の意味分類は未完了

### C-003 — 公開状態は不変

不変条件を持つ型は**ヘッダに不完全型だけ**を置く（`struct note_id;`）。メンバーは実装ファイルの中でだけ定義し、読み出しは関数で行う。
公開してよい完全型は「全メンバーが独立に妥当な値の集まり」（座標・寸法・色）だけで、その場合も呼び出し側は `const` ポインタで受ける。
可変な配列・バッファを返さない。外から受け取った可変データは複製して所有する。

- 機械強制: **planned** → 不完全型へのメンバーアクセスはコンパイルエラー（M2-opaque）。`const` の剥がしは `-Wcast-qual`（K13）。`uintptr_t` 経由の洗浄（M2-const-launder）は言語では塞げず、CNF の字句検査で `(uintptr_t)` からポインタへの再変換を拒否する予定

### C-004 — `null` の意味は一つ

`null` 相当が意味してよいのは「省略可能な値が無い」だけ。無効・未読込・失敗・未知・削除済みを表さない。公開 API は `null` を返さない。

- 機械強制: **planned** → 未初期化の読み出しは `-Wuninitialized`（K7）。`nullptr` の逆参照はコンパイルが通り（K5）、clang-tidy の `clang-analyzer-core.NullDereference` が拒否する（T2）。`_Nonnull` / `_Nullable` の注釈は `-Wnullability-completeness` で強制し、注釈の無い公開ポインタ引数を拒否する（注釈は clang 拡張なので `-Wpedantic` との併用には名指しの `-Wno-nullability-extension` が要る・ADR 0004）。`-fsanitize=nullability` が `_Nonnull` への `nullptr` を単体テストで検出する。`{0}` によるゼロ値の構築（K15）は C-007 で扱う

### C-005 — 期待される失敗は例外で表さない

C に例外は無い。検証エラー・見つからない・拒否・非互換は**閉じた `enum` の結果型**で返し、`NULL` / `-1` / `bool` / `errno` で複数の業務的な結果を表さない。
結果を返す関数は `[[nodiscard]]` を付け、呼び出し側は必ず分岐する。`abort()` / `exit()` は合成ルート以外で呼ばない。

- 機械強制: **planned** → `[[nodiscard]]`＋`-Wunused-result` で戻り値の無視を拒否（K1-c23-nodiscard）。結果型の選択はレビュー事項。`abort` / `exit` の呼び出しは `eng/symbols.py` の許可リスト外なので中核では落ちる

### C-006 — 汎用データバッグを禁じる

`void *` / 文字列キーの連想配列 / 意味を持つ値の裸の配列で型を代用しない。名前付きの `struct` を作る。`void *` を許すのは Win32 のコールバック引数を境界で受ける 1 か所だけである。ポートの文脈は不完全型（`struct persistence_adapter;`）で受ける（ADR 0004）。

- 機械強制: **planned**（`void *` から具体型への暗黙変換は C が許す（C11-void-star-assign）。CNF の字句検査で core / application の `void *` を拒否する予定）

### C-007 — 構築が不変条件を守る

不変条件を持つ型はコンストラクタを非公開にし、生成経路を**唯一のファクトリ**に集約する。ファクトリは想定内の不正入力に例外を投げず、結果型を返す。

- 機械強制: **planned** → 不完全型は直接構築もコピーもできない（M4-private / M4-copy）。`malloc`＋キャストによる偽造（K14）と `{0}` のゼロ値（K15）は言語では塞げず、**中核の型は必ず不完全型にする**ことで `{0}` を書けなくし、`malloc` の呼び出しをファクトリの実装ファイルに閉じる（CNF で検査する予定）

---

## 2. 構築と可視性

### C-008 — 可視性は最小

既定は非公開。他から実際に使うものだけ公開する。テストのためだけに公開しない。

- 機械強制: **planned**

### C-009 — 言語マジックを制限する

関数風マクロ・`setjmp` / `longjmp`・可変長引数関数の新設・`goto`・インラインアセンブリ・`#pragma` を禁じる（使うなら ADR）。
オブジェクト風マクロは `#define` より `constexpr` / `enum` を使う。

- 機械強制: **planned**（`#pragma` は CNF-003。それ以外は CNF の字句検査で拒否する予定）

### C-010 — 名前が役割を語る

常に禁止する型名の語尾: `Manager` / `Helper` / `Util` / `Utils` / `Common`。
常に禁止するパッケージ／モジュール名: `utils` / `helpers` / `managers` / `misc` / `common`。

承認された役割の語尾は `port` / `adapter` / `query` / `command` / `handler` / `policy` / `factory` / `codec` / `mapper` / `renderer` / `reducer` / `outcome`（C では snake_case）。
**その型がその役割そのものであるときだけ**使う。

`Processor` や `Data` のように**文脈次第で妥当な語は機械では拒否しない**。機械が拒否してよいのは「常に禁止」だけで、判断が要る語はレビューの仕事である。

- 機械強制: **planned** → CNF-001

### C-011 — 1 ファイル 1 主要宣言

ファイルは 1 つの主要な型とその周辺に閉じる。ヘッダの主要型名とファイル名（`note_id.h` ↔ `struct note_id`）を一致させ、
実装ファイルの外部関数は `note_id_` の接頭辞を持つ。寄せ集めのファイルを作らない。

- 機械強制: **planned** → CNF-002

### C-012 — 複雑度に上限を置く

<!-- 数値は言語の道具に合わせてよい（施主決定 2026-09-06）。lint 名を必ず併記する。
     複雑度 10・引数 4 は全言語共通。 -->

| 指標 | 上限 | 道具 |
| --- | --- | --- |
| 認知的複雑度（関数） | 10 | clang-tidy `readability-function-cognitive-complexity` |
| 関数の長さ | 60 行 | clang-tidy `readability-function-size.LineThreshold` |
| ネストの深さ | 3 | clang-tidy `readability-function-size.NestingThreshold` |
| 引数の数 | 4 | clang-tidy `readability-function-size.ParameterThreshold` |
| bool の制御引数（公開 API） | 禁止 | planned（意味と公開境界の検査は未実装） |

閾値を満たすためだけに意味のある処理を割るのは目的に反する。超える必要があるときは**測定可能な理由**を添えて ADR にする。

- 機械強制: **planned**

---

## 3. 実行時の規律

### C-013 — 並行性の形は一つ

UI と application の状態遷移は単一の UI スレッドで行う。背景スレッドは追加しない（全文検索は起動時にメモリへ載せ、同期で引く）。
背景スレッドが要る計測が出たら新しい ADR。

- 機械強制: **planned** → `CreateThread` / `_beginthreadex` / `thrd_create` を `eng/symbols.py` の許可リスト外とし、中核では落ちる。UI 層はレビュー事項

### C-014 — 日時・数値・文字集合の扱いを一つに固定する

内部の文字列は **UTF-8 の `char`** で持ち、Win32 の `W` 系 API を呼ぶ境界（ui / adapters）でだけ UTF-16 へ変換する。`A` 系 API と `setlocale` を呼ばない。
日時は使わない（ファイルの更新時刻を表示するなら adapters が `FILETIME` を受け、core は不透明な値として扱う）。

- 機械強制: **planned** → `setlocale` は ARC-007 のシンボル検査。`A` 系 API の禁止は CNF の字句検査で拒否する予定

### C-015 — 抑制は例外であって道具ではない

<!-- 言語で封じられる（Rust forbid）なら「台帳を持たない」と書き、封じられないなら waiver 台帳（CNF）で補う。
     どちらにせよ理由と規則 ID を必須にする。 -->

抑制には**直前行の `// Waiver: WVR-NNNN`** と有効な waiver 台帳の項目が両方そろっているときにだけ書ける。
ファイル単位・ディレクトリ単位の抑制、静的解析の除外設定、lint の baseline は禁止。

- 機械強制: **planned** → CNF-003 / CNF-004
- 機械強制: **不能**（抑制機能そのものの禁止。`#pragma clang diagnostic ignored` は `-Werror` で指定した診断も抑制できる（K4）。ADR 0003）

---

## 4. 素の Win32 規約

### C-016 — メモリ安全は検出層で守り、書き方で狭める

C はメモリ安全を言語で保証しない。本リポジトリでは次を固定する。

- 可変長配列（VLA）を書かない。上限のある固定長か、ファクトリで確保した所有者付きの領域を使う
- 確保と解放は同じモジュールの対になる関数で行う（`x_create` / `x_destroy`）。`free` を呼ぶのは `destroy` だけ
- 単体テストは常に AddressSanitizer と UndefinedBehaviorSanitizer 付きでビルドし、`-fno-sanitize-recover` で必ず落とす
- JSON と Markdown の読み取りは境界で長さを受け取り、`strcpy` / `sprintf` 系の長さを持たない関数を呼ばない

- 機械強制: **planned** → VLA は `-Werror=vla`（K2-vla-werror。`-Wvla` だけでは C23 で通る＝K2-vla-plain-hole）。`strcpy` 等は clang-tidy `clang-analyzer-security.insecureAPI.*`。ASan / UBSan / nullability は Debug 構成の全 target と単体テストに結線済みで、確保失敗の経路は測定ビルドの注入で全部走る（2026-09-09・ADR 0004）。「確保と解放の対」はレビュー事項なので全体としては planned

### C-017 — ウィンドウ手続きは意図を渡し、描画は表示値を写す

ウィンドウ手続き（`WndProc`）は操作を application の意図として渡し、描画は application が作った表示値を反映する（ARC-011）。
メッセージ番号は開いた OS の集合なので `DefWindowProcW` へ渡す既定分岐を許す。閉じた業務 enum の既定分岐とは区別する（C-002）。
HWND・HDC・HBITMAP・HFONT の所有者は 1 つで、作った関数と対になる関数で解放する（C-016）。
描画はメモリ DC で完成させてから転送し、更新途中の画面を見せない。ドロワーは自前描画の 1 ウィンドウクラスで、共通コントロールの ListView / TreeView を使わない（ADR 0002）。

- 機械強制: **planned**（`src/ui/win32` から adapters への依存は ARC-002 が拒否。意図と表示値の分離はレビュー事項）

---

## 5. 依存の方針

新しい依存を足すときは、次の 3 つを揃える。**1 つでも欠けたら足していない。**

1. ADR を 1 本立てる
2. 許可リスト（eng/architecture.json の runtimeDependencies）に 1 行足す
3. 下の表に 1 行足す

| 依存 | 用途 | 根拠 |
| --- | --- | --- |
| — | — | 現在の実行時依存は 0。C ランタイムは `/MT` で静的に結び、実行ファイルは kernel32 以外の DLL を import しない（R1） |

版は lock ファイルで固定し、マニフェストに範囲や `*` を書かない。ゲートは lock が更新される状態を拒む。

---

## 6. 採用しなかった検査と、その理由

**「有効にしなかった」ことも決定である。** 再提案するときは、ここに書かれた理由への反論から始めること。

| 検査 | 不採用の理由 |
| --- | --- |
| 全 lint の一括有効化（`enable-all` / `restriction` 群） | 相互に矛盾する規則が同時に入り、規則ではなく道具の機嫌に従うことになる（前例: nene-recall・xi-tools） |
| MSVC `cl` をコンパイラにする | `/std:clatest` でも C23 の `nullptr` / `typeof` / `bool` が通らない（C10-c23-nullptr-msvc-hole・C10-c23-bool-typeof-msvc-hole）。`-Wcast-qual` / `-Wswitch-enum` / nullability / UBSan に相当する検査が無い。リンカと SDK にだけ使う（ADR 0003） |
| MSVC `/analyze` を静的解析層に足す | clang-analyzer が同じ null 逆参照（C6011 相当）を拒否する（T2）。2 つのコンパイラを走らせると「どちらが正か」が揺れる |
| cppcheck | 固定した Visual Studio の同梱物ではなく、版の固定先が増える。clang-tidy で足りない検査が実測で出たら再提案 |
| clang-cl に `-Wall` を渡す | `/Wall`＝`-Weverything` と解釈され、C++ 互換警告まで入って C23 の `constexpr` が落ちる（K16）。`/W4` を使う |
| `-Wvla` だけで VLA を禁じる | C23 モードでは警告にならず通った（K2-vla-plain-hole）。`-Werror=vla` を使う |
| `-Wswitch-default`（`default` の強制） | 本規約は網羅済みの `default` を禁じる（C-002）。逆向きの規則なので `-Wno-switch-default` で外し、`-Wcovered-switch-default` を使う |
| `-Wpre-c23-compat` | C23 の構文（`nullptr` 等）を「C17 と互換でない」として拒否する互換性警告で、安全性の警告ではない。`-Wno-pre-c23-compat` で外す（K11） |
| 正規表現だけで全 API 呼び出しを証明する | マクロ・別名・間接呼び出しを解決しない。`llvm-nm` のシンボル検査を正とし、字句検査は補助にする |

---

## 7. 規約に違反したくなったとき

1. **まず、規約が間違っている可能性を検討する。** 規約は実装より新しくない
2. 規約が正しいなら、設計を変える。抑制で通さない
3. どうしても抑制が要るなら、規則 ID と理由を伴う最小スコープの抑制を書く（C-015）
4. **規則そのものを緩めるなら ADR を書く**（QLT-010）。旧規則のどの部分が今も有効かを明記すること
