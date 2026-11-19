<#
.SYNOPSIS
Checks full loopback fixture delivery and native command output capture.

.DESCRIPTION
Compiles the exact embedded server helpers from the packaged runner, sends two
complete local fixture responses, and verifies exact bodies, byte counts, and
hashes. It also verifies successful stderr preservation and nonzero exit handling.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$runnerPath = Join-Path $PSScriptRoot "run_packaged_next_calendar_import.ps1"
$runnerText = Get-Content -LiteralPath $runnerPath -Raw
$parseTokens = $null
$parseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile($runnerPath, [ref]$parseTokens, [ref]$parseErrors)
if ($parseErrors.Count -gt 0) { throw "Packaged runner has PowerShell syntax errors: $($parseErrors -join '; ')" }
$buildRunnerPath = Join-Path $PSScriptRoot "build_packaged_next_calendar_import_release.ps1"
if (-not (Test-Path -LiteralPath $buildRunnerPath -PathType Leaf)) { throw "Bounded Release build helper is missing." }
$buildRunnerText = Get-Content -LiteralPath $buildRunnerPath -Raw
$buildParseTokens = $null
$buildParseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile($buildRunnerPath, [ref]$buildParseTokens, [ref]$buildParseErrors)
if ($buildParseErrors.Count -gt 0) { throw "Bounded Release build helper has PowerShell syntax errors: $($buildParseErrors -join '; ')" }
$serverMarker = "Add-Type -TypeDefinition @'"
$serverStart = $runnerText.IndexOf($serverMarker, [StringComparison]::Ordinal) + $serverMarker.Length
$serverEnd = $runnerText.IndexOf("'@", $serverStart, [StringComparison]::Ordinal)
if ($serverStart -lt $serverMarker.Length -or $serverEnd -lt $serverStart) { throw "Could not isolate packaged runner's embedded C# helpers." }
$serverSource = $runnerText.Substring($serverStart, $serverEnd - $serverStart)
Add-Type -TypeDefinition $serverSource -ErrorAction Stop

