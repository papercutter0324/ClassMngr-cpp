[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$ArtifactRoot,

    [switch]$RequireVerified,

    [string]$ProjectRoot = (Join-Path $PSScriptRoot "..\..\..")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ledgerPath = Join-Path $ProjectRoot "docs\porting\windows-direct2d\phase-0\capture-ledger.csv"
if (-not (Test-Path -LiteralPath $ledgerPath -PathType Leaf))
{
    throw "Capture ledger is missing: $ledgerPath"
}
if (-not (Test-Path -LiteralPath $ArtifactRoot -PathType Container))
{
    throw "Capture artifact root is missing: $ArtifactRoot"
}

$ledgerIds = @{}
foreach ($entry in (Import-Csv -LiteralPath $ledgerPath))
{
    $ledgerIds[$entry.id] = $true
}

$metadataFiles = Get-ChildItem -LiteralPath $ArtifactRoot -Recurse -File -Filter "*.json"
if (@($metadataFiles).Count -eq 0)
{
    Write-Host "No capture metadata files found under $ArtifactRoot."
    return
}

$validatedCount = 0
foreach ($metadataFile in $metadataFiles)
{
    try
    {
        $metadata = Get-Content -LiteralPath $metadataFile.FullName -Raw | ConvertFrom-Json
    }
    catch
    {
        throw "Capture metadata is not valid JSON: $($metadataFile.FullName)"
    }

    if ($metadata.format -ne "classmngr-phase0-capture-v1")
    {
        throw "Capture metadata has an unsupported format: $($metadataFile.FullName)"
    }
    if (-not $ledgerIds.ContainsKey($metadata.ledgerId))
    {
        throw "Capture metadata references an unknown ledger ID: $($metadataFile.FullName)"
    }

    $validPlatformValues = $metadata.architecture -in "x64", "ARM64" -and $metadata.theme -in "light", "dark" -and $metadata.displayScalePercent -in 100, 150, 200 -and $metadata.appLanguage -in "en", "ko" -and $metadata.inputLanguage -in "en-US", "ko-KR" -and $metadata.verification -in "captured", "verified", "blocked"
    if (-not $validPlatformValues)
    {
        throw "Capture metadata has invalid platform values: $($metadataFile.FullName)"
    }

    foreach ($value in @(
            $metadata.sourceRevision,
            $metadata.fixtureId,
            $metadata.windows.edition,
            $metadata.windows.build,
            $metadata.artifact.file,
            $metadata.artifact.sha256,
            $metadata.observations.keyboard,
            $metadata.observations.inputMethod,
            $metadata.observations.accessibility,
            $metadata.observations.notes
        ))
    {
        if ([string]::IsNullOrWhiteSpace($value) -or $value -eq "TODO")
        {
            throw "Capture metadata has an incomplete required value: $($metadataFile.FullName)"
        }
    }
    if (@($metadata.actions).Count -eq 0)
    {
        throw "Capture metadata requires at least one action: $($metadataFile.FullName)"
    }

    $artifactRelativePath = [string]$metadata.artifact.file
    $isUnsafeArtifactPath = [System.IO.Path]::IsPathRooted($artifactRelativePath) -or $artifactRelativePath -match "[\\/]"
    if ($isUnsafeArtifactPath)
    {
        throw "Capture artifact path must be a sidecar filename: $($metadataFile.FullName)"
    }

    $artifactPath = Join-Path $metadataFile.DirectoryName $artifactRelativePath
    if (-not (Test-Path -LiteralPath $artifactPath -PathType Leaf))
    {
        throw "Capture artifact is missing: $artifactPath"
    }

    $expectedBaseName = [System.IO.Path]::GetFileNameWithoutExtension($artifactRelativePath)
    if ($metadataFile.BaseName -ne $expectedBaseName)
    {
        throw "Capture metadata is not a sidecar for its artifact: $($metadataFile.FullName)"
    }

    $actualHash = ((Get-FileHash -LiteralPath $artifactPath -Algorithm SHA256).Hash).ToLowerInvariant()
    if ($actualHash -ne $metadata.artifact.sha256)
    {
        throw "Capture artifact checksum mismatch: $artifactPath"
    }
    if ($RequireVerified -and $metadata.verification -ne "verified")
    {
        throw "Capture metadata is not verified: $($metadataFile.FullName)"
    }

    $validatedCount++
}

Write-Host "Phase 0 capture artifacts valid: $validatedCount metadata sidecars."

