# nav_bench.ps1 - measures navigation speed and UI-thread stalls.
# Launches fast_viewer.exe on -Start, then posts Right-arrow key presses every
# -IntervalMs for -Steps presses. Between presses it pings the window with
# SendMessageTimeout(WM_NULL); a slow ping means the UI thread was busy (the
# user feels this as stutter). Reads the viewer's own timing log for per-image
# navigation latency.
#
# Example:
#   pwsh tools\nav_bench.ps1 -Exe build-a2\fast_viewer.exe -Start D:\pics\DSC_1.jpg -Steps 40 -IntervalMs 600
param(
    [Parameter(Mandatory)] [string]$Exe,
    [Parameter(Mandatory)] [string]$Start,
    [int]$Steps = 40,
    [int]$IntervalMs = 600,
    [int]$SettleMs = 2500,
    [string]$Label = ''
)
$ErrorActionPreference = 'Stop'
Add-Type -Namespace FvBench -Name W -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
[DllImport("user32.dll")] public static extern IntPtr SendMessageTimeoutW(IntPtr h, uint m, IntPtr w, IntPtr l, uint flags, uint timeout, out IntPtr result);
'@

$log = Join-Path ([System.IO.Path]::GetTempPath()) ("fv_bench_{0}.log" -f [guid]::NewGuid().ToString('N'))
$env:FAST_VIEWER_TIMING = $log
$p = Start-Process -FilePath $Exe -ArgumentList "`"$Start`"" -PassThru
Remove-Item Env:FAST_VIEWER_TIMING

$sw = [Diagnostics.Stopwatch]::StartNew()
while ($p.MainWindowHandle -eq 0 -and $sw.ElapsedMilliseconds -lt 5000) { Start-Sleep -Milliseconds 20; $p.Refresh() }
$hwnd = $p.MainWindowHandle
if ($hwnd -eq 0) { throw 'viewer window not found' }
Start-Sleep -Milliseconds 1500   # let first image + preload settle

$stalls = New-Object System.Collections.Generic.List[double]
$ping = {
    $t = [Diagnostics.Stopwatch]::StartNew(); $r = [IntPtr]::Zero
    [void][FvBench.W]::SendMessageTimeoutW($hwnd, 0, [IntPtr]::Zero, [IntPtr]::Zero, 0, 5000, [ref]$r)
    $stalls.Add($t.Elapsed.TotalMilliseconds)
}
$peakWs = 0
$clock = [Diagnostics.Stopwatch]::StartNew()
for ($i = 0; $i -lt $Steps; $i++) {
    [void][FvBench.W]::PostMessageW($hwnd, 0x0100, [IntPtr]0x27, [IntPtr]1)   # WM_KEYDOWN VK_RIGHT
    $next = ($i + 1) * $IntervalMs
    while ($clock.ElapsedMilliseconds -lt $next) { & $ping; Start-Sleep -Milliseconds 4 }
    $p.Refresh(); if ($p.WorkingSet64 -gt $peakWs) { $peakWs = $p.WorkingSet64 }
}
$end = $clock.ElapsedMilliseconds + $SettleMs
while ($clock.ElapsedMilliseconds -lt $end) { & $ping; Start-Sleep -Milliseconds 4 }
$p.Refresh(); $settleWs = $p.WorkingSet64; $cpu = $p.TotalProcessorTime.TotalMilliseconds
[void][FvBench.W]::PostMessageW($hwnd, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)  # WM_CLOSE
if (-not $p.WaitForExit(5000)) { $p.Kill(); Write-Warning 'viewer did not exit within 5 s' }

$lines = Get-Content $log -Encoding UTF8
Remove-Item $log -ErrorAction SilentlyContinue
$nav = foreach ($l in $lines) { if ($l -match 'nav: \S+ idx=(\d+) .*total_ms=([\d.]+)') { [double]$Matches[2] } }
$interim = ($lines | Select-String 'decode: interim').Count
$hits = ($lines | Select-String 'cache: hit').Count
$miss = ($lines | Select-String 'cache: miss').Count
$lastIdx = ($lines | Select-String 'nav: \S+ idx=(\d+)' | Select-Object -Last 1).Matches.Groups[1].Value
function Pct($a, $q) { if (-not $a) { return 0 }; $s = $a | Sort-Object; $s[[math]::Min($s.Count - 1, [int][math]::Floor($q * $s.Count))] }
$st = $stalls.ToArray()
[pscustomobject]@{
    label          = $Label
    presses        = $Steps
    interval_ms    = $IntervalMs
    shown          = @($nav).Count
    interim        = $interim
    last_index     = $lastIdx
    cache_hit      = $hits
    cache_miss     = $miss
    nav_median_ms  = [math]::Round((Pct $nav 0.5), 1)
    nav_p90_ms     = [math]::Round((Pct $nav 0.9), 1)
    nav_max_ms     = [math]::Round(($nav | Measure-Object -Maximum).Maximum, 1)
    ui_stall_max   = [math]::Round(($st | Measure-Object -Maximum).Maximum, 1)
    ui_stalls_50ms = @($st | Where-Object { $_ -gt 50 }).Count
    ui_stalls_100ms = @($st | Where-Object { $_ -gt 100 }).Count
    peak_ws_mb     = [math]::Round($peakWs / 1MB)
    settle_ws_mb   = [math]::Round($settleWs / 1MB)
    cpu_ms         = [math]::Round($cpu)
}
