# MermaidのER図の品質記録 — #882

2026-11-17。開始main `9159c79`（#878統合後）。先行ADR0105。
作業木E:/NeNeFolio/worktrees/882-er-diagram（branch feat/882-er-diagram）、対象実行の原本E:/NeNeFolio/outputs/882-er-diagram、画像D:/NeNeFolio/agents/er-diagram-20261117。
この記録は対象検証の事実だけを残す。正典の`eng/check.ps1`全体と統合は最終headの必須CIとPRが正本。

## 試行

| 試行 | 結果 |
| --- | --- |
| build-01 | 成功（clang-tidy込み、最初の構築で警告なし） |
| unit-01 | 失敗（試験）: 属性が多すぎる実体の上限の試験で、試験の原文が小さく、ラベルの上限（2048バイト）に届いていなかった。原文の大きさを上限の2倍にした |
| unit-02 | 成功（`folio_tests.exe`全体）。模型・多重度・線の種類・属性・段の追加・向き・20通りの拒否 |
| 描画 | `diagram_render_tests`にER図を加え548 checks成功。`visual/er-{light,dark}.png` |
| conformance | CNF-002（1ファイル1型定義）: 多重度の表に小さな構造体を使っていた。文字列の表3つへ直した |
| 見た目 | 実体・属性の行・線のラベルの多重度・破線を明暗で確かめた。気づいた点（今回は直さない）: 属性が名前と同じ中央揃えで、名前との区切りが無い（ADR0105の却下した案「属性の表」）。同じ実体から出る2本の線のラベルが近い（フローチャートの配置の既存の性質） |
