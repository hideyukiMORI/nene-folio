# ADR 0004 — 最初の縦切りで中核を生み、リンカ段と分岐の検査を結線する

- 状態: 受理
- 日付: 2026-09-09
- Issue: #3
- 影響する規則: ARC-002 / ARC-003 / ARC-004 / ARC-005 / ARC-006 / ARC-007 / ARC-008 / ARC-009 / ARC-011 / C-002 / C-004 / C-006 / C-007 / C-011 / C-012 / C-014 / C-016 / C-017 / QLT-009 / QLT-013 / CNF-002

## 文脈

Phase 0〜2（Issue #1 / PR #2）で検査基盤は緑だったが、製品コードは 0 行で、`eng/symbols.py` は「0 libraries checked」で
通っていた。つまり憲章の中心である決定性（ARC-007）とプラットフォーム独立（ARC-003）は**何も守っていなかった**（ADR 0003）。
今回は FR-001 / FR-002 / FR-003 / FR-007 / FR-008 / FR-013 / FR-015 の最小部分——起動して `data/` の 1 カテゴリ・1 ノートを
枠なし窓のドロワーに描く——で、承認済みの 5 層すべてに初めて具体的な責務を置き、その瞬間に検査を結線した。

実測の要点は 2 つある。第 1 に、規約は書いたとおりには通らなかった。10 箇所で規約同士・規約と道具が衝突し、そのたびに
**緩めずに**設計か検査器の側を直した（下の表）。第 2 に、C の「確保失敗を必ず分岐で扱う」書き方は、分岐カバレッジの
下限 90%（QLT-009）と正面から衝突する。初回の実測は 86.29% で、未到達の大半が `malloc` の失敗経路だった。

## 決定

**application の `folio_state` が、起動時にポートから受けた台帳と走査結果を core の照合で 1 つの索引に固定し、UI は core の
`drawer_layout` が返す行を描くだけにする。中核の静的ライブラリが生まれたこの変更で、ARC-002 / ARC-003 / ARC-007 / QLT-009 を
active にする。**

- `core`: `json_reader`（文法を検証する字句の読み手。木は作らない）と `json_writer`（整形付きの書き手）、`name_list`
  （名前の規則を 1 度だけ検証する順序付き集合）、`category_ledger` / `note_ledger`（版 1 の台帳の codec と走査結果との照合）、
  `drawer_layout`（行の y 座標を返す純関数。名前を複製して所有する）、`utf8_text` / `utf16_text`（Win32 境界の文字集合変換。
  C では `wchar_t` と `char16_t` が同じ型であることを実測した）。不変条件を持つ型はすべて不完全型で、結果は閉じた `enum`
- 版 1 の台帳は**キーの順序も固定**する（`version` → `categories` / `notes`、項目は `name` → `color` → `expanded`）。
  順序が違えば MALFORMED であって既定値には落とさない（FR-015）。書くのは本製品自身なので順序は保てる
- `application`: `persistence_port` は関数ポインタの束で、文脈は `struct persistence_adapter;` の**不完全型**として受ける
  （`void *` を使わない・C-006）。`folio_state` が文書一覧とカテゴリ設定の唯一の所有者で、`data/` が無ければ空、
  台帳が読めない・形が違うときは起動を止める結果を返す
- `adapters/win32`: `persistence_adapter` が実行ファイルの場所から `data/` を決め、`FindFirstFileExW` で走査し、
  `file_bytes` で json を読んで core の codec で台帳に変える（ARC-008 / ARC-009）。先頭の UTF-8 BOM はこの境界で捨てる
- `ui/win32`: `folio_window` は `WS_POPUP | WS_THICKFRAME` に `WM_NCCALCSIZE` を 0 で返して枠を消し、縁と掴む帯を
  `WM_NCHITTEST` で自前判定する。`drawer_window` は自前描画の子ウィンドウで、`WM_PAINT` ごとに application から配置を
  受け取り、メモリ DC で完成させてから転送する。`PostQuitMessage(0)` は「窓が閉じた」の合図であり、終了コードは合成ルートが決める
- `app`: `wWinMain` がアダプタ・状態・窓を結び、起動できなかった理由 1 行を `MessageBoxW` で出す
- 検出層: Debug 構成の全 C target を ASan / UBSan / nullability で計装し `-fno-sanitize-recover=all` で必ず落とす。
  CRT は常に `/MT`（ASan は Debug CRT と共存しない）。ランタイムは clang-cl の resource dir から lld-link へ明示的に結ぶ
