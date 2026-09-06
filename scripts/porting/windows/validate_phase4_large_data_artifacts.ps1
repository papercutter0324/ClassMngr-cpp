[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ReportPath,

    [int]$ExpectedProcessId = 0,

    [string]$ExpectedRunId = '',

    [ValidateSet('x64', 'x86')]
    [string]$ExpectedArchitecture = '',

    [ValidateSet('Debug', 'Release')]
    [string]$ExpectedConfiguration = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-RequiredProperty {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Object,

        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if ($null -eq $Object) {
        throw "$Description is missing its '$Name' property."
    }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) {
        throw "$Description is missing its '$Name' property."
    }
    return $property.Value
}

function Convert-ToFiniteNonNegativeNumber {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Value,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if ($Value -is [bool]) {
        throw "$Description must be a finite non-negative number; boolean was supplied."
    }
    if ($Value -isnot [byte] -and $Value -isnot [sbyte] -and
        $Value -isnot [int16] -and $Value -isnot [uint16] -and
        $Value -isnot [int32] -and $Value -isnot [uint32] -and
        $Value -isnot [int64] -and $Value -isnot [uint64] -and
        $Value -isnot [single] -and $Value -isnot [double] -and
        $Value -isnot [decimal]) {
        throw "$Description must be a JSON number, not $($Value.GetType().Name)."
    }
    $number = [double]$Value
    if ([double]::IsNaN($number) -or [double]::IsInfinity($number) -or $number -lt 0) {
        throw "$Description must be a finite non-negative number; value '$Value' is invalid."
    }
    return $number
}

function Convert-ToNonNegativeInteger {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Value,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    $number = Convert-ToFiniteNonNegativeNumber -Value $Value -Description $Description
    if ($number -ne [math]::Floor($number) -or $number -gt [int64]::MaxValue) {
        throw "$Description must be a non-negative integer; value '$Value' is invalid."
    }
    return [int64]$number
}

function Convert-ToPositiveNumber {
    param(
        [Parameter(Mandatory = $true)][object]$Value,
        [Parameter(Mandatory = $true)][string]$Description
    )
    $number = Convert-ToFiniteNonNegativeNumber -Value $Value -Description $Description
    if ($number -le 0) {
        throw "$Description must be greater than zero."
    }
    return $number
}

function Assert-String {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Value,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )
    if ($Value -isnot [string] -or [string]::IsNullOrWhiteSpace([string]$Value)) {
        throw "$Description must be a non-empty string."
    }
}

function Assert-Boolean {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Value,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )
    if ($Value -isnot [bool]) {
        throw "$Description must be a boolean."
    }
}

if (-not [System.IO.Path]::IsPathRooted($ReportPath)) {
    throw "ReportPath must be absolute: $ReportPath"
}
$resolvedReportPath = [System.IO.Path]::GetFullPath($ReportPath)
if (-not (Test-Path -LiteralPath $resolvedReportPath -PathType Leaf)) {
    throw "Phase 4 large-data report is missing: $resolvedReportPath"
}

try {
    $report = Get-Content -LiteralPath $resolvedReportPath -Raw | ConvertFrom-Json
}
catch {
    throw "Phase 4 large-data report is not valid JSON: $resolvedReportPath ($($_.Exception.Message))"
}

$format = Get-RequiredProperty -Object $report -Name 'format' -Description 'Large-data report'
if ($format -ne 'classmngr.winui.phase4.large-data.v1') {
    throw "Unsupported Phase 4 large-data report format '$format': $resolvedReportPath"
}

$runId = Get-RequiredProperty -Object $report -Name 'runId' -Description 'Large-data report'
Assert-String -Value $runId -Description 'Large-data report runId'
if (-not [string]::IsNullOrWhiteSpace($ExpectedRunId) -and $runId -ne $ExpectedRunId) {
    throw "Large-data report runId '$runId' does not match expected runId '$ExpectedRunId': $resolvedReportPath"
}

$processId = Convert-ToNonNegativeInteger `
    -Value (Get-RequiredProperty -Object $report -Name 'processId' -Description 'Large-data report') `
    -Description 'Large-data report processId'
