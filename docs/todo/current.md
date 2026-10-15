# いまのタスク — NeNe Folio

記録日: 2026-10-15。確認main `e7d5259c286c84bee4c9000ee824d532c8f6efeb`、公開tag `v0.1.0`。
現在のhide指示・Issue/PR・checkoutを優先する。

## 現在の作業

- [Issue #308](https://github.com/hideyukiMORI/nene-folio/issues/308)の外部講評の事実表は[PR #313](https://github.com/hideyukiMORI/nene-folio/pull/313)で統合済み。
  判定・根拠・対応区分・hide判断への推しは[日報](../reports/2026-10-15.md#外部講評の事実確認308)、
  再開条件は[引き継ぎ](../handoffs/2026-10-15.md)。表は22:28時点の確認結果。
- [P0 #309](https://github.com/hideyukiMORI/nene-folio/issues/309)は[終了契約ADR0053](../adr/0053-session-end-and-exit-contract.md)を実装し対象検証済み。
  [品質記録](../quality/2026-10-15-session-end-checks.md)の172確認と短文表示補足を収載し、最終head必須CIで統合する。
  [P1 #310](https://github.com/hideyukiMORI/nene-folio/issues/310)も故障注入で部分本文の誤保存をred再現し、別作業木で対策中。
  P2の1万本/数MiBは計測中。P3は規則全体の強制と部分実装を区別する。[P4サイズ #311](https://github.com/hideyukiMORI/nene-folio/issues/311)・
  [a11y/OS contrast #312](https://github.com/hideyukiMORI/nene-folio/issues/312)は記録済み・未着手。定期draft退避等を実装へ広げない。
- 作業木は`D:/NeNeFolio/worktrees/309-session-end`と`310-stream-failure`、各同名agentsへ証跡を保持。
  #308の当該作業木は整理済み。#309の統合・通常exe反映・整理は実PRと`D:/NeNeFolio/agents/309-session-end/completion.json`で確定する。

## 確定済みと次の条件

- #306は[PR #307](https://github.com/hideyukiMORI/nene-folio/pull/307)でMERGED/CLOSED。
  [必須CI](https://github.com/hideyukiMORI/nene-folio/actions/runs/38740120010)成功、通常exe反映・当該作業木/一時出力整理まで完了。
  `D:/NeNeFolio/agents/306-note-search/completion.json`と恒久証跡を保持。旧枝を再開しない。
- 通常exe更新後のhide実機受理、物理IME/実打鍵、UIA/実読み上げ、別OS/DPI、全テーマは未測定。
  更新時は`tools/prepare-real-machine.ps1`の起動中保護を使う。公開v0.1.0差替え、#243、旧作業木整理は別件。

成功済み対象検証は関係入力が不変なら再利用する（QLT-012 / ADR0044）。分担は今回の指定範囲だけ。Waivers: none。
