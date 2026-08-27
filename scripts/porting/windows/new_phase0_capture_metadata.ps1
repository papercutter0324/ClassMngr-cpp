[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$LedgerId,

    [Parameter(Mandatory)]
    [ValidateSet("x64", "ARM64")]
    [string]$Architecture,

    [Parameter(Mandatory)]
    [ValidateSet("light", "dark")]
    [string]$Theme,

    [Parameter(Mandatory)]
    [ValidateSet(100, 150, 200)]
    [int]$DisplayScalePercent,

    [Parameter(Mandatory)]
    [ValidateSet("en", "ko")]
    [string]$AppLanguage,

    [Parameter(Mandatory)]
    [ValidateSet("en-US", "ko-KR")]
    [string]$InputLanguage,

    [Parameter(Mandatory)]
    [string]$FixtureId,

    [Parameter(Mandatory)]
    [string]$ArtifactPath,

    [Parameter(Mandatory)]
    [string]$SourceRevision,

    [Parameter(Mandatory)]
    [string]$WindowsEdition,

    [Parameter(Mandatory)]
    [string]$WindowsBuild,

    [ValidateSet("small", "normal", "large", "extra-large")]
    [string]$FontSize = "normal",

    [int]$WindowWidth = 0,

    [int]$WindowHeight = 0,

    [string[]]$Action = @(),

    [string]$ProjectRoot = (Join-Path $PSScriptRoot "..\..\..")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ledgerPath = Join-Path $ProjectRoot "docs\porting\windows-direct2d\phase-0\capture-ledger.csv"
if (-not (Test-Path -LiteralPath $ledgerPath -PathType Leaf))
{
    throw "Capture ledger is missing: $ledgerPath"
}

$ledgerEntry = Import-Csv -LiteralPath $ledgerPath |
    Where-Object { $_.id -eq $LedgerId }
if (-not $ledgerEntry)
{
    throw "Unknown capture-ledger ID: $LedgerId"
}
if (@($ledgerEntry).Count -ne 1)
{
    throw "Capture ledger ID is not unique: $LedgerId"
}

$artifact = Get-Item -LiteralPath $ArtifactPath -ErrorAction Stop
if ($artifact.PSIsContainer)
{
    throw "Capture artifact must be a file: $ArtifactPath"
}

$artifactBaseName = [System.IO.Path]::GetFileNameWithoutExtension($artifact.Name)
$expectedPrefix = "$($ledgerEntry.artifact_prefix)__"
if (-not $artifactBaseName.StartsWith($expectedPrefix, [System.StringComparison]::OrdinalIgnoreCase))
{
    throw "Artifact name must begin with '$expectedPrefix': $($artifact.Name)"
}

$metadataPath = Join-Path $artifact.DirectoryName "$artifactBaseName.json"
if (Test-Path -LiteralPath $metadataPath)
{
    throw "Refusing to overwrite capture metadata: $metadataPath"
}

$metadata = [ordered]@{
    format = "classmngr-phase0-capture-v1"
    ledgerId = $LedgerId
    sourceRevision = $SourceRevision
    fixtureId = $FixtureId
    architecture = $Architecture
    windows = [ordered]@{
        edition = $WindowsEdition
        build = $WindowsBuild
    }
    displayScalePercent = $DisplayScalePercent
    theme = $Theme
    appLanguage = $AppLanguage
    inputLanguage = $InputLanguage
    fontSize = $FontSize
    window = [ordered]@{
        width = $WindowWidth
        height = $WindowHeight
    }
    actions = @($Action)
    artifact = [ordered]@{
        file = $artifact.Name
        sha256 = ((Get-FileHash -LiteralPath $artifact.FullName -Algorithm SHA256).Hash).ToLowerInvariant()
    }
    observations = [ordered]@{
        keyboard = "TODO"
        inputMethod = "TODO"
        accessibility = "TODO"
        notes = "TODO"
    }
    verification = "captured"
}

$metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $metadataPath -Encoding utf8
Write-Host "Created capture metadata: $metadataPath"

