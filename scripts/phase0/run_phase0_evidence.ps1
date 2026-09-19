<#
.SYNOPSIS
Plans or runs the packaged Windows x64 Phase 0 evidence routes.

.DESCRIPTION
Without -EvidenceParent the script is plan-only. A run never writes into the
repository; it creates a timestamped child directory beneath EvidenceParent.
RunDirectoryName, when supplied, selects that child directory's leaf name.

.PARAMETER EvidenceParent
Absolute parent directory for the timestamped evidence run directory. This is
the parent, not the run directory itself. If omitted, the invocation remains
plan-only. -EvidenceRoot is retained as an alias.

.PARAMETER RunDirectoryName
Optional leaf directory name beneath EvidenceParent. If omitted, a UTC
timestamped name is selected.

.PARAMETER Plan
Prints the selected routes and commands without creating files or launching
processes. -PlanOnly is an alias.

.PARAMETER SkipRun
Skips evidence route execution and records routes as skipped. Build steps may
still run unless -SkipBuild is also supplied.

.PARAMETER AllowExistingRoot
Allows reusing only an existing empty run directory. Non-empty or retained
evidence roots are never overwritten.
#>
[CmdletBinding()]
param(
    [Alias("EvidenceRoot")][string]$EvidenceParent = "",
    [string]$RunDirectoryName = "",
    [string]$ApplicationPath = "",
    [string]$TestExecutable = "",
    [string]$ReleaseBuildDirectory = "",
    [string]$TestBuildDirectory = "",
    [string]$InstallDirectory = "",
    [string]$QtPrefix = "C:\Qt\6.12.0\msvc2022_64",
    [string]$QtBinPath = "",
    [string]$PythonCommand = "python",
    [int]$Parallel = 2,
    [int]$TimeoutSeconds = 900,
    [string[]]$Routes = @("all"),
    [Alias("PlanOnly")][switch]$Plan,
    [switch]$SkipBuild,
    [switch]$SkipRun,
    [switch]$SkipValidation,
    [switch]$AllowExistingRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$TaskId = "QT0-EVIDENCE-AUTOMATION"
$RunnerTaskId = "QT0-AUTO-RUNNER"
$RunSchema = "classmngr-phase0-evidence-run-v1"
$RouteSchema = "classmngr-phase0-route-v1"
$DeferredPlatforms = @("windows-arm64", "linux")
$SupportedPlatforms = @("windows-x64", "macos-universal")
$LegacyResidentTargetBytes = 250 * 1024 * 1024
$TemporaryDiagnosticCeilingBytes = 512 * 1024 * 1024
$RepositoryRoot = [System.IO.Path]::GetFullPath(
    [System.IO.Path]::Combine($PSScriptRoot, "..", "..")
)

function Get-FullPath {
    param([string]$Path, [string]$BasePath)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($BasePath, $Path))
}

function Write-JsonFile {
    param([string]$Path, [object]$Value)
    $parent = [System.IO.Path]::GetDirectoryName($Path)
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        [System.IO.Directory]::CreateDirectory($parent) | Out-Null
    }
    $json = $Value | ConvertTo-Json -Depth 40
    [System.IO.File]::WriteAllText(
        $Path,
        $json + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false)
    )
}

