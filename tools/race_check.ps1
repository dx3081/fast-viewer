# race_check.ps1 - press Right x4 then Left x2 quickly (into images not yet
# preloaded) and check that the image on screen matches the index the viewer
# reports. Needs a folder of large JPEGs named DSC_1.jpg .. DSC_N.jpg (N >= 160).
param([Parameter(Mandatory)] [string]$Exe, [Parameter(Mandatory)] [string]$PhotoDir)
$sp = $env:TEMP
if (-not ('T6.W' -as [type])) { Add-Type -Namespace T6 -Name W -MemberDefinition '[DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);' }
$bad = 0
foreach ($run in 1..5) {
    $log = "$sp\rev.log"; Remove-Item $log -ErrorAction SilentlyContinue; $env:FAST_VIEWER_TIMING = $log
    $p = Start-Process $Exe -ArgumentList "`"$PhotoDir\DSC_$(100 + $run * 10).jpg`"" -PassThru
    Remove-Item Env:FAST_VIEWER_TIMING
    Start-Sleep 2; $p.Refresh(); $h = $p.MainWindowHandle
    foreach ($k in 0x27, 0x27, 0x27, 0x27, 0x25, 0x25) { [void][T6.W]::PostMessageW($h, 0x100, [IntPtr]$k, [IntPtr]1); Start-Sleep -Milliseconds 25 }
    Start-Sleep 2
    [void][T6.W]::PostMessageW($h, 0x10, 0, 0); Start-Sleep 1
    $lines = Get-Content $log
    # last image actually presented, and the index the viewer believes it shows
    $shown = $lines | Select-String 'decode_ms=.* (\S+\.jpg) upload|cache: hit (\S+)$' | Select-Object -Last 1
    $name = if ($shown.Matches.Groups[1].Success) { $shown.Matches.Groups[1].Value } else { $shown.Matches.Groups[2].Value }
    $idx = ($lines | Select-String 'nav: \S+ idx=(\d+)|interim id=\d+ idx=(\d+)' | Select-Object -Last 1).Matches
    $i = if ($idx.Groups[1].Success) { $idx.Groups[1].Value } else { $idx.Groups[2].Value }
    $expectName = "DSC_$([int]$i + 1).jpg"   # natural sort: index 0 = DSC_1
    $ok = (Split-Path $name -Leaf) -eq $expectName
    if (-not $ok) { $bad++ }
    "run $run : shown=$(Split-Path $name -Leaf) index=$i ok=$ok"
}
"mismatches: $bad"

