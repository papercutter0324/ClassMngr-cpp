[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$ControlConfigPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not ('P917HeapWorkerSessionNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class P917HeapWorkerSessionNative
{
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern uint WTSGetActiveConsoleSessionId();
}
'@
}

function Get-P917HeapWorkerUtc {
    return [DateTimeOffset]::UtcNow.ToString('o')
}

function Write-P917HeapWorkerJsonAtomic([string]$Path, [object]$Value) {
    $temporaryPath = $Path + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    $json = ConvertTo-Json -InputObject $Value -Depth 14
    [IO.File]::WriteAllText($temporaryPath, $json, [Text.UTF8Encoding]::new($false))
    [IO.File]::Move($temporaryPath, $Path)
}

function Add-P917HeapWorkerJsonLine([string]$Path, [object]$Value) {
    $json = ConvertTo-Json -InputObject $Value -Depth 12 -Compress
    [IO.File]::AppendAllText($Path, $json + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
}

function Get-P917HeapWorkerSha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
        finally { $sha.Dispose() }
    } finally { $stream.Dispose() }
}

function Test-P917HeapPathUnderRoot([string]$Path, [string]$Root) {
    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    return $fullPath.StartsWith($fullRoot, [StringComparison]::OrdinalIgnoreCase)
}

function Get-P917HeapWorkerContext {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    try {
        $principal = [Security.Principal.WindowsPrincipal]::new($identity)
        $process = [Diagnostics.Process]::GetCurrentProcess()
        return [ordered]@{
            identity = [string]$identity.Name
            userSid = if ($null -ne $identity.User) { [string]$identity.User.Value } else { $null }
            isAdministratorRole = [bool]$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
            userInteractive = [bool][Environment]::UserInteractive
            processSessionId = [Int64]$process.SessionId
            activeConsoleSessionId = [Int64][P917HeapWorkerSessionNative]::WTSGetActiveConsoleSessionId()
        }
    } finally { $identity.Dispose() }
}

function ConvertTo-P917HeapWprArgumentLine([string[]]$Arguments) {
    return (@($Arguments | ForEach-Object { '"' + ([string]$_).Replace('"', '\"') + '"' }) -join ' ')
}

function Invoke-P917HeapWprCommand([object]$State, [string[]]$Arguments, [int]$TimeoutSeconds) {
    $startedAtUtc = Get-P917HeapWorkerUtc
    $argumentLine = ConvertTo-P917HeapWprArgumentLine $Arguments
    $record = [ordered]@{
        command = '"' + [string]$State.config.wprPath + '" ' + $argumentLine
        arguments = @($Arguments)
        startedAtUtc = $startedAtUtc
        completedAtUtc = $null
        timeoutSeconds = $TimeoutSeconds
        timedOut = $false
        exitCode = $null
        stdout = ''
        stderr = ''
        launchError = $null
    }
    $process = $null
    $stdoutTask = $null
    $stderrTask = $null
    try {
        $startInfo = [Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = [string]$State.config.wprPath
        $startInfo.Arguments = $argumentLine
        $startInfo.WorkingDirectory = [string]$State.config.repositoryRoot
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $process = [Diagnostics.Process]::new()
        $process.StartInfo = $startInfo
        if (-not $process.Start()) { throw 'WPR command process did not start.' }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            $record.timedOut = $true
            try { $process.Kill() } catch { $record.launchError = 'WPR timeout kill failed: ' + $_.Exception.Message }
            if (-not $process.WaitForExit(5000)) {
                $record.launchError = 'WPR command did not exit within five seconds after timeout termination.'
            }
            $record.stderr = 'WPR command exceeded its bounded timeout.'
        } else {
            $null = $process.WaitForExit(5000)
            $record.exitCode = [int]$process.ExitCode
            if ($null -ne $stdoutTask -and $stdoutTask.Wait(5000)) { $record.stdout = [string]$stdoutTask.GetAwaiter().GetResult() }
            if ($null -ne $stderrTask -and $stderrTask.Wait(5000)) { $record.stderr = [string]$stderrTask.GetAwaiter().GetResult() }
            if (($null -ne $stdoutTask -and -not $stdoutTask.IsCompleted) -or ($null -ne $stderrTask -and -not $stderrTask.IsCompleted)) {
                $record.launchError = 'WPR output streams did not close within five seconds after process exit.'
            }
        }
    } catch {
        $record.launchError = $_.Exception.Message
    } finally {
        $record.completedAtUtc = Get-P917HeapWorkerUtc
        try { Add-P917HeapWorkerJsonLine ([string]$State.config.commandLogPath) $record } catch { }
        if ($null -ne $process) { $process.Dispose() }
    }
    return $record
}

