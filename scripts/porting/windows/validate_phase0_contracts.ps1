[CmdletBinding()]
param(
    [string]$ProjectRoot = (Join-Path $PSScriptRoot "..\..\..")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$phase0Root = Join-Path $ProjectRoot "docs\porting\windows-direct2d\phase-0"
$ledgerPath = Join-Path $phase0Root "capture-ledger.csv"
$matrixPath = Join-Path $phase0Root "parity-matrix.csv"
$manifestPath = Join-Path $ProjectRoot "tests\fixtures\database-port\manifest.json"
$adrPath = Join-Path $ProjectRoot "docs\porting\adr\0001-windows-native-port-foundations.md"

foreach ($path in @($ledgerPath, $matrixPath, $manifestPath, $adrPath))
{
    if (-not (Test-Path -LiteralPath $path -PathType Leaf))
    {
        throw "Required Phase 0 contract file is missing: $path"
    }
}

$adrText = Get-Content -LiteralPath $adrPath -Raw
foreach ($requiredContract in @(
        "Windows 10 version 1703 (build 15063)",
        "10.0.26100.0",
        "{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}",
        "PerMonitorV2, PerMonitor, System",
        "longPathAware"
    ))
{
    if (-not $adrText.Contains($requiredContract))
    {
        throw "Windows native foundation ADR is missing required contract: $requiredContract"
    }
}

$validCaptureStates = @("pending", "in-progress", "captured", "verified", "blocked")
$ledger = Import-Csv -LiteralPath $ledgerPath
if ($ledger.Count -eq 0)
{
    throw "Capture ledger is empty."
}

$ledgerIds = @{}
foreach ($entry in $ledger)
{
    if ([string]::IsNullOrWhiteSpace($entry.id))
    {
        throw "Capture ledger contains an empty ID."
    }
    if ($ledgerIds.ContainsKey($entry.id))
    {
        throw "Capture ledger ID is duplicated: $($entry.id)"
    }
    if ($entry.status -notin $validCaptureStates)
    {
        throw "Capture ledger has an invalid status for $($entry.id): $($entry.status)"
    }
    $ledgerIds[$entry.id] = $true
}

$expectedMatrixColumns = @(
    "feature_id",
    "feature_surface",
    "data_read_write",
    "input_accessibility",
    "visual_state",
    "error_behavior",
    "print_export",
    "performance_x64",
    "performance_arm64",
    "evidence",
    "capture_ledger",
    "phase0_baseline",
    "native_status"
)
$validBaselineStates = @(
    "source-inventory",
    "startup-x64-captured",
    "fixtures-verified-x64",
    "pending"
)
$matrix = Import-Csv -LiteralPath $matrixPath
if ($matrix.Count -eq 0)
{
    throw "Parity matrix is empty."
}

$actualMatrixColumns = @($matrix[0].PSObject.Properties.Name)
if (Compare-Object -ReferenceObject $expectedMatrixColumns -DifferenceObject $actualMatrixColumns)
{
    throw "Parity matrix columns do not match the Phase 0 contract."
}

foreach ($row in $matrix)
{
    if ([string]::IsNullOrWhiteSpace($row.feature_id))
    {
        throw "Parity matrix contains an empty feature ID."
    }
    if ($row.phase0_baseline -notin $validBaselineStates)
    {
        throw "Parity matrix has an invalid Phase 0 baseline for $($row.feature_id): $($row.phase0_baseline)"
    }
    if ($row.native_status -ne "not-started")
    {
        throw "Phase 0 matrix cannot claim native parity for $($row.feature_id): $($row.native_status)"
    }
    if ($row.capture_ledger -and $row.capture_ledger -ne "not-applicable")
    {
        foreach ($ledgerId in $row.capture_ledger.Split("|"))
        {
            if (-not $ledgerIds.ContainsKey($ledgerId))
            {
                throw "Parity matrix references an unknown capture-ledger ID: $($row.feature_id):$ledgerId"
            }
        }
    }
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.format -ne "classmngr-database-port-fixtures-v1")
{
    throw "Database fixture manifest has an unsupported format: $($manifest.format)"
}
if (-not $manifest.fixtures -or $manifest.fixtures.Count -eq 0)
{
    throw "Database fixture manifest is empty."
}

$fixtureRoot = Split-Path -Parent $manifestPath
foreach ($fixture in $manifest.fixtures)
{
    if ([string]::IsNullOrWhiteSpace($fixture.file) -or [string]::IsNullOrWhiteSpace($fixture.sha256) -or -not $fixture.semantic)
    {
        throw "Database fixture manifest entry is incomplete: $($fixture.id)"
    }

    $fixturePath = Join-Path $fixtureRoot $fixture.file
    if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf))
    {
        throw "Required database fixture is missing: $fixturePath"
    }

    $actualHash = ((Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256).Hash).ToLowerInvariant()
    if ($actualHash -ne $fixture.sha256)
    {
        throw "Database fixture checksum mismatch: $($fixture.file)"
    }
}

Write-Host (
    "Phase 0 contracts valid: {0} capture rows, {1} parity rows, {2} fixtures." -f
    $ledger.Count,
    $matrix.Count,
    $manifest.fixtures.Count
    )

