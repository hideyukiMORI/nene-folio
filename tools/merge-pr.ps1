# PR の統合手順を 1 本にする（GIT-002 / GIT-003 / GIT-004・Issue #102）。
# 字面検査 → Ready → CI 待ち → mergeable の確認 → squash merge → main の ff → 閉じた Issue の状態を JSON で返す。
# 使うのは git と gh だけで、merge 以外の外部状態（ruleset・枝の保護）は変えない。
# -WhatIf は字面検査・現在の check・mergeable の判定までを行い、ready / merge / pull はしない。
[CmdletBinding()]
param(
    [Parameter(Mandatory)][int]$Number,
    [Parameter(Mandatory)][string]$Subject,
    [switch]$WhatIf
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$utf8Encoding = [Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8Encoding
[Console]::OutputEncoding = $utf8Encoding
$OutputEncoding = $utf8Encoding

# PR 本文に要るフィールド行（.github/PULL_REQUEST_TEMPLATE.md が正本）。
$script:RequiredFieldLines = @(
    '目的:',
    '使った正典経路:',
    '規則 ID:',
    '振る舞い・スキーマの変更:',
    '検証',
    'Waivers:',
    '残るリスク:',
    'Closes #'
)
$script:SubjectPattern = '^(feat|fix|docs|refactor|test|build|ci|chore)(\([a-z0-9-]+\))?!?: .+ \(#([1-9][0-9]*)\)$'
$script:PollSeconds = 30
$script:WaitLimitSeconds = 20 * 60
$script:ResendAfterSeconds = 60

function Stop-Merge {
    param([Parameter(Mandatory)][string]$Message)
    [Console]::Error.WriteLine("merge-pr: $Message")
    exit 1
}

# 欠けたフィールド行の名前を返す。空の配列なら全部ある。
function Test-PullRequestBody {
    param([Parameter(Mandatory)][AllowEmptyString()][string]$Body)
    $missing = [Collections.Generic.List[string]]::new()
    foreach ($field in $script:RequiredFieldLines) {
        $pattern = '(?m)^[ \t]*' + [regex]::Escape($field)
        if ($field -eq 'Closes #') { $pattern += '[1-9][0-9]*' }
        if ($Body -notmatch $pattern) { $missing.Add($field) }
    }
    return , $missing.ToArray()
}

function Get-ClosedIssueNumber {
    param([Parameter(Mandatory)][AllowEmptyString()][string]$Body)
    $numbers = foreach ($match in [regex]::Matches($Body, '(?m)^[ \t]*Closes #([1-9][0-9]*)')) { [int]$match.Groups[1].Value }
    return , @($numbers | Select-Object -Unique)
}

# 件名の問題を返す。空の配列なら GIT-003 の形で、末尾の (#N) が Closes #N の Issue と一致する。
function Test-MergeSubject {
    param([Parameter(Mandatory)][string]$Text, [Parameter(Mandatory)][AllowEmptyCollection()][int[]]$ClosedIssues)
    $problems = [Collections.Generic.List[string]]::new()
    $match = [regex]::Match($Text, $script:SubjectPattern)
    if (-not $match.Success) {
        $problems.Add('subject is not GIT-003 form: <type>(<scope>): <説明> (#N)')
    } elseif ($ClosedIssues -notcontains [int]$match.Groups[3].Value) {
        $problems.Add("subject ends with (#$($match.Groups[3].Value)) but the body closes #$($ClosedIssues -join ', #')")
    }
    if ($Text.Length -gt 100) { $problems.Add("subject is $($Text.Length) characters; GIT-003 allows 100") }
    return , $problems.ToArray()
}

function Invoke-Gh {
    param([Parameter(Mandatory)][string[]]$Arguments)
    $output = & gh @Arguments 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Stop-Merge "gh $($Arguments -join ' ') failed: $($output.Trim())" }
    return $output
}

# gh pr checks は pending や失敗でも非 0 を返し、check が無いと本文の代わりに文言を返す。
function Get-CheckState {
    param([Parameter(Mandatory)][int]$Pr)
    $output = & gh pr checks $Pr --json 'name,bucket' 2>&1 | Out-String
    $text = $output.Trim()
    if (-not $text.StartsWith('[')) {
        if ($text -match 'no checks reported') { return , @() }
        Stop-Merge "gh pr checks $Pr failed: $text"
    }
    return , @($text | ConvertFrom-Json)
}

function Get-CheckSummary {
    param([Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Checks)
    $failed = @($Checks | Where-Object { $_.bucket -in @('fail', 'cancel') } | ForEach-Object { $_.name })
    $pending = @($Checks | Where-Object { $_.bucket -eq 'pending' } | ForEach-Object { $_.name })
    return [ordered]@{
        count = $Checks.Count
        failed = $failed
        pending = $pending
        done = ($Checks.Count -gt 0 -and $pending.Count -eq 0)
    }
}

function Wait-Check {
    param([Parameter(Mandatory)][int]$Pr)
    $started = [Diagnostics.Stopwatch]::StartNew()
    $resent = $false
    while ($true) {
        $summary = Get-CheckSummary -Checks (Get-CheckState -Pr $Pr)
        if ($summary.failed.Count -gt 0) { Stop-Merge "checks failed: $($summary.failed -join ', ')" }
        if ($summary.done) { return $summary }
        $elapsed = $started.Elapsed.TotalSeconds
        if ($summary.count -eq 0 -and -not $resent -and $elapsed -ge $script:ResendAfterSeconds) {
            # ready_for_review で CI が起動しない既知の事象。1 回だけ Ready を送り直す。
            [Console]::Error.WriteLine("merge-pr: no checks after $([int]$elapsed)s; resending ready once")
            Invoke-Gh @('pr', 'ready', "$Pr", '--undo') | Out-Null
            Invoke-Gh @('pr', 'ready', "$Pr") | Out-Null
            $resent = $true
        }
        if ($elapsed -ge $script:WaitLimitSeconds) { Stop-Merge "checks did not finish within $($script:WaitLimitSeconds / 60) minutes" }
        Start-Sleep -Seconds $script:PollSeconds
    }
}

# GitHub は mergeable を遅れて計算するので UNKNOWN の間だけ少し待つ。
function Get-MergeState {
    param([Parameter(Mandatory)][int]$Pr)
    for ($attempt = 1; ; $attempt++) {
        $state = Invoke-Gh @('pr', 'view', "$Pr", '--json', 'mergeable,mergeStateStatus') | ConvertFrom-Json
        if (($state.mergeable -ne 'UNKNOWN' -and $state.mergeStateStatus -ne 'UNKNOWN') -or $attempt -ge 6) { return $state }
        Start-Sleep -Seconds 5
    }
}

function Test-MergeState {
    param([Parameter(Mandatory)][object]$State)
    if ($State.mergeStateStatus -in @('BEHIND', 'DIRTY') -or $State.mergeable -eq 'CONFLICTING') {
        return "sync が要る（main の merge を取り込んで push する）: mergeable=$($State.mergeable) mergeStateStatus=$($State.mergeStateStatus)"
    }
    return ''
}

# (a) 字面検査
$pr = Invoke-Gh @('pr', 'view', "$Number", '--json', 'body,isDraft,headRefName,state,title') | ConvertFrom-Json
if ($pr.state -ne 'OPEN') { Stop-Merge "PR #$Number is $($pr.state), not OPEN" }
$missing = Test-PullRequestBody -Body $pr.body
if ($missing.Count -gt 0) { Stop-Merge "PR #$Number body lacks field lines: $($missing -join ' / ')" }
$closedIssues = Get-ClosedIssueNumber -Body $pr.body
$subjectProblems = Test-MergeSubject -Text $Subject -ClosedIssues $closedIssues
if ($subjectProblems.Count -gt 0) { Stop-Merge ($subjectProblems -join '; ') }

if ($WhatIf) {
    $checks = Get-CheckSummary -Checks (Get-CheckState -Pr $Number)
    $mergeState = Get-MergeState -Pr $Number
    $steps = [Collections.Generic.List[string]]::new()
    if ($pr.isDraft) { $steps.Add("gh pr ready $Number") }
    $steps.Add("wait for checks (every $($script:PollSeconds)s, up to $($script:WaitLimitSeconds / 60) min; resend ready once if none after $($script:ResendAfterSeconds)s)")
    $steps.Add('check mergeable / mergeStateStatus (stop on BEHIND / DIRTY / CONFLICTING)')
    $steps.Add("gh pr merge $Number --squash --subject `"$Subject`" --delete-branch")
    $steps.Add('git pull --ff-only (only when the current checkout is main)')
    $steps.Add("gh issue view <N> --json state for #$($closedIssues -join ', #')")
    [ordered]@{
        whatIf = $true
        pr = $Number
        title = $pr.title
        headRefName = $pr.headRefName
        isDraft = $pr.isDraft
        subject = $Subject
        missingFields = @()
        closedIssues = $closedIssues
        checks = $checks
        mergeable = $mergeState.mergeable
        mergeStateStatus = $mergeState.mergeStateStatus
        syncNeeded = (Test-MergeState -State $mergeState)
        steps = $steps.ToArray()
    } | ConvertTo-Json -Depth 5
    exit 0
}

# (b) Draft なら Ready
if ($pr.isDraft) { Invoke-Gh @('pr', 'ready', "$Number") | Out-Null }

# (c) CI 待ち
Wait-Check -Pr $Number | Out-Null

# (d) mergeable の確認
$syncProblem = Test-MergeState -State (Get-MergeState -Pr $Number)
if ($syncProblem) { Stop-Merge $syncProblem }

# (e) squash merge。--delete-branch のローカル枝の削除だけが失敗しても統合は済んでいるので、状態で判断する。
$mergeOutput = & gh pr merge $Number --squash --subject $Subject --delete-branch 2>&1 | Out-String
$mergeExit = $LASTEXITCODE
$merged = Invoke-Gh @('pr', 'view', "$Number", '--json', 'state,mergeCommit') | ConvertFrom-Json
if ($merged.state -ne 'MERGED') { Stop-Merge "gh pr merge $Number did not merge (exit $mergeExit): $($mergeOutput.Trim())" }
if ($mergeExit -ne 0) { [Console]::Error.WriteLine("merge-pr: merged, but gh reported: $($mergeOutput.Trim())") }

# (f) 現在の checkout が main のときだけ ff
$branch = (& git rev-parse --abbrev-ref HEAD 2>&1 | Out-String).Trim()
$mainSynced = $false
if ($LASTEXITCODE -eq 0 -and $branch -eq 'main') {
    $pullOutput = & git pull --ff-only 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Stop-Merge "merged, but git pull --ff-only failed: $($pullOutput.Trim())" }
    $mainSynced = $true
}

# (g) 閉じた Issue の状態
$issues = foreach ($issue in $closedIssues) {
    $state = (Invoke-Gh @('issue', 'view', "$issue", '--json', 'state', '-q', '.state')).Trim()
    [ordered]@{ number = $issue; state = $state }
}
[ordered]@{
    pr = $Number
    merged = $true
    mergeCommit = $merged.mergeCommit.oid
    mainSynced = $mainSynced
    closedIssues = @($issues)
} | ConvertTo-Json -Depth 5
