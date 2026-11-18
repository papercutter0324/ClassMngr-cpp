<#
.SYNOPSIS
Measures the source-bound packaged Windows x64 Release app for five idle minutes.

.DESCRIPTION
Each invocation creates a new run directory beneath EvidenceRoot. The package
is staged locally from the Release package build, and only the two normal-use
workspace fixtures are measured. The startup-ready diagnostic, startup-boundary
snapshots, and all idle process samples are retained beside package identities.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Join-Path $PSScriptRoot "..\.."),
    [string]$BuildDirectory = "build\windows-x64-release",
    [string]$QtPrefix = "",
    [string]$EvidenceRoot = "artifacts\phase9\packaged-idle-memory",
    [int]$ReadinessTimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$TaskId = "P9_06_PACKAGED_STARTUP_IDLE_GATE"
$ManifestSchema = "classmngr-phase9-packaged-idle-memory-v3"
$StartupBoundaryMetricsSchema = "classmngr-phase9-startup-boundary-metrics-v1"
$StartupBoundaryMaximumDelayMilliseconds = 2000
$ExpectedStartupBoundaryEvents = @(
    [ordered]@{ eventId = "theme-loaded"; kind = "stage"; value = "theme-loaded" },
    [ordered]@{ eventId = "inter-font-resource-loaded"; kind = "resource"; value = "resource:assets/fonts/Inter-Regular.ttf" },
    [ordered]@{ eventId = "pretendard-font-resource-loaded"; kind = "resource"; value = "resource:assets/fonts/PretendardVariable.ttf" },
    [ordered]@{ eventId = "required-fonts-loaded"; kind = "stage"; value = "required-fonts-loaded" },
    [ordered]@{ eventId = "shell-created"; kind = "stage"; value = "shell-created" }
)
$ExpectedInterFontResourceIds = @(
    "resource:assets/fonts/Inter-Regular.ttf",
    "resource:assets/fonts/Inter-Medium.ttf",
    "resource:assets/fonts/Inter-SemiBold.ttf",
    "resource:assets/fonts/Inter-Bold.ttf",
    "resource:assets/fonts/Inter-BoldItalic.ttf",
    "resource:assets/fonts/Inter-Black.ttf"
)
$ThresholdBytes = [Int64]262144000
$IdleSeconds = 300
$SampleIntervalSeconds = 1
$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$BuildDirectory = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    [System.IO.Path]::GetFullPath($BuildDirectory)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot $BuildDirectory))
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
$ValidatorPath = Join-Path $PSScriptRoot "validate_packaged_idle_memory_evidence.py"
$ManifestPath = Join-Path $EvidenceDirectory "manifest.json"
$MetricsPath = Join-Path $EvidenceDirectory "metrics.jsonl"
$StartupLogPath = Join-Path $EvidenceDirectory "startup-log.jsonl"
$PackageFilesPath = Join-Path $EvidenceDirectory "package-files.json"
$SourceFilesPath = Join-Path $EvidenceDirectory "source-files.json"
$StartupBoundaryMetricsPath = Join-Path $EvidenceDirectory "startup-boundary-metrics.jsonl"
$BuildLogPath = Join-Path $EvidenceDirectory "cmake-build.log"
$SummaryPath = Join-Path $EvidenceDirectory "summary.json"
$ValidatorOutputPath = Join-Path $EvidenceDirectory "validator-output.txt"
$TemporaryRoot = $null
$StartupEvents = [System.Collections.Generic.List[object]]::new()
$BoundaryEventCounts = @{}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class Phase9NativeProcessChecks
{
    private const ushort ImageFileMachineUnknown = 0x0000;
    private const ushort ImageFileMachineAmd64 = 0x8664;

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool IsWow64Process2(IntPtr process, out ushort processMachine, out ushort nativeMachine);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);

    [DllImport("kernel32.dll", EntryPoint = "QueryFullProcessImageNameW", ExactSpelling = true,
        SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool QueryFullProcessImageNameW(IntPtr process, uint flags,
        StringBuilder executablePath, ref int pathLength);

    public static string GetProcessImagePath(IntPtr process)
    {
        StringBuilder executablePath = new StringBuilder(32768);
        int pathLength = executablePath.Capacity;
        if (!QueryFullProcessImageNameW(process, 0, executablePath, ref pathLength))
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(), "QueryFullProcessImageNameW failed.");
        return executablePath.ToString();
    }

    public static bool IsNativeX64Process(IntPtr process)
    {
        ushort processMachine;
        ushort nativeMachine;
        if (!IsWow64Process2(process, out processMachine, out nativeMachine))
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(), "IsWow64Process2 failed.");
        return processMachine == ImageFileMachineUnknown && nativeMachine == ImageFileMachineAmd64;
    }
}
'@

function Get-UtcTimestamp {
    return [DateTimeOffset]::UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss.fffffff'Z'")
}

function Write-JsonFile {
    param([string]$Path, [object]$Value)
    $json = $Value | ConvertTo-Json -Depth 40
    [System.IO.File]::WriteAllText($Path, $json + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
}

function Add-JsonLine {
    param([string]$Path, [object]$Value)
    $line = $Value | ConvertTo-Json -Depth 16 -Compress
    [System.IO.File]::AppendAllText($Path, $line + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
}

function Get-StringSha256 {
    param([string]$Value)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
    } finally { $sha.Dispose() }
}

function Get-FileSha256 {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-QtBinaryVersion {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Qt binary was not found: $Path" }
    $fileInfo = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
    $versionText = if (-not [string]::IsNullOrWhiteSpace($fileInfo.ProductVersion)) { $fileInfo.ProductVersion } else { $fileInfo.FileVersion }
    $match = [regex]::Match([string]$versionText, '\d+\.\d+\.\d+')
    if (-not $match.Success) { throw "Qt binary has no readable semantic version: $Path" }
    return $match.Value
}

function Get-PeArchitecture {
    param([string]$Path)
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $reader = [System.IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Image has no MZ header: $Path" }
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 64 -or $peOffset -gt ($stream.Length - 6)) { throw "Image has an invalid PE header offset: $Path" }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw "Image has no PE signature: $Path" }
        $machine = $reader.ReadUInt16()
        if ($machine -eq 0x8664) { return "x64" }
        if ($machine -eq 0xaa64) { return "arm64" }
        return ("unknown-0x{0:x4}" -f $machine)
    } finally { $stream.Dispose() }
}

function Invoke-CheckedCommand {
    param([string]$FilePath, [string[]]$Arguments, [string]$Description)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Description failed with exit code $LASTEXITCODE." }
}

