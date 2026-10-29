<#
.SYNOPSIS
Stages and externally measures the Windows x64 Release ClassMngr Next GUI.

.DESCRIPTION
The runner creates only synthetic .tps fixtures and isolated settings roots
beneath a fresh temporary directory. Its stage is temporary and is never
uploaded. The evidence directory contains only a manifest, metric trace,
summary, and concise validator output.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Join-Path $PSScriptRoot "..\.."),
    [string]$BuildDirectory = "build\windows-x64-release",
    [string]$QtPrefix = "C:\Qt\6.12.0\msvc2022_64",
    [string]$EvidenceDirectory = "artifacts\phase7\windows-x64-next-memory",
    [int]$ReadinessTimeoutSeconds = 120,
    [int]$SettleSeconds = 10
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$TaskId = "P7A-NEXT-MEMORY-IMPLEMENT"
$ManifestSchema = "classmngr-phase7-next-memory-v1"
$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$BuildDirectory = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    [System.IO.Path]::GetFullPath($BuildDirectory)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot $BuildDirectory))
}
$QtPrefix = [System.IO.Path]::GetFullPath($QtPrefix)
$EvidenceDirectory = if ([System.IO.Path]::IsPathRooted($EvidenceDirectory)) {
    [System.IO.Path]::GetFullPath($EvidenceDirectory)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceDirectory))
}
$ValidatorPath = Join-Path $PSScriptRoot "validate_next_memory_evidence.py"
$MetricsPath = Join-Path $EvidenceDirectory "metrics.jsonl"
$ManifestPath = Join-Path $EvidenceDirectory "manifest.json"
$SummaryPath = Join-Path $EvidenceDirectory "summary.json"
$ValidatorOutputPath = Join-Path $EvidenceDirectory "validator-output.txt"

if ($ReadinessTimeoutSeconds -lt 10) {
    throw "ReadinessTimeoutSeconds must be at least 10."
}
if ($SettleSeconds -lt 1) {
    throw "SettleSeconds must be positive."
}
if (-not [Environment]::Is64BitOperatingSystem -or $env:PROCESSOR_ARCHITECTURE -ne "AMD64") {
    throw "Phase 7 Next memory evidence requires a native Windows x64 host."
}
if (Test-Path -LiteralPath $EvidenceDirectory) {
    throw "Refusing to overwrite existing evidence directory: $EvidenceDirectory"
}
if (-not (Test-Path -LiteralPath $ValidatorPath -PathType Leaf)) {
    throw "Phase 7 validator was not found: $ValidatorPath"
}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class Phase7NativeProcessChecks
{
    private const ushort ImageFileMachineUnknown = 0x0000;
    private const ushort ImageFileMachineAmd64 = 0x8664;

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool IsWow64Process2(
        IntPtr process,
        out ushort processMachine,
        out ushort nativeMachine);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);

    public static bool IsNativeX64Process(IntPtr process)
    {
        ushort processMachine;
        ushort nativeMachine;
        if (!IsWow64Process2(process, out processMachine, out nativeMachine))
        {
            throw new System.ComponentModel.Win32Exception(
                Marshal.GetLastWin32Error(), "IsWow64Process2 failed.");
        }
        return processMachine == ImageFileMachineUnknown
            && nativeMachine == ImageFileMachineAmd64;
    }
}
'@

function Get-UtcTimestamp {
    return [DateTimeOffset]::UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss.fffffff'Z'")
}

function ConvertTo-UtcTimestamp {
    param([DateTime]$Value)
    return $Value.ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss.fffffff'Z'")
}

function Write-JsonFile {
    param([string]$Path, [object]$Value)
    $json = $Value | ConvertTo-Json -Depth 32
    [System.IO.File]::WriteAllText(
        $Path,
        $json + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false)
    )
}

function Add-MetricsRow {
    param([string]$Path, [System.Collections.IDictionary]$Row)
    $line = $Row | ConvertTo-Json -Depth 8 -Compress
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
    } finally {
        $sha.Dispose()
    }
}

