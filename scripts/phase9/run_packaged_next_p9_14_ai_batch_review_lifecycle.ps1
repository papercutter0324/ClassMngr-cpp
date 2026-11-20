[CmdletBinding()]
param(
    [string]$RepositoryRoot = '',
    [string]$EvidenceRoot = 'artifacts/phase9/packaged-next-p9-14-ai-batch-review',
    [string]$PriorRunManifestPath = 'artifacts/phase9/packaged-next-speaking-evaluation/20261120T133615659Z-4a085b90bf3842319599ca4f1c24bff5/manifest.json',
    [ValidateRange(30, 300)][int]$ReadinessTimeoutSeconds = 120,
    [ValidateRange(60, 900)][int]$ExitTimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) { $RepositoryRoot = Join-Path $PSScriptRoot '../..' }
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$PriorRunManifestPath = if ([IO.Path]::IsPathRooted($PriorRunManifestPath)) { [IO.Path]::GetFullPath($PriorRunManifestPath) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $PriorRunManifestPath)) }
$EvidenceRoot = if ([IO.Path]::IsPathRooted($EvidenceRoot)) { [IO.Path]::GetFullPath($EvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
. (Join-Path $PSScriptRoot 'p9_14_packaged_lifecycle_common.ps1')
Invoke-P914PackagedLifecycle -Mode 'ai-batch-review' -RepositoryRoot $RepositoryRoot -EvidenceRoot $EvidenceRoot -PriorRunManifestPath $PriorRunManifestPath -ReadinessTimeoutSeconds $ReadinessTimeoutSeconds -ExitTimeoutSeconds $ExitTimeoutSeconds