$routeStart = $runnerText.IndexOf("function Invoke-VerifiedRoute", [StringComparison]::Ordinal)
$routeEnd = $runnerText.IndexOf("function Copy-VerifiedFixture", $routeStart, [StringComparison]::Ordinal)
if ($routeStart -lt 0 -or $routeEnd -le $routeStart) { throw "Could not locate packaged route implementation." }
$routeSource = $runnerText.Substring($routeStart, $routeEnd - $routeStart)
foreach ($forbidden in @("WaitForPartialFile", "PartialResponseSent", "ReleaseResponse", "stageFileObservedDuringDownload")) {
    if ($routeSource.Contains($forbidden)) { throw "Packaged route still depends on diagnostic partial-staging probe '$forbidden'." }
}
foreach ($required in @("ResponseCompleted.Wait(10000)", "tempFilesAfterResult")) {
    if (-not $routeSource.Contains($required)) { throw "Packaged route is missing full-response cleanup evidence '$required'." }
}
foreach ($required in @("serviceTerminalCheckpoint", "cacheSettledCheckpoint", "teardownCheckpoints", "Wait-ForCalendarImportCheckpoint")) {
    if (-not $routeSource.Contains($required)) { throw "Packaged route is missing ordered cache/teardown evidence '$required'." }
}
$terminalAt = $routeSource.IndexOf('Wait-ForCalendarImportCheckpoint $context "terminal"', [StringComparison]::Ordinal)
$settledAt = $routeSource.IndexOf('Wait-ForCalendarImportCheckpoint $context "cache-settled"', [StringComparison]::Ordinal)
$closeAt = $routeSource.IndexOf('"close-calendar-preferences"', [StringComparison]::Ordinal)
$suspendAt = $routeSource.IndexOf('"suspend-calendar-to-my-workspace"', [StringComparison]::Ordinal)
if ($terminalAt -lt 0 -or $settledAt -le $terminalAt -or $closeAt -le $settledAt -or $suspendAt -le $closeAt) {
    throw "Packaged route does not wait for terminal counters and settled cache before teardown."
}
$suspendSelectionAt = $routeSource.IndexOf('Add-UiaAction $context $otherNavigation "suspend-calendar-to-my-workspace"', [StringComparison]::Ordinal)
$suspendFallbackAt = $routeSource.IndexOf('Add-UiaClickAction $context $otherNavigation "suspend-calendar-to-my-workspace"', [StringComparison]::Ordinal)
$reentrySelectionAt = $routeSource.IndexOf('Add-UiaAction $context $calendarNavigation "reenter-calendar"', [StringComparison]::Ordinal)
$reentryFallbackAt = $routeSource.IndexOf('Add-UiaClickAction $context $calendarNavigation "reenter-calendar"', [StringComparison]::Ordinal)
if ($suspendSelectionAt -lt 0 -or $suspendFallbackAt -le $suspendSelectionAt -or $reentrySelectionAt -lt 0 -or $reentryFallbackAt -le $reentrySelectionAt) {
    throw "Packaged Calendar suspension/re-entry lack the bounded UIA selection fallback."
}
$reentryCacheWaitAt = $routeSource.IndexOf('Wait-ForCalendarImportCheckpoint $context "cache-settled" ([int]$reentered.observedLogIndex)', [StringComparison]::Ordinal)
$reentryUiWaitAt = $routeSource.IndexOf('Wait-ForCalendarLoad $context $minimumItems 40', [StringComparison]::Ordinal)
if ($reentryCacheWaitAt -lt 0 -or $reentryUiWaitAt -le $reentryCacheWaitAt -or -not $routeSource.Contains('calendar-reentry-cache-timeout.json')) {
    throw "Packaged route does not capture and wait for post-reentry cache/load completion evidence."
}
$calendarListTypeFilterAt = $runnerText.IndexOf('Find-UiaById $Context "calendarUpcomingEvents" "ControlType.List"', [StringComparison]::Ordinal)
$snapshotStart = $runnerText.IndexOf("function Save-UiaControlSnapshot", [StringComparison]::Ordinal)
$snapshotEnd = $runnerText.IndexOf("function Get-PageLifecycleByState", $snapshotStart, [StringComparison]::Ordinal)
$snapshotSource = if ($snapshotStart -ge 0 -and $snapshotEnd -gt $snapshotStart) { $runnerText.Substring($snapshotStart, $snapshotEnd - $snapshotStart) } else { "" }
if ($calendarListTypeFilterAt -lt 0 -or -not $snapshotSource.Contains('$expectedType = if ($automationId -ceq "calendarUpcomingEvents") { "ControlType.List" } else { "" }')) {
    throw "Calendar UIA load and evidence snapshots must select the parent List instead of inherited-id ListItems."
}
if (-not $runnerText.Contains('[switch]$RouteAOnly') -or -not $runnerText.Contains('afterTerminationWorkspaceSnapshot')) {
    throw "Packaged runner is missing its bounded Route A diagnostic mode or linked post-termination snapshot."
}
$configureRecordAt = $runnerText.IndexOf('$script:Manifest.build.configureLogPath = $configureResult.logPath', [StringComparison]::Ordinal)
$buildCommandAt = $runnerText.IndexOf('$buildResult = Invoke-CapturedCommand $cmake @(', [StringComparison]::Ordinal)
if ($configureRecordAt -lt 0 -or $buildCommandAt -le $configureRecordAt -or -not $runnerText.Contains('"--", "/m:4", "/p:TrackFileAccess=false"')) {
    throw "Packaged runner must record configure evidence before building and disable failing MSBuild file tracking in this sandbox."
}
foreach ($required in @("PrebuiltBuildReceiptPath", "prebuiltBuildReceiptPath", "sourceSha256", "cacheSha256After")) {
    if (-not $runnerText.Contains($required)) { throw "Packaged runner cannot verify the elevated build receipt field '$required'." }
}
foreach ($required in @("prebuilt-release", "derived-release-build-receipt.json", "raw-release-build-receipt.json", "post-build-source-delta.json", "post-build-current-source-files.json", "raw-build-source-files.json", "rawPrebuiltBuildReceiptPath", "postBuildCurrentInventoryPath")) {
    if (-not $runnerText.Contains($required)) { throw "Prebuilt Release provenance is not copied into route evidence: '$required'." }
}
foreach ($required in @("GetTopLevelWindowsForProcess", "GetUiaWindowInfoWithTimeout", "Assembly.LoadFrom", "AutomationElement].Assembly.Location", "Get-ProcessWindowSnapshot", "uiaProbeStatus", "uiaProbeTimeoutMillisecondsPerWindow", "closeMainWindowReturned", "graceful-close-in-progress", "graceful-close-timeout", "shutdown-observability.json", "shutdownObservability")) {
    if (-not $runnerText.Contains($required)) { throw "Packaged runner lacks linked process-shutdown diagnostics '$required'." }
}
foreach ($required in @("CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH", "Save-ApplicationShutdownTrace", "applicationShutdownTrace", "application-shutdown.jsonl")) {
    if (-not $runnerText.Contains($required)) { throw "Packaged runner lacks its linked application shutdown trace '$required'." }
}
foreach ($required in @("WriteNormalThreadInfoMiniDump", "MiniDumpWriteDump", "MiniDumpWithThreadInfo", "shutdownMiniDump", "shutdown-threadinfo-12s.dmp", '$dumpTriggerSeconds = 12', "shutdown-dump-capture-failed")) {
    if (-not $runnerText.Contains($required)) { throw "Packaged runner lacks bounded shutdown dump evidence '$required'." }
}
$validatorText = Get-Content -LiteralPath (Join-Path $PSScriptRoot "validate_packaged_next_calendar_import.py") -Raw
foreach ($required in @("application.exec.enter", "application.about_to_quit", "application.exec.returned", "application_bootstrap.finalization.scheduled", "application_bootstrap.finalization.callback_started", "application_bootstrap.finalization.workspace_release_completed", "application_bootstrap.finalization.application_quit_returned")) {
    if (-not $validatorText.Contains($required)) { throw "Packaged validator does not verify shutdown trace milestone '$required'." }
}
foreach ($required in @("application_bootstrap.finalization.theme_release_started", "application_bootstrap.finalization.theme_release_completed")) {
    if (-not $validatorText.Contains($required)) { throw "Packaged validator does not enforce stylesheet restoration after shell release: '$required'." }
}
foreach ($required in @("shutdownMiniDump", "MiniDumpNormal | MiniDumpWithThreadInfo", "shutdown dump artifact identity mismatch")) {
    if (-not $validatorText.Contains($required)) { throw "Packaged validator does not verify shutdown dump evidence '$required'." }
}
$bootstrapSource = Get-Content -LiteralPath (Join-Path $RepositoryRoot "src/next/bootstrap/application_bootstrap.cpp") -Raw
$shellSource = Get-Content -LiteralPath (Join-Path $RepositoryRoot "src/next/ui/application_shell.cpp") -Raw
foreach ($required in @("application_bootstrap.request_window_close.coordinator_request_completed", "application_bootstrap.shutdown_sequence.started", "application_bootstrap.shutdown_sequence.completed", "application_bootstrap.finalization.scheduled", "application_bootstrap.finalization.started", "application_bootstrap.finalization.application_quit_returned")) {
    if (-not $bootstrapSource.Contains($required)) { throw "Bootstrap source lacks environment-gated shutdown trace milestone '$required'." }
}
$finalizerStart = $bootstrapSource.IndexOf("void ApplicationBootstrap::finalizeShutdown()", [StringComparison]::Ordinal)
$finalizerEnd = $bootstrapSource.IndexOf("bool ApplicationBootstrap::shutdownRequested() const noexcept", $finalizerStart, [StringComparison]::Ordinal)
$finalizerSource = if ($finalizerStart -ge 0 -and $finalizerEnd -gt $finalizerStart) { $bootstrapSource.Substring($finalizerStart, $finalizerEnd - $finalizerStart) } else { "" }
$shellResetAt = $finalizerSource.IndexOf("m_shell.reset()", [StringComparison]::Ordinal)
$themeResetAt = $finalizerSource.IndexOf("m_coreServices->themePresenter.reset()", [StringComparison]::Ordinal)
if ($shellResetAt -lt 0 -or $themeResetAt -le $shellResetAt) {
    throw "Shutdown finalization must destroy the ApplicationShell before restoring the previous stylesheet."
}
$destructorStart = $bootstrapSource.IndexOf("ApplicationBootstrap::~ApplicationBootstrap()", [StringComparison]::Ordinal)
$destructorEnd = $bootstrapSource.IndexOf("Application::StartupResult ApplicationBootstrap::start()", $destructorStart, [StringComparison]::Ordinal)
$destructorSource = if ($destructorStart -ge 0 -and $destructorEnd -gt $destructorStart) { $bootstrapSource.Substring($destructorStart, $destructorEnd - $destructorStart) } else { "" }
$destructorShellResetAt = $destructorSource.IndexOf("m_shell.reset()", [StringComparison]::Ordinal)
$destructorThemeResetAt = $destructorSource.IndexOf("m_coreServices->themePresenter.reset()", [StringComparison]::Ordinal)
if ($destructorShellResetAt -lt 0 -or $destructorThemeResetAt -le $destructorShellResetAt) {
    throw "Bootstrap destructor must destroy the ApplicationShell before restoring the previous stylesheet."
}
foreach ($required in @("application_shell.close_event.enter", "application_shell.close_event.acceptance")) {
    if (-not $shellSource.Contains($required)) { throw "ApplicationShell source lacks environment-gated close trace milestone '$required'." }
}
foreach ($required in @('$graceSeconds = 45', "Get-ProcessExitStatusSample", "exitStatusSamples", "cpuTimeMilliseconds", "workingSetBytes", "privateBytes", "handles =", "threads =", 'WaitForExit($remainingMilliseconds)')) {
    if (-not $runnerText.Contains($required)) { throw "Packaged runner lacks the 45-second per-second process status series '$required'." }
}
foreach ($required in @('$windowCount = [Int64]@($nativeWindows).Count', '$threadCount = [Int64]0', '$threadCount = [Int64]@($Process.Threads).Count', 'threads = $threadCount', 'topLevelWindowCount = $windowCount')) {
    if (-not $runnerText.Contains($required)) { throw "Packaged runner does not safely record the fast natural-exit status sample '$required'." }
}
foreach ($required in @("classmngr-phase9-calendar-import-release-build-receipt-v1", "releaseExecutableSha256", "sourceInventoryFileSha256", "ClassMngrNextResourcePackage", "generatedResources", "verified-resource-inputs", "currentUserSid", "SetAccessRule", '"--", "/m:4", "/p:TrackFileAccess=false"')) {
    if (-not $buildRunnerText.Contains($required)) { throw "Bounded Release build helper is missing receipt/build evidence '$required'." }
}
foreach ($required in @("ClassMngrNextResourcePackage", "generatedResources", "prebuiltBuildReceiptPath", "verifiedResourceInputsInventorySha256", "non-elevated-copy-from-verified-release-build-resource-inputs")) {
    if (-not $runnerText.Contains($required)) { throw "Packaged runner is missing resource-package receipt verification '$required'." }
}
$resourceReceiptCheckAt = $runnerText.IndexOf('$currentStagedInventory = Get-CanonicalInventory $expectedStagingRoot', [StringComparison]::Ordinal)
$resourceCopyAt = $runnerText.IndexOf('non-elevated-copy-from-verified-release-build-resource-inputs', [StringComparison]::Ordinal)
$deployAt = $runnerText.IndexOf('Invoke-CapturedCommand $windeployqt', [StringComparison]::Ordinal)
if ($resourceReceiptCheckAt -lt 0 -or $resourceCopyAt -le $resourceReceiptCheckAt -or $deployAt -le $resourceCopyAt) {
    throw "Packaged runner must verify staged Release resources before its non-elevated package copy and Qt deployment."
}
foreach ($required in @("downloaded_bytes", "staged_workbook_removed_at_terminal", "staged_workbook_released_before_apply", "cacheSettledCheckpoint", "upcoming_range_loaded", "calendarLoadWait")) {
    if (-not $validatorText.Contains($required)) { throw "Packaged validator is missing service staging evidence '$required'." }
}
foreach ($required in @("raw build source inventory fingerprint mismatch", "source delta changed paths do not match raw/current inventory rows", "verified Release resource stage fingerprint differs", "post-build changes escaped the runner/validator/harness")) {
    if (-not $validatorText.Contains($required)) { throw "Packaged validator is missing script-only receipt verification '$required'." }
}
foreach ($required in @("_validate_shutdown_observability", "gracefulExitCompleted", "gracefulExitTimedOut", "shutdown observability artifact identity mismatch")) {
    if (-not $validatorText.Contains($required)) { throw "Packaged validator does not enforce linked normal-exit diagnostics '$required'." }
}

