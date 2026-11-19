<#
.SYNOPSIS
Captures the seven Next Calendar import Qt cases independently.

.DESCRIPTION
Runs each selected CalendarPageTests function in a fresh process and retains
the Qt test log, native stdout/stderr, exit code, executable identity, and a
summary under a unique evidence directory.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = "",
    [string]$BuildDirectory = "build\windows-x64-debug",
    [string]$EvidenceRoot = "artifacts\phase9\calendar-import-qt-cases"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) { $RepositoryRoot = Join-Path $PSScriptRoot "..\.." }
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$BuildDirectory = if ([IO.Path]::IsPathRooted($BuildDirectory)) {
    [IO.Path]::GetFullPath($BuildDirectory)
} else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $BuildDirectory)) }
$EvidenceRoot = if ([IO.Path]::IsPathRooted($EvidenceRoot)) {
    [IO.Path]::GetFullPath($EvidenceRoot)
} else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
$RunId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + "-" + [Guid]::NewGuid().ToString("N")
$EvidenceDirectory = Join-Path $EvidenceRoot $RunId
if (Test-Path -LiteralPath $EvidenceDirectory) { throw "Refusing to overwrite test evidence directory $EvidenceDirectory." }
[void][IO.Directory]::CreateDirectory($EvidenceDirectory)

$ExecutablePath = Join-Path $BuildDirectory "Debug\ClassMngrNextCalendarPageTests.exe"
$CachePath = Join-Path $BuildDirectory "CMakeCache.txt"
if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) { throw "Built Calendar Qt test executable is missing: $ExecutablePath" }
if (-not (Test-Path -LiteralPath $CachePath -PathType Leaf)) { throw "Debug CMake cache is missing: $CachePath" }
$cacheLines = @(Get-Content -LiteralPath $CachePath)
$qtCache = @($cacheLines | Where-Object { $_ -match '^Qt6_DIR:[^=]+=' })
if ($qtCache.Count -ne 1) { throw "Expected one Qt6_DIR cache entry, found $($qtCache.Count)." }
$qt6Directory = [IO.Path]::GetFullPath(($qtCache[0] -split "=", 2)[1])
$qtPrefix = [IO.Path]::GetFullPath((Join-Path $qt6Directory "..\..\.."))
$qtBin = Join-Path $qtPrefix "bin"
if (-not (Test-Path -LiteralPath $qtBin -PathType Container)) { throw "Qt binary directory is missing: $qtBin" }

function ConvertTo-WindowsProcessArgument {
    param([AllowEmptyString()][string]$Value)
    $builder = [Text.StringBuilder]::new()
    [void]$builder.Append([char]'"')
    $backslashCount = 0
    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq [char]'\') { ++$backslashCount; continue }
        if ($character -eq [char]'"') {
            for ($index = 0; $index -lt (2 * $backslashCount + 1); $index++) { [void]$builder.Append([char]'\') }
            [void]$builder.Append([char]'"')
        } else {
            for ($index = 0; $index -lt $backslashCount; $index++) { [void]$builder.Append([char]'\') }
            [void]$builder.Append($character)
        }
        $backslashCount = 0
    }
    for ($index = 0; $index -lt (2 * $backslashCount); $index++) { [void]$builder.Append([char]'\') }
    [void]$builder.Append([char]'"')
    $builder.ToString()
}

$testFunctions = @(
    "calendarImportUsesStreamingTypedPathApplyAndRefreshesOnlyOnWrites",
    "calendarImportUsesEnvironmentSourceUrlOverride",
    "calendarImportMalformedAndTransportFailuresAreRecoverable",
    "calendarImportSuspensionAndPathChangeCancelBeforeApply",
    "calendarImportServiceDestructionCleansStagingWithoutWrites",
    "calendarImportQueryFailureIsVisibleAndWritesNothing",
    "calendarImportBatchSaveFailureRollsBackAllRows"
)
$results = [Collections.Generic.List[object]]::new()
foreach ($testFunction in $testFunctions) {
    $testLogPath = Join-Path $EvidenceDirectory ($testFunction + ".qt-test.txt")
    $stdoutPath = Join-Path $EvidenceDirectory ($testFunction + ".stdout.txt")
    $stderrPath = Join-Path $EvidenceDirectory ($testFunction + ".stderr.txt")
    $arguments = @($testFunction, "-o", ($testLogPath + ",txt"), "-v1")
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = [IO.Path]::GetFullPath($ExecutablePath)
    $startInfo.Arguments = (@($arguments | ForEach-Object { ConvertTo-WindowsProcessArgument ([string]$_) }) -join " ")
    $startInfo.WorkingDirectory = $BuildDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.EnvironmentVariables["QT_QPA_PLATFORM"] = "offscreen"
    $startInfo.EnvironmentVariables["QT_SCALE_FACTOR"] = "1"
    $startInfo.EnvironmentVariables["PATH"] = "$qtBin;$env:PATH"
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) { throw "Could not start Calendar Qt test case $testFunction." }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $exitCode = [int]$process.ExitCode
        $stdout = [string]$stdoutTask.Result
        $stderr = [string]$stderrTask.Result
    } finally {
        $process.Dispose()
    }
    [IO.File]::WriteAllText($stdoutPath, $stdout, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($stderrPath, $stderr, [Text.UTF8Encoding]::new($false))
    $testLog = if (Test-Path -LiteralPath $testLogPath -PathType Leaf) { Get-Content -LiteralPath $testLogPath -Raw } else { "" }
    $passMarker = "PASS   : CalendarPageTests::$testFunction()"
    $failLines = @($testLog -split "`r?`n" | Where-Object { $_ -match '^FAIL!\s*:' })
    $passed = ($exitCode -eq 0 -and $testLog.Contains($passMarker) -and $failLines.Count -eq 0)
    $results.Add([ordered]@{
        testFunction = $testFunction
        passed = $passed
        exitCode = $exitCode
        passMarkerObserved = $testLog.Contains($passMarker)
        failureLines = $failLines
        qtTestLogPath = [IO.Path]::GetFullPath($testLogPath)
        stdoutPath = [IO.Path]::GetFullPath($stdoutPath)
        stderrPath = [IO.Path]::GetFullPath($stderrPath)
    })
}

$summary = [ordered]@{
    schema = "classmngr-phase9-calendar-import-focused-qt-tests-v1"
    taskId = "P9_10_PACKAGED_CALENDAR_IMPORT_ACCEPTANCE"
    runId = $RunId
    capturedAtUtc = [DateTimeOffset]::UtcNow.ToString("o")
    executablePath = [IO.Path]::GetFullPath($ExecutablePath)
    executableSha256 = (Get-FileHash -LiteralPath $ExecutablePath -Algorithm SHA256).Hash.ToLowerInvariant()
    buildDirectory = $BuildDirectory
    qtPrefix = $qtPrefix
    expectedCaseCount = 7
    passedCaseCount = @($results | Where-Object passed).Count
    tests = @($results.ToArray())
}
$summaryPath = Join-Path $EvidenceDirectory "summary.json"
[IO.File]::WriteAllText($summaryPath, ($summary | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))
Write-Output "Calendar import Qt case evidence: $EvidenceDirectory"
foreach ($result in $results) { Write-Output "$($result.testFunction): $(if ($result.passed) { 'PASS' } else { "FAIL exit=$($result.exitCode)" })" }
if ($summary.passedCaseCount -ne 7) { throw "Only $($summary.passedCaseCount) of seven isolated Calendar import Qt cases passed; see $summaryPath." }
