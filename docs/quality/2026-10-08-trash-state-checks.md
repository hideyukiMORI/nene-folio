# ノートをごみ箱へ移した後の状態遷移 — #196（単位 B2）

- 日付: 2026-10-08
- 親: #169、仕様: FR-035、判断: ADR 0040 の #196 補正（事前確保・結果の文言）
- 実装: NeNe Folioサナ（SOL）。Waivers: none。
- 対象: core / application、偽ポート、文言と UI の失敗分類表。B1 の実ごみ箱 probe は反復していない。

## 実装境界

`folio_state_trash_note(state, target, units, count)` は範囲検査 → 同期 → 現在の EDIT 文書だけの保存 → 削除後の台帳・一致集合・空表示の事前確保 → trash port → 採用を通る。
`TRASH_TRASHED` / `TRASH_ABSENT` だけを採用し、3 つの拒否結果は準備物を捨て、必要な同期・保存後の状態を維持する。
現在文書の削除は NONE / VIEW / 空本文 / カーソルなしで、次のノートを読まない。別ノートの削除は本文・RTF・モード・無題・置換下見・履歴を保ち、選択添字と非表示になったカーソルだけを補正する。
台帳書き戻し失敗は `index_pending` の既存同期で再試行し、NONE の保存も同期後に `NOTHING_SELECTED` で拒む。
`index_filter_removed` は一致判定をやり直さず、対象を除き、同カテゴリの後続番号を詰めた不変な写しを作る。失敗では入力と out を変えない。
`note_corpus_remove` は `locate` / `discard` を再利用し、名前を旧台帳破棄前に使い切る。採用後の処理は新しい確保を要しない。

## 固定した境界

| 検証境界 | 結果 |
| --- | --- |
| 現在 VIEW / EDIT と archive → write → trash → ledger の順 | 単体で固定 |
| 別ノート（前・後・別カテゴリ・読めない対象）、無題保持、本文/RTF同一性 | 単体で固定 |
| 置換下見の再適用、履歴保持、現在削除時の破棄 | 単体で固定 |
| 3 拒否値・ABSENT・保存失敗・保存成功後の trash 失敗 | 単体で固定 |
| 範囲外では同期しない、改名未完了は進まない、同期完了後の新名を対象にする | 単体で固定 |
| 台帳失敗後の状態採用 → NONE の保存でも再試行 → 他ノートへ書かない | 単体で固定 |
| 一致/非一致/最後の一致/別カテゴリ、検索語とスクロール、削除済み cache の再出現防止 | 単体で固定 |
| 最後の一致削除で隠れるカテゴリからカーソルを外す | 単体で固定 |
| 準備中の全確保失敗で trash 呼出し 0、filter 入力/out 不変 | 測定ビルドで確認する |

## 検証コマンドとログ

- `cmake --build D:/NeNeFolio/worktrees/196-trash-state/build --target folio_tests`: exit 0。
- `ctest --test-dir D:/NeNeFolio/worktrees/196-trash-state/build -R '^folio_unit$' --output-on-failure`: 1/1、exit 0。
- `python eng/conformance.py`: exit 0（中間コード）。
- 最終コードの `pwsh -NoProfile -File ./eng/check.ps1`: 未実行。
- ログ: `D:/NeNeFolio/agents/196-state/build.log`、`unit.log`、最終 gate は `gate.log`。
- 初回対象ビルドは B1 の healthy_adapter 行数制限で失敗し、B1 追補取込で解消。初回 unit は追加 ui_text ID の期待値表未追記で UBSan が拒否し、3 行追加で解消。

## 自己レビューと残る確認

ARC-001 / ARC-003 / ARC-004 / ARC-010 / ARC-011: 一致判定・保存・同期・文言を既存の唯一の経路に載せ、OS 呼出しは B1 の port に閉じる。
C-002 / C-003 / C-005 / C-012 / C-018、CNF-009 / CNF-011: 閉じた結果 3 値と全表、3 言語を同じ変更で追加する。新しい公開可変型は無い。
QLT-008 / QLT-009 / QLT-010: 変更境界の単体と確保失敗を追加し、閾値・除外・抑制は変更していない。

EN / zh-Hans の母語話者による確認は未実施。実 UI の Undo と面の後始末は B3 の責務。FAILED は元残存を保証できないので実体とごみ箱の確認を促す。
保存形式・スキーマの変更は無い。B1 の成功後にエラーが生じた場合の実体差異は adapter の限界として残る。
