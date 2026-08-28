$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $repoRoot 'build-qt5-mingw-release'
$qmake = 'D:\QT\5.12.0\mingw73_64\bin\qmake.exe'
$make = 'D:\QT\Tools\mingw730_64\bin\mingw32-make.exe'

if (!(Test-Path -LiteralPath $qmake) -or !(Test-Path -LiteralPath $make)) {
    throw 'Qt 5.12 MinGW 7.3 toolchain was not found under D:\QT.'
}
if (!(Test-Path -LiteralPath 'D:\qtdeps\opencv452') -or !(Test-Path -LiteralPath 'D:\qtdeps\SeetaFace')) {
    throw 'D:\qtdeps junction is missing. Point it at the supplied OpenCV/SeetaFace package first.'
}

$env:PATH = 'D:\QT\Tools\mingw730_64\bin;D:\QT\5.12.0\mingw73_64\bin;' + $env:PATH
New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
Set-Location -LiteralPath $buildDirectory

& $qmake (Join-Path $repoRoot 'src\FaceRecognition.pro') -spec win32-g++
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $make -j 4
exit $LASTEXITCODE
