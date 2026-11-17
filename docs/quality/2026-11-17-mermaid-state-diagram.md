# 状態遷移図の品質記録 — #872

2026-11-17。開始main `ca6cb3c`（#869統合後）。設計は[ADR0104](../adr/0104-mermaid-state-diagram.md)。
作業木E:/NeNeFolio/worktrees/872-state-diagram（branch feat/872-state-diagram）、対象実行の原本E:/NeNeFolio/outputs/872-state-diagram、
完了後の置き場D:/NeNeFolio/agents/state-diagram-20261117。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 試行（時刻はJST）

| 試行 | 結果 |
| --- | --- |
| build-01 | 成功（最初の構築で警告なし） |
| unit-01 | 失敗（試験）: 複合状態の試験に「複合状態への遷移」（`[*] --> Outer`の後で`state Outer {`）を入れていた。設計では未対応なので試験から外し、拒否の側へ移した。製品側は、この場合を「不正」ではなく「未対応」と答えるよう直した |
| unit-02 | 失敗（試験）: 複合状態に図全体と違う`direction`を書いた例が、配置で未対応になった。フローチャートのサブグラフと同じ既存の制限で、試験の例を同じ向きへ直した（向きの記録は確かめる） |
| unit-03 | 成功（`folio_tests.exe`全体）。模型・複合状態・17通りの拒否 |
| 描画 | 最初の採取の手順で、出力先の中身を消す行が道具の安全確認で止められ、何も実行されなかった。消さずに作り直す形で取り直した。`diagram_render_tests`に状態遷移図を加え、533 checks成功。`visual/state-{light,dark}.png` |
| 見た目 | 状態・ラベル付きの遷移・choice・複合状態の枠と中の開始と終了を確かめた。気づいた点（今回は直さない）: 開始・終了の円がふつうの円と同じ大きさ（Mermaidは小さな点）。戻りの遷移の線が複合状態の枠の中を通る（フローチャートの配置の既存の性質） |
| v1（全体） | 失敗: `eng/symbols.py`がARC-003「core: undeclared external symbol memchr」。coreで使ってよいCの標準関数の一覧に`memchr`は無い（リンカの記号で検査）。引用符の終わりを探す所を素の繰り返しに直した。ほかは成功（61/61、分岐90.82%）。新しい`mermaid_state.c`は81.34% |
| 補強 | `[*]`の再利用・後からの宣言（表示名・choice）・素の`state C`・空の端点・同じ複合状態の重複・名前の無い複合状態・`style`/`click`/`accTitle:`/`<<join>>`・入れ子の上限（17段）の試験を足した |
