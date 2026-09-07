[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$StageDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateSet('x64', 'Win32')]
    [string]$Platform,

    [string]$ReportPath = '',

    [ValidateRange(1, 10)]
    [int]$Iterations = 3,

    [string[]]$ScenarioArguments = @(),

    [ValidateRange(1000, 120000)]
    [int]$WindowTimeoutMilliseconds = 30000,

    [ValidateRange(0, 600000)]
    [int]$SettleMilliseconds = 1000,

    [ValidateRange(100, 10000)]
    [int]$ResizeWidth = 1280,

    [ValidateRange(100, 10000)]
    [int]$ResizeHeight = 800,

    [switch]$PlanOnly
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

$stagePath = Get-AbsolutePath -Path $StageDirectory
$executablePath = Join-Path -Path $stagePath -ChildPath 'ClassMngrWinUI.exe'
if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $resolvedReportPath = Join-Path -Path $stagePath -ChildPath "phase5-winui-$Platform.json"
}
else {
    $resolvedReportPath = Get-AbsolutePath -Path $ReportPath
}

$plan = [ordered]@{
    format = 'classmngr-winui-phase5-measurement-plan-v1'
    platform = $Platform
    stageDirectory = $stagePath
    executable = $executablePath
    reportPath = $resolvedReportPath
    iterations = $Iterations
    categories = @('cold (first run)', 'warm (remaining runs)')
    scenarioArguments = @($ScenarioArguments)
    windowTimeoutMilliseconds = $WindowTimeoutMilliseconds
    settleMilliseconds = $SettleMilliseconds
    resizeRequested = [ordered]@{
        width = $ResizeWidth
        height = $ResizeHeight
    }
    steps = @(
        'launch-process',
        'wait-for-visible-process-window',
        'record-launch-to-window-visible-and-scenario-ready-proxy',
        'wait-for-settle',
        'collect-memory-and-handle-sample',
        'move-and-resize-window',
        'request-close-and-verify-clean-exit',
        'verify-window-release'
    )
    targetNotes = @(
        'The only evaluated target is the shared steady-state working-set target of 200 MiB when at least one numeric working-set sample exists.',
        'First-navigation readiness is represented by the requested-arguments visible-window event as a proxy; no first-navigation threshold is fabricated.',
        'Resize latency is reported for comparison with the Phase 0 provisional p95 target of 32 ms; this helper does not assert a resize pass/fail result.',
        'No startup or handle-count pass/fail budget is supplied by this helper.'
    )
}

if ($PlanOnly) {
    $plan | ConvertTo-Json -Depth 12
    return
}

if (-not (Test-Path -LiteralPath $stagePath -PathType Container)) {
    throw "WinUI stage directory was not found: $stagePath"
}
if (-not (Test-Path -LiteralPath $executablePath -PathType Leaf)) {
    throw "WinUI executable was not found: $executablePath"
}
if (Test-Path -LiteralPath $resolvedReportPath) {
    throw "Refusing to overwrite Phase 5 measurement report: $resolvedReportPath"
}

if (-not ('ClassMngrWinUIPhase5.NativeMethods' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace ClassMngrWinUIPhase5
{
    public static class NativeMethods
    {
        [StructLayout(LayoutKind.Sequential)]
        public struct Rect
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        private delegate bool EnumWindowsCallback(IntPtr window, IntPtr parameter);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool EnumWindows(
            EnumWindowsCallback callback,
            IntPtr parameter
            );

        [DllImport("user32.dll", SetLastError = true)]
        private static extern uint GetWindowThreadProcessId(
            IntPtr window,
            out uint processId
            );

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool GetWindowRect(
            IntPtr window,
            out Rect rectangle
            );

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool MoveWindow(
            IntPtr window,
            int x,
            int y,
            int width,
            int height,
            bool repaint
            );

        [DllImport("user32.dll")]
        public static extern bool IsWindow(IntPtr window);

        [DllImport("user32.dll")]
        public static extern bool IsWindowVisible(IntPtr window);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool PostMessage(
            IntPtr window,
            uint message,
            IntPtr wParam,
            IntPtr lParam
            );

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool GetProcessHandleCount(
            IntPtr processHandle,
            out uint handleCount
            );

        public static IntPtr FindVisibleProcessWindow(int processId)
        {
            IntPtr found = IntPtr.Zero;
            EnumWindows(
                (window, parameter) =>
                {
                    if (!IsWindowVisible(window))
                    {
                        return true;
                    }

                    uint candidateProcessId;
                    GetWindowThreadProcessId(window, out candidateProcessId);
                    if (candidateProcessId != processId)
                    {
                        return true;
                    }

                    found = window;
                    return false;
                },
                IntPtr.Zero
                );
            return found;
        }
    }
}
'@
}

function Wait-ForVisibleProcessWindow {
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process]$Process,

        [Parameter(Mandatory = $true)]
        [int]$TimeoutMilliseconds
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "WinUI process exited before a visible window appeared (exit code $($Process.ExitCode))."
        }

        $window = [ClassMngrWinUIPhase5.NativeMethods]::FindVisibleProcessWindow($Process.Id)
        if ($window -ne [IntPtr]::Zero) {
            return $window
        }
        Start-Sleep -Milliseconds 50
    }

    throw "Timed out waiting for a visible WinUI process window ($TimeoutMilliseconds ms)."
}

