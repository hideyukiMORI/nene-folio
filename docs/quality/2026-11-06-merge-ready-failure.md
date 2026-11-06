# Ready後の確定CI失敗は再送しない — 2026-11-06（#769）

GIT-004、QLT-008/010/012/013。[ADR0037](../adr/0037-required-check-verdict.md) /
[ADR0044](../adr/0044-tests-at-completion-and-integration.md) / [#163の意図](https://github.com/hideyukiMORI/nene-folio/issues/163)。
追加waiver none、既存WVR-0001継続。

## 問題と正典経路

#767のmerge01はDraftからReadyを送った後、必須CI42206482231の本物のexport_native失敗に対し、
同じheadをDraft→Readyに戻して42207000260を実行した。後者も同じ比較で失敗した。
stderrは`failed check(s) full-gate, check with nothing pending; resending ready once`。
元の#163はReady直後に古いDraft failureしか見えない窓を、ReadyAt以前のcheck除外で解決している。
Wait-Checkの`$resent`だけの判定が、既にReadyAtを持つ今回の確定失敗にも再送を許していた。

正典tools/merge-pr.ps1の同じ分岐へ`$null -ne $script:ReadyAt`を加える。
ただし時刻だけでは10秒の時計許容内にある直前Draft結果を区別できないため、Ready/Undo前に同headの既存check-suite IDを全page取得する。
既存suiteは成功/失敗/queuedを問わず判定から外し、未知suiteには従来のReadyAt除外を適用する。取得失敗ならReady/Undoを送らず停止する。
今回Readyを送った後のfailure/cancelledは非zeroで止まり、undo/二度目のReady/merge/main更新へ進まない。
もともとReadyで由来が不明なfailureの一回再同期、Ready以前の結果除外、pending待機、未起動時の既存再送は維持する。
新たな統合経路やretry policyは作らない。失敗を成功へ変更する処理も追加しない。

## 機械確認

tests/conformance/test_merge_pr.pyが実tools/merge-pr.ps1を別PowerShellプロセスで実行する。
同process内だけgh/git/Start-Sleepを差し替え、CLI呼出列・raw stdout/stderr・終了値をfixtureごとに保存する。
PowerShellの実分岐/JSON解析/Ready時刻による除外/merge結果処理はそのまま通る。製品への試験口、外部通信、実git変更は無い。
既存eng/test-conformance.pyのdiscoverがこの8テスト（9子process）を必須CIへ収載する。新しいrunner/依存/除外は追加しない。

| ケース | 独立に要求する振る舞い |
| --- | --- |
| 新Ready後failure | 非zero、Ready1回、undo/再送/merge/git/Issue状態読取なし |
| 新Ready後cancelled | 同じく非zeroで即停止、追加待機もなし |
| 既存Readyの由来不明failure | undo→Readyを一回だけ、2回目の確認で停止、merge/gitなし |
| 古いDraft failure→新pending→success | 古いfailureを除外し、Ready1回、squash1回、mainff1回、Issue CLOSED確認 |
| 既存Ready success | Ready0回、squash1回、mainff1回、Issue CLOSED確認 |
| 時計許容内の既存Draft failure→新pending→success | 時刻で新しいと見える旧suiteも除外し、Ready1回だけで正典統合 |
| 既存success→新pending→failure | 古い成功で統合せず、新しい失敗で停止、merge/gitなし |
| suite取得失敗（Draft/既存Ready） | 非zero、Ready/Undo/merge/gitなし |

fixtureのstarted_atは固定2000/9999を使い、旧/新の意味だけを表す。sleepを実時間で待たず、64呼出し/20秒の有限上限を持つ。
このfixtureでは60秒未起動再送や20分待機期限そのもの、認証/ネットワーク、GitHub実サービスの更新遅延を再測定していない。
同fixtureのCLI字面が実サービス全体の互換性を保証するとは扱わない。

## 全試行

| 試行 | 結果 |
| --- | --- |
| red-01 | 実旧toolは余計なundo/Readyと停止を記録したが、試験ラッパーが入れ子scriptのexit1を親processへ引き継がず0で返した。終了値assertで失敗。outer child/wrapper1、記録errorなし、入力不変 |
| red-02 | ラッパー末尾でLASTEXITCODEを引き継ぐ。旧toolの終了1は確認でき、期待Ready1回に対するundo/二度目のReadyの実呼出列差で失敗。child/wrapper1、記録errorなし、入力不変 |
| test-01 | 停止条件修正後、5ケースが3.848秒で成功。child/wrapper0、記録errorなし、入力不変 |
| verify-01 | 初期候補の構文/conformance0/diff/7文書162参照成功。child/wrapper0、記録errorなし、入力不変 |
| 初期CI42212795356 | head81b2f22のfull-gate/check成功。ただし追加反例を発見したため統合せず、最終候補の証拠には流用しない |
| clock-base-01 | 実UTCで直前Draft時刻を生成。旧toolは881.7007ms内に一回再同期して成功。child/wrapper0、入力不変 |
| clock-fixed-01 | 初期停止条件だけの候補は658.5317ms内に旧Draft failureで誤停止。child/wrapper1、入力不変。10秒許容内での反例を確認 |
| test-02 | suite識別追加後、8テスト/9子processが5.856秒で成功。child/wrapper0、記録errorなし、入力不変 |
| clock-final-01 | 同じ実UTC境界で813.5453ms内にReady1回だけで成功。child/wrapper0、記録errorなし、入力不変 |

redは`python -B tests/conformance/test_merge_pr.py MergePrTests.test_ready_failure_does_not_restart_ci -v`、
完成候補は`python -B -m unittest discover -s tests/conformance -p test_merge_pr.py -v`。
実argv/source/fixture/runner snapshotとraw結果はtools/record-run.pyで収載する。
NENEFOLIO_MERGE_TEST_ROOTをD:/NeNeFolio/outputs/769-merge-ready-failure内のケース別directoryへ向け、既存試行を上書きしない。
既定のCI出力もrepo内out/merge-pr-testsとし、system Tempへ保存しない。

Python/PowerShell構文、関係conformance、git diff --check、文書の相対参照と最終3sourceの成功入力一致を確認する。
製品runtime/通常build/ローカルfullgateは反復しない。最終PR headの必須CIは従来どおり要求する。

## 差分・限界・完了

toolsの実変更は既存suite取得/除外と同じ失敗分岐の停止条件。workflow/ruleset/必須check/重大度/閾値/除外/待機時間は不変。
製品src/API/schema/依存/CMake/版/通常exeは変更しない。単体自己レビュー。独立レビュー追加なし。
GitHub連携のIssue作成は403だったため既存認証CLIで#769を起票した。試験自体は実サービスを操作しない。
初期merge01は追加反例の検証中、自身のPID/argvを照合して停止した（child/wrapper4294967295）。PR770はDraftへ戻した。
それ以前のrecord-run起動1回は親directory未作成で子起動/run-directory作成前に拒否され、修正後のmerge01と分離保存した。
実GitHub読取でDraftのfull-gate/checkが同じsuiteを持ち、Ready後のrunは別suiteになることを確認した。
APIはGitHubの[List check suites for a Git reference](https://docs.github.com/en/rest/checks/suites#list-check-suites-for-a-git-reference)を使う。
snapshot取得後からReadyまでの間に初めて現れる旧suite、並行head更新、未収載check-runの100件を超える既存取得境界までは解決しない。
fixtureや今回の実サービス読取を、一般の並行操作・遅延全体の保証へ拡張しない。
個別保留466/634/530と旧削除拒否領域を維持する。最終CI/統合/通常exe不変/全保全整理は完了後の
D:/NeNeFolio/agents/769-merge-ready-failure/integration-completion.jsonへ確定する。

## 前作業#767の確定

#767 / PR768は最終head`c1cedc2db5487ee781575b1fcbdd920122f6f69c`、必須CI42209596693のfull-gate/check成功。
CIのexport_nativeは2.01秒、46/46成功。main`e81c15a37450b9070fb3343fcbae828af3f08678`へ正典merge02で統合、Issue767 CLOSED。
通常exe hash/bytes/更新日時不変、製品src/eng/fonts/packaging不変、通常build/app起動なし。
8864filesを全hash保全し、絶対2roots/同squashtree/clean・ignored・untracked/link/稼働参照0を確認して専用worktree/output整理、branch/commit保持。
保全ZIP381481479bytes/SHA256 `3e8005f8bd30b19e5a2085dab61fe456768419a3de0158a9fc6b42d5485d8e7a`。
原本D:/NeNeFolio/agents/767-export-native/integration-completion.json。
初回2CIと診断CIの期待path不存在、全補助失敗、最終入力修正を区別して保存した。Windows内部の特定原因や全OSへの保証はしない。
