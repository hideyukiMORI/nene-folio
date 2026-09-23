# #112（必須 check の判定 job）の確認記録

2026-09-26・枝 `ci/112-required-check-verdict`。ADR 0037 の「検証」に対する結果。
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
| `full-gate` の結論 | `success` | （実測待ち） |
| `check` の結論 | `success` | （実測待ち） |
| `mergeStateStatus` | `CLEAN` | （実測待ち） |

## 3. 測っていないこと

- Ready の run を手で cancel したときの `check` の結論（`always()` の job が cancel 後に走って failure になるか、run ごと cancelled になるか）。
  どちらでも `success` にはならないので、この単位では測っていない。
