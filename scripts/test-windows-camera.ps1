param(
    [string]$DeviceName = '',
    [ValidateRange(0, 10)]
    [int]$WarmupSeconds = 2,
    [ValidateRange(0.0, 255.0)]
    [double]$MinimumMeanBrightness = 1.0
)

$ErrorActionPreference = 'Stop'

if ($PSVersionTable.PSEdition -ne 'Desktop') {
    $windowsPowerShell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $arguments = @(
        '-NoLogo', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
        '-File', $PSCommandPath,
        '-WarmupSeconds', $WarmupSeconds,
        '-MinimumMeanBrightness', $MinimumMeanBrightness
    )
    if (![string]::IsNullOrWhiteSpace($DeviceName)) {
        $arguments += @('-DeviceName', $DeviceName)
    }
    & $windowsPowerShell @arguments
    exit $LASTEXITCODE
}

Add-Type -AssemblyName System.Runtime.WindowsRuntime
Add-Type -AssemblyName System.Drawing

$null = [Windows.Devices.Enumeration.DeviceInformation, Windows.Devices.Enumeration, ContentType=WindowsRuntime]
$null = [Windows.Devices.Enumeration.DeviceClass, Windows.Devices.Enumeration, ContentType=WindowsRuntime]
$null = [Windows.Media.Capture.MediaCapture, Windows.Media.Capture, ContentType=WindowsRuntime]
$null = [Windows.Media.Capture.MediaCaptureInitializationSettings, Windows.Media.Capture, ContentType=WindowsRuntime]
$null = [Windows.Media.Capture.StreamingCaptureMode, Windows.Media.Capture, ContentType=WindowsRuntime]
$null = [Windows.Media.MediaProperties.ImageEncodingProperties, Windows.Media.MediaProperties, ContentType=WindowsRuntime]
$null = [Windows.Storage.Streams.InMemoryRandomAccessStream, Windows.Storage.Streams, ContentType=WindowsRuntime]

function Wait-WinRtResult($operation, [Type]$resultType)
{
    $method = [System.WindowsRuntimeSystemExtensions].GetMethods() |
        Where-Object {
            $_.Name -eq 'AsTask' -and $_.IsGenericMethod -and $_.GetParameters().Count -eq 1
        } |
        Select-Object -First 1
    $task = $method.MakeGenericMethod($resultType).Invoke($null, @($operation))
    $task.Wait()
    return $task.Result
}

function Wait-WinRtAction($operation)
{
    $method = [System.WindowsRuntimeSystemExtensions].GetMethods() |
        Where-Object {
            $_.Name -eq 'AsTask' -and !$_.IsGenericMethod -and $_.GetParameters().Count -eq 1
        } |
        Select-Object -First 1
    $task = $method.Invoke($null, @($operation))
    try {
        $task.Wait()
    } catch {
        throw $_.Exception.GetBaseException()
    }
}

$stage = 'enumerate-device'
$capture = $null
$stream = $null
$dotNetStream = $null
$bitmap = $null
$exitCode = 0

try {
    $operation = [Windows.Devices.Enumeration.DeviceInformation]::FindAllAsync(
        [Windows.Devices.Enumeration.DeviceClass]::VideoCapture)
    $collectionType = [Windows.Devices.Enumeration.DeviceInformationCollection, Windows.Devices.Enumeration, ContentType=WindowsRuntime]
    $devices = Wait-WinRtResult $operation $collectionType
    $device = if ([string]::IsNullOrWhiteSpace($DeviceName)) {
        $devices | Where-Object IsEnabled | Select-Object -First 1
    } else {
        $devices | Where-Object Name -eq $DeviceName | Select-Object -First 1
    }
    if ($null -eq $device) {
        $requestedDevice = if ([string]::IsNullOrWhiteSpace($DeviceName)) {
            'the first enabled video capture device'
        } else {
            $DeviceName
        }
        throw "Windows MediaCapture did not enumerate the requested camera: $requestedDevice"
    }

    $settings = New-Object Windows.Media.Capture.MediaCaptureInitializationSettings
    $settings.VideoDeviceId = $device.Id
    $settings.StreamingCaptureMode = [Windows.Media.Capture.StreamingCaptureMode]::Video
    $capture = New-Object Windows.Media.Capture.MediaCapture

    $stage = 'initialize-media-capture'
    Wait-WinRtAction ($capture.InitializeAsync($settings))
    if ($WarmupSeconds -gt 0) {
        Start-Sleep -Seconds $WarmupSeconds
    }

    $stage = 'capture-photo'
    $stream = New-Object Windows.Storage.Streams.InMemoryRandomAccessStream
    $encoding = [Windows.Media.MediaProperties.ImageEncodingProperties]::CreateJpeg()
    Wait-WinRtAction ($capture.CapturePhotoToStreamAsync($encoding, $stream))

    $stage = 'measure-frame'
    $stream.Seek(0)
    $dotNetStream = [System.IO.WindowsRuntimeStreamExtensions]::AsStreamForRead($stream)
    $bitmap = New-Object System.Drawing.Bitmap($dotNetStream)
    $sumRed = [double]0
    $sumGreen = [double]0
    $sumBlue = [double]0
    $sampleCount = 0
    $stepX = [Math]::Max(1, [int]($bitmap.Width / 64))
    $stepY = [Math]::Max(1, [int]($bitmap.Height / 48))
    for ($y = 0; $y -lt $bitmap.Height; $y += $stepY) {
        for ($x = 0; $x -lt $bitmap.Width; $x += $stepX) {
            $pixel = $bitmap.GetPixel($x, $y)
            $sumRed += $pixel.R
            $sumGreen += $pixel.G
            $sumBlue += $pixel.B
            ++$sampleCount
        }
    }

    $meanBlue = [Math]::Round($sumBlue / $sampleCount, 3)
    $meanGreen = [Math]::Round($sumGreen / $sampleCount, 3)
    $meanRed = [Math]::Round($sumRed / $sampleCount, 3)
    $maximumMean = (@($meanBlue, $meanGreen, $meanRed) | Measure-Object -Maximum).Maximum
    $success = $maximumMean -gt $MinimumMeanBrightness
    [PSCustomObject]@{
        Success = $success
        Stage = 'complete'
        Device = $device.Name
        Width = $bitmap.Width
        Height = $bitmap.Height
        Samples = $sampleCount
        MeanBgr = @($meanBlue, $meanGreen, $meanRed)
        MinimumMeanBrightness = $MinimumMeanBrightness
    } | ConvertTo-Json -Depth 4
    if (!$success) {
        $exitCode = 3
    }
} catch {
    $baseException = $_.Exception.GetBaseException()
    [PSCustomObject]@{
        Success = $false
        Stage = $stage
        ExceptionType = $baseException.GetType().FullName
        HResult = ('0x{0:X8}' -f ($baseException.HResult -band 0xffffffffL))
        Message = $baseException.Message
    } | ConvertTo-Json -Depth 4
    $exitCode = 2
} finally {
    if ($null -ne $bitmap) {
        $bitmap.Dispose()
    }
    if ($null -ne $dotNetStream) {
        $dotNetStream.Dispose()
    }
    if ($null -ne $stream) {
        $stream.Dispose()
    }
    if ($null -ne $capture) {
        $capture.Dispose()
    }
}

exit $exitCode
