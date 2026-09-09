# CLAUDE.md — NeNe Folio

Claude Code / AI エージェントがこのリポジトリで作業するための**中核ハンドブック**。
簡潔な英語版の入口は [AGENTS.md](AGENTS.md)。詳細の正本は `docs/` にあり、ここには複製しない。

---

## 0. まず読むもの（production コードに触れる前に必ず）

1. [docs/ARCHITECTURE_CONSTITUTION.md](docs/ARCHITECTURE_CONSTITUTION.md) — 憲章（ARC-NNN）
2. [docs/PROJECT_LAYOUT.md](docs/PROJECT_LAYOUT.md) — モジュールと依存方向
3. [docs/CODING_RULES.md](docs/CODING_RULES.md) — C (C23, clang-cl) 規約（C-NNN）
4. [docs/QUALITY_GATES.md](docs/QUALITY_GATES.md) — **いま何が機械で守られているか**（QLT-NNN / CNF-NNN）
5. [docs/DEVELOPMENT_WORKFLOW.md](docs/DEVELOPMENT_WORKFLOW.md) — 手順
6. [docs/COMMIT_CONVENTIONS.md](docs/COMMIT_CONVENTIONS.md) — Issue・ブランチ・コミット・PR（GIT-NNN）
7. [docs/GLOSSARY.md](docs/GLOSSARY.md) — 用語
8. 該当する ADR（`docs/adr/`）と有効な waiver（`docs/waivers/`）

---

## 1. このリポジトリの統治原則

> **一つのことを実現する方法を 1 つに固定し、そのことを人の記憶ではなく機械に守らせる。**

その帰結として、次の 3 つを常に守る。

1. **正典の経路を先に特定してから編集する。** 「ここで書いたほうが早いから」で第 2 の経路を作らない（ARC-001 / ARC-012）
2. **ゲートを弱めて通さない。** 検査が落ちたらコードを直す。閾値・除外・重大度を触るのは ADR 相当の判断（QLT-010）
3. **`planned` を `active` と書かない。** 未実装の強制を実装済みに見せるのは、この規約体系で唯一「壊す」行為（[ADR 0001](docs/adr/0001-strictness-is-mechanically-enforced.md)）

---

## 2. このプロジェクトで間違えやすい所

### 現在時刻を読む場所は 1 つしかない

現在時刻・乱数・既定ロケール・環境変数・ファイルを読んでよいのは **`src/adapters/win32`** だけである（ARC-007 / ARC-003）。
中核で必要なら、型のあるポートから注入する。**テストが実時刻を読むことも決定性の破壊である。**
検査はソースの名前ではなく **リンカのシンボル**で行う（`eng/symbols.py`）。`time()` は `_time64` として現れる。

### 網羅性検査を殺す分岐を書かない

閉じた選択肢の分岐に `default` / `else` / `_` を書かない。選択肢が増えたらコンパイルが落ちるのが正しい状態（C-002）。

### 期待される失敗は `NULL` や `-1` で表さない

検証エラー・見つからない・拒否・非互換は閉じた `enum` の結果型で返し、`[[nodiscard]]` を付ける（ARC-010 / C-005）。

### 中核の型はヘッダに不完全型だけを置く

`struct note_id;` と関数だけを公開し、メンバーは実装ファイルに閉じる（C-003 / C-007）。これが C で `{0}` と直接コピーを書けなくする唯一の手段。

### コンパイラは clang-cl だけ

`cl` は C23 を話さない（ADR 0003）。`eng/toolchain.ps1` が `CC=clang-cl` を固定する。VLA・`const` 剥がし・分岐漏れ・網羅済みの `default` はコンパイルが落ちる。

### 抑制は言語で封じられない

`#pragma clang diagnostic` は `-Werror` を黙らせる。だから CNF-003 が pragma を拒否し、行単位の抑制には waiver が要る（C-015）。

### 結果の `enum` は型ごとに別ファイル