function ConvertTo-CommandLineArgument {
    param([AllowEmptyString()][string]$Value)
    if ($Value.Length -gt 0 -and $Value -notmatch '[\s"]') {
        return $Value
    }
    $builder = [System.Text.StringBuilder]::new()
    [void]$builder.Append('"')
    $slashes = 0
    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq '\') {
            $slashes++
            continue
        }
        if ($character -eq '"') {
            [void]$builder.Append(('\' * (($slashes * 2) + 1)))
            [void]$builder.Append('"')
            $slashes = 0
            continue
        }
        if ($slashes -gt 0) {
            [void]$builder.Append(('\' * $slashes))
            $slashes = 0
        }
        [void]$builder.Append($character)
    }
    if ($slashes -gt 0) {
        [void]$builder.Append(('\' * ($slashes * 2)))
    }
    [void]$builder.Append('"')
    return $builder.ToString()
}

function Get-CommandDisplay {
    param([string]$FilePath, [string[]]$Arguments)
    return ((@($FilePath) + $Arguments) | ForEach-Object {
        ConvertTo-CommandLineArgument -Value ([string]$_)
    }) -join " "
}

function Test-IsWindowsX64 {
    return (
        [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT -and
        [Environment]::GetEnvironmentVariable("PROCESSOR_ARCHITECTURE") -eq "AMD64"
    )
}

function Get-HostPlatformName {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        return "non-windows"
    }
    $architecture = [Environment]::GetEnvironmentVariable("PROCESSOR_ARCHITECTURE")
    if ($architecture -eq "AMD64") {
        return "windows-x64"
    }
    return "windows-$($architecture.ToLowerInvariant())"
}

function Assert-SafeEvidenceParent {
    param([string]$Path, [string]$Repository)
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $pathRoot = [System.IO.Path]::GetPathRoot($fullPath)
    if ([StringComparer]::OrdinalIgnoreCase.Equals($fullPath.TrimEnd('\'), $pathRoot.TrimEnd('\'))) {
        throw "Evidence path '$fullPath' cannot be a filesystem root."
    }
    $fullPath = $fullPath.TrimEnd('\')
    $repo = $Repository.TrimEnd('\')
    $forbidden = @(
        $repo,
        (Join-Path $repo "build"),
        (Join-Path $repo "dist"),
        (Join-Path $repo ".git"),
        (Join-Path $repo "agent_docs"),
        (Join-Path $repo "src"),
        (Join-Path $repo "tests"),
        (Join-Path $repo "cmake")
    )
    foreach ($candidate in $forbidden) {
        $resolved = [System.IO.Path]::GetFullPath($candidate).TrimEnd('\')
        $descendantPrefix = $resolved + [System.IO.Path]::DirectorySeparatorChar
        if (
            [StringComparer]::OrdinalIgnoreCase.Equals($fullPath, $resolved) -or
            $fullPath.StartsWith($descendantPrefix, [StringComparison]::OrdinalIgnoreCase)
        ) {
            throw "Evidence parent '$fullPath' is a protected project path. Choose a dedicated output directory."
        }
    }
    $cursor = [System.IO.DirectoryInfo]::new($fullPath)
    while ($null -ne $cursor) {
        if (
            $cursor.Exists -and
            ($cursor.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0
        ) {
            throw "Evidence path traverses a junction/symlink and cannot be safely validated: $($cursor.FullName)"
        }
        if ($cursor.Exists) {
            foreach ($marker in @(
                "run-manifest.json",
                "route-manifest.json",
                "validation-summary.json",
                "phase0-evidence-summary.json"
            )) {
                if (Test-Path -LiteralPath (Join-Path $cursor.FullName $marker) -PathType Leaf) {
                    throw "Evidence path '$fullPath' is inside or selects retained evidence root '$($cursor.FullName)'. Choose a separate evidence parent."
                }
            }
        }
        $cursor = $cursor.Parent
    }
}

function Assert-SafeEvidenceRoot {
    param(
        [string]$Path,
        [string]$Repository,
        [switch]$AllowExistingRoot
    )
    Assert-SafeEvidenceParent -Path $Path -Repository $Repository
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        throw "Evidence root is a file, not a directory: $Path"
    }
    if ([System.IO.Directory]::Exists($Path)) {
        if (-not $AllowExistingRoot) {
            throw "Evidence root already exists: $Path. Use a new run name, or -AllowExistingRoot only for an empty directory."
        }
        $existingItem = Get-ChildItem -LiteralPath $Path -Force -ErrorAction Stop | Select-Object -First 1
        if ($null -ne $existingItem) {
            throw "Evidence root is not empty and will not be overwritten: $Path"
        }
    }
}

function Assert-SafeBuildPath {
    param([string]$Path, [string]$Repository)
    $fullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    if ($null -eq [System.IO.Directory]::GetParent($fullPath)) {
        throw "Build path cannot be a filesystem root: $fullPath"
    }
    $repo = [System.IO.Path]::GetFullPath($Repository).TrimEnd('\')
    $fullPrefix = $fullPath + [System.IO.Path]::DirectorySeparatorChar
    if (
        [StringComparer]::OrdinalIgnoreCase.Equals($fullPath, $repo) -or
        $repo.StartsWith($fullPrefix, [StringComparison]::OrdinalIgnoreCase)
    ) {
        throw "Build/install path cannot be the repository root or a parent of it: $fullPath"
    }
    foreach ($protectedName in @("build", "dist", ".git", "agent_docs", "src", "tests", "cmake")) {
        $protectedPath = [System.IO.Path]::GetFullPath(
            (Join-Path $Repository (Join-Path $protectedName "unused"))
        )
        $protectedRoot = [System.IO.Path]::GetDirectoryName($protectedPath)
        $protectedRoot = $protectedRoot.TrimEnd('\')
        $prefix = $protectedRoot + [System.IO.Path]::DirectorySeparatorChar
        if (
            [StringComparer]::OrdinalIgnoreCase.Equals($fullPath, $protectedRoot) -or
            $fullPath.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)
        ) {
            throw "Build/install paths may not write beneath the protected project directory '$protectedRoot': $fullPath"
        }
    }
    $cursor = [System.IO.DirectoryInfo]::new($fullPath)
    while ($null -ne $cursor) {
        if (
            $cursor.Exists -and
            ($cursor.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0
        ) {
            throw "Build/install path traverses a junction/symlink and cannot be safely validated: $($cursor.FullName)"
        }
        $cursor = $cursor.Parent
    }
}

function New-RouteDefinition {
    param([string]$Id, [string]$Category, [string]$Artifact, [string]$Scenario, [string]$Fixture = "packaged Release")
    return [pscustomobject][ordered]@{
        routeId = $Id
        category = $Category
        artifactPath = $Artifact
        scenario = $Scenario
        fixture = $Fixture
    }
}

$RouteDefinitions = @(
    (New-RouteDefinition "visual-empty-english-light" "visual" "visual/empty/english-light" "empty packaged startup, English, light" "empty workspace"),
    (New-RouteDefinition "visual-empty-english-dark" "visual" "visual/empty/english-dark" "empty packaged startup, English, dark" "empty workspace"),
    (New-RouteDefinition "visual-empty-korean-light" "visual" "visual/empty/korean-light" "empty packaged startup, Korean, light" "empty workspace"),
    (New-RouteDefinition "visual-empty-korean-dark" "visual" "visual/empty/korean-dark" "empty packaged startup, Korean, dark" "empty workspace"),
    (New-RouteDefinition "fixture-representative" "fixture" "fixtures/representative" "deterministic representative startup fixture" "representative_startup.sql"),
    (New-RouteDefinition "visual-representative" "visual" "visual/representative" "representative packaged startup language/theme matrix" "representative startup fixture"),
    (New-RouteDefinition "visual-classes" "visual" "visual/classes" "large Classes entry and selection language/theme states" "large_startup.sql"),
    (New-RouteDefinition "visual-sub-prep" "visual" "visual/sub-prep" "large Sub Prep populated, changed, read-only, and empty states" "large_startup.sql"),
    (New-RouteDefinition "visual-pdf-viewer" "visual" "visual/pdf-viewer" "PDF catalog, open, close, error, and reopen states" "large PDF viewer fixture"),
    (New-RouteDefinition "startup-empty" "memory" "startup/empty" "empty packaged Release startup metrics" "empty workspace"),
    (New-RouteDefinition "startup-representative" "memory" "startup/representative" "representative packaged Release startup metrics" "representative startup fixture"),
    (New-RouteDefinition "workflow-representative" "workflow" "workflow/representative" "representative packaged Release full navigation and PDF lifecycle" "representative startup fixture"),
    (New-RouteDefinition "lifecycle-sub-prep" "memory" "memory/sub-prep" "large Sub Prep refresh, leave, and repeated re-entry lifecycle" "large_startup.sql"),
    (New-RouteDefinition "lifecycle-classes" "memory" "memory/classes" "large Classes selection, refresh, leave, and repeated re-entry lifecycle" "large_startup.sql"),
    (New-RouteDefinition "lifecycle-schedule" "memory" "memory/schedule" "large Schedule refresh, leave, and repeated re-entry lifecycle" "large_startup.sql"),
    (New-RouteDefinition "lifecycle-schedule-import" "output" "memory/schedule-import" "large Schedule Import parse, conflict review, cancel, and release lifecycle" "large_startup.sql"),
    (New-RouteDefinition "lifecycle-schedule-import-apply" "output" "memory/schedule-import-apply" "large Schedule Import parse, review, apply, and release lifecycle" "large_startup.sql"),
    (New-RouteDefinition "lifecycle-calendar-import" "output" "memory/calendar-import" "large Calendar Import parse, apply, refresh, and release lifecycle" "large_startup.sql"),
    (New-RouteDefinition "lifecycle-calendar-import-error" "output" "memory/calendar-import-error" "large Calendar Import parser failure and release lifecycle" "large_startup.sql"),
    (New-RouteDefinition "lifecycle-class-transfer" "memory" "memory/class-transfer" "large Class Transfer review, apply, and release lifecycle" "large_startup.sql"),
    (New-RouteDefinition "lifecycle-speaking-evaluation" "output" "output/speaking-evaluation" "large Speaking Evaluation report, export, and release lifecycle" "large_startup.sql"),
    (New-RouteDefinition "lifecycle-staff-directory" "memory" "memory/staff-directory" "large Staff Directory refresh, re-entry, and release lifecycle" "large_startup.sql"),
    (New-RouteDefinition "output-sub-prep" "output" "output/sub-prep" "large Sub Prep generated PDF output and validation-error evidence" "large_startup.sql"),
    (New-RouteDefinition "resource-trace" "memory" "memory/resource-trace" "large packaged resource payload and lease lifecycle trace" "large resource-trace fixture")
)
$RouteById = @{}
foreach ($definition in $RouteDefinitions) {
    $RouteById[$definition.routeId] = $definition
}
$AllRouteIds = @($RouteDefinitions | ForEach-Object { $_.routeId })
$CategoryRouteIds = @{}
foreach ($category in @("visual", "workflow", "output", "memory", "fixture")) {
    $CategoryRouteIds[$category] = @(
        $RouteDefinitions |
            Where-Object { $_.category -eq $category } |
            ForEach-Object { $_.routeId }
    )
}

function Expand-RequestedRoutes {
    param([string[]]$RequestedValues)
    $expanded = New-Object System.Collections.Generic.List[string]
    foreach ($value in $RequestedValues) {
        foreach ($token in $value.Split(",")) {
            $trimmed = $token.Trim()
            if ([string]::IsNullOrWhiteSpace($trimmed)) {
                continue
            }
            if ($trimmed -eq "all") {
                foreach ($routeId in $AllRouteIds) {
                    if (-not $expanded.Contains($routeId)) { $expanded.Add($routeId) }
                }
            } elseif ($CategoryRouteIds.ContainsKey($trimmed)) {
                foreach ($routeId in $CategoryRouteIds[$trimmed]) {
                    if (-not $expanded.Contains($routeId)) { $expanded.Add($routeId) }
                }
            } elseif ($RouteById.ContainsKey($trimmed)) {
                if (-not $expanded.Contains($trimmed)) { $expanded.Add($trimmed) }
            } else {
                throw "Unknown route/category '$trimmed'. Use all, visual, workflow, output, memory, fixture, or a route ID."
            }
        }
    }
    if (
        ($expanded.Contains("startup-representative") -or $expanded.Contains("workflow-representative")) -and
        -not $expanded.Contains("fixture-representative")
    ) {
        $expanded.Add("fixture-representative")
    }
    $ordered = New-Object System.Collections.Generic.List[string]
    foreach ($routeId in $AllRouteIds) {
        if ($expanded.Contains($routeId)) { $ordered.Add($routeId) }
    }
    return @($ordered)
}

function Get-CleanChildEnvironment {
    param([string]$Application, [string]$TestBinary, [string]$QtBinaryDirectory)
    $environment = @{}
    foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) {
        $environment[[string]$entry.Key] = [string]$entry.Value
    }
    $removeNames = @(
        "CLASSMNGR_TEST_APP_PATH", "CLASSMNGR_SETTINGS_ROOT",
        "CLASSMNGR_VISUAL_BASELINE_OUTPUT_DIR", "CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH",
        "CLASSMNGR_STARTUP_RESOURCE_TRACE_PATH", "CLASSMNGR_STARTUP_PDF_CAPTURE_OUTPUT_DIR",
        "CLASSMNGR_STARTUP_CALENDAR_IMPORT_EXPECTED_FAILURE", "CLASSMNGR_STARTUP_CALENDAR_IMPORT_OUTPUT_DIR",
        "CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL", "CLASSMNGR_STARTUP_CLASS_TRANSFER_OUTPUT_DIR",
        "CLASSMNGR_STARTUP_CLASS_TRANSFER_PATH", "CLASSMNGR_STARTUP_CLASSES_VISUAL_OUTPUT_DIR",
        "CLASSMNGR_STARTUP_SCHEDULE_IMPORT_OUTPUT_DIR", "CLASSMNGR_STARTUP_SCHEDULE_IMPORT_PATH",
        "CLASSMNGR_STARTUP_SPEAKING_EVALUATION_OUTPUT_DIR", "CLASSMNGR_STARTUP_STAFF_DIRECTORY_OUTPUT_DIR",
        "CLASSMNGR_STARTUP_SUB_PREP_OUTPUT_TARGET_ROOT", "CLASSMNGR_STARTUP_PROCESS_MAX_MS",
        "CLASSMNGR_STARTUP_READY_MAX_MS", "CLASSMNGR_STARTUP_WINDOW_MAX_MS",
        "CLASSMNGR_STARTUP_FIXTURE_OUTPUT_PATH", "CLASSMNGR_LARGE_STARTUP_FIXTURE_OUTPUT_PATH",
        "CLASSMNGR_LEGACY_STARTUP_FIXTURE_OUTPUT_PATH", "CLASSMNGR_CLASS_TRANSFER_FIXTURE_OUTPUT_PATH",
        "CLASSMNGR_CONFLICT_REVIEW_OUTPUT_PATH", "CLASSMNGR_SCHEDULE_IMPORT_FIXTURE_OUTPUT_PATH",
        "CLASSMNGR_SCHEDULE_REVIEW_OUTPUT_PATH", "CLASSMNGR_SCHEDULE_CONFLICT_FIXTURE_OUTPUT_PATH",
        "CLASSMNGR_SCHEDULE_CONFLICT_REVIEW_OUTPUT_PATH", "CLASSMNGR_SUB_PREP_OUTPUT_REFERENCE_DIR",
        "CLASSMNGR_LARGE_SUB_PREP_BOUNDARY_REFERENCE_DIR", "CLASSMNGR_LARGE_CLASSES_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_CLASSES_VISUAL_REFERENCE_DIR", "CLASSMNGR_LARGE_SCHEDULE_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_SCHEDULE_IMPORT_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_SCHEDULE_IMPORT_APPLY_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_CALENDAR_IMPORT_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_CALENDAR_IMPORT_ERROR_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_CLASS_TRANSFER_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_SPEAKING_EVALUATION_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_STAFF_DIRECTORY_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_SUB_PREP_OUTPUT_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_SUB_PREP_VISUAL_REFERENCE_DIR",
        "CLASSMNGR_LARGE_PDF_VIEWER_VISUAL_REFERENCE_DIR",
        "CLASSMNGR_LARGE_RESOURCE_TRACE_REFERENCE_DIR"
    )
    foreach ($name in $removeNames) { $environment.Remove($name) }
    $pathParts = @(
        [System.IO.Path]::GetDirectoryName($Application),
        [System.IO.Path]::GetDirectoryName($TestBinary),
        $QtBinaryDirectory,
        $environment["PATH"]
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    $environment["PATH"] = $pathParts -join [System.IO.Path]::PathSeparator
    $environment["CLASSMNGR_TEST_APP_PATH"] = $Application
    $environment["QT_QPA_PLATFORM"] = "offscreen"
    return $environment
}

function Get-RecordedEnvironment {
    param([hashtable]$Environment)
    $recorded = [ordered]@{}
    foreach ($name in ($Environment.Keys | Sort-Object)) {
        if ($name -eq "PATH" -or $name -eq "QT_QPA_PLATFORM" -or $name.StartsWith("CLASSMNGR_")) {
            $recorded[$name] = [string]$Environment[$name]
        }
    }
    return $recorded
}

function Invoke-CapturedProcess {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [hashtable]$Environment,
        [string]$WorkingDirectory,
        [string]$LogDirectory,
        [int]$Timeout,
        [string]$Label
    )
    [System.IO.Directory]::CreateDirectory($LogDirectory) | Out-Null
    $stdoutPath = Join-Path $LogDirectory "runner-stdout.txt"
    $stderrPath = Join-Path $LogDirectory "runner-stderr.txt"
    $command = @($FilePath) + $Arguments
    $result = [ordered]@{
        label = $Label
        command = $command
        commandLine = Get-CommandDisplay -FilePath $FilePath -Arguments $Arguments
        workingDirectory = $WorkingDirectory
        environment = Get-RecordedEnvironment -Environment $Environment
        startedAtUtc = [DateTime]::UtcNow.ToString("o")
        stdoutPath = $stdoutPath
        stderrPath = $stderrPath
        processFinished = $false
        exitStatus = "not-started"
        exitCode = -1
        timedOut = $false
        durationSeconds = 0
    }
    Write-Host ("[{0}] {1}" -f $Label, $result.commandLine)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $process = $null
    try {
        $startInfo = [Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $FilePath
        $startInfo.Arguments = (($Arguments | ForEach-Object {
            ConvertTo-CommandLineArgument -Value ([string]$_)
        }) -join " ")
        $startInfo.WorkingDirectory = $WorkingDirectory
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $startInfo.EnvironmentVariables.Clear()
        foreach ($name in $Environment.Keys) {
            $startInfo.EnvironmentVariables[[string]$name] = [string]$Environment[$name]
        }
        $process = [Diagnostics.Process]::new()
        $process.StartInfo = $startInfo
        if (-not $process.Start()) { throw "Process.Start returned false." }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $finished = $process.WaitForExit($Timeout * 1000)
        if (-not $finished) {
            $result.timedOut = $true
            $terminationPath = Join-Path $LogDirectory "timeout-termination.txt"
            $taskkillPath = Join-Path $env:SystemRoot "System32/taskkill.exe"
            if (Test-Path -LiteralPath $taskkillPath -PathType Leaf) {
                $terminationOutput = @(& $taskkillPath /PID $process.Id /T /F 2>&1)
                $result.terminationExitCode = $LASTEXITCODE
                $result.terminationPath = $terminationPath
                [System.IO.File]::WriteAllText(
                    $terminationPath,
                    (($terminationOutput | ForEach-Object { [string]$_ }) -join [Environment]::NewLine),
                    [System.Text.UTF8Encoding]::new($false)
                )
            }
            if (-not $process.HasExited) {
                try { $process.Kill() } catch { $result.killError = $_.Exception.Message }
            }
            $process.WaitForExit()
        }
        [System.IO.File]::WriteAllText($stdoutPath, $stdoutTask.Result, [System.Text.UTF8Encoding]::new($false))
        [System.IO.File]::WriteAllText($stderrPath, $stderrTask.Result, [System.Text.UTF8Encoding]::new($false))
        $result.processFinished = $process.HasExited
        if ($result.processFinished) {
            $result.exitCode = $process.ExitCode
            $result.exitStatus = "normal"
        }
    } catch {
        $result.error = $_.Exception.Message
        [System.IO.File]::WriteAllText($stdoutPath, "", [System.Text.UTF8Encoding]::new($false))
        [System.IO.File]::WriteAllText($stderrPath, $_.Exception.ToString(), [System.Text.UTF8Encoding]::new($false))
    } finally {
        if ($null -ne $process) { $process.Dispose() }
    }
    $timer.Stop()
    $result.durationSeconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3)
    if ($result.timedOut) { $result.exitStatus = "timeout" }
    return [pscustomobject]$result
}

function Write-DeterministicSettings {
    param([string]$SettingsRoot)
    $settingsDirectory = Join-Path $SettingsRoot "PaperCloud"
    [System.IO.Directory]::CreateDirectory($settingsDirectory) | Out-Null
    $settingsPath = Join-Path $settingsDirectory "ClassMngr.ini"
    $contents = @"
[options]
theme=1
fontSize=2
language=1
saveMode=0
documentPageSpacing=2
documentViewerBackground=1
sidebarTooltipsEnabled=true
sidebarMarqueeEnabled=false

[updates]
automaticChecksEnabled=false
"@
    [System.IO.File]::WriteAllText($settingsPath, $contents + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    return $settingsPath
}

function Write-RunManifest {
    param([string]$Path, [object]$Manifest)
    Write-JsonFile -Path $Path -Value $Manifest
}

function Add-RouteRecord {
    param(
        [object]$Definition,
        [string]$RunRoot,
        [string]$Application,
        [string]$TestBinary,
        [string]$QtBinaryDirectory,
        [string]$FilePath,
        [string[]]$Arguments,
        [hashtable]$EnvironmentOverrides = @{},
        [System.Collections.Generic.List[object]]$CommandList,
        [int]$Timeout,
        [switch]$IsPlan
    )
    $routeRoot = Join-Path $RunRoot ($Definition.artifactPath -replace '/', '\')
    if (-not $IsPlan) {
        [System.IO.Directory]::CreateDirectory($routeRoot) | Out-Null
    }
    $environment = Get-CleanChildEnvironment -Application $Application -TestBinary $TestBinary -QtBinaryDirectory $QtBinaryDirectory
    foreach ($name in $EnvironmentOverrides.Keys) {
        $environment[$name] = [string]$EnvironmentOverrides[$name]
    }
    if ($IsPlan) {
        $invocation = [ordered]@{
            label = $Definition.routeId
            command = @($FilePath) + $Arguments
            commandLine = Get-CommandDisplay -FilePath $FilePath -Arguments $Arguments
            workingDirectory = $RepositoryRoot
            environment = Get-RecordedEnvironment -Environment $environment
            processFinished = $false
            exitStatus = "planned"
            exitCode = $null
            timedOut = $false
        }
        $status = "planned"
    } else {
        $invokeParameters = @{
            FilePath = $FilePath
            Arguments = $Arguments
            Environment = $environment
            WorkingDirectory = $RepositoryRoot
            LogDirectory = (Join-Path $routeRoot "runner")
            Timeout = $Timeout
            Label = $Definition.routeId
        }
        $invocation = Invoke-CapturedProcess @invokeParameters
        $status = if (
            $invocation.processFinished -and
            $invocation.exitStatus -eq "normal" -and
            $invocation.exitCode -eq 0 -and
            -not $invocation.timedOut
        ) { "completed" } else { "failed" }
    }
    $routeManifest = [ordered]@{
        schema = $RouteSchema
        taskId = $TaskId
        routeId = $Definition.routeId
        category = $Definition.category
        scenario = $Definition.scenario
        fixture = $Definition.fixture
        artifactPath = $Definition.artifactPath
        status = $status
        invocation = $invocation
    }
    if (-not $IsPlan) {
        Write-JsonFile -Path (Join-Path $routeRoot "route-manifest.json") -Value $routeManifest
    }
    if ($null -ne $CommandList) {
        $commandRecord = [ordered]@{
            id = $Definition.routeId
            kind = "route"
            command = $invocation.command
            commandLine = $invocation.commandLine
            workingDirectory = $invocation.workingDirectory
            environment = $invocation.environment
            status = $status
        }
        foreach ($propertyName in @(
            "processFinished", "exitStatus", "exitCode", "timedOut",
            "stdoutPath", "stderrPath", "durationSeconds",
            "terminationPath", "terminationExitCode", "killError"
        )) {
            $property = $invocation.PSObject.Properties[$propertyName]
            if ($null -ne $property) {
                $commandRecord[$propertyName] = $property.Value
            } elseif ($invocation -is [System.Collections.IDictionary] -and $invocation.Contains($propertyName)) {
                $commandRecord[$propertyName] = $invocation[$propertyName]
            }
        }
        $CommandList.Add($commandRecord)
    }
    return [pscustomobject]$routeManifest
}

function Add-BuildCommandRecord {
    param(
        [System.Collections.Generic.List[object]]$CommandList,
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$Label,
        [string]$RunRoot,
        [hashtable]$Environment,
        [int]$Timeout,
        [switch]$IsPlan
    )
    if ($IsPlan) {
        $CommandList.Add([ordered]@{
            id = $Label
            kind = "external"
            command = @($FilePath) + $Arguments
            commandLine = Get-CommandDisplay -FilePath $FilePath -Arguments $Arguments
            status = "planned"
        })
        return $null
    }
    $invokeParameters = @{
        FilePath = $FilePath
        Arguments = $Arguments
        Environment = $Environment
        WorkingDirectory = $RepositoryRoot
        LogDirectory = (Join-Path $RunRoot (Join-Path "runner" $Label))
        Timeout = $Timeout
        Label = $Label
    }
    $result = Invoke-CapturedProcess @invokeParameters
    $ok = $result.processFinished -and $result.exitCode -eq 0 -and -not $result.timedOut
    $CommandList.Add([ordered]@{
        id = $Label
        kind = "external"
        command = $result.command
        commandLine = $result.commandLine
        workingDirectory = $result.workingDirectory
        environment = $result.environment
        status = if ($ok) { "completed" } else { "failed" }
        processFinished = $result.processFinished
        exitStatus = $result.exitStatus
        exitCode = $result.exitCode
        timedOut = $result.timedOut
        stdoutPath = $result.stdoutPath
        stderrPath = $result.stderrPath
        durationSeconds = $result.durationSeconds
    })
    if (-not $ok) {
        throw "$Label failed with exit code $($result.exitCode). See $($result.stderrPath)."
    }
    return $result
}

function Get-CMakeConfigureArguments {
    param(
        [string]$BuildDirectory,
        [string]$QtDirectory,
        [string]$BuildType,
        [string]$BuildTesting,
        [string]$InstallPrefix
    )
    $arguments = @(
        "-S", $RepositoryRoot,
        "-B", $BuildDirectory,
        "-G", "Ninja",
        "-DQt6_DIR=$([System.IO.Path]::Combine($QtDirectory, 'lib', 'cmake', 'Qt6'))",
        "-DBUILD_TESTING=$BuildTesting",
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DCLASSMNGR_UPDATE_CHECK_ON_STARTUP=OFF",
        "-DCLASSMNGR_RESOURCE_PACK_CHECK_ON_STARTUP=OFF"
    )
    if (-not [string]::IsNullOrWhiteSpace($InstallPrefix)) {
        $arguments += "-DCMAKE_INSTALL_PREFIX=$InstallPrefix"
    }
    return $arguments
}

$ExplicitReleaseBuildDirectory = -not [string]::IsNullOrWhiteSpace($ReleaseBuildDirectory)
$ExplicitTestBuildDirectory = -not [string]::IsNullOrWhiteSpace($TestBuildDirectory)
$ExplicitInstallDirectory = -not [string]::IsNullOrWhiteSpace($InstallDirectory)
$ExplicitApplicationPath = -not [string]::IsNullOrWhiteSpace($ApplicationPath)
$ExplicitTestExecutable = -not [string]::IsNullOrWhiteSpace($TestExecutable)
$RequestedRouteIds = Expand-RequestedRoutes -RequestedValues $Routes
$HostPlatform = Get-HostPlatformName
$HostSupported = Test-IsWindowsX64
$EffectivePlan = [bool]$Plan
if ([string]::IsNullOrWhiteSpace($EvidenceParent) -and -not $Plan) {
    $EffectivePlan = $true
    Write-Host "No -EvidenceParent supplied: planning only; execution requires an explicit evidence parent."
}

$ExplicitRunDirectoryName = -not [string]::IsNullOrWhiteSpace($RunDirectoryName)
$RunName = $RunDirectoryName
if ([string]::IsNullOrWhiteSpace($RunName)) {
    $RunName = "phase0-" + [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssfffZ")
}
if ($RunName -notmatch '^[A-Za-z0-9._-]+$') {
    throw "RunDirectoryName must contain only letters, numbers, '.', '_' and '-'."
}
if ($RunName -in @('.', '..')) {
    throw "RunDirectoryName cannot be '.' or '..'."
}
if ([string]::IsNullOrWhiteSpace($EvidenceParent)) {
    $EvidenceParent = Join-Path ([System.IO.Path]::GetTempPath()) "ClassMngr-Phase0-Evidence"
} else {
    $EvidenceParent = Get-FullPath -Path $EvidenceParent -BasePath $RepositoryRoot
}
$EvidenceParent = [System.IO.Path]::GetFullPath($EvidenceParent)
Assert-SafeEvidenceParent -Path $EvidenceParent -Repository $RepositoryRoot

$RunRoot = [System.IO.Path]::GetFullPath((Join-Path $EvidenceParent $RunName))
if (Test-Path -LiteralPath $RunRoot -PathType Leaf) {
    throw "Run path exists as a file, not a directory: $RunRoot"
}
if (
    [System.IO.Directory]::Exists($RunRoot) -and
    ([System.IO.DirectoryInfo]::new($RunRoot).Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0
) {
    throw "Run directory cannot be a junction/symlink: $RunRoot"
}
if (-not $ExplicitRunDirectoryName) {
    $baseRunName = $RunName
    $suffix = 1
    while ([System.IO.Directory]::Exists($RunRoot) -or [System.IO.File]::Exists($RunRoot)) {
        $RunName = "{0}-{1:D2}" -f $baseRunName, $suffix
        $RunRoot = Join-Path $EvidenceParent $RunName
        $suffix++
    }
}
Assert-SafeEvidenceRoot -Path $RunRoot -Repository $RepositoryRoot -AllowExistingRoot:$AllowExistingRoot

if ([string]::IsNullOrWhiteSpace($ReleaseBuildDirectory)) {
    $ReleaseBuildDirectory = Join-Path $RunRoot "_build/release"
}
if ([string]::IsNullOrWhiteSpace($TestBuildDirectory)) {
    $TestBuildDirectory = Join-Path $RunRoot "_build/test"
}
if ([string]::IsNullOrWhiteSpace($InstallDirectory)) {
    $InstallDirectory = Join-Path $RunRoot "package"
}
if ([string]::IsNullOrWhiteSpace($ApplicationPath)) {
    $ApplicationPath = Join-Path $InstallDirectory "ClassMngr.exe"
}
if ([string]::IsNullOrWhiteSpace($TestExecutable)) {
    $TestExecutable = Join-Path $TestBuildDirectory "ClassMngrStartupPerformanceTests.exe"
}
if ([string]::IsNullOrWhiteSpace($QtBinPath)) {
    $QtBinPath = Join-Path $QtPrefix "bin"
}

$ReleaseBuildDirectory = Get-FullPath -Path $ReleaseBuildDirectory -BasePath $RepositoryRoot
$TestBuildDirectory = Get-FullPath -Path $TestBuildDirectory -BasePath $RepositoryRoot
$InstallDirectory = Get-FullPath -Path $InstallDirectory -BasePath $RepositoryRoot
$ApplicationPath = Get-FullPath -Path $ApplicationPath -BasePath $RepositoryRoot
$TestExecutable = Get-FullPath -Path $TestExecutable -BasePath $RepositoryRoot
$QtPrefix = Get-FullPath -Path $QtPrefix -BasePath $RepositoryRoot
$QtBinPath = Get-FullPath -Path $QtBinPath -BasePath $RepositoryRoot
if ($Parallel -lt 1) { throw "Parallel must be at least 1." }
if ($TimeoutSeconds -lt 1) { throw "TimeoutSeconds must be at least 1." }
if ($SkipBuild -and -not $EffectivePlan -and -not $SkipRun -and (-not $ExplicitApplicationPath -or -not $ExplicitTestExecutable)) {
    throw "-SkipBuild requires explicit -ApplicationPath and -TestExecutable values; repository build/dist paths are never selected implicitly."
}
if (-not $EffectivePlan) {
    [System.IO.Directory]::CreateDirectory($RunRoot) | Out-Null
}

$CommandRecords = New-Object System.Collections.Generic.List[object]
$RouteRecords = New-Object System.Collections.Generic.List[object]
$RunFailures = New-Object System.Collections.Generic.List[object]
$RunWarnings = New-Object System.Collections.Generic.List[object]
$RunManifestPath = Join-Path $RunRoot "run-manifest.json"
$RunManifest = [ordered]@{
    schema = $RunSchema
    taskId = $TaskId
    runnerTaskId = $RunnerTaskId
    recordedAtUtc = [DateTime]::UtcNow.ToString("o")
    status = if ($EffectivePlan) { "planned" } else { "running" }
    scenario = "Phase 0 packaged Release evidence automation"
    fixture = "packaged Release Windows x64"
    supportedPlatform = "windows-x64"
    supportedPlatforms = $SupportedPlatforms
    deferredPlatforms = $DeferredPlatforms
    platformCoverage = @(
        [ordered]@{
            name = "windows-x64"
            supported = $true
            status = if ($HostSupported) { "runnable-on-this-host" } else { "not-runnable-on-this-host" }
        },
        [ordered]@{
            name = "macos-universal"
            supported = $true
            status = "supported-but-not-run-on-this-host"
        }
    )
    platform = [ordered]@{
        name = $HostPlatform
        supported = $HostSupported
        acceptanceScope = "Windows x64 is runnable here; macOS universal remains supported and is not run by this Windows-hosted runner."
    }
    requestedRoutes = $RequestedRouteIds
    paths = [ordered]@{
        repositoryRoot = $RepositoryRoot
        applicationPath = $ApplicationPath
        testExecutable = $TestExecutable
        releaseBuildDirectory = $ReleaseBuildDirectory
        testBuildDirectory = $TestBuildDirectory
        installDirectory = $InstallDirectory
        qtPrefix = $QtPrefix
        qtBinPath = $QtBinPath
        evidenceRoot = $RunRoot
    }
    options = [ordered]@{
        plan = $EffectivePlan
        skipBuild = [bool]$SkipBuild
        skipRun = [bool]$SkipRun
        skipValidation = [bool]$SkipValidation
        allowExistingRoot = [bool]$AllowExistingRoot
        parallel = $Parallel
            timeoutSeconds = $TimeoutSeconds
            routes = $Routes
    }
    requiredOptInEnvironmentVariables = @(
        "QT_QPA_PLATFORM", "CLASSMNGR_TEST_APP_PATH", "CLASSMNGR_SETTINGS_ROOT",
        "CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH", "CLASSMNGR_STARTUP_PDF_CAPTURE_OUTPUT_DIR",
        "CLASSMNGR_VISUAL_BASELINE_OUTPUT_DIR", "CLASSMNGR_LARGE_CLASSES_VISUAL_REFERENCE_DIR",
        "CLASSMNGR_LARGE_SUB_PREP_VISUAL_REFERENCE_DIR", "CLASSMNGR_LARGE_PDF_VIEWER_VISUAL_REFERENCE_DIR",
        "CLASSMNGR_LARGE_SUB_PREP_BOUNDARY_REFERENCE_DIR", "CLASSMNGR_LARGE_CLASSES_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_SCHEDULE_BOUNDARY_REFERENCE_DIR", "CLASSMNGR_LARGE_SCHEDULE_IMPORT_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_SCHEDULE_IMPORT_APPLY_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_CALENDAR_IMPORT_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_CALENDAR_IMPORT_ERROR_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_CLASS_TRANSFER_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_SPEAKING_EVALUATION_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_STAFF_DIRECTORY_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_LARGE_SUB_PREP_OUTPUT_BOUNDARY_REFERENCE_DIR",
        "CLASSMNGR_STARTUP_RESOURCE_TRACE_PATH"
    )
    memoryContract = [ordered]@{
        legacyResidentTargetBytes = $LegacyResidentTargetBytes
        legacyResidentTargetMiB = 250
        temporaryDiagnosticCeilingBytes = $TemporaryDiagnosticCeilingBytes
        temporaryDiagnosticCeilingMiB = 512
        legacy250MiBIsPhase0Failure = $false
        temporary512MiBIsPhase0Failure = $false
    }
    commands = $CommandRecords
    routes = $RouteRecords
    failures = $RunFailures
    warnings = $RunWarnings
}
if (-not $EffectivePlan) {
    Write-RunManifest -Path $RunManifestPath -Manifest $RunManifest
}

try {
    if (-not $HostSupported) {
        $RunManifest.status = "deferred"
        $RunWarnings.Add([ordered]@{
            code = "deferred-unofficial-platform"
            message = "Host '$HostPlatform' is outside the packaged Windows x64 Phase 0 acceptance scope; requested routes were not executed."
            platforms = $DeferredPlatforms
        })
        $RunManifest.warnings = $RunWarnings
        if (-not $EffectivePlan) {
            Write-RunManifest -Path $RunManifestPath -Manifest $RunManifest
        }
        Write-Host "Phase 0 evidence execution deferred on $HostPlatform. Unofficial ports are metadata, not failures."
        exit 0
    }

    if (-not $SkipBuild) {
        Assert-SafeBuildPath -Path $ReleaseBuildDirectory -Repository $RepositoryRoot
        Assert-SafeBuildPath -Path $TestBuildDirectory -Repository $RepositoryRoot
        Assert-SafeBuildPath -Path $InstallDirectory -Repository $RepositoryRoot
    }
    if (-not $SkipBuild) {
        foreach ($target in @($ReleaseBuildDirectory, $TestBuildDirectory, $InstallDirectory)) {
            if (Test-Path -LiteralPath $target) {
                throw "Build/install target already exists and will not be overwritten: $target. Use a new run root or -SkipBuild with explicit executables."
            }
        }
    }

    $BaseEnvironment = Get-CleanChildEnvironment -Application $ApplicationPath -TestBinary $TestExecutable -QtBinaryDirectory $QtBinPath
    if (-not $SkipBuild) {
        $qtConfigDirectory = Join-Path $QtPrefix "lib/cmake/Qt6"
        if (-not (Test-Path -LiteralPath $qtConfigDirectory -PathType Container)) {
            throw "Qt prefix does not contain lib/cmake/Qt6: $QtPrefix"
        }
        $releaseConfigure = Get-CMakeConfigureArguments -BuildDirectory $ReleaseBuildDirectory -QtDirectory $QtPrefix -BuildType "Release" -BuildTesting "OFF" -InstallPrefix $InstallDirectory
        $testConfigure = Get-CMakeConfigureArguments -BuildDirectory $TestBuildDirectory -QtDirectory $QtPrefix -BuildType "Debug" -BuildTesting "ON" -InstallPrefix ""
        $cmake = "cmake"
        $buildStep = @{
            CommandList = $CommandRecords
            FilePath = $cmake
            Arguments = $releaseConfigure
            Label = "configure-release"
            RunRoot = $RunRoot
            Environment = $BaseEnvironment
            Timeout = $TimeoutSeconds
            IsPlan = [bool]$EffectivePlan
        }
        [void](Add-BuildCommandRecord @buildStep)
        $buildStep.Arguments = @("--build", $ReleaseBuildDirectory, "--config", "Release", "--parallel", "$Parallel")
        $buildStep.Label = "build-release"
        [void](Add-BuildCommandRecord @buildStep)
        $buildStep.Arguments = @("--install", $ReleaseBuildDirectory, "--config", "Release")
        $buildStep.Label = "install-release"
        [void](Add-BuildCommandRecord @buildStep)
        $buildStep.Arguments = $testConfigure
        $buildStep.Label = "configure-test-harness"
        [void](Add-BuildCommandRecord @buildStep)
        $buildStep.Arguments = @("--build", $TestBuildDirectory, "--config", "Debug", "--target", "ClassMngrStartupPerformanceTests", "--parallel", "$Parallel")
        $buildStep.Label = "build-test-harness"
        [void](Add-BuildCommandRecord @buildStep)
    } else {
        Write-Host "Skipping build; using the explicit packaged application and test executable paths."
    }

    if (-not $EffectivePlan) {
        if ($SkipBuild) {
            if (-not (Test-Path -LiteralPath $ApplicationPath -PathType Leaf)) {
                throw "Packaged Release executable does not exist at the explicit path: $ApplicationPath"
            }
            if (-not (Test-Path -LiteralPath $TestExecutable -PathType Leaf)) {
                throw "Startup performance test executable does not exist at the explicit path: $TestExecutable"
            }
        } else {
            $builtApplication = Join-Path $InstallDirectory "ClassMngr.exe"
            $builtTest = Join-Path $TestBuildDirectory "ClassMngrStartupPerformanceTests.exe"
            if (-not $ExplicitApplicationPath) { $ApplicationPath = $builtApplication }
            if (-not $ExplicitTestExecutable) { $TestExecutable = $builtTest }
            if (-not (Test-Path -LiteralPath $ApplicationPath -PathType Leaf)) {
                throw "Release executable was not produced at the explicit path: $ApplicationPath"
            }
            if (-not (Test-Path -LiteralPath $TestExecutable -PathType Leaf)) {
                throw "Startup performance test executable was not produced at the explicit path: $TestExecutable"
            }
            $BaseEnvironment = Get-CleanChildEnvironment -Application $ApplicationPath -TestBinary $TestExecutable -QtBinaryDirectory $QtBinPath
            $RunManifest.paths.applicationPath = $ApplicationPath
            $RunManifest.paths.testExecutable = $TestExecutable
        }
    }

    if ($SkipRun -and -not $EffectivePlan) {
        foreach ($routeId in $RequestedRouteIds) {
            $definition = $RouteById[$routeId]
            $RouteRecords.Add([ordered]@{
                schema = $RouteSchema
                taskId = $TaskId
                routeId = $definition.routeId
                category = $definition.category
                scenario = $definition.scenario
                fixture = $definition.fixture
                artifactPath = $definition.artifactPath
                status = "skipped"
            })
        }
        $RunManifest.status = "skipped"
        $RunManifest.routeExecution = [ordered]@{
            status = "skipped"
            reason = "-SkipRun"
            requestedRouteCount = $RequestedRouteIds.Count
            executedRouteIds = @()
        }
        $RunWarnings.Add([ordered]@{
            code = "route-execution-skipped"
            message = "Requested evidence routes were not executed because -SkipRun was supplied. Build commands may still have run unless -SkipBuild was also supplied."
        })
        $RunManifest.routes = $RouteRecords
        $RunManifest.commands = $CommandRecords
        $RunManifest.warnings = $RunWarnings
        Write-RunManifest -Path $RunManifestPath -Manifest $RunManifest
        Write-Host "Run skipped by request; no evidence routes were executed."
        exit 0
    }

    $fixturePath = Join-Path $RunRoot "fixtures/representative/representative-startup.tps"
    $representativeSettings = Join-Path $RunRoot "runtime/representative-settings"
    if ($RequestedRouteIds -contains "fixture-representative") {
        $definition = $RouteById["fixture-representative"]
        $fixtureEnvironment = @{ CLASSMNGR_STARTUP_FIXTURE_OUTPUT_PATH = $fixturePath }
        $routeParameters = @{
            Definition = $definition
            RunRoot = $RunRoot
            Application = $ApplicationPath
            TestBinary = $TestExecutable
            QtBinaryDirectory = $QtBinPath
            FilePath = $TestExecutable
            Arguments = @("representativeStartupFixtureIsCompleteAndDeterministic")
            EnvironmentOverrides = $fixtureEnvironment
            CommandList = $CommandRecords
            Timeout = $TimeoutSeconds
            IsPlan = [bool]$EffectivePlan
        }
        $RouteRecords.Add((Add-RouteRecord @routeParameters))
    }
    if (
        ($RequestedRouteIds -contains "startup-representative" -or $RequestedRouteIds -contains "workflow-representative") -and
        -not $EffectivePlan
    ) {
        [void](Write-DeterministicSettings -SettingsRoot $representativeSettings)
    }

    foreach ($routeId in $RequestedRouteIds) {
        if ($routeId -eq "fixture-representative") { continue }
        $definition = $RouteById[$routeId]
        $routeRoot = Join-Path $RunRoot ($definition.artifactPath -replace '/', '\')
        $arguments = @()
        $environmentOverrides = @{}
        $filePath = $TestExecutable

        switch -Exact ($routeId) {
            "visual-empty-english-light" {
                $language = "english"; $theme = "light"
                $settingsRoot = Join-Path $RunRoot "runtime/settings-english-light"
                if (-not $EffectivePlan) { [void](Write-DeterministicSettings -SettingsRoot $settingsRoot) }
                $environmentOverrides["CLASSMNGR_SETTINGS_ROOT"] = $settingsRoot
                $arguments = @("--startup-performance-test", "--startup-performance-scenario", "minimal", "--startup-performance-settle-ms", "1000", "--startup-performance-output", (Join-Path $routeRoot "startup-metrics.json"), "--startup-visual-capture-output", $routeRoot, "--startup-visual-capture-language", $language, "--startup-visual-capture-theme", $theme)
                $filePath = $ApplicationPath
                break
            }
            "visual-empty-english-dark" {
                $language = "english"; $theme = "dark"
                $settingsRoot = Join-Path $RunRoot "runtime/settings-english-dark"
                if (-not $EffectivePlan) { [void](Write-DeterministicSettings -SettingsRoot $settingsRoot) }
                $environmentOverrides["CLASSMNGR_SETTINGS_ROOT"] = $settingsRoot
                $arguments = @("--startup-performance-test", "--startup-performance-scenario", "minimal", "--startup-performance-settle-ms", "1000", "--startup-performance-output", (Join-Path $routeRoot "startup-metrics.json"), "--startup-visual-capture-output", $routeRoot, "--startup-visual-capture-language", $language, "--startup-visual-capture-theme", $theme)
                $filePath = $ApplicationPath
                break
            }
            "visual-empty-korean-light" {
                $language = "korean"; $theme = "light"
                $settingsRoot = Join-Path $RunRoot "runtime/settings-korean-light"
                if (-not $EffectivePlan) { [void](Write-DeterministicSettings -SettingsRoot $settingsRoot) }
                $environmentOverrides["CLASSMNGR_SETTINGS_ROOT"] = $settingsRoot
                $arguments = @("--startup-performance-test", "--startup-performance-scenario", "minimal", "--startup-performance-settle-ms", "1000", "--startup-performance-output", (Join-Path $routeRoot "startup-metrics.json"), "--startup-visual-capture-output", $routeRoot, "--startup-visual-capture-language", $language, "--startup-visual-capture-theme", $theme)
                $filePath = $ApplicationPath
                break
            }
            "visual-empty-korean-dark" {
                $language = "korean"; $theme = "dark"
                $settingsRoot = Join-Path $RunRoot "runtime/settings-korean-dark"
                if (-not $EffectivePlan) { [void](Write-DeterministicSettings -SettingsRoot $settingsRoot) }
                $environmentOverrides["CLASSMNGR_SETTINGS_ROOT"] = $settingsRoot
                $arguments = @("--startup-performance-test", "--startup-performance-scenario", "minimal", "--startup-performance-settle-ms", "1000", "--startup-performance-output", (Join-Path $routeRoot "startup-metrics.json"), "--startup-visual-capture-output", $routeRoot, "--startup-visual-capture-language", $language, "--startup-visual-capture-theme", $theme)
                $filePath = $ApplicationPath
                break
            }
            "visual-representative" {
                $environmentOverrides["CLASSMNGR_VISUAL_BASELINE_OUTPUT_DIR"] = $routeRoot
                $arguments = @("capturesRepresentativeVisualVariants")
                break
            }
            "visual-classes" {
                $environmentOverrides["CLASSMNGR_LARGE_CLASSES_VISUAL_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeClassesVisualStatesWhenConfigured")
                break
            }
            "visual-sub-prep" {
                $environmentOverrides["CLASSMNGR_LARGE_SUB_PREP_VISUAL_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeSubPrepVisualStatesWhenConfigured")
                break
            }
            "visual-pdf-viewer" {
                $environmentOverrides["CLASSMNGR_LARGE_PDF_VIEWER_VISUAL_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargePdfViewerVisualStatesWhenConfigured")
                break
            }
            "startup-empty" {
                $settingsRoot = Join-Path $RunRoot "runtime/settings-empty"
                if (-not $EffectivePlan) { [void](Write-DeterministicSettings -SettingsRoot $settingsRoot) }
                $environmentOverrides["CLASSMNGR_SETTINGS_ROOT"] = $settingsRoot
                $arguments = @("--startup-performance-test", "--startup-performance-scenario", "minimal", "--startup-performance-settle-ms", "1000", "--startup-performance-output", (Join-Path $routeRoot "startup-metrics.json"))
                $filePath = $ApplicationPath
                break
            }
            "startup-representative" {
                $environmentOverrides["CLASSMNGR_SETTINGS_ROOT"] = $representativeSettings
                $arguments = @("--startup-performance-test", "--startup-performance-scenario", "representative", "--startup-performance-settle-ms", "5000", "--startup-performance-output", (Join-Path $routeRoot "startup-metrics.json"), "--startup-visual-capture-output", $routeRoot, "--startup-visual-capture-language", "english", "--startup-visual-capture-theme", "light", $fixturePath)
                $filePath = $ApplicationPath
                break
            }
            "workflow-representative" {
                $environmentOverrides["CLASSMNGR_SETTINGS_ROOT"] = $representativeSettings
                $environmentOverrides["CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"] = Join-Path $routeRoot "workflow-trace.txt"
                $environmentOverrides["CLASSMNGR_STARTUP_PDF_CAPTURE_OUTPUT_DIR"] = Join-Path $routeRoot "pdf-captures"
                $arguments = @("--startup-performance-test", "--startup-performance-workflow", "--startup-performance-scenario", "representative", "--startup-performance-settle-ms", "1000", "--startup-performance-output", (Join-Path $routeRoot "startup-metrics.json"), "--startup-visual-capture-output", (Join-Path $routeRoot "captures"), "--startup-visual-capture-language", "english", "--startup-visual-capture-theme", "light", $fixturePath)
                $filePath = $ApplicationPath
                break
            }
            "lifecycle-sub-prep" {
                $environmentOverrides["CLASSMNGR_LARGE_SUB_PREP_BOUNDARY_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeSubPrepBoundaryWhenConfigured")
                break
            }
            "lifecycle-classes" {
                $environmentOverrides["CLASSMNGR_LARGE_CLASSES_BOUNDARY_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeClassesBoundaryWhenConfigured")
                break
            }
            "lifecycle-schedule" {
                $environmentOverrides["CLASSMNGR_LARGE_SCHEDULE_BOUNDARY_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeScheduleBoundaryWhenConfigured")
                break
            }
            "lifecycle-schedule-import" {
                $environmentOverrides["CLASSMNGR_LARGE_SCHEDULE_IMPORT_BOUNDARY_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeScheduleImportBoundaryWhenConfigured")
                break
            }
            "lifecycle-schedule-import-apply" {
                $environmentOverrides["CLASSMNGR_LARGE_SCHEDULE_IMPORT_APPLY_BOUNDARY_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeScheduleImportApplyBoundaryWhenConfigured")
                break
            }
            "lifecycle-calendar-import" {
                $environmentOverrides["CLASSMNGR_LARGE_CALENDAR_IMPORT_BOUNDARY_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeCalendarImportBoundaryWhenConfigured")
                break
            }
            "lifecycle-calendar-import-error" {
                $environmentOverrides["CLASSMNGR_LARGE_CALENDAR_IMPORT_ERROR_BOUNDARY_REFERENCE_DIR"] = $routeRoot
                $environmentOverrides["CLASSMNGR_STARTUP_CALENDAR_IMPORT_EXPECTED_FAILURE"] = "1"
                $arguments = @("capturesLargeCalendarImportBoundaryWhenConfigured")
                break
            }
            "lifecycle-class-transfer" {
                $environmentOverrides["CLASSMNGR_LARGE_CLASS_TRANSFER_BOUNDARY_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeClassTransferBoundaryWhenConfigured")
                break
            }
            "lifecycle-speaking-evaluation" {
                $environmentOverrides["CLASSMNGR_LARGE_SPEAKING_EVALUATION_BOUNDARY_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeSpeakingEvaluationBoundaryWhenConfigured")
                break
            }
            "lifecycle-staff-directory" {
                $environmentOverrides["CLASSMNGR_LARGE_STAFF_DIRECTORY_BOUNDARY_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeStaffDirectoryBoundaryWhenConfigured")
                break
            }
            "output-sub-prep" {
                $environmentOverrides["CLASSMNGR_LARGE_SUB_PREP_OUTPUT_BOUNDARY_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeSubPrepOutputBoundaryWhenConfigured")
                break
            }
            "resource-trace" {
                $environmentOverrides["CLASSMNGR_LARGE_RESOURCE_TRACE_REFERENCE_DIR"] = $routeRoot
                $arguments = @("capturesLargeResourceTraceWhenConfigured")
                break
            }
            default {
                throw "No runner command mapping exists for route $routeId."
            }
        }

        $routeParameters = @{
            Definition = $definition
            RunRoot = $RunRoot
            Application = $ApplicationPath
            TestBinary = $TestExecutable
            QtBinaryDirectory = $QtBinPath
            FilePath = $filePath
            Arguments = $arguments
            EnvironmentOverrides = $environmentOverrides
            CommandList = $CommandRecords
            Timeout = $TimeoutSeconds
            IsPlan = [bool]$EffectivePlan
        }
        $RouteRecords.Add((Add-RouteRecord @routeParameters))
    }

    if ($EffectivePlan) {
        Write-Host "Phase 0 packaged Release plan"
        Write-Host "  Host: $HostPlatform (supported=$HostSupported)"
        Write-Host "  Application: $ApplicationPath"
        Write-Host "  Test harness: $TestExecutable"
        Write-Host "  Evidence run directory: $RunRoot"
        Write-Host ("  Routes: " + ($RequestedRouteIds -join ", "))
        foreach ($record in $RouteRecords) {
            Write-Host ("  {0}: {1}" -f $record.routeId, $record.invocation.commandLine)
        }
        Write-Host "Plan complete. No files or processes were changed."
        exit 0
    }

    $RunManifest.status = "completed"
    $RunManifest.recordedAtUtc = [DateTime]::UtcNow.ToString("o")
    $RunManifest.routes = $RouteRecords
    $RunManifest.commands = $CommandRecords
    $RunManifest.failures = $RunFailures
    $RunManifest.warnings = $RunWarnings
    Write-RunManifest -Path $RunManifestPath -Manifest $RunManifest

    $validationResult = $null
    if (-not $SkipValidation) {
        $validatorPath = Join-Path $PSScriptRoot "validate_phase0_evidence.py"
        $summaryPath = Join-Path $RunRoot "validation-summary.json"
        $validationArguments = @(
            $validatorPath,
            "--evidence-root", $RunRoot,
            "--summary-output", $summaryPath
        )
        $validationParameters = @{
            FilePath = $PythonCommand
            Arguments = $validationArguments
            Environment = $BaseEnvironment
            WorkingDirectory = $RepositoryRoot
            LogDirectory = Join-Path $RunRoot "runner/validation"
            Timeout = $TimeoutSeconds
            Label = "validate-evidence"
        }
        $validationResult = Invoke-CapturedProcess @validationParameters
        $CommandRecords.Add([ordered]@{
            id = "validate-evidence"
            kind = "validator"
            command = $validationResult.command
            commandLine = $validationResult.commandLine
            workingDirectory = $validationResult.workingDirectory
            environment = $validationResult.environment
            status = if ($validationResult.processFinished -and $validationResult.exitCode -eq 0 -and -not $validationResult.timedOut) { "completed" } else { "failed" }
            processFinished = $validationResult.processFinished
            exitStatus = $validationResult.exitStatus
            exitCode = $validationResult.exitCode
            timedOut = $validationResult.timedOut
            stdoutPath = $validationResult.stdoutPath
            stderrPath = $validationResult.stderrPath
            durationSeconds = $validationResult.durationSeconds
        })
        $RunManifest.validation = [ordered]@{
            summaryPath = $summaryPath
            processFinished = $validationResult.processFinished
            exitStatus = $validationResult.exitStatus
            exitCode = $validationResult.exitCode
            timedOut = $validationResult.timedOut
        }
        if (-not ($validationResult.processFinished -and $validationResult.exitCode -eq 0 -and -not $validationResult.timedOut)) {
            $RunManifest.status = "failed"
            $RunFailures.Add([ordered]@{
                code = "validator-failed"
                message = "Exit-gate validator returned $($validationResult.exitCode)."
                stderrPath = $validationResult.stderrPath
            })
        }
    } else {
        $RunManifest.validation = [ordered]@{ status = "skipped" }
        $RunWarnings.Add([ordered]@{
            code = "validation-skipped"
            message = "Exit-gate validation was skipped by request; run validate_phase0_evidence.py before accepting this evidence."
        })
    }

    $RunManifest.recordedAtUtc = [DateTime]::UtcNow.ToString("o")
    $RunManifest.routes = $RouteRecords
    $RunManifest.commands = $CommandRecords
    $RunManifest.failures = $RunFailures
    $RunManifest.warnings = $RunWarnings
    Write-RunManifest -Path $RunManifestPath -Manifest $RunManifest
    if (
        $RunManifest.status -eq "completed" -and
        $null -ne $validationResult -and
        $validationResult.processFinished -and
        $validationResult.exitCode -eq 0 -and
        -not $validationResult.timedOut
    ) {
        Write-Host "Phase 0 evidence run completed and passed validation."
        Write-Host "Windows x64 evidence-run pass; the overall Phase 0 gate remains incomplete until macOS universal passes. For a gate-enforced check, run validate_phase0_evidence.py --evidence-root `"$RunRoot`" --require-exit-gate."
        Write-Host "  Evidence: $RunRoot"
        Write-Host "  Summary:  $(Join-Path $RunRoot 'validation-summary.json')"
        exit 0
    }
    if ($RunManifest.status -eq "completed" -and $SkipValidation) {
        Write-Host "Evidence routes completed; validation was skipped, so this is not an acceptance pass."
        Write-Host "  Evidence: $RunRoot"
        Write-Host "  Run validate_phase0_evidence.py before accepting this evidence."
        exit 0
    }
    Write-Host "Phase 0 evidence run failed validation; evidence was retained."
    Write-Host "  Evidence: $RunRoot"
    if ($null -ne $validationResult) {
        Write-Host "  Validator logs: $($validationResult.stdoutPath), $($validationResult.stderrPath)"
    }
    exit 1
} catch {
    $message = $_.Exception.Message
    Write-Host "ERROR: $message" -ForegroundColor Red
    if (-not $EffectivePlan) {
        $RunFailures.Add([ordered]@{ code = "runner-error"; message = $message })
        $RunManifest.status = "failed"
        $RunManifest.recordedAtUtc = [DateTime]::UtcNow.ToString("o")
        $RunManifest.commands = $CommandRecords
        $RunManifest.routes = $RouteRecords
        $RunManifest.failures = $RunFailures
        $RunManifest.warnings = $RunWarnings
        try {
            Write-RunManifest -Path $RunManifestPath -Manifest $RunManifest
        } catch {
            Write-Host ("Unable to persist failure manifest: " + $_.Exception.Message) -ForegroundColor Red
        }
        Write-Host "Evidence, if any, was retained at: $RunRoot"
    }
    exit 1
}
