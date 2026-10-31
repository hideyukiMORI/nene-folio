[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$WorkDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [ValidateSet('Zip', 'Msix')][string]$Format = 'Zip'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
Push-Location $repoRoot
try {
    $formatArguments = @('--format', $Format.ToLowerInvariant())
    if ($Format -eq 'Msix') {
        $makeAppx = Join-Path ${env:ProgramFiles(x86)} "Windows Kits/10/bin/$($versions.windowsPackagingSdk)/x64/makeappx.exe"
        $makeAppxVersion = (Get-Item -LiteralPath $makeAppx).VersionInfo.FileVersion.Split(' ')[0]
        if ($makeAppxVersion -ne $versions.makeappx) { throw "QLT-011: MakeAppx must be $($versions.makeappx); actual: $makeAppxVersion" }
        $formatArguments += @('--makeappx', $makeAppx)
    }
    & python eng/release.py destinations --work $WorkDirectory --output $OutputDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Release destinations rejected.' }
    # Create only this task's new directory; existing paths are never deleted.
    New-Item -ItemType Directory -Path $WorkDirectory -ErrorAction Stop | Out-Null
    & python eng/release.py record-inputs --work $WorkDirectory --output $OutputDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Could not identify release inputs.' }
    $releaseBuild = Join-Path $WorkDirectory 'build'
    New-Item -ItemType Directory -Path (Join-Path $releaseBuild '.cmake/api/v1/query') | Out-Null
    New-Item -ItemType File -Path (Join-Path $releaseBuild '.cmake/api/v1/query/codemodel-v2') | Out-Null
    & cmake -S . -B $releaseBuild -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_RC_COMPILER=rc
    if ($LASTEXITCODE -ne 0) { throw 'Release configure failed.' }
    & cmake --build $releaseBuild --target nenefolio
    if ($LASTEXITCODE -ne 0) { throw 'Release compilation or analysis failed.' }
    & python eng/conformance.py --build-dir $releaseBuild
    if ($LASTEXITCODE -ne 0) { throw 'Release conformance or build graph failed.' }
    & python eng/symbols.py --build-dir $releaseBuild --require core application
    if ($LASTEXITCODE -ne 0) { throw 'Release undefined symbols failed.' }
    & python eng/release.py assemble --work $WorkDirectory --output $OutputDirectory @formatArguments
    if ($LASTEXITCODE -ne 0) { throw 'Release artifact validation failed.' }
}
finally { Pop-Location }
