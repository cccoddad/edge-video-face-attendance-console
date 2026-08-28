$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$application = Join-Path $repoRoot 'build-qt5-mingw-release\release\FaceAttendance.exe'

if (!(Test-Path -LiteralPath $application)) {
    & (Join-Path $PSScriptRoot 'build-qt5.ps1')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$env:PATH = 'D:\QT\5.12.0\mingw73_64\bin;D:\QT\Tools\mingw730_64\bin;D:\qtdeps\opencv452\x64\mingw\bin;D:\qtdeps\SeetaFace\bin;' + $env:PATH
$env:FACE_ATTENDANCE_MODEL_DIR = 'D:\qtdeps\SeetaFace\bin\model'
$env:FACE_ATTENDANCE_DATA_DIR = Join-Path $repoRoot 'runtime-data'

& $application