- 分岐の測定は `eng/coverage.py` の**測定ビルド**（同じソース・同じテストを計装コンパイル。ASan 付き）で行い、
  中核の確保だけを `-Dmalloc=folio_probe_malloc -Dcalloc=… -Drealloc=…` でテストの注入口へ向ける。
  `tests/unit/allocation_tests.c` が全確保を 1 回ずつ失敗させ、OUT_OF_MEMORY を返して片付けることを確かめる。
  製品の正典ビルドにテスト用の窓口は無い（C-008）
- `eng/symbols.py` は、アーカイブ内の相互参照と**宣言済み依存モジュール**が定義するシンボルを解決してから許可リストと照合する。
  `nenefolio_.*` の許可は削除した（接頭辞さえ合えば何でも通る穴だった）
- CMake の既定リンクライブラリ（user32 / gdi32 / advapi32 …）を `kernel32.lib` だけに絞り、OS ライブラリは
  `nenefolio_system_link` でだけ足す。`platformLibraries` がこれで物理的な制約になる（ゲートを強める変更）

### 規約が邪魔になった箇所と、緩めずに通した方法

| 衝突 | 実測（2026-09-09） | 通し方 |
| --- | --- | --- |
| `_Nonnull` / `_Nullable`（C-004）を `-Wpedantic` が拒否する | `-Wnullability-extension` で `/WX` が落ちる | 名指しの `-Wno-nullability-extension` を警告集合に足す（CNF-005 の対象外の「規則の選択」）。公開 API の全ポインタに注釈を付け、`-Wnullability-completeness` を効かせる |
| `eng/symbol-allowlist.json` の `nenefolio_.*` と CNF-002 の `<stem>_` 接頭辞 | core の相互参照（`json_reader_next` 等）が ARC-003 で 40 件落ちた | 許可リストを広げず、symbols.py を「アーカイブ内と宣言済み依存で解決してから照合」に直す。`nenefolio_.*` は削除 |
| ASan と Debug CRT | `-MTd` は `-fsanitize=address` と共存できない | CRT を常に `/MT` に固定（R1 とも整合） |
| CMake は link を clang-cl の driver 経由で呼ばない | `__asan_init` 等が lld-link で未解決 | resource dir の `clang_rt.asan / asan_cxx / ubsan_standalone(_cxx)` を `targets.cmake` の 1 か所で明示的に結ぶ |
| CMake の既定リンクライブラリ | user32 / gdi32 / advapi32 が全 target に結ばれ、ARC-002 の `platformLibraries` が形骸 | `CMAKE_C_STANDARD_LIBRARIES` を kernel32 だけにする |
| C-006（`void *` 禁止）とポートの文脈引数 | `void *context` が書けない | application が `struct persistence_adapter;` を宣言し、adapters（とテスト）が定義する |
| CNF-002（1 ファイル 1 型定義） | 型のヘッダに結果の `enum` を同居させると落ちる | `*_outcome.h` を型ごとに分ける（core 22 ファイル中 11 が `enum` だけのヘッダ） |
| QLT-009（分岐 90%）と C の確保失敗の分岐 | 初回 86.29%。未到達 102 本のうち大半が `malloc` 失敗 | 閾値も除外も触らず、測定ビルドでだけ確保失敗を注入して全確保を 1 回ずつ失敗させる → 707/744（95.03%） |
| C-012（認知的複雑度 10） | JSON の数値文法 13・カテゴリ配列 12・`FindFirstFile` の列挙 17・縁の判定 | 分割（`skip_fraction` / `skip_exponent`・`unexpected()`・`collect()` / `first_entry_outcome()`・`band()`） |
| clang-format と日本語コメント | 100 桁超のコメントを不自然な位置で折り返す | 短い行で書く。規約は変えない |

## 強制

- ARC-002: **active**。`eng/targets.cmake` が宣言外の依存と OS ライブラリを configure で拒否し、`eng/conformance.py --build-dir` が
  File API の実グラフと include を照合する。実 target（core / application / adapters_win32 / ui_win32 / app / unit_tests）が生まれた