function Wait-ForWindowRelease {
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Window,

        [Parameter(Mandatory = $true)]
        [int]$TimeoutMilliseconds
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (-not [ClassMngrWinUIPhase5.NativeMethods]::IsWindow($Window)) {
            return $true
        }
        Start-Sleep -Milliseconds 50
    }

    return -not [ClassMngrWinUIPhase5.NativeMethods]::IsWindow($Window)
}

function Get-WindowBounds {
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Window
    )

    $rectangle = [ClassMngrWinUIPhase5.NativeMethods+Rect]::new()
    if (-not [ClassMngrWinUIPhase5.NativeMethods]::GetWindowRect(
            $Window,
            [ref]$rectangle
            )) {
        throw "Could not read the bounds of window $Window."
    }

    return [ordered]@{
        left = $rectangle.Left
        top = $rectangle.Top
        right = $rectangle.Right
        bottom = $rectangle.Bottom
        width = $rectangle.Right - $rectangle.Left
        height = $rectangle.Bottom - $rectangle.Top
    }
}

function Get-HandleCount {
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process]$Process
    )

    try {
        $count = [uint32]0
        if ([ClassMngrWinUIPhase5.NativeMethods]::GetProcessHandleCount(
                $Process.Handle,
                [ref]$count
                )) {
            return [int64]$count
        }
    }
    catch {
    }

    return $null
}

function Get-NumberSummary {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Values
    )

    $numericValues = @($Values | ForEach-Object { [double]$_ } | Sort-Object)
    if ($numericValues.Count -eq 0) {
        return [ordered]@{
            sampleCount = 0
            minimum = $null
            median = $null
            maximum = $null
        }
    }

    $middle = [int][Math]::Floor($numericValues.Count / 2)
    if (($numericValues.Count % 2) -eq 1) {
        $median = $numericValues[$middle]
    }
    else {
        $median = ($numericValues[$middle - 1] + $numericValues[$middle]) / 2
    }

    return [ordered]@{
        sampleCount = $numericValues.Count
        minimum = [Math]::Round($numericValues[0], 1)
        median = [Math]::Round($median, 1)
        maximum = [Math]::Round($numericValues[$numericValues.Count - 1], 1)
    }
}

$closeTimeoutMilliseconds = [Math]::Max(5000, $WindowTimeoutMilliseconds)
$runs = [System.Collections.Generic.List[object]]::new()

