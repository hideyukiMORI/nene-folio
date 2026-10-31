# Original vector artwork for the package icon candidate. Windows GDI+, no downloaded assets.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not [IO.Path]::IsPathFullyQualified($OutputDirectory) -or (Test-Path -LiteralPath $OutputDirectory)) {
    throw 'Use a new absolute output directory.'
}
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Path $OutputDirectory -ErrorAction Stop | Out-Null
$specifications = @(
    @{ Name = 'Square44x44Logo.png'; Size = 44 },
    @{ Name = 'Square150x150Logo.png'; Size = 150 },
    @{ Name = 'StoreLogo.png'; Size = 50 },
    @{ Name = 'Preview.png'; Size = 512 }
)
foreach ($specification in $specifications) {
    $size = $specification.Size
    $bitmap = [Drawing.Bitmap]::new($size * 4, $size * 4)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.ScaleTransform($size * 4 / 256.0, $size * 4 / 256.0)
    $background = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#244C59'))
    $paper = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#F8F3E7'))
    $fold = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#ACC9CB'))
    $accent = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#D7AE69'))
    $lines = [Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml('#557981'), 9)
    $lines.StartCap = [Drawing.Drawing2D.LineCap]::Round
    $lines.EndCap = [Drawing.Drawing2D.LineCap]::Round
    $round = [Drawing.Drawing2D.GraphicsPath]::new()
    $round.AddArc(8, 8, 56, 56, 180, 90)
    $round.AddArc(192, 8, 56, 56, 270, 90)
    $round.AddArc(192, 192, 56, 56, 0, 90)
    $round.AddArc(8, 192, 56, 56, 90, 90)
    $round.CloseFigure()
    $graphics.FillPath($background, $round)
    $points = [Drawing.PointF[]]@([Drawing.PointF]::new(65, 45), [Drawing.PointF]::new(158, 45),
        [Drawing.PointF]::new(195, 82), [Drawing.PointF]::new(195, 211), [Drawing.PointF]::new(65, 211))
    $graphics.FillPolygon($paper, $points)
    $graphics.FillRectangle($accent, 65, 45, 12, 166)
    $graphics.FillPolygon($fold, [Drawing.PointF[]]@([Drawing.PointF]::new(158, 45),
        [Drawing.PointF]::new(158, 82), [Drawing.PointF]::new(195, 82)))
    $graphics.DrawLine($lines, 100, 117, 163, 117)
    $graphics.DrawLine($lines, 100, 145, 163, 145)
    $graphics.DrawLine($lines, 100, 173, 143, 173)
    $small = [Drawing.Bitmap]::new($size, $size)
    $scaler = [Drawing.Graphics]::FromImage($small)
    $scaler.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $scaler.DrawImage($bitmap, 0, 0, $size, $size)
    $small.Save((Join-Path $OutputDirectory $specification.Name), [Drawing.Imaging.ImageFormat]::Png)
    foreach ($resource in @($scaler, $small, $round, $lines, $accent, $fold, $paper, $background, $graphics, $bitmap)) {
        $resource.Dispose()
    }
}