- ARC-003 / ARC-007: **active**。`eng/check.ps1` が `eng/symbols.py --build-dir build --require core application` を呼ぶ。
  2 ライブラリを検査して 0 件。反例（`_time64` / `GetTickCount` / `CreateFileW` / 無い必須モジュール）は `eng/prove-gates.py` が毎回仕込む
- QLT-009: **active**。`eng/coverage.py` が測定ビルドで core / application の全 `.c` を測り、反例（テストを省いた実行＝21.51%）が
  落ちることを毎回確かめてから、全テスト（95.03%）を判定する
- C-016 の検出層（ASan / UBSan / nullability）は結線した。C-016 全体は「確保と解放の対」がレビュー事項なので planned のまま
- QLT-013: planned。実機の確認は `docs/quality/gate-proofs.md` 第 5 節に環境と手順を残した

## 結果

得る: 5 層すべてに正典の経路ができ、次の縦切り（トグル・並び替え・右ペイン）は既存の型に振る舞いを足すだけになる。
決定性と依存方向が「約束」から「リンカ段の検査」になった。C の確保失敗の経路が全部テストで走る。

失う・残る:

- 🔴 緑は core / application の分岐 95% と、96 DPI・1 モニタで 1 回起動したことしか示さない。DPI 変更・スナップ・縁のリサイズ・
  高 DPI のフォントは実機で全部は見ていない
- 🔴 `eng/symbols.py` は依存モジュールが定義するシンボルを**名前**で解決する。application が core のヘッダに無い外部関数を
  呼んでも検出しない（ヘッダに無い外部関数を作らないことは C-008 のレビュー事項）
- 確保失敗の注入は測定ビルドだけで効く。正典ビルドの CTest では OOM の経路は走らない（測定ビルドは ASan 付きなので、
  片付けの誤りは測定側で検出される）
- `-fsanitize=nullability` は実行時の検出であり、`_Nonnull` に `nullptr` を渡すコードはコンパイルが通る
- キー順序を固定した台帳は、手で並べ替えると MALFORMED になる。黙って既定値に落ちるより見えるほうを取った
- パスの上限は 1024 単位。それより深い場所に置かれた実行ファイルは「読めない」として起動を止める
- 台帳の**保存**はまだ無い。core の書き手は往復テストで検証したが、起動時に書き戻さない（FR-008 の「次の保存で消す」は保存を持つ縦切りで）
- 走査で受け入れられない名前（255 バイト超・末尾の空白）があると起動を止める。無視して続ける道は取らなかった

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| ポートに `void *context` を置く | C-006 の唯一の例外は Win32 のコールバック引数。不完全型で足りる |
| 台帳の JSON を木（DOM）に読む | 汎用データバッグ（C-006）。字句の読み手と型ごとの codec で足りる |
| 台帳のキー順序を自由にする | 後から来る `name` のための一時保持と型が要る（C-011 でファイルが増える）。書くのは本製品自身なので固定でよい |
| QLT-009 の閾値を 85% に下げる ADR | C の OOM 分岐は本当に到達すべき分岐であり、注入で到達できた。下げる理由が無い |
| 製品コードに確保の窓口（`core_memory_fail_after`）を置く | テストのためだけの公開（C-008）と可変なグローバル（ARC-006）。測定ビルドの `-D` で足りる |
| `nenefolio_.*` を許可リストに残す | 接頭辞さえ合えば外部シンボルが通る穴。しかも CNF-002 の接頭辞規則と両立しない |
| 起動時に台帳を書き戻す | 保存を持たない縦切りで書き込み経路を開くと、FR-008 の「次の保存で消す」と二重になる |
| ListView / TreeView でドロワーを作る | ADR 0002 |
| `GET_X_LPARAM` を自前のマクロで書く | 関数風マクロ（C-009）。`windowsx.h` を使う |
| core を 2 度ビルドする（製品用と計装用の STATIC target） | 計装用の core も module core として symbols.py の対象になり、注入口の外部シンボルを許可する穴が要る |

## 参考

- [ADR 0002](0002-plain-win32-no-ui-library.md) / [ADR 0003](0003-c23-clang-cl-foundation-and-measured-limits.md)
- NeNe Loupe ADR 0004（最初の縦切り）・`eng/coverage.py`（測定ビルドの原型）
- [LLVM source-based coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html)
- [AddressSanitizer on Windows](https://clang.llvm.org/docs/AddressSanitizer.html)
