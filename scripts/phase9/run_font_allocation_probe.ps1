<#
.SYNOPSIS
Runs source-bound fresh-process Qt font allocation probes for Windows x64 Release.

.DESCRIPTION
This diagnostic captures synchronous process-memory snapshots around Qt startup,
application-font registration and queries, startup-font application, and the full
system-font-family query. It does not launch or modify the packaged application.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Join-Path $PSScriptRoot "..\.."),
    [string]$BuildDirectory = "build\windows-x64-release",
    [string]$PackageDirectory = "dist\ClassMngr-windows-x64",
    [string]$QtPrefix = "",
    [string]$EvidenceRoot = "artifacts\phase9\font-allocation-probe",
    [ValidateRange(5, 20)]
    [int]$Repetitions = 7,
    [switch]$SkipBuild,
    [string]$ExistingBuildLog = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$TaskId = "P9_06S_FONT_ALLOCATION_PROBE"
$Schema = "classmngr-phase9-font-allocation-probe-v1"
$IncludedRoots = @(
    "CMakeLists.txt",
    "CMakePresets.json",
    "cmake",
    "docs/qt-rewrite/phase-4-resource-inventory.json",
    "licenses",
    "resources",
    "scripts/phase4",
    "scripts/phase9",
    "src",
    "tests",
    "third_party"
)
$Scenarios = @(
    "no-font",
    "inter-only",
    "pretendard-only",
    "inter-then-pretendard",
    "pretendard-then-inter"
)
$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$BuildDirectory = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    [System.IO.Path]::GetFullPath($BuildDirectory)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot $BuildDirectory))
}
$PackageDirectory = if ([System.IO.Path]::IsPathRooted($PackageDirectory)) {
    [System.IO.Path]::GetFullPath($PackageDirectory)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot $PackageDirectory))
}
$EvidenceRoot = if ([System.IO.Path]::IsPathRooted($EvidenceRoot)) {
    [System.IO.Path]::GetFullPath($EvidenceRoot)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot))
}
$RunId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + "-" + [Guid]::NewGuid().ToString("N")
$EvidenceDirectory = Join-Path $EvidenceRoot $RunId
if (Test-Path -LiteralPath $EvidenceDirectory) {
    throw "Refusing to overwrite existing evidence run: $EvidenceDirectory"
}
New-Item -ItemType Directory -Path $EvidenceDirectory | Out-Null
$RunsDirectory = Join-Path $EvidenceDirectory "runs"
New-Item -ItemType Directory -Path $RunsDirectory | Out-Null
$ManifestPath = Join-Path $EvidenceDirectory "manifest.json"
$SummaryPath = Join-Path $EvidenceDirectory "summary.json"
$BuildLogPath = Join-Path $EvidenceDirectory "cmake-build.log"
$SourceStartPath = Join-Path $EvidenceDirectory "source-files-at-start.json"
$SourceEndPath = Join-Path $EvidenceDirectory "source-files-at-end.json"
$ProcessSummaryPath = Join-Path $EvidenceDirectory "process-summary.jsonl"
$MediansCsvPath = Join-Path $EvidenceDirectory "scenario-medians.csv"
$ProcessRuns = [System.Collections.Generic.List[object]]::new()
$UniquePids = [System.Collections.Generic.HashSet[int]]::new()
$Manifest = [ordered]@{
    schema = $Schema
    taskId = $TaskId
    runId = $RunId
    status = "running"
    createdAtUtc = [DateTimeOffset]::UtcNow.ToString("o")
    completedAtUtc = $null
    repositoryRoot = $RepositoryRoot
    buildDirectory = $BuildDirectory
    packageDirectory = $PackageDirectory
    repetitionsPerScenario = $Repetitions
    sourceIdentity = $null
    configuration = $null
    qtRuntime = $null
    package = $null
    probe = $null
    executionOrder = [System.Collections.Generic.List[string]]::new()
    processRuns = $ProcessRuns
    failure = $null
}

function Get-UtcTimestamp {
    return [DateTimeOffset]::UtcNow.ToString("o")
}

function Get-FileSha256 {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Write-JsonFile {
    param([string]$Path, [object]$Value)
    $json = $Value | ConvertTo-Json -Depth 40
    [System.IO.File]::WriteAllText(
        $Path,
        $json + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false)
    )
}

function Add-JsonLine {
    param([string]$Path, [object]$Value)
    $line = $Value | ConvertTo-Json -Depth 24 -Compress
    [System.IO.File]::AppendAllText(
        $Path,
        $line + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false)
    )
}

function Get-StringSha256 {
    param([string]$Value)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
    } finally { $sha.Dispose() }
}

