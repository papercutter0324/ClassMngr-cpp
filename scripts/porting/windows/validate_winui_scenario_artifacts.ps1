[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ArtifactRoot,

    [switch]$RequirePassed
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ArtifactRoot -PathType Container))
{
    throw "WinUI scenario artifact root is missing: $ArtifactRoot"
}

function Get-MetadataValue
{
    param(
        [Parameter(Mandatory = $true)]
        [object]$Object,

        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property)
    {
        return $null
    }
    return $property.Value
}

function Assert-OrderedSteps
{
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Actual,

        [Parameter(Mandatory = $true)]
        [string[]]$Expected,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    $previous = -1
    foreach ($name in $Expected)
    {
        $index = [Array]::IndexOf($Actual, $name)
        if ($index -lt 0 -or $index -le $previous)
        {
            throw "$Description is missing or out of order: $name"
        }
        $previous = $index
    }
}

function Assert-Capture
{
    param(
        [Parameter(Mandatory = $true)]
        [object]$Capture,

        [Parameter(Mandatory = $true)]
        [System.IO.FileInfo]$MetadataFile,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if ($null -eq $Capture)
    {
        throw "$Description capture is missing: $($MetadataFile.FullName)"
    }

    $relativeFile = [string](Get-MetadataValue $Capture 'file')
    if (([string]::IsNullOrWhiteSpace($relativeFile)) -or
        [System.IO.Path]::IsPathRooted($relativeFile) -or
        $relativeFile -match '[\\/]')
    {
        throw "$Description capture file must be a sidecar filename: $($MetadataFile.FullName)"
    }

    $capturePath = Join-Path $MetadataFile.DirectoryName $relativeFile
    if (-not (Test-Path -LiteralPath $capturePath -PathType Leaf))
    {
        throw "$Description capture is missing: $capturePath"
    }

    $width = [int](Get-MetadataValue $Capture 'width')
    $height = [int](Get-MetadataValue $Capture 'height')
    if ($width -le 0 -or $height -le 0)
    {
        throw "$Description capture bounds are invalid: $capturePath"
    }

    $expectedHash = [string](Get-MetadataValue $Capture 'sha256')
    $actualHash = ((Get-FileHash -LiteralPath $capturePath -Algorithm SHA256).Hash).ToLowerInvariant()
    if (([string]::IsNullOrWhiteSpace($expectedHash)) -or
        $actualHash -ne $expectedHash.ToLowerInvariant())
    {
        throw "$Description capture checksum mismatch: $capturePath"
    }
}

$metadataFiles = @(Get-ChildItem -LiteralPath $ArtifactRoot -Recurse -File -Filter '*.json')
if ($metadataFiles.Count -eq 0)
{
    throw "No WinUI scenario metadata files found under $ArtifactRoot."
}

$validatedCount = 0
foreach ($metadataFile in $metadataFiles)
{
    try
    {
        $metadata = Get-Content -LiteralPath $metadataFile.FullName -Raw |
            ConvertFrom-Json
    }
    catch
    {
        throw "WinUI scenario metadata is not valid JSON: $($metadataFile.FullName)"
    }

    if ((Get-MetadataValue $metadata 'format') -ne 'classmngr-winui-scenario-v1')
    {
        throw "Unsupported WinUI scenario metadata format: $($metadataFile.FullName)"
    }

    $scenario = Get-MetadataValue $metadata 'scenario'
    $result = Get-MetadataValue $metadata 'result'
    $process = Get-MetadataValue $metadata 'process'
    $window = Get-MetadataValue $metadata 'window'
    $dialog = Get-MetadataValue $metadata 'dialog'
    $steps = @(Get-MetadataValue $metadata 'steps')
    foreach ($value in @($scenario, $result, $process, $window, $dialog))
    {
        if ($null -eq $value)
        {
            throw "WinUI scenario metadata is missing a required object: $($metadataFile.FullName)"
        }
    }

    $scenarioName = [string](Get-MetadataValue $scenario 'name')
    if ([string]::IsNullOrWhiteSpace($scenarioName))
    {
        throw "WinUI scenario name is missing: $($metadataFile.FullName)"
    }

    $status = [string](Get-MetadataValue $result 'status')
    if ($status -notin @('passed', 'failed'))
    {
        throw "WinUI scenario result status is invalid: $($metadataFile.FullName)"
    }
    $passed = [bool](Get-MetadataValue $result 'passed')
    if ($passed -ne ($status -eq 'passed'))
    {
        throw "WinUI scenario result passed flag is inconsistent: $($metadataFile.FullName)"
    }

    $stepNames = @(
        $steps |
            ForEach-Object {
                [string](Get-MetadataValue $_ 'name')
            }
    )
    if ($stepNames.Count -eq 0)
    {
        throw "WinUI scenario has no recorded steps: $($metadataFile.FullName)"
    }
    $previousElapsed = -1.0
    for ($index = 0; $index -lt $steps.Count; $index++)
    {
        $sequence = [int](Get-MetadataValue $steps[$index] 'sequence')
        if ($sequence -ne $index + 1)
        {
            throw "WinUI scenario step sequence is not contiguous: $($metadataFile.FullName)"
        }
        $elapsed = [double](Get-MetadataValue $steps[$index] 'elapsedMs')
        if ($elapsed -lt $previousElapsed)
        {
            throw "WinUI scenario step timing moves backwards: $($metadataFile.FullName)"
        }
        $previousElapsed = $elapsed
    }

    $dialogRequested = [bool](Get-MetadataValue $dialog 'requested')
    $expectedSteps = @(
        'launch-requested',
        'process-started'
    )
    if ($passed)
    {
        $expectedSteps += @('window-shown', 'settled', 'capture-started', 'captured')
        if ($dialogRequested)
        {
            $expectedSteps += @('dialog-shown', 'dialog-captured', 'dialog-released')
        }
        $expectedSteps += @('close-requested', 'process-exited', 'resources-released')
    }
    else
    {
        $expectedSteps += 'scenario-failed'
    }
    Assert-OrderedSteps -Actual $stepNames -Expected $expectedSteps -Description $metadataFile.Name

    $failure = [string](Get-MetadataValue $result 'failure')
    if (-not $passed -and [string]::IsNullOrWhiteSpace($failure))
    {
        throw "Failed WinUI scenario has no failure detail: $($metadataFile.FullName)"
    }
    if ($RequirePassed -and -not $passed)
    {
        throw "WinUI scenario is not passed: $($metadataFile.FullName)"
    }

    if ($passed)
    {
        if (((Get-MetadataValue $process 'exited') -ne $true) -or
            ((Get-MetadataValue $process 'forcedTermination') -eq $true))
        {
            throw "Passed WinUI scenario did not exit cleanly: $($metadataFile.FullName)"
        }
        if ((Get-MetadataValue $window 'mainReleased') -ne $true)
        {
            throw "Passed WinUI main window was not released: $($metadataFile.FullName)"
        }
        Assert-Capture -Capture (Get-MetadataValue $window 'capture') -MetadataFile $metadataFile -Description 'Main window'
        if ($dialogRequested)
        {
            if ((Get-MetadataValue $dialog 'released') -ne $true)
            {
                throw "Passed WinUI dialog was not released: $($metadataFile.FullName)"
            }
            Assert-Capture -Capture (Get-MetadataValue $dialog 'capture') -MetadataFile $metadataFile -Description 'Dialog'
        }
    }

    $validatedCount++
}

Write-Host "WinUI scenario artifacts valid: $validatedCount metadata sidecars."