for ($iteration = 1; $iteration -le $Iterations; $iteration++) {
    $process = $null
    $window = [IntPtr]::Zero
    $processExited = $false
    $windowReleased = $false
    $forcedTermination = $false
    $closeRequested = $false
    $failureMessage = $null
    $run = [ordered]@{
        iteration = $iteration
        category = if ($iteration -eq 1) { 'cold' } else { 'warm' }
        arguments = @($ScenarioArguments)
        launchToWindowVisibleMs = $null
        scenarioReadyProxyMs = $null
        scenarioReadyProxyMethod = 'same visible-window event after requested arguments; proxy only'
        workingSetMiB = $null
        privateMemoryMiB = $null
        peakWorkingSetMiB = $null
        handleCount = $null
        initialBounds = $null
        resizeRequested = [ordered]@{
            width = $ResizeWidth
            height = $ResizeHeight
        }
        resizeLatencyMs = $null
        resultingBounds = $null
        resizeApplied = $false
        closeRequested = $false
        cleanClose = $false
        forcedTermination = $false
        windowReleased = $false
        processExited = $false
        status = 'failed'
        failure = $null
    }

    try {
        $launchTimer = [System.Diagnostics.Stopwatch]::StartNew()
        $startParameters = @{
            FilePath = $executablePath
            WorkingDirectory = $stagePath
            PassThru = $true
            WindowStyle = 'Normal'
        }
        if (@($ScenarioArguments).Count -gt 0) {
            $startParameters.ArgumentList = @($ScenarioArguments)
        }

        $process = Start-Process @startParameters
        $launchTimer.Stop()
        $processStartUtc = $process.StartTime.ToUniversalTime()
        $window = Wait-ForVisibleProcessWindow `
            -Process $process `
            -TimeoutMilliseconds $WindowTimeoutMilliseconds
        $visibleUtc = [DateTime]::UtcNow
        $visibleMilliseconds = [Math]::Max(
            0,
            [Math]::Round(($visibleUtc - $processStartUtc).TotalMilliseconds, 1)
            )
        $run.launchToWindowVisibleMs = $visibleMilliseconds
        $run.scenarioReadyProxyMs = $visibleMilliseconds

        if ($SettleMilliseconds -gt 0) {
            Start-Sleep -Milliseconds $SettleMilliseconds
        }

        $process.Refresh()
        $run.workingSetMiB = [Math]::Round($process.WorkingSet64 / 1MB, 2)
        $run.privateMemoryMiB = [Math]::Round($process.PrivateMemorySize64 / 1MB, 2)
        $run.peakWorkingSetMiB = [Math]::Round($process.PeakWorkingSet64 / 1MB, 2)
        $run.handleCount = Get-HandleCount -Process $process

        $run.initialBounds = Get-WindowBounds -Window $window
        $resizeTimer = [System.Diagnostics.Stopwatch]::StartNew()
        $resizeSucceeded = [ClassMngrWinUIPhase5.NativeMethods]::MoveWindow(
                $window,
                [int]$run.initialBounds.left,
                [int]$run.initialBounds.top,
                $ResizeWidth,
                $ResizeHeight,
                $true
                )
        $resizeTimer.Stop()
        $run.resizeLatencyMs = [Math]::Round($resizeTimer.Elapsed.TotalMilliseconds, 1)
        if (-not $resizeSucceeded) {
            throw "Could not move or resize window $window."
        }
        $run.resultingBounds = Get-WindowBounds -Window $window
        $run.resizeApplied = $true
        $run.status = 'measured'
    }
    catch {
        $failureMessage = $_.Exception.Message
    }
    finally {
        if ($null -ne $process) {
            try {
                $process.Refresh()
                if (-not $process.HasExited -and $window -ne [IntPtr]::Zero) {
                    $closeRequested = [ClassMngrWinUIPhase5.NativeMethods]::PostMessage(
                        $window,
                        0x0010,
                        [IntPtr]::Zero,
                        [IntPtr]::Zero
                        )
                }
                if (-not $closeRequested -and -not $process.HasExited) {
                    $closeRequested = $process.CloseMainWindow()
                }
                if (-not $process.HasExited) {
                    if (-not $process.WaitForExit($closeTimeoutMilliseconds)) {
                        $forcedTermination = $true
                        $process.Kill()
                        $process.WaitForExit($closeTimeoutMilliseconds) | Out-Null
                    }
                }
                $process.Refresh()
                $processExited = $process.HasExited
            }
            catch {
                if ($null -eq $failureMessage) {
                    $failureMessage = "Process close or exit verification failed: $($_.Exception.Message)"
                }
            }
        }

        if ($window -ne [IntPtr]::Zero) {
            $windowReleased = Wait-ForWindowRelease `
                -Window $window `
                -TimeoutMilliseconds $closeTimeoutMilliseconds
        }

        $run.closeRequested = [bool]$closeRequested
        $run.cleanClose = [bool]($processExited -and -not $forcedTermination -and $windowReleased)
        $run.forcedTermination = [bool]$forcedTermination
        $run.windowReleased = [bool]$windowReleased
        $run.processExited = [bool]$processExited
        if ($null -ne $failureMessage) {
            $run.failure = $failureMessage
            $run.status = 'failed'
        }
        elseif (-not $run.cleanClose) {
            $run.failure = 'Process or window did not close cleanly.'
            $run.status = 'failed'
        }
    }

    $runs.Add($run) | Out-Null
}

