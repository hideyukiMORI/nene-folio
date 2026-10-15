# いまのタスク — NeNe Folio

記録日: 2026-10-15。確認main `55f08641a8940cbad98b2dd2c3e677946d717908`、公開tag `v0.1.0`。
現在のhide指示・Issue/PR・checkoutを優先する。

## 現在の作業

- [Issue #308](https://github.com/hideyukiMORI/nene-folio/issues/308)で外部講評の事実表を記録する。
  判定・根拠・対応区分・hide判断への推しは[日報](../reports/2026-10-15.md#外部講評の事実確認308)、
  再開条件は[引き継ぎ](../handoffs/2026-10-15.md)。表は22:28時点の確認結果。
- [P0 #309](https://github.com/hideyukiMORI/nene-folio/issues/309)は実Win32でsession通知注入redを確認。
  別作業木`D:/NeNeFolio/worktrees/309-session-end`で終了契約を先に書き、実装へ進む。green・実ログオフは未実施。
  [P1 #310](https://github.com/hideyukiMORI/nene-folio/issues/310)は静読のみで故障注入未実施。個別修正は別PRへ分ける。
  P2は現在の1万本/数MiBを未計測。P3の状態は変更せず、[P4サイズ #311](https://github.com/hideyukiMORI/nene-folio/issues/311)・
  [a11y/OS contrast #312](https://github.com/hideyukiMORI/nene-folio/issues/312)は記録済み・未着手。定期draft退避等を実装へ広げない。
- 作業木は`D:/NeNeFolio/worktrees/308-external-review-facts`、証跡は`D:/NeNeFolio/agents/external-review-20261015/`。
  文書3件の差分・conformanceを確認し、統合は最終headの必須CI成功後。結果は実PR/証跡で確定する。

## 確定済みと次の条件

- #306は[PR #307](https://github.com/hideyukiMORI/nene-folio/pull/307)でMERGED/CLOSED。
  [必須CI](https://github.com/hideyukiMORI/nene-folio/actions/runs/38740120010)成功、通常exe反映・当該作業木/一時出力整理まで完了。
  `D:/NeNeFolio/agents/306-note-search/completion.json`と恒久証跡を保持。旧枝を再開しない。
- 通常exe更新後のhide実機受理、物理IME/実打鍵、UIA/実読み上げ、別OS/DPI、全テーマは未測定。
  更新時は`tools/prepare-real-machine.ps1`の起動中保護を使う。公開v0.1.0差替え、#243、旧作業木整理は別件。

成功済み対象検証は関係入力が不変なら再利用する（QLT-012 / ADR0044）。分担は今回の指定範囲だけ。Waivers: none。
