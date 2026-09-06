[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$StageDirectory,

    [ValidateSet('x64', 'Win32')]
    [string]$Platform = 'x64',

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [switch]$SkipVisualScenarios,

    [switch]$SkipMemory,

    [switch]$SkipLargeData,

    [ValidateSet('required', 'passed')]
    [string]$KoreanImeStatus = 'required',

    [ValidateSet('required', 'passed')]
    [string]$DpiStatus = 'required',

    [switch]$PlanOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-AbsolutePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath(
        (Join-Path -Path (Get-Location).Path -ChildPath $Path)
    )
}

function Get-UniqueEvidenceDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ParentDirectory
    )

    for ($attempt = 0; $attempt -lt 10; $attempt++) {
        $timestamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
        $suffix = [Guid]::NewGuid().ToString('N').Substring(0, 8)
        $candidate = Join-Path $ParentDirectory "phase4-$timestamp-$suffix"
        try {
            New-Item -ItemType Directory -Path $candidate -ErrorAction Stop | Out-Null
            return $candidate
        }
        catch {
            if (-not (Test-Path -LiteralPath $candidate)) {
                throw
            }
        }
    }

    throw "Could not create a unique Phase 4 evidence directory below '$ParentDirectory'."
}

function New-Check {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string]$Status,

        [Parameter(Mandatory = $true)]
        [bool]$Enabled,

        [string]$Path = '',

        [Nullable[int]]$ExitCode = $null,

        [string]$Failure = ''
    )

    return [ordered]@{
        name = $Name
        enabled = $Enabled
        status = $Status
        passed = $Status -eq 'passed'
        path = $Path
        exitCode = $ExitCode
        failure = $Failure
    }
}

$stagePath = Get-AbsolutePath -Path $StageDirectory
$requestedOutputPath = Get-AbsolutePath -Path $OutputDirectory
$executablePath = Join-Path $stagePath 'ClassMngrWinUI.exe'
$scenarioScriptPath = Join-Path $PSScriptRoot 'run_winui_scenario.ps1'
$largeDataScriptPath = Join-Path $PSScriptRoot 'run_phase4_large_data.ps1'
$expectedLargeDataArchitecture = if ($Platform -eq 'x64') { 'x64' } else { 'x86' }
$memoryScriptPath = Get-AbsolutePath -Path (
    Join-Path $PSScriptRoot '..\..\measure_windows_winui_memory.ps1'
)

$plan = [ordered]@{
    format = 'classmngr-winui-phase4-evidence-plan-v1'
    platform = $Platform
    stageDirectory = $stagePath
    executable = $executablePath
    outputDirectory = $requestedOutputPath
    checks = @(
        [ordered]@{
            name = 'phase4-gallery-scenario'
            enabled = $true
            skipped = $SkipVisualScenarios.IsPresent
            script = $scenarioScriptPath
            scenarioName = 'phase4-gallery'
        },
        [ordered]@{
            name = 'memory-budget'
            enabled = $true
            skipped = $SkipMemory.IsPresent
            script = $memoryScriptPath
        },
        [ordered]@{
            name = 'phase4-semantic-test'
            enabled = $true
            argument = '--phase4-semantic-test'
        },
        [ordered]@{
            name = 'phase4-large-data'
            enabled = $true
            script = $largeDataScriptPath
            repetitions = 3
            expectedArchitecture = $expectedLargeDataArchitecture
            expectedConfiguration = $Configuration
            requiredFullGate = $true
        }
    )
    manualChecks = @(
        [ordered]@{
            name = 'korean-ime-composition'
            status = $KoreanImeStatus
            automated = $false
        },
        [ordered]@{
            name = 'dpi-100-to-300-percent'
            status = $DpiStatus
            automated = $false
        },
        [ordered]@{
            name = 'touch-interaction'
            status = 'not-applicable'
            automated = $false
        },
        [ordered]@{
            name = 'high-contrast'
            status = 'not-applicable'
            automated = $false
        },
        [ordered]@{
            name = 'accessibility-automation'
            status = 'not-applicable'
            automated = $false
        }
    )
}

if ($PlanOnly) {
    $plan | ConvertTo-Json -Depth 10
    return
}

if (-not (Test-Path -LiteralPath $stagePath -PathType Container)) {
    throw "WinUI stage directory was not found: $stagePath"
}
if (-not (Test-Path -LiteralPath $executablePath -PathType Leaf)) {
    throw "WinUI executable was not found: $executablePath"
}
if (-not (Test-Path -LiteralPath $scenarioScriptPath -PathType Leaf)) {
    throw "WinUI scenario helper was not found: $scenarioScriptPath"
}
if (-not (Test-Path -LiteralPath $largeDataScriptPath -PathType Leaf)) {
    throw "WinUI large-data helper was not found: $largeDataScriptPath"
}
if (-not (Test-Path -LiteralPath $memoryScriptPath -PathType Leaf)) {
    throw "WinUI memory helper was not found: $memoryScriptPath"
}