$coldRuns = @($runs | Where-Object { $_.category -eq 'cold' })
$warmRuns = @($runs | Where-Object { $_.category -eq 'warm' })
$successfulRuns = @($runs | Where-Object { $_.status -eq 'measured' })
$failedRuns = @($runs | Where-Object { $_.status -eq 'failed' })

$coldStartup = @( $coldRuns | Where-Object { $null -ne $_.launchToWindowVisibleMs } |
    ForEach-Object { $_.launchToWindowVisibleMs } )
$warmStartup = @( $warmRuns | Where-Object { $null -ne $_.launchToWindowVisibleMs } |
    ForEach-Object { $_.launchToWindowVisibleMs } )
$scenarioReady = @( $runs | Where-Object { $null -ne $_.scenarioReadyProxyMs } |
    ForEach-Object { $_.scenarioReadyProxyMs } )
$workingSet = @( $runs | Where-Object { $null -ne $_.workingSetMiB } |
    ForEach-Object { $_.workingSetMiB } )
$privateMemory = @( $runs | Where-Object { $null -ne $_.privateMemoryMiB } |
    ForEach-Object { $_.privateMemoryMiB } )
$peakWorkingSet = @( $runs | Where-Object { $null -ne $_.peakWorkingSetMiB } |
    ForEach-Object { $_.peakWorkingSetMiB } )
$handles = @( $runs | Where-Object { $null -ne $_.handleCount } |
    ForEach-Object { $_.handleCount } )
$resizeLatency = @( $runs | Where-Object { $null -ne $_.resizeLatencyMs } |
    ForEach-Object { $_.resizeLatencyMs } )

$withinKnownTargets = [bool]($workingSet.Count -gt 0 -and
    (($workingSet | Measure-Object -Maximum).Maximum -le 200))
$reportDirectory = Split-Path -Parent $resolvedReportPath
if (-not (Test-Path -LiteralPath $reportDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
}
if (Test-Path -LiteralPath $resolvedReportPath) {
    throw "Refusing to overwrite Phase 5 measurement report: $resolvedReportPath"
}

$report = [ordered]@{
    format = 'classmngr-winui-phase5-measurements-v1'
    measuredAtUtc = [DateTime]::UtcNow.ToString('o')
    platform = $Platform
    stageDirectory = $stagePath
    executable = $executablePath
    iterations = $Iterations
    scenarioArguments = @($ScenarioArguments)
    windowTimeoutMilliseconds = $WindowTimeoutMilliseconds
    settleMilliseconds = $SettleMilliseconds
    resizeRequested = [ordered]@{
        width = $ResizeWidth
        height = $ResizeHeight
    }
    targetNotes = $plan.targetNotes
    withinKnownTargets = $withinKnownTargets
    runs = @($runs.ToArray())
    summary = [ordered]@{
        requestedRuns = $Iterations
        completedRuns = $runs.Count
        successfulRuns = $successfulRuns.Count
        failedRuns = $failedRuns.Count
        coldRuns = $coldRuns.Count
        warmRuns = $warmRuns.Count
        launchToWindowVisibleMs = [ordered]@{
            cold = Get-NumberSummary -Values $coldStartup
            warm = Get-NumberSummary -Values $warmStartup
        }
        scenarioReadyProxyMs = Get-NumberSummary -Values $scenarioReady
        workingSetMiB = Get-NumberSummary -Values $workingSet
        privateMemoryMiB = Get-NumberSummary -Values $privateMemory
        peakWorkingSetMiB = Get-NumberSummary -Values $peakWorkingSet
        handleCount = Get-NumberSummary -Values $handles
        resizeLatencyMs = Get-NumberSummary -Values $resizeLatency
        knownTargetEvaluation = [ordered]@{
            workingSetTargetMiB = 200
            numericWorkingSetSampleCount = $workingSet.Count
            evaluated = ($workingSet.Count -gt 0)
            withinTarget = $withinKnownTargets
            note = 'This is the only target evaluation; startup, first-navigation, resize, and handle-count pass/fail claims are intentionally omitted.'
        }
    }
}

$json = $report | ConvertTo-Json -Depth 16
[System.IO.File]::WriteAllText(
    $resolvedReportPath,
    $json + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false)
    )

Write-Host ("WinUI Phase 5 measurement complete: {0} run(s), {1} failed; report: {2}" -f `
    $runs.Count,
    $failedRuns.Count,
    $resolvedReportPath
    )
