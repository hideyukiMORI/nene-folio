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

tests/win32（専用desktopと専用portable dataの検証合成ルート）
    -> core, application, adapters/win32, ui/win32
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
ドロップ先・スクロール上限）、カテゴリと索引の json 読み書き、md → RTF のサブセット変換を**純関数**として持つ。
全文検索は`index_filter_build`だけに非公開の継続状態を認め、有限workの同一matcherから不変の`index_filter`を作る（ADR0073）。
step自体を純関数とは呼ばず、同じ構築状態・入力・work列から同じ結果になる決定性を守る。
テーマの固定ID/名前/UI・RTF配色は `folio_theme` の一表、systemを含む名前の解析・解決は `folio_theme_choice` が持つ（ADR0049）。
`note_file_times`はadapterが得たローカルの作成・更新日時を検証した不変の表示値で、時計やファイルを読まない（ADR0087）。
Markdown生成物は`markdown_rtf`、leafと段落spanは`markdown_blocks`、順序付きquote/list pathとitem連番・未表示の印は
`markdown_containers`が所有する。通常受理は旧段落のflushを要求でき、fence/tableの所属先読みは同ownerへの純粋な厳密照合を使う。
inline、tableの解釈も同じcore内の処理へ分け、RTF文字escapeは`rtf_text`へ集約する。
fenceの言語・aliases・字句規則は`code_language`、ブロック内の色分けと継続状態は`code_syntax`が所有し、
同じ`markdown_blocks`の行入力とRTF出力へ合流する。配色は既存の`folio_theme`表だけに置く（ADR0086）。
`web_uri`がWeb起動対象の条件を所有し、`markdown_web_link`は既存字句処理でdestinationを展開して同じfactoryへ渡す（ADR0079）。
`note_archive`は検証済み本文のsnapshotから、UTF-8名・固定日時・CRC32を持つZIP32 STOREを純粋に生成する（ADR0082）。
同じparserの`markdown_images`が画像参照と原文destination範囲を収集する。`markdown_destination`の展開をリンクと共有し、
`markdown_image_copy`は出力コピーの該当範囲だけを変更する。`archive_assets`の画像bytesも同じZIP writerへ渡す（ADR0085）。
表の借用表示入力`markdown_table_format`はkindとcontainer owner由来のtwip字下げを持つ。
`markdown_table_native`の純粋な対応判定（1〜63列・字下げ32767twip以下）をblocksと直接row入力で共有し、
範囲外は同じ全行literal経路へ渡す。nativeの`trleft`と絶対`cellx`もtableが生成する（ADR0072）。
原文は長さ付きで借用し、tab途中の残余列も短命queryで表す。表示用の第2の本文や永続ASTは持たない（ADR0070/0071）。
UI 状態・ファイル・現在時刻を持たない。実行時依存の許可表は空で、外部シンボルは `eng/symbol-allowlist.json` の libc の一部だけ（ARC-003）。

### `src/application`

振る舞いの調整を持つ。ポートの宣言・状態の所有者（`folio_state`）・意図を受ける reducer・表示値の生成・結果型。
Win32・永続化・ファイル・ネットワークを知らない。
Webリンク起動は`folio_state`の意図から`web_link_port`へ渡し、合成rootが必須portを明示する（ADR0079）。
取り込み/書き出しも`folio_state`が所有し、`transfer_port`で外部ファイルを扱う。取り込みは既存create_note/台帳同期、
単票書き出しは既存copied_text、ZIPは保存済み本文の読込とcore archiveへ渡す（ADR0082）。
画像の要求/取消/結果は必須`image_port`へ渡し、`image_export`が同じ解析情報とportでZIP素材を揃える。
カテゴリ移動/改名の画像基準の改訂も`folio_state`が所有する（ADR0085）。
現在ノートの日時snapshotも`folio_state`が所有し、既存`persistence_port.read_note_times`から取得する。
未選択・未保存・取得不能・取得済みを`note_times_state`で区別し、描画への借用値は`note_times_view`で渡す（ADR0087）。
ノート保存先は`storage_state`が現在/次回/初回・復旧phaseを所有し、`storage_port`へ検証と設定保存を渡す。
coreの`storage_path`と`startup_settings`がpath検証と版1のcodecを持ち、ノート側の設定版5とは分離する（ADR0077）。
全文検索の要求・読込位置・corpus・pending build handleと数値cursorは`folio_state`が所有する。
本文準備は一件、入力/初回末尾のmatchingは有限workで継続し、完成時だけ採用する。
確定filter修復の同期driverも同じcore engine/entry解決helperを使う（ADR0068/0073）。

