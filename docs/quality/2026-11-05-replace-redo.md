# 置換の下見と一単位のRedo — 2026-11-05（#691）

既存replace_native（image_status_tests --replace）に、Undo後の下見開閉と実Redo/Undoを追加した。
**237checks/1.355697秒、stderr空**。1件/全置換の両方がRedo一回で正確な本文へ戻り、再Undo一回で元本文へ戻った。
保存前は全保存履歴とstore_revisionを保持し、既存の明示保存一世代も成功した。製品欠陥は再現せず、製品/API/schema/CMake/selector/gateは変更しない。

## 正典経路と確認内容

[先行#653](2026-11-05-replace-native.md)の実Win32 window、実ICU、実persistence adapter、公開入力と世代付きボタン、全履歴snapshotを共有する。
[ADR0028](../adr/0028-regex-replace.md)決定7の一単位の置換と、下見・置換が保存しない契約を固定する。
正常な3行「猫1 😀」「猫2 **saved**」「猫1 `old`」、パターン(猫)([12])、群2/群1/足跡絵文字/改行の既存入力を使う。

1. 既存1件置換をUndoして元本文へ戻し、Redo可能を確認する。
2. 公開置換を再開し、実下見が3件となる正常パターン/置換欄入力を通して閉じる。
3. 元の全UTF-16本文/SAME、本文focus、Redo可能、現在保存ノートと全履歴の全bytes・不存在、store_revisionを確認する。
4. 実EM_REDO一回で1件置換済みの固定本文/CHANGEDへ戻し、全保存内容の不変を確認する。
5. 実Undo一回で元本文/SAMEへ戻し、全保存内容を再照合する。
6. 全置換の既存Undo後にも同じ2〜5を通し、3件すべてを含む固定本文が一度のRedoで戻ることを確認する。
7. 既存の再全置換→明示Saveを維持し、保存本文/履歴の一世代更新を確認する。

変更はtests/win32/image_status_tests.cのredo_replacement（4引数）と既存replace_and_undoからの2呼出だけ。
製品のRedo経路を試験用に追加せず、既存RichEditと通知処理を使う。本文の全期待値は試験側の固定文字列で、製品の計算結果を期待へ流用しない。

## 全試行と検証

mainfed5429からD専用test/691-replace-redoを作成。全試行はtools/record-run.pyで新run-dirへ入力snapshot/hash/raw/終了値を保存した。

- configureDebug01成功、buildDebug01は正規formatter/静的解析を保って142actions成功。
- #689統合後maine17c178へFFし、保存した今回の差分を再適用。差分の内容は同一、製品/CMake不変。
  共有test sourceに#689の検索試験が加わるためbuildDebug02で2actions再buildし、最終sourceを対象試験に使った。
- contract01は呼出側の-Selector=--replace指定が不適切で、補助入口の引数assertで停止した。native試験未起動、data未作成。
  子1、未生成artifactで記録125/error1。初稿・入力・rawを保持し、成功扱いへ書き換えない。
- contract02は引数を-Selector '--replace'へ訂正。試験コード/期待/規則は不変。
  専用data初期0file、30秒deadline、独立desktop切替なし。237checks/1.355697秒、native/補助/記録0、stderr空、画像要求0。

最終docs/source/buildgraph conformanceと自己レビューで最終SHA/差分範囲/全試行実値を照合し、最終PR head必須CIにeng/check.ps1を要求する。
最終CI/統合/通常exe-data不変/全保全・専用整理の原本はD:/NeNeFolio/agents/691-replace-redo/integration-completion.json。
作業木と出力の全fileをZIP/hash照合し、取込tree/未保存・untracked・ignored/link/稼働参照を監査して整理する。branch/commitと全失敗は保持する。

## 範囲と限界

短い正常3行と既存の群参照/CR/Unicodeが対象。物理keyboard/IME/巨大本文/全OS-DPIや追加の失敗注入の保証はしない。
Redo後のcaret/scroll位置や置換帯を開いたままのUndoは新たに規定しない。通常app/実ノート/Storeを試験せず、#634保留を維持する。
OS全I/Oの捕捉ではなく保存ファイルの全bytesとrevisionの照合。性能効果は主張しない。主担当単体、追加委任なし。
ARC-001/004/008/012、C-008/012/014/016、QLT-008/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。[日報](../reports/2026-11-05.md)・[引き継ぎ](../handoffs/2026-11-05.md)。

## 21:01 統合・全保全整理完了

#691はPR692/head6819f2b4b3077b044f8ef43235b5946ef2abd38c、最終CI42022605286 full-gate/check成功。
mainbd2394f42b415c7c64549ff88286c8d1fa770ca0へ正典統合、Issue691 CLOSED。#683通常exe/data32file bytes/hash/作成更新日時不変。
1390file全保全ZIP再hash/取込tree/clean-untracked/ignored/link/稼働参照監査後、691専用worktree/outputだけ削除、branch/commit保持。
初回の引数指定による補助入口失敗も全実値と照合。原本D:/NeNeFolio/agents/691-replace-redo/integration-completion.json。
