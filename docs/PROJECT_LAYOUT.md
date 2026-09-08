# モジュール構成と依存規則 — NeNe Folio

> Status: normative（規範）/ 2026-09-09 初版
> パッケージルート: `nenefolio`

モジュールグラフはアーキテクチャの一部である。**パッケージの命名規約だけでは依存の境界にならない。**
承認された一覧の機械可読な正本は `eng/architecture.json` で、規約検査がそれと実際のビルドグラフを突き合わせる（ARC-002）。

---

## 1. 承認されたモジュール

```text
src/app（wWinMain）
    合成ルート。ポートに実装を結び、UI を起動する。端末出力と終了コードを持つ唯一の場所

src/ui/win32
    画面。枠なし窓・自前描画のドロワー・RichEdit のビュー／編集。状態を描き、意図を発行する

src/adapters/win32
    data/ の走査・md と json の読み書き・時刻ポートの実装。現在時刻・乱数・環境を読め、ファイルに触れる唯一のモジュール

src/application
    ポートの宣言、状態の所有者（folio_state）、意図を受ける reducer、表示値の生成、結果型

src/core
    値・不変条件・閉じた選択肢。ドロワーのレイアウト計算・json の読み書き・md → RTF 変換・全文検索。標準ライブラリ以外に依存しない
```

---

## 2. 許可された依存グラフ

```text
src/core
    -> 標準ライブラリのみ

src/application
    -> src/core

adapters/win32 / adapters/win32
    -> src/application, src/core

src/ui/win32
    -> src/application, src/core

src/app
    -> すべて（明示的な合成のためだけに）
```

ここに無い依存はすべて禁止である。とくに:

- core -> Win32 / `stdio.h` / `time.h`: **禁止**（`#include` は言語では塞げないので、`eng/symbols.py` のシンボル検査と字句検査で塞ぐ）
- core -> application: 禁止
- application -> アダプタ: 禁止
- ui -> アダプタ: 禁止
- アダプタ -> 別のアダプタ: 禁止
- 何か -> 合成ルート: 禁止

---

## 3. 各モジュールの責務

### `src/core`

意味の正本を持つ。値型・閉じた選択肢・拒否理由と結果型。ドロワーのレイアウト（どの行が何ピクセルに見えるか・ヒットテスト・
ドロップ先・スクロール上限）、カテゴリと索引の json 読み書き、md → RTF のサブセット変換、全文検索の索引を**純関数**として持つ。
UI 状態・ファイル・現在時刻を持たない。実行時依存の許可表は空で、外部シンボルは `eng/symbol-allowlist.json` の libc の一部だけ（ARC-003）。

### `src/application`

振る舞いの調整を持つ。ポートの宣言・状態の所有者（`folio_state`）・意図を受ける reducer・表示値の生成・結果型。
Win32・永続化・ファイル・ネットワークを知らない。

### `src/adapters/win32`

時刻ポートと永続化ポートの実装を持つ。🔑 **決定性の禁止を適用しない唯一のモジュール**であり（ARC-007）、その差分は
`eng/symbols.py` と `eng/conformance.py` の区画判定に明示されている。保存形式の版と移行（ARC-009）も DTO もここに閉じる。
`data/` の走査・`*.md` と `*.json` の読み書き・UTF-8 と UTF-16 の変換はここだけが行う。

### `src/ui/win32`

application が作った値を描き、操作を意図として渡す（ARC-011）。可変性の隔離区画（ARC-005）。

### `src/app`

依存を結ぶ。端末出力と終了コードを扱ってよい唯一の場所（ARC-006）。

---

## 4. ソースの置き場

<!-- テスト・fixture・生成コード・設定の置き場を書く。生成コードは生成場所を宣言し手で編集しない。 -->

| 種類 | 置き場 |
| --- | --- |
| production | `src/core` / `src/application` / `src/adapters/win32` / `src/ui/win32` / `src/app`（2026-09-09 の最初の縦切りで全層を作った。ADR 0004） |
| テスト | `tests/build`（C23 基盤のスモーク）/ `tests/unit`（OS 非依存の中核。ASan / UBSan / nullability 付き。`allocation_probe` は測定ビルドでだけ効く確保失敗の注入口）/ `tests/conformance`（検査器自身の正例・反例。規約検査の対象外だが決定性の禁止は適用する） |
| 検査設定 | `.clang-format` / `.clang-tidy` / `eng/*.json`。参照の一覧は `eng/config-bindings.json`（CNF-007） |
| 生成物 | `build/`（CMake・オブジェクト・検証 exe）/ `out/`（Phase 0 の実測・証明 fixture・測定ビルド `out/coverage`・出力）。製品 C コードの生成は未採用 |
| 利用者データ | 実行ファイルと同じ場所の `data/`（カテゴリ = ディレクトリ・`categories.json`・各カテゴリの `index.json`・`*.md`）。リポジトリには入れない |