function Get-FileSha256 {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-PeArchitecture {
    param([string]$Path)
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $reader = [System.IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5a4d) {
            throw "Staged image has no MZ header: $Path"
        }
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 64 -or $peOffset -gt ($stream.Length - 6)) {
            throw "Staged image has an invalid PE header offset: $Path"
        }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "Staged image has no PE signature: $Path"
        }
        $machine = $reader.ReadUInt16()
        if ($machine -eq 0x8664) { return "x64" }
        if ($machine -eq 0xaa64) { return "arm64" }
        return ("unknown-0x{0:x4}" -f $machine)
    } finally {
        $stream.Dispose()
    }
}

function Get-CMakeCacheValue {
    param([string]$CachePath, [string]$Name, [string]$DefaultValue)
    if (-not (Test-Path -LiteralPath $CachePath -PathType Leaf)) {
        throw "CMake cache was not found: $CachePath"
    }
    $line = Get-Content -LiteralPath $CachePath |
        Where-Object { $_ -match "^$([regex]::Escape($Name)):[^=]+=.*$" } |
        Select-Object -Last 1
    if ($null -eq $line) { return $DefaultValue }
    return (($line -split "=", 2)[1]).Trim()
}

function Invoke-CheckedCommand {
    param([string]$FilePath, [string[]]$Arguments, [string]$Description)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

function Invoke-FixtureTest {
    param(
        [string]$Executable,
        [string]$TestFunction,
        [string]$OutputEnvironmentName,
        [string]$OutputPath,
        [string]$QtBin
    )
    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Fixture preparation test executable was not found: $Executable"
    }
    if (Test-Path -LiteralPath $OutputPath) {
        throw "Refusing to overwrite synthetic fixture: $OutputPath"
    }

    $oldEnvironmentValue = [Environment]::GetEnvironmentVariable(
        $OutputEnvironmentName,
        [EnvironmentVariableTarget]::Process
    )
    $oldPath = [Environment]::GetEnvironmentVariable(
        "PATH",
        [EnvironmentVariableTarget]::Process
    )
    try {
        [Environment]::SetEnvironmentVariable(
            $OutputEnvironmentName,
            $OutputPath,
            [EnvironmentVariableTarget]::Process
        )
        [Environment]::SetEnvironmentVariable(
            "PATH",
            "$QtBin;$oldPath",
            [EnvironmentVariableTarget]::Process
        )
        & $Executable $TestFunction
        if ($LASTEXITCODE -ne 0) {
            throw "$TestFunction fixture preparation failed with exit code $LASTEXITCODE."
        }
        if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
            throw "$TestFunction did not create the requested .tps fixture."
        }
    } finally {
        [Environment]::SetEnvironmentVariable(
            $OutputEnvironmentName,
            $oldEnvironmentValue,
            [EnvironmentVariableTarget]::Process
        )
        [Environment]::SetEnvironmentVariable(
            "PATH",
            $oldPath,
            [EnvironmentVariableTarget]::Process
        )
    }
}