function Test-P917HeapWprRecording([object]$CommandResult) {
    if ($null -eq $CommandResult -or $null -eq $CommandResult.exitCode -or [int]$CommandResult.exitCode -ne 0 -or [bool]$CommandResult.timedOut) { return $false }
    $output = [string]$CommandResult.stdout + "`n" + [string]$CommandResult.stderr
    return ($output -match '(?i)\brecording\b' -and $output -notmatch '(?i)\bnot recording\b')
}

function Write-P917HeapWorkerResponse([object]$State, [object]$Request, [object]$Result) {
    $responsePath = Join-Path ([string]$State.config.responseRoot) ('response-{0:D4}.json' -f [int]$Request.sequence)
    $response = [ordered]@{
        schema = 'classmngr-phase9-p9-17-heap-worker-response-v1'
        runId = [string]$State.config.runId
        nonce = [string]$State.config.nonce
        sequence = [int]$Request.sequence
        command = [string]$Request.command
        status = [string]$Result.status
        result = $Result
        completedAtUtc = Get-P917HeapWorkerUtc
    }
    Write-P917HeapWorkerJsonAtomic $responsePath $response
    return $response
}

function Stop-P917HeapWorkerTrace([object]$State) {
    if ([bool]$State.cleanupCompleted) { return }
    $State.status = 'stopping'
    try {
        $sessionStatus = Invoke-P917HeapWprCommand $State @('-status', '-instancename', [string]$State.config.wprInstanceName) ([int]$State.config.wprCommandTimeoutSeconds)
        if (Test-P917HeapWprRecording $sessionStatus) {
            $State.sessionStopAttempted = $true
            $stop = Invoke-P917HeapWprCommand $State @('-stop', [string]$State.config.tracePath, 'P9-17 PID-scoped heap snapshot diagnostic', '-instancename', [string]$State.config.wprInstanceName) ([int]$State.config.wprCommandTimeoutSeconds)
            if ($null -ne $stop.exitCode -and [int]$stop.exitCode -eq 0 -and -not [bool]$stop.timedOut) { $State.sessionStopped = $true }
            else { $State.cleanupErrors.Add("WPR stop failed: $($stop.stderr) $($stop.stdout) $($stop.launchError)") }
        } elseif ([bool]$State.sessionStartAttempted -and -not [bool]$State.sessionStarted) {
            $State.cleanupErrors.Add('No P9-17 named WPR session was confirmed after the start attempt.')
        } else {
            $State.sessionStopped = $true
        }
    } catch { $State.cleanupErrors.Add('WPR session cleanup failed: ' + $_.Exception.Message) }

    if ([bool]$State.snapshotConfigEnableAttempted) {
        try {
            $State.snapshotConfigDisableAttempted = $true
            $disable = Invoke-P917HeapWprCommand $State @('-snapshotconfig', 'heap', '-pid', [string]$State.config.processId, 'disable') ([int]$State.config.wprCommandTimeoutSeconds)
            if ($null -ne $disable.exitCode -and [int]$disable.exitCode -eq 0 -and -not [bool]$disable.timedOut) { $State.snapshotConfigDisabled = $true }
            else { $State.cleanupErrors.Add("WPR PID-scoped snapshot configuration disable failed: $($disable.stderr) $($disable.stdout) $($disable.launchError)") }
        } catch { $State.cleanupErrors.Add('WPR PID-scoped snapshot configuration cleanup failed: ' + $_.Exception.Message) }
    } else {
        $State.snapshotConfigDisabled = $true
    }

    try {
        $finalStatus = Invoke-P917HeapWprCommand $State @('-status') ([int]$State.config.wprCommandTimeoutSeconds)
        $State.noActiveWprSessionAtEnd = ($null -ne $finalStatus.exitCode -and [int]$finalStatus.exitCode -eq 0 -and -not [bool]$finalStatus.timedOut -and [string]$finalStatus.stdout -match '(?i)not recording' -and [string]$finalStatus.stdout -notmatch '(?i)\brecording\b.*\bactive\b')
        if (-not [bool]$State.noActiveWprSessionAtEnd) { $State.cleanupErrors.Add('Final wpr -status did not confirm that no WPR session is active.') }
    } catch { $State.noActiveWprSessionAtEnd = $false; $State.cleanupErrors.Add('Final wpr -status failed: ' + $_.Exception.Message) }

    if (Test-Path -LiteralPath ([string]$State.config.tracePath) -PathType Leaf) {
        $State.traceSizeBytes = [Int64](Get-Item -LiteralPath ([string]$State.config.tracePath)).Length
        $State.traceSha256 = Get-P917HeapWorkerSha256 ([string]$State.config.tracePath)
    } elseif ([bool]$State.sessionStarted) {
        $State.cleanupErrors.Add('WPR stopped without producing the expected ETL file.')
    }
    $State.cleanupCompleted = $true
    $State.completedAtUtc = Get-P917HeapWorkerUtc
    $allSnapshotsCaptured = (@($State.snapshots | Where-Object { [string]$_.status -cne 'captured' }).Count -eq 0 -and @($State.snapshots).Count -eq 3)
    $cleanupOkay = ([bool]$State.sessionStopped -and [bool]$State.snapshotConfigDisabled -and [bool]$State.noActiveWprSessionAtEnd -and $State.cleanupErrors.Count -eq 0)
    $State.status = if ($allSnapshotsCaptured -and $cleanupOkay -and $null -ne $State.traceSha256) { 'completed' } else { 'failed' }
}

