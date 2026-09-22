# 前後のスクリーンショット 2 枚を画素（ARGB）で比べ、差分の外接矩形を JSON 1 つで返す（#103）。
# 合否は判定しない。読む人が bounds / inside を見て決める。exit は比べられたら常に 0。
# 読めないファイル・寸法の違いは非 0 で止まる。第三者の配布物は使わない（QLT-011: System.Drawing だけ）。
# 矩形は RECT と同じく l / t を含み r / b を含まない。差分が 0 画素なら bounds は null で、-Expect があれば inside は true。
param(
    [Parameter(Mandatory)][string]$Before,
    [Parameter(Mandatory)][string]$After,
    [string]$Expect
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$utf8Encoding = [Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = $utf8Encoding
$OutputEncoding = $utf8Encoding

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class ScreenDiff
{
    // 32bpp ARGB で LockBits した 2 枚を行ごとに写して比べる。
    // 戻り値: [changedPixels, l, t, r, b]。差分 0 なら l / t / r / b は -1。
    public static long[] Compare(IntPtr scanA, int strideA, IntPtr scanB, int strideB, int width, int height)
    {
        var rowA = new int[width];
        var rowB = new int[width];
        long changed = 0;
        int left = int.MaxValue, top = int.MaxValue, right = -1, bottom = -1;
        for (int y = 0; y < height; y++)
        {
            Marshal.Copy(IntPtr.Add(scanA, y * strideA), rowA, 0, width);
            Marshal.Copy(IntPtr.Add(scanB, y * strideB), rowB, 0, width);
            for (int x = 0; x < width; x++)
            {
                if (rowA[x] != rowB[x])
                {
                    changed++;
                    if (x < left) left = x;
                    if (x > right) right = x;
                    if (y < top) top = y;
                    bottom = y;
                }
            }
        }
        if (changed == 0) return new long[] { 0, -1, -1, -1, -1 };
        return new long[] { changed, left, top, right + 1, bottom + 1 };
    }
}
'@

function Read-Png([string]$path) {
    $full = [IO.Path]::GetFullPath($path, (Get-Location).Path)
    if (-not [IO.File]::Exists($full)) { throw "compare-screens: file not found: $full" }
    return [Drawing.Bitmap]::new($full)
}

$expectRect = $null
if ($Expect) {
    $parts = $Expect.Split(',')
    if ($parts.Count -ne 4) { throw 'compare-screens: -Expect must be "l,t,r,b".' }
    $values = $parts | ForEach-Object { [int]::Parse($_.Trim()) }
    $expectRect = [ordered]@{ l = $values[0]; t = $values[1]; r = $values[2]; b = $values[3] }
}

$beforeImage = Read-Png $Before
try {
    $afterImage = Read-Png $After
    try {
        if ($beforeImage.Width -ne $afterImage.Width -or $beforeImage.Height -ne $afterImage.Height) {
            [Console]::Error.WriteLine("compare-screens: size differs: $($beforeImage.Width)x$($beforeImage.Height) vs $($afterImage.Width)x$($afterImage.Height)")
            exit 2
        }
        $area = [Drawing.Rectangle]::new(0, 0, $beforeImage.Width, $beforeImage.Height)
        $format = [Drawing.Imaging.PixelFormat]::Format32bppArgb
        $mode = [Drawing.Imaging.ImageLockMode]::ReadOnly
        $dataA = $beforeImage.LockBits($area, $mode, $format)
        try {
            $dataB = $afterImage.LockBits($area, $mode, $format)
            try {
                $result = [ScreenDiff]::Compare($dataA.Scan0, $dataA.Stride, $dataB.Scan0, $dataB.Stride, $area.Width, $area.Height)
            }
            finally { $afterImage.UnlockBits($dataB) }
        }
        finally { $beforeImage.UnlockBits($dataA) }
        $bounds = $null
        if ($result[0] -gt 0) {
            $bounds = [ordered]@{ l = $result[1]; t = $result[2]; r = $result[3]; b = $result[4] }
        }
        $inside = $null
        if ($null -ne $expectRect) {
            $inside = ($null -eq $bounds) -or (
                $bounds.l -ge $expectRect.l -and $bounds.t -ge $expectRect.t -and
                $bounds.r -le $expectRect.r -and $bounds.b -le $expectRect.b)
        }
        [ordered]@{
            width         = $beforeImage.Width
            height        = $beforeImage.Height
            changedPixels = $result[0]
            bounds        = $bounds
            expect        = $expectRect
            inside        = $inside
        } | ConvertTo-Json -Compress -Depth 3
    }
    finally { $afterImage.Dispose() }
}
finally { $beforeImage.Dispose() }
exit 0
