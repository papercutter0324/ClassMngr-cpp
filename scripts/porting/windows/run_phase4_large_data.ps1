[CmdletBinding()]
param(
    [string]$Executable,

    [string]$OutputDirectory,

    [ValidateRange(1, 20)]
    [int]$Repetitions = 3,

    [ValidateRange(1000, 900000)]
    [int]$TimeoutMilliseconds = 180000,

    [ValidateSet('x64', 'x86')]
    [string]$ExpectedArchitecture = 'x64',

    [ValidateSet('Debug', 'Release')]
    [string]$ExpectedConfiguration = 'Release',

    [switch]$PlanOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-AbsolutePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location).Path $Path)
    )
}

function Get-HostCapture {
    $capture = [ordered]@{
        capturedAtUtc = [DateTime]::UtcNow.ToString('o')
        operatingSystem = [Environment]::OSVersion.VersionString
        powershell = $PSVersionTable.PSVersion.ToString()
        hostArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
        cpu = @()
        gpu = @()
        displays = @()
        errors = @()
    }

    $queries = @(
        @{ name = 'cpu'; className = 'Win32_Processor'; properties = @('Name', 'Manufacturer', 'NumberOfLogicalProcessors') },
        @{ name = 'gpu'; className = 'Win32_VideoController'; properties = @('Name', 'DriverVersion', 'CurrentHorizontalResolution', 'CurrentVerticalResolution', 'CurrentRefreshRate') },
        @{ name = 'displays'; className = 'Win32_DisplayConfiguration'; properties = @('DeviceName', 'PelsWidth', 'PelsHeight', 'DisplayFrequency', 'BitsPerPel') }
    )
    foreach ($query in $queries) {
        try {
            $rows = @(Get-CimInstance -ClassName $query.className -ErrorAction Stop)
            $capture[$query.name] = @($rows | ForEach-Object {
                $entry = [ordered]@{}
                foreach ($property in $query.properties) {
                    $entry[$property] = $_.$property
                }
                $entry
            })
        }
        catch {
            $capture.errors += "$($query.className): $($_.Exception.Message)"
        }
    }
    return $capture
}

function Get-RevisionCapture {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)
    $capture = [ordered]@{ commit = ''; dirty = $null; status = @(); error = '' }
    try {
        $capture.commit = (& git -C $ProjectRoot rev-parse HEAD 2>$null).Trim()
        $capture.status = @(& git -C $ProjectRoot status --porcelain --untracked-files=normal 2>$null)
        $capture.dirty = $capture.status.Count -gt 0
    }
    catch {
        $capture.error = $_.Exception.Message
    }
    return $capture
}

function Get-Percentile {
    param(
        [Parameter(Mandatory = $true)][double[]]$Values,
        [Parameter(Mandatory = $true)][double]$Percent
    )
    $sorted = @($Values | Sort-Object)
    $rank = [math]::Ceiling($Percent * $sorted.Count)
    $index = [math]::Max(0, [math]::Min($sorted.Count - 1, $rank - 1))
    return [math]::Round([double]$sorted[$index], 3)
}

function Add-MetricObservation {
    param(
        [Parameter(Mandatory = $true)][hashtable]$Map,
        [Parameter(Mandatory = $true)][string]$Key,
        [Parameter(Mandatory = $true)][object]$Value
    )
    if ($Value -is [bool]) { return }
    try { $number = [double]$Value } catch { return }
    if ([double]::IsNaN($number) -or [double]::IsInfinity($number) -or $number -lt 0) { return }
    if (-not $Map.ContainsKey($Key)) {
        $Map[$Key] = [System.Collections.Generic.List[double]]::new()
    }
    $Map[$Key].Add($number)
}

$plan = [ordered]@{
    format = 'classmngr-winui-phase4-large-data-runner-plan-v1'
    executable = if ([string]::IsNullOrWhiteSpace($Executable)) { '<required>' } else { (Get-AbsolutePath $Executable) }
    outputDirectory = if ([string]::IsNullOrWhiteSpace($OutputDirectory)) { '<required>' } else { (Get-AbsolutePath $OutputDirectory) }
    repetitions = $Repetitions
    timeoutMilliseconds = $TimeoutMilliseconds
    expectedArchitecture = $ExpectedArchitecture
    expectedConfiguration = $ExpectedConfiguration
    arguments = @('--phase4-large-data-test', '--phase4-large-data-output <absolute.json>', '--phase4-large-data-run-id <uuid>')
    mode = if ($ExpectedConfiguration -eq 'Release' -and $ExpectedArchitecture -eq 'x64') { 'release-performance-gate' } else { 'functional-only-not-full-gate' }
}
if ($PlanOnly) {
    $plan | ConvertTo-Json -Depth 8
    return
}

