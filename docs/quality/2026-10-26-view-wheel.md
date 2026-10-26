# 閲覧本文への通常ホイール配送 — 2026-10-26

Issue: [#392](https://github.com/hideyukiMORI/nene-folio/issues/392)。基点main `875a214e7b231a0a5e0a878890dc9f452cc8e747`。
規則: ARC-001/004/005/011/012、C-008/012/013/016/017、QLT-008/012/013、GIT-001〜004。
判断は[ADR0067の補正](../adr/0067-note-display-zoom.md)。Waivers: none。

## 問題と修正

hideの「閲覧モードでスクロールホイールが効かない気がする」という報告を受けた。
閲覧への切替は本文から主窓へfocusを戻すが、主窓の本文へのホイール転送はCtrl付きだけに限定されていた。
通常ホイールは本文上でも主窓が消費していた。本文へ直接届いた通常ホイールは既存のnative処理へ進む。

`folio_window.c`の既存の座標による振分けを通常/Ctrl共通にし、同じ本文HWNDへ渡す。
`note_pane.c`はnativeの同期ホイール処理中だけ再入を拒否する。未処理入力が親へ伝播して戻っても、
native処理と既存の行番号通知を重ねない。スクロール量やfocusの新しい所有者は作らず、倍率の経路も維持する。
公開API・保存schema・依存・ゲートの変更は無い。

## 修正前の再現と対照

恒久証跡は`D:/NeNeFolio/agents/view-wheel-20261026/`。
`baseline-inputs.json`に入力の識別、`baseline-routing.log`に修正前の結果を保存した。

- 製品の`folio_window.c`と`note_pane.c`を直接コンパイルし、専用非入力desktopで配送先を観測した。
  16 checks中4 failures、exit 1。閲覧/編集それぞれの主窓focus・絞り込み欄focusで通常ホイールが本文へ0回届く
  （期待1回）。直接本文への通常ホイール、主窓経由のCtrl+ホイール、本文外座標、nativeモーダルの対照は成功した。
- 素のMSFTEDITでreadonly有無、縦スクロールバー有無、バーの非表示を6条件比較した。
  全条件でwheel lines=3、focus/active/visible/enabledが正常でも、合成ホイールでは先頭行とscrollYが動かず、
  PageDownは11行動いた。`native-wheel.c`と`native-wheel.log`に記録した。
- このnative対照の不動を製品欠陥のredには用いない。ADR0026と#375の品質記録にも同様の未判定が残る。
  確定した不具合は主窓での配送欠落であり、物理ホイールによる実スクロールの確認とは区別する。

## 対象検証

修正後は106 checks / 0 failures、process exit 0（`candidate-routing.log` / `candidate-routing-exit.json`）。
元の4失敗はすべて本文への配送1回へ変わった。閲覧/編集の両方で以下を確認した。

- 直接本文/主窓から始まるnative親伝播を故意に発生させても、native呼出しと行番号通知は各1回。
  本文HWNDには再入分を含めて2回届くが、再入した呼出しをnativeへ渡さない。次の独立した入力ではguardが解除されている。
- 本文外、一覧への通常/Ctrl、非表示/無効な本文、設定画面の背後、nativeモーダル、正典のRTF投入中の配送境界。
- 通常ホイールの元のdelta/修飾/画面座標、Ctrlの既存倍率経路、本文・選択・変更印・実focusの保持。
  未保存編集のUndo/Redoがホイール後も実行できること。

実行したコマンド:

- `pwsh -NoProfile -File D:/NeNeFolio/agents/view-wheel-20261026/build-fixture.ps1` — fixture生成/コンパイル成功。
- `D:/NeNeFolio/outputs/view-wheel-20261026/runtime/fixture.exe`（cwdは同runtime）— 上記106 checks、exit 0。
- `pwsh -NoProfile -File D:/NeNeFolio/agents/view-wheel-20261026/build-product.ps1` — 最終候補のDebug target `nenefolio`をclang-tidy付きでビルドし、変更2ソースの整形も成功、exit 0。
- `python eng/conformance.py` — 0 violations。`git diff --check` — 成功。

最終入力とbuild終了値は同directoryの`candidate-inputs.json`と`implementation-report.md`へ収載した。
fixtureのstderrには既存の`interception_win: unhandled instruction`起動warningがある。ASan/UBSan/nullabilityの違反診断は無い。
成功済み対象結果は関係入力不変なら再利用する。

## 統合の記録先

最終PR headの`pwsh -NoProfile -File ./eng/check.ps1`と必須`check`は統合条件とし、
最終PR/CI/通常exe反映/整理は同directoryの`completion.json`と実際のPRを照合する。

## 限界と整理

専用desktopの合成入力検証であり、物理ホイール・物理IME・実reader・DWM呈示を検証済みとは扱わない。
通常アプリや利用者の本文を操作しない。通常exe更新は既存の`tools/prepare-real-machine.ps1`を使い、
対象が稼働中なら保存して終了されるまで更新しない。アプリの自動終了・自動起動は行わない。

作業木は`D:/NeNeFolio/worktrees/392-view-wheel`、一時出力は`D:/NeNeFolio/outputs/view-wheel-20261026`。
統合・必要反映・恒久成果物の収載後に、取込状態・未保存物・稼働参照・リンクを確認して当該2箇所を整理する。
branch/commitは保持する。旧削除拒否済み13箇所は本件の対象外。
