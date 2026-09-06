[CmdletBinding()]
param(
    [string]$OutputDirectory = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$validatorPath = Join-Path $PSScriptRoot 'validate_phase4_large_data_artifacts.ps1'
if (-not (Test-Path -LiteralPath $validatorPath -PathType Leaf)) {
    throw "Large-data validator was not found: $validatorPath"
}

function Get-AbsolutePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) { return [System.IO.Path]::GetFullPath($Path) }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location).Path $Path))
}

function New-ValidReport {
    $definitions = @(
        @{ name = 'class-list'; sourceRows = 10000; columns = 1 },
        @{ name = 'roster'; sourceRows = 5000; columns = 1 },
        @{ name = 'schedule'; sourceRows = 2000; columns = 1 },
        @{ name = 'speaking'; sourceRows = 10000; columns = 8 }
    )
    $workloads = foreach ($definition in $definitions) {
        $baseline = 100000000
        $checkpoints = @(
            [ordered]@{ name = 'initial'; viewportRows = 50; realizedRows = 80; realizedCells = 80 * $definition.columns; liveRowViewModels = 80; liveCellViewModels = 80 * $definition.columns; activeEditors = 0; frameSampleCount = 30; frameP95Ms = 10.0; frameMaxMs = 20.0; privateBytes = $baseline + 1000000; privateWorkingSetBytes = $baseline + 2000000; privateWorkingSetAvailable = $true; nativeAllocationBytes = 1000000; nativeAllocationCount = 10; functionalPassed = $true; performancePassed = $true; passed = $true },
            [ordered]@{ name = 'scrolled'; viewportRows = 50; realizedRows = 90; realizedCells = 90 * $definition.columns; liveRowViewModels = 90; liveCellViewModels = 90 * $definition.columns; activeEditors = 0; frameSampleCount = 30; frameP95Ms = 11.0; frameMaxMs = 21.0; privateBytes = $baseline + 2000000; privateWorkingSetBytes = $baseline + 3000000; privateWorkingSetAvailable = $true; nativeAllocationBytes = 1100000; nativeAllocationCount = 11; functionalPassed = $true; performancePassed = $true; passed = $true },
            [ordered]@{ name = 'edited'; viewportRows = 50; realizedRows = 100; realizedCells = 100 * $definition.columns; liveRowViewModels = 100; liveCellViewModels = 100 * $definition.columns; activeEditors = 1; frameSampleCount = 30; frameP95Ms = 12.0; frameMaxMs = 22.0; privateBytes = $baseline + 3000000; privateWorkingSetBytes = $baseline + 4000000; privateWorkingSetAvailable = $true; nativeAllocationBytes = 1200000; nativeAllocationCount = 12; functionalPassed = $true; performancePassed = $true; passed = $true },
            [ordered]@{ name = 'released'; viewportRows = 0; realizedRows = 0; realizedCells = 0; liveRowViewModels = 0; liveCellViewModels = 0; activeEditors = 0; frameSampleCount = 0; frameP95Ms = 0.0; frameMaxMs = 0.0; privateBytes = $baseline + 2500000; privateWorkingSetBytes = $baseline + 3000000; privateWorkingSetAvailable = $true; nativeAllocationBytes = 0; nativeAllocationCount = 0; functionalPassed = $true; performancePassed = $true; passed = $true }
        )
        [ordered]@{ name = $definition.name; sourceRows = $definition.sourceRows; columns = $definition.columns; baselinePrivateBytes = $baseline; baselinePrivateWorkingSetBytes = $baseline; baselinePrivateWorkingSetAvailable = $true; functionalPassed = $true; performancePassed = $true; passed = $true; checkpoints = $checkpoints }
    }
    return [ordered]@{
        format = 'classmngr.winui.phase4.large-data.v1'
        runId = '00000000-0000-0000-0000-000000000001'
        processId = 4242
        architecture = 'x64'
        configuration = 'Release'
        overallStatus = 'passed'
        functionalPassed = $true
        performancePassed = $true
        metricsScope = [ordered]@{
            allocation = 'Native C++ application-data allocations only; no VM/view-model allocations are present; full process heap allocations require a separate trace.'
            frame = 'At least 30 active viewport frame samples; frameP95Ms and frameMaxMs are measured sample timings.'
            managed = 'N/A: native C++ build has no managed allocation metric.'
        }
        budgets = [ordered]@{ realizedRowMultiplier = 3; frameP95Ms = 16.7; frameMaxMs = 100; privateBytesDeltaMax = 67108864; nativeAllocationBytesMax = 16777216 }
        workloads = $workloads
    }
}

