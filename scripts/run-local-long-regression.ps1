param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Video')]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$VideoPath,
    [Parameter(Mandatory = $true, ParameterSetName = 'Camera')]
    [switch]$LocalCamera,
    [ValidateRange(0, 15)]
    [int]$LocalCameraIndex = 0,
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string]$DataDirectory,
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string]$SeedDataDirectory,
    [ValidateRange(1, 240)]
    [int]$DurationMinutes = 30,
    [ValidateRange(5, 60)]
    [int]$SampleIntervalSeconds = 10
)

$ErrorActionPreference = 'Stop'

function Save-EnvironmentValue {
    param([string]$Name)
    return [PSCustomObject]@{ Name = $Name; Value = [Environment]::GetEnvironmentVariable($Name, 'Process') }
}

function Restore-EnvironmentValue {
    param($SavedValue)
    [Environment]::SetEnvironmentVariable($SavedValue.Name, $SavedValue.Value, 'Process')
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$application = Join-Path $repoRoot 'build-qt5-mingw-release\release\FaceAttendance.exe'
if (!(Test-Path -LiteralPath $application)) {
    & (Join-Path $PSScriptRoot 'build-qt5.ps1')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$runId = Get-Date -Format 'yyyyMMdd-HHmmss'
$evidenceDirectory = Join-Path $repoRoot ("runtime-data\long-regression\" + $runId)
New-Item -ItemType Directory -Force -Path $evidenceDirectory | Out-Null
if ($DataDirectory -and $SeedDataDirectory) {
    throw 'DataDirectory and SeedDataDirectory cannot be used together.'
}
$runDataDirectory = if ([string]::IsNullOrWhiteSpace($DataDirectory)) {
    Join-Path $evidenceDirectory 'data'
} else {
    (Resolve-Path -LiteralPath $DataDirectory).Path
}
if ($SeedDataDirectory) {
    New-Item -ItemType Directory -Force -Path $runDataDirectory | Out-Null
    $seedDirectory = (Resolve-Path -LiteralPath $SeedDataDirectory).Path
    foreach ($databaseFile in @('attendance.db', 'faces.db')) {
        $sourcePath = Join-Path $seedDirectory $databaseFile
        if (Test-Path -LiteralPath $sourcePath -PathType Leaf) {
            Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runDataDirectory $databaseFile)
        }
    }
}
$performanceLog = Join-Path $evidenceDirectory 'application-performance.csv'
$processSamples = Join-Path $evidenceDirectory 'process-samples.csv'
$databaseAudit = Join-Path $evidenceDirectory 'database-audit.txt'
$runSummary = Join-Path $evidenceDirectory 'summary.txt'

$savedEnvironment = @(
    Save-EnvironmentValue 'PATH'
    Save-EnvironmentValue 'FACE_ATTENDANCE_MODEL_DIR'
    Save-EnvironmentValue 'FACE_ATTENDANCE_DATA_DIR'
    Save-EnvironmentValue 'FACE_ATTENDANCE_LOCAL_VIDEO_LOOP'
    Save-EnvironmentValue 'FACE_ATTENDANCE_LOCAL_CAMERA_INDEX'
    Save-EnvironmentValue 'FACE_ATTENDANCE_AUTO_OPEN_VIDEO_PATH'
    Save-EnvironmentValue 'FACE_ATTENDANCE_AUTO_OPEN_LOCAL_CAMERA'
    Save-EnvironmentValue 'FACE_ATTENDANCE_PERFORMANCE_LOG_PATH'
    Save-EnvironmentValue 'FACE_ATTENDANCE_PERFORMANCE_LOG_INTERVAL_MS'
    Save-EnvironmentValue 'FACE_ATTENDANCE_DATABASE_AUDIT_PATH'
    Save-EnvironmentValue 'FACE_ATTENDANCE_TEST_EXIT_AFTER_MS'
)

try {
    $env:PATH = 'D:\QT\5.12.0\mingw73_64\bin;D:\QT\Tools\mingw730_64\bin;D:\qtdeps\opencv452\x64\mingw\bin;D:\qtdeps\SeetaFace\bin;' + $env:PATH
    $env:FACE_ATTENDANCE_MODEL_DIR = 'D:\qtdeps\SeetaFace\bin\model'
    $env:FACE_ATTENDANCE_DATA_DIR = $runDataDirectory
    $env:FACE_ATTENDANCE_LOCAL_VIDEO_LOOP = '1'
    $env:FACE_ATTENDANCE_LOCAL_CAMERA_INDEX = [string]$LocalCameraIndex
    $env:FACE_ATTENDANCE_PERFORMANCE_LOG_PATH = $performanceLog
    $env:FACE_ATTENDANCE_PERFORMANCE_LOG_INTERVAL_MS = '5000'
    $env:FACE_ATTENDANCE_DATABASE_AUDIT_PATH = $databaseAudit
    $env:FACE_ATTENDANCE_TEST_EXIT_AFTER_MS = [string]($DurationMinutes * 60 * 1000)
    if ($LocalCamera) {
        $env:FACE_ATTENDANCE_AUTO_OPEN_LOCAL_CAMERA = '1'
        Remove-Item Env:FACE_ATTENDANCE_AUTO_OPEN_VIDEO_PATH -ErrorAction SilentlyContinue
    } else {
        $env:FACE_ATTENDANCE_AUTO_OPEN_VIDEO_PATH = (Resolve-Path -LiteralPath $VideoPath).Path
        Remove-Item Env:FACE_ATTENDANCE_AUTO_OPEN_LOCAL_CAMERA -ErrorAction SilentlyContinue
    }

    'timestamp,cpu_percent,working_set_mb,private_memory_mb,handles,threads' | Set-Content -LiteralPath $processSamples -Encoding utf8
    $process = Start-Process -FilePath $application -PassThru
    $lastTimestamp = Get-Date
    $lastCpuSeconds = 0.0
    $hasPreviousSample = $false

    while (!$process.HasExited) {
        Start-Sleep -Seconds $SampleIntervalSeconds
        $process.Refresh()
        if ($process.HasExited) { break }
        $now = Get-Date
        $cpuSeconds = $process.CPU
        $cpuPercent = 0.0
        if ($hasPreviousSample) {
            $wallSeconds = ($now - $lastTimestamp).TotalSeconds
            if ($wallSeconds -gt 0) {
                $cpuPercent = (($cpuSeconds - $lastCpuSeconds) / $wallSeconds / [Environment]::ProcessorCount) * 100.0
            }
        }
        ('{0},{1:N3},{2:N3},{3:N3},{4},{5}' -f $now.ToString('o'), $cpuPercent,
            ($process.WorkingSet64 / 1MB), ($process.PrivateMemorySize64 / 1MB),
            $process.HandleCount, $process.Threads.Count) | Add-Content -LiteralPath $processSamples -Encoding utf8
        $lastTimestamp = $now
        $lastCpuSeconds = $cpuSeconds
        $hasPreviousSample = $true
    }

    $process.WaitForExit()
    @(
        "source=$($PSCmdlet.ParameterSetName)"
        "local_camera_index=$LocalCameraIndex"
        "duration_minutes=$DurationMinutes"
        "process_exit_code=$($process.ExitCode)"
        "performance_log=$performanceLog"
        "process_samples=$processSamples"
        "database_audit=$databaseAudit"
        "data_directory=$runDataDirectory"
        "database=$(Join-Path $runDataDirectory 'attendance.db')"
    ) | Set-Content -LiteralPath $runSummary -Encoding utf8
    Write-Output "Long regression evidence: $evidenceDirectory"
    exit $process.ExitCode
}
finally {
    foreach ($savedValue in $savedEnvironment) {
        Restore-EnvironmentValue $savedValue
    }
}
