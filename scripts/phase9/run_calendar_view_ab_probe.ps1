<#
.SYNOPSIS
Measures Calendar page lifecycle memory in four diagnostic-only view arms.

.DESCRIPTION
Runs one fresh test process per arm with the same CalendarPage, workspace,
providers, and shell fixture. The arms vary only the CalendarViewFactory result:
the real QML grid, an empty QQuickWidget, a plain QWidget, and the production
QWidget grid. The results are not P9-07 acceptance evidence.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Join-Path $PSScriptRoot "..\.."),
    [string]$BuildDirectory = "build\windows-x64-release",
    [string]$EvidenceRoot = "artifacts\phase9\calendar-view-ab",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$TaskId = "P9_07_CALENDAR_MEMORY_REMEDIATION"
$Schema = "classmngr-phase9-calendar-view-ab-probe-v2"
$Arms = @("qml", "minimal-qquick", "minimal-widget", "widget-grid")
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
    throw "Refusing to overwrite diagnostic evidence: $EvidenceDirectory"
}
New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
$ArmsDirectory = Join-Path $EvidenceDirectory "arms"
New-Item -ItemType Directory -Path $ArmsDirectory | Out-Null
$BuildLogPath = Join-Path $EvidenceDirectory "release-build.log"
$ManifestPath = Join-Path $EvidenceDirectory "manifest.json"