function Add-P917HeapWorkerSnapshot([object]$State, [object]$Request) {
    $allowed = [ordered]@{
        'post-primer-plateau-baseline' = 'plateau-baseline-05'
        'cycle-01-return' = 'cycle-01-return'
        'cycle-10-return' = 'cycle-10-return'
    }
    $label = [string]$Request.label
    $windowId = [string]$Request.windowId
    if (-not $allowed.ContainsKey($label) -or [string]$allowed[$label] -cne $windowId) {
        throw 'Heap snapshot request is outside the three approved P9-17 return checkpoints.'
    }
    $expectedLabels = @($allowed.Keys)
    if ($State.snapshots.Count -ge $expectedLabels.Count -or [string]$expectedLabels[$State.snapshots.Count] -cne $label) {
        throw 'Heap snapshot checkpoints were requested out of the required plateau, cycle-1, cycle-10 order.'
    }
    $expectedNextUiAction = @{
        'post-primer-plateau-baseline' = 'cycle-01-open-roster'
        'cycle-01-return' = 'cycle-02-open-roster'
        'cycle-10-return' = 'cycle-final-reenter-roster'
    }
    if ([string]$Request.routeCheckpoint.windowId -cne $windowId -or
        [string]$Request.routeCheckpoint.label -cne $label -or
        [string]$Request.routeCheckpoint.nextUiActionKind -cne [string]$expectedNextUiAction[$label] -or
        -not [bool]$Request.routeCheckpoint.nextUiActionExpectedNotStarted -or
        [string]::IsNullOrWhiteSpace([string]$Request.routeCheckpoint.memoryWindowCompletedAtUtc)) {
        throw 'Heap snapshot request does not bind the target sampled window and its not-yet-started next UI action.'
    }
    if (@($State.snapshots | Where-Object { [string]$_.label -ceq $label }).Count -ne 0) {
        throw "Duplicate heap snapshot checkpoint requested: $label"
    }
    $target = Get-Process -Id ([int]$State.config.processId) -ErrorAction Stop
    $target.Refresh()
    if ($target.HasExited) { throw 'Measured application exited before the requested heap snapshot.' }
    $targetImagePath = [IO.Path]::GetFullPath([string]$target.MainModule.FileName)
    if ($targetImagePath -ine [IO.Path]::GetFullPath([string]$State.config.processImagePath) -or
        (Get-P917HeapWorkerSha256 $targetImagePath) -cne [string]$State.config.processImageSha256 -or
        [Int64]$target.SessionId -ne [Int64]$State.config.processSessionId) {
        throw 'Measured application PID image or session changed before the requested heap snapshot.'
    }
    $triggeredAtUtc = Get-P917HeapWorkerUtc
    $result = Invoke-P917HeapWprCommand $State @('-singlesnapshot', 'Heap', [string]$State.config.processId) ([int]$State.config.wprCommandTimeoutSeconds)
    $snapshot = [ordered]@{
        label = $label
        windowId = $windowId
        processId = [Int64]$State.config.processId
        targetImagePath = $targetImagePath
        targetImageSha256 = [string]$State.config.processImageSha256
        targetSessionId = [Int64]$target.SessionId
        triggeredAtUtc = $triggeredAtUtc
        commandStartedAtUtc = [string]$result.startedAtUtc
        commandCompletedAtUtc = [string]$result.completedAtUtc
        returnCode = $result.exitCode
        timedOut = [bool]$result.timedOut
        stdout = [string]$result.stdout
        stderr = [string]$result.stderr
        launchError = $result.launchError
        status = if ($null -ne $result.exitCode -and [int]$result.exitCode -eq 0 -and -not [bool]$result.timedOut) { 'captured' } else { 'failed' }
        routeCheckpoint = $Request.routeCheckpoint
    }
    $State.snapshots.Add($snapshot)
    if ([string]$snapshot.status -cne 'captured') { throw "WPR heap snapshot '$label' failed: $($snapshot.stderr) $($snapshot.stdout) $($snapshot.launchError)" }
    return $snapshot
}

