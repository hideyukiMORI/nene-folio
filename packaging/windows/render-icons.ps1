# One SVG master -> native ICO and qualified MSIX PNGs (ADR0088).
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$Chrome
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$spec = Get-Content -LiteralPath (Join-Path $repo 'eng/windows-icons.json') -Raw | ConvertFrom-Json
if (-not [IO.Path]::IsPathFullyQualified($OutputDirectory) -or (Test-Path -LiteralPath $OutputDirectory)) {
    throw 'Use a new absolute output directory.'
}
$output = [IO.Path]::GetFullPath($OutputDirectory)
if ($output.StartsWith($repo + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Icon generation output must be outside the checkout.'
}
for ($ancestor = [IO.DirectoryInfo]::new($output); $null -ne $ancestor; $ancestor = $ancestor.Parent) {
    if ($ancestor.Exists -and ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Icon output cannot pass through a reparse point.'
    }
}
$renderer = Get-Item -LiteralPath $Chrome
$rendererHash = (Get-FileHash -LiteralPath $renderer.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
if ($renderer.VersionInfo.FileVersion -ne $spec.renderer.version -or $rendererHash -ne $spec.renderer.sha256) {
    throw 'QLT-011: icon renderer differs from eng/windows-icons.json.'
}
$source = Join-Path $repo $spec.source
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
$svg = [IO.File]::ReadAllText($source)
[xml]$document = $svg
if ($document.DocumentElement.LocalName -ne 'svg' -or $document.DocumentElement.GetAttribute('viewBox') -ne '0 0 256 256' -or
    $document.DocumentElement.GetAttribute('width') -ne '256' -or $document.DocumentElement.GetAttribute('height') -ne '256') {
    throw 'Expected the canonical 256 by 256 SVG.'
}
foreach ($node in $document.SelectNodes('//*')) {
    if ($node.LocalName -notin @('svg','title','desc','rect','path','g','line','circle','ellipse','polygon','polyline')) {
        throw "Unsupported icon SVG element: $($node.LocalName)"
    }
    foreach ($attribute in $node.Attributes) {
        if ($attribute.Name -match '^on|href|style' -or $attribute.Value -match 'url\(') {
            throw 'The icon SVG must be self-contained vector geometry.'
        }
    }
}
$logos = [ordered]@{}
foreach ($base in $spec.bases.PSObject.Properties) {
    foreach ($scale in $spec.scales) {
        $logos["$($base.Name).scale-$scale.png"] = [int][Math]::Floor(($base.Value * $scale + 50) / 100)
    }
}
foreach ($size in $spec.targetSizes) {
    foreach ($suffix in @('', '_altform-unplated', '_altform-lightunplated')) {
        $logos["Square44x44Logo.targetsize-$size$suffix.png"] = [int]$size
    }
}
$sizes = @(@($spec.iconSizes) + @($logos.Values) | ForEach-Object { [int]$_ } | Sort-Object -Unique)
New-Item -ItemType Directory -Path $output -ErrorAction Stop | Out-Null
$positions = [ordered]@{}
$markup = [Text.StringBuilder]::new()
$x = 0; $y = 0; $row = 0
foreach ($size in $sizes) {
    if ($x + $size -gt 1024) { $x = 0; $y += $row + 8; $row = 0 }
    $positions[[string]$size] = @{ x=$x; y=$y; size=$size }
    $document.DocumentElement.SetAttribute('width', [string]$size)
    $document.DocumentElement.SetAttribute('height', [string]$size)
    $scaled = $document.DocumentElement.OuterXml
    [void]$markup.Append("<div style=`"position:absolute;left:$($x)px;top:$($y)px;width:$($size)px;height:$($size)px`">$scaled</div>")
    $x += $size + 8
    $row = [Math]::Max($row, $size)
}
$height = $y + $row
$html = "<!doctype html><html><head><meta charset=`"utf-8`"><style>html,body{margin:0;background:transparent;width:1024px;height:$($height)px}svg{display:block}</style></head><body>$markup</body></html>"
$htmlPath = Join-Path $output 'render-sheet.html'
[IO.File]::WriteAllText($htmlPath, $html, [Text.UTF8Encoding]::new($false))
$sheetPath = Join-Path $output 'render-sheet.png'
$arguments = @('--headless=new','--disable-gpu','--hide-scrollbars','--no-first-run','--no-default-browser-check',
    '--default-background-color=00000000','--force-device-scale-factor=1','--virtual-time-budget=1000',
    "--user-data-dir=$(Join-Path $output 'renderer-profile')", "--window-size=1024,$height", "--screenshot=$sheetPath",
    ([Uri]::new($htmlPath).AbsoluteUri))
$quotedArguments = @($arguments | ForEach-Object { '"' + $_ + '"' })
$process = Start-Process -FilePath $renderer.FullName -ArgumentList $quotedArguments -WindowStyle Hidden -PassThru -Wait -RedirectStandardError (Join-Path $output 'renderer.log')
if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $sheetPath)) { throw 'SVG rendering failed.' }
Add-Type -AssemblyName System.Drawing
$sheet = [Drawing.Bitmap]::FromFile($sheetPath)
if ($sheet.Width -ne 1024 -or $sheet.Height -ne $height) { $sheet.Dispose(); throw 'Rendered sheet dimensions differ.' }
$rendered = @{}
try {
    foreach ($size in $sizes) {
        $point = $positions[[string]$size]
        $bitmap = $sheet.Clone([Drawing.Rectangle]::new($point.x,$point.y,$size,$size), [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            foreach ($corner in @(@(0,0),@(($size-1),0),@(0,($size-1)),@(($size-1),($size-1)))) {
                if ($bitmap.GetPixel($corner[0],$corner[1]).A -eq 255) { throw "Opaque corner in $size pixel icon." }
            }
            if ($bitmap.GetPixel([int]($size/2),[int]($size/2)).A -ne 255) { throw 'The icon center must be opaque.' }
            $path = Join-Path $output "render-$size.png"
            $bitmap.Save($path, [Drawing.Imaging.ImageFormat]::Png)
            $rendered[[int]$size] = $path
        } finally { $bitmap.Dispose() }
    }
} finally { $sheet.Dispose() }
# 32-bit DIB frames retain partial alpha and an AND mask for fully transparent pixels.
$frames = [Collections.Generic.List[byte[]]]::new()
foreach ($size in $spec.iconSizes) {
    if ($size -eq 256) { $frames.Add([IO.File]::ReadAllBytes($rendered[[int]$size])); continue }
    $bitmap = [Drawing.Bitmap]::FromFile($rendered[[int]$size])
    $stream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($stream)
    try {
        $writer.Write([uint32]40); $writer.Write([int]$size); $writer.Write([int]($size*2))
        $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]0); $writer.Write([uint32]($size*$size*4))
        1..4 | ForEach-Object { $writer.Write([uint32]0) }
        for ($iy=$size-1; $iy -ge 0; --$iy) {
            for ($ix=0; $ix -lt $size; ++$ix) {
                $pixel=$bitmap.GetPixel($ix,$iy)
                $writer.Write([byte]$pixel.B); $writer.Write([byte]$pixel.G); $writer.Write([byte]$pixel.R); $writer.Write([byte]$pixel.A)
            }
        }
        $stride=[int]([Math]::Floor(($size+31)/32)*4)
        for ($iy=$size-1; $iy -ge 0; --$iy) {
            $mask=[byte[]]::new($stride)
            for ($ix=0; $ix -lt $size; ++$ix) {
                if ($bitmap.GetPixel($ix,$iy).A -eq 0) {
                    $slot=[int][Math]::Floor($ix/8)
                    $mask[$slot]=$mask[$slot] -bor (0x80 -shr ($ix % 8))
                }
            }
            $writer.Write($mask)
        }
        $frames.Add($stream.ToArray())
    } finally { $writer.Dispose(); $stream.Dispose(); $bitmap.Dispose() }
}
$iconPath=Join-Path $output ([IO.Path]::GetFileName($spec.icon))
$stream=[IO.File]::Open($iconPath,[IO.FileMode]::CreateNew)
$writer=[IO.BinaryWriter]::new($stream)
try {
    $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$frames.Count)
    $offset=6+16*$frames.Count
    for ($index=0; $index -lt $frames.Count; ++$index) {
        $size=$spec.iconSizes[$index]; $encoded=if ($size -eq 256) { 0 } else { $size }
        $writer.Write([byte]$encoded); $writer.Write([byte]$encoded); $writer.Write([uint16]0)
        $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]$frames[$index].Length); $writer.Write([uint32]$offset)
        $offset += $frames[$index].Length
    }
    foreach ($frame in $frames) { $writer.Write($frame) }
} finally { $writer.Dispose(); $stream.Dispose() }
$configuration=Get-Content -LiteralPath (Join-Path $repo 'eng/msix.json') -Raw | ConvertFrom-Json
$assets=@(foreach ($entry in $logos.GetEnumerator()) {
    $path=Join-Path $output $entry.Key
    Copy-Item -LiteralPath $rendered[$entry.Value] -Destination $path
    [ordered]@{path="packaging/windows/$($entry.Key)";destination="Assets/$($entry.Key)";width=$entry.Value;height=$entry.Value;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
})
$configuration.assets=$assets
$artwork=[ordered]@{sourceSHA256=$sourceHash;iconSHA256=(Get-FileHash -LiteralPath $iconPath -Algorithm SHA256).Hash.ToLowerInvariant();rendererSHA256=$rendererHash}
$configuration | Add-Member -NotePropertyName artwork -NotePropertyValue $artwork -Force
$configuration | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'msix.json') -Encoding utf8
$positions | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'render-layout.json') -Encoding utf8
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sourceHash -or
    (Get-FileHash -LiteralPath $renderer.FullName -Algorithm SHA256).Hash.ToLowerInvariant() -ne $rendererHash) {
    throw 'Source or renderer changed during generation; output retained for diagnosis.'
}
Write-Host "Rendered $($assets.Count) MSIX assets and $($frames.Count) ICO frames from $($spec.source)."