if ($processId -le 0) {
    throw "Large-data report processId must be greater than zero: $resolvedReportPath"
}
if ($ExpectedProcessId -gt 0 -and $processId -ne $ExpectedProcessId) {
    throw "Large-data report processId '$processId' does not match expected process ID '$ExpectedProcessId': $resolvedReportPath"
}

$architecture = Get-RequiredProperty -Object $report -Name 'architecture' -Description 'Large-data report'
if ($architecture -cnotin @('x64', 'x86')) {
    throw "Large-data report architecture must be x64 or x86: $resolvedReportPath"
}
if (-not [string]::IsNullOrWhiteSpace($ExpectedArchitecture) -and $architecture -cne $ExpectedArchitecture) {
    throw "Large-data report architecture '$architecture' does not match expected '$ExpectedArchitecture': $resolvedReportPath"
}

$configuration = Get-RequiredProperty -Object $report -Name 'configuration' -Description 'Large-data report'
if ($configuration -cnotin @('Debug', 'Release')) {
    throw "Large-data report configuration must be Debug or Release: $resolvedReportPath"
}
if (-not [string]::IsNullOrWhiteSpace($ExpectedConfiguration) -and $configuration -cne $ExpectedConfiguration) {
    throw "Large-data report configuration '$configuration' does not match expected '$ExpectedConfiguration': $resolvedReportPath"
}

$overallStatus = Get-RequiredProperty -Object $report -Name 'overallStatus' -Description 'Large-data report'
if ($overallStatus -cnotin @('passed', 'failed')) {
    throw "Large-data report overallStatus must be passed or failed: $resolvedReportPath"
}
$rootFunctionalPassed = Get-RequiredProperty -Object $report -Name 'functionalPassed' -Description 'Large-data report'
$rootPerformancePassed = Get-RequiredProperty -Object $report -Name 'performancePassed' -Description 'Large-data report'
Assert-Boolean -Value $rootFunctionalPassed -Description 'Large-data report functionalPassed'
Assert-Boolean -Value $rootPerformancePassed -Description 'Large-data report performancePassed'

$metricsScope = Get-RequiredProperty -Object $report -Name 'metricsScope' -Description 'Large-data report'
foreach ($scopeName in @('allocation', 'frame', 'managed')) {
    $scopeValue = Get-RequiredProperty -Object $metricsScope -Name $scopeName -Description 'Large-data report metricsScope'
    Assert-String -Value $scopeValue -Description "Large-data report metricsScope.$scopeName"
}
$allocationScope = [string](Get-RequiredProperty -Object $metricsScope -Name 'allocation' -Description 'Large-data report metricsScope')
if ($allocationScope -notmatch '(?i)(no|none|without|not\s+(?:tracked|allocated|counted)|N/A).{0,80}(VM|view[\s-]?model)|(VM|view[\s-]?model).{0,80}(no|none|without|not\s+(?:tracked|allocated|counted)|N/A)') {
    throw "Large-data metricsScope.allocation must explicitly state that no view-model/VM allocations are present or tracked: $resolvedReportPath"
}

$budgets = Get-RequiredProperty -Object $report -Name 'budgets' -Description 'Large-data report'
$realizedRowMultiplier = Convert-ToFiniteNonNegativeNumber `
    -Value (Get-RequiredProperty -Object $budgets -Name 'realizedRowMultiplier' -Description 'Large-data report budgets') `
    -Description 'budgets.realizedRowMultiplier'
$frameP95Budget = Convert-ToFiniteNonNegativeNumber `
    -Value (Get-RequiredProperty -Object $budgets -Name 'frameP95Ms' -Description 'Large-data report budgets') `
    -Description 'budgets.frameP95Ms'
$frameMaxBudget = Convert-ToFiniteNonNegativeNumber `
    -Value (Get-RequiredProperty -Object $budgets -Name 'frameMaxMs' -Description 'Large-data report budgets') `
    -Description 'budgets.frameMaxMs'
$privateBytesDeltaBudget = Convert-ToFiniteNonNegativeNumber `
    -Value (Get-RequiredProperty -Object $budgets -Name 'privateBytesDeltaMax' -Description 'Large-data report budgets') `
    -Description 'budgets.privateBytesDeltaMax'
