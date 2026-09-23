# NeNe Folio の窓へ PostMessage でキーを投げる（#156）。前面化しない・他の窓には届かない。
# -Text の `{ENTER}` `{ESC}` `{TAB}` `{UP}` `{DOWN}` `{F2}` `{CTRL+P}` `{CTRL+S}` `{PAUSE}` はキー、それ以外の文字は WM_CHAR。
# -TargetPid でプロセスを選ぶ（hide の実機と枝の exe を取り違えない）。-Title を与えるとそのプロセスの可視の窓（面）を題で選ぶ。
# 投げる先は GetGUIThreadInfo のフォーカス窓（無ければ主窓）。SetForegroundWindow / SendKeys は使わない（ADR 0034 決定 2）。
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Text,
    [string]$Title = '',
    [int]$WaitMs = 800,
    [Alias('Pid')][int]$TargetPid = 0
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$utf8Encoding = [Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = $utf8Encoding
$OutputEncoding = $utf8Encoding

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class KeyPost
{
    public delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    public struct ThreadInfo { public uint Size, Flags; public IntPtr Active, Focus, Capture, Menu, Move, Caret; public Rect Caret2; }

    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool GetGUIThreadInfo(uint thread, ref ThreadInfo info);

    // pid の可視トップレベル窓のうち、title が空なら最初の 1 つ、そうでなければ題が一致するものを返す。
    public static IntPtr Find(uint pid, string title)
    {
        IntPtr found = IntPtr.Zero;
        EnumWindows((hwnd, lParam) =>
        {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner != pid || !IsWindowVisible(hwnd)) return true;
            if (title.Length > 0)
            {
                var text = new StringBuilder(256);
                GetWindowTextW(hwnd, text, text.Capacity);
                if (text.ToString() != title) return true;
            }
            found = hwnd;
            return false;
        }, IntPtr.Zero);
        return found;
    }

    // そのスレッドのフォーカス窓。無い・不可視なら root。
    public static IntPtr Focus(IntPtr root)
    {
        uint pid;
        uint thread = GetWindowThreadProcessId(root, out pid);
        var info = new ThreadInfo();
        info.Size = (uint)Marshal.SizeOf(typeof(ThreadInfo));
        if (GetGUIThreadInfo(thread, ref info) && info.Focus != IntPtr.Zero && IsWindowVisible(info.Focus)) return info.Focus;
        return root;
    }
}
'@

$process = if ($TargetPid -ne 0) { Get-Process -Id $TargetPid -ErrorAction SilentlyContinue }
           else { Get-Process -Name 'NeNeFolio' -ErrorAction SilentlyContinue | Select-Object -First 1 }
if ($null -eq $process) {
    [Console]::Error.WriteLine('post-keys: NeNeFolio process not found')
    exit 2
}
$root = [KeyPost]::Find([uint32]$process.Id, $Title)
if ($root -eq [IntPtr]::Zero) {
    [Console]::Error.WriteLine("post-keys: window not found (pid $($process.Id), title '$Title')")
    exit 2
}

$WM_KEYDOWN = 0x100
$WM_KEYUP = 0x101
$WM_CHAR = 0x102

function Send-Key {
    param([int]$VirtualKey, [int]$Char)
    $target = [KeyPost]::Focus($root)
    [void][KeyPost]::PostMessageW($target, $WM_KEYDOWN, [IntPtr]$VirtualKey, [IntPtr]1)
    if ($Char -ge 0) { [void][KeyPost]::PostMessageW($target, $WM_CHAR, [IntPtr]$Char, [IntPtr]1) }
    [void][KeyPost]::PostMessageW($target, $WM_KEYUP, [IntPtr]$VirtualKey, [IntPtr]0xC0000001)
    Start-Sleep -Milliseconds 40
}

function Send-Control {
    param([int]$Char)
    $target = [KeyPost]::Focus($root)
    [void][KeyPost]::PostMessageW($target, $WM_CHAR, [IntPtr]$Char, [IntPtr]1)
    Start-Sleep -Milliseconds 300
}

$index = 0
while ($index -lt $Text.Length) {
    if ($Text[$index] -eq '{') {
        $end = $Text.IndexOf('}', $index)
        if ($end -lt 0) { [Console]::Error.WriteLine('post-keys: unterminated {token}'); exit 1 }
        $token = $Text.Substring($index + 1, $end - $index - 1)
        switch ($token) {
            'ENTER'  { Send-Key 0x0D 0x0D }
            'ESC'    { Send-Key 0x1B 0x1B }
            'TAB'    { Send-Key 0x09 0x09 }
            'DOWN'   { Send-Key 0x28 -1 }
            'UP'     { Send-Key 0x26 -1 }
            'F2'     { Send-Key 0x71 -1 }
            'CTRL+P' { Send-Control 0x10 }
            'CTRL+S' { Send-Control 0x13 }
            'PAUSE'  { Start-Sleep -Milliseconds 700 }
            default  { [Console]::Error.WriteLine("post-keys: unknown token {$token}"); exit 1 }
        }
        $index = $end + 1
    }
    else {
        $target = [KeyPost]::Focus($root)
        [void][KeyPost]::PostMessageW($target, $WM_CHAR, [IntPtr][int][char]$Text[$index], [IntPtr]1)
        Start-Sleep -Milliseconds 15
        $index++
    }
}
Start-Sleep -Milliseconds $WaitMs
[ordered]@{
    pid   = $process.Id
    root  = ('0x{0:X}' -f $root.ToInt64())
    focus = ('0x{0:X}' -f ([KeyPost]::Focus($root)).ToInt64())
    text  = $Text
} | ConvertTo-Json -Compress
exit 0
