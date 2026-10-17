# いまのタスク — NeNe Folio

記録日: 2026-10-17。[#319](https://github.com/hideyukiMORI/nene-folio/issues/319)の基本UI Automation/キーボード到達を実装し、対象検証を完了した。
基点mainはa2f253c、実装枝はfeat/319-basic-accessibility。

## 確認済み

- UIスレッド上のCOM provider、native入力名、ヘッダ/索引/本文操作列、F6/Tab到達。
- applicationの単一cursorを文書選択と分離。SetFocusは保存なし、Select/Invokeは既存保存/未保存確認へ合流。
- 対象build/tidy・状態/3言語文言試験、Cの製品provider試験106項目すべて成功。
- NVDA 2026.2で名前・役割・focus・選択・展開の実発話を取得。寿命境界の独立レビューに追加blocker/highなし。
- FR-006/018、ARC-004、ADR0013/0015/0056を実装契約へ更新。詳細と限界は[品質記録](../quality/2026-10-17-basic-accessibility-checkpoint.md)。

## 統合と反映の確認先

最終PR headの必須CI、main統合、通常exe反映、恒久証跡、専用作業物整理の実結果は
#319の関連PRとD:/NeNeFolio/agents/319-basic-accessibility/completion.jsonを照合する。
途中commit fd1bf23やローカル対象検証の成功だけで統合済みとは扱わない。

通常exeはC:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe。
専用作業木はD:/NeNeFolio/worktrees/319-basic-accessibility、仮出力はD:/NeNeFolio/outputs/319-basic-accessibility。
完了確認後に専用作業物だけを整理し、枝とcommit・恒久証跡を保持する。

次の候補は#320のOS高コントラスト、#321の補助画面公開。性能・配布・UI分割は別件。
#312の設計はPR #322で統合済み。履歴は[日報](../reports/2026-10-17.md)と[引き継ぎ](../handoffs/2026-10-17.md)。
初回CIで露呈したwrong-machine配布試験の解析エラー判定順を修正し、配布検証17項目も成功した。
成功済みで関係入力が不変の検証を再利用する（QLT-012）。保存schema・ゲート判定基準変更なし。Waivers: none。
