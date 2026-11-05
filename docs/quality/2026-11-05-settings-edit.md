# 設定切替と未保存本文・編集履歴 — 2026-11-05（#679）

行番号・ミニマップ・表示言語の公開SELECTが、未保存の本文と編集履歴を保持する契約を
settings_edit_native（既存image_status_tests --settings-edit）へ固定した。ADR0031/0032の意味は変更しない。
#645の設定値・選択印・3言語表示の回帰に加え、実RichEditと実persistence adapterの組合せを確認する。
製品欠陥は再現せず、製品/API/schema/所有境界/ゲートは変更しない。

## 守る契約

専用の名前付き日本語/emoji本文と全保存履歴を用意し、未保存追記後とUndoしてRedo可能な状態の2回で同じ操作を通す。

1. 本文全units・選択・native modified・Undo/Redo可否・focusを既存native_body_snapshotで保存する。
2. 公開settings Invokeを通し、現revisionの行を辿って行番号/ミニマップを交互に4回SELECTする。
   2値の全4組合せを経由し、元の値へ戻す。EN→ZH→JAも各SELECTする。
3. 各操作後に実settingsを再読し、全field（2boolean/language/theme）が意図した値であることとstate側の値を比較する。
4. 本文snapshot、全保存履歴の各本文、store_revision、設定面の可視性を各操作後に比較する。
5. 設定面を閉じて本文focusを含む閉鎖前snapshotへ戻ることを確認する。
6. 2回のcycleの前後で本文とcanonical dirtyを比較し、最後の実Redo/Undoが期待全文へ戻ることを確認する。

試験sourceはtests/win32/image_status_tests.c。既存plain note/履歴fixture、本文snapshot/全履歴比較helperを共有する。
新しい製品hook、設定保存経路、本文同期経路は作らない。CMakeLists.txtに同targetのselector登録と
TIMEOUT30/RESOURCE_LOCK image_status_dataの2行だけを追加した。CTest生成登録の読み取り照合でも同exe/selector/lock/timeoutを確認する。

## 実行と結果

mainf1b5a138916b7bbeceb2644887a9cfb5be212f1b（#677統合済み）から専用Debugを構成した。
作業木D:/NeNeFolio/worktrees/679-settings-edit、出力D:/NeNeFolio/outputs/679-settings-edit。
全実行原本はD:/NeNeFolio/agents/679-settings-editへtools/record-run.pyで入力snapshot/hash、raw出力、終了値を保全する。

- configure-debug-01成功。
- build-debug-01は試験コードの未宣言ACCESSIBLE_SURFACE_ROOTとrun_fixture認知的複雑度11>10で停止。子/記録1、記録error0。
  正しい公開名ACCESSIBLE_SURFACEへ直し、新selectorの分岐を既存run_other_fixtureへ移した。規則/抑制/期待は変えない。
- build-debug-02: 正典Debugのformat/静的解析を含むimage_status_tests target、4actions成功。
- contract-01: 専用data初期0file、独立desktop（切替なし）、30秒timeoutを維持して--settings-editを実行。
  **472 checks / 2.565864秒、native/補助/記録0、stderr空**。
- registration-01: ctest --show-only=json-v1 -R ^settings_edit_native$ で実生成登録を読み取り、試験の反復はしていない。

成功したtest/CMake入力は以後不変で再利用する。最終docsの参照/source/buildgraph conformanceと自己レビューでSHAを照合し、
最終PR headの必須CIに正典eng/check.ps1成功を要求する。統合後は通常exe/data不変確認と全保全/専用整理を行う。
最終CI/統合/通常不変/保全整理の原本は同agents/679-settings-edit/integration-completion.json。branch/commitと全途中失敗を保持する。

## 範囲と規則

短い有効な本文の通常操作が対象。折返し・論理scroll位置・長文reflow・物理入力の新しい保証は含めない。
実ノート、Store、通常appは試験に使わない。速度効果を主張しない。親単体、追加委任なし。
初回compile/静的解析失敗、読取補助のctest未初期化PATHも記録し、正典toolchainを読んだ実登録照合へ進めた。
ARC-001/004/008/011、C-005/008/012/014、QLT-008/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。[日報](../reports/2026-11-05.md)・[引き継ぎ](../handoffs/2026-11-05.md)。