function Write-P917HeapWorkerFinal([object]$State, [string]$FailureMessage) {
    if (-not [string]::IsNullOrWhiteSpace($FailureMessage) -and $State.cleanupErrors.Count -eq 0) { $State.cleanupErrors.Add($FailureMessage) }
    if (-not [bool]$State.cleanupCompleted) { Stop-P917HeapWorkerTrace $State }
    $final = [ordered]@{
        schema = 'classmngr-phase9-p9-17-heap-worker-final-v1'
        runId = [string]$State.config.runId
        nonce = [string]$State.config.nonce
        status = [string]$State.status
        diagnosticOnly = $true
        accepted = $false
        processId = [Int64]$State.config.processId
        workerProcessId = [Int64]$PID
        executionContext = $State.executionContext
        snapshots = $State.snapshots.ToArray()
        tracePath = [string]$State.config.tracePath
        traceSha256 = $State.traceSha256
        traceSizeBytes = $State.traceSizeBytes
        sessionStopped = [bool]$State.sessionStopped
        snapshotConfigDisabled = [bool]$State.snapshotConfigDisabled
        noActiveWprSessionAtEnd = $State.noActiveWprSessionAtEnd
        cleanupCompleted = [bool]$State.cleanupCompleted
        cleanupErrors = $State.cleanupErrors.ToArray()
        failure = if ([string]::IsNullOrWhiteSpace($FailureMessage)) { $null } else { $FailureMessage }
        completedAtUtc = Get-P917HeapWorkerUtc
    }
    try { Write-P917HeapWorkerJsonAtomic ([string]$State.config.finalStatusPath) $final } catch { }
    return $final
}

