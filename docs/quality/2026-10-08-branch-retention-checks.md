# #200 — 統合後のブランチ保持

hide の 2026-10-04 指定「ブランチと commit は保持」を統合の正典へ反映した。規則: GIT-002 / GIT-004。Waivers: none。

- `tools/merge-pr.ps1` の実行と `-WhatIf` 表示から `--delete-branch` を除いた。
- #198 の統合時に削除された remote `feat/195-trash-adapter` は、保持された local の `58983fd` から復元した。
  `git ls-remote --heads origin feat/195-trash-adapter` で同じ SHA を確認した。
- GitHub repository の `delete_branch_on_merge` が `true` だったため、#200 の範囲で `false` に変更した。
  `gh api --method PATCH repos/hideyukiMORI/nene-folio -F delete_branch_on_merge=false` と、その後の GET の両方が `false` を返した。

検証:

1. PowerShell Parser: 構文エラー 0。
2. `pwsh -NoProfile -File tools/merge-pr.ps1 -Number 199 -Subject 'feat(state): ごみ箱移動後の文書と索引を揃える (#196)' -WhatIf`:
   exit 0。既存 Draft PR の読み取りだけを行い、出力された squash merge コマンドに枝の削除が無い。
3. `pwsh -NoProfile -File D:/NeNeFolio/agents/branch-retention/smoke.ps1 -Script D:/NeNeFolio/worktrees/200-keep-branches/tools/merge-pr.ps1`:
   exit 0。外部の `gh` / `git` を process 内の偽物に置き換え、Ready → check → strict 判定 → squash → main ff → Issue close 確認まで通した。
   実行された merge の引数に `--delete-branch` が無いことと `--squash` があることを確認した。
   外部の PR・リポジトリはこの smoke では変更していない。呼出記録は同じ場所の `smoke-calls.json`。

製品コードの変更は無く、local ではツールの対象確認に限定した。CI の必須 check は通常の PR 経路で確認する。
