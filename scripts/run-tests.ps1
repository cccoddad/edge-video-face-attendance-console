$ErrorActionPreference = 'Continue'

$repoRoot = Split-Path -Parent $PSScriptRoot
$testBase = Join-Path $repoRoot 'build-qt5-mingw-tests'

$env:PATH = 'D:\QT\5.12.0\mingw73_64\bin;D:\QT\Tools\mingw730_64\bin;D:\qtdeps\opencv452\x64\mingw\bin;D:\qtdeps\SeetaFace\bin;' + $env:PATH
$env:FACE_ATTENDANCE_MODEL_DIR = 'D:\qtdeps\SeetaFace\bin\model'
$env:FACE_ATTENDANCE_DATA_DIR = Join-Path $repoRoot 'runtime-data'

$passCount = 0
$failCount = 0

function Run-Test($name, $exePath, $args) {
    $script:passCount; $script:failCount
    if (!(Test-Path -LiteralPath $exePath)) {
        Write-Host "SKIP  $name (exe not found)" -ForegroundColor Yellow
        return
    }
    Write-Host "RUN   $name" -ForegroundColor Cyan -NoNewline
    try {
        if ($args -and $args.Count -gt 0) {
            $output = & $exePath @args 2>&1
        } else {
            $output = & $exePath 2>&1
        }
        $exitCode = $LASTEXITCODE
        if ($exitCode -eq 0) {
            Write-Host "  PASS" -ForegroundColor Green
            $script:passCount++
        } else {
            Write-Host "  FAIL (exit $exitCode)" -ForegroundColor Red
            $script:failCount++
            if ($output) { Write-Host "  $output" }
        }
    } catch {
        Write-Host "  ERROR: $_" -ForegroundColor Red
        $script:failCount++
    }
}

Run-Test "AttendanceStateMachineTest" "$testBase\AttendanceStateMachineTest\release\AttendanceStateMachineTest.exe" @()
Run-Test "AttendanceReportTest" "$testBase\AttendanceReportTest\release\AttendanceReportTest.exe" @("$env:FACE_ATTENDANCE_DATA_DIR\attendance-report-test.csv")
Run-Test "SnapshotStoreTest" "$testBase\SnapshotStoreTest\release\SnapshotStoreTest.exe" @("$env:FACE_ATTENDANCE_DATA_DIR\snapshot-store-test")
Run-Test "VideoSourceRuntimeLogTest" "$testBase\VideoSourceRuntimeLogTest\release\VideoSourceRuntimeLogTest.exe" @()
Run-Test "RtspSourceTest" "$testBase\RtspSourceTest\release\RtspSourceTest.exe" @()
Run-Test "LocalCameraSourceTest" "$testBase\LocalCameraSourceTest\release\LocalCameraSourceTest.exe" @()
Run-Test "RtspConfigurationDialogTest" "$testBase\RtspConfigurationDialogTest\release\RtspConfigurationDialogTest.exe" @()
Run-Test "FaceQualityAssessorTest" "$testBase\FaceQualityAssessorTest\release\FaceQualityAssessorTest.exe" @("$env:FACE_ATTENDANCE_MODEL_DIR")

Write-Host "`n========================================" -ForegroundColor White
Write-Host "Results: $passCount passed, $failCount failed" -ForegroundColor $(if ($failCount -eq 0) { "Green" } else { "Red" })
Write-Host "========================================" -ForegroundColor White
if ($failCount -gt 0) { exit 1 }
exit 0
