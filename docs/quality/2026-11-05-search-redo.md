# 検索とRedo可能な編集履歴 — 2026-11-05（#689）

既存search_native（image_status_tests --search）を拡張し、Undo後の検索もRedoを失わないことを確認した。
**246checks/1.001543秒、stderr空**。実Redo/Undoで固定期待の追記本文/元本文へ戻り、全保存履歴とstore_revisionを保持した。
製品欠陥は再現せず、製品/API/schema/所有/保存経路/CMake/selector/gateは変更しない。

## 既存経路と追加する確認

[先行#655](2026-11-05-search-native.md)の専用fixture、実RichEdit、実persistence adapter、公開FIND/前後/closeを共有する。
短い名前付きの正常本文「猫 😀 aaaa 日本語」と追記「 🐾」を使用する。
search_roundtripは従来のUndoに加えてRedoの可否も前後で比較する。VIEW/未保存EDITの既存期待は維持する。

1. 先行caseの未保存追記→検索→Undoを通し、元本文/SAME/全履歴不変を確認する。
2. native EM_CANREDOが非zeroであることを確認し、同じ公開検索操作をもう一度通す。
   ASCII大小無視の重なり3件を前後に巡回、emoji一件、不存在語、空語、closeを扱う。
3. 元本文/SAME、本文focus、Undo/Redo可否、記憶方向、全保存履歴/store_revisionの保持を確認する。
4. 実EM_REDOで追記全文/CHANGEDへ戻り、全保存履歴不変を確認する。実Undo一回で元全文/SAMEへ戻り、再度保存履歴を比較する。

検索による一致選択の移動は既存仕様であり、selectionを元位置へ戻す保証は追加しない。
試験sourceはtests/win32/image_status_tests.cの既存search_roundtrip/search_unsavedのみ。
新helper/fixture/target/selectorや別の検索経路は作らない。ADR0023/0052の本文を再投入しない契約を固定する。

## 全試行と検証

main2c5d5fc（#685統合済み）からD:/NeNeFolio/worktrees/689-search-redo、同outputsへ専用Debugを構成した。
全実行はtools/record-run.pyで入力snapshot/hash/raw/終了値をD:/NeNeFolio/agents/689-search-redoへ保存する。

- configureDebug01成功。
- buildDebug01は新変数redoの挿入先を別のcompleted_image関数へ誤り、検索関数の未宣言変数として停止（子/記録1、記録error0）。
  build開始後の差分確認で発見し、実行中の入力は書き換えず終了を待った。正しいsearch_roundtripへ移し、既存image関数の差分を除いた。
- buildDebug02はformat/静的解析を保って残り4actions成功。規則/抑制/期待を緩めていない。
- contract01: --search、初期data0file、既存独立desktop（切替なし）/30秒deadline。
  246checks/1.001543秒、native/補助/記録0、stderr空。画像要求0。

#687の文書のみの統合後へFFし、成功したtest/CMake/src/engは不変で再利用する。
最終docs/source/buildgraph conformanceと自己レビューでSHA・差分範囲・全実行値を照合し、最終PR head必須CIでeng/check.ps1を要求する。
最終CI/統合/通常exe-data不変/全保全/取込・未保存・ignored・link・稼働参照監査/専用整理の原本は同agents/integration-completion.json。
branch/commitと初稿/全失敗を保持する。通常版の再buildは不要。

## 範囲と規則

正常な短い一行、既定の前方向、公開UI action/native messageが対象。物理入力/IME/大文書/scroll/全OS-DPIや追加の保存失敗の保証はしない。
実ノート/通常app/Storeを試験せず、#634保留を再開しない。性能効果は主張しない。親単体、追加委任なし。
ARC-001/004/008/011、C-005/008/012/014、QLT-008/010/012/013、GIT-001〜004。
追加waiver none、既存WVR-0001継続。[日報](../reports/2026-11-05.md)・[引き継ぎ](../handoffs/2026-11-05.md)。