function Get-FileSha256 {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Write-JsonFile {
    param([string]$Path, [object]$Value)
    $Json = $Value | ConvertTo-Json -Depth 40
    [System.IO.File]::WriteAllText(
        $Path, $Json + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
}

function Get-SourceIdentity {
    param([string]$Root)
    $RelativeFiles = @(
        "cmake/tests/next.cmake",
        "src/next/bootstrap/application_bootstrap.cpp",
        "src/next/bootstrap/qt_page_lifecycle_diagnostics.cpp",
        "src/next/bootstrap/qt_page_lifecycle_diagnostics.h",
        "src/next/ui/calendar_page.cpp",
        "src/next/ui/calendar_page.h",
        "src/next/ui/calendar_view.cpp",
        "src/next/ui/calendar_view.h",
        "src/features/calendar/ui/qml/EventCalendar.qml",
        "src/features/calendar/ui/qml/MonthGridDelegate.qml",
        "scripts/phase9/run_calendar_view_ab_probe.ps1",
        "tests/next/calendar_page_tests.cpp"
    )
    $Files = [System.Collections.Generic.List[object]]::new()
    foreach ($Relative in $RelativeFiles) {
        $FullPath = Join-Path $Root $Relative
        if (-not (Test-Path -LiteralPath $FullPath -PathType Leaf)) {
            throw "Required source identity file is missing: $Relative"
        }
        $Item = Get-Item -LiteralPath $FullPath
        $Files.Add([ordered]@{
            path = $Relative.Replace("\", "/")
            sizeBytes = [Int64]$Item.Length
            sha256 = Get-FileSha256 -Path $FullPath
        })
    }
    $Canonical = $Files | ConvertTo-Json -Depth 8 -Compress
    $Sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $Digest = [System.BitConverter]::ToString(
            $Sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($Canonical)))
        return [ordered]@{
            sha256 = $Digest.Replace("-", "").ToLowerInvariant()
            files = @($Files)
        }
    } finally {
        $Sha.Dispose()
    }
}

$CachePath = Join-Path $BuildDirectory "CMakeCache.txt"
if (-not (Test-Path -LiteralPath $CachePath -PathType Leaf)) {
    throw "Configured Release tree does not contain CMakeCache.txt: $BuildDirectory"
}
$CacheText = [System.IO.File]::ReadAllText($CachePath)
$QtMatch = [regex]::Match($CacheText, "(?m)^Qt6_DIR:PATH=(.+)$")
if (-not $QtMatch.Success) {
    throw "Release CMakeCache.txt does not contain Qt6_DIR."
}
$Qt6Directory = [System.IO.Path]::GetFullPath($QtMatch.Groups[1].Value.Trim())
$QtPrefix = [System.IO.Path]::GetFullPath((Join-Path $Qt6Directory "..\..\.."))
$QtBin = Join-Path $QtPrefix "bin"
$PluginRoot = Join-Path $QtPrefix "plugins"
$QtCorePath = Join-Path $QtBin "Qt6Core.dll"
$OffscreenPlugin = Join-Path $PluginRoot "platforms\qoffscreen.dll"
foreach ($RequiredPath in @($QtCorePath, $OffscreenPlugin)) {
    if (-not (Test-Path -LiteralPath $RequiredPath -PathType Leaf)) {
        throw "Release build Qt runtime is incomplete: $RequiredPath"
    }
}

if (-not $SkipBuild) {
    $BuildOutput = & cmake --build $BuildDirectory --config Release --target ClassMngrNextCalendarPageTests 2>&1
    $BuildExitCode = $LASTEXITCODE
    [System.IO.File]::WriteAllText(
        $BuildLogPath,
        (($BuildOutput | ForEach-Object { [string]$_ }) -join [Environment]::NewLine) + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
    if ($BuildExitCode -ne 0) {
        throw "Release Calendar page test target build failed with exit code $BuildExitCode. Log: $BuildLogPath"
    }
} else {
    [System.IO.File]::WriteAllText(
        $BuildLogPath,
        "Build skipped by caller; executable identity recorded below." + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
}

$Executable = Join-Path $BuildDirectory "Release\ClassMngrNextCalendarPageTests.exe"
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Release Calendar page test executable was not produced: $Executable"
}
$SourceStart = Get-SourceIdentity -Root $RepositoryRoot
$ExecutableHash = Get-FileSha256 -Path $Executable
$QtDllVersion = (Get-Item -LiteralPath $QtCorePath).VersionInfo.FileVersion
$Manifest = [ordered]@{
    schema = $Schema
    taskId = $TaskId
    runId = $RunId
    diagnosticOnly = $true
    acceptanceEvidence = $false
    status = "running"
    createdAtUtc = [DateTimeOffset]::UtcNow.ToString("o")
    completedAtUtc = $null
    repositoryRoot = $RepositoryRoot
    buildDirectory = $BuildDirectory
    configuration = "Release"
    cmakeCachePath = $CachePath
    cmakeCacheSha256 = Get-FileSha256 -Path $CachePath
    qt6Directory = $Qt6Directory
    qtPrefix = $QtPrefix
    qtCoreFileVersion = $QtDllVersion
    qtCoreSha256 = Get-FileSha256 -Path $QtCorePath
    offscreenPluginPath = $OffscreenPlugin
    offscreenPluginSha256 = Get-FileSha256 -Path $OffscreenPlugin
    executable = $Executable
    executableSha256 = $ExecutableHash
    sourceIdentityAtStart = $SourceStart
    sourceIdentityAtEnd = $null
    arms = [System.Collections.Generic.List[object]]::new()
    failure = $null
}
Write-JsonFile -Path $ManifestPath -Value $Manifest

try {
    foreach ($Arm in $Arms) {
        $ArmDirectory = Join-Path $ArmsDirectory $Arm
        New-Item -ItemType Directory -Path $ArmDirectory | Out-Null
        $StdoutPath = Join-Path $ArmDirectory "stdout.log"
        $StderrPath = Join-Path $ArmDirectory "stderr.log"
        $ResultPath = Join-Path $ArmDirectory "result.json"
        $StartInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $StartInfo.FileName = $Executable
        $StartInfo.WorkingDirectory = $BuildDirectory
        $StartInfo.UseShellExecute = $false
        $StartInfo.CreateNoWindow = $true
        $StartInfo.RedirectStandardOutput = $true
        $StartInfo.RedirectStandardError = $true
        $StartInfo.ArgumentList.Add("calendarViewMemoryProbe")
        $StartInfo.Environment["PATH"] = "$QtBin;$($env:PATH)"
        $StartInfo.Environment["QT_PLUGIN_PATH"] = $PluginRoot
        $StartInfo.Environment["QT_QPA_PLATFORM_PLUGIN_PATH"] = Join-Path $PluginRoot "platforms"
        $StartInfo.Environment["QT_QPA_PLATFORM"] = "offscreen"
        $StartInfo.Environment["QML2_IMPORT_PATH"] = Join-Path $QtPrefix "qml"
        $StartInfo.Environment["CLASSMNGR_CALENDAR_VIEW_PROBE_ARM"] = $Arm
        $StartInfo.Environment["CLASSMNGR_CALENDAR_VIEW_PROBE_OUTPUT"] = $ResultPath
        foreach ($Name in @("QT_QPA_FONTDIR", "QT_QPA_PLATFORMTHEME", "QT_SCALE_FACTOR", "QT_SCALE_FACTOR_ROUNDING_POLICY", "QT_AUTO_SCREEN_SCALE_FACTOR")) {
            $null = $StartInfo.Environment.Remove($Name)
        }

        $Process = [System.Diagnostics.Process]::new()
        $Process.StartInfo = $StartInfo
        if (-not $Process.Start()) {
            throw "Could not start diagnostic arm '$Arm'."
        }
        $CreatedAtUtc = $Process.StartTime.ToUniversalTime().ToString("o")
        $StdoutTask = $Process.StandardOutput.ReadToEndAsync()
        $StderrTask = $Process.StandardError.ReadToEndAsync()
        $Process.WaitForExit()
        $Stdout = $StdoutTask.GetAwaiter().GetResult()
        $Stderr = $StderrTask.GetAwaiter().GetResult()
        [System.IO.File]::WriteAllText($StdoutPath, $Stdout, [System.Text.UTF8Encoding]::new($false))
        [System.IO.File]::WriteAllText($StderrPath, $Stderr, [System.Text.UTF8Encoding]::new($false))
        if ($Process.ExitCode -ne 0) {
            throw "Diagnostic arm '$Arm' exited with $($Process.ExitCode). See $StderrPath."
        }

        if (-not (Test-Path -LiteralPath $ResultPath -PathType Leaf)) {
            throw "Diagnostic arm '$Arm' did not write its JSON result file. See $StdoutPath and $StderrPath."
        }
        $Result = [System.IO.File]::ReadAllText($ResultPath) | ConvertFrom-Json -AsHashtable
        if ($Result.arm -cne $Arm) {
            throw "Diagnostic arm '$Arm' returned mismatched result identity '$($Result.arm)'."
        }
        $Manifest.arms.Add([ordered]@{
            arm = $Arm
            processId = [int]$Process.Id
            processCreatedAtUtc = $CreatedAtUtc
            completedAtUtc = [DateTimeOffset]::UtcNow.ToString("o")
            exitCode = [int]$Process.ExitCode
            stdoutPath = $StdoutPath
            stdoutSha256 = Get-FileSha256 -Path $StdoutPath
            stderrPath = $StderrPath
            stderrSha256 = Get-FileSha256 -Path $StderrPath
            resultPath = $ResultPath
            resultSha256 = Get-FileSha256 -Path $ResultPath
            workingSetDeltaBytes = [Int64]$Result.content_ready.workingSetBytes - [Int64]$Result.baseline.workingSetBytes
            privateUsageDeltaBytes = [Int64]$Result.content_ready.privateUsageBytes - [Int64]$Result.baseline.privateUsageBytes
            postSuspensionWorkingSetDeltaBytes = [Int64]$Result.post_suspension.workingSetBytes - [Int64]$Result.baseline.workingSetBytes
            postSuspensionPrivateUsageDeltaBytes = [Int64]$Result.post_suspension.privateUsageBytes - [Int64]$Result.baseline.privateUsageBytes
            quickRootObjectPresent = [bool]$Result.quick_root_object_present
            widgetDayCellCount = [int]$Result.widget_day_cell_count
            widgetEventButtonCount = [int]$Result.widget_event_button_count
            runtimeMetrics = $Result.content_ready_runtime_metrics
        })
        Write-JsonFile -Path $ManifestPath -Value $Manifest
    }

    $SourceEnd = Get-SourceIdentity -Root $RepositoryRoot
    if ($SourceStart.sha256 -cne $SourceEnd.sha256) {
        throw "Source identity changed during the diagnostic A/B run."
    }
    $Manifest.sourceIdentityAtEnd = $SourceEnd
    $Manifest.status = "complete"
    $Manifest.completedAtUtc = [DateTimeOffset]::UtcNow.ToString("o")
    Write-JsonFile -Path $ManifestPath -Value $Manifest
} catch {
    $Manifest.status = "failed"
    $Manifest.completedAtUtc = [DateTimeOffset]::UtcNow.ToString("o")
    $Manifest.failure = $_.Exception.Message
    try {
        $Manifest.sourceIdentityAtEnd = Get-SourceIdentity -Root $RepositoryRoot
    } catch {
        $Manifest.failure += " | end fingerprint: " + $_.Exception.Message
    }
    Write-JsonFile -Path $ManifestPath -Value $Manifest
    throw
}

Write-Host "Calendar view diagnostic run: $RunId"
Write-Host "Manifest: $ManifestPath"