if ([string]::IsNullOrWhiteSpace($Executable)) {
    throw 'Executable is required unless -PlanOnly is specified.'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    throw 'OutputDirectory is required unless -PlanOnly is specified.'
}
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "WinUI executable was not found: $Executable"
}
$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$resolvedOutputDirectory = Get-AbsolutePath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null
$summaryPath = Join-Path $resolvedOutputDirectory 'phase4-large-data-summary.json'
if (Test-Path -LiteralPath $summaryPath) {
    throw "Refusing to overwrite large-data runner summary: $summaryPath"
}
$validatorPath = Join-Path $PSScriptRoot 'validate_phase4_large_data_artifacts.ps1'
if (-not (Test-Path -LiteralPath $validatorPath -PathType Leaf)) {
    throw "Large-data validator was not found: $validatorPath"
}

$hostCapture = Get-HostCapture
$hostCapture.fixtureIdentifier = 'classmngr-phase4-large-data-v1-class-list-10000x1-roster-5000x1-schedule-2000x1-speaking-10000x8'
$executableHash = ((Get-FileHash -LiteralPath $resolvedExecutable -Algorithm SHA256).Hash).ToLowerInvariant()
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$revisionCapture = Get-RevisionCapture -ProjectRoot $projectRoot
$iterations = [System.Collections.Generic.List[object]]::new()
$metricObservations = @{}

for ($iteration = 1; $iteration -le $Repetitions; $iteration++) {
    $runId = [Guid]::NewGuid().ToString()
    $reportPath = Join-Path $resolvedOutputDirectory "phase4-large-data-$runId.json"
    $process = $null
    $processId = 0
    $processExited = $false
    $timedOut = $false
    $exitCode = $null
    $validationStatus = 'not-run'
    $validationGateStatus = 'not-run'
    $validationMessage = ''
    $reportStatus = 'missing'

    try {
        $quotedReportPath = '"' + $reportPath.Replace('"', '\"') + '"'
        $process = Start-Process `
            -FilePath $resolvedExecutable `
            -WorkingDirectory ([System.IO.Path]::GetDirectoryName($resolvedExecutable)) `
            -ArgumentList @('--phase4-large-data-test', '--phase4-large-data-output', $quotedReportPath, '--phase4-large-data-run-id', $runId) `
            -WindowStyle Normal `
            -PassThru
        $processId = $process.Id
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            $timedOut = $true
        }
        $process.Refresh()
        if ($process.HasExited) {
            $processExited = $true
            $exitCode = $process.ExitCode
        }
    }
    catch {
        $validationMessage = "Launch/wait failed: $($_.Exception.Message)"
    }
    finally {
        if ($null -ne $process) {
            try {
                $process.Refresh()
                if (-not $process.HasExited) {
                    $timedOut = $true
                    $process.Kill()
                    $process.WaitForExit(5000)
                }
                $process.Refresh()
                $processExited = $process.HasExited
                if ($processExited) { $exitCode = $process.ExitCode }
            }
            catch {
                if ([string]::IsNullOrWhiteSpace($validationMessage)) {
                    $validationMessage = "Owned-process cleanup failed: $($_.Exception.Message)"
                }
            }
        }
    }

    if (Test-Path -LiteralPath $reportPath -PathType Leaf) {
        try {
            $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
            $reportStatus = [string]$report.overallStatus
            foreach ($workload in @($report.workloads)) {
                foreach ($checkpoint in @($workload.checkpoints)) {
                    $prefix = "workload/$($workload.name)/checkpoint/$($checkpoint.name)"
                    foreach ($field in @('realizedRows', 'realizedCells', 'frameSampleCount', 'frameP95Ms', 'frameMaxMs', 'privateBytes', 'privateWorkingSetBytes', 'nativeAllocationBytes', 'nativeAllocationCount')) {
                        Add-MetricObservation -Map $metricObservations -Key "$prefix/$field" -Value $checkpoint.$field
                    }
                }
            }
        }
        catch {
            $reportStatus = 'malformed'
            if ([string]::IsNullOrWhiteSpace($validationMessage)) {
                $validationMessage = "Report could not be read: $($_.Exception.Message)"
            }
        }
    }

    if (Test-Path -LiteralPath $reportPath -PathType Leaf) {
        try {
            $validationOutput = & $validatorPath `
                -ReportPath $reportPath `
                -ExpectedProcessId $processId `
                -ExpectedRunId $runId `
                -ExpectedArchitecture $ExpectedArchitecture `
                -ExpectedConfiguration $ExpectedConfiguration 2>&1 | Out-String
            $validationResult = $validationOutput.Trim() | ConvertFrom-Json
            $validationGateStatus = [string]$validationResult.gateStatus
            $validationStatus = if ([bool]$validationResult.passed) { 'passed' } else { 'functional-passed-performance-failed' }
            if ($validationStatus -ne 'passed') { $validationMessage = $validationOutput.Trim() }
        }
        catch {
            $validationStatus = 'failed'
            $validationMessage = $_.Exception.Message
        }
    }
    else {
        $validationStatus = 'failed'
        if ([string]::IsNullOrWhiteSpace($validationMessage)) { $validationMessage = "Report was not produced: $reportPath" }
    }

    $iterations.Add([ordered]@{
        repetition = $iteration
        runId = $runId
        processId = $processId
        processExited = $processExited
        timedOut = $timedOut
        exitCode = $exitCode
        reportStatus = $reportStatus
        reportPath = [System.IO.Path]::GetFullPath($reportPath)
        validationStatus = $validationStatus
        validationGateStatus = $validationGateStatus
        validationMessage = $validationMessage
    }) | Out-Null
}