function Get-CandidateSourceIdentity {
    param([string]$Root)
    $items = [System.Collections.Generic.List[object]]::new()
    foreach ($relativeRoot in $IncludedRoots) {
        $rootPath = Join-Path $Root $relativeRoot
        if (-not (Test-Path -LiteralPath $rootPath)) {
            throw "Source identity root was not found: $relativeRoot"
        }
        $files = if (Test-Path -LiteralPath $rootPath -PathType Leaf) {
            @(Get-Item -LiteralPath $rootPath)
        } else {
            @(Get-ChildItem -LiteralPath $rootPath -File -Recurse)
        }
        $files = @($files | Where-Object {
            ($_.FullName -notmatch '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]') -and
            ($_.Extension -notin @(".pyc", ".pyo"))
        })
        foreach ($file in $files) {
            $relativePath = [System.IO.Path]::GetRelativePath($Root, $file.FullName).Replace("\", "/")
            $items.Add([ordered]@{
                path = $relativePath
                sizeBytes = [Int64]$file.Length
                sha256 = Get-FileSha256 -Path $file.FullName
            })
        }
    }
    $sorted = @($items | Sort-Object { ([string]$_.path).ToLowerInvariant() })
    if ($sorted.Count -eq 0) { throw "Candidate source inventory contains no files." }
    $canonical = @($sorted | ForEach-Object {
        "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256
    }) -join "`n"
    return [ordered]@{
        sha256 = Get-StringSha256 -Value ($canonical + "`n")
        includedRoots = $IncludedRoots
        fileCount = $sorted.Count
        files = $sorted
    }
}

function Get-CacheValue {
    param([string[]]$Lines, [string]$Name)
    $escaped = [regex]::Escape($Name)
    $matches = @($Lines | Where-Object { $_ -match "^${escaped}:[^=]+=" })
    if ($matches.Count -ne 1) { throw "Expected one $Name cache entry; found $($matches.Count)." }
    return (($matches[0] -split "=", 2)[1])
}

function Get-PeArchitecture {
    param([string]$Path)
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $reader = [System.IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Image has no MZ header: $Path" }
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 64 -or $peOffset -gt ($stream.Length - 6)) { throw "Image has invalid PE header: $Path" }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw "Image has no PE signature: $Path" }
        $machine = $reader.ReadUInt16()
        if ($machine -eq 0x8664) { return "x64" }
        if ($machine -eq 0xaa64) { return "arm64" }
        return ("unknown-0x{0:x4}" -f $machine)
    } finally { $stream.Dispose() }
}

function Get-BinaryQtVersion {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Qt binary was not found: $Path" }
    $info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
    $value = if (-not [string]::IsNullOrWhiteSpace($info.ProductVersion)) { $info.ProductVersion } else { $info.FileVersion }
    $match = [regex]::Match([string]$value, '\d+\.\d+\.\d+')
    if (-not $match.Success) { throw "Qt binary has no semantic version: $Path" }
    return $match.Value
}

function Get-ReleaseConfiguration {
    param([string[]]$CacheLines, [string]$BuildRoot)
    $generator = Get-CacheValue -Lines $CacheLines -Name "CMAKE_GENERATOR"
    if ($generator -notmatch '^Visual Studio') { throw "Expected the local MSVC Visual Studio Release tree; found '$generator'." }
    $platform = Get-CacheValue -Lines $CacheLines -Name "CMAKE_GENERATOR_PLATFORM"
    if ($platform -ne "x64") { throw "Expected an x64 Visual Studio build tree; found '$platform'." }
    $cacheFlags = Get-CacheValue -Lines $CacheLines -Name "CMAKE_CXX_FLAGS_RELEASE"
    if ($cacheFlags -notmatch '(?i)(?:^|\s)/O2(?=\s|$)' -or
        $cacheFlags -match '(?i)(?:^|\s)/Od(?=\s|$)' -or
        $cacheFlags -notmatch '(?i)(?:^|\s)/D\s*NDEBUG(?=\s|$)') {
        throw "Release C++ cache flags must include /O2 and /DNDEBUG and must not include /Od."
    }
    $projectPath = Join-Path $BuildRoot "cmake\next_app\ClassMngrNext.vcxproj"
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) { throw "Generated Release app project is missing: $projectPath" }
    $project = [System.Xml.XmlDocument]::new()
    $project.Load($projectPath)
    $namespace = [System.Xml.XmlNamespaceManager]::new($project.NameTable)
    $namespace.AddNamespace("msbuild", "http://schemas.microsoft.com/developer/msbuild/2003")
    $groups = @($project.SelectNodes("//msbuild:ItemDefinitionGroup", $namespace) | Where-Object {
        $_.GetAttribute("Condition") -match "'\$\(Configuration\)\|\$\(Platform\)'\s*==\s*'Release\|x64'"
    })
    if ($groups.Count -ne 1) { throw "Expected one Release|x64 app compiler group; found $($groups.Count)." }
    $compile = $groups[0].SelectSingleNode("msbuild:ClCompile", $namespace)
    $optimization = $compile.SelectSingleNode("msbuild:Optimization", $namespace).InnerText.Trim()
    $definitions = $compile.SelectSingleNode("msbuild:PreprocessorDefinitions", $namespace).InnerText
    if ($optimization -notin @("MaxSpeed", "MinSpace") -or $definitions -notmatch '(^|;)NDEBUG(;|%\()') {
        throw "Generated Release|x64 app project does not enable optimization and NDEBUG."
    }
    return [ordered]@{
        generator = $generator
        configuration = "Release"
        platform = $platform
        pointerSizeBytes = 8
        cxxFlagsRelease = $cacheFlags
        appProjectPath = [System.IO.Path]::GetFullPath($projectPath)
        appProjectSha256 = Get-FileSha256 -Path $projectPath
        projectOptimization = $optimization
        projectDefinesNdebug = $true
    }
}

