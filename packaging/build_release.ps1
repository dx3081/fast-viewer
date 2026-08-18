# build_release.ps1 — reproducible 1.0 RC1 release assembly (release-only tool).
# Requires: MSVC Build Tools (vcvars64), CMake, Ninja, Inno Setup 6 (ISCC.exe).
# Produces, under the staging dir (default: repo root .\release):
#   FastViewer-<version>-portable\   (fast_viewer.exe + README.txt)
#   FastViewer-<version>-portable.zip
#   FastViewer-<version>-setup.exe   (per-user Inno installer)
#   SHA256SUMS.txt
# No binaries are committed to Git; the staging dir is gitignored.
param(
    [string]$Staging = (Join-Path $PSScriptRoot '..\release')
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$version = '1.0.0-rc1'
$exeName = 'fast_viewer.exe'

$vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$cmakeBin = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$ninjaBin = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
$iscc = 'C:\Users\xdu3\AppData\Local\Programs\Inno Setup 6\ISCC.exe'

$buildDir = Join-Path $repo 'build'
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

# 3. Portable zip
Write-Host '==> Portable zip'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = Join-Path $staging "FastViewer-$version-portable.zip"
if (Test-Path $zip) { Remove-Item $zip }
[System.IO.Compression.ZipFile]::CreateFromDirectory($portableDir, $zip, [System.IO.Compression.CompressionLevel]::Optimal, $false)

# 4. Installer
Write-Host '==> Installer'
$iss = Join-Path $repo 'packaging\FastViewer.iss'
# rewrite staging output path into a temp copy so ISCC emits into $staging
$issText = Get-Content $iss -Raw
$issText = $issText -replace 'OutputDir=.*', "OutputDir=$($staging -replace '\\','\')"
$issTmp = Join-Path $env:TEMP 'FastViewer.iss'
Set-Content -Path $issTmp -Value $issText -Encoding ASCII
& $iscc /Qp $issTmp
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