$nativeAllocationBudget = Convert-ToFiniteNonNegativeNumber `
    -Value (Get-RequiredProperty -Object $budgets -Name 'nativeAllocationBytesMax' -Description 'Large-data report budgets') `
    -Description 'budgets.nativeAllocationBytesMax'
if ($realizedRowMultiplier -ne 3 -or $frameP95Budget -ne 16.7 -or $frameMaxBudget -ne 100 -or
    $privateBytesDeltaBudget -ne 67108864 -or $nativeAllocationBudget -ne 16777216) {
    throw "Large-data report budgets do not match the Phase 4 contract: $resolvedReportPath"
}

$expectedWorkloads = [ordered]@{
    'class-list' = @{ sourceRows = 10000; columns = 1 }
    'roster' = @{ sourceRows = 5000; columns = 1 }
    'schedule' = @{ sourceRows = 2000; columns = 1 }
    'speaking' = @{ sourceRows = 10000; columns = 8 }
}
$workloads = @(Get-RequiredProperty -Object $report -Name 'workloads' -Description 'Large-data report')
if ($workloads.Count -ne $expectedWorkloads.Count) {
    throw "Large-data report must contain exactly $($expectedWorkloads.Count) workloads; found $($workloads.Count): $resolvedReportPath"
}

$expectedCheckpointNames = @('initial', 'scrolled', 'edited', 'released')
$performanceFailures = [System.Collections.Generic.List[string]]::new()
$functionalFailureCount = 0
$performanceFailureCount = 0
$seenWorkloads = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($workload in $workloads) {
    $workloadName = Get-RequiredProperty -Object $workload -Name 'name' -Description 'Large-data workload'
    Assert-String -Value $workloadName -Description 'Large-data workload name'
    if (-not (@('class-list', 'roster', 'schedule', 'speaking') -ccontains $workloadName) -or -not $seenWorkloads.Add($workloadName)) {
        throw "Unexpected or duplicate large-data workload '$workloadName': $resolvedReportPath"
    }
    $expectation = $expectedWorkloads[$workloadName]
    $sourceRows = Convert-ToNonNegativeInteger `
        -Value (Get-RequiredProperty -Object $workload -Name 'sourceRows' -Description "Workload '$workloadName'") `
        -Description "Workload '$workloadName' sourceRows"
    $columns = Convert-ToNonNegativeInteger `
        -Value (Get-RequiredProperty -Object $workload -Name 'columns' -Description "Workload '$workloadName'") `
        -Description "Workload '$workloadName' columns"
    if ($sourceRows -ne $expectation.sourceRows -or $columns -ne $expectation.columns) {
        throw "Workload '$workloadName' has wrong fixture size (sourceRows=$sourceRows, columns=$columns): expected $($expectation.sourceRows)x$($expectation.columns)."
    }
    $baselinePrivateBytes = Convert-ToFiniteNonNegativeNumber `
        -Value (Get-RequiredProperty -Object $workload -Name 'baselinePrivateBytes' -Description "Workload '$workloadName'") `
        -Description "Workload '$workloadName' baselinePrivateBytes"
    $baselinePrivateWorkingSetBytes = Convert-ToFiniteNonNegativeNumber `
        -Value (Get-RequiredProperty -Object $workload -Name 'baselinePrivateWorkingSetBytes' -Description "Workload '$workloadName'") `
        -Description "Workload '$workloadName' baselinePrivateWorkingSetBytes"
    $baselinePrivateWorkingSetAvailable = Get-RequiredProperty -Object $workload -Name 'baselinePrivateWorkingSetAvailable' -Description "Workload '$workloadName'"
    Assert-Boolean -Value $baselinePrivateWorkingSetAvailable -Description "Workload '$workloadName' baselinePrivateWorkingSetAvailable"
    if (-not [bool]$baselinePrivateWorkingSetAvailable) {
        throw "Workload '$workloadName' baseline private working-set measurement is unavailable."
    }
    if ($baselinePrivateBytes -le 0 -or $baselinePrivateWorkingSetBytes -le 0) {
        throw "Workload '$workloadName' baseline process memory must be greater than zero."
    }

    $workloadFunctionalPassed = Get-RequiredProperty -Object $workload -Name 'functionalPassed' -Description "Workload '$workloadName'"
    $workloadPerformancePassed = Get-RequiredProperty -Object $workload -Name 'performancePassed' -Description "Workload '$workloadName'"
    $workloadPassed = Get-RequiredProperty -Object $workload -Name 'passed' -Description "Workload '$workloadName'"
    Assert-Boolean -Value $workloadFunctionalPassed -Description "Workload '$workloadName' functionalPassed"
    Assert-Boolean -Value $workloadPerformancePassed -Description "Workload '$workloadName' performancePassed"
    Assert-Boolean -Value $workloadPassed -Description "Workload '$workloadName' passed"

    $checkpoints = @(Get-RequiredProperty -Object $workload -Name 'checkpoints' -Description "Workload '$workloadName'")
    if ($checkpoints.Count -ne $expectedCheckpointNames.Count) {
        throw "Workload '$workloadName' must contain initial, scrolled, edited, and released checkpoints."
    }
    $checkpointNames = @($checkpoints | ForEach-Object {
        $checkpointName = Get-RequiredProperty -Object $_ -Name 'name' -Description "Workload '$workloadName' checkpoint"
        Assert-String -Value $checkpointName -Description "Workload '$workloadName' checkpoint name"
        $checkpointName
    })
    if (@($checkpointNames | Sort-Object -Unique).Count -ne 4 -or
        (@($checkpointNames | Where-Object { $_ -cnotin $expectedCheckpointNames }).Count -gt 0)) {
        throw "Workload '$workloadName' has duplicate, missing, or unexpected checkpoint names."
    }

    $checkpointFunctionalResults = [System.Collections.Generic.List[bool]]::new()
    $checkpointPerformanceResults = [System.Collections.Generic.List[bool]]::new()
    foreach ($checkpoint in $checkpoints) {
        $checkpointName = [string](Get-RequiredProperty -Object $checkpoint -Name 'name' -Description "Workload '$workloadName' checkpoint")
        $numericFields = @(
            'viewportRows', 'realizedRows', 'realizedCells', 'liveRowViewModels',
            'liveCellViewModels', 'activeEditors', 'frameSampleCount', 'frameP95Ms',
            'frameMaxMs', 'privateBytes', 'privateWorkingSetBytes',
            'nativeAllocationBytes', 'nativeAllocationCount'
        )
        $values = @{}
        foreach ($field in $numericFields) {
            if ($field -in @('viewportRows', 'realizedRows', 'realizedCells', 'liveRowViewModels', 'liveCellViewModels', 'activeEditors', 'frameSampleCount', 'nativeAllocationCount')) {
                $values[$field] = Convert-ToNonNegativeInteger `
                    -Value (Get-RequiredProperty -Object $checkpoint -Name $field -Description "Workload '$workloadName' checkpoint '$checkpointName'") `
                    -Description "Workload '$workloadName' checkpoint '$checkpointName' $field"
            }
            else {
                $values[$field] = Convert-ToFiniteNonNegativeNumber `
                    -Value (Get-RequiredProperty -Object $checkpoint -Name $field -Description "Workload '$workloadName' checkpoint '$checkpointName'") `
                    -Description "Workload '$workloadName' checkpoint '$checkpointName' $field"
            }
        }
        $checkpointFunctionalPassed = Get-RequiredProperty -Object $checkpoint -Name 'functionalPassed' -Description "Workload '$workloadName' checkpoint '$checkpointName'"
        $checkpointPerformancePassed = Get-RequiredProperty -Object $checkpoint -Name 'performancePassed' -Description "Workload '$workloadName' checkpoint '$checkpointName'"
        $passed = Get-RequiredProperty -Object $checkpoint -Name 'passed' -Description "Workload '$workloadName' checkpoint '$checkpointName'"
        Assert-Boolean -Value $checkpointFunctionalPassed -Description "Workload '$workloadName' checkpoint '$checkpointName' functionalPassed"
        Assert-Boolean -Value $checkpointPerformancePassed -Description "Workload '$workloadName' checkpoint '$checkpointName' performancePassed"
        Assert-Boolean -Value $passed -Description "Workload '$workloadName' checkpoint '$checkpointName' passed"
        $checkpointFunctionalResults.Add([bool]$checkpointFunctionalPassed)
        $checkpointPerformanceResults.Add([bool]$checkpointPerformancePassed)
        $privateWorkingSetAvailable = Get-RequiredProperty -Object $checkpoint -Name 'privateWorkingSetAvailable' -Description "Workload '$workloadName' checkpoint '$checkpointName'"
        Assert-Boolean -Value $privateWorkingSetAvailable -Description "Workload '$workloadName' checkpoint '$checkpointName' privateWorkingSetAvailable"
        if (-not [bool]$privateWorkingSetAvailable) {
            throw "Workload '$workloadName' checkpoint '$checkpointName' private working-set measurement is unavailable."
        }
        if ($values.privateBytes -le 0 -or $values.privateWorkingSetBytes -le 0) {
            throw "Workload '$workloadName' checkpoint '$checkpointName' process memory capture must be greater than zero."
        }
        if ($checkpointName -ne 'released' -and $values.frameSampleCount -lt 30) {
            throw "Workload '$workloadName' checkpoint '$checkpointName' must contain at least 30 frame samples."
        }
        if ($checkpointName -ne 'released' -and ($values.frameP95Ms -le 0 -or $values.frameMaxMs -lt $values.frameP95Ms)) {
            throw "Workload '$workloadName' checkpoint '$checkpointName' has invalid active frame metrics."
        }

        if ($checkpointName -eq 'released') {
            foreach ($field in @('realizedRows', 'realizedCells', 'liveRowViewModels', 'liveCellViewModels', 'activeEditors')) {
                if ($values[$field] -ne 0) {
                    throw "Workload '$workloadName' released checkpoint must have $field=0."
                }
            }
        }
        else {
            if ($values.viewportRows -le 0) {
                throw "Workload '$workloadName' checkpoint '$checkpointName' viewportRows must be greater than zero."
            }
            if ($values.realizedRows -le 0 -or $values.realizedCells -le 0) {
                throw "Workload '$workloadName' checkpoint '$checkpointName' has an empty realized surface for a non-empty fixture."
            }
            if (-not ($values.realizedRows -lt ($realizedRowMultiplier * $values.viewportRows))) {
                throw "Workload '$workloadName' checkpoint '$checkpointName' realizedRows must be strictly below 3x viewportRows."
            }
        }
        if ($values.realizedCells -gt ($values.realizedRows * $columns)) {
            throw "Workload '$workloadName' checkpoint '$checkpointName' realizedCells exceeds realizedRows*columns."
        }
        if ($values.liveRowViewModels -gt $values.realizedRows -or $values.liveCellViewModels -gt $values.realizedCells) {
            throw "Workload '$workloadName' checkpoint '$checkpointName' live view-model counts exceed realized counts."
        }
        if ($values.liveCellViewModels -gt ($values.liveRowViewModels * $columns)) {
            throw "Workload '$workloadName' checkpoint '$checkpointName' liveCellViewModels exceeds liveRowViewModels*columns."
        }
        if ($values.activeEditors -gt 1) {
            throw "Workload '$workloadName' checkpoint '$checkpointName' has more than one active editor."
        }
        if ($checkpointName -eq 'edited' -and $values.activeEditors -ne 1) {
            throw "Workload '$workloadName' edited checkpoint must have exactly one active editor."
        }
        $frameFailure = $values.frameP95Ms -gt $frameP95Budget -or $values.frameMaxMs -gt $frameMaxBudget
        $nativeFailure = $values.nativeAllocationBytes -gt $nativeAllocationBudget
        $privateFailure = [math]::Max(0, $values.privateBytes - $baselinePrivateBytes) -gt $privateBytesDeltaBudget
        $availabilityFailure = -not [bool]$privateWorkingSetAvailable
        $measuredPerformancePassed = -not ($frameFailure -or $nativeFailure -or $privateFailure -or $availabilityFailure)
        if (-not $measuredPerformancePassed) {
            $details = @()
            if ($values.frameP95Ms -gt $frameP95Budget) { $details += "frameP95Ms=$($values.frameP95Ms)>$frameP95Budget" }
            if ($values.frameMaxMs -gt $frameMaxBudget) { $details += "frameMaxMs=$($values.frameMaxMs)>$frameMaxBudget" }
            if ($nativeFailure) { $details += "nativeAllocationBytes=$($values.nativeAllocationBytes)>$nativeAllocationBudget" }
            if ($privateFailure) { $details += "privateBytes growth above baseline>$privateBytesDeltaBudget" }
            if ($availabilityFailure) { $details += 'private working-set measurement unavailable' }
            $performanceFailures.Add("$workloadName/${checkpointName}: $($details -join ', ')") | Out-Null
            $performanceFailureCount++
        }
        if (-not [bool]$checkpointFunctionalPassed) {
            $functionalFailureCount++
        }
        if ([bool]$checkpointPerformancePassed -ne $measuredPerformancePassed) {
            throw "Workload '$workloadName' checkpoint '$checkpointName' performancePassed does not match measured thresholds."
        }
        $releaseLane = $architecture -eq 'x64' -and $configuration -eq 'Release'
        $expectedPassed = [bool]$checkpointFunctionalPassed -and ((-not $releaseLane) -or [bool]$checkpointPerformancePassed)
        if ([bool]$passed -ne $expectedPassed) {
            throw "Workload '$workloadName' checkpoint '$checkpointName' passed does not match functional/performance lane semantics."
        }
    }

    $expectedWorkloadFunctionalPassed = (@($checkpointFunctionalResults | Where-Object { -not $_ }).Count -eq 0)
    $expectedWorkloadPerformancePassed = (@($checkpointPerformanceResults | Where-Object { -not $_ }).Count -eq 0)
    $expectedWorkloadPassed = $expectedWorkloadFunctionalPassed -and ((-not $releaseLane) -or $expectedWorkloadPerformancePassed)
    if ([bool]$workloadFunctionalPassed -ne $expectedWorkloadFunctionalPassed -or
        [bool]$workloadPerformancePassed -ne $expectedWorkloadPerformancePassed -or
        [bool]$workloadPassed -ne $expectedWorkloadPassed) {
        throw "Workload '$workloadName' functional/performance result fields are inconsistent with checkpoint results."
    }
}

