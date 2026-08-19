# build_release.ps1 — reproducible 1.0 RC1 release assembly (release-only tool).
# Requires: MSVC Build Tools (vcvars64), CMake, Ninja, Inno Setup 6 (ISCC.exe).
# Produces, under the staging dir (default: repo root .\release):
#   FastViewer-<version>-portable\   (fast_viewer.exe + README.txt + LICENSE)
#   FastViewer-<version>-portable.zip
#   FastViewer-<version>-setup.exe   (per-user Inno installer)
#   SHA256SUMS.txt
# No binaries are committed to Git; the staging dir is gitignored.
param(
    [string]$Staging = (Join-Path $PSScriptRoot '..\release'),
    [string]$BuildDir = 'build',
    # Optional explicit tool paths; when empty, each tool is auto-discovered.
    [string]$VcVars = '',   # vcvars64.bat (MSVC Build Tools)
    [string]$CmakeBin = '', # directory containing cmake.exe
    [string]$NinjaBin = '', # directory containing ninja.exe
    [string]$Iscc = ''      # ISCC.exe (Inno Setup 6)
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$version = '1.0.0-rc1'
$exeName = 'fast_viewer.exe'

# --- Tool auto-discovery (no machine-specific hardcoded defaults) ------------
# 1. Visual Studio installation (Build Tools or full VS) via vswhere.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsInstall = ''
if (Test-Path $vswhere) {
    $vsInstall = (& $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null | Select-Object -First 1)
}

# 2. vcvars64.bat: explicit param, else the vswhere installation.
if (-not $VcVars -and $vsInstall) {
    $candidate = Join-Path $vsInstall 'VC\Auxiliary\Build\vcvars64.bat'
    if (Test-Path $candidate) { $VcVars = $candidate }
}

# 3. CMake: explicit param, else PATH, else the VS-bundled copy.
if (-not $CmakeBin) {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        $CmakeBin = Split-Path $cmd.Source
    } elseif ($vsInstall) {
        $candidate = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
        if (Test-Path $candidate) { $CmakeBin = $candidate }
    }
}

# 4. Ninja: explicit param, else PATH, else the VS-bundled copy.
if (-not $NinjaBin) {
    $cmd = Get-Command ninja -ErrorAction SilentlyContinue
    if ($cmd) {
        $NinjaBin = Split-Path $cmd.Source
    } elseif ($vsInstall) {
        $candidate = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
        if (Test-Path $candidate) { $NinjaBin = $candidate }
    }
}

# 5. ISCC.exe: explicit param, else PATH, else common install locations.
if (-not $Iscc) {
    $cmd = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        $Iscc = $cmd.Source
    } else {
        foreach ($candidate in @(
            (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
            (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'),
            (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'))) {
            if (Test-Path $candidate) { $Iscc = $candidate; break }
        }
    }
}

# 6. Fail loudly with actionable guidance when a tool cannot be located.
foreach ($tool in @(
    @{ Name = 'vcvars64.bat (MSVC Build Tools)'; Path = $VcVars; Param = '-VcVars' },
    @{ Name = 'cmake'; Path = $CmakeBin; Param = '-CmakeBin' },
    @{ Name = 'ninja'; Path = $NinjaBin; Param = '-NinjaBin' },
    @{ Name = 'ISCC.exe (Inno Setup 6)'; Path = $Iscc; Param = '-Iscc' })) {
    if (-not $tool.Path -or -not (Test-Path $tool.Path)) {
        throw "Required tool not found: $($tool.Name). Auto-discovery failed - install it or pass the exact path via $($tool.Param)."
    }
}

$buildDir = Join-Path $repo $BuildDir
$staging = [System.IO.Path]::GetFullPath($Staging)
$portableDir = Join-Path $staging "FastViewer-$version-portable"

Write-Host "==> Release staging: $staging"

# 1. Clean release build (static runtime, metadata, icon; zero-warning policy)
Write-Host '==> Configuring + building (Release /MT)'
if (Test-Path $buildDir) { Remove-Item $buildDir -Recurse -Force }
$env:PATH = "$cmakeBin;$ninjaBin;$env:PATH"
& cmd /c "`"$vcvars`" >nul 2>&1 && cmake -S `"$repo`" -B `"$buildDir`" -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build `"$buildDir`""
if ($LASTEXITCODE -ne 0) { throw 'build failed' }

# 2. Portable layout
Write-Host '==> Portable layout'
if (Test-Path $portableDir) { Remove-Item $portableDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $portableDir | Out-Null
Copy-Item (Join-Path $buildDir $exeName) $portableDir
Copy-Item (Join-Path $repo 'README.md') (Join-Path $portableDir 'README.txt')
Copy-Item (Join-Path $repo 'LICENSE') (Join-Path $portableDir 'LICENSE')

# 3. Portable zip
Write-Host '==> Portable zip'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = Join-Path $staging "FastViewer-$version-portable.zip"
if (Test-Path $zip) { Remove-Item $zip }
[System.IO.Compression.ZipFile]::CreateFromDirectory($portableDir, $zip, [System.IO.Compression.CompressionLevel]::Optimal, $false)

# 4. Installer
Write-Host '==> Installer'
$iss = Join-Path $repo 'packaging\FastViewer.iss'
# Compile in place, passing the build dir and staging dir as ISPP defines.
& $iscc /Qp "/DMyAppBuildDir=$buildDir" "/DMyOutputDir=$staging" $iss
if ($LASTEXITCODE -ne 0) { throw 'installer build failed' }

# 5. Checksums
Write-Host '==> Checksums'
$targets = @(
    "FastViewer-$version-portable.zip",
    "FastViewer-$version-setup.exe",
    "FastViewer-$version-portable\$exeName"
)
$sums = foreach ($rel in $targets) {
    $h = (Get-FileHash (Join-Path $staging $rel) -Algorithm SHA256).Hash
    "$h  $rel"
}
$sums | Set-Content (Join-Path $staging 'SHA256SUMS.txt') -Encoding ASCII
$sums
Write-Host "==> Done. Artifacts in $staging"
