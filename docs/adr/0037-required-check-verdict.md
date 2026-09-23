# ADR 0037 — 必須 check は判定 job にし、フルゲートが成功していない head（Draft の skipped・cancelled）を failure で止める

- 状態: 受理（設計リナ 2026-09-23。hide の判断: 候補 (b) を採用。実測は #112 の PR 自身で行い、[確認記録](../quality/2026-09-23-required-check-checks.md)に写す）
- 日付: 2026-09-23
- Issue: #112
- 影響する規則: QLT-012（planned → active）、GIT-004（Draft の表示の補足）、QLT-010（ゲートを強める方向の変更）、ADR 0001（未実測を active と書かない）
- 関連: ADR 0034（統合は `tools/merge-pr.ps1` の 1 本）

## 文脈

`.github/workflows/check.yml` は `pull_request` の `ready_for_review` / `synchronize` / `edited` / `reopened` で起動し、
job `check` は `if: github.event.pull_request.draft == false` で **Draft の間は skipped** になる（QLT-012「draft の間にフルゲートを回さない」）。
ruleset `main` の必須 check は context `check` の 1 本で `strict` が有効である。

2026-09-23 の #103 / PR #111 で次を実測した。Draft の push（skipped）→ `gh pr ready`（run 35769439691）→ その run が後続の event の
`cancel-in-progress` で cancelled → head `3e74f59` に残った check は skipped だけ → **GitHub は `mergeStateStatus = CLEAN` と報告した**。
GitHub は必須 check の最新の結論が `skipped` なら「満たした」と数える。つまりフルゲートが 1 度も走っていない head を squash merge できた
（設計席が気づいて止めた）。`tools/merge-pr.ps1` は #102 の差し戻し 2 で skipped を完了と数えなくなったが、それは script 側の防波堤で、
GitHub 側の穴（`gh pr merge` や Web の釦）は残っている。

## 決定

**必須 check を「判定 job」にし、フルゲートの job が `success` で終わった head だけを通す。**

1. workflow の job を 2 つにする。
   - `full-gate`: 従来の `check` の中身そのまま（`if: draft == false`・windows-2022・`eng/check.ps1`）。Draft の push では skipped のまま。
   - `check`: `needs: [full-gate]`・`if: always()`。`needs.full-gate.result` が `success` 以外なら `exit 1` で failure にする。
     checkout 無し・ubuntu-24.04・timeout 2 分。
2. ruleset の必須 check の名前 `check` は変えない。判定 job がその名を継ぐので、ruleset に触らない。
3. **Draft の head は必須 check が failure になる。** これは意図した表示で、Draft は「まだ統合しない」の宣言である。
   Ready にすれば `full-gate` が走り、成功すれば `check` も success になる。
4. `concurrency.cancel-in-progress` は残す。cancel された run は `full-gate` が cancelled になり、判定は success にならない
   （`always()` の job が cancel 後に走って failure になるか、run ごと cancelled になるかのどちらでも「満たした」にはならない）。
5. QLT-012 の「draft の間にフルゲートを回さない」は変えない。Draft の push で増えるのは数秒の判定 job だけである。

## 強制

- ruleset `main` の `required_status_checks`（context `check`・strict）と、この workflow の 2 job。
- CI の YAML は `eng/prove-gates.py` の対象外（GitHub 側の挙動は手元で再現できない）。反例は **Draft の push で `check` が failure・
  `mergeStateStatus` が CLEAN でないこと**を 1 度実測して[確認記録](../quality/2026-09-23-required-check-checks.md)に残す（#112 の受け入れ条件）。
- QLT-012 の機械強制を **active** にする（実測の記録が根拠。ADR 0001）。

## 結果

- 得るもの: skipped だけの head・cancel された head は `BLOCKED` になり、`gh pr merge` も Web の釦も通らない。
  防波堤が script（`merge-pr.ps1`）から GitHub 側へ移る。
- 失うもの: Draft PR に赤い印が付き、通知が出る。run ごとに判定 job が 1 つ増える（数秒・Linux ランナー）。
- 前提: GitHub が `needs` の結果 `skipped` / `cancelled` を `always()` の job へ渡すこと（実測で確かめる）。

## 却下した選択肢

| 選択肢 | 却下した理由 |
| --- | --- |
| (a) job の `if` を外して Draft でもフルゲートを回す | Draft の push ごとに約 10 分の Windows ランナーが走る。QLT-012 の「draft の間に回さない」を捨てる |
| (c) ruleset で skipped を失敗と見なす | `required_status_checks` のパラメータは `context` / `integration_id` / `strict_required_status_checks_policy` だけで、そういう設定が無い（API の応答で確認） |
| (d) Draft の間は workflow を起動しない | `on:` に draft の条件は書けず、job の `if` は skipped の結論を作る。結論が無い head は `EXPECTED` で止まるが、それを作る手段が無い |
| (e) `tools/merge-pr.ps1` の防波堤だけに頼る | script を経ない経路（`gh pr merge`・Web）が残る。#112 の発端そのもの |
| 判定 job を windows-2022 で走らせる | checkout も toolchain も要らない 1 step に Windows ランナーは無駄 |

## 検証

1. この PR の Draft の push: `full-gate` = skipped・`check` = failure・`gh pr view --json mergeStateStatus` が `CLEAN` でない。
2. `gh pr ready`: `full-gate` = success・`check` = success・`mergeStateStatus` = `CLEAN`。
3. 結果は [確認記録](../quality/2026-09-23-required-check-checks.md)に run の番号と結論の字面で写す。

## 移行

無し。`pull_request` の run は PR の merge ref の workflow を使うので、統合後に開く PR は次の event から新しい 2 job で走る。
統合時点で開いている PR は無い。
