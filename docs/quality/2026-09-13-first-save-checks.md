# 無題と初回保存の確認（#53 / ADR0020）

## 実装と中核の検証

2026-09-13、無題→名前付きの状態、名前/引数の検証、初回のLF正規化、カテゴリ0件、同名拒否、
本文取得失敗、確保失敗、md公開後の台帳失敗と再試行、既存カーソル保持を単体で検証した。
固定toolchainの `cmake --build build --target nenefolio folio_tests`（clang-tidy含む）と
`ctest --test-dir build --output-on-failure --no-tests=error` が成功（2/2）。
`python eng/conformance.py` は0違反。途中の `python eng/coverage.py` は1622/1724分岐（94.0835%）、
negative proof 13.34%で閾値を下回り期待どおり拒否。最新の正式測定は最終SHAのフルゲート結果を優先する。

読み取り専用の独立レビューで次の3点を検出し、修正後の再読で解消を確認した。

- 初回保存がカテゴリカーソルを奪い、展開操作で同じ本文を再投入する問題。カーソルを保持し、保存後の選択から再描画要否を比較する。キー移動先はモーダル保存前に写す。
- 最大長のmd名に.tmpを追加すると葉の長さ上限を超える問題。通常保存と初回作成を同じ固定長の一時名へ揃え、CREATE_NEWで候補を取得する。
- 台帳が未同期でも本文が同じなら:qが閉じる問題。変更確認でLEDGER_STALEを返し、保存による再試行を要求する。

新しいカーソル回帰テストの初回失敗は、テストが末尾カテゴリを折り畳む前提を準備していなかったため。
明示的に折り畳んでから無題を作るfixtureへ修正した。製品の失敗を期待値変更で隠したものではない。

## 独立したWindows部品測定

以下は自分のプロセスの非表示/画面外の窓とタスク専用ファイルだけを使う。既存アプリやhideの入力は操作しない。
OSなしの単体、物理キー入力、Computer Useでの操作、実機の目視確認とは区別する。
原本・ログ・fixtureは作業ルートの `out/design/2026-09-13/`（ignored）にある。

| 原本 / ログ | 測定内容 | 結果 |
| --- | --- | --- |
| first-save-file-probe.c / first-save-file-probe-final.log | 新規UTF-8、完全同名/大小文字衝突、他の一時ファイル保持、空本文、不在カテゴリ、再保存、248/249/252文字のstem作成・置換・読込 | exit 0、失敗0 |
| first-save-adapter-probe.c / first-save-adapter-probe.log | 実persistence_adapterで252文字stemを作成・衝突拒否・履歴・再保存・読込・走査 | exit 0、失敗0 |
| first-save-form-probe.c / first-save-form-probe.log | 取消、予約名/同名の面内表示、入力保持、日本語名とカテゴリ、合成composition中の確定/取消抑止、台帳部分失敗 | exit 0、4シナリオ、失敗0 |
| first-save-window-probe.c / first-save-window-probe.log | 主窓のCtrl+N/S経路、取消と初回保存の本文/Undo保持、:w 名前、台帳失敗後の:q拒否、:wで修復、名前付き:w 名前の拒否 | exit 0、失敗0 |

`eng/toolchain.ps1`を読み込み、固定clang-clのC23・/MT・/utf-8で原本と実製品ソース/ライブラリをリンクした。
file測定はfile_bytes.cとkernel32。adapter測定は実adapters/application/coreとkernel32。
form/main-window測定は実name_promptまたはnenefolio_window、application/core、単体のfakeポートを含むstate_tests.objと
user32/gdi32/kernel32（主窓はcomdlg32/dwmapiも）をリンクした。計装済みライブラリを使うexeには-fsanitize=addressを付けた。
そのexeには `interception_win: unhandled instruction` 警告が出た。終了コード0と各測定の成功は確認したが、
この部品測定を警告のないASan検証とは報告しない。正規ゲートの単体検証結果は別に記録する。

長い名前の最初の実ファイル測定は9件失敗し、葉だけでなく通常パスのMAX_PATH制限が原因と分かった。
アダプタのroot生成をWin32拡張絶対パスへ揃え、以後の実ファイル・実アダプタの履歴まで成功した。
このroot変更は上記独立レビューの後で、ビルドと実測により確認した。UNC起動と既存拡張moduleパスの実測はしていない。

主窓測定の最初の警告ダイアログは、非表示/非アクティブのため通常のボタン通知では閉じず測定が待機した。
実行パスを照合したタスク専用probeだけ停止し、probe側でEndDialogを使って続けた。警告表示の発生は測定したが、
その閉じ方を実際のユーザークリックの検証とはしない。名前フォームの理由表示はGDIで文字高が欄内に収まることだけ測った。

## 最終ゲートと残る確認

最終SHAの `pwsh -NoProfile -File ./eng/check.ps1`、実行前後のclean状態、Ready後のCIは#53のPR本文へ記録する。
本文の保存は一つの経路で、台帳の版1・既存の履歴深さ5・依存・プラットフォームライブラリ・ゲートを変更していない。

未確認: 実際の日本語IMEの候補確定、物理Ctrl+N/S、マウス/Tab操作の使い勝手、最小560×360の見え方、
100/150/200%とモニタ間の移動、長い名前/理由の目視、HHKB。保存名の面は起動時DPIで配置するOSモーダル。
Computer Useは直近15:45 JSTにnative pipe unavailable / os error 2で、部品測定は接続復旧の証拠ではない。
一時ファイルは既存候補を奪わず256個まで次を試す。異常終了後の残存候補の自動削除はしない。
既存アプリのhideによる「大丈夫そう」を今回のexeの個別確認へ流用しない。

Waivers: none