$state = $null
$context = $null
$failureMessage = $null
try {
    $configPath = [IO.Path]::GetFullPath($ControlConfigPath)
    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) { throw 'Heap worker configuration file is missing.' }
    $config = Get-Content -LiteralPath $configPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ([string]$config.schema -cne 'classmngr-phase9-p9-17-heap-worker-config-v1') { throw 'Heap worker configuration schema is unsupported.' }
    $controlRoot = [IO.Path]::GetFullPath([string]$config.controlRoot)
    $evidenceDirectory = [IO.Path]::GetFullPath([string]$config.evidenceDirectory)
    if (-not (Test-P917HeapPathUnderRoot $configPath $controlRoot) -or -not (Test-P917HeapPathUnderRoot $controlRoot $evidenceDirectory)) { throw 'Heap worker control paths escape their diagnostic evidence directory.' }
    foreach ($controlPath in @($config.requestRoot, $config.responseRoot, $config.readyStatusPath, $config.finalStatusPath, $config.authorizationPath, $config.commandLogPath)) {
        if (-not (Test-P917HeapPathUnderRoot ([string]$controlPath) $controlRoot)) { throw 'Heap worker request, response, status, or log path escapes its control directory.' }
    }
    if (-not (Test-P917HeapPathUnderRoot $config.tracePath $evidenceDirectory) -or [IO.Path]::GetFullPath([string]$config.tracePath) -notmatch '\\trace\\') { throw 'Heap worker trace path is outside the diagnostic trace directory.' }
    if ((Get-Date).ToUniversalTime() -gt [DateTimeOffset]::Parse([string]$config.configExpiresAtUtc).UtcDateTime) { throw 'Heap worker launch authorization expired before elevated startup.' }
    if ([int]$config.wprCommandTimeoutSeconds -lt 10 -or [int]$config.wprCommandTimeoutSeconds -gt 60) { throw 'Heap worker WPR command timeout is outside the allowed 10–60 second range.' }
    if ([int]$config.sessionWatchdogSeconds -lt 120 -or [int]$config.sessionWatchdogSeconds -gt 1200) { throw 'Heap worker session watchdog is outside the allowed 120–1200 second range.' }
    if (-not (Test-Path -LiteralPath ([string]$config.wprPath) -PathType Leaf) -or (Get-P917HeapWorkerSha256 ([string]$config.wprPath)) -cne [string]$config.wprSha256) { throw 'Elevated worker WPR executable hash differs from its standard-runner configuration.' }
    $currentWorkerProcess = [Diagnostics.Process]::GetCurrentProcess()
    $workerImagePath = [IO.Path]::GetFullPath([string]$currentWorkerProcess.MainModule.FileName)
    $workerImageSha256 = Get-P917HeapWorkerSha256 $workerImagePath
    if ([IO.Path]::GetFullPath([string]$config.workerExecutablePath) -ine $workerImagePath -or
        [string]$config.workerExecutableSha256 -notmatch '^[0-9a-fA-F]{64}$' -or
        $workerImageSha256 -cne [string]$config.workerExecutableSha256) {
        throw 'Elevated WPR worker process image path or hash differs from its standard-runner launch configuration.'
    }

    $context = Get-P917HeapWorkerContext
    if (-not [bool]$context.isAdministratorRole) { throw 'The WPR worker did not receive an elevated Administrator-role token.' }
    if (-not [bool]$context.userInteractive) { throw 'The WPR worker is not attached to an interactive session.' }
    if ([string]$context.userSid -cne [string]$config.runnerUserSid) { throw 'The elevated WPR worker user differs from the standard route runner identity.' }
    if ([Int64]$context.processSessionId -lt 1 -or [Int64]$context.activeConsoleSessionId -ne [Int64]$context.processSessionId) { throw 'The elevated WPR worker is not in the active console session.' }
    $context.workerRole = 'WPR-only elevated worker; does not launch, control, or terminate the application process.'

    $target = Get-Process -Id ([int]$config.processId) -ErrorAction Stop
    $target.Refresh()
    if ($target.HasExited) { throw 'Measured application exited before the WPR worker verified its PID.' }
    $targetImagePath = [IO.Path]::GetFullPath([string]$target.MainModule.FileName)
    if ($targetImagePath -ine [IO.Path]::GetFullPath([string]$config.processImagePath) -or
        (Get-P917HeapWorkerSha256 $targetImagePath) -cne [string]$config.processImageSha256) {
        throw 'Measured application PID image path or hash differs from the standard-runner binding.'
    }
    if ([Int64]$target.SessionId -ne [Int64]$context.activeConsoleSessionId) { throw 'Measured application is not in the worker active-console session.' }

    if ([int]$config.authorizationTimeoutSeconds -lt 10 -or [int]$config.authorizationTimeoutSeconds -gt 120) { throw 'Heap worker authorization wait is outside the allowed 10–120 second range.' }
    $authorizationDeadline = [DateTime]::UtcNow.AddSeconds([int]$config.authorizationTimeoutSeconds)
    $authorization = $null
    while ([DateTime]::UtcNow -lt $authorizationDeadline) {
        if (Test-Path -LiteralPath ([string]$config.authorizationPath) -PathType Leaf) {
            $authorization = Get-Content -LiteralPath ([string]$config.authorizationPath) -Raw -Encoding UTF8 | ConvertFrom-Json
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if ($null -eq $authorization -or
        [string]$authorization.schema -cne 'classmngr-phase9-p9-17-heap-worker-authorization-v1' -or
        [string]$authorization.runId -cne [string]$config.runId -or [string]$authorization.nonce -cne [string]$config.nonce -or
        [Int64]$authorization.workerProcessId -ne [Int64]$PID -or
        [Int64]$authorization.routeProcessId -ne [Int64]$config.processId -or
        [string]$authorization.routeImageSha256 -cne [string]$config.processImageSha256 -or
        [string]$authorization.runnerUserSid -cne [string]$config.runnerUserSid) {
        throw 'Elevated WPR worker did not receive the matching runner authorization before its bounded deadline.'
    }
    $authorizationSha256 = Get-P917HeapWorkerSha256 ([string]$config.authorizationPath)

    $state = [ordered]@{
        config = $config
        status = 'starting'
        executionContext = $context
        authorization = $authorization
        authorizationSha256 = $authorizationSha256
        commands = [Collections.Generic.List[object]]::new()
        snapshots = [Collections.Generic.List[object]]::new()
        cleanupErrors = [Collections.Generic.List[string]]::new()
        snapshotConfigEnableAttempted = $false
        snapshotConfigEnabled = $false
        sessionStartAttempted = $false
        sessionStarted = $false
        sessionStopAttempted = $false
        sessionStopped = $false
        snapshotConfigDisableAttempted = $false
        snapshotConfigDisabled = $false
        cleanupCompleted = $false
        noActiveWprSessionAtEnd = $null
        traceSha256 = $null
        traceSizeBytes = $null
        createdAtUtc = Get-P917HeapWorkerUtc
        completedAtUtc = $null
    }

    $initialStatus = Invoke-P917HeapWprCommand $state @('-status') ([int]$config.wprCommandTimeoutSeconds)
    if ($null -eq $initialStatus.exitCode -or [int]$initialStatus.exitCode -ne 0 -or [bool]$initialStatus.timedOut) { throw 'Could not query WPR status before starting the P9-17 heap snapshot diagnostic.' }
    if (Test-P917HeapWprRecording $initialStatus) { throw 'WPR already has an active recording; refusing to modify or stop another session.' }

    $state.snapshotConfigEnableAttempted = $true
    $enable = Invoke-P917HeapWprCommand $state @('-snapshotconfig', 'heap', '-pid', [string]$config.processId, 'enable') ([int]$config.wprCommandTimeoutSeconds)
    if ($null -eq $enable.exitCode -or [int]$enable.exitCode -ne 0 -or [bool]$enable.timedOut) { throw "WPR PID-scoped Heap snapshot configuration failed: $($enable.stderr) $($enable.stdout)" }
    $state.snapshotConfigEnabled = $true

    $state.sessionStartAttempted = $true
    $start = Invoke-P917HeapWprCommand $state @('-start', 'HeapSnapshot', '-filemode', '-instancename', [string]$config.wprInstanceName) ([int]$config.wprCommandTimeoutSeconds)
    if ($null -eq $start.exitCode -or [int]$start.exitCode -ne 0 -or [bool]$start.timedOut) { throw "WPR HeapSnapshot profile did not start: $($start.stderr) $($start.stdout)" }
    $state.sessionStarted = $true
    $state.status = 'recording'
    $status = Invoke-P917HeapWprCommand $state @('-status', '-instancename', [string]$config.wprInstanceName) ([int]$config.wprCommandTimeoutSeconds)
    if (-not (Test-P917HeapWprRecording $status)) { throw 'WPR did not confirm the named HeapSnapshot recording session.' }

    $ready = [ordered]@{
        schema = 'classmngr-phase9-p9-17-heap-worker-ready-v1'
        runId = [string]$config.runId
        nonce = [string]$config.nonce
        status = 'ready'
        diagnosticOnly = $true
        accepted = $false
        workerProcessId = [Int64]$PID
        workerImagePath = $workerImagePath
        workerImageSha256 = $workerImageSha256
        executionContext = $context
        processId = [Int64]$config.processId
        processImagePath = $targetImagePath
        processImageSha256 = [string]$config.processImageSha256
        wprPath = [string]$config.wprPath
        wprSha256 = [string]$config.wprSha256
        wprInstanceName = [string]$config.wprInstanceName
        activationAuthorizationSha256 = [string]$authorizationSha256
        authorizedAtUtc = [string]$authorization.authorizedAtUtc
        readyAtUtc = Get-P917HeapWorkerUtc
    }
    Write-P917HeapWorkerJsonAtomic ([string]$config.readyStatusPath) $ready
    $sessionDeadline = [DateTime]::UtcNow.AddSeconds([int]$config.sessionWatchdogSeconds)
    $nextSequence = 1
    $stopRequested = $false
    while (-not $stopRequested) {
        if ([DateTime]::UtcNow -ge $sessionDeadline) { throw 'Elevated WPR worker session watchdog expired; stopping its named trace.' }
        $requestPath = Join-Path ([string]$config.requestRoot) ('request-{0:D4}.json' -f $nextSequence)
        if (-not (Test-Path -LiteralPath $requestPath -PathType Leaf)) { Start-Sleep -Milliseconds 100; continue }
        $request = Get-Content -LiteralPath $requestPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ([string]$request.schema -cne 'classmngr-phase9-p9-17-heap-worker-request-v1' -or
            [string]$request.runId -cne [string]$config.runId -or
            [string]$request.nonce -cne [string]$config.nonce -or
            [int]$request.sequence -ne $nextSequence) {
            throw 'Heap worker request failed its schema, nonce, run, or sequence binding.'
        }
        $requestStartedAtUtc = Get-P917HeapWorkerUtc
        if ([string]$request.command -ceq 'snapshot') {
            try {
                $snapshot = Add-P917HeapWorkerSnapshot $state $request
                $result = [ordered]@{ status = 'captured'; snapshot = $snapshot; requestStartedAtUtc = $requestStartedAtUtc }
            } catch {
                $result = [ordered]@{ status = 'failed'; failure = $_.Exception.Message; requestStartedAtUtc = $requestStartedAtUtc }
            }
            $null = Write-P917HeapWorkerResponse $state $request $result
        } elseif ([string]$request.command -ceq 'stop') {
            Stop-P917HeapWorkerTrace $state
            $stopResult = [ordered]@{
                status = [string]$state.status
                tracePath = [string]$config.tracePath
                traceSha256 = $state.traceSha256
                traceSizeBytes = $state.traceSizeBytes
                sessionStopped = [bool]$state.sessionStopped
                snapshotConfigDisabled = [bool]$state.snapshotConfigDisabled
                noActiveWprSessionAtEnd = $state.noActiveWprSessionAtEnd
                cleanupCompleted = [bool]$state.cleanupCompleted
                cleanupErrors = $state.cleanupErrors.ToArray()
                snapshotsCaptured = $state.snapshots.Count
                requestStartedAtUtc = $requestStartedAtUtc
            }
            $null = Write-P917HeapWorkerResponse $state $request $stopResult
            $stopRequested = $true
        } else {
            throw 'Heap worker supports only approved snapshot and stop requests.'
        }
        $nextSequence++
    }
} catch {
    $failureMessage = $_.Exception.Message
} finally {
    if ($null -ne $state) {
        try { $null = Write-P917HeapWorkerFinal $state $failureMessage }
        catch { $failureMessage = if ($null -eq $failureMessage) { $_.Exception.Message } else { $failureMessage + '; finalization: ' + $_.Exception.Message } }
    } elseif (-not [string]::IsNullOrWhiteSpace($failureMessage)) {
        try {
            $config = Get-Content -LiteralPath $ControlConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json
            $failure = [ordered]@{
                schema = 'classmngr-phase9-p9-17-heap-worker-final-v1'; runId = [string]$config.runId; nonce = [string]$config.nonce
                status = 'failed'; diagnosticOnly = $true; accepted = $false; workerProcessId = [Int64]$PID
                executionContext = if ($null -ne $context) { $context } else { $null }
                cleanupCompleted = $false; cleanupErrors = @(); failure = $failureMessage; completedAtUtc = Get-P917HeapWorkerUtc
            }
            Write-P917HeapWorkerJsonAtomic ([string]$config.finalStatusPath) $failure
        } catch { }
    }
}

if (-not [string]::IsNullOrWhiteSpace($failureMessage)) { exit 1 }
exit 0
