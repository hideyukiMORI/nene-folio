# 常設ステータスバーと保存ファイル日時 — #530

## 変更と契約

hideの11月3日午後の「ステータスバー改善まで進める」指定で、既存の下端通知を常設32pxのバーにした。
本文と異なる背景、更新・作成日時、既存のヘルプ／コマンドキーを同じ描画・UIA・tooltipへ渡す。
狭幅では作成日時と長い案内を順に省略し、通知を優先する。通知が消えると通常情報へ戻る。
本文・一覧・splitterはバーに重ならない。検索／置換の低い窓でも下余白32pxを縮めない。

日時はファイル属性のcreation/last-writeを取得時のローカル時刻へ変換した値で、編集開始時刻ではない。
`file_times`→既存`persistence_port`→`folio_state`所有の`note_file_times`→表示値の単一路を使う。
再描画は属性を読まず、ノート切替・保存・改名・移動・安全な再activationでsnapshotを更新する。
未選択、無題、取得不能、取得済みを型で分け、取得失敗や未完了改名で前の日時を表示しない。
同一本文の保存は従来どおり書かない。原子的置換前に作成時刻を一時ファイルへ渡し、失敗なら公開しない。

規則はARC-001/002/003/004/007/008/010/011、C-002/003/007/008/012/014/016/017/018、
QLT-008/009/012/013、GIT-001〜004。設計は[ADR0087](../adr/0087-note-status-bar.md)。
README・配置規約・用語集へ反映した。保存schemaと製品版は不変。追加waiverはnone、既存WVR-0001は継続。
実描画の確認でmemory DC/DIBを使うため、Win32テストだけに宣言済み`gdi32`依存を追加した。

## 検証の記録

対象は正規Debug targetのclang-tidy・ASan/UBSan/nullability付きunitと、隔離desktopの実主窓/RichEdit。
`tools/record-run.py`で入力snapshot/hash、raw stdout/stderr、終了値を各新規run-dirに保存する。
作業原本は`D:/NeNeFolio/outputs/530-status-bar/`、恒久原本は`D:/NeNeFolio/agents/status-bar-20261103/`。
対象結果は以下へ追記し、最終headの必須CI、main、通常反映、data不変、保全整理は
同agentsの`integration-completion.json`で確定する。

| 原本・コマンド | 結果 |
| --- | --- |
| `targeted-06`の`build/folio_tests.exe --status` | 日時のcalendar検証・snapshot遷移・同一本文/実保存/属性失敗・新規/削除/改名、127固定配色の4.5:1、OS高コントラスト対、三言語文言が成功。後続nativeの失敗とは分離して再利用 |
| `native-08`: `cmake --build build --target image_status_tests`、`ctest --test-dir build -R '^(image_status\|note_status_.+)$' --output-on-failure --no-tests=error` | 6件成功。通知の読込/加工→通常表示、本文/位置/選択/dirty/Undo保持、固定属性→原子的保存の作成時刻保持・更新時刻前進、置換拒否時の旧bytes保持、三言語・明暗・UIA/tooltip同一性、最小高の検索/置換で本文非侵入 |
| `narrow-09`: 同target buildと`ctest --test-dir build -R '^note_status_zh$' --output-on-failure --no-tests=error` | 一覧専用の狭幅へ条件を変えた1件が成功。更新日時と短いヘルプを保持し、作成日時と長いキー案内を省略。全文のUIA/tooltip一致も維持 |

実表示のBMP3枚を保存し、目視確認した。英語の最小「両方」幅では作成日時を省き、更新とキーを保持する。
通常の三言語描画と検索／置換のgeometryを別CTestケースにし、各30秒の上限と共用fixtureのresource lockを持たせた。
追加した確保失敗の分岐は既存の測定ビルドでのみ注入され、最終必須CIのcoverage工程で実行する。
正常な条件を工程の変更だけで再実行せず、変更した狭幅条件は個別に確認する。

初期試行も保存した。`targeted-01`はGDIの未宣言依存、`02`は対象名の誤り、`03`は日時字形検証の複雑度、
`04`はtest mainの複雑度と未使用定数で停止した。`04`は失敗後の入力変更も検出しているため成功記録ではない。
`05`のtooltip取得はABIサイズを修正。`06`の一括nativeは時間切れ、`native-07`はテストが存在しない
メニュー項目を要求して失敗した。既存の検索ボタンと操作descriptorを使い、受入条件ごとのケースへ分けて`08`で解消した。
抑制・除外・複雑度閾値・タイムアウトを緩めていない。

読み取り専用レビューでは低い検索／置換窓の本文侵入を検出し、下余白固定と対象検証を追加した。
テストのtooltip通知とEsc配送も実Win32経路へ合わせた。レビュー担当は編集やテストを実行していない。

## 限界

外部copyや別editorの属性変更を「最初に文章を書いた日時」へ復元するものではない。
実測したWindows/RichEditと代表明暗・三言語・狭幅が対象で、全OS/DPI/実screen readerの保証ではない。
高コントラストはOSのbutton対を保持するため、任意のOS設定色の比は保証しない。
タグ、新規コマンドパレット、Store/配布更新、#466/#479は対象外。
