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
    [string]$VcVars = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    [string]$CmakeBin = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin',
    [string]$NinjaBin = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja',
    [string]$Iscc = 'C:\Users\xdu3\AppData\Local\Programs\Inno Setup 6\ISCC.exe'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$version = '1.0.0-rc1'
$exeName = 'fast_viewer.exe'

# Tool existence checks with clear errors (all paths are overridable via params).
foreach ($tool in @(
    @{ Name = 'vcvars64.bat (MSVC Build Tools)'; Path = $VcVars },
    @{ Name = 'cmake (VS CMake)'; Path = $CmakeBin },
    @{ Name = 'ninja (VS Ninja)'; Path = $NinjaBin },
    @{ Name = 'ISCC.exe (Inno Setup 6)'; Path = $Iscc })) {
    if (-not (Test-Path $tool.Path)) {
        throw "Required tool not found: $($tool.Name) at '$($tool.Path)'. Pass the correct path via -VcVars/-CmakeBin/-NinjaBin/-Iscc."
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