function Invoke-NextScenario {
    param(
        [System.Collections.IDictionary]$Manifest,
        [System.Collections.IDictionary]$Definition,
        [string]$ApplicationPath,
        [string]$ApplicationHash,
        [string]$StageDirectory,
        [string]$TemporaryRoot,
        [string]$MetricsTracePath,
        [int]$ReadinessTimeout,
        [int]$SettlingSeconds
    )

    $scenario = [ordered]@{
        id = $Definition.id
        outcome = "in-progress"
        startedAtUtc = Get-UtcTimestamp
        fixture = $Definition.fixture
        readiness = $null
        process = $null
        settleSeconds = $SettlingSeconds
        closeRequestedAtUtc = $null
        exitedAtUtc = $null
        normalExit = $false
        exitCode = $null
    }
    $Manifest.scenarios = @($Manifest.scenarios) + @($scenario)
    $process = $null

    try {
        $settingsRoot = Join-Path $TemporaryRoot ("settings\" + $Definition.id)
        $localAppData = Join-Path $TemporaryRoot ("local-app-data\" + $Definition.id)
        $appData = Join-Path $TemporaryRoot ("app-data\" + $Definition.id)
        $tempPath = Join-Path $TemporaryRoot ("runtime-temp\" + $Definition.id)
        foreach ($directory in @($settingsRoot, $localAppData, $appData, $tempPath)) {
            [void][System.IO.Directory]::CreateDirectory($directory)
        }

        $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $ApplicationPath
        $startInfo.WorkingDirectory = $StageDirectory
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $false
        if ($Definition.fixture.profilePathArgumentSupplied) {
            $startInfo.ArgumentList.Add([string]$Definition.fixturePath)
        }
        $startInfo.Environment["CLASSMNGR_SETTINGS_ROOT"] = $settingsRoot
        $startInfo.Environment["LOCALAPPDATA"] = $localAppData
        $startInfo.Environment["APPDATA"] = $appData
        $startInfo.Environment["TEMP"] = $tempPath
        $startInfo.Environment["TMP"] = $tempPath
        $startInfo.Environment["PATH"] = "$StageDirectory;$env:SystemRoot\System32;$env:SystemRoot"

        $process = [System.Diagnostics.Process]::new()
        $process.StartInfo = $startInfo
        if (-not $process.Start()) {
            throw "Unable to start staged ClassMngrNext.exe for $($Definition.id)."
        }
        $process.Refresh()
        $processCreatedAt = $process.StartTime.ToUniversalTime()
        $processCreatedAtUtc = ConvertTo-UtcTimestamp $processCreatedAt
        $canonicalProcessPath = [System.IO.Path]::GetFullPath($process.MainModule.FileName)
        if (-not [string]::Equals(
            $canonicalProcessPath,
            $ApplicationPath,
            [StringComparison]::OrdinalIgnoreCase
        )) {
            throw "Launched process image path did not match the staged image."
        }
        if (-not [Phase7NativeProcessChecks]::IsNativeX64Process($process.Handle)) {
            throw "Launched process was not a native x64 process."
        }
        if ((Get-FileSha256 -Path $canonicalProcessPath) -ne $ApplicationHash) {
            throw "Launched process image SHA-256 differed from the staged image."
        }

        $scenario.process = [ordered]@{
            pid = $process.Id
            createdAtUtc = $processCreatedAtUtc
            canonicalExecutablePath = $canonicalProcessPath
            architecture = "x64"
            sha256 = $ApplicationHash
            mainWindowHandle = $null
            mainWindowTitle = $null
        }

        $stablePolls = 0
        $stableHandle = [Int64]0
        $stableTitle = ""
        $readinessFound = $false
        $deadline = [DateTimeOffset]::UtcNow.AddSeconds($ReadinessTimeout)
        while ([DateTimeOffset]::UtcNow -lt $deadline) {
            $process.Refresh()
            if ($process.HasExited) {
                throw "ClassMngrNext.exe exited before GUI readiness with code $($process.ExitCode)."
            }

            $windowHandle = $process.MainWindowHandle
            $windowTitle = $process.MainWindowTitle.Trim()
            $windowVisible = $windowHandle -ne [IntPtr]::Zero -and
                [Phase7NativeProcessChecks]::IsWindowVisible($windowHandle)
            $titleMatches = $windowTitle.Contains("ClassMngr Next")
            if ($windowVisible -and $titleMatches) {
                $handleValue = $windowHandle.ToInt64()
                if ($stableHandle -eq $handleValue -and $stableTitle -ceq $windowTitle) {
                    $stablePolls++
                } else {
                    $stableHandle = $handleValue
                    $stableTitle = $windowTitle
                    $stablePolls = 1
                }
                if ($stablePolls -ge 3) {
                    $readinessFound = $true
                    break
                }
            } else {
                $stablePolls = 0
                $stableHandle = [Int64]0
                $stableTitle = ""
            }
            Start-Sleep -Seconds 1
        }
        if (-not $readinessFound) {
            throw "No stable visible ClassMngr Next main window appeared within $ReadinessTimeout seconds."
        }

        $readyAtUtc = Get-UtcTimestamp
        $scenario.readiness = [ordered]@{
            readyAtUtc = $readyAtUtc
            windowVisible = $true
            stablePolls = 3
            mainWindowHandle = $stableHandle
            mainWindowTitle = $stableTitle
        }
        $scenario.process.mainWindowHandle = $stableHandle
        $scenario.process.mainWindowTitle = $stableTitle

        Start-Sleep -Seconds $SettlingSeconds
        $process.Refresh()
        if ($process.HasExited) {
            throw "ClassMngrNext.exe exited during the $SettlingSeconds-second settling period."
        }

        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        $nextSampleAtMilliseconds = [Int64]0
        for ($sampleIndex = 1; $sampleIndex -le 5; $sampleIndex++) {
            while ($stopwatch.ElapsedMilliseconds -lt $nextSampleAtMilliseconds) {
                $remaining = $nextSampleAtMilliseconds - $stopwatch.ElapsedMilliseconds
                Start-Sleep -Milliseconds ([Math]::Max(1, [Math]::Min(50, $remaining)))
            }
            $nextSampleAtMilliseconds += 1000

            $process.Refresh()
            if ($process.HasExited) {
                throw "ClassMngrNext.exe exited during external memory sampling."
            }
            $currentProcessPath = [System.IO.Path]::GetFullPath($process.MainModule.FileName)
            if (-not [string]::Equals(
                $currentProcessPath,
                $canonicalProcessPath,
                [StringComparison]::OrdinalIgnoreCase
            )) {
                throw "Sampled process image path changed during the evidence run."
            }
            if (-not [Phase7NativeProcessChecks]::IsNativeX64Process($process.Handle)) {
                throw "Sampled process architecture was not native x64."
            }

            $sample = [ordered]@{
                scenarioId = $Definition.id
                sampleIndex = $sampleIndex
                timestampUtc = Get-UtcTimestamp
                pid = $process.Id
                processCreatedAtUtc = $processCreatedAtUtc
                canonicalExecutablePath = $canonicalProcessPath
                architecture = "x64"
                sha256 = $ApplicationHash
                currentWorkingSetBytes = [Int64]$process.WorkingSet64
                privateBytes = [Int64]$process.PrivateMemorySize64
                peakWorkingSetBytes = [Int64]$process.PeakWorkingSet64
                handles = [Int64]$process.HandleCount
                threads = [Int64]$process.Threads.Count
            }
            Add-MetricsRow -Path $MetricsTracePath -Row $sample
        }
        $stopwatch.Stop()

        $scenario.closeRequestedAtUtc = Get-UtcTimestamp
        if (-not $process.CloseMainWindow()) {
            $process.Refresh()
            if (-not $process.HasExited) {
                throw "CloseMainWindow was not accepted for $($Definition.id)."
            }
        }
        if (-not $process.WaitForExit(30000)) {
            throw "ClassMngrNext.exe did not exit within 30 seconds after the normal close request."
        }
        $process.Refresh()
        $scenario.exitedAtUtc = Get-UtcTimestamp
        $scenario.exitCode = $process.ExitCode
        $scenario.normalExit = ($process.ExitCode -eq 0)
        if (-not $scenario.normalExit) {
            throw "ClassMngrNext.exe exited abnormally with code $($process.ExitCode)."
        }
        $scenario.outcome = "completed"
    } catch {
        $scenario.outcome = "failed"
        $scenario.failure = $_.Exception.Message
        if ($null -ne $process) {
            try {
                $process.Refresh()
                if (-not $process.HasExited) {
                    [void]$process.CloseMainWindow()
                    if (-not $process.WaitForExit(5000)) {
                        $process.Kill($true)
                        $process.WaitForExit()
                    }
                }
                if ($null -eq $scenario.exitedAtUtc -and $process.HasExited) {
                    $scenario.exitedAtUtc = Get-UtcTimestamp
                    $scenario.exitCode = $process.ExitCode
                }
            } catch {
                # Preserve the original collection error. Process cleanup is best-effort.
            }
        }
        throw
    } finally {
        if ($null -ne $process) { $process.Dispose() }
    }
}

function Invoke-Validator {
    param([string]$PythonCommand)
    $arguments = @(
        $ValidatorPath,
        "--manifest", $ManifestPath,
        "--metrics", $MetricsPath,
        "--summary", $SummaryPath
    )
    $output = & $PythonCommand @arguments 2>&1
    $exitCode = $LASTEXITCODE
    [System.IO.File]::WriteAllText(
        $ValidatorOutputPath,
        ((@($output) -join [Environment]::NewLine) + [Environment]::NewLine),
        [System.Text.UTF8Encoding]::new($false)
    )
    foreach ($line in $output) { Write-Host $line }
    return $exitCode
}

[void][System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($EvidenceDirectory))
[void][System.IO.Directory]::CreateDirectory($EvidenceDirectory)
[System.IO.File]::WriteAllText(
    $MetricsPath,
    "",
    [System.Text.UTF8Encoding]::new($false)
)

$temporaryParent = if (-not [string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)) {
    $env:RUNNER_TEMP
} else {
    [System.IO.Path]::GetTempPath()
}
$TemporaryRoot = Join-Path $temporaryParent ("phase7-next-memory-" + [Guid]::NewGuid().ToString("N"))
$Manifest = [ordered]@{
    schema = $ManifestSchema
    taskId = $TaskId
    status = "in-progress"
    startedAtUtc = Get-UtcTimestamp
    completedAtUtc = $null
    revision = $null
    configuration = [ordered]@{
        preset = "windows-x64-release"
        buildType = "Release"
        architecture = "x64"
    }
    os = $null
    application = $null
    scenarios = @()
    failure = $null
}
$runnerExitCode = 0

try {
    [void][System.IO.Directory]::CreateDirectory($TemporaryRoot)
    $gitOutput = & git -C $RepositoryRoot rev-parse --verify HEAD 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to read the checked-out Git revision."
    }
    $Manifest.revision = ([string]($gitOutput | Select-Object -Last 1)).Trim()
    if ($Manifest.revision -notmatch '^(?:[0-9a-f]{40}|[0-9a-f]{64})$') {
        throw "The checked-out revision is not a full lowercase Git object ID."
    }

    $osInfo = Get-CimInstance -ClassName Win32_OperatingSystem
    $Manifest.os = [ordered]@{
        platform = "Windows"
        caption = [string]$osInfo.Caption
        version = [string]$osInfo.Version
        buildNumber = [string]$osInfo.BuildNumber
        architecture = "x64"
        machineArchitecture = $env:PROCESSOR_ARCHITECTURE
    }

    $releaseExecutable = Join-Path $BuildDirectory "Release\ClassMngrNext.exe"
    if (-not (Test-Path -LiteralPath $releaseExecutable -PathType Leaf)) {
        throw "Windows x64 Release ClassMngrNext.exe was not found: $releaseExecutable"
    }
    if ((Get-PeArchitecture -Path $releaseExecutable) -ne "x64") {
        throw "Release ClassMngrNext.exe is not a PE x64 image."
    }
    $cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
    $installBindir = Get-CMakeCacheValue -CachePath $cachePath -Name "CMAKE_INSTALL_BINDIR" -DefaultValue "bin"
    if ([System.IO.Path]::IsPathRooted($installBindir) -or ($installBindir -split "[\\/]") -contains "..") {
        throw "CMAKE_INSTALL_BINDIR must be a relative install directory."
    }
    $installBindir = $installBindir.Replace("/", "\").Trim("\")
    if ([string]::IsNullOrWhiteSpace($installBindir)) { $installBindir = "bin" }

    $stageRoot = Join-Path $TemporaryRoot "staged-next"
    $stageDirectory = Join-Path $stageRoot $installBindir
    [void][System.IO.Directory]::CreateDirectory($stageDirectory)

    Invoke-CheckedCommand -FilePath "cmake" -Arguments @(
        "--install", $BuildDirectory,
        "--config", "Release",
        "--prefix", $stageRoot,
        "--component", "ClassMngrNextResources"
    ) -Description "Installing ClassMngrNext resources into the temporary stage"

    $stagedExecutable = Join-Path $stageDirectory "ClassMngrNext.exe"
    Copy-Item -LiteralPath $releaseExecutable -Destination $stagedExecutable
    $sourceHash = Get-FileSha256 -Path $releaseExecutable
    $stagedHash = Get-FileSha256 -Path $stagedExecutable
    if ($sourceHash -ne $stagedHash) {
        throw "Staged ClassMngrNext.exe does not match the Release build image."
    }
    if ((Get-PeArchitecture -Path $stagedExecutable) -ne "x64") {
        throw "Staged ClassMngrNext.exe is not a PE x64 image."
    }

    $resourceDirectory = Join-Path $stageDirectory "resources"
    $resourceCatalog = Join-Path $resourceDirectory "resource-catalog.json"
    if (-not (Test-Path -LiteralPath $resourceCatalog -PathType Leaf)) {
        throw "The ClassMngrNext resource catalog was not installed beside the staged executable."
    }
    if (@(Get-ChildItem -LiteralPath $resourceDirectory -File -Recurse).Count -lt 1) {
        throw "The ClassMngrNext resources directory is empty."
    }

    $qtBin = Join-Path $QtPrefix "bin"
    $windeployqt = Join-Path $qtBin "windeployqt.exe"
    if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) {
        throw "Qt runtime deployment tool was not found: $windeployqt"
    }
    $oldDeployPath = [Environment]::GetEnvironmentVariable("PATH", [EnvironmentVariableTarget]::Process)
    try {
        [Environment]::SetEnvironmentVariable(
            "PATH",
            "$qtBin;$oldDeployPath",
            [EnvironmentVariableTarget]::Process
        )
        $deployArguments = @(
            "--qmldir", (Join-Path $RepositoryRoot "src\features\calendar\ui\qml"),
            "--skip-plugin-types", "qmltooling",
            "--include-plugins", "qoffscreen,qsqlite",
            "--exclude-plugins", "qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql",
            "--translations", "en,ko",
            "--compiler-runtime",
            "--dir", $stageDirectory,
            $stagedExecutable
        )
        Invoke-CheckedCommand -FilePath $windeployqt -Arguments $deployArguments -Description "Deploying Qt runtime to the temporary Next stage"
    } finally {
        [Environment]::SetEnvironmentVariable(
            "PATH",
            $oldDeployPath,
            [EnvironmentVariableTarget]::Process
        )
    }

    foreach ($relativePath in @(
        "Qt6Core.dll",
        "Qt6Gui.dll",
        "Qt6Widgets.dll",
        "Qt6Sql.dll",
        "Qt6Network.dll",
        "platforms\qwindows.dll",
        "sqldrivers\qsqlite.dll"
    )) {
        $deployedPath = Join-Path $stageDirectory $relativePath
        if (-not (Test-Path -LiteralPath $deployedPath -PathType Leaf)) {
            throw "Required deployed runtime file was not found: $relativePath"
        }
    }

    $Manifest.application = [ordered]@{
        canonicalPath = [System.IO.Path]::GetFullPath($stagedExecutable)
        sha256 = $stagedHash
        architecture = "x64"
        resourceCatalogPresent = $true
        qtRuntimeDeployed = $true
    }

    $fixtureDirectory = Join-Path $TemporaryRoot "fixtures"
    [void][System.IO.Directory]::CreateDirectory($fixtureDirectory)
    $blankFixture = Join-Path $fixtureDirectory "schema-only-blank.tps"
    $representativeFixture = Join-Path $fixtureDirectory "representative-96-class.tps"
    $qtBin = Join-Path $QtPrefix "bin"
    Invoke-FixtureTest `
        -Executable (Join-Path $BuildDirectory "Release\ClassMngrPhase7NextBlankFixtureTests.exe") `
        -TestFunction "schemaOnlyBlankWorkspaceIsValidAndDeterministic" `
        -OutputEnvironmentName "CLASSMNGR_PHASE7_SCHEMA_ONLY_FIXTURE_OUTPUT_PATH" `
        -OutputPath $blankFixture `
        -QtBin $qtBin
    Invoke-FixtureTest `
        -Executable (Join-Path $BuildDirectory "Release\ClassMngrStartupPerformanceTests.exe") `
        -TestFunction "largeStartupFixtureIsCompleteAndDeterministic" `
        -OutputEnvironmentName "CLASSMNGR_LARGE_STARTUP_FIXTURE_OUTPUT_PATH" `
        -OutputPath $representativeFixture `
        -QtBin $qtBin

    $noProfileFixtureHash = Get-StringSha256 -Value "classmngr-phase7-no-profile-fixture-v1`n"
    $fixtureDefinitions = @(
        [ordered]@{
            id = "fresh-initial-setup"
            fixturePath = $null
            fixture = [ordered]@{
                id = "fresh-no-profile"
                sha256 = $noProfileFixtureHash
                profilePathArgumentSupplied = $false
                classCount = $null
            }
        },
        [ordered]@{
            id = "schema-only-blank-workspace"
            fixturePath = $blankFixture
            fixture = [ordered]@{
                id = "schema-only-v6-empty"
                sha256 = Get-FileSha256 -Path $blankFixture
                profilePathArgumentSupplied = $true
                classCount = 0
            }
        },
        [ordered]@{
            id = "representative-96-class-workspace"
            fixturePath = $representativeFixture
            fixture = [ordered]@{
                id = "synthetic-large-startup-96-classes"
                sha256 = Get-FileSha256 -Path $representativeFixture
                profilePathArgumentSupplied = $true
                classCount = 96
            }
        }
    )

    foreach ($definition in $fixtureDefinitions) {
        Invoke-NextScenario `
            -Manifest $Manifest `
            -Definition $definition `
            -ApplicationPath $stagedExecutable `
            -ApplicationHash $stagedHash `
            -StageDirectory $stageDirectory `
            -TemporaryRoot $TemporaryRoot `
            -MetricsTracePath $MetricsPath `
            -ReadinessTimeout $ReadinessTimeoutSeconds `
            -SettlingSeconds $SettleSeconds
        Write-JsonFile -Path $ManifestPath -Value $Manifest
    }
    $Manifest.status = "completed"
    $Manifest.completedAtUtc = Get-UtcTimestamp
    Write-JsonFile -Path $ManifestPath -Value $Manifest
} catch {
    $runnerExitCode = 1
    $Manifest.status = "failed"
    $Manifest.failure = $_.Exception.Message
    if ($null -eq $Manifest.completedAtUtc) {
        $Manifest.completedAtUtc = Get-UtcTimestamp
    }
    Write-JsonFile -Path $ManifestPath -Value $Manifest
    Write-Error "Phase 7 Next memory evidence failed: $($Manifest.failure)" -ErrorAction Continue
} finally {
    if (Test-Path -LiteralPath $TemporaryRoot) {
        Remove-Item -LiteralPath $TemporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

try {
    $validationExitCode = Invoke-Validator -PythonCommand "python"
    if ($validationExitCode -ne 0) {
        $runnerExitCode = 1
        if ($Manifest.status -eq "completed") {
            $Manifest.status = "failed"
            $Manifest.failure = "Phase 7 evidence validator rejected the collected run."
            Write-JsonFile -Path $ManifestPath -Value $Manifest
            [void](Invoke-Validator -PythonCommand "python")
        }
    }
} catch {
    $runnerExitCode = 1
    Write-Error "Unable to run the Phase 7 evidence validator: $($_.Exception.Message)" -ErrorAction Continue
}

if ($runnerExitCode -ne 0) {
    throw "Phase 7 Next memory evidence did not produce a valid complete evidence run."
}

Write-Host "Evidence manifest: $ManifestPath"
Write-Host "External metric trace: $MetricsPath"
Write-Host "Validator summary: $SummaryPath"
