# #302 — ドロワー新規入口の確認

2026-10-14、[ADR0051](../adr/0051-drawer-new-note.md)の完成候補をNeNeFolioサナが単体で実装・自己レビュー・検証した。
Windows 11 Pro 10.0.26300、120 DPI、PMv2、合成fixtureと実Win32 window/application/adaptersを使用。
証跡は`D:/NeNeFolio/agents/302-drawer-new-note/`。ビルド入力は同所の`candidate-inputs.json`で識別する。

## コマンドと対象

- 固定toolchainを読み、`cmake -S . -B D:/NeNeFolio/outputs/302-drawer-new-note/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`、
  `cmake --build D:/NeNeFolio/outputs/302-drawer-new-note/build --target nenefolio folio_tests -j 6`が成功。
  正典CMakeのclang-tidy、ASan/UBSan/nullabilityを維持した。`build.log`。
- `folio_tests.exe --commands-ui`が成功。新しい日本語期待値と3言語表、既存コマンドの境界が対象。`unit.log`。
- `python eng/conformance.py`は違反0、`git diff --check`成功。最終headの`eng/check.ps1`は必須CIで確認する。
- 外部harnessの`compile-native.ps1`で正典ライブラリへリンクした`native-probe.exe`を専用desktopで実行。
  通常fixtureは85 assertion成功、カテゴリ0件fixtureは16成功、`compile-filter.ps1`の補完は44成功。いずれも最終実行exit 0。
  判定ファイルは`native-results.json`、`native-empty-results.json`、`native-filter-results.json`。

## 確認した境界

- ja/en/zh-Hansの一覧320 logical px、明暗テーマ、320×360 logical pxで文字付きボタンを実画像で確認。
  新規・検索・カテゴリ＋は別の段で、スクロール後も固定帯を保つ。拡張した帯の下のノート行もクリックで開けた。
- ボタンを押して外で離す、capture喪失は実行しない。既存カテゴリ＋はカテゴリ作成を開き、
  カテゴリ0件からカテゴリを作って新しい無題へ進める。0件で新規を押すと理由を示し、一覧の幅を保つ。
- 別カテゴリの右クリックはそのカテゴリを新規・初回保存の既定先にする。メニュー取消では選択・カーソル・focusを動かさない。
  旧無題を別カテゴリへ保存しても、その後に作る無題の作成先は維持する。
- 新規のキャンセルと同名保存失敗では本文・選択範囲・Undo・dirtyと一覧の幅を保持。
  保存しない選択とCtrl+Nの文字経路も同じ新規へ合流し、成功後に両方へ広げて空の編集本文へfocusを移す。
- 一致0件の絞り込み中の常設新規、別カテゴリだけが一致する右クリック新規、同じ状態での取消/保存しないを確認。
  検索語は消さず、文書カテゴリ・初回保存先を保つ。
- 画像はPrintWindowの実寸取得で、通常利用のアプリ/dataを使っていない。desktop切替・SetForegroundWindowを呼ばない。
  成功した通常・0件・補完の実行前後の前景HWNDはそれぞれ一致。試験窓は終了した。

## 初回の失敗と確認の限界

最初のunitは新しい文言IDの日本語期待値を既存テスト表へ足し忘れ、nullability検査で停止した。
期待値を追加して対象unitを再実行し、成功した。製品の文言表とゲートは緩めていない。`unit-initial.log`を保持。

最初のnativeはメニューへ送るキーの宛先とCtrl+Nの入口を試験側で誤り、69成功/14失敗となった。
本体へHome/Enterを送る方法をpopupのDown/Enterへ直し、Ctrl+Nは製品の既存WM_CHAR経路へ揃えて確認した。
補完では項目の選択前にEnterが届く不安定さが残り、1回は60秒で試験所有プロセスだけを終了、
1回は選択されずに終了した。最終補完はWin32の実際のhighlightを確認してからEnterを送り、44成功/0失敗。
これらは試験helperの変更で、検証中の製品コードは不変。初回ログ・ソースと未完JSONも同証跡に保持した。
初回nativeの前景HWNDは前後で異なったため、一致したとは記録しない。成功実行の結果とは分けている。

ASanの起動時interception診断はnative stderrに残る。最終の3実行はexit 0で、Debug対象unitも成功。
物理IME/実打鍵、UIA/実読み上げ、別OS/DPI、全配色と実利用者評価は未測定。
中核の作成・初回保存・確保失敗の実装は変更していないため、無関係な全件ローカル回帰は加えていない（QLT-012）。
保存schema・依存・品質ゲートの変更なし。Waivers: none。
