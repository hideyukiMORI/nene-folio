[CmdletBinding()]
param()

# Phase 0 の実測（ADR 0001 / ADR 0003）。eng/probes/language.json の各ケースを MSVC と clang-cl で実際にコンパイルし、
# 期待どおりの結果（通る／落ちる）であることを確かめて out/phase0/results.json に残す。
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$probeRoot = Join-Path $repoRoot 'out/phase0'
New-Item -ItemType Directory -Force -Path $probeRoot | Out-Null
$cases = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'probes/language.json') -Raw | ConvertFrom-Json
$results = @()
function Add-Result([hashtable]$entry) { $script:results += [pscustomobject]$entry }
function Relative([string]$text) { return ($text.Trim() -replace [regex]::Escape($probeRoot), '<probe>') }
Write-Host "cl: $((Get-Item (Get-Command cl).Source).VersionInfo.FileVersion)"
Write-Host "clang-cl: $((& clang-cl --version 2>&1 | Select-Object -First 1))"
foreach ($case in $cases) {
    $source = Join-Path $probeRoot ($case.id + '.c')
    Set-Content -LiteralPath $source -Value $case.source -Encoding utf8NoBOM
    if ($case.tool -eq 'msvc') {
        $standard = if ($case.flags -contains '/std:clatest') { @() } else { @('/std:c17') }
        $arguments = @('/nologo') + $standard + @('/W4', '/WX', '/permissive-', '/utf-8', '/c', $source, "/Fo$probeRoot/$($case.id).obj") + @($case.flags)
        $output = (& cl @arguments 2>&1 | Out-String)
    }
    else {
        $arguments = @('/nologo', '/W4', '/WX', '/utf-8', '/c', $source, "/Fo$probeRoot/$($case.id).obj") + @($case.flags)
        $output = (& clang-cl @arguments 2>&1 | Out-String)
    }
    $code = $LASTEXITCODE
    Add-Result @{ id = $case.id; tool = $case.tool; flags = @($case.flags); expectedCompiles = $case.compiles; exitCode = $code; output = (Relative $output) }
    Write-Host ("{0,-30} {1,-7} exit={2} expected={3}" -f $case.id, $case.tool, $code, $case.compiles)
    if (($code -eq 0) -ne $case.compiles) { throw "Unexpected compiler result: $($case.id)`n$output" }
}
# L1: リンカ段の決定性検査。time() は CRT ヘッダで _time64 に写像されるので、名前ではなくシンボルを見る。
$nm = (& llvm-nm --undefined-only (Join-Path $probeRoot 'M5-time.obj') 2>&1 | Out-String)
if ($nm -notmatch '\b_time64\b') { throw 'L1: llvm-nm did not expose _time64' }
Add-Result @{ id = 'L1-llvm-nm-undefined-time'; tool = 'llvm-nm'; exitCode = $LASTEXITCODE; output = (Relative $nm) }
$nm6 = (& llvm-nm --undefined-only (Join-Path $probeRoot 'M6-win32.obj') 2>&1 | Out-String)
if ($nm6 -notmatch '__imp_GetTickCount') { throw 'L2: llvm-nm did not expose __imp_GetTickCount' }
Add-Result @{ id = 'L2-llvm-nm-undefined-win32'; tool = 'llvm-nm'; exitCode = $LASTEXITCODE; output = (Relative $nm6) }
$dumpbin = (& dumpbin /nologo /symbols (Join-Path $probeRoot 'M5-time.obj') 2>&1 | Out-String)
$grep = ($dumpbin -split "`n" | Where-Object { $_ -match 'UNDEF' -and $_ -match '\btime\b' }) -join "`n"
Add-Result @{ id = 'L1-dumpbin-grep-time-misses'; tool = 'dumpbin'; exitCode = 0; matched = ($grep -eq ''); output = 'grep for the source name "time" finds nothing; the undefined symbol is _time64' }
# A1/A2: 検出層。ASan（両コンパイラ）と UBSan（clang-cl）。
$asanSource = Join-Path $probeRoot 'A1-asan.c'
Set-Content -LiteralPath $asanSource -Value "#include <stdlib.h>`nint main(void) { int *p = malloc(4 * sizeof(int)); p[4] = 1; int r = p[4]; free(p); return r == 1 ? 0 : 3; }`n" -Encoding utf8NoBOM
foreach ($compiler in @('cl', 'clang-cl')) {
    $standard = if ($compiler -eq 'cl') { '/std:c17' } else { '/clang:-std=c23' }
    $build = (& $compiler /nologo $standard /W4 /WX /Zi /MT /fsanitize=address $asanSource "/Fo$probeRoot/A1-$compiler.obj" "/Fe$probeRoot/A1-$compiler.exe" 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "A1: $compiler could not build the ASan probe`n$build" }
    $run = (& "$probeRoot/A1-$compiler.exe" 2>&1 | Out-String)
    $runCode = $LASTEXITCODE
    if ($runCode -eq 0 -or $run -notmatch 'AddressSanitizer') { throw "A1: ASan did not fire under $compiler" }
    Add-Result @{ id = "A1-asan-heap-overflow-$compiler"; tool = "$compiler+asan"; exitCode = $runCode; output = (Relative (($build + $run) -split "`n" | Select-Object -First 6 | Out-String)) }
}
$ubsanSource = Join-Path $probeRoot 'A2-ubsan.c'
Set-Content -LiteralPath $ubsanSource -Value "int main(int argc, char **argv) { (void)argv; int x = 2147483647; x += argc; return x < 0 ? 0 : 3; }`n" -Encoding utf8NoBOM
$build = (& clang-cl /nologo /clang:-std=c23 /W4 /WX -fsanitize=undefined -fno-sanitize-recover=all $ubsanSource "/Fo$probeRoot/A2-ubsan.obj" "/Fe$probeRoot/A2-ubsan.exe" 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw "A2: clang-cl could not build the UBSan probe`n$build" }
$run = (& "$probeRoot/A2-ubsan.exe" 2>&1 | Out-String)
if ($LASTEXITCODE -eq 0 -or $run -notmatch 'UndefinedBehaviorSanitizer') { throw 'A2: UBSan did not fire' }
Add-Result @{ id = 'A2-ubsan-signed-overflow'; tool = 'clang-cl+ubsan'; exitCode = $LASTEXITCODE; output = (Relative $run) }
# T1: clang-tidy は C でも認知的複雑度を測る。K5 の null 逆参照は静的解析器が拒否する。
$tidySource = Join-Path $probeRoot 'T1-complexity.c'
$body = "int deep(int a, int b);`nint deep(int a, int b) {`n int r = 0;`n"
foreach ($i in 1..12) { $body += " if (a > $i) { if (b > $i) { r += $i; } else { r -= $i; } }`n" }
$body += " return r;`n}`n"
Set-Content -LiteralPath $tidySource -Value $body -Encoding utf8NoBOM
$tidy = (& clang-tidy $tidySource '--checks=-*,readability-function-cognitive-complexity' '--warnings-as-errors=*' '--config={CheckOptions: {readability-function-cognitive-complexity.Threshold: 10}}' '--' '-std=c23' 2>&1 | Out-String)
if ($LASTEXITCODE -eq 0 -or $tidy -notmatch 'cognitive') { throw 'T1: clang-tidy did not reject the complexity' }
Add-Result @{ id = 'T1-tidy-cognitive-complexity'; tool = 'clang-tidy'; exitCode = $LASTEXITCODE; output = (Relative ($tidy -split "`n" | Select-Object -First 3 | Out-String)) }
$analyzer = (& clang-tidy (Join-Path $probeRoot 'K5-null-deref-compiles-hole.c') '--checks=-*,clang-analyzer-core.NullDereference' '--warnings-as-errors=*' '--' '-std=c23' 2>&1 | Out-String)
if ($LASTEXITCODE -eq 0 -or $analyzer -notmatch 'NullDereference') { throw 'T2: clang-analyzer did not reject the null dereference' }
Add-Result @{ id = 'T2-tidy-analyzer-null-dereference'; tool = 'clang-tidy'; exitCode = $LASTEXITCODE; output = (Relative ($analyzer -split "`n" | Select-Object -First 3 | Out-String)) }
# F1 / V1: 整形の検査と、分岐カバレッジの測定経路が C で動くこと。
$format = (& clang-format --dry-run --Werror "--style=file:$repoRoot/.clang-format" (Join-Path $probeRoot 'M1-w4.c') 2>&1 | Out-String)
if ($LASTEXITCODE -eq 0) { throw 'F1: clang-format accepted an unformatted probe' }
Add-Result @{ id = 'F1-clang-format-rejects'; tool = 'clang-format'; exitCode = $LASTEXITCODE; output = (Relative ($format -split "`n" | Select-Object -First 2 | Out-String)) }
$coverageSource = Join-Path $probeRoot 'V1-cov.c'
Set-Content -LiteralPath $coverageSource -Value "int pick(int a);`nint pick(int a) { if (a > 0) { return 1; } return 0; }`nint main(void) { return pick(1) == 1 ? 0 : 1; }`n" -Encoding utf8NoBOM
$build = (& clang-cl /nologo /clang:-std=c23 /MT -fprofile-instr-generate -fcoverage-mapping $coverageSource "/Fo$probeRoot/V1-cov.obj" "/Fe$probeRoot/V1-cov.exe" 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw "V1: coverage build failed`n$build" }
$env:LLVM_PROFILE_FILE = "$probeRoot/V1.profraw"
& "$probeRoot/V1-cov.exe" | Out-Null
& llvm-profdata merge -sparse "$probeRoot/V1.profraw" -o "$probeRoot/V1.profdata" | Out-Null
$report = (& llvm-cov report "$probeRoot/V1-cov.exe" "-instr-profile=$probeRoot/V1.profdata" 2>&1 | Out-String)
if ($report -notmatch 'TOTAL') { throw 'V1: llvm-cov produced no report' }
Add-Result @{ id = 'V1-llvm-cov-branches'; tool = 'clang-cl+llvm-cov'; exitCode = 0; output = (Relative ($report -split "`n" | Select-Object -Last 3 | Out-String)) }
# R1: /MT の実行ファイルは CRT の DLL を import しない（Phase 4 の成果物検査の根拠）。
$plain = Join-Path $probeRoot 'R1-plain.c'
Set-Content -LiteralPath $plain -Value "int main(void) { return 0; }`n" -Encoding utf8NoBOM
& clang-cl /nologo /clang:-std=c23 /WX /MT $plain "/Fo$probeRoot/R1.obj" "/Fe$probeRoot/R1.exe" | Out-Null
$imports = (& llvm-readobj --coff-imports "$probeRoot/R1.exe" 2>&1 | Out-String)
$names = (($imports -split "`n") | Where-Object { $_ -match 'Name:' } | ForEach-Object { $_.Trim() }) -join ', '
if ($names -match 'VCRUNTIME|UCRTBASE|MSVCP') { throw "R1: static CRT still imports $names" }
Add-Result @{ id = 'R1-static-crt-imports'; tool = 'llvm-readobj'; exitCode = 0; output = $names }
$results | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $probeRoot 'results.json') -Encoding utf8NoBOM
Write-Host "Phase 0 probes matched every expected result ($($results.Count) records)."
