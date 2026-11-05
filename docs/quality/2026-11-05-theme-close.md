# テーマpreview中の通常終了 — 2026-11-05（#677）

ADR0061決定5の「通常終了で未確定themeを書かない」とADR0053/0050の名前付き本文の保存終了を、
既存theme_preview_native（image_status_tests --theme-preview）へ固定した。
製品欠陥は再現せず、製品/API/CMake/schema/timeout/data lock/期待する既存振る舞いは変更しない。

## 守る契約

既存の取消・Ctrl+S・theme確定・別候補取消・Undo/Redo回帰の後、保存本文からUndoした未保存状態を使う。
このため終了で保存を落とせば直前の保存bytesが残り、過剰に保存すればrevision/履歴shiftで検出できる。

1. 未保存本文を保持したまま公開settings Invoke→theme面→別候補Selectを通す。
2. 保存choice/settings全bytesと本文の全保存履歴が操作前と同じことを確認する。
3. 生きているroot HWNDへWM_CLOSEを送り、既存のdeferred close→depart_document/store_body→finish_quitを通す。
4. 既存testと同じ1000messageの有限pumpで処理し、rootの破棄・公開root handleのnull化・WM_QUITの終了値0を確認する。
5. 実persistence adapterから本文と全履歴を再読し、期待本文、全slotの一世代shift、store_revision+1を比較する。
6. 保存choiceとsettingsファイル全bytesが同じことを再確認する。最後のfolio_window_destroyは所有解放だけに使う。

tests/win32/image_status_tests.cだけを変更し、既存本文保存期待helperの本文を引数化してCtrl+S/終了の両方で共有する。
製品へtest hookを足さず、保存stubや新しい終了経路を作らない。新CTest/selectorも追加せず、既存theme回帰へ収める。

## 実行と結果

main6ab2539c6814da8ea15ce42e985373c5edadfe28（#675統合済み）から専用Debugを構成した。
作業木D:/NeNeFolio/worktrees/677-theme-close、出力D:/NeNeFolio/outputs/677-theme-close。
実行原本はD:/NeNeFolio/agents/677-theme-closeへtools/record-run.pyで入力snapshot/hash、raw出力、終了値を保全する。

- configure-debug-01成功。
- build-debug-01は追加testの2箇所のclang-format差分でcompile前停止。子/記録1、記録error0。正規formatterで当該fileを整えた。
- build-debug-02: 正典Debug、変更testのformat/静的解析、image_status_tests target 142actions成功。ゲート/警告/期待は緩めない。
- contract-01: 専用data初期0file、独立desktop（切替なし）、30秒timeoutを維持して--theme-previewを実行。
  **418 checks / 1.304600秒、native/補助/記録0、stderr空**。旧preview/本文保存/確定の回帰も同じselectorで通過。

成功済みtest/CMake入力は以後不変で再利用する。最終docsの参照/source/buildgraph conformanceと自己レビューでSHAを照合し、
最終PR headの必須CIに正典eng/check.ps1成功を要求する。統合後は通常exe/data不変確認と全保全/専用整理を行う。
最終CI/統合/通常不変/保全整理の原本は同agents/677-theme-close/integration-completion.json。branch/commitと全途中失敗を保持する。

## 範囲と規則

今回の正常終了は、名前付きの有効な本文で保存が成功する条件である。無題/保存失敗/OS終了の既存契約は変えていない。
物理入力、実OS shutdown、実ノート、Store、通常appは試験に使わない。新しい速度効果や一般表示保証を主張しない。
初回format失敗、補助readの推測path/glob失敗も全体journalへ記録する。親単体、追加委任なし。
ARC-001/004/008/011、C-005/008/012/014、QLT-008/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。[日報](../reports/2026-11-05.md)・[引き継ぎ](../handoffs/2026-11-05.md)。
