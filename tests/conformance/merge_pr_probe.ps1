# Run the actual merge command with external commands replaced in this child process only.
param(
    [Parameter(Mandatory)][string]$Tool,
    [Parameter(Mandatory)][string]$Scenario,
    [Parameter(Mandatory)][string]$Calls
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$global:MergeProbeScenario = Get-Content -LiteralPath $Scenario -Raw | ConvertFrom-Json
$global:MergeProbeCalls = $Calls
$global:MergeProbeCount = 0
$global:MergeProbeFrame = 0
$global:MergeProbeMerged = $false
$global:MergeProbeDraft = $global:MergeProbeScenario.draft

function Write-MergeProbeCall {
    param([string]$Program, [string[]]$Arguments)
    $global:MergeProbeCount++
    if ($global:MergeProbeCount -gt 64) { throw 'Unexpected unbounded command loop' }
    [ordered]@{ program = $Program; arguments = @($Arguments) } |
        ConvertTo-Json -Compress -Depth 5 |
        Add-Content -LiteralPath $global:MergeProbeCalls -Encoding utf8
}

function gh {
    param([Parameter(ValueFromRemainingArguments)][string[]]$Arguments)
    Write-MergeProbeCall 'gh' $Arguments
    $global:LASTEXITCODE = 0
    if ($Arguments[0] -eq 'pr' -and $Arguments[1] -eq 'view') {
        $state = if ($global:MergeProbeMerged) { 'MERGED' } else { 'OPEN' }
        [ordered]@{
            body = "目的: fixture`n使った正典経路: fixture`n規則 ID: GIT-004`n振る舞い・スキーマの変更: none`n検証: fixture`nWaivers: none`n残るリスク: fixture`nCloses #123"
            isDraft = $global:MergeProbeDraft
            headRefName = 'fix/123-fixture'
            headRefOid = '1111111111111111111111111111111111111111'
            state = $state
            title = 'fix(tools): 確定した失敗で停止する (#123)'
            mergeable = 'MERGEABLE'
            mergeStateStatus = 'CLEAN'
            mergeCommit = @{ oid = '2222222222222222222222222222222222222222' }
        } | ConvertTo-Json -Compress
        return
    }
    if ($Arguments[0] -eq 'pr' -and $Arguments[1] -eq 'ready') {
        $global:MergeProbeDraft = $Arguments -contains '--undo'
        return 'ready state changed'
    }
    if ($Arguments[0] -eq 'api' -and $Arguments[1] -match '/check-suites\?') {
        $global:LASTEXITCODE = $global:MergeProbeScenario.suiteExit
        if ($LASTEXITCODE -ne 0) { return 'suite read failed' }
        foreach ($suite in $global:MergeProbeScenario.knownSuites) { "$suite" }
        return
    }
    if ($Arguments[0] -eq 'api' -and $Arguments[1] -match '/check-runs\?') {
        $frames = $global:MergeProbeScenario.frames
        $index = [Math]::Min($global:MergeProbeFrame, $frames.Count - 1)
        $global:MergeProbeFrame++
        foreach ($row in $frames[$index].rows) { $row | ConvertTo-Json -Compress }
        return
    }
    if ($Arguments[0] -eq 'pr' -and $Arguments[1] -eq 'merge') {
        $global:MergeProbeMerged = $true
        return 'merged fixture'
    }
    if ($Arguments[0] -eq 'issue' -and $Arguments[1] -eq 'view') { return 'CLOSED' }
    throw "Unexpected gh arguments: $($Arguments -join ' ')"
}

function git {
    param([Parameter(ValueFromRemainingArguments)][string[]]$Arguments)
    Write-MergeProbeCall 'git' $Arguments
    $global:LASTEXITCODE = 0
    if (($Arguments -join ' ') -eq 'rev-parse --abbrev-ref HEAD') { return 'main' }
    if (($Arguments -join ' ') -eq 'pull --ff-only') { return 'updated fixture main' }
    throw "Unexpected git arguments: $($Arguments -join ' ')"
}

function Start-Sleep {
    param([int]$Seconds)
    Write-MergeProbeCall 'sleep' @("$Seconds")
}

& $Tool -Number 99 -Subject 'fix(tools): 確定した失敗で停止する (#123)'
exit $LASTEXITCODE