function Get-PackageFontIdentity {
    param([string]$LogicalId, [string]$RelativePath, [string]$ExpectedType, [object]$Catalog, [string]$Root)
    $records = @($Catalog.records | Where-Object { $_.logical_id -eq $LogicalId })
    if ($records.Count -ne 1) { throw "Package catalog has $($records.Count) records for $LogicalId." }
    $record = $records[0]
    if ($record.expected_type -ne $ExpectedType -or $record.package_relative_path -ne $RelativePath) {
        throw "Package catalog identity differs for $LogicalId."
    }
    $baked = $record.provenance.baked
    $path = [System.IO.Path]::GetFullPath((Join-Path $Root $RelativePath))
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Packaged font is missing: $path" }
    $actualHash = Get-FileSha256 -Path $path
    $actualBytes = (Get-Item -LiteralPath $path).Length
    if ($actualHash -ne ([string]$baked.sha256).ToLowerInvariant() -or
        $actualBytes -ne [Int64]$baked.bytes) {
        throw "Packaged font bytes do not match the package catalog: $path"
    }
    return [ordered]@{
        logicalId = $LogicalId
        path = $path
        sizeBytes = [Int64]$actualBytes
        sha256 = $actualHash
        catalogSha256 = ([string]$baked.sha256).ToLowerInvariant()
        catalogBytes = [Int64]$baked.bytes
    }
}

function Get-Median {
    param([double[]]$Values)
    if ($Values.Count -eq 0) { return $null }
    $orderedValues = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($orderedValues.Count / 2)
    if (($orderedValues.Count % 2) -eq 1) { return [double]$orderedValues[$middle] }
    return ([double]$orderedValues[$middle - 1] + [double]$orderedValues[$middle]) / 2.0
}

function Save-Manifest {
    Write-JsonFile -Path $ManifestPath -Value $Manifest
}

function Get-Delta {
    param([object]$Before, [object]$After)
    return [ordered]@{
        workingSetBytes = [Int64]$After.memory.workingSetBytes - [Int64]$Before.memory.workingSetBytes
        privateUsageBytes = [Int64]$After.memory.privateUsageBytes - [Int64]$Before.memory.privateUsageBytes
        privateWorkingSetBytes = if ($After.memory.privateWorkingSetBytesAvailable -and $Before.memory.privateWorkingSetBytesAvailable) {
            [Int64]$After.memory.privateWorkingSetBytes - [Int64]$Before.memory.privateWorkingSetBytes
        } else { $null }
    }
}

