[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $StageDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateSet('x64', 'Win32')]
    [string] $Platform,

    [ValidateRange(1, 300)]
    [int] $WarmupSeconds = 5,

    [ValidateRange(1, 300)]
    [int] $SampleSeconds = 15,

    [ValidateRange(1, 4096)]
    [int] $SteadyStateTargetMiB = 200,

    [string] $ReportPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-AbsolutePath {
    param([Parameter(Mandatory = $true)][string] $Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath(
        (Join-Path -Path (Get-Location).Path -ChildPath $Path)
    )
}

$stagePath = Get-AbsolutePath -Path $StageDirectory
if (-not (Test-Path -LiteralPath $stagePath -PathType Container)) {
    throw "WinUI stage directory was not found: $stagePath"
}

$executablePath = Join-Path $stagePath 'ClassMngrWinUI.exe'
if (-not (Test-Path -LiteralPath $executablePath -PathType Leaf)) {
    throw "WinUI executable was not found: $executablePath"
}

if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $ReportPath = Join-Path $stagePath "phase1-memory-$Platform.json"
}
else {
    $ReportPath = Get-AbsolutePath -Path $ReportPath
}

$process = $null
$samples = @()
$startupWorkingSetMiB = $null
$peakWorkingSetMiB = $null
$failure = $null

try {
    $process = Start-Process `
        -FilePath $executablePath `
        -WorkingDirectory $stagePath `
        -WindowStyle Hidden `
        -PassThru

    $warmupDeadline = [DateTime]::UtcNow.AddSeconds($WarmupSeconds)
    while ([DateTime]::UtcNow -lt $warmupDeadline) {
        $process.Refresh()
        if ($process.HasExited) {
            throw "WinUI process exited during warmup with code $($process.ExitCode)."
        }
        Start-Sleep -Milliseconds 250
    }

    $process.Refresh()
    $startupWorkingSetMiB = [Math]::Round(
        $process.WorkingSet64 / 1MB,
        2
        )

    $sampleDeadline = [DateTime]::UtcNow.AddSeconds($SampleSeconds)
    while ([DateTime]::UtcNow -lt $sampleDeadline) {
        $process.Refresh()
        if ($process.HasExited) {
            throw "WinUI process exited during sampling with code $($process.ExitCode)."
        }

        $samples += [Math]::Round(
            $process.WorkingSet64 / 1MB,
            2
            )
        Start-Sleep -Milliseconds 250
    }

    $process.Refresh()
    $peakWorkingSetMiB = [Math]::Round(
        $process.PeakWorkingSet64 / 1MB,
        2
        )
}
catch {
    $failure = $_
}
finally {
    if ($null -ne $process) {
        $process.Refresh()
        if (-not $process.HasExited) {
            $process.CloseMainWindow() | Out-Null
            if (-not $process.WaitForExit(5000)) {
                $process.Kill()
                $process.WaitForExit()
            }
        }
    }
}

if ($null -ne $failure) {
    throw $failure
}

if ($samples.Count -eq 0) {
    throw 'No working-set samples were collected.'
}

$steadyStatePeakMiB = ($samples | Measure-Object -Maximum).Maximum
$report = [ordered] @{
    format = 'classmngr-winui-phase1-memory-v1'
    measuredAtUtc = [DateTime]::UtcNow.ToString('o')
    platform = $Platform
    stageDirectory = $stagePath
    executable = $executablePath
    warmupSeconds = $WarmupSeconds
    sampleSeconds = $SampleSeconds
    startupWorkingSetMiB = $startupWorkingSetMiB
    steadyStatePeakWorkingSetMiB = $steadyStatePeakMiB
    peakWorkingSetMiB = $peakWorkingSetMiB
    steadyStateTargetMiB = $SteadyStateTargetMiB
    steadyStateWithinTarget = ($steadyStatePeakMiB -le $SteadyStateTargetMiB)
    sampleCount = $samples.Count
    samplesMiB = @($samples)
}

$reportDirectory = Split-Path -Parent $ReportPath
if (-not (Test-Path -LiteralPath $reportDirectory)) {
    New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
}
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ReportPath -Encoding UTF8

Write-Host ("WinUI memory report: {0} MiB steady-state peak, {1} MiB process " +
    "peak; target {2} MiB. Report: {3}" -f `
    $steadyStatePeakMiB,
    $peakWorkingSetMiB,
    $SteadyStateTargetMiB,
    $ReportPath
    )

if (-not $report.steadyStateWithinTarget) {
    throw "WinUI steady-state working set exceeded the target of $SteadyStateTargetMiB MiB."
}
