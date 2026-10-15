# #306 — 本文上部のノート内検索の確認

2026-10-15、[ADR0052](../adr/0052-note-search-surface.md)の候補を対象検証した。
hideの今回の分担指定により、Astraが設計・受理、SOLが実装・native検証・独立静読を担当。
実Win32の環境はWindows 11、PMv2、120 DPI、専用desktopと合成fixture。通常のアプリ/dataは操作していない。
恒久証跡は`D:/NeNeFolio/agents/306-note-search/`。入力は`candidate-inputs.json`と`native-linked-inputs.json`で識別する。
候補exeのSHA256は`1C1AC58FEA90BBE9D18FBD6906FDEB99E47030D16A4843F8BE65ED0348603A2B`。

## コマンドと結果

作業木`D:/NeNeFolio/worktrees/306-note-search-surface`で`eng/toolchain.ps1`の固定環境を使った。

- `cmake -S . -B D:/NeNeFolio/outputs/306-note-search/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`成功。
- `cmake --build D:/NeNeFolio/outputs/306-note-search/build --target nenefolio folio_tests`成功。
  正典CMakeのclang-tidyとDebug計装を維持。初回は配置関数のネストが4になりC-012で停止し、
  検索高の条件加算を同じ式に置いて修正した。`build.log`が初回、`build-fix.log`が成功結果。
- `D:/NeNeFolio/outputs/306-note-search/build/folio_tests.exe --commands-ui`成功。
  変更した表示文言、3言語のEnter表記、既存コマンドの契約を確認。`unit.log`。
- 変更3件の`clang-format --dry-run --Werror`、
  `python eng/conformance.py --build-dir D:/NeNeFolio/outputs/306-note-search/build`（0違反）、`git diff --check`が成功。
- 外部`compile-native.ps1`で現行folio_window.cと正典製品libを使うprobeを作り、`run-native.ps1`で専用desktopへ起動。
  初回345 assertionのうち344成功、下記の試験側誤判定1件でexit 1。
  同じ`compile-native.ps1 -RuntimeDirectory D:/NeNeFolio/outputs/306-note-search/native-runtime-empty`で試験を補正し、
  `NativeDesktop306.Run`へ補足専用のexe/data/stderrを指定した空欄・最小幅確認は28/28成功、exit 0。
  成功済みの機能検証は繰り返さず、製品source/libs/fontsが同一であることをhashで確認した。

## 確認した振る舞い

- マウスの入口・同じ欄の再開・入力・前後・×、Enter/Shift+Enter・F3・n/Nが既存検索を使う。
  ×とEscは可視の元フォーカスへ戻り、本文選択を保つ。前後は覚えている方向を変えない。
- IMEのSTART→入力変更→Enter/前後/×→ENDを合成メッセージで確認。
  変換中は検索語・結果・本文選択・入力欄の本文/選択/Undoを保ち、無視した×を終了後に再実行しない。
- 本文の正確なUTF-16、選択、Undo、dirty、表示論理行を保持。
  検索中Ctrl+Sが実adapterで元の未保存挿入を保存し、欄/語/focusを保つ。
  一連の操作後に本物のUndoでその挿入を取り消せる。
- 全ノート絞り込みが0件のまま、独立した語で本文を検索できる。閲覧と編集の表示本文を対象にする。
- 最小BOTH560×360 DIP（本文ペイン320）、NOTE、DRAWERからの検索、本文幅639/640 DIPの段数切替を確認。
  最小の入力幅は160 DIP、残る本文高は108 DIP。結果の変化だけでは段数を変えない。
- 3言語×明暗の最小BOTH、空語・0件・不正語、広幅境界で対象/方向・結果と×が重ならない。
  Englishの最長エラーは最小時297/380 px、広幅境界297/330 pxで全文収容。
  検索帯とRichEditは重ならず、検索直後の一致が本文内に見える。
- palette/replace/history/settings/表示切替へ移っても予約した帯の空白が残らない。
  PrintWindow画像19件を保存し、native担当は最小BOTH全6件を含む14件を目視、設計席も主要7件を目視した。
  `image-manifest.json`と`visual-qa.json`に画像・hash・目視範囲を記録した。

## 試験側の補正と限界

初回probeは空語にも結果幅が正であることを要求し、正しい`NO_TERM → UI_TEXT_EMPTY`を失敗扱いにした。
初回のsource/exe/結果/起動ログを`native-initial/`に保持し、空結果だけ幅0を正常とする契約へ補正した。
空語の広幅と最小BOTHだけを再確認し、入力幅の確認も設計条件120 DIP以上へ揃えた。
補足初回の起動は既存stderrファイルの再利用が拒否されたため、別名`native-empty-stderr.log`で起動し成功した。
初回全体を成功と書き換えていない。合成集計は`native-summary.json`、詳しい手順は`native-verification-report.json`。

両workerの前景HWNDは実行前後で同じで、desktopを切り替えていない。試験窓は終了済み。
両stderrにASanの`interception_win: unhandled instruction`起動診断1行がある。
外部probeのinclude compileにはnullability警告82件があり、rawログを保持した。
製品のDebug/tidy成功と外部harnessの診断を分け、stderr空やsanitizerによる完全保証とは扱わない。

独立SOLの差分静読ではP1/P2なし。物理IME/実打鍵、UIA/実読み上げ、別OS/DPI、全テーマ、hide実機受理は未測定。
中核検索・保存の実装は不変なので、無関係な全件ローカル回帰を追加していない（QLT-012）。
最終headのフルゲートは紐付くPRの必須CIで確認し、統合・通常exe反映・整理の最終結果は同証跡の`completion.json`へ残す。
保存schema・依存・ゲートの変更なし。Waivers: none。