function Invoke-ProbeProcess {
    param([string]$Scenario, [int]$Repetition, [string]$ProbePath, [string]$ProbeSha, [string]$InterPath, [string]$PretendardPath, [string]$PackageRoot, [string]$QtBin, [string]$PluginRoot)
    $runName = "{0}-run-{1:D2}" -f $Scenario, $Repetition
    $stdoutPath = Join-Path $RunsDirectory ($runName + ".stdout.jsonl")
    $stderrPath = Join-Path $RunsDirectory ($runName + ".stderr.txt")
    $run = [ordered]@{
        scenario = $Scenario
        repetition = $Repetition
        startedAtUtc = Get-UtcTimestamp
        completedAtUtc = $null
        pid = $null
        processCreatedAtUtc = $null
        exitCode = $null
        succeeded = $false
        executablePath = $ProbePath
        executableSha256 = $ProbeSha
        stdoutPath = $stdoutPath
        stdoutSha256 = $null
        stderrPath = $stderrPath
        stderrSha256 = $null
        snapshotCount = 0
        events = @()
        summary = $null
    }
    $process = $null
    $stdout = ""
    $stderr = ""
    try {
        $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $ProbePath
        $startInfo.WorkingDirectory = $PackageRoot
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $startInfo.ArgumentList.Add("--scenario")
        $startInfo.ArgumentList.Add($Scenario)
        $startInfo.ArgumentList.Add("--inter")
        $startInfo.ArgumentList.Add($InterPath)
        $startInfo.ArgumentList.Add("--pretendard")
        $startInfo.ArgumentList.Add($PretendardPath)
        $startInfo.Environment["PATH"] = "$PackageRoot;$QtBin;$($env:PATH)"
        $startInfo.Environment["QT_PLUGIN_PATH"] = $PluginRoot
        $startInfo.Environment["QT_QPA_PLATFORM_PLUGIN_PATH"] = Join-Path $PluginRoot "platforms"
        $startInfo.Environment["QT_QPA_PLATFORM"] = "windows"
        foreach ($name in @("QT_QPA_FONTDIR", "QT_QPA_PLATFORMTHEME", "QT_SCALE_FACTOR", "QT_SCALE_FACTOR_ROUNDING_POLICY", "QT_AUTO_SCREEN_SCALE_FACTOR")) {
            $null = $startInfo.Environment.Remove($name)
        }

        $process = [System.Diagnostics.Process]::new()
        $process.StartInfo = $startInfo
        if (-not $process.Start()) { throw "Could not start $ProbePath." }
        $run.pid = $process.Id
        $run.processCreatedAtUtc = $process.StartTime.ToUniversalTime().ToString("o")
        if (-not $UniquePids.Add([int]$process.Id)) { throw "Probe process reused PID $($process.Id)." }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        $run.exitCode = $process.ExitCode
        $run.completedAtUtc = Get-UtcTimestamp
        [System.IO.File]::WriteAllText($stdoutPath, $stdout, [System.Text.UTF8Encoding]::new($false))
        [System.IO.File]::WriteAllText($stderrPath, $stderr, [System.Text.UTF8Encoding]::new($false))
        $run.stdoutSha256 = Get-FileSha256 -Path $stdoutPath
        $run.stderrSha256 = Get-FileSha256 -Path $stderrPath
        if ($process.ExitCode -ne 0) { throw "Probe process $runName exited with $($process.ExitCode): $stderr" }

        $records = [System.Collections.Generic.List[object]]::new()
        foreach ($line in ($stdout -split "`r?`n")) {
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            $records.Add(($line | ConvertFrom-Json -AsHashtable))
        }
        $snapshots = @($records | Where-Object { $_.recordType -eq "snapshot" })
        $completions = @($records | Where-Object { $_.recordType -eq "complete" })
        if ($completions.Count -ne 1 -or $snapshots.Count -lt 5) {
            throw "Probe process $runName produced incomplete snapshot or completion evidence."
        }
        $expectedEvents = [System.Collections.Generic.List[string]]::new()
        $expectedEvents.Add("before-qapplication")
        $expectedEvents.Add("after-qapplication")
        $order = @(switch ($Scenario) {
            "inter-only" { @("inter") }
            "pretendard-only" { @("pretendard") }
            "inter-then-pretendard" { @("inter", "pretendard") }
            "pretendard-then-inter" { @("pretendard", "inter") }
            default { }
        })
        foreach ($font in $order) {
            if ($font -eq "pretendard") {
                $expectedEvents.Add("before-pretendard-file-read")
                $expectedEvents.Add("after-pretendard-file-read")
            }
            $expectedEvents.Add("before-$font-registration")
            $expectedEvents.Add("after-$font-registration")
            $expectedEvents.Add("before-$font-family-enumeration")
            $expectedEvents.Add("after-$font-family-enumeration")
        }
        if ($order.Count -eq 0) {
            $expectedEvents.Add("startup-font-application-skipped")
        } else {
            $expectedEvents.Add("before-startup-font-application")
            $expectedEvents.Add("after-startup-font-application")
        }
        $expectedEvents.Add("before-system-font-families-query")
        $expectedEvents.Add("after-system-font-families-query")
        $actualEvents = @($snapshots | ForEach-Object { [string]$_.event })
        if (($actualEvents -join "|") -ne ($expectedEvents -join "|")) {
            throw "Probe process $runName snapshot order/schema mismatch: $($actualEvents -join ', ')."
        }
        $previousTime = [DateTimeOffset]::MinValue
        foreach ($index in 0..($snapshots.Count - 1)) {
            $snapshot = $snapshots[$index]
            if ($snapshot.schema -ne "classmngr-phase9-font-allocation-snapshot-v1" -or
                [int]$snapshot.sequence -ne $index -or
                [int]$snapshot.pid -ne [int]$process.Id -or
                $snapshot.architecture -ne "x64" -or
                -not [string]::Equals([string]$snapshot.executablePath, $ProbePath, [StringComparison]::OrdinalIgnoreCase) -or
                $snapshot.qtVersion -ne "6.12.0" -or
                -not $snapshot.memory.isAvailable -or
                -not $snapshot.memory.workingSetBytesAvailable -or
                -not $snapshot.memory.privateUsageBytesAvailable -or
                [Int64]$snapshot.memory.workingSetBytes -le 0 -or
                [Int64]$snapshot.memory.privateUsageBytes -le 0) {
                throw "Probe process $runName has invalid identity or memory metrics at sequence $index."
            }
            $captured = [DateTimeOffset]::Parse([string]$snapshot.capturedAtUtc)
            if ($captured -lt $previousTime) { throw "Probe process $runName timestamps are out of order." }
            $previousTime = $captured
        }
        $completion = $completions[0]
        if ($completion.schema -ne $Schema -or
            [int]$completion.pid -ne [int]$process.Id -or
            $completion.architecture -ne "x64" -or
            $completion.qtVersion -ne "6.12.0" -or
            [int]$completion.snapshotCount -ne $snapshots.Count) {
            throw "Probe process $runName completion record does not match its snapshots."
        }
        $run.snapshotCount = $snapshots.Count
        $run.events = $actualEvents
        $byEvent = @{}
        foreach ($snapshot in $snapshots) { $byEvent[[string]$snapshot.event] = $snapshot }
        $deltas = [ordered]@{
            qapplicationInitialization = Get-Delta $byEvent["before-qapplication"] $byEvent["after-qapplication"]
            systemFontFamiliesQuery = Get-Delta $byEvent["before-system-font-families-query"] $byEvent["after-system-font-families-query"]
        }
        foreach ($font in $order) {
            $deltas[($font + "Registration")] = Get-Delta -Before $byEvent["before-$font-registration"] -After $byEvent["after-$font-registration"]
            $deltas[($font + "FamilyEnumeration")] = Get-Delta -Before $byEvent["before-$font-family-enumeration"] -After $byEvent["after-$font-family-enumeration"]
            if ($font -eq "pretendard") {
                $deltas.pretendardFileRead = Get-Delta -Before $byEvent["before-pretendard-file-read"] -After $byEvent["after-pretendard-file-read"]
            }
        }
        if ($order.Count -gt 0) {
            $deltas.startupFontApplication = Get-Delta $byEvent["before-startup-font-application"] $byEvent["after-startup-font-application"]
        }
        $run.summary = [ordered]@{
            scenario = $Scenario
            repetition = $Repetition
            pid = [int]$process.Id
            capturedAtUtc = @($snapshots | ForEach-Object { $_.capturedAtUtc })
            finalWorkingSetBytes = [Int64]$byEvent["after-system-font-families-query"].memory.workingSetBytes
            finalPrivateUsageBytes = [Int64]$byEvent["after-system-font-families-query"].memory.privateUsageBytes
            finalPrivateWorkingSetBytes = if ($byEvent["after-system-font-families-query"].memory.privateWorkingSetBytesAvailable) {
                [Int64]$byEvent["after-system-font-families-query"].memory.privateWorkingSetBytes
            } else { $null }
            fullSystemFamilyCount = [int]$byEvent["after-system-font-families-query"].details.familyCount
            locale = [string]$completion.locale
            deltas = $deltas
        }
        $run.succeeded = $true
        $Manifest.executionOrder.Add("${Repetition}:$Scenario")
        $Manifest.processRuns = $ProcessRuns
        Save-Manifest
        return $run
    } catch {
        $run.completedAtUtc = if ($null -eq $run.completedAtUtc) { Get-UtcTimestamp } else { $run.completedAtUtc }
        if (-not (Test-Path -LiteralPath $stdoutPath)) {
            [System.IO.File]::WriteAllText($stdoutPath, $stdout, [System.Text.UTF8Encoding]::new($false))
        }
        if (-not (Test-Path -LiteralPath $stderrPath)) {
            [System.IO.File]::WriteAllText($stderrPath, $stderr, [System.Text.UTF8Encoding]::new($false))
        }
        if (Test-Path -LiteralPath $stdoutPath) { $run.stdoutSha256 = Get-FileSha256 -Path $stdoutPath }
        if (Test-Path -LiteralPath $stderrPath) { $run.stderrSha256 = Get-FileSha256 -Path $stderrPath }
        throw
    } finally {
        if ($null -ne $process) { $process.Dispose() }
        $run.completedAtUtc = if ($null -eq $run.completedAtUtc) { Get-UtcTimestamp } else { $run.completedAtUtc }
        if (-not $ProcessRuns.Contains($run)) { $ProcessRuns.Add($run) }
        $Manifest.processRuns = $ProcessRuns
        Save-Manifest
    }
}

