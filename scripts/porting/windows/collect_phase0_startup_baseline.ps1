[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$AppPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [ValidateSet('Debug', 'Release', 'Unknown')]
    [string]$BuildConfiguration = 'Unknown',

    # The current Qt baseline is normally built without deployment beside the
    # executable. Supply Qt's bin directory so the probe uses the same runtime
    # DLL search path as CTest. The later native executable will not need this.
    [ValidateScript({ $_ -eq '' -or (Test-Path -LiteralPath $_ -PathType Container) })]
    [string]$RuntimeDirectory = ''
)

$ErrorActionPreference = 'Stop'

$resolvedAppPath = (Resolve-Path -LiteralPath $AppPath).Path
$resolvedOutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

$settingsDirectory = Join-Path $resolvedOutputDirectory 'settings'
New-Item -ItemType Directory -Force -Path $settingsDirectory | Out-Null

$metricsPath = Join-Path $resolvedOutputDirectory 'startup-metrics.json'
$metadataPath = Join-Path $resolvedOutputDirectory 'startup-metadata.json'

$previousSettingsRoot = $env:CLASSMNGR_SETTINGS_ROOT
$previousPath = $env:PATH
$env:CLASSMNGR_SETTINGS_ROOT = $settingsDirectory

if ($RuntimeDirectory -ne '') {
    $resolvedRuntimeDirectory = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
    $env:PATH = "$resolvedRuntimeDirectory$([System.IO.Path]::PathSeparator)$previousPath"
}

try {
    $quotedMetricsPath = '"' + $metricsPath.Replace('"', '\"') + '"'
    $process = Start-Process `
        -FilePath $resolvedAppPath `
        -ArgumentList @(
            '--startup-performance-test',
            '--startup-performance-output',
            $quotedMetricsPath
        ) `
        -Wait `
        -PassThru `
        -WindowStyle Hidden

    if ($process.ExitCode -ne 0) {
        throw "ClassMngr exited with code $($process.ExitCode) while collecting startup metrics."
    }
}
finally {
    $env:CLASSMNGR_SETTINGS_ROOT = $previousSettingsRoot
    $env:PATH = $previousPath
}

if (-not (Test-Path -LiteralPath $metricsPath -PathType Leaf)) {
    throw "ClassMngr did not produce the expected metrics file: $metricsPath"
}

$operatingSystem = Get-CimInstance Win32_OperatingSystem
$computerSystem = Get-CimInstance Win32_ComputerSystem
$processor = Get-CimInstance Win32_Processor | Select-Object -First 1
$videoControllers = Get-CimInstance Win32_VideoController |
    ForEach-Object {
        [ordered]@{
            name = $_.Name
            adapterCompatibility = $_.AdapterCompatibility
            driverVersion = $_.DriverVersion
            videoProcessor = $_.VideoProcessor
        }
    }
$desktopSettings = Get-ItemProperty `
    -Path 'HKCU:\Control Panel\Desktop' `
    -Name LogPixels `
    -ErrorAction SilentlyContinue
$systemDpi = if ($null -ne $desktopSettings.LogPixels) {
    [int]$desktopSettings.LogPixels
}
else {
    96
}

$metadata = [ordered]@{
    format = 'classmngr-phase0-startup-baseline-v1'
    capturedAtUtc = [DateTime]::UtcNow.ToString('o')
    appPath = $resolvedAppPath
    buildConfiguration = $BuildConfiguration
    architecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
    operatingSystem = [ordered]@{
        caption = $operatingSystem.Caption
        version = $operatingSystem.Version
        buildNumber = $operatingSystem.BuildNumber
    }
    hardware = [ordered]@{
        manufacturer = $computerSystem.Manufacturer
        model = $computerSystem.Model
        processor = $processor.Name
        logicalProcessors = $processor.NumberOfLogicalProcessors
        totalPhysicalMemoryBytes = [UInt64]$computerSystem.TotalPhysicalMemory
    }
    graphics = @($videoControllers)
    display = [ordered]@{
        systemDpi = $systemDpi
        systemScalePercent = [Math]::Round(($systemDpi / 96.0) * 100, 0)
    }
    metricsFile = 'startup-metrics.json'
}

$metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $metadataPath -Encoding utf8

Write-Host "Wrote $metricsPath"
Write-Host "Wrote $metadataPath"