function Write-Fixture {
    param(
        [Parameter(Mandatory = $true)][object]$Report,
        [Parameter(Mandatory = $true)][string]$Name
    )
    $path = Join-Path $fixtureDirectory "$Name.json"
    [System.IO.File]::WriteAllText($path, ($Report | ConvertTo-Json -Depth 20) + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    return $path
}

function Assert-Accepts {
    param([Parameter(Mandatory = $true)][string]$Name, [Parameter(Mandatory = $true)][string]$Path)
    try {
        & $validatorPath -ReportPath $Path -ExpectedProcessId 4242 -ExpectedRunId '00000000-0000-0000-0000-000000000001' -ExpectedArchitecture x64 -ExpectedConfiguration Release | Out-Null
        return [ordered]@{ name = $Name; expected = 'accepted'; actual = 'accepted'; passed = $true }
    }
    catch {
        return [ordered]@{ name = $Name; expected = 'accepted'; actual = 'rejected'; passed = $false; detail = $_.Exception.Message }
    }
}

function Assert-Rejects {
    param([Parameter(Mandatory = $true)][string]$Name, [Parameter(Mandatory = $true)][string]$Path, [int]$ExpectedProcessId = 4242, [string]$ExpectedRunId = '00000000-0000-0000-0000-000000000001')
    try {
        & $validatorPath -ReportPath $Path -ExpectedProcessId $ExpectedProcessId -ExpectedRunId $ExpectedRunId -ExpectedArchitecture x64 -ExpectedConfiguration Release | Out-Null
        return [ordered]@{ name = $Name; expected = 'rejected'; actual = 'accepted'; passed = $false }
    }
    catch {
        return [ordered]@{ name = $Name; expected = 'rejected'; actual = 'rejected'; passed = $true }
    }
}

function Assert-AcceptsFunctional {
    param([Parameter(Mandatory = $true)][string]$Name, [Parameter(Mandatory = $true)][string]$Path)
    try {
        & $validatorPath -ReportPath $Path -ExpectedProcessId 4242 -ExpectedRunId '00000000-0000-0000-0000-000000000001' -ExpectedArchitecture x86 -ExpectedConfiguration Debug | Out-Null
        return [ordered]@{ name = $Name; expected = 'accepted-functional'; actual = 'accepted-functional'; passed = $true }
    }
    catch {
        return [ordered]@{ name = $Name; expected = 'accepted-functional'; actual = 'rejected'; passed = $false; detail = $_.Exception.Message }
    }
}

$baseOutput = if ([string]::IsNullOrWhiteSpace($OutputDirectory)) { [System.IO.Path]::GetTempPath() } else { Get-AbsolutePath $OutputDirectory }
New-Item -ItemType Directory -Force -Path $baseOutput | Out-Null
$fixtureDirectory = Join-Path $baseOutput "phase4-large-data-validator-$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $fixtureDirectory | Out-Null
$results = [System.Collections.Generic.List[object]]::new()
try {
    $results.Add((Assert-Accepts -Name 'valid-release-fixture' -Path (Write-Fixture (New-ValidReport) 'valid'))) | Out-Null

    $report = New-ValidReport
    $results.Add((Assert-Rejects -Name 'wrong-pid' -Path (Write-Fixture $report 'wrong-pid') -ExpectedProcessId 9999)) | Out-Null
    $results.Add((Assert-Rejects -Name 'wrong-run-id' -Path (Write-Fixture $report 'wrong-run-id') -ExpectedRunId '00000000-0000-0000-0000-000000000099')) | Out-Null

    $report = New-ValidReport
    $report.workloads[0].checkpoints = @($report.workloads[0].checkpoints | Where-Object { $_.name -ne 'released' })
    $results.Add((Assert-Rejects -Name 'missing-checkpoint' -Path (Write-Fixture $report 'missing-checkpoint'))) | Out-Null

    $report = New-ValidReport
    $report.workloads[0].checkpoints[0].realizedRows = 150
    $report.workloads[0].checkpoints[0].realizedCells = 150
    $results.Add((Assert-Rejects -Name 'unbounded-rows' -Path (Write-Fixture $report 'unbounded-rows'))) | Out-Null

    $report = New-ValidReport
    $report.workloads[0].checkpoints[0].frameP95Ms = '10'
    $results.Add((Assert-Rejects -Name 'numeric-string' -Path (Write-Fixture $report 'numeric-string'))) | Out-Null

    $report = New-ValidReport
    $report.workloads[0].checkpoints[0].frameP95Ms = 'NaN'
    $results.Add((Assert-Rejects -Name 'nonfinite-metric' -Path (Write-Fixture $report 'nonfinite-metric'))) | Out-Null

    $report = New-ValidReport
    $report.workloads[0].checkpoints[0].viewportRows = 50.5
    $results.Add((Assert-Rejects -Name 'fractional-count' -Path (Write-Fixture $report 'fractional-count'))) | Out-Null

    $report = New-ValidReport
    $report.workloads[0].checkpoints[0].nativeAllocationBytes = -1
    $results.Add((Assert-Rejects -Name 'negative-metric' -Path (Write-Fixture $report 'negative-metric'))) | Out-Null

    $report = New-ValidReport
    $report.overallStatus = 'failed'
    $report.functionalPassed = $false
    $results.Add((Assert-Rejects -Name 'failed-report' -Path (Write-Fixture $report 'failed-report'))) | Out-Null

    $report = New-ValidReport
    $report.workloads[1] = $report.workloads[0]
    $results.Add((Assert-Rejects -Name 'duplicate-workload' -Path (Write-Fixture $report 'duplicate-workload'))) | Out-Null

    $report = New-ValidReport
    $report.workloads[0].sourceRows = 9999
    $results.Add((Assert-Rejects -Name 'wrong-fixture-size' -Path (Write-Fixture $report 'wrong-fixture-size'))) | Out-Null

    $report = New-ValidReport
    $report.workloads[0].checkpoints[0].realizedRows = 0
    $report.workloads[0].checkpoints[0].realizedCells = 0
    $results.Add((Assert-Rejects -Name 'empty-realized-surface' -Path (Write-Fixture $report 'empty-realized-surface'))) | Out-Null

    $report = New-ValidReport
    $report.workloads[0].checkpoints[3].realizedRows = 1
    $results.Add((Assert-Rejects -Name 'released-controls-retained' -Path (Write-Fixture $report 'released-controls-retained'))) | Out-Null

    $report = New-ValidReport
    $report.workloads[0].checkpoints[0].activeEditors = 2
    $results.Add((Assert-Rejects -Name 'too-many-editors' -Path (Write-Fixture $report 'too-many-editors'))) | Out-Null

    $report = New-ValidReport
    $report.functionalPassed = $false
    $report.overallStatus = 'failed'
    $report.workloads[0].checkpoints[0].functionalPassed = $false
    $report.workloads[0].checkpoints[0].passed = $false
    $results.Add((Assert-Rejects -Name 'failed-checkpoint' -Path (Write-Fixture $report 'failed-checkpoint'))) | Out-Null

    $malformedPath = Join-Path $fixtureDirectory 'malformed.json'
    [System.IO.File]::WriteAllText($malformedPath, '{"format":', [System.Text.UTF8Encoding]::new($false))
    $results.Add((Assert-Rejects -Name 'malformed-json' -Path $malformedPath)) | Out-Null
    $results.Add((Assert-Rejects -Name 'missing-report' -Path (Join-Path $fixtureDirectory 'missing.json'))) | Out-Null

    $nonfinitePath = Write-Fixture (New-ValidReport) 'nonfinite-json-number'
    $nonfiniteText = [System.Text.RegularExpressions.Regex]::Replace(
        [System.IO.File]::ReadAllText($nonfinitePath),
        '"frameP95Ms"\s*:\s*10(?:\.0)?',
        '"frameP95Ms": NaN',
        [System.Text.RegularExpressions.RegexOptions]::None
    )
    [System.IO.File]::WriteAllText($nonfinitePath, $nonfiniteText, [System.Text.UTF8Encoding]::new($false))
    $results.Add((Assert-Rejects -Name 'nonfinite-json-number' -Path $nonfinitePath)) | Out-Null

    $report = New-ValidReport
    $report.architecture = 'x86'
    $report.configuration = 'Debug'
    $report.performancePassed = $false
    $report.overallStatus = 'passed'
    $report.workloads[0].performancePassed = $false
    $report.workloads[0].checkpoints[0].performancePassed = $false
    $report.workloads[0].checkpoints[0].passed = $true
    $report.workloads[0].checkpoints[0].frameP95Ms = 20.0
    $results.Add((Assert-AcceptsFunctional -Name 'debug-performance-failure-functional-lane' -Path (Write-Fixture $report 'debug-performance-failure'))) | Out-Null

    $report = New-ValidReport
    $report.performancePassed = $false
    $report.overallStatus = 'failed'
    $report.workloads[0].checkpoints[0].performancePassed = $false
    $report.workloads[0].checkpoints[0].passed = $false
    $results.Add((Assert-Rejects -Name 'claimed-performance-failure-without-measurement' -Path (Write-Fixture $report 'claimed-performance-failure-without-measurement'))) | Out-Null

    $failed = @($results | Where-Object { -not $_.passed })
    $results | ConvertTo-Json -Depth 8 | Write-Host
    if ($failed.Count -gt 0) {
        throw "Phase 4 large-data validator self-test failed $($failed.Count) case(s)."
    }
    Write-Host "Phase 4 large-data validator self-test passed: $($results.Count) cases."
}
finally {
    if (Test-Path -LiteralPath $fixtureDirectory) {
        $resolvedBaseOutput = [System.IO.Path]::GetFullPath($baseOutput)
        $resolvedFixtureDirectory = [System.IO.Path]::GetFullPath($fixtureDirectory)
        $baseWithSeparator = $resolvedBaseOutput.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
        if ($resolvedFixtureDirectory -eq $resolvedBaseOutput -or
            -not $resolvedFixtureDirectory.StartsWith($baseWithSeparator, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to clean fixture directory outside test output root: $resolvedFixtureDirectory"
        }
        Remove-Item -LiteralPath $resolvedFixtureDirectory -Recurse -Force
    }
}