try {
    Save-Manifest
    $sourceStart = Get-CandidateSourceIdentity -Root $RepositoryRoot
    Write-JsonFile -Path $SourceStartPath -Value $sourceStart.files
    $buildInfoPath = Join-Path $BuildDirectory "generated\core\build_info.h"
    $buildInfo = [System.IO.File]::ReadAllText($buildInfoPath)
    $revisionMatch = [regex]::Match($buildInfo, 'GitRevision\s*=\s*"([^"]+)"')
    $buildRevision = if ($revisionMatch.Success) { $revisionMatch.Groups[1].Value } else { "unknown" }

    $cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { throw "Release CMakeCache is missing: $cachePath" }
    $cacheLines = @(Get-Content -LiteralPath $cachePath)
    $releaseConfiguration = Get-ReleaseConfiguration -CacheLines $cacheLines -BuildRoot $BuildDirectory
    if ($releaseConfiguration.pointerSizeBytes -ne 8) { throw "Release build tree is not x64." }
    $qt6Directory = [System.IO.Path]::GetFullPath((Get-CacheValue -Lines $cacheLines -Name "Qt6_DIR"))
    $derivedQtPrefix = [System.IO.Path]::GetFullPath((Join-Path $qt6Directory "..\..\.."))
    if ([string]::IsNullOrWhiteSpace($QtPrefix)) {
        $QtPrefix = $derivedQtPrefix
    } else {
        $QtPrefix = [System.IO.Path]::GetFullPath($QtPrefix)
        if (-not [string]::Equals($QtPrefix, $derivedQtPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "QtPrefix does not match the Release build CMakeCache Qt6_DIR."
        }
    }
    $qtCorePrefix = Join-Path $QtPrefix "bin\Qt6Core.dll"
    $qtVersion = Get-BinaryQtVersion -Path $qtCorePrefix
    if ($qtVersion -ne "6.12.0") { throw "The probe requires the packaged Qt 6.12.0 runtime; found $qtVersion." }
    $qtRuntimeDefinitions = @(
        [ordered]@{ name = "Qt6Core.dll"; package = (Join-Path $PackageDirectory "Qt6Core.dll"); prefix = (Join-Path $QtPrefix "bin\Qt6Core.dll") },
        [ordered]@{ name = "Qt6Gui.dll"; package = (Join-Path $PackageDirectory "Qt6Gui.dll"); prefix = (Join-Path $QtPrefix "bin\Qt6Gui.dll") },
        [ordered]@{ name = "Qt6Widgets.dll"; package = (Join-Path $PackageDirectory "Qt6Widgets.dll"); prefix = (Join-Path $QtPrefix "bin\Qt6Widgets.dll") },
        [ordered]@{ name = "qwindows.dll"; package = (Join-Path $PackageDirectory "plugins\platforms\qwindows.dll"); prefix = (Join-Path $QtPrefix "plugins\platforms\qwindows.dll") }
    )
    $qtRuntimeFiles = [System.Collections.Generic.List[object]]::new()
    foreach ($file in $qtRuntimeDefinitions) {
        foreach ($path in @($file.package, $file.prefix)) {
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required Qt runtime file is missing: $path" }
        }
        $packageHash = Get-FileSha256 -Path $file.package
        $prefixHash = Get-FileSha256 -Path $file.prefix
        if ($packageHash -ne $prefixHash) { throw "Packaged Qt runtime differs from CMakeCache Qt6_DIR: $($file.name)" }
        $qtRuntimeFiles.Add([ordered]@{
            name = $file.name
            packagePath = [System.IO.Path]::GetFullPath($file.package)
            packageSha256 = $packageHash
            qtPrefixPath = [System.IO.Path]::GetFullPath($file.prefix)
            qtPrefixSha256 = $prefixHash
        })
    }

    $catalogPath = Join-Path $PackageDirectory "resources\resource-catalog.json"
    if (-not (Test-Path -LiteralPath $catalogPath -PathType Leaf)) { throw "Packaged resource catalog is missing: $catalogPath" }
    $catalog = Get-Content -LiteralPath $catalogPath -Raw | ConvertFrom-Json
    $inter = Get-PackageFontIdentity -LogicalId "resource:assets/fonts/Inter.ttc" -RelativePath "assets/fonts/Inter.ttc" -ExpectedType "ttc" -Catalog $catalog -Root (Join-Path $PackageDirectory "resources")
    $pretendard = Get-PackageFontIdentity -LogicalId "resource:assets/fonts/PretendardVariable.ttf" -RelativePath "assets/fonts/PretendardVariable.ttf" -ExpectedType "ttf" -Catalog $catalog -Root (Join-Path $PackageDirectory "resources")
    $appPath = Join-Path $PackageDirectory "ClassMngr.exe"
    if (-not (Test-Path -LiteralPath $appPath -PathType Leaf)) { throw "Packaged ClassMngr.exe is missing: $appPath" }
    if ((Get-PeArchitecture -Path $appPath) -ne "x64") { throw "Packaged ClassMngr.exe is not Windows x64." }

    $cmake = (Get-Command cmake -ErrorAction Stop).Source
    $cmakeHash = Get-FileSha256 -Path $cmake
    $buildStartedAtUtc = Get-UtcTimestamp
    $buildCompletedAtUtc = $null
    $buildExitCode = 0
    $buildCommand = "`"$cmake`" --build `"$BuildDirectory`" --config Release --target ClassMngrNextFontAllocationProbe --parallel 4"
    $externalBuildLogPath = $null
    if ($SkipBuild) {
        if ([string]::IsNullOrWhiteSpace($ExistingBuildLog) -or
            -not (Test-Path -LiteralPath $ExistingBuildLog -PathType Leaf)) {
            throw "-SkipBuild requires an existing successful build log."
        }
        $externalBuildLogPath = [System.IO.Path]::GetFullPath($ExistingBuildLog)
        $externalBuildLog = [System.IO.File]::ReadAllText($externalBuildLogPath)
        if ($externalBuildLog -notmatch '(?m)^exitCode:\s*0\s*$' -or
            $externalBuildLog -notmatch 'ClassMngrNextFontAllocationProbe') {
            throw "The supplied external build log does not prove a successful probe target build."
        }
        $startedMatch = [regex]::Match($externalBuildLog, '(?m)^startedAtUtc:\s*(.+)$')
        $completedMatch = [regex]::Match($externalBuildLog, '(?m)^completedAtUtc:\s*(.+)$')
        if ($startedMatch.Success) { $buildStartedAtUtc = $startedMatch.Groups[1].Value.Trim() }
        if ($completedMatch.Success) { $buildCompletedAtUtc = $completedMatch.Groups[1].Value.Trim() }
        if (-not $completedMatch.Success) { throw "The supplied external build log has no completion timestamp." }
        [System.IO.File]::Copy($externalBuildLogPath, $BuildLogPath)
        $buildCommand = "prebuilt target; see external build log"
    } else {
        $buildOutput = @(& $cmake --build $BuildDirectory --config Release --target ClassMngrNextFontAllocationProbe --parallel 4 2>&1)
        $buildExitCode = $LASTEXITCODE
        $buildCompletedAtUtc = Get-UtcTimestamp
        $buildLogLines = @(
            "command: $buildCommand"
            "startedAtUtc: $buildStartedAtUtc"
            "--- build output ---"
        ) + @($buildOutput | ForEach-Object { [string]$_ }) + @(
            "--- end build output ---"
            "exitCode: $buildExitCode"
            "completedAtUtc: $buildCompletedAtUtc"
        )
        [System.IO.File]::WriteAllLines($BuildLogPath, $buildLogLines, [System.Text.UTF8Encoding]::new($false))
        if ($buildExitCode -ne 0) { throw "Release probe build failed with exit code $buildExitCode; see $BuildLogPath" }
    }
    $cacheSha = Get-FileSha256 -Path $cachePath
    $probeProjectPath = Join-Path $BuildDirectory "ClassMngrNextFontAllocationProbe.vcxproj"
    $probePath = Join-Path $BuildDirectory "phase9-probe\ClassMngrNextFontAllocationProbe.exe"
    foreach ($path in @($probeProjectPath, $probePath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Release probe build output is missing: $path" }
    }
    if ((Get-PeArchitecture -Path $probePath) -ne "x64") { throw "Probe executable is not Windows x64." }
    $probeSha = Get-FileSha256 -Path $probePath
    $qtBin = Join-Path $QtPrefix "bin"
    $pluginRoot = Join-Path $PackageDirectory "plugins"

    $Manifest.sourceIdentity = [ordered]@{
        revision = $buildRevision
        sha256AtRunStart = $sourceStart.sha256
        includedRoots = $IncludedRoots
        fileCount = $sourceStart.fileCount
        inventoryFileAtStart = "source-files-at-start.json"
        inventoryFileAtEnd = "source-files-at-end.json"
        stableDuringRun = $null
        sha256AtRunEnd = $null
    }
    $Manifest.configuration = [ordered]@{
        buildDirectory = $BuildDirectory
        cmakeCachePath = [System.IO.Path]::GetFullPath($cachePath)
        cmakeCacheSha256 = $cacheSha
        releaseCompiler = $releaseConfiguration
        appPath = [System.IO.Path]::GetFullPath($appPath)
        appSha256 = Get-FileSha256 -Path $appPath
        appArchitecture = "x64"
        probeBuild = [ordered]@{
            cmakePath = $cmake
            cmakeSha256 = $cmakeHash
            command = $buildCommand
            startedAtUtc = $buildStartedAtUtc
            completedAtUtc = $buildCompletedAtUtc
            exitCode = $buildExitCode
            logPath = $BuildLogPath
            logSha256 = Get-FileSha256 -Path $BuildLogPath
            externalLogPath = $externalBuildLogPath
            externalLogSha256 = if ($null -ne $externalBuildLogPath) { Get-FileSha256 -Path $externalBuildLogPath } else { $null }
        }
    }
    $Manifest.qtRuntime = [ordered]@{
        version = $qtVersion
        qt6Directory = $qt6Directory
        qtPrefix = $QtPrefix
        qtCoreSha256 = Get-FileSha256 -Path $qtCorePrefix
        packagedRuntimeFiles = $qtRuntimeFiles
        platform = "windows"
        platformPlugin = "qwindows"
        fontBackend = "Qt 6.12 Windows DirectWrite"
    }
    $Manifest.package = [ordered]@{
        root = $PackageDirectory
        catalogPath = [System.IO.Path]::GetFullPath($catalogPath)
        catalogSha256 = Get-FileSha256 -Path $catalogPath
        fonts = @($inter, $pretendard)
    }
    $Manifest.probe = [ordered]@{
        path = [System.IO.Path]::GetFullPath($probePath)
        sha256 = $probeSha
        architecture = "x64"
        schema = $Schema
        qtVersion = $qtVersion
        processMemoryProvider = "PlatformProcessMemorySnapshotProvider"
        fontRegistrationRoutes = [ordered]@{
            inter = "QFontDatabase::addApplicationFont(file)"
            pretendard = "QFile::readAll + QFontDatabase::addApplicationFontFromData(bytes)"
        }
    }
    Save-Manifest

    for ($repetition = 1; $repetition -le $Repetitions; $repetition++) {
        $offset = ($repetition - 1) % $Scenarios.Count
        for ($position = 0; $position -lt $Scenarios.Count; $position++) {
            $scenario = $Scenarios[($position + $offset) % $Scenarios.Count]
            $repetitionForScenario = @($ProcessRuns | Where-Object { $_.scenario -eq $scenario }).Count + 1
            $run = Invoke-ProbeProcess -Scenario $scenario -Repetition $repetitionForScenario -ProbePath $probePath -ProbeSha $probeSha -InterPath $inter.path -PretendardPath $pretendard.path -PackageRoot $PackageDirectory -QtBin $qtBin -PluginRoot $pluginRoot
            Add-JsonLine -Path $ProcessSummaryPath -Value $run.summary
        }
    }

    $sourceEnd = Get-CandidateSourceIdentity -Root $RepositoryRoot
    Write-JsonFile -Path $SourceEndPath -Value $sourceEnd.files
    $Manifest.sourceIdentity.sha256AtRunEnd = $sourceEnd.sha256
    $Manifest.sourceIdentity.stableDuringRun = $sourceEnd.sha256 -eq $sourceStart.sha256
    if (-not $Manifest.sourceIdentity.stableDuringRun) { throw "Candidate source changed during the probe run." }

    $deltaNames = [System.Collections.Generic.List[string]]::new()
    foreach ($run in $ProcessRuns) {
        foreach ($name in $run.summary.deltas.Keys) {
            if (-not $deltaNames.Contains([string]$name)) { $deltaNames.Add([string]$name) }
        }
    }
    $medianRows = [System.Collections.Generic.List[object]]::new()
    foreach ($scenario in $Scenarios) {
        $scenarioRuns = @($ProcessRuns | Where-Object { $_.scenario -eq $scenario })
        if ($scenarioRuns.Count -ne $Repetitions) { throw "Scenario $scenario has $($scenarioRuns.Count) runs, expected $Repetitions." }
        foreach ($deltaName in $deltaNames) {
            foreach ($metric in @("workingSetBytes", "privateUsageBytes", "privateWorkingSetBytes")) {
                $values = @($scenarioRuns | ForEach-Object {
                    if ($_.summary.deltas.Contains($deltaName)) { $_.summary.deltas[$deltaName][$metric] }
                } | Where-Object { $null -ne $_ } | ForEach-Object { [double]$_ })
                if ($values.Count -eq 0) { continue }
                $medianRows.Add([ordered]@{
                    scenario = $scenario
                    operation = $deltaName
                    metric = $metric
                    sampleCount = $values.Count
                    medianDeltaBytes = Get-Median -Values $values
                })
            }
        }
    }
    $medianRows | Export-Csv -LiteralPath $MediansCsvPath -NoTypeInformation -Encoding utf8NoBOM
    Write-JsonFile -Path $SummaryPath -Value ([ordered]@{
        schema = $Schema
        taskId = $TaskId
        runId = $RunId
        status = "complete"
        repetitionsPerScenario = $Repetitions
        scenarioCount = $Scenarios.Count
        totalProcessCount = $ProcessRuns.Count
        uniquePidCount = $UniquePids.Count
        sourceSha256 = $sourceEnd.sha256
        qtVersion = $qtVersion
        packageFontHashes = @($inter, $pretendard)
        scenarios = $Scenarios
        executionOrder = @($Manifest.executionOrder)
        processes = @($ProcessRuns | ForEach-Object { $_.summary })
        scenarioMedians = @($medianRows)
        processSummaryFile = $ProcessSummaryPath
        scenarioMediansCsv = $MediansCsvPath
    })
    $Manifest.status = "complete"
} catch {
    $Manifest.status = "failed"
    $Manifest.failure = $_.Exception.Message
} finally {
    $Manifest.completedAtUtc = Get-UtcTimestamp
    Save-Manifest
}

if ($Manifest.status -ne "complete") {
    throw "Font allocation probe did not complete: $($Manifest.failure). Evidence retained at $EvidenceDirectory"
}

Write-Output "Evidence: $EvidenceDirectory"
Write-Output "Probe processes: $($ProcessRuns.Count); repetitions per scenario: $Repetitions; source SHA-256: $($Manifest.sourceIdentity.sha256AtRunEnd)"
