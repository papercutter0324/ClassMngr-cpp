[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [string]$QtArtifactRoot = '',

    [ValidateSet('all', 'startup', 'no-database', 'empty', 'populated', 'error')]
    [string]$Scenario = 'all',

    [ValidateRange(0, 600000)]
    [int]$SettleMilliseconds = 1500
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

function Read-JsonFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return Get-Content -LiteralPath $Path -Raw -Encoding UTF8 |
        ConvertFrom-Json
}

function Find-QtCapture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root,

        [Parameter(Mandatory = $true)]
        [string]$LedgerId,

        [Parameter(Mandatory = $true)]
        [string]$State
    )

    if ([string]::IsNullOrWhiteSpace($Root)) {
        return $null
    }

    $candidates = @(
        Get-ChildItem -LiteralPath $Root -Recurse -File -Filter '*.json' |
            Sort-Object -Property LastWriteTime -Descending |
            ForEach-Object {
                try {
                    $metadata = Read-JsonFile -Path $_.FullName
                }
                catch {
                    return
                }

                if ($metadata.format -ne 'classmngr-phase0-capture-v1' `
                    -or [string]$metadata.ledgerId -ne $LedgerId `
                    -or [string]$metadata.fixtureId -ne 'no-database' `
                    -or [string]$metadata.artifact.file -notlike "*$State*") {
                    return
                }

                return [ordered]@{
                    metadata = $_.FullName
                    capture = Join-Path $_.DirectoryName ([string]$metadata.artifact.file)
                    sourceRevision = [string]$metadata.sourceRevision
                    displayScalePercent = [int]$metadata.displayScalePercent
                    window = $metadata.window
                }
            }
    )

    if ($candidates.Count -eq 0) {
        return $null
    }

    return $candidates[0]
}

$resolvedExecutable = Get-AbsolutePath -Path $Executable
if (-not (Test-Path -LiteralPath $resolvedExecutable -PathType Leaf)) {
    throw "WinUI executable was not found: $resolvedExecutable"
}

$resolvedOutputDirectory = Get-AbsolutePath -Path $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

$runner = Join-Path $PSScriptRoot 'run_winui_scenario.ps1'
if (-not (Test-Path -LiteralPath $runner -PathType Leaf)) {
    throw "WinUI scenario runner was not found: $runner"
}

if (-not [string]::IsNullOrWhiteSpace($QtArtifactRoot)) {
    $QtArtifactRoot = Get-AbsolutePath -Path $QtArtifactRoot
    if (-not (Test-Path -LiteralPath $QtArtifactRoot -PathType Container)) {
        throw "Qt artifact root was not found: $QtArtifactRoot"
    }
}

$catalog = @(
    [ordered]@{
        id = 'shell-startup'
        ledgerId = 'page.personal-details'
        state = 'empty'
        qtState = 'empty'
        fixtureId = 'no-database'
        argument = $null
        scenarioName = 'phase5-shell-startup'
    },
    [ordered]@{
        id = 'feature.campus-information.no-database'
        ledgerId = 'page.campus-dashboard'
        state = 'no-database'
        qtState = 'empty'
        fixtureId = 'no-database'
        argument = '--phase5-campus-no-database'
        scenarioName = 'campus-information-no-database'
    },
    [ordered]@{
        id = 'feature.campus-information.empty'
        ledgerId = 'page.campus-dashboard'
        state = 'empty'
        qtState = 'empty'
        fixtureId = 'empty'
        argument = '--phase5-campus-empty'
        scenarioName = 'campus-information-empty'
    },
    [ordered]@{
        id = 'feature.campus-information.populated'
        ledgerId = 'page.campus-dashboard'
        state = 'populated'
        qtState = 'populated'
        fixtureId = 'phase5-campus-memory'
        argument = '--phase5-campus-populated'
        scenarioName = 'campus-information-populated'
    },
    [ordered]@{
        id = 'feature.campus-information.error'
        ledgerId = 'page.campus-dashboard'
        state = 'error'
        qtState = 'error'
        fixtureId = 'phase5-campus-error'
        argument = '--phase5-campus-error'
        scenarioName = 'campus-information-error'
    }
)

$selectedCatalog = if ($Scenario -eq 'all') {
    @($catalog)
}
else {
    @($catalog | Where-Object {
        $_.state -eq $Scenario -or ($Scenario -eq 'startup' -and $_.id -eq 'shell-startup')
    })
}
if ($selectedCatalog.Count -eq 0) {
    throw "No Phase 5 scenario matched '$Scenario'."
}

$entries = [System.Collections.Generic.List[object]]::new()
foreach ($item in $selectedCatalog) {
    $scenarioDirectory = Join-Path $resolvedOutputDirectory ([string]$item.scenarioName)
    New-Item -ItemType Directory -Force -Path $scenarioDirectory | Out-Null
    $metadataPath = Join-Path $scenarioDirectory ([string]$item.scenarioName + '.json')
    $capturePath = Join-Path $scenarioDirectory ([string]$item.scenarioName + '.png')
    if ((Test-Path -LiteralPath $metadataPath) -or (Test-Path -LiteralPath $capturePath)) {
        throw "Refusing to overwrite Phase 5 scenario artifacts in $scenarioDirectory"
    }

    $runnerArguments = @{
        Executable = $resolvedExecutable
        OutputDirectory = $scenarioDirectory
        ScenarioName = [string]$item.scenarioName
        SettleMilliseconds = $SettleMilliseconds
    }
    if ($null -ne $item.argument) {
        $runnerArguments.Arguments = @([string]$item.argument)
    }

    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner @runnerArguments
    if ($LASTEXITCODE -ne 0) {
        throw "WinUI scenario runner failed for $($item.id) with exit code $LASTEXITCODE"
    }

    $winuiMetadata = Read-JsonFile -Path $metadataPath
    if ($winuiMetadata.format -ne 'classmngr-winui-scenario-v1') {
        throw "Unexpected WinUI scenario metadata format: $metadataPath"
    }

    $qtCapture = Find-QtCapture `
        -Root $QtArtifactRoot `
        -LedgerId ([string]$item.ledgerId) `
        -State ([string]$item.qtState)
    $winuiPassed = [bool]$winuiMetadata.result.passed
    $pairStatus = if (-not $winuiPassed) {
        'failed'
    }
    elseif ($null -eq $qtCapture) {
        'winui-captured-qt-evidence-missing'
    }
    else {
        'captured-awaiting-owner-review'
    }

    $entries.Add([ordered]@{
        pairedScenarioId = [string]$item.id
        ledgerId = [string]$item.ledgerId
        state = [string]$item.state
        fixtureId = [string]$item.fixtureId
        pairStatus = $pairStatus
        qt = [ordered]@{
            scenarioId = [string]$item.ledgerId
            artifact = if ($null -eq $qtCapture) { $null } else {
                [ordered]@{
                    metadata = [string]$qtCapture.metadata
                    capture = [string]$qtCapture.capture
                    sourceRevision = [string]$qtCapture.sourceRevision
                    displayScalePercent = [int]$qtCapture.displayScalePercent
                    window = $qtCapture.window
                }
            }
        }
        winui = [ordered]@{
            scenarioName = [string]$item.scenarioName
            argument = $item.argument
            metadata = Join-Path ([string]$item.scenarioName) ([string]$item.scenarioName + '.json')
            capture = Join-Path ([string]$item.scenarioName) ([string]$item.scenarioName + '.png')
            passed = $winuiPassed
            exitCode = $winuiMetadata.process.exitCode
        }
    })
}

$manifestPath = Join-Path $resolvedOutputDirectory 'phase5-paired-scenarios.json'
if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
    throw "Refusing to overwrite paired-scenario manifest: $manifestPath"
}

$manifest = [ordered]@{
    format = 'classmngr-phase5-paired-scenarios-v1'
    generatedAtUtc = [DateTime]::UtcNow.ToString('o')
    executable = $resolvedExecutable
    qtArtifactRoot = if ([string]::IsNullOrWhiteSpace($QtArtifactRoot)) { $null } else { $QtArtifactRoot }
    scenarios = @($entries.ToArray())
}
[System.IO.File]::WriteAllText(
    $manifestPath,
    ($manifest | ConvertTo-Json -Depth 12) + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false)
    )

$failed = @($entries | Where-Object { $_.pairStatus -eq 'failed' })
if ($failed.Count -gt 0) {
    throw "Phase 5 paired scenarios failed: $($failed.pairedScenarioId -join ', ')"
}

if ([string]::IsNullOrWhiteSpace($QtArtifactRoot)) {
    Write-Warning 'QtArtifactRoot was not supplied; the manifest records WinUI captures awaiting Qt evidence.'
}
Write-Host "Wrote Phase 5 paired-scenario manifest: $manifestPath"