function Get-CandidateSourceIdentity {
    param([string]$Root, [string]$Revision)
    $includedRoots = @(
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
    $items = [System.Collections.Generic.List[object]]::new()
    foreach ($relativeRoot in $includedRoots) {
        $rootPath = Join-Path $Root $relativeRoot
        if (-not (Test-Path -LiteralPath $rootPath)) { throw "Source identity root was not found: $relativeRoot" }
        $files = if (Test-Path -LiteralPath $rootPath -PathType Leaf) {
            @(Get-Item -LiteralPath $rootPath)
        } else {
            @(Get-ChildItem -LiteralPath $rootPath -File -Recurse)
        }
        $files = @($files | Where-Object {
            ($_.FullName -notmatch '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]') -and ($_.Extension -notin @(".pyc", ".pyo"))
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
    if ($sorted.Count -eq 0) { throw "Candidate source identity contains no files." }
    $canonical = @($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    return [ordered]@{
        revision = $Revision
        sha256 = Get-StringSha256 -Value ($canonical + "`n")
        includedRoots = $includedRoots
        fileCount = $sorted.Count
        files = $sorted
    }
}

function Get-RequiredCMakeCacheValue {
    param([string[]]$Lines, [string]$Name)
    $escapedName = [regex]::Escape($Name)
    $cacheMatches = @($Lines | Where-Object { $_ -match "^${escapedName}:[^=]+=" })
    if ($cacheMatches.Count -ne 1) { throw "Expected exactly one $Name entry in CMakeCache.txt; found $($cacheMatches.Count)." }
    return (($cacheMatches[0] -split "=", 2)[1])
}

function Test-OptimizedReleaseFlags {
    param([string]$Flags, [string]$Label)
    $optimizationTokens = @([regex]::Matches($Flags, '(?i)(?:^|\s)[/-](?:O1|O2|Ox)(?=\s|$)'))
    if ($optimizationTokens.Count -eq 0 -or $Flags -match '(?i)(?:^|\s)[/-]Od(?=\s|$)') {
        throw "$Label must include /O1, /O2, or /Ox and must not disable optimization."
    }
    if ($Flags -notmatch '(?i)(?:^|\s)[/-]D\s*NDEBUG(?=\s|$)') {
        throw "$Label must define NDEBUG with /DNDEBUG."
    }
    return [ordered]@{
        raw = $Flags
        optimizationOptions = @($optimizationTokens | ForEach-Object { $_.Value.Trim() })
        definesNdebug = $true
    }
}

function Get-VerifiedReleaseCompilerConfiguration {
    param([string[]]$CacheLines, [string]$Root)
    $generator = Get-RequiredCMakeCacheValue -Lines $CacheLines -Name "CMAKE_GENERATOR"
    if ($generator -notmatch '^Visual Studio') { throw "Release gate requires the MSVC Visual Studio generator; found '$generator'." }
    $cFlags = Get-RequiredCMakeCacheValue -Lines $CacheLines -Name "CMAKE_C_FLAGS_RELEASE"
    $cxxFlags = Get-RequiredCMakeCacheValue -Lines $CacheLines -Name "CMAKE_CXX_FLAGS_RELEASE"
    $verifiedCFlags = Test-OptimizedReleaseFlags -Flags $cFlags -Label "CMAKE_C_FLAGS_RELEASE"
    $verifiedCxxFlags = Test-OptimizedReleaseFlags -Flags $cxxFlags -Label "CMAKE_CXX_FLAGS_RELEASE"

    $projectPath = [System.IO.Path]::GetFullPath((Join-Path $Root "cmake\next_app\ClassMngrNext.vcxproj"))
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) { throw "Generated ClassMngrNext Release project was not found: $projectPath" }
    $project = [System.Xml.XmlDocument]::new()
    $project.Load($projectPath)
    $namespace = [System.Xml.XmlNamespaceManager]::new($project.NameTable)
    $namespace.AddNamespace("msbuild", "http://schemas.microsoft.com/developer/msbuild/2003")
    $releaseCompileGroups = [System.Collections.Generic.List[System.Xml.XmlElement]]::new()
    foreach ($group in $project.SelectNodes("//msbuild:ItemDefinitionGroup", $namespace)) {
        if ($group.GetAttribute("Condition") -match "'\$\(Configuration\)\|\$\(Platform\)'\s*==\s*'Release\|x64'") {
            $compile = $group.SelectSingleNode("msbuild:ClCompile", $namespace)
            if ($null -ne $compile) { $releaseCompileGroups.Add([System.Xml.XmlElement]$compile) }
        }
    }
    if ($releaseCompileGroups.Count -ne 1) { throw "Expected one Release|x64 compiler settings group in ClassMngrNext.vcxproj; found $($releaseCompileGroups.Count)." }
    $optimizationNode = $releaseCompileGroups[0].SelectSingleNode("msbuild:Optimization", $namespace)
    $optimizationSetting = if ($null -ne $optimizationNode) { $optimizationNode.InnerText.Trim() } else { "" }
    if ($optimizationSetting -notin @("MaxSpeed", "MinSpace")) {
        throw "Generated ClassMngrNext Release|x64 project does not enable compiler optimization (found '$optimizationSetting')."
    }
    return [ordered]@{
        compiler = "MSVC"
        generator = $generator
        configuration = "Release"
        platform = "x64"
        cFlagsRelease = $verifiedCFlags
        cxxFlagsRelease = $verifiedCxxFlags
        projectPath = $projectPath
        projectSha256 = Get-FileSha256 -Path $projectPath
        projectOptimizationSetting = $optimizationSetting
    }
}

function Get-StartupBoundaryEventId {
    param([string]$Line)
    foreach ($boundary in $ExpectedStartupBoundaryEvents) {
        if ($boundary.kind -eq "stage" -and $Line.Contains("ClassMngrNext startup stage=$($boundary.value) ")) {
            return [string]$boundary.eventId
        }
    }
    foreach ($boundary in $ExpectedStartupBoundaryEvents) {
        if ($boundary.kind -ne "resource" -or -not $Line.Contains([string]$boundary.value)) { continue }
        $match = [regex]::Match($Line, '^ClassMngrNext resource checkpoint=(?<checkpoint>\{.*\})$')
        if (-not $match.Success) { throw "Startup resource boundary $($boundary.eventId) was malformed." }
        try { $checkpoint = $match.Groups["checkpoint"].Value | ConvertFrom-Json -ErrorAction Stop }
        catch { throw "Startup resource boundary $($boundary.eventId) JSON was malformed: $($_.Exception.Message)" }
        if ($checkpoint.resource_id -ne $boundary.value) {
            throw "Startup resource boundary $($boundary.eventId) resource identity was malformed."
        }
        return [string]$boundary.eventId
    }
    return $null
}

function Get-InterFontResourceEvents {
    param([string]$ScenarioId, [int]$StartupReadyLogIndex)
    $events = [System.Collections.Generic.List[object]]::new()
    foreach ($entry in $StartupEvents) {
        if ($entry.scenarioId -ne $ScenarioId) { continue }
        $line = [string]$entry.line
        if ($line -notmatch 'resource:assets/fonts/Inter') { continue }
        $match = [regex]::Match($line, '^ClassMngrNext resource checkpoint=(?<checkpoint>\{.*\})$')
        if (-not $match.Success) { throw "An Inter font resource diagnostic was malformed for $ScenarioId." }
        try { $checkpoint = $match.Groups["checkpoint"].Value | ConvertFrom-Json -ErrorAction Stop }
        catch { throw "An Inter font resource diagnostic was not valid JSON for ${ScenarioId}: $($_.Exception.Message)" }
        $resourceId = [string]$checkpoint.resource_id
        if ($ExpectedInterFontResourceIds -cnotcontains $resourceId) {
            throw "Unexpected Inter font resource ID '$resourceId' for $ScenarioId; the TTC collection is not accepted."
        }
        $requestScope = $checkpoint.request_scope
        if ($checkpoint.event -ne "resource-load" -or $checkpoint.outcome -ne "succeeded" -or
            $null -eq $requestScope -or $requestScope.kind -ne "startup") {
            throw "Inter font resource $resourceId did not complete successfully during startup for $ScenarioId."
        }
        $events.Add([ordered]@{
            resourceId = $resourceId
            outcome = [string]$checkpoint.outcome
            requestScopeKind = [string]$requestScope.kind
            startupLogIndex = [Int64]$StartupEvents.IndexOf($entry)
            stream = [string]$entry.stream
            observedAtUtc = [string]$entry.observedAtUtc
            rawLine = $line
        })
    }
    if ($events.Count -ne $ExpectedInterFontResourceIds.Count) {
        throw "Expected exactly six catalogued Inter TTF startup events for $ScenarioId; found $($events.Count)."
    }
    for ($index = 0; $index -lt $ExpectedInterFontResourceIds.Count; $index++) {
        if ($events[$index].resourceId -cne $ExpectedInterFontResourceIds[$index]) {
            throw "Catalogued Inter TTF startup events were missing, duplicated, or out of order for $ScenarioId."
        }
        if ([Int64]$events[$index].startupLogIndex -ge $StartupReadyLogIndex) {
            throw "Inter TTF resource $($events[$index].resourceId) occurred after startup-ready for $ScenarioId."
        }
    }
    $requiredFontsEvents = @($StartupEvents | Where-Object {
        $_.scenarioId -eq $ScenarioId -and $_.line -match 'ClassMngrNext startup stage=required-fonts-loaded '
    })
    if ($requiredFontsEvents.Count -ne 1) { throw "Expected exactly one required-fonts-loaded record for $ScenarioId." }
    $requiredFontsIndex = $StartupEvents.IndexOf($requiredFontsEvents[0])
    if ($events[-1].startupLogIndex -ge $requiredFontsIndex) {
        throw "All six Inter TTF resource events must precede required-fonts-loaded for $ScenarioId."
    }
    return @($events)
}

function Invoke-FixtureTest {
    param([string]$Executable, [string]$TestFunction, [string]$OutputEnvironmentName,
        [string]$OutputPath, [string]$QtBin)
    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) { throw "Fixture test executable was not found: $Executable" }
    if (Test-Path -LiteralPath $OutputPath) { throw "Refusing to overwrite synthetic fixture: $OutputPath" }
    $oldOutput = [Environment]::GetEnvironmentVariable($OutputEnvironmentName, [EnvironmentVariableTarget]::Process)
    $oldPath = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Process)
    $locationPushed = $false
    try {
        Push-Location -LiteralPath (Split-Path -Parent $Executable)
        $locationPushed = $true
        [Environment]::SetEnvironmentVariable($OutputEnvironmentName, $OutputPath, [EnvironmentVariableTarget]::Process)
        [Environment]::SetEnvironmentVariable("PATH", "$QtBin;$oldPath", [EnvironmentVariableTarget]::Process)
        & $Executable $TestFunction
        if ($LASTEXITCODE -ne 0) { throw "$TestFunction fixture preparation failed with exit code $LASTEXITCODE." }
        if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) { throw "$TestFunction did not create the requested .tps fixture." }
    } finally {
        if ($locationPushed) { Pop-Location }
        [Environment]::SetEnvironmentVariable($OutputEnvironmentName, $oldOutput, [EnvironmentVariableTarget]::Process)
        [Environment]::SetEnvironmentVariable("PATH", $oldPath, [EnvironmentVariableTarget]::Process)
    }
}

function Receive-ProcessOutput {
    param([System.Collections.IList]$Readers, [string]$ScenarioId,
        [System.Diagnostics.Process]$Process, [string]$ExpectedPath,
        [string]$ExpectedHash, [string]$CreatedAtUtc)
    foreach ($reader in $Readers) {
        while ($null -ne $reader.ReadTask -and $reader.ReadTask.IsCompleted) {
            $line = $reader.ReadTask.GetAwaiter().GetResult()
            if ($null -eq $line) {
                $reader.ReadTask = $null
                break
            }
            $entry = [ordered]@{
                scenarioId = $ScenarioId
                stream = $reader.Stream
                observedAtUtc = Get-UtcTimestamp
                line = [string]$line
            }
            $startupLogIndex = $StartupEvents.Count
            Add-JsonLine -Path $StartupLogPath -Value $entry
            $StartupEvents.Add($entry)
            $eventId = Get-StartupBoundaryEventId -Line ([string]$entry.line)
            if ($null -ne $eventId) {
                if (-not $BoundaryEventCounts.ContainsKey($ScenarioId) -or
                    -not $BoundaryEventCounts[$ScenarioId].ContainsKey($eventId)) {
                    throw "Unexpected startup boundary event $eventId for $ScenarioId."
                }
                if ($BoundaryEventCounts[$ScenarioId][$eventId] -ne 0) {
                    throw "Duplicate startup boundary event $eventId for $ScenarioId."
                }
                $sample = New-ProcessSample -Process $Process -ScenarioId $ScenarioId `
                    -SampleIndex $startupLogIndex -Phase "startup-boundary" `
                    -ExpectedPath $ExpectedPath -ExpectedHash $ExpectedHash -CreatedAtUtc $CreatedAtUtc
                $sampleAt = [DateTimeOffset]::Parse([string]$sample.timestampUtc).ToUniversalTime()
                $eventAt = [DateTimeOffset]::Parse([string]$entry.observedAtUtc).ToUniversalTime()
                $sample["eventId"] = $eventId
                $sample["startupLogIndex"] = $startupLogIndex
                $sample["eventObservedAtUtc"] = [string]$entry.observedAtUtc
                $sample["sampleDelayMilliseconds"] = ($sampleAt - $eventAt).TotalMilliseconds
                $sample["stream"] = [string]$entry.stream
                $sample["rawLine"] = [string]$entry.line
                Add-JsonLine -Path $StartupBoundaryMetricsPath -Value $sample
                $BoundaryEventCounts[$ScenarioId][$eventId]++
            }
            $reader.ReadTask = $reader.Reader.ReadLineAsync()
        }
    }
}

function New-ProcessSample {
    param([System.Diagnostics.Process]$Process, [string]$ScenarioId, [int]$SampleIndex,
        [string]$Phase, [string]$ExpectedPath, [string]$ExpectedHash, [string]$CreatedAtUtc)
    $Process.Refresh()
    if ($Process.HasExited) { throw "ClassMngrNext.exe exited during external process sampling." }
    $currentPath = [System.IO.Path]::GetFullPath([Phase9NativeProcessChecks]::GetProcessImagePath($Process.Handle))
    if (-not [string]::Equals($currentPath, $ExpectedPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Sampled process image path changed from the packaged executable."
    }
    if (-not [Phase9NativeProcessChecks]::IsNativeX64Process($Process.Handle)) { throw "Sampled process was not native x64." }
    if ((Get-FileSha256 -Path $currentPath) -ne $ExpectedHash) { throw "Sampled process image SHA-256 changed from the package." }
    return [ordered]@{
        scenarioId = $ScenarioId
        sampleIndex = $SampleIndex
        phase = $Phase
        timestampUtc = Get-UtcTimestamp
        pid = [Int64]$Process.Id
        processCreatedAtUtc = $CreatedAtUtc
        canonicalExecutablePath = $currentPath
        architecture = "x64"
        sha256 = $ExpectedHash
        currentWorkingSetBytes = [Int64]$Process.WorkingSet64
        privateBytes = [Int64]$Process.PrivateMemorySize64
        peakWorkingSetBytes = [Int64]$Process.PeakWorkingSet64
        handles = [Int64]$Process.HandleCount
        threads = [Int64]$Process.Threads.Count
    }
}

function Invoke-Scenario {
    param([System.Collections.IDictionary]$Definition, [string]$ApplicationPath,
        [string]$ApplicationHash, [string]$StageDirectory, [string]$FixturePath)
    $fixturePathArgument = [System.IO.Path]::GetFullPath($FixturePath)
    $Definition.fixture.pathPassedToApp = $fixturePathArgument
    $scenario = [ordered]@{
        id = $Definition.id
        outcome = "in-progress"
        budgetScope = "normal-use-gate"
        startedAtUtc = Get-UtcTimestamp
        fixture = $Definition.fixture
        process = $null
        workspaceOpenedDiagnostic = $null
        startupReadyDiagnostic = $null
        interFontResourceEvents = @()
        startupBoundarySampleCount = 0
        startupBoundaryEventIds = @()
        idleSeconds = $IdleSeconds
        sampleIntervalSeconds = $SampleIntervalSeconds
        userInteractionDuringIdle = $false
        idleCompletedAtUtc = $null
        closeRequestedAtUtc = $null
        exitedAtUtc = $null
        normalExit = $false
        exitCode = $null
    }
    $scenarioBoundaryCounts = @{}
    foreach ($boundary in $ExpectedStartupBoundaryEvents) {
        $scenarioBoundaryCounts[[string]$boundary.eventId] = 0
    }
    $BoundaryEventCounts[$Definition.id] = $scenarioBoundaryCounts
    $Manifest.scenarios += @($scenario)
    Write-JsonFile -Path $ManifestPath -Value $Manifest
    $process = $null
    try {
        $settingsRoot = Join-Path $TemporaryRoot ("settings\" + $Definition.id)
        $localAppData = Join-Path $TemporaryRoot ("local-app-data\" + $Definition.id)
        $appData = Join-Path $TemporaryRoot ("app-data\" + $Definition.id)
        $runtimeTemp = Join-Path $TemporaryRoot ("runtime-temp\" + $Definition.id)
        foreach ($directory in @($settingsRoot, $localAppData, $appData, $runtimeTemp)) {
            [void][System.IO.Directory]::CreateDirectory($directory)
        }

        $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $ApplicationPath
        $startInfo.WorkingDirectory = $StageDirectory
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $false
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        if ((Get-FileSha256 -Path $fixturePathArgument) -ne $Definition.fixture.sha256) {
            throw "Fixture hash changed before launch for $($Definition.id)."
        }
        $startInfo.ArgumentList.Add($fixturePathArgument)
        $startInfo.Environment["CLASSMNGR_SETTINGS_ROOT"] = $settingsRoot
        $startInfo.Environment["LOCALAPPDATA"] = $localAppData
        $startInfo.Environment["APPDATA"] = $appData
        $startInfo.Environment["TEMP"] = $runtimeTemp
        $startInfo.Environment["TMP"] = $runtimeTemp
        $startInfo.Environment["QT_FORCE_STDERR_LOGGING"] = "1"
        $startInfo.Environment["PATH"] = "$StageDirectory;$env:SystemRoot\System32;$env:SystemRoot"

        $process = [System.Diagnostics.Process]::new()
        $process.StartInfo = $startInfo
        if (-not $process.Start()) { throw "Unable to start the staged packaged app for $($Definition.id)." }
        $process.Refresh()
        $createdAtUtc = $process.StartTime.ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss.fffffff'Z'")
        $canonicalPath = [System.IO.Path]::GetFullPath([Phase9NativeProcessChecks]::GetProcessImagePath($process.Handle))
        if (-not [string]::Equals($canonicalPath, $ApplicationPath, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Launched process path does not match the staged package executable."
        }
        if (-not [Phase9NativeProcessChecks]::IsNativeX64Process($process.Handle)) { throw "Launched process is not native x64." }
        if ((Get-FileSha256 -Path $canonicalPath) -ne $ApplicationHash) { throw "Launched process image SHA-256 differs from package." }
        $scenario.process = [ordered]@{
            pid = [Int64]$process.Id
            createdAtUtc = $createdAtUtc
            canonicalExecutablePath = $canonicalPath
            architecture = "x64"
            sha256 = $ApplicationHash
            mainWindowHandle = $null
            mainWindowTitle = $null
        }

        $readers = @(
            [pscustomobject]@{ Reader = $process.StandardOutput; Stream = "stdout"; ReadTask = $process.StandardOutput.ReadLineAsync() },
            [pscustomobject]@{ Reader = $process.StandardError; Stream = "stderr"; ReadTask = $process.StandardError.ReadLineAsync() }
        )
        $deadline = [DateTimeOffset]::UtcNow.AddSeconds($ReadinessTimeoutSeconds)
        $readyEvent = $null
        $readyMatch = $null
        while ([DateTimeOffset]::UtcNow -lt $deadline) {
            Receive-ProcessOutput -Readers $readers -ScenarioId $Definition.id -Process $process `
                -ExpectedPath $ApplicationPath -ExpectedHash $ApplicationHash -CreatedAtUtc $createdAtUtc
            $readyEvents = @($StartupEvents | Where-Object {
                $_.scenarioId -eq $Definition.id -and $_.line -match 'ClassMngrNext startup stage=startup-ready '
            })
            if ($readyEvents.Count -gt 1) { throw "More than one startup-ready record was emitted for $($Definition.id)." }
            if ($readyEvents.Count -eq 1) {
                $readyEvent = $readyEvents[0]
                $readyMatch = [regex]::Match([string]$readyEvent.line,
                    '(?:^|\s)ClassMngrNext startup stage=startup-ready outcome=(?<outcome>[a-z-]+) diagnostic=(?<diagnostic>[a-z-]+) diagnostic_stage=(?<diagnosticStage>[a-z-]+) checkpoint=(?<checkpoint>\{.*\})$')
                if (-not $readyMatch.Success) { throw "The startup-ready diagnostic did not match the expected app format." }
                if ($readyMatch.Groups["outcome"].Value -ne "completed" -or
                    $readyMatch.Groups["diagnostic"].Value -ne "none" -or
                    $readyMatch.Groups["diagnosticStage"].Value -ne "none") {
                    throw "The app startup-ready record reported a warning or failure."
                }
                break
            }
            $process.Refresh()
            if ($process.HasExited) { throw "The packaged app exited before startup-ready with code $($process.ExitCode)." }
            Start-Sleep -Milliseconds 100
        }
        if ($null -eq $readyEvent) { throw "The packaged app did not emit startup-ready within $ReadinessTimeoutSeconds seconds." }
        $readyLogIndex = $StartupEvents.IndexOf($readyEvent)
        $scenario.interFontResourceEvents = @(Get-InterFontResourceEvents -ScenarioId $Definition.id -StartupReadyLogIndex $readyLogIndex)

        $workspaceEvents = @($StartupEvents | Where-Object {
            $_.scenarioId -eq $Definition.id -and $_.line -match 'ClassMngrNext startup stage=workspace-opened '
        })
        if ($workspaceEvents.Count -ne 1) {
            throw "Expected exactly one workspace-opened record for $($Definition.id); found $($workspaceEvents.Count)."
        }
        $workspaceEvent = $workspaceEvents[0]
        $workspaceMatch = [regex]::Match([string]$workspaceEvent.line,
            '(?:^|\s)ClassMngrNext startup stage=workspace-opened outcome=(?<outcome>[a-z-]+) diagnostic=(?<diagnostic>[a-z-]+) diagnostic_stage=(?<diagnosticStage>[a-z-]+) checkpoint=(?<checkpoint>\{.*\})$')
        if (-not $workspaceMatch.Success) { throw "The workspace-opened diagnostic was malformed for $($Definition.id)." }
        try { $workspaceCheckpoint = $workspaceMatch.Groups["checkpoint"].Value | ConvertFrom-Json -ErrorAction Stop }
        catch { throw "The workspace-opened checkpoint was not valid JSON: $($_.Exception.Message)" }
        $scenario.workspaceOpenedDiagnostic = [ordered]@{
            stage = "workspace-opened"
            outcome = $workspaceMatch.Groups["outcome"].Value
            diagnostic = $workspaceMatch.Groups["diagnostic"].Value
            diagnosticStage = $workspaceMatch.Groups["diagnosticStage"].Value
            stream = $workspaceEvent.stream
            observedAtUtc = $workspaceEvent.observedAtUtc
            checkpoint = $workspaceCheckpoint
            rawLine = $workspaceEvent.line
        }
        if ($StartupEvents.IndexOf($workspaceEvent) -ge $StartupEvents.IndexOf($readyEvent)) {
            throw "workspace-opened did not precede startup-ready for $($Definition.id)."
        }
        if ($scenario.workspaceOpenedDiagnostic.outcome -ne "completed" -or
            $scenario.workspaceOpenedDiagnostic.diagnostic -ne "none" -or
            $scenario.workspaceOpenedDiagnostic.diagnosticStage -ne "none" -or
            $workspaceCheckpoint.stage -ne "workspace-opened" -or
            $workspaceCheckpoint.outcome -ne "completed") {
            throw "workspace-opened did not complete cleanly for $($Definition.id)."
        }

        try { $checkpoint = $readyMatch.Groups["checkpoint"].Value | ConvertFrom-Json -ErrorAction Stop }
        catch { throw "The startup-ready diagnostic checkpoint was not valid JSON: $($_.Exception.Message)" }
        if ($checkpoint.stage -ne "startup-ready" -or $checkpoint.outcome -ne "completed" -or
            $checkpoint.sample_available -ne $true -or $checkpoint.platform -ne "windows") {
            throw "The app startup-ready diagnostic did not include a valid Windows memory checkpoint."
        }
        if ($null -eq $checkpoint.metrics.working_set_bytes -or [Int64]$checkpoint.metrics.working_set_bytes -le 0) {
            throw "The app startup-ready diagnostic did not include a positive working-set sample."
        }
        $scenario.startupReadyDiagnostic = [ordered]@{
            stage = "startup-ready"
            outcome = "completed"
            diagnostic = "none"
            diagnosticStage = "none"
            stream = $readyEvent.stream
            observedAtUtc = $readyEvent.observedAtUtc
            checkpoint = $checkpoint
            rawLine = $readyEvent.line
        }
        foreach ($boundary in $ExpectedStartupBoundaryEvents) {
            $eventId = [string]$boundary.eventId
            if ($scenarioBoundaryCounts[$eventId] -ne 1) {
                throw "Expected exactly one $eventId process boundary sample for $($Definition.id); found $($scenarioBoundaryCounts[$eventId])."
            }
        }
        $scenario.startupBoundarySampleCount = $ExpectedStartupBoundaryEvents.Count
        $scenario.startupBoundaryEventIds = @($ExpectedStartupBoundaryEvents | ForEach-Object { $_.eventId })
        Write-JsonFile -Path $ManifestPath -Value $Manifest
        $windowDeadline = [DateTimeOffset]::UtcNow.AddSeconds(10)
        $stablePolls = 0
        $stableHandle = [Int64]0
        $stableTitle = ""
        while ([DateTimeOffset]::UtcNow -lt $windowDeadline -and $stablePolls -lt 3) {
            $process.Refresh()
            if ($process.HasExited) { throw "The packaged app exited after startup-ready." }
            $handle = $process.MainWindowHandle
            $title = $process.MainWindowTitle.Trim()
            if ($handle -ne [IntPtr]::Zero -and [Phase9NativeProcessChecks]::IsWindowVisible($handle) -and $title.Contains("ClassMngr Next")) {
                if ($stableHandle -eq $handle.ToInt64() -and $stableTitle -ceq $title) { $stablePolls++ }
                else { $stableHandle = $handle.ToInt64(); $stableTitle = $title; $stablePolls = 1 }
            } else { $stableHandle = [Int64]0; $stableTitle = ""; $stablePolls = 0 }
            if ($stablePolls -lt 3) { Start-Sleep -Milliseconds 250 }
        }
        if ($stablePolls -lt 3) { throw "No stable visible ClassMngr Next window appeared after startup-ready." }
        $scenario.process.mainWindowHandle = $stableHandle
        $scenario.process.mainWindowTitle = $stableTitle
        $scenario.readiness = [ordered]@{
            windowVisible = $true
            stablePolls = 3
            mainWindowHandle = $stableHandle
            mainWindowTitle = $stableTitle
        }

        $sample = New-ProcessSample -Process $process -ScenarioId $Definition.id -SampleIndex 0 `
            -Phase "startup-ready" -ExpectedPath $ApplicationPath -ExpectedHash $ApplicationHash -CreatedAtUtc $createdAtUtc
        Add-JsonLine -Path $MetricsPath -Value $sample
        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        $nextSampleMilliseconds = [Int64]1000
        for ($sampleIndex = 1; $sampleIndex -le $IdleSeconds; $sampleIndex++) {
            while ($stopwatch.ElapsedMilliseconds -lt $nextSampleMilliseconds) {
                $remaining = $nextSampleMilliseconds - $stopwatch.ElapsedMilliseconds
                Start-Sleep -Milliseconds ([Math]::Max(1, [Math]::Min(50, $remaining)))
            }
            Receive-ProcessOutput -Readers $readers -ScenarioId $Definition.id -Process $process `
                -ExpectedPath $ApplicationPath -ExpectedHash $ApplicationHash -CreatedAtUtc $createdAtUtc
            $sample = New-ProcessSample -Process $process -ScenarioId $Definition.id -SampleIndex $sampleIndex `
                -Phase "idle" -ExpectedPath $ApplicationPath -ExpectedHash $ApplicationHash -CreatedAtUtc $createdAtUtc
            Add-JsonLine -Path $MetricsPath -Value $sample
            $nextSampleMilliseconds += 1000
        }
        $stopwatch.Stop()
        $scenario.idleCompletedAtUtc = Get-UtcTimestamp
        $scenario.closeRequestedAtUtc = Get-UtcTimestamp
        if (-not $process.CloseMainWindow()) {
            $process.Refresh()
            if (-not $process.HasExited) { throw "CloseMainWindow was not accepted after the idle interval." }
        }
        if (-not $process.WaitForExit(30000)) { throw "Packaged app did not exit within 30 seconds after normal close." }
        $drainDeadline = [DateTimeOffset]::UtcNow.AddSeconds(5)
        while ([DateTimeOffset]::UtcNow -lt $drainDeadline -and
            @($readers | Where-Object { $null -ne $_.ReadTask }).Count -gt 0) {
            Receive-ProcessOutput -Readers $readers -ScenarioId $Definition.id -Process $process `
                -ExpectedPath $ApplicationPath -ExpectedHash $ApplicationHash -CreatedAtUtc $createdAtUtc
            if (@($readers | Where-Object { $null -ne $_.ReadTask -and -not $_.ReadTask.IsCompleted }).Count -gt 0) {
                Start-Sleep -Milliseconds 10
            }
        }
        Receive-ProcessOutput -Readers $readers -ScenarioId $Definition.id -Process $process `
            -ExpectedPath $ApplicationPath -ExpectedHash $ApplicationHash -CreatedAtUtc $createdAtUtc
        $process.Refresh()
        $scenario.exitedAtUtc = Get-UtcTimestamp
        $scenario.exitCode = [Int64]$process.ExitCode
        $scenario.normalExit = ($process.ExitCode -eq 0)
        if (-not $scenario.normalExit) { throw "Packaged app exited abnormally with code $($process.ExitCode)." }
        foreach ($boundary in $ExpectedStartupBoundaryEvents) {
            $eventId = [string]$boundary.eventId
            if ($scenarioBoundaryCounts[$eventId] -ne 1) {
                throw "Expected exactly one $eventId process boundary sample for $($Definition.id); found $($scenarioBoundaryCounts[$eventId])."
            }
        }
        $scenario.outcome = "completed"
        Write-JsonFile -Path $ManifestPath -Value $Manifest
    } catch {
        $scenario.outcome = "failed"
        $scenario.failure = $_.Exception.Message
        if ($null -ne $process) {
            try {
                $process.Refresh()
                if (-not $process.HasExited) {
                    [void]$process.CloseMainWindow()
                    if (-not $process.WaitForExit(5000)) { $process.Kill($true); $process.WaitForExit() }
                }
                if ($null -eq $scenario.exitedAtUtc -and $process.HasExited) {
                    $scenario.exitedAtUtc = Get-UtcTimestamp
                    $scenario.exitCode = [Int64]$process.ExitCode
                }
            } catch { }
        }
        Write-JsonFile -Path $ManifestPath -Value $Manifest
        throw
    } finally {
        if ($null -ne $process) { $process.Dispose() }
    }
}

function Invoke-Validator {
    $output = & python $ValidatorPath --manifest $ManifestPath --metrics $MetricsPath `
        --startup-log $StartupLogPath --startup-boundary-metrics $StartupBoundaryMetricsPath `
        --package-files $PackageFilesPath --source-files $SourceFilesPath `
        --build-log $BuildLogPath `
        --summary $SummaryPath 2>&1
    $exitCode = $LASTEXITCODE
    [System.IO.File]::WriteAllText($ValidatorOutputPath,
        ((@($output) -join [Environment]::NewLine) + [Environment]::NewLine),
        [System.Text.UTF8Encoding]::new($false))
    foreach ($line in $output) { Write-Host $line }
    return $exitCode
}

[void][System.IO.Directory]::CreateDirectory($EvidenceDirectory)
[System.IO.File]::WriteAllText($MetricsPath, "", [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText($StartupLogPath, "", [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText($StartupBoundaryMetricsPath, "", [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText($PackageFilesPath, "{`"files`":[]}" + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText($SourceFilesPath, "{`"files`":[]}" + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
$Manifest = [ordered]@{
    schema = $ManifestSchema
    taskId = $TaskId
    status = "in-progress"
    startedAtUtc = Get-UtcTimestamp
    completedAtUtc = $null
    revision = $null
    candidateSource = $null
    configuration = [ordered]@{
        preset = "windows-x64-release"
        buildType = "Release"
        architecture = "x64"
        releasePackage = $true
        generator = $null
        cmakeCachePath = $null
        cmakeCacheSha256 = $null
        cmakeCacheSha256AtRunEnd = $null
        cmakeCacheStableDuringRun = $false
        releaseCompiler = $null
        packageBuild = $null
    }
    os = [ordered]@{ platform = "Windows"; architecture = "x64"; machineArchitecture = $env:PROCESSOR_ARCHITECTURE }
    application = $null
    package = $null
    qtInstallation = $null
    thresholdBytes = $ThresholdBytes
    comparison = "strictly-less-than"
    idleSeconds = $IdleSeconds
    sampleIntervalSeconds = $SampleIntervalSeconds
    startupBoundaryMetrics = [ordered]@{
        schema = $StartupBoundaryMetricsSchema
        inventoryFile = "startup-boundary-metrics.jsonl"
        expectedEventIds = @($ExpectedStartupBoundaryEvents | ForEach-Object { $_.eventId })
        samplesPerScenario = $ExpectedStartupBoundaryEvents.Count
        maxSampleDelayMilliseconds = $StartupBoundaryMaximumDelayMilliseconds
    }
    requiredInterFontResourceIds = $ExpectedInterFontResourceIds
    advisoryScenarios = @([ordered]@{ id = "fresh-initial-setup"; budgetScope = "advisory"; measuredBy = "Phase 7 advisory route" })
    scenarios = @()
    failure = $null
}
$runnerFailure = $null

try {
    if ($ReadinessTimeoutSeconds -lt 10) { throw "ReadinessTimeoutSeconds must be at least 10." }
    if (-not [Environment]::Is64BitOperatingSystem -or $env:PROCESSOR_ARCHITECTURE -ne "AMD64") {
        throw "P9-06 requires a native Windows x64 host."
    }
    if (-not (Test-Path -LiteralPath $ValidatorPath -PathType Leaf)) {
        throw "P9-06 validator was not found: $ValidatorPath"
    }
    if (-not (Test-Path -LiteralPath $RepositoryRoot -PathType Container)) { throw "RepositoryRoot was not found: $RepositoryRoot" }
    [void][System.IO.Directory]::CreateDirectory((Split-Path -Parent $EvidenceDirectory))
    $revisionOutput = & git -C $RepositoryRoot rev-parse --verify HEAD 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Unable to read local Git HEAD." }
    $revision = ([string]($revisionOutput | Select-Object -Last 1)).Trim().ToLowerInvariant()
    if ($revision -notmatch '^(?:[0-9a-f]{40}|[0-9a-f]{64})$') { throw "Local Git HEAD is not a full Git object ID." }
    $Manifest.revision = $revision
    $sourceIdentity = Get-CandidateSourceIdentity -Root $RepositoryRoot -Revision $revision
    $Manifest.candidateSource = [ordered]@{
        revision = $revision
        sha256 = $sourceIdentity.sha256
        includedRoots = $sourceIdentity.includedRoots
        fileCount = $sourceIdentity.fileCount
        inventoryFile = "source-files.json"
        sha256AtRunEnd = $null
        stableDuringRun = $false
    }
    Write-JsonFile -Path $SourceFilesPath -Value ([ordered]@{ files = @($sourceIdentity.files) })

    $cmakeCachePath = Join-Path $BuildDirectory "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cmakeCachePath -PathType Leaf)) { throw "Release build CMakeCache.txt was not found: $cmakeCachePath" }
    $cmakeCachePath = [System.IO.Path]::GetFullPath($cmakeCachePath)
    $cmakeCacheSha256 = Get-FileSha256 -Path $cmakeCachePath
    $Manifest.configuration.cmakeCachePath = $cmakeCachePath
    $Manifest.configuration.cmakeCacheSha256 = $cmakeCacheSha256
    $cacheLines = @(Get-Content -LiteralPath $cmakeCachePath)
    $homeLine = @($cacheLines | Where-Object { $_ -match '^CMAKE_HOME_DIRECTORY:INTERNAL=' })
    if ($homeLine.Count -ne 1 -or -not [string]::Equals([System.IO.Path]::GetFullPath(($homeLine[0] -split '=', 2)[1]), $RepositoryRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Release build CMakeCache source directory does not match RepositoryRoot."
    }
    $packageSetting = @($cacheLines | Where-Object { $_ -match '^CLASSMNGR_NEXT_RELEASE_PACKAGE:BOOL=' })
    if ($packageSetting.Count -ne 1 -or (($packageSetting[0] -split '=', 2)[1] -ne "ON")) {
        throw "The build must have CLASSMNGR_NEXT_RELEASE_PACKAGE=ON."
    }
    $cmakeExecutablePath = [System.IO.Path]::GetFullPath((Get-RequiredCMakeCacheValue -Lines $cacheLines -Name "CMAKE_COMMAND"))
    if (-not (Test-Path -LiteralPath $cmakeExecutablePath -PathType Leaf)) { throw "The CMake executable recorded by this build tree is missing: $cmakeExecutablePath" }
    if ([System.IO.Path]::GetFileName($cmakeExecutablePath) -ine "cmake.exe") { throw "CMAKE_COMMAND is not a Windows CMake executable: $cmakeExecutablePath" }
    $cmakeExecutableSha256 = Get-FileSha256 -Path $cmakeExecutablePath
    $configurationTypesSetting = @($cacheLines | Where-Object { $_ -match '^CMAKE_CONFIGURATION_TYPES:[^=]+=' })
    $releaseConfigurationDirectory = ""
    if ($configurationTypesSetting.Count -gt 0) {
        $configurationTypes = (($configurationTypesSetting[0] -split '=', 2)[1] -split ';')
        if ($configurationTypes -notcontains "Release") { throw "Build generator does not provide Release configuration." }
        $releaseConfigurationDirectory = "Release"
    } else {
        $buildType = @($cacheLines | Where-Object { $_ -match '^CMAKE_BUILD_TYPE:[^=]+=' })
        if ($buildType.Count -ne 1 -or (($buildType[0] -split '=', 2)[1] -ine "Release")) { throw "Build is not configured as Release." }
    }
    $releaseCompiler = Get-VerifiedReleaseCompilerConfiguration -CacheLines $cacheLines -Root $BuildDirectory
    $Manifest.configuration.generator = $releaseCompiler.generator
    $Manifest.configuration.releaseCompiler = $releaseCompiler

    $requiredBuildTargets = @(
        "ClassMngrNextResourcePackage",
        "ClassMngrPhase7NextBlankFixtureTests",
        "ClassMngrStartupPerformanceTests"
    )
    $buildArguments = @("--build", $BuildDirectory, "--config", "Release", "--target") + $requiredBuildTargets
    $buildCommand = @($cmakeExecutablePath) + $buildArguments
    $buildStartedAtUtc = Get-UtcTimestamp
    $buildOutput = @()
    $buildExitCode = -1
    $priorErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $buildOutput = @(& $cmakeExecutablePath @buildArguments 2>&1 | ForEach-Object { [string]$_ })
        $buildExitCode = $LASTEXITCODE
    } catch {
        $buildOutput += "Build process invocation failed: $($_.Exception.Message)"
        $buildExitCode = -1
    } finally {
        $ErrorActionPreference = $priorErrorActionPreference
    }
    $buildCompletedAtUtc = Get-UtcTimestamp
    $buildLogLines = @(
        "command: `"$cmakeExecutablePath`" $($buildArguments -join ' ')"
        "startedAtUtc: $buildStartedAtUtc"
        "--- cmake build output ---"
    ) + @($buildOutput) + @(
        "--- end cmake build output ---"
        "exitCode: $buildExitCode"
        "completedAtUtc: $buildCompletedAtUtc"
    )
    [System.IO.File]::WriteAllLines($BuildLogPath, [string[]]$buildLogLines, [System.Text.UTF8Encoding]::new($false))
    $Manifest.configuration.packageBuild = [ordered]@{
        targets = $requiredBuildTargets
        command = $buildCommand
        cmakeExecutablePath = $cmakeExecutablePath
        cmakeExecutableSha256 = $cmakeExecutableSha256
        startedAtUtc = $buildStartedAtUtc
        completedAtUtc = $buildCompletedAtUtc
        exitCode = $buildExitCode
        succeeded = ($buildExitCode -eq 0)
        logPath = [System.IO.Path]::GetFullPath($BuildLogPath)
        logSha256 = Get-FileSha256 -Path $BuildLogPath
    }
    Write-JsonFile -Path $ManifestPath -Value $Manifest
    if ($buildExitCode -ne 0) { throw "Required Release package and fixture targets failed to build with exit code $buildExitCode; see $BuildLogPath" }
    $releaseCompilerAfterBuild = Get-VerifiedReleaseCompilerConfiguration -CacheLines @(Get-Content -LiteralPath $cmakeCachePath) -Root $BuildDirectory
    if ($releaseCompilerAfterBuild.cFlagsRelease.raw -ne $releaseCompiler.cFlagsRelease.raw -or
        $releaseCompilerAfterBuild.cxxFlagsRelease.raw -ne $releaseCompiler.cxxFlagsRelease.raw -or
        $releaseCompilerAfterBuild.projectOptimizationSetting -ne $releaseCompiler.projectOptimizationSetting) {
        throw "Release compiler configuration changed while building the package prerequisites."
    }
    $Manifest.configuration.releaseCompiler = $releaseCompilerAfterBuild

    $qt6DirectorySetting = @($cacheLines | Where-Object { $_ -match '^Qt6_DIR:[^=]+=' })
    if ($qt6DirectorySetting.Count -ne 1) { throw "Expected exactly one Qt6_DIR in CMakeCache.txt; found $($qt6DirectorySetting.Count)." }
    $qt6Directory = [System.IO.Path]::GetFullPath(($qt6DirectorySetting[0] -split '=', 2)[1])
    if (-not (Test-Path -LiteralPath $qt6Directory -PathType Container)) { throw "CMakeCache Qt6_DIR does not exist: $qt6Directory" }
    $derivedQtPrefix = [System.IO.Path]::GetFullPath((Join-Path $qt6Directory "..\..\.."))
    if ([string]::IsNullOrWhiteSpace($QtPrefix)) {
        $QtPrefix = $derivedQtPrefix
    } else {
        $QtPrefix = [System.IO.Path]::GetFullPath($QtPrefix)
        if (-not [string]::Equals($QtPrefix, $derivedQtPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "QtPrefix does not match the Release build's Qt6_DIR installation."
        }
    }
    $expectedQt6Directory = [System.IO.Path]::GetFullPath((Join-Path $QtPrefix "lib\cmake\Qt6"))
    if (-not [string]::Equals($qt6Directory, $expectedQt6Directory, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Qt6_DIR is not under the expected QtPrefix installation layout."
    }
    $qtBin = Join-Path $QtPrefix "bin"
    $windeployqt = Join-Path $qtBin "windeployqt.exe"
    $qtCoreDll = Join-Path $qtBin "Qt6Core.dll"
    if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) { throw "windeployqt was not found in the CMakeCache Qt installation: $windeployqt" }
    $qtVersion = Get-QtBinaryVersion -Path $qtCoreDll
    $windeployqtVersion = Get-QtBinaryVersion -Path $windeployqt
    if ($qtVersion -ne $windeployqtVersion) { throw "windeployqt version $windeployqtVersion does not match Qt6Core version $qtVersion." }
    $qtInstallationId = Get-StringSha256 -Value ("{0}`n{1}`n{2}`n" -f $QtPrefix, $qtVersion, $qt6Directory)

    $fixtureGeneratorDefinitions = @(
        [ordered]@{ id = "empty-workspace-fixture-generator"; path = [System.IO.Path]::GetFullPath((Join-Path $BuildDirectory "Release\ClassMngrPhase7NextBlankFixtureTests.exe")) },
        [ordered]@{ id = "representative-workspace-fixture-generator"; path = [System.IO.Path]::GetFullPath((Join-Path $BuildDirectory "Release\ClassMngrStartupPerformanceTests.exe")) }
    )
    $qtFixtureGenerators = [System.Collections.Generic.List[object]]::new()
    foreach ($generator in $fixtureGeneratorDefinitions) {
        if (-not (Test-Path -LiteralPath $generator.path -PathType Leaf)) { throw "Fixture generator executable was not found: $($generator.path)" }
        $colocatedQtDlls = [System.Collections.Generic.List[object]]::new()
        $generatorDirectory = Split-Path -Parent $generator.path
        foreach ($dll in @(Get-ChildItem -LiteralPath $generatorDirectory -Filter "Qt6*.dll" -File)) {
            $prefixDll = Join-Path $qtBin $dll.Name
            if (-not (Test-Path -LiteralPath $prefixDll -PathType Leaf)) { throw "Fixture generator has a Qt runtime DLL absent from CMakeCache QtPrefix: $($dll.FullName)" }
            $colocatedHash = Get-FileSha256 -Path $dll.FullName
            $prefixHash = Get-FileSha256 -Path $prefixDll
            if ($colocatedHash -ne $prefixHash) { throw "Fixture generator runtime DLL does not match CMakeCache QtPrefix: $($dll.FullName)" }
            $colocatedQtDlls.Add([ordered]@{
                name = $dll.Name
                colocatedPath = $dll.FullName
                colocatedSha256 = $colocatedHash
                qtPrefixPath = $prefixDll
                qtPrefixSha256 = $prefixHash
            })
        }
        $qtFixtureGenerators.Add([ordered]@{
            id = $generator.id
            executablePath = $generator.path
            executableSha256 = Get-FileSha256 -Path $generator.path
            runtimeSearchPathPrefix = $qtBin
            colocatedQtDlls = @($colocatedQtDlls)
            verifiedSameQtInstallation = $true
        })
    }
    $Manifest.qtInstallation = [ordered]@{
        id = $qtInstallationId
        version = $qtVersion
        prefix = $QtPrefix
        qt6Directory = $qt6Directory
        coreDllPath = $qtCoreDll
        coreDllSha256 = Get-FileSha256 -Path $qtCoreDll
        windeployqtPath = [System.IO.Path]::GetFullPath($windeployqt)
        windeployqtSha256 = Get-FileSha256 -Path $windeployqt
        windeployqtVersion = $windeployqtVersion
        fixtureGenerators = @($qtFixtureGenerators)
    }
    $releaseOutputDirectory = Join-Path $BuildDirectory "next-package-build"
    if (-not [string]::IsNullOrEmpty($releaseConfigurationDirectory)) { $releaseOutputDirectory = Join-Path $releaseOutputDirectory $releaseConfigurationDirectory }
    $releaseExecutable = Join-Path $releaseOutputDirectory "ClassMngr.exe"
    if (-not (Test-Path -LiteralPath $releaseExecutable -PathType Leaf)) { throw "Packaged Release ClassMngr.exe was not found: $releaseExecutable" }
    $releaseExecutable = [System.IO.Path]::GetFullPath($releaseExecutable)
    if ((Get-PeArchitecture -Path $releaseExecutable) -ne "x64") { throw "Release app is not a PE x64 image." }

    $temporaryParent = if (-not [string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
    $TemporaryRoot = Join-Path $temporaryParent ("phase9-packaged-idle-memory-" + [Guid]::NewGuid().ToString("N"))
    [void][System.IO.Directory]::CreateDirectory($TemporaryRoot)
    $stageRoot = Join-Path $TemporaryRoot "package-stage"
    [void][System.IO.Directory]::CreateDirectory($stageRoot)
    Invoke-CheckedCommand -FilePath "cmake" -Arguments @("--install", $BuildDirectory, "--config", "Release", "--prefix", $stageRoot, "--component", "ClassMngrNextResources") -Description "Installing packaged app resources"
    $resourceCatalogs = @(Get-ChildItem -LiteralPath $stageRoot -Filter "resource-catalog.json" -File -Recurse)
    if ($resourceCatalogs.Count -ne 1) { throw "Expected exactly one installed resource catalog; found $($resourceCatalogs.Count)." }
    $resourceDirectory = Split-Path -Parent $resourceCatalogs[0].FullName
    if ([System.IO.Path]::GetFileName($resourceDirectory) -ne "resources") { throw "Installed resource catalog is not under resources." }
    $stageDirectory = [System.IO.Path]::GetFullPath((Split-Path -Parent $resourceDirectory))
    $resourceCatalogPath = [System.IO.Path]::GetFullPath($resourceCatalogs[0].FullName)
    $resourceCatalog = Get-Content -LiteralPath $resourceCatalogPath -Raw | ConvertFrom-Json -ErrorAction Stop
    $interCatalogRecords = @($resourceCatalog.records | Where-Object {
        ([string]$_.logical_id).StartsWith("resource:assets/fonts/Inter", [StringComparison]::Ordinal)
    })
    if ($interCatalogRecords.Count -ne $ExpectedInterFontResourceIds.Count) {
        throw "Installed resource catalog does not contain exactly the six Inter TTF records."
    }
    $catalogInterEntries = [System.Collections.Generic.List[object]]::new()
    foreach ($expectedResourceId in $ExpectedInterFontResourceIds) {
        $matchingCatalogRecords = @($interCatalogRecords | Where-Object { $_.logical_id -ceq $expectedResourceId })
        if ($matchingCatalogRecords.Count -ne 1) {
            throw "Installed resource catalog is missing, duplicating, or replacing Inter resource $expectedResourceId."
        }
        $catalogRecord = $matchingCatalogRecords[0]
        $packageRelativePath = [string]$catalogRecord.package_relative_path
        $sourcePath = [string]$catalogRecord.provenance.source.path
        $sourceBytes = [Int64]$catalogRecord.provenance.source.bytes
        $sourceSha256 = [string]$catalogRecord.provenance.source.sha256
        $bakedBytes = [Int64]$catalogRecord.provenance.baked.bytes
        $bakedSha256 = [string]$catalogRecord.provenance.baked.sha256
        $expectedPackageRelativePath = $expectedResourceId.Substring("resource:".Length)
        if ($catalogRecord.resource_kind -ne "required_ui_font" -or
            $catalogRecord.expected_type -ne "ttf" -or
            $catalogRecord.location -ne "resource_file" -or
            $packageRelativePath -cne $expectedPackageRelativePath -or
            $sourcePath -cne ("resources/" + $expectedPackageRelativePath) -or
            $sourceBytes -le 0 -or $bakedBytes -ne $sourceBytes -or
            $sourceSha256 -notmatch '^[0-9a-f]{64}$' -or $bakedSha256 -cne $sourceSha256) {
            throw "Installed catalog provenance is invalid for Inter resource $expectedResourceId."
        }
        $sourceFontPath = Join-Path $RepositoryRoot ($sourcePath.Replace("/", "\"))
        $packagedFontPath = Join-Path $resourceDirectory ($packageRelativePath.Replace("/", "\"))
        if (-not (Test-Path -LiteralPath $sourceFontPath -PathType Leaf) -or
            -not (Test-Path -LiteralPath $packagedFontPath -PathType Leaf)) {
            throw "Catalogued Inter font file is missing from source or package: $expectedResourceId."
        }
        if ((Get-Item -LiteralPath $sourceFontPath).Length -ne $sourceBytes -or
            (Get-FileSha256 -Path $sourceFontPath) -cne $sourceSha256 -or
            (Get-Item -LiteralPath $packagedFontPath).Length -ne $bakedBytes -or
            (Get-FileSha256 -Path $packagedFontPath) -cne $bakedSha256) {
            throw "Catalogued Inter font source/package identity mismatch: $expectedResourceId."
        }
        $catalogInterEntries.Add([ordered]@{
            resourceId = $expectedResourceId
            sourcePath = $sourcePath
            packageRelativePath = $packageRelativePath
            sizeBytes = $sourceBytes
            sha256 = $sourceSha256
        })
    }
    $stageRelative = [System.IO.Path]::GetRelativePath($stageRoot, $stageDirectory)
    if ([System.IO.Path]::IsPathRooted($stageRelative) -or $stageRelative -eq ".." -or $stageRelative.StartsWith("..\", [StringComparison]::Ordinal)) {
        throw "Resource catalog resolved outside the package stage."
    }
    $applicationPath = Join-Path $stageDirectory "ClassMngrNext.exe"
    Copy-Item -LiteralPath $releaseExecutable -Destination $applicationPath
    $applicationPath = [System.IO.Path]::GetFullPath($applicationPath)
    $applicationHash = Get-FileSha256 -Path $applicationPath
    if ($applicationHash -ne (Get-FileSha256 -Path $releaseExecutable)) { throw "Staged app differs from packaged Release executable." }
    $oldPath = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Process)
    try {
        [Environment]::SetEnvironmentVariable("PATH", "$qtBin;$oldPath", [EnvironmentVariableTarget]::Process)
        Invoke-CheckedCommand -FilePath $windeployqt -Arguments @(
            "--qmldir", (Join-Path $RepositoryRoot "src\features\calendar\ui\qml"),
            "--skip-plugin-types", "qmltooling", "--include-plugins", "qoffscreen,qsqlite",
            "--exclude-plugins", "qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql",
            "--translations", "en,ko", "--compiler-runtime", "--dir", $stageDirectory, $applicationPath
        ) -Description "Deploying Qt runtime to the local package stage"
    } finally { [Environment]::SetEnvironmentVariable("PATH", $oldPath, [EnvironmentVariableTarget]::Process) }
    foreach ($relativePath in @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Sql.dll", "Qt6Network.dll", "platforms\qwindows.dll", "sqldrivers\qsqlite.dll")) {
        $stagedRuntimePath = Join-Path $stageDirectory $relativePath
        if (-not (Test-Path -LiteralPath $stagedRuntimePath -PathType Leaf)) { throw "Required packaged runtime file is missing: $relativePath" }
        if ([System.IO.Path]::GetFileName($relativePath).StartsWith("Qt6", [StringComparison]::OrdinalIgnoreCase)) {
            $prefixRuntimePath = Join-Path $qtBin ([System.IO.Path]::GetFileName($relativePath))
            if ((Get-FileSha256 -Path $stagedRuntimePath) -ne (Get-FileSha256 -Path $prefixRuntimePath)) {
                throw "Packaged Qt runtime does not match the CMakeCache Qt installation: $relativePath"
            }
        }
    }
    $packageEntries = [System.Collections.Generic.List[object]]::new()
    foreach ($file in @(Get-ChildItem -LiteralPath $stageDirectory -File -Recurse | Sort-Object { $_.FullName.ToLowerInvariant() })) {
        $relativePath = [System.IO.Path]::GetRelativePath($stageDirectory, $file.FullName).Replace("\", "/")
        $packageEntries.Add([ordered]@{ path = $relativePath; sizeBytes = [Int64]$file.Length; sha256 = Get-FileSha256 -Path $file.FullName })
    }
    $packageCanonical = @($packageEntries | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    $packageHash = Get-StringSha256 -Value ($packageCanonical + "`n")
    Write-JsonFile -Path $PackageFilesPath -Value ([ordered]@{ files = @($packageEntries) })
    $applicationRelative = [System.IO.Path]::GetRelativePath($stageDirectory, $applicationPath).Replace("\", "/")
    $resourceCatalogRelativePath = [System.IO.Path]::GetRelativePath($stageDirectory, $resourceCatalogPath).Replace("\", "/")
    $resourceCatalogPackageEntries = @($packageEntries | Where-Object { $_.path -ceq $resourceCatalogRelativePath })
    if ($resourceCatalogRelativePath -cne "resources/resource-catalog.json" -or $resourceCatalogPackageEntries.Count -ne 1) {
        throw "The installed resource catalog is not represented exactly once in the package inventory."
    }
    $interFontLicenseSourcePath = "licenses/fonts/inter/LICENSE.txt"
    $interFontLicensePackagePath = "licenses/fonts/inter/LICENSE.txt"
    $interFontLicenseSourceFile = Join-Path $RepositoryRoot ($interFontLicenseSourcePath.Replace("/", "\"))
    $interFontLicenseInstalledFile = Join-Path $stageDirectory ($interFontLicensePackagePath.Replace("/", "\"))
    if (-not (Test-Path -LiteralPath $interFontLicenseSourceFile -PathType Leaf) -or
        -not (Test-Path -LiteralPath $interFontLicenseInstalledFile -PathType Leaf)) {
        throw "The Inter OFL license is missing from source or the staged package."
    }
    $interFontLicenseSizeBytes = [Int64](Get-Item -LiteralPath $interFontLicenseSourceFile).Length
    $interFontLicenseSha256 = Get-FileSha256 -Path $interFontLicenseSourceFile
    $interFontLicensePackageEntries = @($packageEntries | Where-Object { $_.path -ceq $interFontLicensePackagePath })
    if ($interFontLicensePackageEntries.Count -ne 1 -or
        (Get-Item -LiteralPath $interFontLicenseInstalledFile).Length -ne $interFontLicenseSizeBytes -or
        (Get-FileSha256 -Path $interFontLicenseInstalledFile) -cne $interFontLicenseSha256 -or
        $interFontLicensePackageEntries[0].sizeBytes -ne $interFontLicenseSizeBytes -or
        $interFontLicensePackageEntries[0].sha256 -cne $interFontLicenseSha256) {
        throw "The packaged Inter OFL license differs from its exact source identity."
    }
    $Manifest.application = [ordered]@{
        canonicalPath = $applicationPath
        sha256 = $applicationHash
        architecture = "x64"
        candidateSourceSha256 = $Manifest.candidateSource.sha256
        releaseExecutablePath = $releaseExecutable
        releaseExecutableSha256 = Get-FileSha256 -Path $releaseExecutable
        resourceCatalogPresent = $true
        qtRuntimeDeployed = $true
    }
    $Manifest.package = [ordered]@{
        id = "classmngr-next-windows-x64-release-stage-v1"
        sourceReleasePackage = $true
        candidateSourceSha256 = $Manifest.candidateSource.sha256
        sha256 = $packageHash
        fileCount = $packageEntries.Count
        applicationRelativePath = $applicationRelative
        applicationSha256 = $applicationHash
        inventoryFile = "package-files.json"
        resourceCatalogPath = $resourceCatalogRelativePath
        resourceCatalogSha256 = $resourceCatalogPackageEntries[0].sha256
        interFontCatalogEntries = @($catalogInterEntries)
        interFontLicense = [ordered]@{
            sourcePath = $interFontLicenseSourcePath
            packagePath = $interFontLicensePackagePath
            sizeBytes = $interFontLicenseSizeBytes
            sha256 = $interFontLicenseSha256
        }
    }

    $fixtureDirectory = Join-Path $TemporaryRoot "fixtures"
    [void][System.IO.Directory]::CreateDirectory($fixtureDirectory)
    $emptyFixture = Join-Path $fixtureDirectory "schema-only-empty.tps"
    $representativeFixture = Join-Path $fixtureDirectory "representative-96-class.tps"
    Invoke-FixtureTest -Executable $qtFixtureGenerators[0].executablePath `
        -TestFunction "schemaOnlyBlankWorkspaceIsValidAndDeterministic" `
        -OutputEnvironmentName "CLASSMNGR_PHASE7_SCHEMA_ONLY_FIXTURE_OUTPUT_PATH" -OutputPath $emptyFixture -QtBin $qtBin
    Invoke-FixtureTest -Executable $qtFixtureGenerators[1].executablePath `
        -TestFunction "largeStartupFixtureIsCompleteAndDeterministic" `
        -OutputEnvironmentName "CLASSMNGR_LARGE_STARTUP_FIXTURE_OUTPUT_PATH" -OutputPath $representativeFixture -QtBin $qtBin
    $definitions = @(
        [ordered]@{
            id = "empty-workspace"
            fixturePath = $emptyFixture
            fixture = [ordered]@{
                id = "schema-only-v6-empty"
                sha256 = Get-FileSha256 -Path $emptyFixture
                classCount = 0
                profilePathArgumentSupplied = $true
                generatorExecutablePath = $qtFixtureGenerators[0].executablePath
                generatorExecutableSha256 = $qtFixtureGenerators[0].executableSha256
                qtInstallationId = $qtInstallationId
            }
        },
        [ordered]@{
            id = "representative-96-class-workspace"
            fixturePath = $representativeFixture
            fixture = [ordered]@{
                id = "synthetic-large-startup-96-classes"
                sha256 = Get-FileSha256 -Path $representativeFixture
                classCount = 96
                profilePathArgumentSupplied = $true
                generatorExecutablePath = $qtFixtureGenerators[1].executablePath
                generatorExecutableSha256 = $qtFixtureGenerators[1].executableSha256
                qtInstallationId = $qtInstallationId
            }
        }
    )
    foreach ($definition in $definitions) {
        Invoke-Scenario -Definition $definition -ApplicationPath $applicationPath -ApplicationHash $applicationHash `
            -StageDirectory $stageDirectory -FixturePath $definition.fixturePath
    }
    $sourceIdentityAtEnd = Get-CandidateSourceIdentity -Root $RepositoryRoot -Revision $revision
    $Manifest.candidateSource.sha256AtRunEnd = $sourceIdentityAtEnd.sha256
    $Manifest.candidateSource.stableDuringRun = ($sourceIdentityAtEnd.sha256 -eq $sourceIdentity.sha256)
    if (-not $Manifest.candidateSource.stableDuringRun) { throw "Candidate source inputs changed during the evidence run." }
    $cmakeCacheSha256AtRunEnd = Get-FileSha256 -Path $cmakeCachePath
    $Manifest.configuration.cmakeCacheSha256AtRunEnd = $cmakeCacheSha256AtRunEnd
    $Manifest.configuration.cmakeCacheStableDuringRun = ($cmakeCacheSha256AtRunEnd -eq $cmakeCacheSha256)
    if (-not $Manifest.configuration.cmakeCacheStableDuringRun) { throw "CMakeCache.txt changed during the evidence run." }
    $Manifest.status = "completed"
    $Manifest.completedAtUtc = Get-UtcTimestamp
    Write-JsonFile -Path $ManifestPath -Value $Manifest
} catch {
    $runnerFailure = $_.Exception.Message
    $Manifest.status = "failed"
    $Manifest.failure = $runnerFailure
    $Manifest.completedAtUtc = Get-UtcTimestamp
    try { Write-JsonFile -Path $ManifestPath -Value $Manifest } catch { }
    Write-Error "P9-06 package idle-memory collection failed: $runnerFailure" -ErrorAction Continue
} finally {
    if ($null -ne $TemporaryRoot -and (Test-Path -LiteralPath $TemporaryRoot)) {
        Remove-Item -LiteralPath $TemporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

try {
    $validationExitCode = Invoke-Validator
} catch {
    $validationExitCode = 1
    [System.IO.File]::WriteAllText($ValidatorOutputPath, $_.Exception.Message + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    Write-Error "Unable to run P9-06 evidence validator: $($_.Exception.Message)" -ErrorAction Continue
}

Write-Host "Evidence run: $EvidenceDirectory"
Write-Host "Evidence manifest: $ManifestPath"
Write-Host "Process metric trace: $MetricsPath"
Write-Host "Startup diagnostic trace: $StartupLogPath"
Write-Host "Gate summary: $SummaryPath"
if ($null -ne $runnerFailure) { throw "P9-06 collection did not complete; evidence is preserved at $EvidenceDirectory" }
if ($validationExitCode -eq 1) { throw "P9-06 evidence is incomplete or invalid; gate was not evaluated. Evidence: $EvidenceDirectory" }
if ($validationExitCode -eq 2) { throw "P9-06 measured valid evidence over budget; gate failed. Evidence: $EvidenceDirectory" }
if ($validationExitCode -ne 0) { throw "P9-06 validator returned unexpected exit code $validationExitCode. Evidence: $EvidenceDirectory" }
