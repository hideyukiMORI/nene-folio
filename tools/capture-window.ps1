# 題名の完全一致で見つけた窓を PrintWindow(PW_RENDERFULLCONTENT) で写し、PNG に保存して JSON 1 つを返す（#103）。
# 窓が無い・PrintWindow が 0 を返したときは非 0 で止まる。画面取得が写らない環境の回避はしない。
# 第三者の配布物は使わない（QLT-011: System.Drawing と Win32 の P/Invoke だけ）。
param(
    [Parameter(Mandatory)][string]$Title,
    [Parameter(Mandatory)][string]$Out
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
using System.Text;

public static class WindowCapture
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }

    public delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowW(string className, string windowName);
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumProc callback, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowTextW(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll")]
    public static extern int GetWindowTextLengthW(IntPtr hwnd);
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")]
    public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);

    public static IntPtr Find(string title)
    {
        IntPtr found = FindWindowW(null, title);
        if (found != IntPtr.Zero) return found;
        EnumWindows((hwnd, lParam) =>
        {
            int length = GetWindowTextLengthW(hwnd);
            if (length != title.Length) return true;
            var text = new StringBuilder(length + 1);
            GetWindowTextW(hwnd, text, text.Capacity);
            if (text.ToString() != title) return true;
            found = hwnd;
            return false;
        }, IntPtr.Zero);
        return found;
    }
}
'@

# 物理画素の寸法で測る（DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = -4）。
[void][WindowCapture]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
$hwnd = [WindowCapture]::Find($Title)
if ($hwnd -eq [IntPtr]::Zero) {
    [Console]::Error.WriteLine("capture-window: window not found: $Title")
    exit 2
}
$rect = [WindowCapture+Rect]::new()
if (-not [WindowCapture]::GetWindowRect($hwnd, [ref]$rect)) {
    [Console]::Error.WriteLine('capture-window: GetWindowRect failed')
    exit 3
}
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
if ($width -le 0 -or $height -le 0) {
    [Console]::Error.WriteLine("capture-window: empty window rect: ${width}x${height}")
    exit 3
}
$outPath = [IO.Path]::GetFullPath($Out, (Get-Location).Path)
$bitmap = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $printed = $false
    try {
        $hdc = $graphics.GetHdc()
        try { $printed = [WindowCapture]::PrintWindow($hwnd, $hdc, 2) }
        finally { $graphics.ReleaseHdc($hdc) }
    }
    finally { $graphics.Dispose() }
    if (-not $printed) {
        [Console]::Error.WriteLine('capture-window: PrintWindow returned 0')
        exit 4
    }
    $bitmap.Save($outPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally { $bitmap.Dispose() }
[ordered]@{
    hwnd   = ('0x{0:X}' -f $hwnd.ToInt64())
    width  = $width
    height = $height
    out    = $outPath
} | ConvertTo-Json -Compress
exit 0
