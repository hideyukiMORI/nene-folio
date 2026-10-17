# いまのタスク — NeNe Folio

記録日: 2026-10-17。[#319](https://github.com/hideyukiMORI/nene-folio/issues/319)の基本UI Automation/キーボード到達を実装中。
基点mainはa2f253cd6d008b400e05908e9f2600079c5fc60e。枝はfeat/319-basic-accessibility。
hideのcommit/push指示により途中状態を保存する。受理・統合・通常exe反映は未完了。

## 現在の到達点

- UIスレッド上のCOM provider、native入力名の注釈、ヘッダ/索引/本文操作列の公開、F6/Tab移動を実装した。
- applicationの索引cursorを文書選択から独立させ、SetFocusと保存を伴うSelectを分けた。
- 対象build/tidyと`folio_tests.exe --accessibility`は成功。実行コマンドと限界は[品質記録](../quality/2026-10-17-basic-accessibility-checkpoint.md)。
- 独立レビューは途中。Invokeの実行通知不足が指摘され、未修正。
- 実画面確認はhideのEsc操作で停止。試験データのJSONキー順を修正した後の起動、reader発話は未確認。

## 残る受理条件

- Invoke通知の修正と、COM寿命/参照保持/破棄/再入の独立レビュー完了。
- 製品providerのproperty/pattern/event、SetFocus/Select、失敗/取消、native Text/Value/Undoの対象検証。
- 3言語/3表示/狭幅/通常幅/絞込0件/画面外行、IME/modal/終了中の拒否、実reader発話の確認。
- ADR0015/関連するADR0013、FR-018、ARC-004の状態契約との照合・更新。
- 最終PR headの必須CI、main統合、通常exe反映、恒久証跡収載、専用作業物整理。

## 保持する場所と後続

作業木はD:/NeNeFolio/worktrees/319-basic-accessibility、build/試験データはD:/NeNeFolio/outputs/319-basic-accessibility。
証跡はD:/NeNeFolio/agents/319-basic-accessibility。未完了の実装と検証入力があるため、現時点では削除しない。
通常exeはC:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe（#311反映時のまま）。

#312の範囲設計はPR #322で統合済み。履歴は[日報](../reports/2026-10-17.md)と[引き継ぎ](../handoffs/2026-10-17.md)。
#320のOS高コントラスト、#321の補助画面公開、性能、配布、UI分割は別件。
成功済みで関係入力が不変の対象検証は再利用する（QLT-012）。保存schema・ゲート変更なし。Waivers: none。
