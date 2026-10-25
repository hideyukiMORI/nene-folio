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
テーマの固定ID/名前/UI・RTF配色は `folio_theme` の一表、systemを含む名前の解析・解決は `folio_theme_choice` が持つ（ADR0049）。
UI 状態・ファイル・現在時刻を持たない。実行時依存の許可表は空で、外部シンボルは `eng/symbol-allowlist.json` の libc の一部だけ（ARC-003）。

### `src/application`

振る舞いの調整を持つ。ポートの宣言・状態の所有者（`folio_state`）・意図を受ける reducer・表示値の生成・結果型。
Win32・永続化・ファイル・ネットワークを知らない。

### `src/adapters/win32`

時刻ポートと永続化ポートの実装を持つ。🔑 **決定性の禁止を適用しない唯一のモジュール**であり（ARC-007）、その差分は
`eng/symbols.py` と `eng/conformance.py` の区画判定に明示されている。台帳・md・改名記録のファイル形式の版と移行（ARC-009）も DTO もここに閉じる（設定 `data/settings.json` の解析と版は ADR 0025 の決定 3 に従い core の `folio_settings` が持つ）。
`data/` の走査・`*.md` と `*.json` の読み書き・UTF-8 と UTF-16 の変換はここだけが行う。OS のテーマ（レジストリ）を読む `appearance_adapter` もここ。

### `src/ui/win32`

application が作った値を描き、操作を意図として渡す（ARC-011）。可変性の隔離区画（ARC-005）。
UIAのCOM寿命は`accessibility_host`、自前要素のABIは`accessibility_node`、native入力の名前注釈は
`accessibility_native`に集約する。主窓・索引・補助面は同じ公開基盤から既存操作を呼ぶ（ADR0056/0058）。
固定UIの派生計測は`folio_window`と`name_prompt_metrics`が入力条件付きで保持する（ADR0063）。
操作/テーマ候補の対応は主窓所有の`command_candidates`/`theme_candidates`で共有し、描画・hit・鍵・UIAが
同じ派生列を使う。入力後の可視面の描画は既存通知合流点から既存WM_PAINTへ渡す（ADR0065）。
native同期面のowner境界は`modal_owner`に集約する。主窓のshell操作と内容操作の可否、通常終了の安全な合流は
`folio_window`が所有し、名前入力・失敗箱・色選択へ別のmodal状態を作らない（ADR0064）。

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
| 生成物 | `build/`（CMake・オブジェクト・検証 exe・CMakeから導出したmanifest資源と配布metadata）/ `out/`（Phase 0 の実測・証明 fixture・測定ビルド `out/coverage`・出力）。製品 C/header/RCコードの生成は未採用 |
| 配布入力 | `src/app/NeNeFolio.rc`と`NeNeFolio.manifest.in`は正典app targetに属する。`docs/release/`に配布案内、`eng/release-layout.json`に固定同梱一覧を置く。版はCMake、書体一覧は`fonts/manifest.json`から導出する（ADR0045） |
| 配布出力 | `eng/package-release.ps1`に明示した新しいWorkDirectoryとOutputDirectory。開発作業では`D:/NeNeFolio/outputs/`配下に置く。利用者dataや開発用生成物をZIPに入れない |
| 利用者データ | 実行ファイルと同じ場所の `data/`（カテゴリ = ディレクトリ・`categories.json`・各カテゴリの `index.json`・`*.md`）。リポジトリには入れない |
