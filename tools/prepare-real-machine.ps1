# 実機で見る checkout を準備する（#101）。起動中の NeNeFolio.exe を閉じ、clean を確かめ、
# pull --ff-only で進め、eng/toolchain.ps1 と同じ呼び出しで nenefolio を名指しでビルドし、
# ビルドした SHA と exe の情報を標準出力に JSON 1 つで返す。起動はしない（起動は別の 1 行）。
# ツールの出力はすべて標準エラーへ流し、標準出力は JSON だけにする。
[CmdletBinding()]
param([string]$RepoRoot = (Split-Path -Parent $PSScriptRoot))

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# toolchain.ps1 は $repoRoot を自分の位置から代入する（変数名は大小を区別しない）ので、先に別名へ固定する。
$target = (Resolve-Path -LiteralPath $RepoRoot).Path

function Invoke-Tool {
    param([string]$Name, [string[]]$Arguments, [string]$Failure)
    & $Name @Arguments 2>&1 | ForEach-Object { [Console]::Error.WriteLine("$_") }
    if ($LASTEXITCODE -ne 0) { throw $Failure }
}

# (a) 開いたままだと exe のリンクが落ちる。名前だけで探し、ほかのプロセスは触らない。
$running = @(Get-Process -Name 'NeNeFolio' -ErrorAction SilentlyContinue)
if ($running.Count -gt 0) {
    $running | Stop-Process -Force
    $running | Wait-Process -Timeout 30 -ErrorAction SilentlyContinue
}

# (b) clean でない checkout は進めない。
$status = @(& git -C $target status --porcelain)
if ($LASTEXITCODE -ne 0) { throw 'git status failed.' }
if ($status.Count -gt 0) { throw "The checkout is not clean: $target" }

# (c) 早送りだけで進める。
Invoke-Tool -Name 'git' -Arguments @('-C', $target, 'pull', '--ff-only') -Failure 'git pull --ff-only failed.'

# (d) 固定した toolchain で nenefolio を名指しでビルドする（名指ししないと exe が再リンクされない）。
. (Join-Path $target 'eng/toolchain.ps1')
Push-Location $target
try {
    Invoke-Tool -Name 'cmake' -Arguments @('-S', '.', '-B', 'build', '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug') -Failure 'CMake configure failed.'
    Invoke-Tool -Name 'cmake' -Arguments @('--build', 'build', '--target', 'nenefolio') -Failure 'Build of nenefolio failed.'
}
finally { Pop-Location }

# (e) ビルドした SHA と exe の情報を返す。
$sha = (& git -C $target rev-parse HEAD)
if ($LASTEXITCODE -ne 0) { throw 'git rev-parse failed.' }
$subject = (& git -C $target log -1 --format=%s)
if ($LASTEXITCODE -ne 0) { throw 'git log failed.' }
$exe = Join-Path $target 'build/NeNeFolio.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "The executable was not built: $exe" }
$item = Get-Item -LiteralPath $exe
[ordered]@{
    sha = "$sha".Trim()
    subject = "$subject".Trim()
    exe = $item.FullName
    bytes = $item.Length
    modified = $item.LastWriteTime.ToString('o')
} | ConvertTo-Json -Compress
