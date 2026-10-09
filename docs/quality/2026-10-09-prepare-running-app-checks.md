# 実機準備で起動中アプリを守る検証（#217）

- 日付: 2026-10-09
- 規則: GIT-001 / GIT-003 / GIT-004、ADR 0034 決定 2
- 対象: `tools/prepare-real-machine.ps1`
- Waivers: none

## 変更と境界

準備対象の `build/NeNeFolio.exe` が起動中なら、保存して閉じる必要を示して非 0 で終了する。実行パスを取得できない同名プロセスがある場合も、準備の副作用より前に止める。対象パスは絶対パス化して大文字・小文字を区別せず比較する。別 checkout の同名アプリには触れず、対象の準備を続ける。

`Stop-Process` / `Wait-Process` を除去した。製品のソース、テスト、CMake、永続化スキーマ、ゲートに変更はない。停止済みの通常経路は git status → pull → toolchain → configure → build → SHA と exe 情報の JSON という既存順序を維持する。

## 対象試験

実ユーザーのプロセスを起動・終了せず、別 PowerShell プロセスで `Get-Process` / git / cmake / toolchain の境界を制御した。`Stop-Process` / `Wait-Process` は呼ばれれば失敗する番兵に置き換えた。fixture は `D:/NeNeFolio/outputs/217-check/`、試験スクリプト・ログ・JSON は `D:/NeNeFolio/agents/217-preserve-running-app/` に保存した。

```powershell
python D:/NeNeFolio/agents/217-preserve-running-app/check_prepare.py D:/NeNeFolio/agents/217-preserve-running-app/prepare-before.ps1 before
python D:/NeNeFolio/agents/217-preserve-running-app/check_prepare.py D:/NeNeFolio/worktrees/217-preserve-running-app/tools/prepare-real-machine.ps1 after
```

| 条件 | 修正前 | 修正後 |
|---|---|---|
| 対象 exe が起動中 | 強制終了の番兵に到達 | 理由付き拒否、準備操作なし |
| 対象 exe のパス表記が大文字 | 強制終了の番兵に到達 | 同じ対象と判定して拒否 |
| 同名プロセスのパスが null | 強制終了の番兵に到達 | 確認不能として拒否 |
| パス取得が例外 | 強制終了の番兵に到達 | 確認不能として拒否 |
| 別 checkout の同名 exe | 強制終了の番兵に到達 | 無干渉で準備成功、JSON 契約を維持 |
| 同名プロセスなし | 準備成功 | 準備成功、JSON 契約を維持 |

修正前は受入れ条件 1/6、修正後は 6/6。拒否時の stdout は空で、git / configure / build に到達しない。全ケースで fixture の既存 exe の SHA-256 は変わらなかった。修正後ツールの SHA-256 は `0c7c53464b424524f23b0688321995b5faabbdc4d415b3c02beebc63036e3fe7`。

初回の試験記録処理は空配列を JSON として保存できず停止したため、`ConvertTo-Json -InputObject` へ修正してから上記の前後比較を実行した。製品側の成功とは数えていない。

## 検証の範囲

ツール変更だけなので、製品コードが同一の `4330745` で成功済みの正典ゲートを再利用する。ローカルではこの対象試験と差分・リンク規約検査に限定し、統合は既存の必須 CI と正典 `tools/merge-pr.ps1` を通す。ゲートの変更・除外・迂回はない。

`git diff --check` は exit 0、`python eng/conformance.py` は 0 violation、`git diff --exit-code 4330745 -- src tests CMakeLists.txt` は exit 0（差分なし）。

起動確認後に別プロセスが対象 exe を起動する競合は抑止していない。その場合は既存の build 失敗として報告され、アプリを終了する操作はない。実プロセスの終了試験や利用者の本文を使った試験は行っていない。
