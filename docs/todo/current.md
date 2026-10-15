# いまのタスク — NeNe Folio

記録日: 2026-10-15。製品main `7d65ef16924d33e2581aef8f8d9a3c9f93cbd1b0`、公開tag `v0.1.0`。
hideの「区切りのいいところで止めて日報と引き継ぎ書を書いて」に従い、記録を完了して停止する。
再開は次のhide指示・実Issue/PR・checkoutを照合して判断する。過去の分担や許可を自動継承しない。

## 完了した範囲

- #306検索UIはPR #307、#308事実表はPR #313で統合済み。事実表は[日報](../reports/2026-10-15.md#外部講評の事実確認308)。
- P0 #309は[PR #314](https://github.com/hideyukiMORI/nene-folio/pull/314)で統合・必須CI成功・通常exe反映・専用worktree/outputs整理まで完了。
  [終了契約ADR0053](../adr/0053-session-end-and-exit-contract.md)、[対象検証](../quality/2026-10-15-session-end-checks.md)、
  `D:/NeNeFolio/agents/309-session-end/completion.json`を照合する。
- P2は[1万本/数MiBの計測](../quality/2026-10-15-external-review-performance.md)完了。性能修正は未実装。
- #315でCNF-002/005/007の限定検査をactiveへ補正、001/003/004と親規則は維持。
  文書の最終統合・整理は#315のPRと`D:/NeNeFolio/agents/315-gate-status/completion.json`で確定する。

## 保留中と再開条件

- P1 [#310](https://github.com/hideyukiMORI/nene-folio/issues/310)はred再現・候補実装・対象build/unit/conformanceまで。
  `fix/310-stream-failure`、commit `01d40fedd16d7dee03de67f46d6072fce844e1a7`を保存。main未統合、PR未作成、native green未実行。
  作業木`D:/NeNeFolio/worktrees/310-stream-failure`とoutputs同名を未完了のため保持。
  P0取込・競合解決、入力guard/notice統合と再試行文言を確認し、関係入力を凍結してnative検証する。
  詳細は[引き継ぎ](../handoffs/2026-10-15.md)。候補を完了扱いにしない。
- P4 [サイズ #311](https://github.com/hideyukiMORI/nene-folio/issues/311)、[a11y/OS contrast #312](https://github.com/hideyukiMORI/nene-folio/issues/312)はIssueのみ。
- portable/installer、アクセシビリティの範囲、巨大UI分割、定期draft退避、起動temp回収はhide判断。
  推しと理由は日報の判断表。公開v0.1.0差替え、#243、旧作業木整理は別件。

通常exeは`C:/Users/info/WORKS/NeNeFolio/build/NeNeFolio.exe`。更新後起動・hide実機受理は未実施。
実ログオフ/物理IME/電断/別OS/DPIは未測定。成功済検証は入力不変なら再利用（QLT-012）。Waivers: none。