if (-not (Test-Path -LiteralPath $requestedOutputPath)) {
    New-Item -ItemType Directory -Path $requestedOutputPath -Force | Out-Null
}
elseif (-not (Test-Path -LiteralPath $requestedOutputPath -PathType Container)) {
    throw "OutputDirectory is not a directory: $requestedOutputPath"
}

$resolvedStagePath = (Resolve-Path -LiteralPath $stagePath).Path
$resolvedExecutablePath = (Resolve-Path -LiteralPath $executablePath).Path
$resolvedOutputPath = (Resolve-Path -LiteralPath $requestedOutputPath).Path
$evidencePath = Get-UniqueEvidenceDirectory -ParentDirectory $resolvedOutputPath
$summaryPath = Join-Path $evidencePath 'phase4-evidence.json'

$checks = [System.Collections.Generic.List[object]]::new()

if ($SkipVisualScenarios) {
    $checks.Add((New-Check `
        -Name 'phase4-gallery-scenario' `
        -Status 'skipped' `
        -Enabled $true)) | Out-Null
}
else {
    $scenarioFailure = ''
    $scenarioMetadataPath = Join-Path $evidencePath 'phase4-gallery.json'
    try {
        & $scenarioScriptPath `
            -Executable $resolvedExecutablePath `
            -OutputDirectory $evidencePath `
            -ScenarioName 'phase4-gallery' | Out-Host
        $scenarioStatus = 'passed'
    }
    catch {
        $scenarioStatus = 'failed'
        $scenarioFailure = $_.Exception.Message
    }
    $checks.Add((New-Check `
        -Name 'phase4-gallery-scenario' `
        -Status $scenarioStatus `
        -Enabled $true `
        -Path $scenarioMetadataPath `
        -Failure $scenarioFailure)) | Out-Null
}

if ($SkipMemory) {
    $checks.Add((New-Check `
        -Name 'memory-budget' `
        -Status 'skipped' `
        -Enabled $true)) | Out-Null
}
else {
    $memoryFailure = ''
    $memoryReportPath = Join-Path $evidencePath "phase4-memory-$Platform.json"
    try {
        & $memoryScriptPath `
            -StageDirectory $resolvedStagePath `
            -Platform $Platform `
            -ReportPath $memoryReportPath | Out-Host
        $memoryStatus = 'passed'
    }
    catch {
        $memoryStatus = 'failed'
        $memoryFailure = $_.Exception.Message
    }
    $checks.Add((New-Check `
        -Name 'memory-budget' `
        -Status $memoryStatus `
        -Enabled $true `
        -Path $memoryReportPath `
        -Failure $memoryFailure)) | Out-Null
}

$semanticExitCode = $null
$semanticFailure = ''
$semanticStatus = 'failed'
$semanticProcess = $null
try {
    $semanticProcess = Start-Process `
        -FilePath $resolvedExecutablePath `
        -WorkingDirectory $resolvedStagePath `
        -ArgumentList @('--phase4-semantic-test') `
        -WindowStyle Hidden `
        -PassThru
    if (-not $semanticProcess.WaitForExit(120000)) {
        throw 'Phase 4 semantic test did not exit within 120000 ms.'
    }
    $semanticProcess.Refresh()
    $semanticExitCode = $semanticProcess.ExitCode
    if ($semanticExitCode -eq 0) {
        $semanticStatus = 'passed'
    }
    else {
        $semanticFailure = "Process exited with code $semanticExitCode."
    }
}
catch {
    $semanticFailure = $_.Exception.Message
}
finally {
    if ($null -ne $semanticProcess) {
        try {
            $semanticProcess.Refresh()
            if (-not $semanticProcess.HasExited) {
                $semanticProcess.Kill()
                $semanticProcess.WaitForExit(5000)
            }
        }
        catch {
            if ([string]::IsNullOrWhiteSpace($semanticFailure)) {
                $semanticFailure = "Semantic process cleanup failed: $($_.Exception.Message)"
            }
            $semanticStatus = 'failed'
        }
    }
}
$checks.Add((New-Check `
    -Name 'phase4-semantic-test' `
    -Status $semanticStatus `
    -Enabled $true `
    -Path $resolvedExecutablePath `
    -ExitCode $semanticExitCode `
    -Failure $semanticFailure)) | Out-Null

$largeDataSummaryPath = Join-Path $evidencePath 'phase4-large-data-summary.json'
if ($SkipLargeData) {
    $checks.Add((New-Check `
        -Name 'phase4-large-data' `
        -Status 'skipped' `
        -Enabled $true `
        -Path $largeDataSummaryPath `
        -Failure 'Required large-data gate was explicitly skipped.')) | Out-Null
}
else {
    $largeDataFailure = ''
    try {
        & $largeDataScriptPath `
            -Executable $resolvedExecutablePath `
            -OutputDirectory $evidencePath `
            -Repetitions 3 `
            -ExpectedArchitecture $expectedLargeDataArchitecture `
            -ExpectedConfiguration $Configuration | Out-Host
        if (-not (Test-Path -LiteralPath $largeDataSummaryPath -PathType Leaf)) {
            throw "Large-data runner did not produce its summary: $largeDataSummaryPath"
        }
        $largeDataSummary = Get-Content -LiteralPath $largeDataSummaryPath -Raw | ConvertFrom-Json
        $largeDataOverall = $largeDataSummary.overall
        if ($null -eq $largeDataOverall) {
            throw "Large-data runner summary has no overall result: $largeDataSummaryPath"
        }
        $largeDataStatus = if ([bool]$largeDataOverall.fullGatePassed -and [string]$largeDataOverall.status -eq 'passed') {
            'passed'
        }
        else {
            [string]$largeDataOverall.status
        }
        if ($largeDataStatus -notin @('passed', 'incomplete', 'failed')) {
            throw "Large-data runner summary has unsupported overall status '$largeDataStatus': $largeDataSummaryPath"
        }
        if ($largeDataStatus -ne 'passed') {
            $largeDataFailure = "Large-data runner overall status is '$largeDataStatus'; fullGatePassed=$($largeDataOverall.fullGatePassed)."
        }
    }
    catch {
        $largeDataStatus = 'failed'
        $largeDataFailure = $_.Exception.Message
    }
    $checks.Add((New-Check `
        -Name 'phase4-large-data' `
        -Status $largeDataStatus `
        -Enabled $true `
        -Path $largeDataSummaryPath `
        -Failure $largeDataFailure)) | Out-Null
}