表示だけの設定変更（行番号/ミニマップ）は同じ保存成功後の採用へ渡す。設定版5の`minimap`を
既存codecが読み書きし、旧1〜4はtrueを補う（ADR0084）。

### `src/adapters/win32`

時刻ポートと永続化ポートの実装を持つ。🔑 **決定性の禁止を適用しない唯一のモジュール**であり（ARC-007）、その差分は
`eng/symbols.py` と `eng/conformance.py` の区画判定に明示されている。台帳・md・改名記録のファイル形式の版と移行（ARC-009）も DTO もここに閉じる（設定 `data/settings.json` の解析と版は ADR 0025 の決定 3 に従い core の `folio_settings` が持つ）。
`data/` の走査・`*.md` と `*.json` の読み書き・UTF-8 と UTF-16 の変換はここだけが行う。OS のテーマ（レジストリ）を読む `appearance_adapter` もここ。
`storage_adapter`は配布形式/Known Folder/起動設定を扱い、`persistence_adapter`は確定rootを明示的に受ける。
両者の`.nenefolio.lock`取得は`data_lock`、native path変換は`storage_native_path`へ集約する（ADR0077）。
`web_link_adapter`だけが検証済みURIをShellExecuteExWへ渡し、COM初期化/解放とOS失敗を扱う（ADR0079）。
`transfer_adapter`は外部Markdownの読込と検証済み本文/ZIPの新規公開だけを扱い、`file_bytes`の原子的な公開へ合流する（ADR0082）。
`image_adapter`だけが画像workerと有界cacheを所有する。通常ファイルのreadは`image_file`、HTTPSは`image_download`、
PNG/JPEGのWIC復号/縮小は`image_decode`へ閉じる。WinHTTP callbackは通知だけを渡す（ADR0085）。

### `src/ui/win32`

`note_minimap`はRichEdit表示行の最大256sample・画像最大256矩形と構築進捗を持つ。主窓の120ms timerと
入力優先のidleで16件ずつ更新し、完成像だけ描く。`note_pane_geometry`は同じnative範囲から文書pixel座標を導く。
本文・位置はRichEditが所有し、`note_pane_scroll_pixels`から同じ全高/可視高の上限でnative scrollへ渡す。同一geometryとpaletteを
描画/pointer/focus/UIAで共有し、RangeValue providerは一覧幅と共用する（ADR0084）。

application が作った値を描き、操作を意図として渡す（ARC-011）。可変性の隔離区画（ARC-005）。
`note_pane_links`はnative書式由来のhidden/visible位置対応とクリック状態をpane寿命内で所有する。
`note_pane_images`は同じRichEditのhidden markerと画像blobを対応させ、画面近傍の要求順と幅変更時の再配置を扱う。
画像差替えは既存の選択/scroll/Undo/dirty保持境界で行い、表幅は`note_pane_table`のnative row queryを使う（ADR0085）。
TOMのリンク先をapplicationへ渡し、検索/選択とnative表配置の座標を混同しない（ADR0079）。
`storage_prompt`が初回/復旧/設定変更を同じnative面へまとめ、`folder_picker`はOSのフォルダー選択だけを担当する。
`file_picker`は複数Markdown/出力先のOS選択を担当し、同じmodal_ownerと内容入力guardを使う（ADR0082）。
初回も成立するようpromptがCOMと`icon_paint`の描画寿命を対で所有する。通常時は既存modal ownerへ合流する（ADR0077）。
UIAのCOM寿命は`accessibility_host`、自前要素のABIは`accessibility_node`、native入力の名前注釈は
`accessibility_native`に集約する。主窓・索引・補助面は同じ公開基盤から既存操作を呼ぶ（ADR0056/0058）。
両表示の一覧幅も`folio_window`が実行中だけ所有し、境界drag・鍵・UIA RangeValueを同じ配置へ渡す。
一覧の寸法変更は既存WM_PAINTの全client更新へ合流し、幅ごとの描画経路を作らない（ADR0076）。
固定UIの派生計測は`folio_window`と`name_prompt_metrics`が入力条件付きで保持する（ADR0063）。
操作/テーマ候補の対応は主窓所有の`command_candidates`/`theme_candidates`で共有し、描画・hit・鍵・UIAが
同じ派生列を使う。入力後の可視面の描画は既存通知合流点から既存WM_PAINTへ渡す（ADR0065）。
native同期面のowner境界は`modal_owner`に集約する。主窓のshell操作と内容操作の可否、通常終了の安全な合流は
`folio_window`が所有し、名前入力・失敗箱・色選択へ別のmodal状態を作らない（ADR0064）。
無効な本文の地は`note_pane`の既存subclassが実効paletteから描き、文字/IME/無効化通知はnativeに保つ。
EDITの無効時配色も`folio_window`の一つの配色経路へ合流する（ADR0066）。
本文の実行中の表示倍率は`note_pane`が所有し、キー/ホイールとRTF再投入を同じEM_SETZOOM経路へ渡す。
主窓の行番号font/幅は本文倍率から導く派生資源であり、本文の書式や操作文字用fontは変えない（ADR0067）。
通常ホイールの短い減速も`note_pane`が所有し、message-only窓のtimerから同じnative位置へ反映する。
経過はcallback引数から取り、UIが時計を読んだり別threadで動かしたりしない（ADR0026の#473補正）。
閲覧表の列幅も`note_pane`が同じ本文幅/倍率からnativeのrow書式へ反映する。RTFを再投入せず、
native構造を正本として選択・dirty・Undoを保ち、配置失敗は既存の本文準備契約へ戻す（ADR0070）。
native rowの字下げを引いた非負の残幅を商と余りで均等配分し、列幅0も構造を保つ。
UIはMarkdownを再解釈せず、更新後の字下げ・列数・列幅をnative readbackで照合する（ADR0072）。