CNF-002 は「1 ファイル 1 型定義」を字句で数える。`struct x;` の不完全型と関数は `x.h` に、`enum x_outcome` は `x_outcome.h` に置く。
実装ファイルの外部関数は `x_` 接頭辞。`nenefolio_` 接頭辞は要らない（`eng/symbols.py` はアーカイブ内と宣言済み依存で解決する・ADR 0004）。

### ポートの文脈は不完全型で受ける

`void *context` は C-006 が禁じる。application が `struct persistence_adapter;` を宣言し、adapters（とテストの偽物）が定義する。

### 確保失敗の経路は測定ビルドでだけ走る

`malloc` が失敗する分岐は正典のビルドでは到達しない。`eng/coverage.py` の測定ビルドが `-Dmalloc=folio_probe_malloc` で
中核の確保を `tests/unit/allocation_probe.c` へ向け、`allocation_tests.c` が全確保を 1 回ずつ失敗させる。
製品コードにテスト用の窓口を置かない（C-008）。閾値（90%）と除外は触らない（QLT-009 / QLT-010）。

### OS ライブラリは `nenefolio_system_link` でだけ足す

CMake の既定リンクライブラリは kernel32 だけに絞ってある。user32 / gdi32 を呼ぶ target は `eng/architecture.json` の
`platformLibraries` にあるものを明示的に結ぶ。無ければ configure が落ちる（ARC-002）。

---

## 3. 検証コマンド

```bash
pwsh -NoProfile -File ./eng/check.ps1          # 唯一の完了定義（ローカルと CI で同じ）
```

開発中は最も狭い検査を使ってよい。フルゲートは **PR を Draft → Ready にする直前**に必ず通す。

🔴 **`pwsh -NoProfile -File ./eng/check.ps1` が通っていないものを「できた」と報告しない。**
実行していないコマンドの結果を書かない。テストの失敗を隠さない。
テストが本当の欠陥を見つけたら、期待値ではなく production コードを直す。

---

## 4. 変更の進め方

[docs/DEVELOPMENT_WORKFLOW.md](docs/DEVELOPMENT_WORKFLOW.md) が正本。要約すると:

Issue → 正典経路の特定 → ブランチ → （設計を変えるなら先に ADR）→ 最小の実装 →
テスト → 狭い検査 → `pwsh -NoProfile -File ./eng/check.ps1` → 規則 ID ごとの自己レビュー → PR（draft）→ Ready → squash merge。

コミットは Conventional Commits（`type` と `scope` は英語、説明は日本語、末尾に `(#N)`）。形の正本は GIT-003。

---

## 5. 完了報告の形

作業を終えたら必ず次を報告する。

```text
Issue / 規則 ID:
変更したファイルと振る舞い:
実行した検証コマンドと結果:
ドキュメント・スキーマの変更:
Waivers: none | WVR-NNNN
残るリスク:
```

調査だけを頼まれたときは、編集・コミット・push・PR 作成・外部状態の変更を行わない。

---

## 6. いまの状況

現在のタスクは [docs/todo/current.md](docs/todo/current.md)。GitHub Issue が正で、そこは要約。

2026-09-10: Phase 3 の縦切り 6 本（Issue #3 / ADR 0004、#5、#7、#9 / ADR 0005、#11 / ADR 0006、#13 / ADR 0007）。起動して `data/` の索引を枠なし窓のドロワーに描き、カテゴリ行のクリックでトグルして `expanded` を書き戻し、ノート行のクリックで右ペインに md を表示し、頭の「編集」の札で同じ RichEdit を平文の編集にして Ctrl+S と編集を抜ける操作で同じ md へ原子的に書き戻す。改行の形は元のまま・BOM は書かない・同じなら書かない。行をドラッグすると挿入線が出て、離すと表示順が変わり `categories.json` / `index.json` へ書き戻る（ドロップ先は core の `drawer_layout_drop` が決める・ADR 0007）。見た目はデザイン「案2 堅」（ADR 0005）で、OS のライト／ダークに従う。5 層すべてに正典の経路があり、ARC-002 / ARC-003 / ARC-007 / QLT-009 が active。色変更・検索・スクロール・破棄（保存せずに抜ける）はまだ無い。仕様は [SPECIFICATION.md](SPECIFICATION.md)。