$failedChecks = @($checks | Where-Object { $_.enabled -and $_.status -eq 'failed' })
$incompleteChecks = @($checks | Where-Object { $_.enabled -and $_.status -ne 'passed' -and $_.status -ne 'failed' })
$requiredManualChecks = @(
    [ordered]@{ name = 'korean-ime-composition'; status = $KoreanImeStatus },
    [ordered]@{ name = 'dpi-100-to-300-percent'; status = $DpiStatus }
)
$manualFailedChecks = @($requiredManualChecks | Where-Object { $_.status -eq 'failed' })
$manualIncompleteChecks = @($requiredManualChecks | Where-Object { $_.status -ne 'passed' -and $_.status -ne 'failed' })
$automatedStatus = if ($failedChecks.Count -gt 0) { 'failed' } elseif ($incompleteChecks.Count -gt 0) { 'incomplete' } else { 'passed' }
$manualStatus = if ($manualFailedChecks.Count -gt 0) { 'failed' } elseif ($manualIncompleteChecks.Count -gt 0) { 'incomplete' } else { 'passed' }
$fullStatus = if ($automatedStatus -eq 'failed' -or $manualStatus -eq 'failed') { 'failed' } elseif ($automatedStatus -ne 'passed' -or $manualStatus -ne 'passed') { 'incomplete' } else { 'passed' }
$summary = [ordered]@{
    format = 'classmngr-winui-phase4-evidence-v1'
    collectedAtUtc = [DateTime]::UtcNow.ToString('o')
    overall = [ordered]@{
        status = $fullStatus
        passed = $fullStatus -eq 'passed'
        automatedStatus = $automatedStatus
        manualStatus = $manualStatus
        fullGatePassed = $fullStatus -eq 'passed'
        failedAutomatedChecks = $failedChecks.Count
        incompleteAutomatedChecks = $incompleteChecks.Count
        failedManualChecks = $manualFailedChecks.Count
        incompleteManualChecks = $manualIncompleteChecks.Count
    }
    host = [ordered]@{
        operatingSystem = [Environment]::OSVersion.VersionString
        architecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
        powershell = $PSVersionTable.PSVersion.ToString()
    }
    build = [ordered]@{
        platform = $Platform
        configuration = $Configuration
        stageDirectory = $resolvedStagePath
        executable = $resolvedExecutablePath
    }
    output = [ordered]@{
        directory = $evidencePath
        summary = $summaryPath
    }
    automatedChecks = @($checks.ToArray())
    manualChecks = @(
        [ordered]@{
            name = 'korean-ime-composition'
            status = $KoreanImeStatus
            automated = $false
            note = 'Manual result supplied from a real interactive Korean IME composition session.'
        },
        [ordered]@{
            name = 'dpi-100-to-300-percent'
            status = $DpiStatus
            automated = $false
            note = 'Manual result supplied from an interactive DPI session.'
        },
        [ordered]@{
            name = 'touch-interaction'
            status = 'not-applicable'
            automated = $false
            note = 'Touch hardware is not a target feature for this program.'
        },
        [ordered]@{
            name = 'high-contrast'
            status = 'not-applicable'
            automated = $false
            note = 'High-contrast support is not a target feature for this program.'
        },
        [ordered]@{
            name = 'accessibility-automation'
            status = 'not-applicable'
            automated = $false
            note = 'Accessibility automation is not a target feature for this program.'
        }
    )
}

if (Test-Path -LiteralPath $summaryPath) {
    throw "Refusing to overwrite evidence summary: $summaryPath"
}
[System.IO.File]::WriteAllText(
    $summaryPath,
    ($summary | ConvertTo-Json -Depth 16) + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false)
)

Write-Host "Phase 4 evidence summary: $summaryPath"
if ($fullStatus -ne 'passed') {
    throw "Phase 4 evidence collection is $fullStatus (automated=$automatedStatus, manual=$manualStatus). Summary: $summaryPath"
}
