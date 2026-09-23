# #112（必須 check の判定 job）の確認記録

2026-09-23・枝 `ci/112-required-check-verdict`。ADR 0037 の「検証」に対する結果。
CI の YAML は `eng/prove-gates.py` の対象外なので、反例（Draft の head が merge できないこと）はこの PR 自身で実測して写す。
値は `gh run view` / `gh pr view --json mergeStateStatus,statusCheckRollup` の字面。

## 1. Draft の push（フルゲートを回さない head）

| 項目 | 期待 | 結果 |
| --- | --- | --- |
| `full-gate` の結論 | `skipped` | `SKIPPED`（PR #146・head `fb2725e`・run 35843768164・event `pull_request` / synchronize） |
| `check` の結論 | `failure` | `FAILURE`（同 run。`full-gate result: skipped (draft: true)` を出して `exit 1`） |
| `mergeStateStatus` | `CLEAN` でない（`BLOCKED`） | `BLOCKED`（`gh pr view 146 --json mergeStateStatus`・`isDraft = true`） |

## 2. Ready にした後（フルゲートが走る head）

| 項目 | 期待 | 結果 |
| --- | --- | --- |
| `full-gate` の結論 | `success` | `success`（同じ head `3143151`・`gh pr ready 146` → run 35844073589・event `pull_request` / ready_for_review） |
| `check` の結論 | `success` | `success`（同 run） |
| `mergeStateStatus` | `CLEAN` | `CLEAN`（`isDraft = false`） |

Draft の実測は 2 度の push（head `fb2725e` = run 35843768164・head `3143151` = run 35843898385）で同じ結論だった。
同じ head `3143151` が Draft では `BLOCKED`、Ready でフルゲートが通ると `CLEAN` になったので、通す・止めるの差はフルゲートの結論だけである。

## 3. 測っていないこと

- Ready の run を手で cancel したときの `check` の結論（`always()` の job が cancel 後に走って failure になるか、run ごと cancelled になるか）。
  どちらでも `success` にはならないので、この単位では測っていない。