### `src/app`

依存を結ぶ。通常message loopは、queueが空でUIが準備を継続できるときだけ一件進め、それ以外は待機する（ADR0068）。端末出力と終了コードを扱ってよい唯一の場所（ARC-006）。

---

## 4. ソースの置き場

<!-- テスト・fixture・生成コード・設定の置き場を書く。生成コードは生成場所を宣言し手で編集しない。 -->

| 種類 | 置き場 |
| --- | --- |
| production | `src/core` / `src/application` / `src/adapters/win32` / `src/ui/win32` / `src/app`（2026-09-09 の最初の縦切りで全層を作った。ADR 0004） |
| テスト | `tests/build`（C23 基盤のスモーク）/ `tests/unit`（OS 非依存の中核。ASan / UBSan / nullability 付き。`allocation_probe` は測定ビルドでだけ効く確保失敗の注入口）/ `tests/conformance`（検査器自身の正例・反例。規約検査の対象外だが決定性の禁止は適用する）/ `tests/win32`（実windowの公開UI境界。製品libraryとmanifestを共有し、専用desktop/dataで設定の一意な有限巡回を確認する。ADR0058 / #504） |
| 検査設定 | `.clang-format` / `.clang-tidy` / `eng/*.json`。参照の一覧は `eng/config-bindings.json`（CNF-007） |
| 作業用ツール | `tools/`。対象実行の証跡は `tools/record-run.py` が明示された新規run-dirへ保存し、境界テストは既存 `tests/conformance` に置く。開発時の出力は `D:/NeNeFolio/outputs/`、恒久証跡は `D:/NeNeFolio/agents/` |
| 生成物 | `build/`（CMake・オブジェクト・検証 exe・CMakeから導出したmanifest資源と配布metadata）/ `out/`（Phase 0 の実測・証明 fixture・測定ビルド `out/coverage`・出力）。製品 C/header/RCコードの生成は未採用 |
| 配布入力 | `src/app/NeNeFolio.rc`と`NeNeFolio.manifest.in`は正典app targetに属する。`docs/release/`に配布案内、`eng/release-layout.json`に固定同梱一覧を置く。版はCMake、書体一覧は`fonts/manifest.json`から導出する（ADR0045）。MSIXのidentity/表示情報/画像hashは`eng/msix.json`、図案原本とPNGは`packaging/windows/`（ADR0080） |
| 配布出力 | `eng/package-release.ps1`に明示した新しいWorkDirectoryとOutputDirectory。既定ZIPまたは`-Format Msix`。開発作業では`D:/NeNeFolio/outputs/`配下に置く。利用者dataや開発用生成物を配布物に入れない |
| 利用者データ | 選択したroot。portable既定はexe隣の `data/`、package identityを持つ版の候補はDocuments/NeNeFolio（カテゴリ = ディレクトリ・台帳・md・設定・履歴）。リポジトリには入れない |
| 起動設定 | 版1 `nenefolio-startup.json`。portableはexe隣、package identityを持つ版はLocalAppData/NeNeFolio。ノート側settings版5とは別（ADR0077） |
