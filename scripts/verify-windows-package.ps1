param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string]$PackageDirectory,
    [ValidateRange(1000, 60000)]
    [int]$StartupDurationMilliseconds = 3000
)

$ErrorActionPreference = 'Stop'

function Save-EnvironmentValue {
    param([string]$Name)
    return [PSCustomObject]@{
        Name = $Name
        Value = [Environment]::GetEnvironmentVariable($Name, 'Process')
    }
}

function Restore-EnvironmentValue {
    param($SavedValue)
    [Environment]::SetEnvironmentVariable($SavedValue.Name, $SavedValue.Value, 'Process')
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$packageRoot = (Resolve-Path -LiteralPath $PackageDirectory).Path
$application = Join-Path $packageRoot 'FaceAttendance.exe'
$runId = Get-Date -Format 'yyyyMMdd-HHmmss'
$evidenceDirectory = Join-Path $repoRoot ("runtime-data\package-verification\" + $runId)
$dataDirectory = Join-Path $evidenceDirectory 'data'
$databaseAudit = Join-Path $evidenceDirectory 'database-audit.txt'
$hashManifest = Join-Path $evidenceDirectory 'package-files.sha256'
$reportPath = Join-Path $evidenceDirectory 'report.txt'

$requiredFiles = @(
    'FaceAttendance.exe',
    'Qt5Core.dll',
    'Qt5Gui.dll',
    'Qt5Widgets.dll',
    'Qt5Sql.dll',
    'libgcc_s_seh-1.dll',
    'libstdc++-6.dll',
    'libwinpthread-1.dll',
    'libopencv_core452.dll',
    'libopencv_imgproc452.dll',
    'libopencv_imgcodecs452.dll',
    'libopencv_videoio452.dll',
    'libopencv_highgui452.dll',
    'libSeetaFaceDetector.dll',
    'libSeetaFaceLandmarker.dll',
    'libSeetaFaceRecognizer.dll',
    'libSeetaFaceTracker.dll',
    'libSeetaNet.dll',
    'libSeetaQualityAssessor.dll',
    'platforms\qwindows.dll',
    'sqldrivers\qsqlite.dll',
    'models\fd_2_00.dat',
    'models\pd_2_00_pts5.dat',
    'models\fr_2_10.dat'
)

$missingFiles = @($requiredFiles | Where-Object {
    !(Test-Path -LiteralPath (Join-Path $packageRoot $_) -PathType Leaf)
})
if ($missingFiles.Count -gt 0) {
    throw ('Package is missing required files: ' + ($missingFiles -join ', '))
}

$seetaLibraries = @(Get-ChildItem -LiteralPath $packageRoot -Filter 'libSeeta*.dll' -File)
if ($seetaLibraries.Count -eq 0) {
    throw 'Package does not contain any SeetaFace runtime libraries.'
}

$privateExtensions = @('.db', '.sqlite', '.sqlite3', '.csv', '.log', '.jpg', '.jpeg',
                       '.avi', '.mp4', '.mkv', '.mov', '.wmv')
$privateArtifacts = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File | Where-Object {
    $hasPrivateExtension = $privateExtensions -contains ($_.Extension.ToLowerInvariant())
    $hasUserConfiguration = $_.Name -like '*.pro.user'
    $hasPrivateDependencyPath = $_.Name -eq 'third_party.pri'
    $hasPrivateExtension -or $hasUserConfiguration -or $hasPrivateDependencyPath
})
if ($privateArtifacts.Count -gt 0) {
    $relativePaths = $privateArtifacts | ForEach-Object {
        $_.FullName.Substring($packageRoot.Length + 1)
    }
    throw ('Package contains private or development artifacts: ' + ($relativePaths -join ', '))
}

New-Item -ItemType Directory -Force -Path $dataDirectory | Out-Null
$packageFiles = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File | Sort-Object FullName)
$hashLines = foreach ($file in $packageFiles) {
    $relativePath = $file.FullName.Substring($packageRoot.Length + 1)
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $relativePath"
}
$hashLines | Set-Content -LiteralPath $hashManifest -Encoding utf8

$environmentNames = @(
    'PATH',
    'FACE_ATTENDANCE_MODEL_DIR',
    'FACE_ATTENDANCE_DATA_DIR',
    'FACE_ATTENDANCE_AUTO_OPEN_VIDEO_PATH',
    'FACE_ATTENDANCE_AUTO_OPEN_LOCAL_CAMERA',
    'FACE_ATTENDANCE_RTSP_URL',
    'FACE_ATTENDANCE_DATABASE_AUDIT_PATH',
    'FACE_ATTENDANCE_TEST_EXIT_AFTER_MS'
)
$savedEnvironment = @($environmentNames | ForEach-Object { Save-EnvironmentValue $_ })

try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    Remove-Item Env:FACE_ATTENDANCE_MODEL_DIR -ErrorAction SilentlyContinue
    Remove-Item Env:FACE_ATTENDANCE_AUTO_OPEN_VIDEO_PATH -ErrorAction SilentlyContinue
    Remove-Item Env:FACE_ATTENDANCE_AUTO_OPEN_LOCAL_CAMERA -ErrorAction SilentlyContinue
    Remove-Item Env:FACE_ATTENDANCE_RTSP_URL -ErrorAction SilentlyContinue
    $env:FACE_ATTENDANCE_DATA_DIR = $dataDirectory
    $env:FACE_ATTENDANCE_DATABASE_AUDIT_PATH = $databaseAudit
    $env:FACE_ATTENDANCE_TEST_EXIT_AFTER_MS = [string]$StartupDurationMilliseconds

    $process = Start-Process -FilePath $application -WorkingDirectory $packageRoot -PassThru -Wait
    if ($process.ExitCode -ne 0) {
        throw "Packaged application exited with code $($process.ExitCode)."
    }
    if (!(Test-Path -LiteralPath $databaseAudit -PathType Leaf)) {
        throw 'Packaged application did not write the expected database audit.'
    }

    $auditLines = @(Get-Content -LiteralPath $databaseAudit)
    foreach ($expectedLine in @('total_events=0', 'duplicate_event_keys=0', 'missing_event_keys=0')) {
        if ($auditLines -notcontains $expectedLine) {
            throw "Database audit did not contain: $expectedLine"
        }
    }

    @(
        "package_directory=$packageRoot"
        "file_count=$($packageFiles.Count)"
        "seetaface_library_count=$($seetaLibraries.Count)"
        "startup_duration_ms=$StartupDurationMilliseconds"
        "process_exit_code=$($process.ExitCode)"
        "database_audit=$databaseAudit"
        "hash_manifest=$hashManifest"
        'development_path_removed=true'
        'private_artifacts_found=0'
        'result=passed'
    ) | Set-Content -LiteralPath $reportPath -Encoding utf8

    Write-Output "Package verification passed: $packageRoot"
    Write-Output "Package verification evidence: $evidenceDirectory"
}
finally {
    foreach ($savedValue in $savedEnvironment) {
        Restore-EnvironmentValue $savedValue
    }
}
