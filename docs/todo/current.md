# いまのタスク — NeNe Folio

記録日: 2026-10-18。hideの「残ってる対応を進めて」に基づく#321の実装・対象検証が完了。
基点mainは8a50e4c、枝feat/321-accessible-surfaces。最終head必須CI・main/通常exe反映・整理を確定する段階。
後から読む際は関連PR/Issueの実状態と`D:/NeNeFolio/agents/321-accessible-surfaces/completion.json`を先に照合する。

設定/テーマ・コマンド/ヘルプ/履歴・検索/置換・通知は同じUIA hostと既存操作へ接続した。
native226+3+18項目、対象unit、実NVDAの発話を確認済み。[品質記録](../quality/2026-10-18-accessible-surfaces-checkpoint.md)とADR0058を参照。
schema・外部runtime・ゲート変更なし。Waivers: none。公開Releaseは対象外。

旧42作業木は証跡収載・hash照合後に整理済み。枝とcommitは保持。
残留は#319のChrome参照中DLL2個、#320の自動承認審査で削除を拒否された仮出力。
最終の稼働参照/整理結果と再開条件は[引き継ぎ](../handoffs/2026-10-18.md)とcompletion.jsonを参照する。
