param(
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$application = Join-Path $repoRoot 'build-qt5-mingw-release\release\FaceAttendance.exe'
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot 'dist\FaceAttendance-windows-x64'
}
$outputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$qtBin = 'D:\QT\5.12.0\mingw73_64\bin'
$qtPlugins = 'D:\QT\5.12.0\mingw73_64\plugins'
$mingwBin = 'D:\QT\Tools\mingw730_64\bin'

if (!(Test-Path -LiteralPath $application)) {
    & (Join-Path $PSScriptRoot 'build-qt5.ps1')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
if (!(Test-Path -LiteralPath $qtBin) -or !(Test-Path -LiteralPath $qtPlugins) -or !(Test-Path -LiteralPath $mingwBin)) {
    throw 'The Qt 5.12 MinGW runtime was not found under D:\QT.'
}

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
Copy-Item -LiteralPath $application -Destination $outputDirectory -Force

# Qt 5.12's windeployqt cannot reliably inspect this legacy MinGW executable.
# Copy the small, known runtime set explicitly so packaging remains deterministic.
@('Qt5Core.dll', 'Qt5Gui.dll', 'Qt5Widgets.dll', 'Qt5Sql.dll') | ForEach-Object {
    Copy-Item -LiteralPath (Join-Path $qtBin $_) -Destination $outputDirectory -Force
}
@('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll') | ForEach-Object {
    Copy-Item -LiteralPath (Join-Path $mingwBin $_) -Destination $outputDirectory -Force
}

$platforms = Join-Path $outputDirectory 'platforms'
New-Item -ItemType Directory -Force -Path $platforms | Out-Null
Copy-Item -LiteralPath (Join-Path $qtPlugins 'platforms\qwindows.dll') -Destination $platforms -Force

$sqlDrivers = Join-Path $outputDirectory 'sqldrivers'
New-Item -ItemType Directory -Force -Path $sqlDrivers | Out-Null
Copy-Item -LiteralPath (Join-Path $qtPlugins 'sqldrivers\qsqlite.dll') -Destination $sqlDrivers -Force


Copy-Item -LiteralPath 'D:\qtdeps\opencv452\x64\mingw\bin\libopencv_core452.dll' -Destination $outputDirectory -Force
Copy-Item -LiteralPath 'D:\qtdeps\opencv452\x64\mingw\bin\libopencv_imgproc452.dll' -Destination $outputDirectory -Force
Copy-Item -LiteralPath 'D:\qtdeps\opencv452\x64\mingw\bin\libopencv_imgcodecs452.dll' -Destination $outputDirectory -Force
Copy-Item -LiteralPath 'D:\qtdeps\opencv452\x64\mingw\bin\libopencv_videoio452.dll' -Destination $outputDirectory -Force
Copy-Item -LiteralPath 'D:\qtdeps\opencv452\x64\mingw\bin\libopencv_highgui452.dll' -Destination $outputDirectory -Force
Get-ChildItem -LiteralPath 'D:\qtdeps\SeetaFace\bin' -Filter 'libSeeta*.dll' | Copy-Item -Destination $outputDirectory -Force

$models = Join-Path $outputDirectory 'models'
New-Item -ItemType Directory -Force -Path $models | Out-Null
Copy-Item -LiteralPath 'D:\qtdeps\SeetaFace\bin\model\fd_2_00.dat' -Destination $models -Force
Copy-Item -LiteralPath 'D:\qtdeps\SeetaFace\bin\model\pd_2_00_pts5.dat' -Destination $models -Force
Copy-Item -LiteralPath 'D:\qtdeps\SeetaFace\bin\model\fr_2_10.dat' -Destination $models -Force

Write-Host "Package created: $outputDirectory"