$captureStart = $runnerText.IndexOf("function ConvertTo-WindowsProcessArgument", [StringComparison]::Ordinal)
$captureEnd = $runnerText.IndexOf("function Invoke-PythonJson", $captureStart, [StringComparison]::Ordinal)
if ($captureStart -lt 0 -or $captureEnd -le $captureStart) { throw "Could not locate native command capture helpers." }
$captureSource = $runnerText.Substring($captureStart, $captureEnd - $captureStart)
$shaStart = $runnerText.IndexOf("function Get-FileSha256", [StringComparison]::Ordinal)
$shaEnd = $runnerText.IndexOf("function Write-JsonFile", $shaStart, [StringComparison]::Ordinal)
if ($shaStart -lt 0 -or $shaEnd -le $shaStart) { throw "Could not locate the runner's file hash helper." }
$shaSource = $runnerText.Substring($shaStart, $shaEnd - $shaStart)

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}
function Assert-BytesEqual {
    param([byte[]]$Expected, [byte[]]$Actual, [string]$Message)
    if ($Expected.Length -ne $Actual.Length) { throw "$Message (length $($Actual.Length), expected $($Expected.Length))." }
    for ($index = 0; $index -lt $Expected.Length; $index++) {
        if ($Expected[$index] -ne $Actual[$index]) { throw "$Message (different byte at offset $index)." }
    }
}
function Read-HttpHeaders {
    param([IO.Stream]$Stream)
    $builder = [Text.StringBuilder]::new()
    $matched = 0
    while ($builder.Length -lt 32768) {
        $value = $Stream.ReadByte()
        if ($value -lt 0) { throw "Fixture server closed before sending HTTP headers." }
        [void]$builder.Append([char]$value)
        $matched = if ($matched -eq 0 -and $value -eq 13) { 1 }
            elseif ($matched -eq 1 -and $value -eq 10) { 2 }
            elseif ($matched -eq 2 -and $value -eq 13) { 3 }
            elseif ($matched -eq 3 -and $value -eq 10) { 4 }
            elseif ($value -eq 13) { 1 } else { 0 }
        if ($matched -eq 4) { return $builder.ToString() }
    }
    throw "Fixture server returned an oversized HTTP header block."
}
function Invoke-CompleteFixtureResponse {
    param([byte[]]$Body)
    $server = [P910LoopbackFixtureServer]::new($Body)
    $client = [Net.Sockets.TcpClient]::new()
    try {
        $client.ReceiveTimeout = 5000
        $client.Connect([Net.IPAddress]::Loopback, $server.Port)
        $stream = $client.GetStream()
        $request = [Text.Encoding]::ASCII.GetBytes("GET /calendar-import HTTP/1.1`r`nHost: 127.0.0.1`r`nConnection: close`r`n`r`n")
        $stream.Write($request, 0, $request.Length)
        $stream.Flush()
        $headers = Read-HttpHeaders $stream
        Assert-True ($server.RequestSeen.Wait(3000)) "Fixture server did not accept the local request."
        Assert-True ($server.ResponseCompleted.Wait(3000)) "Fixture server did not complete the full response."
        $bodyBuffer = [IO.MemoryStream]::new()
        $buffer = [byte[]]::new(4096)
        while (($read = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) { $bodyBuffer.Write($buffer, 0, $read) }
        [ordered]@{ server = $server; client = $client; headers = $headers; body = $bodyBuffer.ToArray(); bodyBuffer = $bodyBuffer }
    } catch {
        $server.Abort()
        $server.Dispose()
        $client.Dispose()
        throw
    }
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ("p9-10-harness-test-" + [Guid]::NewGuid().ToString("N"))
[void][IO.Directory]::CreateDirectory($testRoot)
try {
    Invoke-Expression $shaSource
    Invoke-Expression $captureSource
    $selfProcess = [Diagnostics.Process]::GetCurrentProcess()
    $selfDumpPath = Join-Path $testRoot "harness-self-threadinfo.dmp"
    $selfDumpBytes = [P910Native]::WriteNormalThreadInfoMiniDump($selfProcess.Handle, [int]$selfProcess.Id, $selfDumpPath)
    $selfDumpInfo = Get-Item -LiteralPath $selfDumpPath
    Assert-True ($selfDumpBytes -eq $selfDumpInfo.Length -and $selfDumpInfo.Length -gt 4) "MiniDumpWriteDump did not write a complete nonempty self-process dump."
    $selfDumpStream = [IO.File]::OpenRead($selfDumpPath)
    try {
        $signatureBytes = [byte[]]::new(4)
        if ($selfDumpStream.Read($signatureBytes, 0, 4) -ne 4) { throw "Self-process minidump header was truncated." }
        Assert-True ([Text.Encoding]::ASCII.GetString($signatureBytes) -ceq "MDMP") "Self-process minidump has an invalid header signature."
    } finally { $selfDumpStream.Dispose(); $selfProcess.Dispose() }
    $warningDirectory = Join-Path $testRoot "native-capture"
    [void][IO.Directory]::CreateDirectory($warningDirectory)
    $pythonPath = [IO.Path]::GetFullPath((Get-Command python.exe -ErrorAction Stop).Source)
    $warningLog = Join-Path $warningDirectory "successful-stderr-warning.log"
    $warningResult = Invoke-CapturedCommand $pythonPath @("-c", "import sys; print('capture-stdout'); print('capture-stderr', file=sys.stderr)") $warningLog "successful stderr capture test"
    Assert-True ($warningResult.exitCode -eq 0) "Successful command with stderr output was treated as a failure."
    Assert-True ((Get-Content -LiteralPath $warningResult.stdoutLogPath -Raw).Contains("capture-stdout")) "Native stdout was not preserved."
    Assert-True ((Get-Content -LiteralPath $warningResult.stderrLogPath -Raw).Contains("capture-stderr")) "Successful native stderr was not preserved."
    $failureLog = Join-Path $warningDirectory "nonzero-exit.log"
    $nonzeroThrew = $false
    try {
        $null = Invoke-CapturedCommand $pythonPath @("-c", "import sys; print('capture-failure-stderr', file=sys.stderr); sys.exit(7)") $failureLog "nonzero exit capture test"
    } catch {
        $nonzeroThrew = $_.Exception.Message.Contains("exit code 7")
        if (-not $nonzeroThrew) { throw }
    }
    Assert-True $nonzeroThrew "Native nonzero exit code was not surfaced as failure."
    Assert-True ((Get-Content -LiteralPath $failureLog -Raw).Contains("exitCode: 7")) "Native exit code was not retained."
    Assert-True ((Get-Content -LiteralPath ($failureLog + ".stderr.txt") -Raw).Contains("capture-failure-stderr")) "Nonzero native stderr was not preserved."

    $successBytes = [byte[]]::new(16284)
    for ($index = 0; $index -lt $successBytes.Length; $index++) { $successBytes[$index] = [byte](($index * 37 + 11) % 256) }
    $errorBytes = [byte[]]::new(69)
    for ($index = 0; $index -lt $errorBytes.Length; $index++) { $errorBytes[$index] = [byte](($index * 19 + 7) % 256) }
    foreach ($fixtureCase in @(
        [ordered]@{ name = "success-sized"; bytes = $successBytes },
        [ordered]@{ name = "malformed-sized"; bytes = $errorBytes }
    )) {
        $result = Invoke-CompleteFixtureResponse $fixtureCase.bytes
        try {
            Assert-True ($result.headers.Contains("HTTP/1.1 200 OK")) "$($fixtureCase.name) response status was not HTTP 200."
            Assert-True ($result.headers.Contains("Content-Length: $($fixtureCase.bytes.Length)")) "$($fixtureCase.name) content length differed from source bytes."
            Assert-BytesEqual $fixtureCase.bytes $result.body "$($fixtureCase.name) response body differed from fixture bytes"
            Assert-True ($result.server.ResponseBytesSent -eq $fixtureCase.bytes.Length) "$($fixtureCase.name) server byte count was incomplete."
            $sha = [Security.Cryptography.SHA256]::Create()
            try { $expectedHash = ([BitConverter]::ToString($sha.ComputeHash($fixtureCase.bytes))).Replace("-", "").ToLowerInvariant() }
            finally { $sha.Dispose() }
            Assert-True ($result.server.ResponseSha256 -ceq $expectedHash) "$($fixtureCase.name) server response hash differed from source bytes."
            Assert-True ($result.server.RequestCount -eq 1 -and $result.server.RequestPath -ceq "/calendar-import" -and $result.server.RemoteAddress -ceq "127.0.0.1") "$($fixtureCase.name) request identity differed."
            Assert-True ($result.server.ResponseCompleted.IsSet -and -not $result.server.IsAborted) "$($fixtureCase.name) server did not complete normally."
        } finally {
            $result.bodyBuffer.Dispose()
            $result.client.Dispose()
            $result.server.Dispose()
        }
    }
} finally {
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}
"Packaged Calendar import harness checks passed: fixture delivery, native command capture, and normal thread-info minidump capture."