$releaseLane = $architecture -eq 'x64' -and $configuration -eq 'Release'
$expectedRootPassed = [bool]$rootFunctionalPassed -and ((-not $releaseLane) -or [bool]$rootPerformancePassed)
if (($overallStatus -eq 'passed') -ne $expectedRootPassed) {
    throw "Phase 4 large-data overallStatus does not match functional/performance lane semantics: $resolvedReportPath"
}
$expectedRootFunctionalPassed = (@($workloads | ForEach-Object { [bool](Get-RequiredProperty -Object $_ -Name 'functionalPassed' -Description 'Large-data workload') } | Where-Object { -not $_ }).Count -eq 0)
$expectedRootPerformancePassed = (@($workloads | ForEach-Object { [bool](Get-RequiredProperty -Object $_ -Name 'performancePassed' -Description 'Large-data workload') } | Where-Object { -not $_ }).Count -eq 0)
if ([bool]$rootFunctionalPassed -ne $expectedRootFunctionalPassed -or
    [bool]$rootPerformancePassed -ne $expectedRootPerformancePassed) {
    throw "Phase 4 large-data root functional/performance result is inconsistent with workload results: $resolvedReportPath"
}

$mode = if ($releaseLane) { 'release-performance-gate' } else { 'functional-only-not-full-gate' }
$gateStatus = 'passed'
if ($performanceFailures.Count -gt 0) {
    if ($releaseLane) {
        throw "Phase 4 Release large-data performance gate failed: $($performanceFailures -join '; ')"
    }
    $gateStatus = 'functional-passed-performance-failed'
}
if (-not [bool]$rootFunctionalPassed) {
    throw "Phase 4 large-data functional gate failed; failed metrics are preserved for diagnosis: $resolvedReportPath"
}

$result = [ordered]@{
    format = 'classmngr.winui.phase4.large-data.validation.v1'
    reportPath = $resolvedReportPath
    runId = $runId
    processId = $processId
    architecture = $architecture
    configuration = $configuration
    mode = $mode
    gateStatus = $gateStatus
    passed = $overallStatus -eq 'passed' -and $gateStatus -eq 'passed'
    performanceFailures = @($performanceFailures.ToArray())
}
Write-Output ($result | ConvertTo-Json -Depth 8 -Compress)
