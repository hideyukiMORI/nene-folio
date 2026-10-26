# 本文の通常ホイール配送とnativeスクロール — 2026-10-26

Issue: [#392](https://github.com/hideyukiMORI/nene-folio/issues/392)、PR: [#393](https://github.com/hideyukiMORI/nene-folio/pull/393)。
基点main `875a214e7b231a0a5e0a878890dc9f452cc8e747`。
規則: ARC-001/004/005/011/012、C-008/012/013/016/017、QLT-008/012/013、GIT-001〜004。
判断は[ADR0026](../adr/0026-line-numbers.md)と[ADR0067](../adr/0067-note-display-zoom.md)の同日補正。Waivers: none。

## 問題と修正

hideの閲覧モードでホイールが効かない報告に、本文クリック後も効かないという追加情報があった。原因は二つ。

- 閲覧へ戻るとfocusが主窓へ移る一方、主窓は本文へのホイール転送をCtrl付きに限定していた。
  `folio_window.c`の既存の座標振分けを通常/Ctrl共通にし、可視・有効・準備済みの同じ本文HWNDへ渡す。
- `note_pane.c`がバーを隠すため`WS_VSCROLL`を付けずに生成しており、直接本文へ届いてもnativeで動かなかった。
  `WS_VSCROLL | ES_DISABLENOSCROLL`で生成し、直後の`EM_SHOWSCROLLBAR(SB_VERT, FALSE)`で外観だけを隠す。
  初期0寸法・空/短文でもnativeの能力を維持し、バーは不可視・占有幅0を保つ。

nativeの同期ホイール処理中だけ本文側で再入を拒否し、未処理入力が親へ伝播して戻っても二重処理しない。
同期呼出し後の独自通知は1回、非同期移動の複数の`EN_VSCROLL`は既存の行番号更新へ通す。
スクロール量やタイマの独自実装、focusの移動は加えない。Ctrl倍率・公開API・保存schema・依存・ゲートの変更は無い。

## 再現と診断の補正

恒久証跡は`D:/NeNeFolio/agents/view-wheel-20261026/`。入力の識別は`baseline-inputs.json`と`candidate-inputs.json`。

- `baseline-routing.log`: 修正前16 checks中4 failures、exit 1。閲覧/編集×主窓/絞り込み欄focusで
  通常ホイールの本文配送0回（期待1回）を確認した。
- 初期`native-wheel.log`は合成ホイール直後だけを読み、すべて不動と記録した。この結果から専用desktopの制約を
  推測したのは不十分だった。追加情報を受け、PRをDraftへ戻して統合を止め、非同期処理を待つ試験へ補正した。
- `native-wheel-v4.log`: readonly有無とも、`WS_VSCROLL`なしは待っても不動、同styleありは3行移動。
  長文投入後の`EM_SHOWSCROLLBAR(FALSE)`は幅を使わず移動を保つ。`ShowScrollBar(FALSE)`はバーが再表示された。
- `native-wheel-v5.log`では生成直後に隠すだけでは初期0寸法で能力を失った。
  `ES_DISABLENOSCROLL`を加えたv6/v7では初期0寸法、長文→短文→長文、RTF再投入、150%倍率、
  リサイズ、非表示中のリサイズからの復帰でもバー不可視・幅0と移動を維持した。v7は64 checks / 0 failures、exit 0。

## 対象検証

配送と再入は`candidate-routing.log`の106 checks / 0 failures、exit 0を再利用する。
配送/guard経路は追加修正で変えていない。元の4条件、直接/主窓からの故意のnative親伝播、
native呼出しと独自通知各1回、次の入力でのguard解除を確認した。
本文外・一覧の通常/Ctrl・非表示/無効・設定背後・nativeモーダル・RTF投入中の境界、
delta/修飾/画面座標、本文/選択/変更印/focusとUndo/Redoの保持も含む。

最終ソースの実製品fixtureは`product-scroll.log`の197 checks / 0 failures、exit 0。
専用非入力desktopで製品の`folio_window.c`と`note_pane.c`をコンパイルし、移動開始後150msの安定を待つ（上限1500ms）。

- 閲覧/編集×主窓/本文focus×上下の8条件で、先頭表示行0→3→0を確認した。
  scrollYは編集0→100→0、閲覧0→270→0。安定まで422〜484msだった。
- 各入力でnative `EN_VSCROLL`が9〜10回、独自通知が1回。編集の行番号は11回描画され、
  最終描画の行と位置が安定後のviewportと一致。全可視行baselineを独立TOM照会でも照合した。
- 本文・選択・変更印・Undo/Redo・実focus・倍率を保持し、Ctrl倍率変更とUndoの実行も成功した。
  両モードでnativeバーは不可視・占有幅0、本文のclient幅を維持した。

実行コマンドと結果:

- `pwsh -NoProfile -File D:/NeNeFolio/agents/view-wheel-20261026/build-fixture.ps1`と旧runtimeの`fixture.exe` — 106項目の配送検証成功（再利用）。
- `pwsh -NoProfile -File D:/NeNeFolio/agents/view-wheel-20261026/build-native-v7.ps1` — native対照の生成/コンパイル成功。実行結果は上記64項目。
- `pwsh -NoProfile -File D:/NeNeFolio/agents/view-wheel-20261026/build-scroll-fixture.ps1`と`runtime-v2/fixture.exe` — 最終実移動197項目成功。
- `pwsh -NoProfile -File D:/NeNeFolio/agents/view-wheel-20261026/build-product.ps1` — 最終Debug target `nenefolio`のclang-tidy付きbuildと変更2ソースの整形成功、exit 0。
- `python eng/conformance.py`、`git diff --check` — 最終結果は同directoryの`implementation-report.md`と`completion.json`へ収載する。

fixtureには既存の`interception_win: unhandled instruction`起動warningがある。ASan/UBSan/nullabilityの違反診断は無い。
成功済み対象結果は関係入力不変なら再利用する。配送だけの旧head `e844baa`のCI成功を最終検証には用いず、
最終PR headで`pwsh -NoProfile -File ./eng/check.ps1`と必須`check`の成功を統合条件とする。

## 統合・限界・整理

最終PR/CI/通常exe反映/整理は同directoryの`completion.json`と実際のPRを照合する。
実移動はバックグラウンドで検証でき、Computer Useは必須ではない。途中の専用テスト窓のGUI試験はhideの停止操作で中止した。
以後GUIを操作せず、別の`runtime-v2`で対象検証を完了した。物理ホイール・物理IME・実reader・DWM呈示は未確認。
通常exe更新は`tools/prepare-real-machine.ps1`を使い、稼働アプリの終了・自動起動・公開Release更新は行わない。

作業木は`D:/NeNeFolio/worktrees/392-view-wheel`、一時出力は`D:/NeNeFolio/outputs/view-wheel-20261026`。
統合・反映・恒久成果物収載後に取込状態・未保存物・稼働参照・リンクを確認して整理する。branch/commitは保持する。
旧runtimeのテスト窓が稼働中なら一時出力は保持し、停止を迂回するための終了操作はしない。旧削除拒否済み13箇所は対象外。