$repeatSummaries = [System.Collections.Generic.List[object]]::new()
foreach ($key in @($metricObservations.Keys | Sort-Object)) {
    $values = @($metricObservations[$key].ToArray())
    if ($values.Count -eq 0) { continue }
    $repeatSummaries.Add([ordered]@{
        metric = $key
        observedCount = $values.Count
        median = Get-Percentile -Values $values -Percent 0.5
        p95 = Get-Percentile -Values $values -Percent 0.95
        max = [math]::Round((($values | Measure-Object -Maximum).Maximum), 3)
    }) | Out-Null
}

$iterationFunctionalFailures = @($iterations | Where-Object {
    -not $_.processExited -or $_.timedOut -or $_.exitCode -ne 0 -or $_.validationStatus -notin @('passed', 'functional-passed-performance-failed')
})
$releaseGateEligible = $ExpectedArchitecture -eq 'x64' -and $ExpectedConfiguration -eq 'Release' -and $Repetitions -ge 3
$functionalValidationPassed = $iterationFunctionalFailures.Count -eq 0
$fullGatePassed = $releaseGateEligible -and $functionalValidationPassed -and @($iterations | Where-Object { $_.validationStatus -ne 'passed' }).Count -eq 0
$overallStatus = if ($fullGatePassed) { 'passed' } elseif ($functionalValidationPassed) { 'incomplete' } else { 'failed' }
$summary = [ordered]@{
    format = 'classmngr.winui.phase4.large-data-runner.v1'
    collectedAtUtc = [DateTime]::UtcNow.ToString('o')
    executable = $resolvedExecutable
    executableSha256 = $executableHash
    outputDirectory = [System.IO.Path]::GetFullPath($resolvedOutputDirectory)
    expectedArchitecture = $ExpectedArchitecture
    expectedConfiguration = $ExpectedConfiguration
    mode = if ($releaseGateEligible) { 'release-performance-gate' } else { 'functional-only-not-full-gate' }
    repetitions = $Repetitions
    timeoutMilliseconds = $TimeoutMilliseconds
    hostCapture = $hostCapture
    revision = $revisionCapture
    iterations = @($iterations.ToArray())
    repeatSummaries = @($repeatSummaries.ToArray())
    overall = [ordered]@{
        status = $overallStatus
        passed = $fullGatePassed
        fullGatePassed = $fullGatePassed
        functionalValidationPassed = $functionalValidationPassed
        releaseGateEligible = $releaseGateEligible
        failedIterations = $iterationFunctionalFailures.Count
    }
}
[System.IO.File]::WriteAllText(
    $summaryPath,
    ($summary | ConvertTo-Json -Depth 20) + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false)
)
Write-Host "Phase 4 large-data summary: $summaryPath"
if ($overallStatus -eq 'failed') {
    throw "Phase 4 large-data runner did not pass the x64 Release gate. Summary: $summaryPath"
}
