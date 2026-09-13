# 別名保存の確認（#55 / ADR0021）

2026-09-13、元のwrite/archiveを呼ばない新規作成、編集入力のCRLF正規化、閲覧原文のMarkdownと混在改行、
同名/書込失敗での元選択の保持、作成後の台帳失敗、閲覧中の:q拒否と保存での復旧を単体で確認した。
名前の解析と既存機能も含め、固定toolchainの `cmake --build build`（clang-tidy含む）、
`ctest --test-dir build --output-on-failure --no-tests=error` が成功（2/2）。
`python eng/coverage.py` は確保失敗を含む1638/1742分岐（94.0299%）、negative proof13.20%で期待どおり拒否。
初回の新しい単体シナリオは、固定本文を前提とする既存edited_state fixtureを異なる本文で使って失敗した。
新シナリオはready_state→選択→編集開始で明示的に準備するよう修正し、製品の振る舞いは変えていない。

## Windowsの独立部品測定

ルート `out/design/2026-09-13/save-as-window-probe.c` と `save-as-window-probe.log` が原本と結果。
#53の自プロセス測定の窓/ダイアログフックを再利用し、タスク自身の主窓とRichEditを画面外で作成して測った。
既存ユーザーアプリ/データを操作せず、物理キーやComputer Useの代替として扱わない。

- Ctrl+Shift+SをOSのTranslateAcceleratorへ渡し、本文/INDEX/コマンド入力からフォームへ進む。
- Ctrl+SとAlt併用は対象外。合成composition中の本文/コマンド入力、名前フォームの入力も対象外。
- 3入口で取消、4回目で保存。元への保存/履歴は0回、新md作成だけ1回。
- 取消も保存成功も本文/Undoを保持し、Undoで保存前の挿入を戻せる。
- :saveas 日本語名も同じ作成で、元への保存を増やさない。その後の変更なしCtrl+Sも書かない。
- GUIパレットの「保存して閲覧」で実際にMarkdown表示へ変え、別名保存が表示文字でなく原文を保持する。
- 台帳失敗中の閲覧:qと:wqは終了を拒否。保存で台帳だけを復旧できる。

固定clang-cl /std=c23 /MT /utf-8 -fsanitize=addressで測定C、state_tests.obj、
nenefolio_window/application/core.lib、user32/gdi32/kernel32/comdlg32/dwmapiを結合して実行。
結果: `save-as component forms=4 failures=0`、exit 0。
測定のSet/GetKeyboardStateは自スレッドだけ、製品に修飾キー状態の読取/保持/合成入力を増やしていない。
警告ダイアログは測定のEndDialogで閉じており、クリック操作の検証ではない。
計装ライブラリを使うexeには `interception_win: unhandled instruction` 警告が出る。
終了コードと各条件は成功だが、警告なしのASan確認とは扱わず、正規ゲートの結果とは区別する。

最終SHAの `pwsh -NoProfile -File ./eng/check.ps1`、前後のclean状態、Ready後CIは#55のPR本文へ記録する。
物理HHKBキー、日本語IME候補窓、クリック/Tab、最小寸法と高DPI/両テーマの目視は未確認。
永続化スキーマ版1、依存、ゲートの変更なし。Waivers: none。
