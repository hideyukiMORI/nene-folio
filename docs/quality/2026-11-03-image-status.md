# 画像読込完了後に残る通知 — #526

## 原因と修正

hideの実機報告で、画像を表示した後も「画像を読み込んでいます」が残る症状を確認した。
`folio_window.c`の`image_progress`は、完了後の`image_line`をEMPTYにしてから通知矩形を求めていた。
通知の状態自体は非表示になるが、新矩形の高さは0のため、既に描いた文字の領域を更新しなかった。

変更前の通知矩形を退避し、変更後の矩形との和を既存の`InvalidateRect`→主窓paintへ渡す。
本文の再投入・再配置、全画面の周期再描画、通知の優先順位、画像I/O/復号処理を変更しない。
規則はARC-001/004/005/009/012、C-002/008/012/016、QLT-008/012/013、GIT-001〜004。
ADR0060の本文位置保持とADR0085の画像通知を維持し、ADR0085の検証境界に成功完了を明記した。
保存schema・版・依存の変更なし。追加waiverはnone。既存WVR-0001のWinHTTP callback ABIは変更しない。

## 対象検証

`tests/win32/image_status_tests.c`の制御portを成功まで進められるようにし、実主窓・実RichEditを隔離desktopで確認した。
通知のUIA非表示だけでなく、OS更新領域が以前の通知欄全体を含むことを検証する。
読み込み/加工からの成功完了、本文位置・本文文字・選択・dirty・Undo保持、更新領域の描画完了を確認する。
既存の本文先行・モード取消・カテゴリ移動時の要求失効も同じ対象で維持した。

正規Debug targetのclang-tidy・ASan/UBSan/nullabilityを有効にし、次を実行した。

| コマンド・原本 | 結果 |
| --- | --- |
| `cmake --build build --target image_status_tests` / `ctest --test-dir build -R '^image_status$' --output-on-failure --no-tests=error`（`repro-01`） | 製品修正前は`completed image repaints the entire old notice area`で失敗。通知の内部状態が非表示になる確認までは成功し、残像の原因を再現した |
| 同じコマンド（`target-02`） | 製品修正後は65checks成功。失敗した期待を変えず、同じテストを再実行した |

原本は`D:/NeNeFolio/outputs/526-image-status/`、恒久記録は`D:/NeNeFolio/agents/image-status-20261103/`。
`tools/record-run.py`で入力・成果物のsnapshot/hashとraw出力を保持した。成功後に対象試験を繰り返さない。
最終headの必須`eng/check.ps1` CI、main統合、通常exe反映、データ保持、追加作業物の保全整理は
同agentsの`integration-completion.json`で確定する。CI成功前に完了と扱わない。

## 実機確認用サンプル

hideの追加依頼で、通常dataの`サンプル/シンタックスカラーサンプル.md`を新規作成した。
C/C++・JS/TS・Python・JSON・Shell・PowerShell・SQL、別名/大文字、複数行引用/コメント、単色の例を収載した。
UTF-8原本を上記agentsへ保持し、新規作成専用の書込みで既存ノートを上書きせず、両ファイルのhashを照合した。
indexや既存ノートは直接変更しない。通常反映前のデータsnapshotは、この追加後の32filesを基準とする。

## 限界

実測は観測Windows/RichEditと制御portの成功完了に対するもの。全OS・実reader・全ネットワーク条件の保証ではない。
通常版でhideの実機確認を行う。画像の取得が実際に続いている間は、従来どおり読み込み/加工通知を表示する。
