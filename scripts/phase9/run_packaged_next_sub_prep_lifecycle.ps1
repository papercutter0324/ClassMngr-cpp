<#
.SYNOPSIS
Runs three source-bound local Windows Release Sub Prep lifecycle processes.

.DESCRIPTION
Builds and stages the optimized packaged Next app, generates the deterministic
96-class workspace fixture, and measures actual UIA-driven Sub Prep lifecycle,
package generation, release, repeated navigation, and five-minute idle.
Every invocation writes a unique evidence directory and preserves the package,
fixture copies, output packages, traces, and failed attempts.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Join-Path $PSScriptRoot "..\.."),
    [string]$BuildDirectory = "build\windows-x64-release",
    [string]$QtPrefix = "",
    [string]$PreparedReleaseEvidenceDirectory = "",
    [switch]$RouteSmokeOnly,
    [switch]$RetentionDiagnosticOnly,
    [ValidateRange(2, 30)][int]$DiagnosticTotalCycles = 30,
    [string]$EvidenceRoot = "artifacts\phase9\packaged-next-sub-prep-lifecycle",
    [int]$ReadinessTimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$TaskId = "P9_08_PACKAGED_NEXT_SUBPREP_LIFECYCLE"
$ManifestSchema = "classmngr-phase9-packaged-next-sub-prep-lifecycle-v1"
$ExpectedInterFontResourceIds = @(
    "resource:assets/fonts/Inter-Regular.ttf",
    "resource:assets/fonts/Inter-Medium.ttf",
    "resource:assets/fonts/Inter-SemiBold.ttf",
    "resource:assets/fonts/Inter-Bold.ttf",
    "resource:assets/fonts/Inter-BoldItalic.ttf",
    "resource:assets/fonts/Inter-Black.ttf"
)
$ExpectedProcesses = 3
$RepeatedCycles = if ($RetentionDiagnosticOnly) { $DiagnosticTotalCycles - 1 } else { 10 }
$ThresholdBytes = [Int64]262144000
$IdleSeconds = 300
$IdleSampleCount = 301
$SampleIntervalSeconds = 1
$MaxEventSampleDelayMilliseconds = 2000
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
$PreparedReleaseEvidenceDirectory = if ([string]::IsNullOrWhiteSpace($PreparedReleaseEvidenceDirectory)) {
    ""
} elseif ([System.IO.Path]::IsPathRooted($PreparedReleaseEvidenceDirectory)) {
    [System.IO.Path]::GetFullPath($PreparedReleaseEvidenceDirectory)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot $PreparedReleaseEvidenceDirectory))
}
$RunId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + "-" + [Guid]::NewGuid().ToString("N")
$EvidenceDirectory = Join-Path $EvidenceRoot $RunId
if (Test-Path -LiteralPath $EvidenceDirectory) { throw "Refusing to overwrite evidence run: $EvidenceDirectory" }
[void][System.IO.Directory]::CreateDirectory($EvidenceDirectory)

$ManifestPath = Join-Path $EvidenceDirectory "manifest.json"
$SourceFilesPath = Join-Path $EvidenceDirectory "source-files.json"
$PackageFilesPath = Join-Path $EvidenceDirectory "package-files.json"
$StartupLogPath = Join-Path $EvidenceDirectory "startup-log.jsonl"
$ProcessMetricsPath = Join-Path $EvidenceDirectory "process-metrics.jsonl"
$NavigationMetricsPath = Join-Path $EvidenceDirectory "navigation-metrics.jsonl"
$UiaActionsPath = Join-Path $EvidenceDirectory "uia-actions.jsonl"
$GeneratedFilesPath = Join-Path $EvidenceDirectory "generated-files.json"
$RetentionDiagnosticPath = Join-Path $EvidenceDirectory "retention-diagnostic.json"
$BuildLogPath = Join-Path $EvidenceDirectory "cmake-build.log"
$SummaryPath = Join-Path $EvidenceDirectory "summary.json"
$ValidatorOutputPath = Join-Path $EvidenceDirectory "validator-output.txt"
$RunnerFailure = $null
$SourceAtStart = $null
$Manifest = [ordered]@{
    schema = $ManifestSchema
    taskId = $TaskId
    runId = $RunId
    status = "not-evaluated"
    startedAtUtc = [DateTimeOffset]::UtcNow.ToString("o")
    completedAtUtc = $null
    revision = $null
    repositoryRoot = $RepositoryRoot
    candidateSource = $null
    configuration = [ordered]@{}
    qtInstallation = $null
    application = $null
    package = $null
    fixture = $null
    thresholds = [ordered]@{
        normalUseWorkingSetBytesExclusive = $ThresholdBytes
        generationPeakWorkingSetBytesAdvisory = [Int64]536870912
        generationPeakMetric = "process-lifetime peak working set at package-generation completion"
        idleSeconds = $IdleSeconds
        idleSampleCount = $IdleSampleCount
        sampleIntervalSeconds = $SampleIntervalSeconds
        strictAtOrAboveFails = $true
    }
    repeatedSubPrepCycles = $RepeatedCycles
    diagnostic = if ($RetentionDiagnosticOnly) { [ordered]@{ mode = "representative-retention-trace"; totalCyclesIncludingGeneration = $DiagnosticTotalCycles; budgetEvaluation = "not-evaluated" } } else { $null }
    processRuns = @()
    files = [ordered]@{
        sourceInventory = "source-files.json"
        packageInventory = "package-files.json"
        startupLog = "startup-log.jsonl"
        processMetrics = "process-metrics.jsonl"
        navigationMetrics = "navigation-metrics.jsonl"
        uiaActions = "uia-actions.jsonl"
        generatedFiles = "generated-files.json"
        buildLog = "cmake-build.log"
    }
    failure = $null
}
$script:RunnerIntegrityLevel = if ([Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { "elevated" } else { "standard" }
$script:PreparedRelease = $null
$script:PreparedSourceFiles = @()
$script:PreparedPackageFiles = @()
$script:PreparedPackageDirectory = ""
$script:PreparedFixturePath = ""
$script:PreparedBuildLogPath = ""
$script:ReuseSourceDifferences = @()
if ($RouteSmokeOnly) { $Manifest.files.routeSmoke = "route-smoke.json" }
if ($RetentionDiagnosticOnly) {
    if ($RouteSmokeOnly) { throw "RouteSmokeOnly and RetentionDiagnosticOnly are mutually exclusive." }
    $Manifest.files.retentionDiagnostic = "retention-diagnostic.json"
}

Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

public sealed class P908PointerResult
{
    public long windowHandle;
    public uint expectedProcessId;
    public uint windowProcessId;
    public uint targetThreadId;
    public uint callerThreadId;
    public uint priorForegroundThreadId;
    public bool allowSetForegroundWindowReturned;
    public uint waitForInputIdleResult;
    public bool showWindowPreviouslyVisible;
    public bool targetThreadInputAttached;
    public bool foregroundThreadInputAttached;
    public uint foregroundThreadInputAttachErrorCode;
    public bool bringWindowToTopReturned;
    public bool setActiveWindowReturnedNonZero;
    public bool setFocusReturnedNonZero;
    public bool setForegroundWindowReturned;
    public bool foregroundVerified;
    public long foregroundWindowHandle;
    public uint foregroundProcessId;
    public int screenX;
    public int screenY;
    public uint inputEventsSent;
}

public sealed class P908WindowMessageResult
{
    public long windowHandle;
    public uint expectedProcessId;
    public uint windowProcessId;
    public int screenX;
    public int screenY;
    public int clientX;
    public int clientY;
    public int clientLeft;
    public int clientTop;
    public int clientRight;
    public int clientBottom;
    public uint waitForInputIdleResult;
    public bool waitForInputIdleSucceeded;
    public bool mouseMoveSent;
    public bool leftButtonDownSent;
    public bool leftButtonUpSent;
    public uint messagesSent;
}

public sealed class P908ScrollMessageResult
{
    public long windowHandle;
    public uint expectedProcessId;
    public uint windowProcessId;
    public int screenX;
    public int screenY;
    public int clientX;
    public int clientY;
    public int clientLeft;
    public int clientTop;
    public int clientRight;
    public int clientBottom;
    public int listLeft;
    public int listTop;
    public int listRight;
    public int listBottom;
    public int wheelDelta;
    public uint waitForInputIdleResult;
    public bool waitForInputIdleSucceeded;
    public bool messageSent;
}

public static class P908Native
{
    private const ushort ImageFileMachineUnknown = 0x0000;
    private const ushort ImageFileMachineAmd64 = 0x8664;
    private const int SwRestore = 9;
    private const uint InputMouse = 0;
    private const uint LeftDown = 0x0002;
    private const uint LeftUp = 0x0004;
    private const uint WmMouseMove = 0x0200;
    private const uint WmLeftButtonDown = 0x0201;
    private const uint WmLeftButtonUp = 0x0202;
    private const uint WmMouseWheel = 0x020A;
    private const uint MouseKeyLeftButton = 0x0001;
    private const uint SmtoAbortIfHung = 0x0002;

    [StructLayout(LayoutKind.Sequential)]
    private struct Point { public int x; public int y; }
    [StructLayout(LayoutKind.Sequential)]
    private struct Rect { public int left; public int top; public int right; public int bottom; }

    [StructLayout(LayoutKind.Sequential)]
    private struct MouseInput
    {
        public int dx;
        public int dy;
        public uint mouseData;
        public uint flags;
        public uint time;
        public UIntPtr extraInfo;
    }
    [StructLayout(LayoutKind.Sequential)]
    private struct Input
    {
        public uint type;
        public MouseInput mouseInput;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool IsWow64Process2(IntPtr process, out ushort processMachine, out ushort nativeMachine);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool AllowSetForegroundWindow(uint processId);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool BringWindowToTop(IntPtr window);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr SetActiveWindow(IntPtr window);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr SetFocus(IntPtr window);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool AttachThreadInput(uint from, uint to, bool attach);
    [DllImport("kernel32.dll")]
    private static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")]
    private static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool ScreenToClient(IntPtr window, ref Point point);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool GetClientRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll", EntryPoint = "SendMessageTimeoutW", SetLastError = true)]
    private static extern IntPtr SendMessageTimeoutW(IntPtr window, uint message, UIntPtr wParam,
        IntPtr lParam, uint flags, uint timeoutMilliseconds, out IntPtr result);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint SendInput(uint count, Input[] inputs, int size);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool QueryFullProcessImageNameW(IntPtr process, uint flags,
        StringBuilder imageName, ref int size);

    public static bool IsNativeX64(IntPtr process)
    {
        ushort processMachine;
        ushort nativeMachine;
        if (!IsWow64Process2(process, out processMachine, out nativeMachine))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "IsWow64Process2 failed.");
        return nativeMachine == ImageFileMachineAmd64 && processMachine == ImageFileMachineUnknown;
    }
    public static string GetProcessImagePath(IntPtr process)
    {
        StringBuilder path = new StringBuilder(32768);
        int size = path.Capacity;
        if (!QueryFullProcessImageNameW(process, 0, path, ref size))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "QueryFullProcessImageName failed.");
        return path.ToString();
    }
    public static uint GetWindowProcessId(IntPtr window)
    {
        uint processId;
        if (GetWindowThreadProcessId(window, out processId) == 0)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowThreadProcessId failed.");
        return processId;
    }
    public static P908PointerResult ClickWindowPoint(IntPtr window, IntPtr processHandle,
        int x, int y, uint expectedProcessId)
    {
        P908PointerResult result = new P908PointerResult();
        result.windowHandle = window.ToInt64();
        result.expectedProcessId = expectedProcessId;
        result.windowProcessId = GetWindowProcessId(window);
        if (result.windowProcessId != expectedProcessId)
            throw new InvalidOperationException("The clicked HWND does not belong to the measured app PID.");
        result.allowSetForegroundWindowReturned = AllowSetForegroundWindow(expectedProcessId);
        result.waitForInputIdleResult = WaitForInputIdle(processHandle, 10000);
        if (result.waitForInputIdleResult != 0)
            throw new InvalidOperationException("The packaged app did not reach input-idle before UIA click.");
        result.showWindowPreviouslyVisible = ShowWindow(window, SwRestore);
        uint targetThread = GetWindowThreadProcessId(window, out result.windowProcessId);
        uint callerThread = GetCurrentThreadId();
        IntPtr priorForeground = GetForegroundWindow();
        uint priorForegroundPid;
        uint foregroundThread = priorForeground == IntPtr.Zero ? 0 : GetWindowThreadProcessId(priorForeground, out priorForegroundPid);
        result.targetThreadId = targetThread;
        result.callerThreadId = callerThread;
        result.priorForegroundThreadId = foregroundThread;
        bool targetAttached = false;
        bool foregroundAttached = false;
        try
        {
            if (targetThread != callerThread)
            {
                targetAttached = AttachThreadInput(callerThread, targetThread, true);
                if (!targetAttached) throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not attach UIA caller to target thread.");
            }
            if (foregroundThread != 0 && foregroundThread != callerThread && foregroundThread != targetThread)
            {
                foregroundAttached = AttachThreadInput(callerThread, foregroundThread, true);
                if (!foregroundAttached)
                    result.foregroundThreadInputAttachErrorCode = (uint)Marshal.GetLastWin32Error();
            }
            result.targetThreadInputAttached = targetAttached;
            result.foregroundThreadInputAttached = foregroundAttached;
            result.bringWindowToTopReturned = BringWindowToTop(window);
            result.setActiveWindowReturnedNonZero = SetActiveWindow(window) != IntPtr.Zero;
            result.setFocusReturnedNonZero = SetFocus(window) != IntPtr.Zero;
            result.setForegroundWindowReturned = SetForegroundWindow(window);
            System.Threading.Thread.Sleep(100);
            IntPtr foreground = GetForegroundWindow();
            result.foregroundWindowHandle = foreground.ToInt64();
            result.foregroundProcessId = foreground == IntPtr.Zero ? 0 : GetWindowProcessId(foreground);
            result.foregroundVerified = foreground == window && result.foregroundProcessId == expectedProcessId;
            if (!result.foregroundVerified)
                return result;
            if (!SetCursorPos(x, y)) throw new Win32Exception(Marshal.GetLastWin32Error(), "SetCursorPos failed.");
            Input[] input = new Input[2];
            input[0].type = InputMouse; input[0].mouseInput = new MouseInput { flags = LeftDown };
            input[1].type = InputMouse; input[1].mouseInput = new MouseInput { flags = LeftUp };
            result.inputEventsSent = SendInput(2, input, Marshal.SizeOf(typeof(Input)));
            if (result.inputEventsSent != 2)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "SendInput did not deliver the complete pointer click.");
            result.screenX = x; result.screenY = y;
            return result;
        }
        finally
        {
            if (foregroundAttached) AttachThreadInput(callerThread, foregroundThread, false);
            if (targetAttached) AttachThreadInput(callerThread, targetThread, false);
        }
    }
    private static IntPtr PackCoordinates(int x, int y)
    {
        int packed = unchecked((int)(((uint)(ushort)x) | ((uint)(ushort)y << 16)));
        return new IntPtr(packed);
    }
    private static void SendVerifiedMouseMessage(IntPtr window, uint message, uint keyState,
        IntPtr coordinates, out bool sent)
    {
        IntPtr ignoredResult;
        IntPtr callResult = SendMessageTimeoutW(window, message, new UIntPtr(keyState), coordinates,
            SmtoAbortIfHung, 2000, out ignoredResult);
        sent = callResult != IntPtr.Zero;
        if (!sent) throw new Win32Exception(Marshal.GetLastWin32Error(), "SendMessageTimeout did not deliver the requested click message.");
    }
    public static P908WindowMessageResult ClickWindowMessage(IntPtr window, IntPtr processHandle,
        int screenX, int screenY, uint expectedProcessId)
    {
        P908WindowMessageResult result = new P908WindowMessageResult();
        result.windowHandle = window.ToInt64();
        result.expectedProcessId = expectedProcessId;
        result.windowProcessId = GetWindowProcessId(window);
        result.screenX = screenX;
        result.screenY = screenY;
        if (result.windowProcessId != expectedProcessId)
            throw new InvalidOperationException("The selected control HWND does not belong to the measured app PID.");
        result.waitForInputIdleResult = WaitForInputIdle(processHandle, 10000);
        result.waitForInputIdleSucceeded = result.waitForInputIdleResult == 0;
        if (!result.waitForInputIdleSucceeded)
            throw new InvalidOperationException("Packaged app did not reach input-idle before window-message click.");
        Point point = new Point { x = screenX, y = screenY };
        if (!ScreenToClient(window, ref point))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "ScreenToClient failed for the UIA-selected control.");
        Rect rect;
        if (!GetClientRect(window, out rect))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetClientRect failed for the measured app window.");
        if (point.x < rect.left || point.x >= rect.right || point.y < rect.top || point.y >= rect.bottom)
            throw new InvalidOperationException("The UIA-selected control point is outside the measured app client bounds.");
        result.clientX = point.x;
        result.clientY = point.y;
        result.clientLeft = rect.left;
        result.clientTop = rect.top;
        result.clientRight = rect.right;
        result.clientBottom = rect.bottom;
        IntPtr coordinates = PackCoordinates(point.x, point.y);
        SendVerifiedMouseMessage(window, WmMouseMove, 0, coordinates, out result.mouseMoveSent);
        SendVerifiedMouseMessage(window, WmLeftButtonDown, MouseKeyLeftButton, coordinates, out result.leftButtonDownSent);
        SendVerifiedMouseMessage(window, WmLeftButtonUp, 0, coordinates, out result.leftButtonUpSent);
        result.messagesSent = 3;
        return result;
    }
    public static P908ScrollMessageResult ScrollListWindowMessage(IntPtr window,
        IntPtr processHandle, int listLeft, int listTop, int listRight, int listBottom,
        int wheelDelta, uint expectedProcessId)
    {
        P908ScrollMessageResult result = new P908ScrollMessageResult();
        result.windowHandle = window.ToInt64();
        result.expectedProcessId = expectedProcessId;
        result.windowProcessId = GetWindowProcessId(window);
        result.listLeft = listLeft;
        result.listTop = listTop;
        result.listRight = listRight;
        result.listBottom = listBottom;
        result.wheelDelta = wheelDelta;
        if (result.windowProcessId != expectedProcessId)
            throw new InvalidOperationException("The Sub Prep list scroll HWND does not belong to the measured app PID.");
        if (wheelDelta != -120 || listRight <= listLeft || listBottom <= listTop)
            throw new InvalidOperationException("The Sub Prep list scroll request has invalid bounds or wheel direction.");
        result.waitForInputIdleResult = WaitForInputIdle(processHandle, 10000);
        result.waitForInputIdleSucceeded = result.waitForInputIdleResult == 0;
        if (!result.waitForInputIdleSucceeded)
            throw new InvalidOperationException("Packaged app did not reach input-idle before list scroll.");
        result.screenX = listLeft + ((listRight - listLeft) / 2);
        result.screenY = listTop + ((listBottom - listTop) / 2);
        Point point = new Point { x = result.screenX, y = result.screenY };
        if (!ScreenToClient(window, ref point))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "ScreenToClient failed for the Sub Prep list scroll.");
        Rect rect;
        if (!GetClientRect(window, out rect))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetClientRect failed for the measured app window.");
        if (point.x < rect.left || point.x >= rect.right || point.y < rect.top || point.y >= rect.bottom)
            throw new InvalidOperationException("The Sub Prep list scroll point is outside the measured app client bounds.");
        result.clientX = point.x;
        result.clientY = point.y;
        result.clientLeft = rect.left;
        result.clientTop = rect.top;
        result.clientRight = rect.right;
        result.clientBottom = rect.bottom;
        uint wheelWord = unchecked((uint)(ushort)(short)wheelDelta) << 16;
        SendVerifiedMouseMessage(window, WmMouseWheel, wheelWord,
            PackCoordinates(result.screenX, result.screenY), out result.messageSent);
        return result;
    }
    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint WaitForInputIdle(IntPtr process, uint milliseconds);
}
'@

function Get-UtcTimestamp { [DateTimeOffset]::UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss.fff'Z'") }
function Get-FileSha256 { param([string]$Path) (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Get-StringSha256 {
    param([string]$Value)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value)))).Replace("-", "").ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Add-JsonLine {
    param([string]$Path, [object]$Value)
    [IO.File]::AppendAllText($Path, ($Value | ConvertTo-Json -Depth 32 -Compress) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
}
function Write-JsonFile {
    param([string]$Path, [object]$Value)
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 32), [Text.UTF8Encoding]::new($false))
}
function Sort-InventoryRowsOrdinal {
    param([object[]]$Rows)
    $ordered = [Collections.Generic.List[object]]::new()
    foreach ($row in $Rows) { $ordered.Add($row) }
    $ordered.Sort([Comparison[object]]{ param($left, $right) [StringComparer]::Ordinal.Compare(([string]$left.path).ToLowerInvariant(), ([string]$right.path).ToLowerInvariant()) })
    @($ordered.ToArray())
}
function Get-CandidateSourceIdentity {
    param([string]$Root, [string]$Revision)
    $includedRoots = @("CMakeLists.txt", "CMakePresets.json", "cmake", "docs/qt-rewrite/phase-4-resource-inventory.json", "licenses", "resources", "scripts/phase4", "scripts/phase9", "src", "tests", "third_party")
    $items = [Collections.Generic.List[object]]::new()
    foreach ($relativeRoot in $includedRoots) {
        $rootPath = Join-Path $Root $relativeRoot
        if (-not (Test-Path -LiteralPath $rootPath)) { throw "Source identity root is missing: $relativeRoot" }
        $files = if (Test-Path -LiteralPath $rootPath -PathType Leaf) { @(Get-Item -LiteralPath $rootPath) } else { @(Get-ChildItem -LiteralPath $rootPath -File -Recurse) }
        foreach ($file in @($files | Where-Object { $_.FullName -notmatch '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -and $_.Extension -notin @(".pyc", ".pyo") })) {
            $relative = [IO.Path]::GetRelativePath($Root, $file.FullName).Replace("\", "/")
            $items.Add([ordered]@{ path = $relative; sizeBytes = [Int64]$file.Length; sha256 = Get-FileSha256 $file.FullName })
        }
    }
    $sorted = Sort-InventoryRowsOrdinal @($items.ToArray())
    $canonical = @($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    [ordered]@{ revision = $Revision; sha256 = Get-StringSha256 ($canonical + "`n"); includedRoots = $includedRoots; fileCount = $sorted.Count; files = $sorted }
}
function Compare-PreparedSourceInventory {
    param([object[]]$CurrentFiles, [object[]]$PreparedFiles)
    $currentByPath = @{}
    foreach ($row in $CurrentFiles) { $currentByPath[[string]$row.path.ToLowerInvariant()] = $row }
    $preparedByPath = @{}
    foreach ($row in $PreparedFiles) { $preparedByPath[[string]$row.path.ToLowerInvariant()] = $row }
    [string[]]$keys = @($currentByPath.Keys + $preparedByPath.Keys | Sort-Object -Unique)
    [Array]::Sort($keys, [StringComparer]::Ordinal)
    $differences = [Collections.Generic.List[object]]::new()
    foreach ($key in $keys) {
        $current = $currentByPath[$key]
        $prepared = $preparedByPath[$key]
        if ($null -eq $current) { $difference = [pscustomobject]@{ path = [string]$prepared.path; change = "removed" } }
        elseif ($null -eq $prepared) { $difference = [pscustomobject]@{ path = [string]$current.path; change = "added" } }
        elseif ([Int64]$current.sizeBytes -ne [Int64]$prepared.sizeBytes -or [string]$current.sha256 -cne [string]$prepared.sha256) { $difference = [pscustomobject]@{ path = [string]$current.path; change = "changed" } }
        else { continue }
        if (-not ([string]$difference.path).StartsWith("scripts/phase9/", [StringComparison]::OrdinalIgnoreCase)) {
            throw "Prepared Release source differs outside scripts/phase9: $($difference.path) ($($difference.change))."
        }
        $differences.Add($difference)
    }
    @($differences.ToArray())
}
function Get-RequiredCacheValue {
    param([string[]]$Lines, [string]$Name)
    $matches = @($Lines | Where-Object { $_ -match "^$([regex]::Escape($Name)):[^=]+=" })
    if ($matches.Count -ne 1) { throw "Expected one CMake cache value $Name; found $($matches.Count)." }
    (($matches[0] -split "=", 2)[1])
}
function Test-OptimizedFlags {
    param([string]$Flags, [string]$Label)
    $optimizations = @([regex]::Matches($Flags, '(?i)(?:^|\s)[/-](?:O1|O2|Ox)(?=\s|$)'))
    if ($optimizations.Count -eq 0 -or $Flags -match '(?i)(?:^|\s)[/-]Od(?=\s|$)' -or $Flags -notmatch '(?i)(?:^|\s)[/-]D\s*NDEBUG(?=\s|$)') { throw "$Label must enable optimization and /DNDEBUG." }
    [ordered]@{ raw = $Flags; optimizationOptions = @($optimizations | ForEach-Object { $_.Value.Trim() }); definesNdebug = $true }
}
function Get-ReleaseCompilerConfiguration {
    param([string[]]$CacheLines, [string]$Root)
    $generator = Get-RequiredCacheValue $CacheLines "CMAKE_GENERATOR"
    if ($generator -notmatch '^Visual Studio') { throw "Expected the MSVC Visual Studio generator; found $generator." }
    $c = Test-OptimizedFlags (Get-RequiredCacheValue $CacheLines "CMAKE_C_FLAGS_RELEASE") "CMAKE_C_FLAGS_RELEASE"
    $cxx = Test-OptimizedFlags (Get-RequiredCacheValue $CacheLines "CMAKE_CXX_FLAGS_RELEASE") "CMAKE_CXX_FLAGS_RELEASE"
    $projectPath = Join-Path $Root "cmake\next_app\ClassMngrNext.vcxproj"
    $xml = [Xml.XmlDocument]::new(); $xml.Load($projectPath)
    $ns = [Xml.XmlNamespaceManager]::new($xml.NameTable); $ns.AddNamespace("msbuild", "http://schemas.microsoft.com/developer/msbuild/2003")
    $nodes = @($xml.SelectNodes("//msbuild:ItemDefinitionGroup", $ns) | Where-Object { $_.GetAttribute("Condition") -match "'\$\(Configuration\)\|\$\(Platform\)'\s*==\s*'Release\|x64'" } | ForEach-Object { $_.SelectSingleNode("msbuild:ClCompile/msbuild:Optimization", $ns) } | Where-Object { $null -ne $_ })
    if ($nodes.Count -ne 1 -or $nodes[0].InnerText.Trim() -notin @("MaxSpeed", "MinSpace")) { throw "Generated ClassMngrNext Release|x64 project has no enabled optimization setting." }
    [ordered]@{ compiler = "MSVC"; generator = $generator; configuration = "Release"; platform = "x64"; cFlagsRelease = $c; cxxFlagsRelease = $cxx; projectPath = [IO.Path]::GetFullPath($projectPath); projectSha256 = Get-FileSha256 $projectPath; projectOptimizationSetting = $nodes[0].InnerText.Trim() }
}
function Invoke-Checked {
    param([string]$Path, [string[]]$Arguments, [string]$Description)
    & $Path @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Description failed with exit code $LASTEXITCODE." }
}
function Get-QtVersion {
    param([string]$Path)
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
    $version = if ($info.ProductVersion) { $info.ProductVersion } else { $info.FileVersion }
    $match = [regex]::Match([string]$version, '\d+\.\d+\.\d+')
    if (-not $match.Success) { throw "Could not read Qt version from $Path." }
    $match.Value
}
function Get-PackagedQtRuntimeIdentity {
    param([string]$PackageRoot, [string]$QtPrefix)
    $files = [Collections.Generic.List[object]]::new()
    $qtDlls = @(Get-ChildItem -LiteralPath $PackageRoot -Filter "Qt6*.dll" -File | Sort-Object Name)
    foreach ($required in @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Sql.dll")) {
        if (-not ($qtDlls | Where-Object { $_.Name -ceq $required })) { throw "Packaged Qt runtime is missing $required." }
    }
    foreach ($dll in $qtDlls) {
        $source = Join-Path (Join-Path $QtPrefix "bin") $dll.Name
        if (-not (Test-Path -LiteralPath $source -PathType Leaf) -or (Get-FileSha256 $source) -cne (Get-FileSha256 $dll.FullName)) { throw "Packaged Qt DLL differs from the CMakeCache-bound Qt installation: $($dll.Name)." }
        $files.Add([ordered]@{ packagePath = $dll.Name; qtPrefixPath = [IO.Path]::GetFullPath($source); sizeBytes = [Int64]$dll.Length; sha256 = Get-FileSha256 $dll.FullName })
    }
    foreach ($plugin in @(@{ packagePath = "platforms/qwindows.dll"; sourcePath = "plugins/platforms/qwindows.dll" }, @{ packagePath = "sqldrivers/qsqlite.dll"; sourcePath = "plugins/sqldrivers/qsqlite.dll" })) {
        $packagePath = Join-Path $PackageRoot ($plugin.packagePath.Replace('/', '\\'))
        $sourcePath = Join-Path $QtPrefix ($plugin.sourcePath.Replace('/', '\\'))
        if (-not (Test-Path -LiteralPath $packagePath -PathType Leaf) -or -not (Test-Path -LiteralPath $sourcePath -PathType Leaf) -or (Get-FileSha256 $packagePath) -cne (Get-FileSha256 $sourcePath)) { throw "Packaged Qt plugin differs from the CMakeCache-bound Qt installation: $($plugin.packagePath)." }
        $files.Add([ordered]@{ packagePath = $plugin.packagePath; qtPrefixPath = [IO.Path]::GetFullPath($sourcePath); sizeBytes = [Int64](Get-Item -LiteralPath $packagePath).Length; sha256 = Get-FileSha256 $packagePath })
    }
    @($files)
}
function Get-PeArchitecture {
    param([string]$Path)
    $stream = [IO.File]::OpenRead($Path)
    try { $reader = [IO.BinaryReader]::new($stream); if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Bad MZ header: $Path" }; $stream.Position = 0x3c; $offset = $reader.ReadInt32(); $stream.Position = $offset; if ($reader.ReadUInt32() -ne 0x4550) { throw "Bad PE signature: $Path" }; switch ($reader.ReadUInt16()) { 0x8664 { "x64" } 0xaa64 { "arm64" } default { "unknown" } } }
    finally { $stream.Dispose() }
}
function Get-PackageInventory {
    param([string]$Root)
    $items = [Collections.Generic.List[object]]::new()
    foreach ($file in @(Get-ChildItem -LiteralPath $Root -File -Recurse)) {
        $relative = [IO.Path]::GetRelativePath($Root, $file.FullName).Replace("\", "/")
        $items.Add([ordered]@{ path = $relative; sizeBytes = [Int64]$file.Length; sha256 = Get-FileSha256 $file.FullName })
    }
    $sorted = Sort-InventoryRowsOrdinal @($items.ToArray())
    $canonical = @($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    [ordered]@{ sha256 = Get-StringSha256 ($canonical + "`n"); fileCount = $sorted.Count; files = $sorted }
}
function Invoke-FixtureGenerator {
    param([string]$Executable, [string]$OutputPath, [string]$QtBin)
    if (Test-Path -LiteralPath $OutputPath) { throw "Refusing to overwrite fixture $OutputPath." }
    $oldPath = [Environment]::GetEnvironmentVariable("PATH", "Process")
    $oldOutput = [Environment]::GetEnvironmentVariable("CLASSMNGR_LARGE_STARTUP_FIXTURE_OUTPUT_PATH", "Process")
    try {
        [Environment]::SetEnvironmentVariable("PATH", "$QtBin;$oldPath", "Process")
        [Environment]::SetEnvironmentVariable("CLASSMNGR_LARGE_STARTUP_FIXTURE_OUTPUT_PATH", $OutputPath, "Process")
        Push-Location (Split-Path -Parent $Executable)
        try { & $Executable largeStartupFixtureIsCompleteAndDeterministic; if ($LASTEXITCODE -ne 0) { throw "Representative fixture generator failed with exit code $LASTEXITCODE." } }
        finally { Pop-Location }
    } finally {
        [Environment]::SetEnvironmentVariable("PATH", $oldPath, "Process")
        [Environment]::SetEnvironmentVariable("CLASSMNGR_LARGE_STARTUP_FIXTURE_OUTPUT_PATH", $oldOutput, "Process")
    }
    if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) { throw "Fixture generator produced no output at $OutputPath." }
}

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
function Get-UiaDescendants {
    param([System.Windows.Automation.AutomationElement]$Root)
    $condition = [System.Windows.Automation.Condition]::TrueCondition
    $Root.FindAll([System.Windows.Automation.TreeScope]::Descendants, $condition)
}
function Get-UiaElementById {
    param([System.Windows.Automation.AutomationElement]$Root, [string]$AutomationId,
        [string]$ExpectedControlType = "")
    $matches = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach ($element in (Get-UiaDescendants $Root)) {
        $id = [string]$element.Current.AutomationId
        $idMatches = $id -ceq $AutomationId -or $id.EndsWith(".$AutomationId", [StringComparison]::Ordinal)
        $typeMatches = [string]::IsNullOrWhiteSpace($ExpectedControlType) -or
            $element.Current.ControlType.ProgrammaticName -ceq $ExpectedControlType
        if ($idMatches -and $typeMatches) { $matches.Add($element) }
    }
    if ($matches.Count -ne 1) {
        $candidateRows = @($matches | ForEach-Object {
            $current = $_.Current
            $rect = $current.BoundingRectangle
            [ordered]@{
                automationId = [string]$current.AutomationId
                controlType = [string]$current.ControlType.ProgrammaticName
                name = [string]$current.Name
                className = [string]$current.ClassName
                frameworkId = [string]$current.FrameworkId
                processId = [Int64]$current.ProcessId
                nativeWindowHandle = [Int64]$current.NativeWindowHandle
                isOffscreen = [bool]$current.IsOffscreen
                rectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height }
            }
        })
        Add-JsonLine $UiaActionsPath ([ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; actionKind = "uia-control-match-diagnostic"; requestedAutomationId = $AutomationId; candidateCount = $candidateRows.Count; candidates = $candidateRows })
        throw "UIA expected one control '$AutomationId', found $($matches.Count); exact candidates were saved in uia-actions.jsonl."
    }
    $matches[0]
}
function Get-MainWindow {
    param([Diagnostics.Process]$Process)
    $Process.Refresh()
    $handle = $Process.MainWindowHandle
    if ($handle -eq [IntPtr]::Zero -or -not [P908Native]::IsWindowVisible($handle) -or [P908Native]::GetWindowProcessId($handle) -ne [uint32]$Process.Id) { throw "The exact app process has no verified visible main window." }
    $window = [System.Windows.Automation.AutomationElement]::FromHandle($handle)
    if ($null -eq $window -or $window.Current.ProcessId -ne $Process.Id) { throw "UIA main window identity does not match the process." }
    $window
}
function Get-PointerElementIdentity {
    param([System.Windows.Automation.AutomationElement]$Element, [string]$ExpectedName, [Diagnostics.Process]$Process)
    $current = $Element.Current
    if ($current.ProcessId -ne $Process.Id -or $current.Name -cne $ExpectedName -or $current.IsOffscreen) { throw "UIA element name/PID/visibility failed exact identity check for '$ExpectedName'." }
    $rect = $current.BoundingRectangle
    if ($rect.Width -le 0 -or $rect.Height -le 0) { throw "UIA element '$ExpectedName' has no visible rectangle." }
    [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height }
}
function Invoke-VerifiedUiaControlClick {
    param([System.Windows.Automation.AutomationElement]$Element, [string]$ExpectedName,
        [IntPtr]$WindowHandle, [Diagnostics.Process]$Process, [string]$ActionKind,
        [string]$TargetId, [string]$FromPage, [string]$ToPage,
        [int]$ClickScreenX = -1, [int]$ClickScreenY = -1,
        [Collections.IDictionary]$CachedElementSnapshot = $null)
    if ($null -eq $CachedElementSnapshot) {
        $rectangle = Get-PointerElementIdentity $Element $ExpectedName $Process
        $observedName = [string]$Element.Current.Name
        $automationId = [string]$Element.Current.AutomationId
        $controlType = [string]$Element.Current.ControlType.ProgrammaticName
        $uiaProcessId = [Int64]$Element.Current.ProcessId
    } else {
        $rectangle = $CachedElementSnapshot.rectangle
        $observedName = [string]$CachedElementSnapshot.observedAccessibleName
        $automationId = [string]$CachedElementSnapshot.automationId
        $controlType = [string]$CachedElementSnapshot.controlType
        $uiaProcessId = [Int64]$CachedElementSnapshot.processId
        if ($CachedElementSnapshot.expectedAccessibleName -cne $ExpectedName -or
            $observedName -cne $ExpectedName -or $uiaProcessId -ne $Process.Id -or
            $CachedElementSnapshot.isOffscreen) {
            throw "Cached UIA element identity is invalid for '$ExpectedName'."
        }
    }
    if (($ClickScreenX -ge 0) -xor ($ClickScreenY -ge 0)) { throw "UIA click point must provide both screen coordinates or neither." }
    if ($ClickScreenX -ge 0) {
        $x = $ClickScreenX
        $y = $ClickScreenY
        if (($x -lt $rectangle.x) -or
            ($x -ge ($rectangle.x + $rectangle.width)) -or
            ($y -lt $rectangle.y) -or
            ($y -ge ($rectangle.y + $rectangle.height))) {
            throw "Requested UIA click point is outside the selected accessible element rectangle."
        }
    } else {
        $x = [int][Math]::Round($rectangle.x + $rectangle.width / 2)
        $y = [int][Math]::Round($rectangle.y + $rectangle.height / 2)
    }
    if ([P908Native]::GetWindowProcessId($WindowHandle) -ne [uint32]$Process.Id) { throw "UIA click window HWND does not belong to the measured process." }
    $pointer = [P908Native]::ClickWindowPoint($WindowHandle, $Process.Handle, $x, $y, [uint32]$Process.Id)
    $method = "uia-verified-pointer-click"
    $nativeActivation = [ordered]@{
        kind = "foreground-pointer"
        sequence = @("AllowSetForegroundWindow(pid)", "WaitForInputIdle", "ShowWindow(SW_RESTORE)", "AttachThreadInput", "BringWindowToTop", "SetActiveWindow", "SetFocus", "SetForegroundWindow", "verify-foreground-hwnd-pid", "SendInput")
        allowSetForegroundWindowReturned = [bool]$pointer.allowSetForegroundWindowReturned
        waitForInputIdleResult = [Int64]$pointer.waitForInputIdleResult
        targetThreadInputAttached = [bool]$pointer.targetThreadInputAttached
        foregroundThreadInputAttached = [bool]$pointer.foregroundThreadInputAttached
        foregroundThreadInputAttachErrorCode = [Int64]$pointer.foregroundThreadInputAttachErrorCode
        bringWindowToTopReturned = [bool]$pointer.bringWindowToTopReturned
        setActiveWindowReturnedNonZero = [bool]$pointer.setActiveWindowReturnedNonZero
        setFocusReturnedNonZero = [bool]$pointer.setFocusReturnedNonZero
        setForegroundWindowReturned = [bool]$pointer.setForegroundWindowReturned
        foregroundVerified = [bool]$pointer.foregroundVerified
        foregroundWindowHandle = [Int64]$pointer.foregroundWindowHandle
        foregroundProcessId = [Int64]$pointer.foregroundProcessId
        screenX = [int]$pointer.screenX
        screenY = [int]$pointer.screenY
        inputEventsSent = [Int64]$pointer.inputEventsSent
    }
    $pointerAttempt = $null
    if (-not $pointer.foregroundVerified) {
        $message = [P908Native]::ClickWindowMessage($WindowHandle, $Process.Handle, $x, $y, [uint32]$Process.Id)
        $method = "uia-verified-window-message-click"
        $nativeActivation = [ordered]@{
            kind = "verified-window-message"
            sequence = @("WaitForInputIdle", "ScreenToClient", "GetClientRect", "WM_MOUSEMOVE", "WM_LBUTTONDOWN", "WM_LBUTTONUP")
            windowHandle = [Int64]$message.windowHandle
            expectedProcessId = [Int64]$message.expectedProcessId
            windowProcessId = [Int64]$message.windowProcessId
            screenX = [int]$message.screenX
            screenY = [int]$message.screenY
            clientX = [int]$message.clientX
            clientY = [int]$message.clientY
            clientBounds = [ordered]@{ left = [int]$message.clientLeft; top = [int]$message.clientTop; right = [int]$message.clientRight; bottom = [int]$message.clientBottom }
            waitForInputIdleResult = [Int64]$message.waitForInputIdleResult
            waitForInputIdleSucceeded = [bool]$message.waitForInputIdleSucceeded
            mouseMoveSent = [bool]$message.mouseMoveSent
            leftButtonDownSent = [bool]$message.leftButtonDownSent
            leftButtonUpSent = [bool]$message.leftButtonUpSent
            messagesSent = [Int64]$message.messagesSent
        }
        $pointerAttempt = [ordered]@{
            method = "uia-verified-pointer-click"
            fallbackReason = "foreground-not-verified"
            windowHandle = [Int64]$pointer.windowHandle
            expectedProcessId = [Int64]$pointer.expectedProcessId
            windowProcessId = [Int64]$pointer.windowProcessId
            foregroundVerified = [bool]$pointer.foregroundVerified
            foregroundWindowHandle = [Int64]$pointer.foregroundWindowHandle
            foregroundProcessId = [Int64]$pointer.foregroundProcessId
            setForegroundWindowReturned = [bool]$pointer.setForegroundWindowReturned
            foregroundThreadInputAttachErrorCode = [Int64]$pointer.foregroundThreadInputAttachErrorCode
            inputEventsSent = [Int64]$pointer.inputEventsSent
        }
    }
    $record = [ordered]@{
        processInstanceId = $script:CurrentProcess.processInstanceId
        actionKind = if ($ActionKind -ceq "package-generation-submit") { "package-generation-submit-attempt" } else { $ActionKind }
        method = $method
        expectedAccessibleName = $ExpectedName
        observedAccessibleName = $observedName
        automationId = $automationId
        controlType = $controlType
        itemId = $TargetId
        fromPageId = $FromPage
        toPageId = $ToPage
        hwnd = [Int64]$WindowHandle.ToInt64()
        hwndProcessId = [Int64][P908Native]::GetWindowProcessId($WindowHandle)
        uiaProcessId = $uiaProcessId
        rectangle = $rectangle
        nativeActivation = $nativeActivation
        pointerAttempt = $pointerAttempt
        issuedAtUtc = Get-UtcTimestamp
        verifiedAppEvent = $false
    }
    Add-JsonLine $UiaActionsPath $record
    $record
}
function Find-PageNavigationItem {
    param([System.Windows.Automation.AutomationElement]$Window, [string]$Name)
    $trees = @((Get-UiaDescendants $Window) | Where-Object { $_.Current.Name -ceq "ClassMngr Next navigation" -and $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Tree })
    if ($trees.Count -ne 1) { throw "UIA expected one navigation tree, found $($trees.Count)." }
    $rows = @((Get-UiaDescendants $trees[0]) | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::TreeItem -and $_.Current.Name -ceq $Name })
    if ($rows.Count -ne 1) { throw "UIA expected one page row named '$Name', found $($rows.Count)." }
    [pscustomobject]@{ Tree = $trees[0]; Item = $rows[0] }
}
function Get-FixtureClassAccessibleTarget {
    param([string]$FixturePath, [int]$ClassId, [string]$Grade = "M2")
    $cacheKey = "$ClassId/$Grade"
    if ($script:CurrentProcess.classRowTargetCache.ContainsKey($cacheKey)) {
        return $script:CurrentProcess.classRowTargetCache[$cacheKey]
    }
    $helperPath = Join-Path $PSScriptRoot "sub_prep_accessible_summary.py"
    if (-not (Test-Path -LiteralPath $helperPath -PathType Leaf)) { throw "Fixture-derived Sub Prep accessibility helper is missing: $helperPath" }
    $helperOutput = & python $helperPath --fixture $FixturePath --class-id $ClassId --grade $Grade --selected-days Monday Tuesday Wednesday Thursday Friday Saturday 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Could not derive the expected accessible name for class $ClassId from the exact fixture: $($helperOutput -join ' ')" }
    $json = [string]($helperOutput | Select-Object -Last 1)
    $target = $json | ConvertFrom-Json -ErrorAction Stop
    if ([string]$target.classId -cne [string]$ClassId -or [string]$target.grade -cne $Grade -or
        [string]::IsNullOrWhiteSpace([string]$target.accessibleName) -or [int]$target.visibleClassCount -lt 1) {
        throw "Fixture-derived Sub Prep target identity was incomplete for class $ClassId."
    }
    $target | Add-Member -NotePropertyName fixtureSha256 -NotePropertyValue $script:CurrentProcess.fixtureHash
    $target | Add-Member -NotePropertyName helperPath -NotePropertyValue ([IO.Path]::GetFullPath($helperPath))
    $target | Add-Member -NotePropertyName helperSha256 -NotePropertyValue (Get-FileSha256 $helperPath)
    $script:CurrentProcess.classRowTargetCache[$cacheKey] = $target
    return $target
}
function Get-ClassListItemById {
    param([System.Windows.Automation.AutomationElement]$Window, [int]$ClassId, [Diagnostics.Process]$Process)
    $list = Get-UiaElementById $Window "subPrepClassSummaryList" "ControlType.List"
    if ($list.Current.ProcessId -ne $Process.Id) { throw "Sub Prep class list belongs to a different process." }
    $target = Get-FixtureClassAccessibleTarget $script:CurrentProcess.fixturePath $ClassId "M2"
    $automationId = [string]$list.Current.AutomationId
    if (-not $automationId.EndsWith("subPrepClassSummaryList", [StringComparison]::Ordinal)) { throw "Sub Prep row container automation identity is invalid." }
    $scroll = $null
    $scrollMethod = "uia-scrollpattern"
    try { $scroll = [System.Windows.Automation.ScrollPattern]$list.GetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern) }
    catch {
        $scrollMethod = "verified-window-message-wheel"
        $patternRows = [Collections.Generic.List[object]]::new()
        foreach ($candidate in @($list) + @((Get-UiaDescendants $list) | Where-Object {
            $_.Current.ControlType -in @([System.Windows.Automation.ControlType]::ScrollBar,
                [System.Windows.Automation.ControlType]::ListItem)
        })) {
            $candidatePatterns = @()
            try { $candidatePatterns = @($candidate.GetSupportedPatterns() | ForEach-Object { [string]$_.ProgrammaticName }) }
            catch { $candidatePatterns = @("pattern-query-failed:$($_.Exception.GetType().Name)") }
            $candidateRect = $candidate.Current.BoundingRectangle
            $patternRows.Add([ordered]@{
                name = [string]$candidate.Current.Name
                automationId = [string]$candidate.Current.AutomationId
                controlType = [string]$candidate.Current.ControlType.ProgrammaticName
                processId = [Int64]$candidate.Current.ProcessId
                nativeWindowHandle = [Int64]$candidate.Current.NativeWindowHandle
                isOffscreen = [bool]$candidate.Current.IsOffscreen
                rectangle = [ordered]@{ x = [double]$candidateRect.X; y = [double]$candidateRect.Y; width = [double]$candidateRect.Width; height = [double]$candidateRect.Height }
                supportedPatterns = $candidatePatterns
            })
        }
        Add-JsonLine $UiaActionsPath ([ordered]@{
            processInstanceId = $script:CurrentProcess.processInstanceId
            actionKind = "class-list-scroll-pattern-diagnostic"
            classId = [string]$ClassId
            listAutomationId = $automationId
            listProcessId = [Int64]$list.Current.ProcessId
            listCandidates = @($patternRows)
        })
    }
    if ($null -ne $scroll -and -not $scroll.Current.VerticallyScrollable) { throw "Sub Prep UIA list is not vertically scrollable for class $ClassId." }
    $mainWindowHandle = [IntPtr]$Process.MainWindowHandle
    if ($mainWindowHandle -eq [IntPtr]::Zero -or [P908Native]::GetWindowProcessId($mainWindowHandle) -ne [uint32]$Process.Id) { throw "Sub Prep list scroll has no verified main-window HWND." }
    $scrollTrace = [Collections.Generic.List[object]]::new()
    $maxScrollSteps = 40
    for ($step = 0; $step -le $maxScrollSteps; $step++) {
        $visibleItems = @((Get-UiaDescendants $list) | Where-Object {
            $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::ListItem
        })
        $rowRows = [Collections.Generic.List[object]]::new()
        $matches = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
        foreach ($element in $visibleItems) {
            if ($element.Current.ProcessId -ne $Process.Id) { throw "Sub Prep list row belongs to another process." }
            $rect = $element.Current.BoundingRectangle
            $row = [ordered]@{
                accessibleName = [string]$element.Current.Name
                helpText = [string]$element.Current.HelpText
                automationId = [string]$element.Current.AutomationId
                controlType = [string]$element.Current.ControlType.ProgrammaticName
                processId = [Int64]$element.Current.ProcessId
                isOffscreen = [bool]$element.Current.IsOffscreen
                rectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height }
            }
            $rowRows.Add($row)
            if ([string]$element.Current.Name -ceq [string]$target.accessibleName -and
                -not $element.Current.IsOffscreen) { $matches.Add($element) }
        }
        $percent = $null
        if ($null -ne $scroll) { $percent = [double]$scroll.Current.VerticalScrollPercent }
        $traceStep = [ordered]@{
            step = $step
            scrollMethod = $scrollMethod
            verticalScrollPercent = $percent
            visibleRows = @($rowRows)
        }
        $scrollTrace.Add($traceStep)
        if ($matches.Count -gt 1) { throw "UIA exposed duplicate exact accessible rows for fixture class $ClassId." }
        if ($matches.Count -eq 1) {
            $item = $matches[0]
            if ($item.Current.IsOffscreen -or $item.Current.ProcessId -ne $Process.Id -or
                $item.Current.ControlType -ne [System.Windows.Automation.ControlType]::ListItem -or
                -not ([string]$item.Current.AutomationId).EndsWith("subPrepClassSummaryList", [StringComparison]::Ordinal)) {
                throw "Fixture-derived class $ClassId UIA row identity or visibility is invalid."
            }
            $itemRect = $item.Current.BoundingRectangle
            $currentListRect = $list.Current.BoundingRectangle
            $resolution = [ordered]@{
                method = "fixture-derived-exact-accessible-name"
                scrollMethod = $scrollMethod
                classId = [string]$ClassId
                grade = [string]$target.grade
                accessibleName = [string]$target.accessibleName
                fixturePath = [string]$target.fixturePath
                fixtureSha256 = [string]$target.fixtureSha256
                helperPath = [string]$target.helperPath
                helperSha256 = [string]$target.helperSha256
                selectedDays = @($target.selectedDays)
                visibleClassCount = [Int64]$target.visibleClassCount
                baseLabelDuplicateCount = [Int64]$target.baseLabelDuplicateCount
                listAutomationId = $automationId
                listProcessId = [Int64]$list.Current.ProcessId
                listControlType = [string]$list.Current.ControlType.ProgrammaticName
                mainWindowHandle = [Int64]$mainWindowHandle.ToInt64()
                mainWindowProcessId = [Int64][P908Native]::GetWindowProcessId($mainWindowHandle)
                listRectangle = [ordered]@{ x = [double]$currentListRect.X; y = [double]$currentListRect.Y; width = [double]$currentListRect.Width; height = [double]$currentListRect.Height }
                rowAutomationId = [string]$item.Current.AutomationId
                rowProcessId = [Int64]$item.Current.ProcessId
                rowControlType = [string]$item.Current.ControlType.ProgrammaticName
                rowRectangle = [ordered]@{ x = [double]$itemRect.X; y = [double]$itemRect.Y; width = [double]$itemRect.Width; height = [double]$itemRect.Height }
                exactVisibleMatchCount = [Int64]$matches.Count
                scrollSteps = [Int64]$step
                scrollTrace = @($scrollTrace)
            }
            return [pscustomobject]@{ Item = $item; Resolution = $resolution }
        }
        if ($step -eq $maxScrollSteps -or ($null -ne $percent -and $percent -ge 100)) {
            $failedResolution = [ordered]@{
                method = "fixture-derived-exact-accessible-name"
                scrollMethod = $scrollMethod
                classId = [string]$ClassId; grade = [string]$target.grade
                accessibleName = [string]$target.accessibleName
                fixtureSha256 = [string]$target.fixtureSha256
                listAutomationId = $automationId; listProcessId = [Int64]$list.Current.ProcessId
                exactVisibleMatchCount = [Int64]0; scrollSteps = [Int64]$step
                scrollTrace = @($scrollTrace)
            }
            Add-JsonLine $UiaActionsPath ([ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; actionKind = "class-row-resolution"; outcome = "failed"; resolution = $failedResolution })
            throw "UIA did not expose the fixture-derived accessible row for class $ClassId after $step scroll steps."
        }
        if ($null -ne $scroll) {
            $scroll.Scroll([System.Windows.Automation.ScrollAmount]::NoAmount,
                [System.Windows.Automation.ScrollAmount]::LargeIncrement)
        } else {
            $beforeNames = @($rowRows | Where-Object { -not $_.isOffscreen } | ForEach-Object { [string]$_.accessibleName })
            $listRect = $list.Current.BoundingRectangle
            $listLeft = [int][Math]::Floor($listRect.X)
            $listTop = [int][Math]::Floor($listRect.Y)
            $listRight = [int][Math]::Ceiling($listRect.X + $listRect.Width)
            $listBottom = [int][Math]::Ceiling($listRect.Y + $listRect.Height)
            $wheel = [P908Native]::ScrollListWindowMessage($mainWindowHandle, $Process.Handle,
                $listLeft, $listTop, $listRight, $listBottom, -120, [uint32]$Process.Id)
            Start-Sleep -Milliseconds 120
            $Process.Refresh()
            if ($Process.HasExited) { throw "Packaged app exited while scrolling the class summary list." }
            $afterRows = @((Get-UiaDescendants $list) | Where-Object {
                $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::ListItem
            })
            $afterNames = @($afterRows | Where-Object { -not $_.Current.IsOffscreen } | ForEach-Object { [string]$_.Current.Name })
            $beforeSignature = [string]::Join("`n", [string[]]$beforeNames)
            $afterSignature = [string]::Join("`n", [string[]]$afterNames)
            if ($beforeSignature -ceq $afterSignature) { throw "Verified WM_MOUSEWHEEL did not change visible Sub Prep class rows." }
            $traceStep["scrollInput"] = [ordered]@{
                method = "verified-window-message-wheel"
                sequence = @("WaitForInputIdle", "ScreenToClient", "GetClientRect", "WM_MOUSEWHEEL")
                windowHandle = [Int64]$wheel.windowHandle
                expectedProcessId = [Int64]$wheel.expectedProcessId
                windowProcessId = [Int64]$wheel.windowProcessId
                screenX = [Int64]$wheel.screenX
                screenY = [Int64]$wheel.screenY
                clientX = [Int64]$wheel.clientX
                clientY = [Int64]$wheel.clientY
                clientBounds = [ordered]@{ left = [Int64]$wheel.clientLeft; top = [Int64]$wheel.clientTop; right = [Int64]$wheel.clientRight; bottom = [Int64]$wheel.clientBottom }
                listBounds = [ordered]@{ left = [Int64]$wheel.listLeft; top = [Int64]$wheel.listTop; right = [Int64]$wheel.listRight; bottom = [Int64]$wheel.listBottom }
                wheelDelta = [Int64]$wheel.wheelDelta
                waitForInputIdleSucceeded = [bool]$wheel.waitForInputIdleSucceeded
                messageSent = [bool]$wheel.messageSent
                beforeVisibleAccessibleNames = @($beforeNames)
                afterVisibleAccessibleNames = @($afterNames)
                visibleRowsChanged = $true
            }
        }
        Start-Sleep -Milliseconds 80
        $Process.Refresh()
        if ($Process.HasExited) { throw "Packaged app exited while locating the class $ClassId UIA row." }
    }
    throw "UIA class $ClassId row search reached an unreachable state."
}
function Get-AppDialogCandidates {
    param([Diagnostics.Process]$Process, [IntPtr]$MainHandle)
    $root = [System.Windows.Automation.AutomationElement]::RootElement
    $condition = [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Window)
    $seen = [Collections.Generic.HashSet[int]]::new()
    $matches = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    $collections = [Collections.Generic.List[System.Windows.Automation.AutomationElementCollection]]::new()
    try { $collections.Add($root.FindAll([System.Windows.Automation.TreeScope]::Children, $condition)) } catch { }
    try {
        $main = [System.Windows.Automation.AutomationElement]::FromHandle($MainHandle)
        if ($null -ne $main) { $collections.Add($main.FindAll([System.Windows.Automation.TreeScope]::Descendants, $condition)) }
    } catch { }
    foreach ($windows in $collections) {
        foreach ($window in $windows) {
            try {
                $current = $window.Current
                $handle = [int]$current.NativeWindowHandle
                if ($current.ProcessId -eq $Process.Id -and $handle -ne $MainHandle.ToInt32() -and $handle -ne 0 -and $seen.Add($handle)) {
                    $matches.Add($window)
                }
            } catch { }
        }
    }
    return $matches
}
function Get-AppDialog {
    param([Diagnostics.Process]$Process, [IntPtr]$MainHandle)
    $matches = @(Get-AppDialogCandidates $Process $MainHandle)
    if ($matches.Count -ne 1) { throw "UIA expected one modal app window, found $($matches.Count)." }
    $matches[0]
}
function Invoke-UiaButton {
    param([System.Windows.Automation.AutomationElement]$Element, [string]$ExpectedId, [Diagnostics.Process]$Process)
    if ($Element.Current.ProcessId -ne $Process.Id -or -not $Element.Current.AutomationId.EndsWith($ExpectedId, [StringComparison]::Ordinal)) { throw "UIA button identity mismatch for $ExpectedId." }
    $pattern = $Element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    ([System.Windows.Automation.InvokePattern]$pattern).Invoke()
}
function Set-UiaValue {
    param([System.Windows.Automation.AutomationElement]$Element, [string]$ExpectedId,
        [string]$Value, [Diagnostics.Process]$Process)
    if ($Element.Current.ProcessId -ne $Process.Id -or -not $Element.Current.AutomationId.EndsWith($ExpectedId, [StringComparison]::Ordinal)) { throw "UIA editable identity mismatch for $ExpectedId." }
    $pattern = [System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $pattern.SetValue($Value)
    if ($pattern.Current.Value -cne $Value) { throw "UIA did not retain the requested value for $ExpectedId." }
}
function Set-UiaCheckBox {
    param([System.Windows.Automation.AutomationElement]$Element, [string]$ExpectedId,
        [bool]$Checked, [Diagnostics.Process]$Process)
    if ($Element.Current.ProcessId -ne $Process.Id -or -not $Element.Current.AutomationId.EndsWith($ExpectedId, [StringComparison]::Ordinal)) { throw "UIA checkbox identity mismatch for $ExpectedId." }
    $pattern = [System.Windows.Automation.TogglePattern]$Element.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
    $current = $pattern.Current.ToggleState -eq [System.Windows.Automation.ToggleState]::On
    if ($current -ne $Checked) { $pattern.Toggle() }
    if (($pattern.Current.ToggleState -eq [System.Windows.Automation.ToggleState]::On) -ne $Checked) { throw "UIA checkbox state did not settle for $ExpectedId." }
}

function New-ProcessSample {
    param([Diagnostics.Process]$Process, [string]$Phase, [int]$SampleIndex = 0)
    $Process.Refresh()
    if ($Process.HasExited) { throw "Measured packaged app exited during process sampling." }
    $path = [IO.Path]::GetFullPath([P908Native]::GetProcessImagePath($Process.Handle))
    if (-not [string]::Equals($path, $script:CurrentProcess.expectedPath, [StringComparison]::OrdinalIgnoreCase) -or
        -not [P908Native]::IsNativeX64($Process.Handle) -or
        (Get-FileSha256 $path) -cne $script:CurrentProcess.expectedHash) { throw "Sampled process identity differs from packaged x64 Release app." }
    [ordered]@{
        processInstanceId = $script:CurrentProcess.processInstanceId
        phase = $Phase
        sampleIndex = $SampleIndex
        timestampUtc = Get-UtcTimestamp
        pid = [Int64]$Process.Id
        processCreatedAtUtc = $script:CurrentProcess.createdAtUtc
        canonicalExecutablePath = $path
        architecture = "x64"
        sha256 = $script:CurrentProcess.expectedHash
        currentWorkingSetBytes = [Int64]$Process.WorkingSet64
        privateBytes = [Int64]$Process.PrivateMemorySize64
        peakWorkingSetBytes = [Int64]$Process.PeakWorkingSet64
        handles = [Int64]$Process.HandleCount
        threads = [Int64]$Process.Threads.Count
    }
}

function Parse-StartupLine {
    param([string]$Line)
    if ($Line -match '^ClassMngrNext page lifecycle checkpoint=(?<json>\{.*\})$') {
        try { return $Matches.json | ConvertFrom-Json -DateKind String -ErrorAction Stop } catch { throw "Malformed app page checkpoint JSON: $($_.Exception.Message)" }
    }
    $null
}
function Get-OptionalObjectProperty {
    param([object]$Object, [string]$Name)
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    $property.Value
}
function Receive-ProcessOutput {
    param([Diagnostics.Process]$Process)
    foreach ($reader in $script:CurrentProcess.readers) {
        while ($null -ne $reader.ReadTask -and $reader.ReadTask.IsCompleted) {
            $line = $reader.ReadTask.GetAwaiter().GetResult()
            if ($null -eq $line) { $reader.ReadTask = $null; break }
            $entry = [ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; scenarioId = "representative-96-class-workspace"; pid = [Int64]$Process.Id; stream = $reader.Stream; observedAtUtc = Get-UtcTimestamp; line = [string]$line }
            Add-JsonLine $StartupLogPath $entry
            $script:CurrentProcess.logEntries.Add($entry)
            $parsed = Parse-StartupLine ([string]$line)
            if ($null -ne $parsed) {
                if ($parsed.event -in @("page-action-checkpoint", "page-lifecycle", "page-instance-destroyed")) {
                    $script:CurrentProcess.pageEvents.Add([ordered]@{ logIndex = $script:CurrentProcess.logEntries.Count - 1; observedAtUtc = $entry.observedAtUtc; record = $parsed })
                    $memory = Get-OptionalObjectProperty $parsed "memory"
                    $page = Get-OptionalObjectProperty $parsed "page"
                    $row = [ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; eventIndex = $script:CurrentProcess.logEntries.Count - 1; sourceEvent = (Get-OptionalObjectProperty $parsed "event"); pageKey = (Get-OptionalObjectProperty $parsed "page_key"); checkpoint = (Get-OptionalObjectProperty $parsed "checkpoint"); outcome = (Get-OptionalObjectProperty $parsed "outcome"); state = (Get-OptionalObjectProperty $parsed "state"); capturedAtUtc = (Get-OptionalObjectProperty $parsed "captured_at"); platform = (Get-OptionalObjectProperty $parsed "platform"); sampleAvailable = (Get-OptionalObjectProperty $parsed "sample_available"); memory = $memory; page = $page }
                    Add-JsonLine $ProcessMetricsPath $row
                }
            }
            if ($line -match '^ClassMngrNext navigation event=page-route ') {
                $script:CurrentProcess.routeEntries.Add([ordered]@{ logIndex = $script:CurrentProcess.logEntries.Count - 1; observedAtUtc = $entry.observedAtUtc; line = [string]$line })
            }
            $reader.ReadTask = $reader.Reader.ReadLineAsync()
        }
    }
}
function Wait-ForCondition {
    param([Diagnostics.Process]$Process, [scriptblock]$Condition, [string]$Description, [int]$TimeoutSeconds = 20)
    $deadline = [DateTimeOffset]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTimeOffset]::UtcNow -lt $deadline) {
        Receive-ProcessOutput $Process
        $value = & $Condition
        if ($null -ne $value -and $value -ne $false) { return $value }
        $Process.Refresh(); if ($Process.HasExited) { throw "App exited while waiting for $Description." }
        Start-Sleep -Milliseconds 20
    }
    Receive-ProcessOutput $Process
    $value = & $Condition
    if ($null -ne $value -and $value -ne $false) { return $value }
    throw "Timed out waiting for $Description."
}
function Assert-AppProcessMetrics {
    param([object]$Record, [string]$Description)
    if ($null -eq $Record -or $Record.sample_available -ne $true -or $Record.platform -cne "windows") { throw "$Description lacks a Windows process-memory snapshot." }
    $captured = [DateTimeOffset]::MinValue
    $capturedValue = $Record.captured_at
    if ($capturedValue -is [DateTime]) {
        if ($capturedValue.Kind -ne [DateTimeKind]::Utc) { throw "$Description capture timestamp was not explicitly UTC." }
        $capturedText = $capturedValue.ToUniversalTime().ToString("o")
    } elseif ($capturedValue -is [DateTimeOffset]) {
        if ($capturedValue.Offset -ne [TimeSpan]::Zero) { throw "$Description capture timestamp was not explicitly UTC." }
        $capturedText = $capturedValue.ToString("o")
    } else {
        $capturedText = [string]$capturedValue
    }
    if (-not [DateTimeOffset]::TryParse($capturedText, [ref]$captured) -or $captured.Offset -ne [TimeSpan]::Zero) { throw "$Description has an invalid/non-UTC capture timestamp." }
    $memory = $Record.memory
    foreach ($key in @("working_set_bytes", "private_usage_bytes", "private_working_set_bytes", "peak_working_set_bytes", "handle_count", "thread_count")) {
        $value = $memory.$key
        if ($null -eq $value -or [Int64]$value -lt 0 -or ($key -ne "handle_count" -and [Int64]$value -eq 0)) { throw "$Description has a missing or invalid process counter $key." }
    }
}
function Assert-AppProcessSnapshot {
    param([object]$Record, [string]$Description)
    Assert-AppProcessMetrics $Record $Description
    if ($Record.page_instance_available -ne $true -or $null -eq $Record.page) { throw "$Description lacks a captured page/widget snapshot." }
}
function Get-NewPageAction {
    param([int]$StartEventIndex, [string]$Checkpoint, [scriptblock]$Predicate = { param($r) $true })
    $matches = @($script:CurrentProcess.pageEvents | Where-Object { $_.record.event -eq "page-action-checkpoint" -and $_.record.page_key -eq "sub_prep" -and $_.record.checkpoint -ceq $Checkpoint -and $_.logIndex -ge $StartEventIndex -and (& $Predicate $_.record) })
    if ($matches.Count -gt 1) { throw "Duplicate $Checkpoint action checkpoints followed one UI interaction." }
    if ($matches.Count -eq 1) { return $matches[0] }
    $null
}
function Get-NewSubPrepActivation {
    param([int]$StartEventIndex)
    $matches = @($script:CurrentProcess.pageEvents | Where-Object {
        $_.record.event -eq "page-lifecycle" -and $_.record.page_key -eq "sub_prep" -and
        $_.record.state -eq "activated" -and $_.logIndex -ge $StartEventIndex
    })
    if ($matches.Count -gt 1) { throw "Duplicate Sub Prep activation snapshots followed one entry route." }
    if ($matches.Count -eq 1) { return $matches[0] }
    $null
}
function Wait-SubPrepSummaryReady {
    param([Diagnostics.Process]$Process, [int]$StartEventIndex, [string]$Description,
        [Int64]$ExpectedSummaryClassCount = 96)
    $action = Wait-ForCondition $Process { Get-NewPageAction $StartEventIndex "summary-data-ready" } $Description 60
    if ($action.record.outcome -cne "completed") { throw "$Description action checkpoint did not complete." }
    # SubPrepPage emits this action from onActivating before the page host emits
    # its activated snapshot. Pair the action with that following, data-backed
    # lifecycle record instead of inventing page metrics on the earlier event.
    Assert-AppProcessMetrics $action.record $Description
    $activation = Wait-ForCondition $Process { Get-NewSubPrepActivation $StartEventIndex } "$Description page activation snapshot" 30
    Assert-AppProcessSnapshot $activation.record "$Description page activation snapshot"
    $metrics = $activation.record.page.sub_prep_runtime_metrics
    if ($null -eq $metrics -or [Int64]$metrics.summary_class_count -ne $ExpectedSummaryClassCount -or
        [Int64]$metrics.teacher_summary_count -ne 24 -or [Int64]$metrics.summary_model_count -ne 1 -or
        [Int64]$metrics.active_grade_row_count -le 0 -or [Int64]$metrics.active_grade_row_count -gt $ExpectedSummaryClassCount) {
        throw "$Description activated lifecycle snapshot has invalid $ExpectedSummaryClassCount-class summary/model/row metrics."
    }
    [ordered]@{ action = $action; activation = $activation; metrics = $metrics }
}
function Enable-SubPrepSaturdaySummaryFilter {
    param([Diagnostics.Process]$Process)
    $window = Get-MainWindow $Process
    $filter = Get-UiaElementById $window "subPrepDay_5" "ControlType.CheckBox"
    $current = $filter.Current
    if ($current.ProcessId -ne $Process.Id -or $current.Name -cne "Saturday" -or
        $current.IsOffscreen -or -not $current.IsEnabled -or
        -not ([string]$current.AutomationId).EndsWith("subPrepDay_5", [StringComparison]::Ordinal)) {
        throw "Saturday summary filter does not have the exact enabled visible UIA identity."
    }
    $pattern = [System.Windows.Automation.TogglePattern]$filter.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
    $before = [string]$pattern.Current.ToggleState
    if ($pattern.Current.ToggleState -ne [System.Windows.Automation.ToggleState]::Off) {
        throw "Saturday summary filter was unexpectedly enabled before the measured toggle."
    }
    $startIndex = $script:CurrentProcess.logEntries.Count
    $pattern.Toggle()
    $after = [string]$pattern.Current.ToggleState
    if ($after -cne [string][System.Windows.Automation.ToggleState]::On) {
        throw "Saturday summary filter did not switch to On through its UIA TogglePattern."
    }
    [ordered]@{
        processInstanceId = $script:CurrentProcess.processInstanceId
        actionKind = "sub-prep-summary-day-filter"
        method = "uia-toggle-pattern"
        automationId = [string]$current.AutomationId
        accessibleName = [string]$current.Name
        uiaProcessId = [Int64]$current.ProcessId
        mainWindowHandle = [Int64]$Process.MainWindowHandle.ToInt64()
        hwndProcessId = [Int64][P908Native]::GetWindowProcessId([IntPtr]$Process.MainWindowHandle)
        controlType = [string]$current.ControlType.ProgrammaticName
        isOffscreen = [bool]$current.IsOffscreen
        isEnabled = [bool]$current.IsEnabled
        toggleStateBefore = $before
        toggleStateAfter = $after
        actionStartLogIndex = [Int64]$startIndex
        issuedAtUtc = Get-UtcTimestamp
        verifiedByAppEvent = $false
    }
}
function Wait-PageAction {
    param([Diagnostics.Process]$Process, [int]$StartEventIndex, [string]$Checkpoint, [scriptblock]$Predicate = { param($r) $true }, [string]$Description = $Checkpoint, [int]$TimeoutSeconds = 60)
    $event = Wait-ForCondition $Process { Get-NewPageAction $StartEventIndex $Checkpoint $Predicate } $Description $TimeoutSeconds
    Assert-AppProcessSnapshot $event.record $Description
    $event
}
function Add-PageReleaseSample {
    param([Diagnostics.Process]$Process, [object]$Event, [int]$Cycle = 0)
    $phase = if ($Event.record.checkpoint -ceq "page-suspended") {
        "page-suspended"
    } else {
        "page-release-" + [string]$Event.record.details.delay
    }
    $sample = New-ProcessSample $Process $phase ([int]$Event.logIndex)
    $sampleDelay = ([DateTimeOffset]::Parse($sample.timestampUtc).ToUniversalTime() - [DateTimeOffset]::Parse($Event.observedAtUtc).ToUniversalTime()).TotalMilliseconds
    if ($sampleDelay -lt 0 -or $sampleDelay -gt $MaxEventSampleDelayMilliseconds) {
        throw "$script:CurrentProcess.processInstanceId $phase process snapshot exceeded the event correlation window."
    }
    $sample.sampleDelayMilliseconds = [Math]::Round($sampleDelay, 3)
    if ($Cycle -gt 0) { $sample.cycle = $Cycle }
    $sample.appCheckpoint = $Event.record
    Add-JsonLine $ProcessMetricsPath $sample
    $sample
}
function Get-LatestSubPrepMetrics {
    $events = @($script:CurrentProcess.pageEvents | Where-Object { $_.record.page_key -eq "sub_prep" -and $_.record.page.sub_prep_runtime_metrics -ne $null })
    if ($events.Count -eq 0) { return $null }
    $events[-1].record.page.sub_prep_runtime_metrics
}
function Get-NewRoute {
    param([int]$StartIndex, [string]$From, [string]$To)
    $matches = @($script:CurrentProcess.routeEntries | Where-Object {
        $_.logIndex -ge $StartIndex -and $_.line -match "^ClassMngrNext navigation event=page-route outcome=completed cause=selection from=$([regex]::Escape($From)) to=$([regex]::Escape($To)) catalog_document_count=(?<docs>-?\d+) loaded_pdf_body_count=(?<bodies>\d+)$"
    })
    if ($matches.Count -gt 1) { throw "Duplicate route events for $From to $To." }
    if ($matches.Count -eq 1) { $matches[0] }
}
function Invoke-PageRoute {
    param([Diagnostics.Process]$Process, [string]$From, [string]$To, [string]$TargetName, [int]$Cycle = 0)
    $window = Get-MainWindow $Process
    $windowHandle = [IntPtr]$Process.MainWindowHandle
    $target = Find-PageNavigationItem $window $TargetName
    $beforeIndex = $script:CurrentProcess.logEntries.Count
    $uiAction = Invoke-VerifiedUiaControlClick $target.Item $TargetName $windowHandle $Process "page-navigation" $To $From $To
    $startRoutes = $script:CurrentProcess.routeEntries.Count
    $route = Wait-ForCondition $Process { Get-NewRoute $beforeIndex $From $To } "authentic page route $From to $To" 15
    $eventLine = [string]$route.line
    if ([P908Native]::GetWindowProcessId($windowHandle) -ne [uint32]$Process.Id -or [Int64]$uiAction.hwndProcessId -ne [Int64]$Process.Id) { throw "Route click HWND PID does not match the measured process." }
    $sample = New-ProcessSample $Process "navigation" $script:CurrentProcess.navigationIndex
    if ($Cycle -gt 0) { $sample.cycle = $Cycle }
    $delay = ([DateTimeOffset]::Parse($sample.timestampUtc).ToUniversalTime() - [DateTimeOffset]::Parse($route.observedAtUtc).ToUniversalTime()).TotalMilliseconds
    if ($delay -lt 0 -or $delay -gt $MaxEventSampleDelayMilliseconds) { throw "Route process snapshot exceeded the event correlation window." }
    $row = [ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; transitionIndex = $script:CurrentProcess.navigationIndex; actionStartLogIndex = [Int64]$beforeIndex; fromPageId = $From; toPageId = $To; targetAccessibleName = $TargetName; selectionMethod = $uiAction.method; uiaAction = $uiAction; appEventLogIndex = [Int64]$route.logIndex; appEventObservedAtUtc = [string]$route.observedAtUtc; sampleDelayMilliseconds = [Math]::Round($delay, 3); processSample = $sample; appEventLine = $eventLine; observedAtUtc = Get-UtcTimestamp }
    if ($Cycle -gt 0) { $row.cycle = $Cycle }
    $uiAction.verifiedAppEvent = $true
    $uiAction.appEventLogIndex = [Int64]$route.logIndex
    $uiAction.appEventObservedAtUtc = [string]$route.observedAtUtc
    Add-JsonLine $UiaActionsPath $uiAction
    Add-JsonLine $NavigationMetricsPath $row
    $script:CurrentProcess.navigationIndex++
    $row
}
function Invoke-SubPrepPageClick {
    param([Diagnostics.Process]$Process, [string]$ClassId)
    $window = Get-MainWindow $Process
    $resolvedTarget = Get-ClassListItemById $window ([int]$ClassId) $Process
    $item = $resolvedTarget.Item
    $currentMetrics = Get-LatestSubPrepMetrics
    if ($null -ne $currentMetrics -and [string]$currentMetrics.selected_class_id -ceq $ClassId) {
        $list = Get-UiaElementById $window "subPrepClassSummaryList" "ControlType.List"
        $alternate = @((Get-UiaDescendants $list) | Where-Object { $_.Current.ControlType -in @([System.Windows.Automation.ControlType]::ListItem, [System.Windows.Automation.ControlType]::DataItem) -and $_.Current.Name -cne $item.Current.Name } | Select-Object -First 1)
        if ($alternate.Count -ne 1) { throw "UIA cannot move away from already-selected class $ClassId to prove a real selection callback." }
        $alternateName = [string]$alternate[0].Current.Name
        $alternateStart = $script:CurrentProcess.logEntries.Count
        if ($alternate[0].Current.IsOffscreen) { ([System.Windows.Automation.ScrollItemPattern]$alternate[0].GetCurrentPattern([System.Windows.Automation.ScrollItemPattern]::Pattern)).ScrollIntoView(); Start-Sleep -Milliseconds 100 }
        $alternateClick = Invoke-VerifiedUiaControlClick $alternate[0] $alternateName ([IntPtr]$Process.MainWindowHandle) $Process "sub-prep-selection-warmup" "other-class" "sub_prep" "sub_prep"
        $alternateEvent = Wait-PageAction $Process $alternateStart "class-selection" { param($r) $r.outcome -eq "details-ready" -and $r.page.sub_prep_runtime_metrics.selected_class_id -cne $ClassId } "real alternate class selection" 30
        $alternateClick.verifiedAppEvent = $true
        $alternateClick.appEventLogIndex = [Int64]$alternateEvent.logIndex
        $alternateClick.appEventSelectedClassId = [string]$alternateEvent.record.page.sub_prep_runtime_metrics.selected_class_id
        Add-JsonLine $UiaActionsPath $alternateClick
        $window = Get-MainWindow $Process
        $resolvedTarget = Get-ClassListItemById $window ([int]$ClassId) $Process
        $item = $resolvedTarget.Item
    }
    $resolvedTarget.Resolution["actionIndex"] = [Int64]$script:CurrentProcess.classSelectionCount
    Add-JsonLine $UiaActionsPath ([ordered]@{
        processInstanceId = $script:CurrentProcess.processInstanceId
        actionKind = "class-row-resolution"
        outcome = "target-found"
        actionIndex = [Int64]$script:CurrentProcess.classSelectionCount
        resolution = $resolvedTarget.Resolution
    })
    $beforeIndex = $script:CurrentProcess.logEntries.Count
    $name = [string]$item.Current.Name
    $windowHandle = [IntPtr]$Process.MainWindowHandle
    $list = Get-UiaElementById $window "subPrepClassSummaryList" "ControlType.List"
    if ($list.Current.ProcessId -ne $Process.Id) { throw "Sub Prep class list belongs to a different process at click time." }
    $listRect = $list.Current.BoundingRectangle
    $rowRect = $item.Current.BoundingRectangle
    $clickLeft = [Math]::Max($listRect.X, $rowRect.X)
    $clickTop = [Math]::Max($listRect.Y, $rowRect.Y)
    $clickRight = [Math]::Min($listRect.X + $listRect.Width, $rowRect.X + $rowRect.Width)
    $clickBottom = [Math]::Min($listRect.Y + $listRect.Height, $rowRect.Y + $rowRect.Height)
    if ($clickRight -le $clickLeft -or $clickBottom -le $clickTop) { throw "The exact class row does not intersect its UIA list viewport." }
    $clickScreenX = [int][Math]::Floor($clickLeft + (($clickRight - $clickLeft) / 2))
    $clickScreenY = [int][Math]::Floor($clickTop + (($clickBottom - $clickTop) / 2))
    $uiAction = Invoke-VerifiedUiaControlClick $item $name $windowHandle $Process "sub-prep-class-selection" $ClassId "sub_prep" "sub_prep" $clickScreenX $clickScreenY
    $uiAction.fixtureDerivedTarget = $resolvedTarget.Resolution
    $selected = Wait-PageAction $Process $beforeIndex "class-selection" { param($r) $r.outcome -eq "details-ready" -and $r.page.sub_prep_runtime_metrics.selected_class_id -ceq $ClassId -and $r.page.sub_prep_runtime_metrics.selected_details_retained -eq $true } "class $ClassId selected details" 30
    $uiAction.verifiedAppEvent = $true
    $uiAction.appEventLogIndex = [Int64]$selected.logIndex
    $uiAction.appEventObservedAtUtc = [string]$selected.observedAtUtc
    $uiAction.appEventSelectedClassId = $selected.record.page.sub_prep_runtime_metrics.selected_class_id
    Add-JsonLine $UiaActionsPath $uiAction
    $selectionSample = New-ProcessSample $Process "class-selection" $script:CurrentProcess.navigationIndex
    $selectionDelay = ([DateTimeOffset]::Parse($selectionSample.timestampUtc).ToUniversalTime() - [DateTimeOffset]::Parse($selected.observedAtUtc).ToUniversalTime()).TotalMilliseconds
    if ($selectionDelay -lt 0 -or $selectionDelay -gt $MaxEventSampleDelayMilliseconds) { throw "Class-selection process snapshot exceeded the event correlation window." }
    $selectionSample.sampleDelayMilliseconds = [Math]::Round($selectionDelay, 3)
    Add-JsonLine $NavigationMetricsPath ([ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; transitionIndex = $null; actionKind = "class-selection"; selectedClassId = $ClassId; selectedAccessibleName = $name; uiaAction = $uiAction; appEventLogIndex = [Int64]$selected.logIndex; appEventObservedAtUtc = [string]$selected.observedAtUtc; appEvent = $selected.record; sample = $selectionSample })
    $script:CurrentProcess.classSelectionCount++
    $selected
}
function Invoke-Refresh {
    param([Diagnostics.Process]$Process)
    $window = Get-MainWindow $Process
    $button = Get-UiaElementById $window "subPrepRefreshButton"
    if ($button.Current.ProcessId -ne $Process.Id) { throw "Refresh button belongs to a different process." }
    $beforeIndex = $script:CurrentProcess.logEntries.Count
    $elementName = [string]$button.Current.Name
    Invoke-UiaButton $button "subPrepRefreshButton" $Process
    $event = Wait-PageAction $Process $beforeIndex "explicit-refresh" { param($r) $true } "successful explicit refresh" 30
    if ($event.record.outcome -ne "completed") { throw "Explicit Sub Prep refresh reported $($event.record.outcome)." }
    $sample = New-ProcessSample $Process "explicit-refresh" ([int]$event.logIndex)
    $sampleDelay = ([DateTimeOffset]::Parse($sample.timestampUtc).ToUniversalTime() - [DateTimeOffset]::Parse($event.observedAtUtc).ToUniversalTime()).TotalMilliseconds
    if ($sampleDelay -lt 0 -or $sampleDelay -gt $MaxEventSampleDelayMilliseconds) { throw "Explicit-refresh process snapshot exceeded the event correlation window." }
    $sample.sampleDelayMilliseconds = [Math]::Round($sampleDelay, 3)
    $sample.appCheckpoint = $event.record
    Add-JsonLine $ProcessMetricsPath $sample
    $event.externalSample = $sample
    Add-JsonLine $UiaActionsPath ([ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; actionKind = "explicit-refresh"; method = "uia-verified-invoke"; automationId = [string]$button.Current.AutomationId; accessibleName = $elementName; uiaProcessId = [Int64]$button.Current.ProcessId; appEventLogIndex = [Int64]$event.logIndex; outcome = $event.record.outcome; event = $event.record })
    $event
}
function Invoke-OutputGeneration {
    param([Diagnostics.Process]$Process, [string]$OutputRoot, [string]$UserName)
    $window = Get-MainWindow $Process
    $generate = Get-UiaElementById $window "subPrepGenerateButton"
    if ($generate.Current.ProcessId -ne $Process.Id) { throw "Generate button belongs to a different process." }
    $generateName = [string]$generate.Current.Name
    $generateClick = Invoke-VerifiedUiaControlClick $generate $generateName ([IntPtr]$Process.MainWindowHandle) $Process "open-generation-dialog-attempt" "subPrepGenerateButton" "sub_prep" "sub_prep"
    $dialog = $null
    $dialogDeadline = [DateTime]::UtcNow.AddSeconds(15)
    $dialogSearchError = $null
    while ($null -eq $dialog -and [DateTime]::UtcNow -lt $dialogDeadline) {
        try { $dialog = Get-AppDialog $Process ([IntPtr]$Process.MainWindowHandle) }
        catch { $dialogSearchError = $_.Exception.Message }
        if ($null -eq $dialog) { Start-Sleep -Milliseconds 150 }
    }
    if ($null -eq $dialog) {
        $candidateRows = @(Get-AppDialogCandidates $Process ([IntPtr]$Process.MainWindowHandle) | ForEach-Object {
            $current = $_.Current
            $rect = $current.BoundingRectangle
            [ordered]@{ name = [string]$current.Name; automationId = [string]$current.AutomationId; processId = [Int64]$current.ProcessId; handle = [Int64]$current.NativeWindowHandle; controlType = [string]$current.ControlType.ProgrammaticName; isOffscreen = [bool]$current.IsOffscreen; rectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height } }
        })
        Add-JsonLine $UiaActionsPath ([ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; actionKind = "generation-dialog-discovery-failed"; mainWindowHandle = [Int64]$Process.MainWindowHandle.ToInt64(); mainWindowProcessId = [Int64][P908Native]::GetWindowProcessId([IntPtr]$Process.MainWindowHandle); dialogSearchError = $dialogSearchError; candidateWindows = $candidateRows; generateButton = [ordered]@{ accessibleName = $generateName; automationId = [string]$generate.Current.AutomationId; processId = [Int64]$generate.Current.ProcessId; isEnabled = [bool]$generate.Current.IsEnabled; isOffscreen = [bool]$generate.Current.IsOffscreen; rectangle = $generateClick.rectangle }; clickAttempt = $generateClick })
        throw "Timed out waiting for Sub Prep output dialog; UIA dialog candidates and button identity were preserved."
    }
    $dialogHandle = [IntPtr]$dialog.Current.NativeWindowHandle
    if ($dialogHandle -eq [IntPtr]::Zero -or [P908Native]::GetWindowProcessId($dialogHandle) -ne [uint32]$Process.Id) { throw "Sub Prep output dialog is not owned by the measured process." }
    $generateClick.actionKind = "open-generation-dialog"
    $generateClick.verifiedAppEvent = $true
    $generateClick.dialogHandle = [Int64]$dialogHandle.ToInt64()
    $generateClick.dialogProcessId = [Int64][P908Native]::GetWindowProcessId($dialogHandle)
    $generateClick.dialogObservedAtUtc = Get-UtcTimestamp
    Add-JsonLine $UiaActionsPath $generateClick
    $folder = Get-UiaElementById $dialog "subPrepOutputFolder"
    $user = Get-UiaElementById $dialog "subPrepOutputUserName"
    $create = Get-UiaElementById $dialog "subPrepCreateFolderCheckBox"
    $paper = Get-UiaElementById $dialog "subPrepPrintPaperCopies"
    $open = Get-UiaElementById $dialog "subPrepOpenFolderAfterGeneration"
    $vacation = Get-UiaElementById $dialog "subPrepNextVacationCheckBox"
    $accept = Get-UiaElementById $dialog "subPrepOutputAccept"
    if ($accept.Current.ProcessId -ne $Process.Id -or
        -not ([string]$accept.Current.AutomationId).EndsWith("subPrepOutputAccept", [StringComparison]::Ordinal)) {
        throw "Package accept button exact UIA identity does not match the measured process."
    }
    $acceptName = [string]$accept.Current.Name
    $acceptSnapshot = [ordered]@{
        expectedAccessibleName = $acceptName
        observedAccessibleName = $acceptName
        automationId = [string]$accept.Current.AutomationId
        controlType = [string]$accept.Current.ControlType.ProgrammaticName
        processId = [Int64]$accept.Current.ProcessId
        isOffscreen = [bool]$accept.Current.IsOffscreen
        rectangle = Get-PointerElementIdentity $accept $acceptName $Process
    }
    Add-JsonLine $UiaActionsPath ([ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; actionKind = "package-generation-control-resolved"; automationId = [string]$accept.Current.AutomationId; accessibleName = [string]$accept.Current.Name; uiaProcessId = [Int64]$accept.Current.ProcessId; controlType = [string]$accept.Current.ControlType.ProgrammaticName })
    Set-UiaValue $folder "subPrepOutputFolder" $OutputRoot $Process
    Set-UiaValue $user "subPrepOutputUserName" $UserName $Process
    Set-UiaCheckBox $create "subPrepCreateFolderCheckBox" $true $Process
    Set-UiaCheckBox $paper "subPrepPrintPaperCopies" $false $Process
    Set-UiaCheckBox $open "subPrepOpenFolderAfterGeneration" $false $Process
    Set-UiaCheckBox $vacation "subPrepNextVacationCheckBox" $false $Process
    foreach ($control in @($folder, $user, $create, $paper, $open, $vacation)) {
        Add-JsonLine $UiaActionsPath ([ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; actionKind = "package-output-control"; method = if ($control.Current.ControlType -eq [System.Windows.Automation.ControlType]::Edit) { "uia-value-pattern" } else { "uia-toggle-pattern" }; automationId = [string]$control.Current.AutomationId; accessibleName = [string]$control.Current.Name; uiaProcessId = [Int64]$control.Current.ProcessId; value = if ($control.Current.ControlType -eq [System.Windows.Automation.ControlType]::Edit) { ([System.Windows.Automation.ValuePattern]$control.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)).Current.Value } else { [string]([System.Windows.Automation.TogglePattern]$control.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)).Current.ToggleState } })
    }
    $beforeIndex = $script:CurrentProcess.logEntries.Count
    Add-JsonLine $UiaActionsPath ([ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; actionKind = "package-generation-submit-request"; method = "uia-verified-control-click"; expectedAccessibleName = $acceptName; automationId = $acceptSnapshot.automationId; uiaProcessId = $acceptSnapshot.processId; dialogHandle = [Int64]$dialogHandle.ToInt64(); outputRoot = [IO.Path]::GetFullPath($OutputRoot); issuedAtUtc = Get-UtcTimestamp })
    $submitClick = Invoke-VerifiedUiaControlClick $accept $acceptName $dialogHandle $Process "package-generation-submit" "subPrepOutputAccept" "sub_prep" "sub_prep" -CachedElementSnapshot $acceptSnapshot
    $started = Wait-PageAction $Process $beforeIndex "package-generation-started" { param($r) $true } "package generation started" 120
    $submitClick.actionKind = "package-generation-submit"
    $submitClick.dialogHandle = [Int64]$dialogHandle.ToInt64()
    $submitClick.appEventLogIndex = [Int64]$started.logIndex
    $submitClick.verifiedAppEvent = ($started.record.outcome -eq "started")
    $submitClick.appEvent = $started.record
    Add-JsonLine $UiaActionsPath $submitClick
    if ($started.record.outcome -ne "started" -or [int]$started.record.details.requestedClassCount -ne 30 -or
        [IO.Path]::GetFullPath([string]$started.record.details.targetRoot) -cne [IO.Path]::GetFullPath($OutputRoot) -or
        -not [bool]$started.record.details.createFolder -or [bool]$started.record.details.openFolderAfterGeneration) {
        throw "Production generation request did not match the intended 30-class weekday output configuration."
    }
    $complete = Wait-PageAction $Process $started.logIndex "package-generation" { param($r) $true } "completed package generation" 900
    if ($complete.record.outcome -ne "completed") {
        $statusText = $null
        $statusError = $null
        try {
            $statusElement = Get-UiaElementById (Get-MainWindow $Process) "subPrepRouteStatus"
            $statusText = [string]$statusElement.Current.Name
        } catch { $statusError = $_.Exception.Message }
        $outputEntries = @()
        if (Test-Path -LiteralPath $OutputRoot -PathType Container) {
            $outputEntries = @(Get-ChildItem -LiteralPath $OutputRoot -Force -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
                [ordered]@{ path = $_.FullName; isDirectory = $_.PSIsContainer; length = if ($_.PSIsContainer) { $null } else { [Int64]$_.Length } }
            })
        }
        Add-JsonLine $UiaActionsPath ([ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; actionKind = "package-generation-failure-diagnostic"; outcome = [string]$complete.record.outcome; appCheckpoint = $complete.record; statusAccessibleText = $statusText; statusReadError = $statusError; outputEntries = $outputEntries; observedAtUtc = Get-UtcTimestamp })
        throw "Package generation reported $($complete.record.outcome): $statusText"
    }
    $generationSample = New-ProcessSample $Process "generation-complete" ([int]$complete.logIndex)
    $generationDelay = ([DateTimeOffset]::Parse($generationSample.timestampUtc).ToUniversalTime() - [DateTimeOffset]::Parse($complete.observedAtUtc).ToUniversalTime()).TotalMilliseconds
    if ($generationDelay -lt 0 -or $generationDelay -gt $MaxEventSampleDelayMilliseconds) { throw "Generation-complete process snapshot exceeded the event correlation window." }
    $generationSample.appCheckpoint = $complete.record
    $generationSample.sampleDelayMilliseconds = [Math]::Round($generationDelay, 3)
    Add-JsonLine $ProcessMetricsPath $generationSample
    $complete.externalSample = $generationSample
    $details = $complete.record.details
    if (-not [bool]$details.folderCreated -or [int]$details.selectedClassCount -ne 30 -or [int]$details.documentCount -le 0) { throw "Successful generation checkpoint has an incomplete output manifest." }
    $post = [ordered]@{}
    foreach ($delay in @("immediate", "1s", "5s")) {
        $event = Wait-PageAction $Process $complete.logIndex "post-generation-snapshot" { param($r) $r.outcome -eq "completed" -and $r.details.delay -ceq $delay } "post-generation $delay snapshot" 20
        $sample = New-ProcessSample $Process "post-generation-$delay" ([int]$event.logIndex)
        if ($RetentionDiagnosticOnly) { $sample.cycle = 1 }
        $snapshotDelay = ([DateTimeOffset]::Parse($sample.timestampUtc).ToUniversalTime() - [DateTimeOffset]::Parse($event.observedAtUtc).ToUniversalTime()).TotalMilliseconds
        if ($snapshotDelay -lt 0 -or $snapshotDelay -gt $MaxEventSampleDelayMilliseconds) { throw "Post-generation $delay process snapshot exceeded the event correlation window." }
        $sample.appCheckpoint = $event.record
        $sample.sampleDelayMilliseconds = [Math]::Round($snapshotDelay, 3)
        Add-JsonLine $ProcessMetricsPath $sample
        $event.externalSample = $sample
        $post[$delay] = $event
    }
    $outputDirectory = [IO.Path]::GetFullPath([string]$details.outputDirectory)
    $rootPrefix = [IO.Path]::GetFullPath($OutputRoot).TrimEnd('\') + "\"
    if (-not $outputDirectory.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw "Generated output directory escaped its unique process root." }
    $documentPaths = @($details.documentPaths | ForEach-Object { [IO.Path]::GetFullPath([string]$_) })
    if ($documentPaths.Count -ne [int]$details.documentCount) { throw "Generated path list count differs from the reported document count." }
    $files = [Collections.Generic.List[object]]::new()
    foreach ($path in $documentPaths) {
        if (-not $path.StartsWith(($outputDirectory.TrimEnd('\') + "\"), [StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Generated document is missing or outside its unique output directory: $path" }
        $info = Get-Item -LiteralPath $path
        if ($info.Length -le 5) { throw "Generated PDF is empty: $path" }
        $stream = [IO.File]::OpenRead($path)
        try { $header = [byte[]]::new(5); [void]$stream.Read($header, 0, 5); if ([Text.Encoding]::ASCII.GetString($header) -ne "%PDF-") { throw "Generated document is not a PDF: $path" } }
        finally { $stream.Dispose() }
        $files.Add([ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; path = $path; relativePath = [IO.Path]::GetRelativePath($outputDirectory, $path).Replace('\', '/'); sizeBytes = [Int64]$info.Length; sha256 = Get-FileSha256 $path; format = "PDF" })
    }
    if (@($files | Where-Object { $_.relativePath -ceq "Sub Prep.pdf" }).Count -ne 1) { throw "Generated package must contain exactly one Sub Prep.pdf." }
    $script:GeneratedFiles.Add([ordered]@{ processInstanceId = $script:CurrentProcess.processInstanceId; outputRoot = [IO.Path]::GetFullPath($OutputRoot); outputDirectory = $outputDirectory; generatedAtUtc = [string]$complete.record.captured_at; documentCount = $documentPaths.Count; documents = @($files) })
    [ordered]@{ started = $started; completed = $complete; postGeneration = $post; outputDirectory = $outputDirectory; documents = @($files) }
}

function Get-FixtureShapeAtExit {
    param([string]$FixturePath, [string]$ProcessId)
    $fixtureShapeCode = @'
import json, sqlite3, sys
connection = sqlite3.connect('file:' + sys.argv[1] + '?mode=ro', uri=True)
tables = ("classes", "teachers", "roster_data")
counts = {name: connection.execute(f"select count(*) from {name}").fetchone()[0] for name in tables}
integrity = connection.execute("pragma integrity_check").fetchone()[0]
connection.close()
counts["integrityCheck"] = integrity
print(json.dumps(counts, sort_keys=True))
'@
    $fixtureShapeOutput = & python -c $fixtureShapeCode $FixturePath
    if ($LASTEXITCODE -ne 0) { throw "$ProcessId could not verify post-exit fixture shape." }
    $shape = [string]($fixtureShapeOutput | Select-Object -Last 1) | ConvertFrom-Json -ErrorAction Stop
    if ([int]$shape.classes -ne 96 -or [int]$shape.teachers -ne 24 -or
        [int]$shape.roster_data -ne 7200 -or [string]$shape.integrityCheck -cne "ok") {
        throw "$ProcessId changed the fixture's 96/24/7200 data shape or database integrity during the run."
    }
    [ordered]@{
        classCount = [int]$shape.classes
        teacherCount = [int]$shape.teachers
        rosterCellCount = [int]$shape.roster_data
        integrityCheck = [string]$shape.integrityCheck
    }
}

function Start-MeasuredProcess {
    param([string]$ApplicationPath, [string]$ApplicationHash, [string]$FixturePath,
        [string]$PreLaunchFixtureSnapshotPath, [string]$OutputRoot, [string]$UserName,
        [int]$RunIndex, [switch]$NavigationSmokeOnly)
    $fixtureHash = Get-FileSha256 $FixturePath
    $processId = "process-{0:D2}" -f $RunIndex
    $runtimeRoot = Join-Path (Join-Path $EvidenceDirectory "runtime") $processId
    $settingsRoot = Join-Path $runtimeRoot "settings"
    $localAppData = Join-Path $runtimeRoot "local-app-data"
    $appData = Join-Path $runtimeRoot "app-data"
    $runtimeTemp = Join-Path $runtimeRoot "temp"
    foreach ($directory in @($settingsRoot, $localAppData, $appData, $runtimeTemp)) {
        [void][System.IO.Directory]::CreateDirectory($directory)
    }
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $ApplicationPath
    $startInfo.WorkingDirectory = Split-Path -Parent $ApplicationPath
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    [void]$startInfo.ArgumentList.Add([IO.Path]::GetFullPath($FixturePath))
    $startInfo.Environment["CLASSMNGR_SETTINGS_ROOT"] = $settingsRoot
    $startInfo.Environment["LOCALAPPDATA"] = $localAppData
    $startInfo.Environment["APPDATA"] = $appData
    $startInfo.Environment["TEMP"] = $runtimeTemp
    $startInfo.Environment["TMP"] = $runtimeTemp
    $startInfo.Environment["QT_FORCE_STDERR_LOGGING"] = "1"
    $startInfo.Environment["PATH"] = "$($script:CurrentPackage.qtBin);$(Split-Path -Parent $ApplicationPath);$env:PATH"
    $process = [Diagnostics.Process]::new(); $process.StartInfo = $startInfo
    if (-not $process.Start()) { throw "Could not launch measured packaged process $processId." }
    $process.Refresh()
    $path = [IO.Path]::GetFullPath([P908Native]::GetProcessImagePath($process.Handle))
    $pathMatches = [string]::Equals($path, $ApplicationPath, [StringComparison]::OrdinalIgnoreCase)
    $nativeX64 = [P908Native]::IsNativeX64($process.Handle)
    $observedHash = if (Test-Path -LiteralPath $path -PathType Leaf) { Get-FileSha256 $path } else { "unavailable" }
    if (-not $pathMatches -or -not $nativeX64 -or $observedHash -cne $ApplicationHash) {
        throw "Launched process identity mismatch: expected path='$ApplicationPath' sha256='$ApplicationHash'; observed path='$path' nativeX64=$nativeX64 sha256='$observedHash'."
    }
    $created = $process.StartTime.ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss.fffffff'Z'")
    $context = [ordered]@{
        processInstanceId = $processId
        process = $process
        expectedPath = $ApplicationPath
        expectedHash = $ApplicationHash
        createdAtUtc = $created
        fixturePath = [IO.Path]::GetFullPath($FixturePath)
        fixtureHash = $fixtureHash
        outputRoot = [IO.Path]::GetFullPath($OutputRoot)
        userName = $UserName
        logEntries = [Collections.Generic.List[object]]::new()
        pageEvents = [Collections.Generic.List[object]]::new()
        routeEntries = [Collections.Generic.List[object]]::new()
        navigationIndex = 0
        classSelectionCount = 0
        classRowTargetCache = @{}
        readers = @(
            [pscustomobject]@{ Reader = $process.StandardOutput; Stream = "stdout"; ReadTask = $process.StandardOutput.ReadLineAsync() },
            [pscustomobject]@{ Reader = $process.StandardError; Stream = "stderr"; ReadTask = $process.StandardError.ReadLineAsync() }
        )
        startupReady = $null
        workspaceOpened = $null
        documentCatalog = $null
        interFontEvents = @()
    }
    $script:CurrentProcess = $context
    $script:CurrentProcesses.Add($context)
    $runManifest = [ordered]@{
        processInstanceId = $processId
        fixturePathPassedToApp = [IO.Path]::GetFullPath($FixturePath)
        fixtureSha256PassedToApp = $fixtureHash
        fixturePreLaunchSnapshotPath = [IO.Path]::GetFullPath($PreLaunchFixtureSnapshotPath)
        fixturePreLaunchSnapshotSha256 = Get-FileSha256 $PreLaunchFixtureSnapshotPath
        outputRoot = [IO.Path]::GetFullPath($OutputRoot)
        userName = $UserName
        processIdentity = [ordered]@{ pid = [Int64]$process.Id; createdAtUtc = $created; canonicalExecutablePath = $path; architecture = "x64"; sha256 = $ApplicationHash; mainWindowHandle = $null; mainWindowTitle = $null }
        commandLineArguments = @([IO.Path]::GetFullPath($FixturePath))
        runtimeEnvironment = [ordered]@{ settingsRoot = [IO.Path]::GetFullPath($settingsRoot); localAppData = [IO.Path]::GetFullPath($localAppData); appData = [IO.Path]::GetFullPath($appData); temp = [IO.Path]::GetFullPath($runtimeTemp); qtForceStderrLogging = "1" }
        startupReady = $null
        workspaceOpened = $null
        startupDocumentCatalog = $null
        interFontResourceEvents = @()
        summaryLifecycleSnapshots = @()
        baselineWorkspace = $null
        generation = $null
        navigationTransitionCount = 0
        classSelectionCount = 0
        explicitRefreshCount = 0
        suspensionCount = 0
        totalSubPrepCycles = if ($RetentionDiagnosticOnly) { $DiagnosticTotalCycles } else { 11 }
        retentionDiagnosticOnly = [bool]$RetentionDiagnosticOnly
        returnWorkingSetBytes = @()
        idleSampleCount = 0
        idleWorkingSetMaximumBytes = $null
        idlePrivateBytesMedian = $null
        normalUseBudgetPassed = $null
        outputDirectory = $null
        budgetViolations = @()
    }
    $Manifest.processRuns += $runManifest

    $deadline = [DateTimeOffset]::UtcNow.AddSeconds($ReadinessTimeoutSeconds)
    while ([DateTimeOffset]::UtcNow -lt $deadline) {
        Receive-ProcessOutput $process
        $ready = @($context.logEntries | Where-Object { $_.line -match 'ClassMngrNext startup stage=startup-ready ' })
        if ($ready.Count -gt 1) { throw "Duplicate startup-ready records in $processId." }
        if ($ready.Count -eq 1) { break }
        $process.Refresh(); if ($process.HasExited) { throw "$processId exited before startup-ready with code $($process.ExitCode)." }
        Start-Sleep -Milliseconds 100
    }
    Receive-ProcessOutput $process
    $readyLines = @($context.logEntries | Where-Object { $_.line -match '^ClassMngrNext startup stage=startup-ready outcome=(?<outcome>[a-z-]+) diagnostic=(?<diagnostic>[a-z-]+) diagnostic_stage=(?<diagnosticStage>[a-z-]+) checkpoint=(?<checkpoint>\{.*\})$' })
    if ($readyLines.Count -ne 1) { throw "$processId did not emit exactly one well-formed startup-ready record." }
    $readyMatch = [regex]::Match([string]$readyLines[0].line, '^ClassMngrNext startup stage=startup-ready outcome=(?<outcome>[a-z-]+) diagnostic=(?<diagnostic>[a-z-]+) diagnostic_stage=(?<diagnosticStage>[a-z-]+) checkpoint=(?<checkpoint>\{.*\})$')
    $readyCheckpoint = $readyMatch.Groups["checkpoint"].Value | ConvertFrom-Json -ErrorAction Stop
    if ($readyMatch.Groups["outcome"].Value -ne "completed" -or $readyMatch.Groups["diagnostic"].Value -ne "none" -or $readyMatch.Groups["diagnosticStage"].Value -ne "none" -or $readyCheckpoint.outcome -ne "completed" -or $readyCheckpoint.stage -ne "startup-ready" -or $readyCheckpoint.platform -ne "windows" -or $readyCheckpoint.sample_available -ne $true) { throw "$processId startup-ready was incomplete or invalid." }
    foreach ($key in @("working_set_bytes", "private_usage_bytes", "private_working_set_bytes", "peak_working_set_bytes", "handle_count", "thread_count")) { $value = $readyCheckpoint.metrics.$key; if ($null -eq $value -or [Int64]$value -lt 0 -or ($key -ne "handle_count" -and [Int64]$value -eq 0)) { throw "$processId startup-ready metric $key is missing or invalid." } }
    $context.startupReady = [ordered]@{ logIndex = [Int64]$context.logEntries.IndexOf($readyLines[0]); observedAtUtc = [string]$readyLines[0].observedAtUtc; checkpoint = $readyCheckpoint; rawLine = [string]$readyLines[0].line }
    $runManifest.startupReady = $context.startupReady
    $startupWS = [Int64]$readyCheckpoint.metrics.working_set_bytes
    $runManifest.startupReadyWorkingSetBytes = $startupWS
    if (-not $RetentionDiagnosticOnly -and $startupWS -ge $ThresholdBytes) { $runManifest.budgetViolations += [ordered]@{ phase = "startup-ready"; sampleIndex = 0; workingSetBytes = $startupWS; thresholdBytes = $ThresholdBytes; metricSource = "synchronous-app-checkpoint" } }

    $workspaceLines = @($context.logEntries | Where-Object { $_.line -match '^ClassMngrNext startup stage=workspace-opened ' })
    if ($workspaceLines.Count -ne 1) { throw "$processId must emit exactly one workspace-opened record." }
    $workspaceMatch = [regex]::Match([string]$workspaceLines[0].line, '^ClassMngrNext startup stage=workspace-opened outcome=(?<outcome>[a-z-]+) diagnostic=(?<diagnostic>[a-z-]+) diagnostic_stage=(?<diagnosticStage>[a-z-]+) checkpoint=(?<checkpoint>\{.*\})$')
    if (-not $workspaceMatch.Success) { throw "$processId workspace-opened record was malformed." }
    $workspaceCheckpoint = $workspaceMatch.Groups["checkpoint"].Value | ConvertFrom-Json -ErrorAction Stop
    if ($workspaceMatch.Groups["outcome"].Value -ne "completed" -or $workspaceMatch.Groups["diagnostic"].Value -ne "none" -or $workspaceMatch.Groups["diagnosticStage"].Value -ne "none" -or $workspaceCheckpoint.outcome -ne "completed" -or $workspaceCheckpoint.platform -ne "windows" -or $workspaceCheckpoint.sample_available -ne $true -or $context.logEntries.IndexOf($workspaceLines[0]) -ge $context.startupReady.logIndex) { throw "$processId workspace-opened was not a successful pre-ready event." }
    foreach ($key in @("working_set_bytes", "private_usage_bytes", "private_working_set_bytes", "peak_working_set_bytes", "handle_count", "thread_count")) { $value = $workspaceCheckpoint.metrics.$key; if ($null -eq $value -or [Int64]$value -lt 0 -or ($key -ne "handle_count" -and [Int64]$value -eq 0)) { throw "$processId workspace-opened metric $key is missing or invalid." } }
    $context.workspaceOpened = [ordered]@{ logIndex = [Int64]$context.logEntries.IndexOf($workspaceLines[0]); observedAtUtc = [string]$workspaceLines[0].observedAtUtc; checkpoint = $workspaceCheckpoint; rawLine = [string]$workspaceLines[0].line }
    $runManifest.workspaceOpened = $context.workspaceOpened

    $catalogLines = @($context.logEntries | Where-Object { $_.line -match '^ClassMngrNext document-catalog event=projected ' })
    if ($catalogLines.Count -ne 1 -or $catalogLines[0].line -notmatch '^ClassMngrNext document-catalog event=projected outcome=completed document_count=(?<docs>\d+) loaded_pdf_body_count=(?<bodies>\d+)$' -or $Matches.bodies -ne "0") { throw "$processId startup document catalog was incomplete or loaded a PDF body." }
    $context.documentCatalog = [ordered]@{ logIndex = [Int64]$context.logEntries.IndexOf($catalogLines[0]); documentCount = [Int64]$Matches.docs; loadedPdfBodyCount = 0; observedAtUtc = [string]$catalogLines[0].observedAtUtc; rawLine = [string]$catalogLines[0].line }
    $runManifest.startupDocumentCatalog = $context.documentCatalog
    if ($context.documentCatalog.logIndex -ge $context.startupReady.logIndex) { throw "$processId document catalog projection followed startup-ready." }

    $fontEvents = [Collections.Generic.List[object]]::new()
    foreach ($entry in $context.logEntries) {
        if ($entry.line -notmatch 'resource:assets/fonts/Inter') { continue }
        if ($entry.line -notmatch '^ClassMngrNext resource checkpoint=(?<json>\{.*\})$') { throw "$processId has malformed Inter resource checkpoint." }
        $font = $Matches.json | ConvertFrom-Json -ErrorAction Stop
        if ($ExpectedInterFontResourceIds -cnotcontains [string]$font.resource_id -or $font.event -ne "resource-load" -or $font.outcome -ne "succeeded" -or $font.request_scope.kind -ne "startup") { throw "$processId Inter font event identity or success state is invalid." }
        $fontEvents.Add([ordered]@{ resourceId = [string]$font.resource_id; logIndex = [Int64]$context.logEntries.IndexOf($entry); observedAtUtc = [string]$entry.observedAtUtc; rawLine = [string]$entry.line })
    }
    if ($fontEvents.Count -ne 6) { throw "$processId must have six Inter TTF startup events." }
    for ($index = 0; $index -lt 6; $index++) { if ($fontEvents[$index].resourceId -cne $ExpectedInterFontResourceIds[$index] -or $fontEvents[$index].logIndex -ge $context.startupReady.logIndex) { throw "$processId Inter startup events were missing, duplicate, unordered, or late." } }
    $context.interFontEvents = @($fontEvents); $runManifest.interFontResourceEvents = @($fontEvents)

    $windowDeadline = [DateTimeOffset]::UtcNow.AddSeconds(10)
    $stable = 0; $stableHandle = [Int64]0; $stableTitle = ""
    while ([DateTimeOffset]::UtcNow -lt $windowDeadline -and $stable -lt 3) {
        Receive-ProcessOutput $process; $process.Refresh()
        $handle = $process.MainWindowHandle; $title = $process.MainWindowTitle.Trim()
        if ($handle -ne [IntPtr]::Zero -and [P908Native]::IsWindowVisible($handle) -and $title.Contains("ClassMngr Next") -and [P908Native]::GetWindowProcessId($handle) -eq [uint32]$process.Id) {
            if ($stableHandle -eq $handle.ToInt64() -and $stableTitle -ceq $title) { $stable++ } else { $stableHandle = $handle.ToInt64(); $stableTitle = $title; $stable = 1 }
        } else { $stable = 0; $stableHandle = 0; $stableTitle = "" }
        if ($stable -lt 3) { Start-Sleep -Milliseconds 250 }
    }
    if ($stable -lt 3) { throw "$processId has no stable verified app window." }
    $runManifest.processIdentity.mainWindowHandle = $stableHandle; $runManifest.processIdentity.mainWindowTitle = $stableTitle
    $context.mainWindowHandle = $stableHandle; $context.mainWindowTitle = $stableTitle

    $initialRoutes = @(Wait-ForCondition $process { @($context.routeEntries | Where-Object { $_.line -match '^ClassMngrNext navigation event=page-route outcome=completed cause=initial from=none to=my_workspace catalog_document_count=\d+ loaded_pdf_body_count=0$' }) } "initial Workspace route" 15)
    if ($initialRoutes.Count -ne 1) { throw "$processId did not emit exactly one initial Workspace route." }
    $baselineEvents = @(Wait-ForCondition $process { @($context.pageEvents | Where-Object { $_.record.event -eq "page-lifecycle" -and $_.record.page_key -eq "my_workspace" -and $_.record.state -eq "activated" }) } "My Workspace activation snapshot" 15)
    if ($baselineEvents.Count -ne 1) { throw "$processId has no unique My Workspace activated baseline checkpoint." }
    Assert-AppProcessSnapshot $baselineEvents[0].record "$processId Workspace activation"
    $baseline = New-ProcessSample $process "workspace-baseline" 0
    $baselineDelay = ([DateTimeOffset]::Parse($baseline.timestampUtc).ToUniversalTime() - [DateTimeOffset]::Parse($baselineEvents[0].observedAtUtc).ToUniversalTime()).TotalMilliseconds
    if ($baselineDelay -lt 0 -or $baselineDelay -gt $MaxEventSampleDelayMilliseconds) { throw "$processId Workspace baseline process snapshot exceeded the event correlation window." }
    $baseline.sampleDelayMilliseconds = [Math]::Round($baselineDelay, 3)
    $baseline.appLifecycleSnapshot = $baselineEvents[0].record
    Add-JsonLine $ProcessMetricsPath $baseline
    $runManifest.baselineWorkspace = [ordered]@{ processSample = $baseline; appLifecycleSnapshot = $baselineEvents[0].record; startupLogIndex = [Int64]$baselineEvents[0].logIndex }

    if ($NavigationSmokeOnly) {
        $smokeStart = $context.logEntries.Count
        $smokeRoute = Invoke-PageRoute $process "my_workspace" "sub_prep" "Sub Prep"
        $smokeActivation = $smokeRoute.uiaAction.nativeActivation
        $smokeActivationValid = $false
        if ($smokeRoute.uiaAction.method -ceq "uia-verified-pointer-click") {
            $smokeActivationValid = $smokeActivation.foregroundVerified -eq $true -and
                [Int64]$smokeActivation.foregroundProcessId -eq [Int64]$process.Id -and
                [Int64]$smokeActivation.inputEventsSent -eq 2
        } elseif ($smokeRoute.uiaAction.method -ceq "uia-verified-window-message-click") {
            $bounds = $smokeActivation.clientBounds
            $smokeActivationValid = $smokeRoute.uiaAction.pointerAttempt.fallbackReason -ceq "foreground-not-verified" -and
                $smokeRoute.uiaAction.pointerAttempt.foregroundVerified -eq $false -and
                [Int64]$smokeActivation.windowHandle -eq [Int64]$smokeRoute.uiaAction.hwnd -and
                [Int64]$smokeActivation.expectedProcessId -eq [Int64]$process.Id -and
                [Int64]$smokeActivation.windowProcessId -eq [Int64]$process.Id -and
                $smokeActivation.waitForInputIdleSucceeded -eq $true -and
                [int]$smokeActivation.clientX -ge [int]$bounds.left -and [int]$smokeActivation.clientX -lt [int]$bounds.right -and
                [int]$smokeActivation.clientY -ge [int]$bounds.top -and [int]$smokeActivation.clientY -lt [int]$bounds.bottom -and
                $smokeActivation.mouseMoveSent -eq $true -and $smokeActivation.leftButtonDownSent -eq $true -and
                $smokeActivation.leftButtonUpSent -eq $true -and [Int64]$smokeActivation.messagesSent -eq 3
        }
        if ($smokeRoute.fromPageId -cne "my_workspace" -or $smokeRoute.toPageId -cne "sub_prep" -or
            $smokeRoute.targetAccessibleName -cne "Sub Prep" -or $smokeRoute.selectionMethod -notin @("uia-verified-pointer-click", "uia-verified-window-message-click") -or
            $smokeRoute.uiaAction.verifiedAppEvent -ne $true -or [Int64]$smokeRoute.uiaAction.uiaProcessId -ne [Int64]$process.Id -or
            [Int64]$smokeRoute.uiaAction.hwndProcessId -ne [Int64]$process.Id -or
            -not $smokeActivationValid -or
            $smokeRoute.appEventLine -notmatch '^ClassMngrNext navigation event=page-route outcome=completed cause=selection from=my_workspace to=sub_prep catalog_document_count=\d+ loaded_pdf_body_count=0$') {
            throw "UIA route smoke did not produce one authentic verified-click Sub Prep app route for the exact measured PID/HWND."
        }
        Receive-ProcessOutput $process
        $smokeRoutes = @($context.routeEntries | Where-Object { $_.line -match '^ClassMngrNext navigation event=page-route outcome=completed cause=selection from=my_workspace to=sub_prep catalog_document_count=\d+ loaded_pdf_body_count=0$' })
        if ($smokeRoutes.Count -ne 1 -or [Int64]$smokeRoutes[0].logIndex -lt $smokeStart) { throw "UIA route smoke did not contain exactly one new successful in-app Sub Prep route event." }
        $runManifest.navigationRouteSmoke = [ordered]@{ expectedTarget = "Sub Prep"; fromPageId = $smokeRoute.fromPageId; toPageId = $smokeRoute.toPageId; method = $smokeRoute.selectionMethod; appEventLogIndex = [Int64]$smokeRoute.appEventLogIndex; appEventLine = $smokeRoute.appEventLine; uiaAction = $smokeRoute.uiaAction; processSample = $smokeRoute.processSample; authenticEventCount = $smokeRoutes.Count }
    }

    if ($NavigationSmokeOnly) {
        # The route smoke already entered Sub Prep; continue that same authentic
        # route into summary, selection, refresh, and package generation.
        $entryStart = $smokeStart
        $routeIn = $smokeRoute
    } else {
        $entryStart = $context.logEntries.Count
        $routeInCycle = if ($RetentionDiagnosticOnly) { 1 } else { 0 }
        $routeIn = Invoke-PageRoute $process "my_workspace" "sub_prep" "Sub Prep" $routeInCycle
    }
    $summaryPair = Wait-SubPrepSummaryReady $process $entryStart "Sub Prep summary data ready" 30
    $summary = $summaryPair.action
    $runManifest.summaryLifecycleSnapshots += [ordered]@{
        summaryActionLogIndex = [Int64]$summary.logIndex
        summaryActionObservedAtUtc = [string]$summary.observedAtUtc
        activationSnapshotLogIndex = [Int64]$summaryPair.activation.logIndex
        activationSnapshotObservedAtUtc = [string]$summaryPair.activation.observedAtUtc
        pageLifecycleSnapshot = $summaryPair.activation.record
    }

    $tab = Get-UiaElementById (Get-MainWindow $process) "subPrepGradeTabs" "ControlType.Tab"
    $gradeTabs = @((Get-UiaDescendants $tab) | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::TabItem -and $_.Current.Name -ceq "M2" })
    if ($gradeTabs.Count -ne 1) { throw "$processId did not expose exactly one M2 grade tab for class 96." }
    if ($gradeTabs[0].Current.ProcessId -ne $process.Id) { throw "M2 grade tab belongs to another process." }
    $gradeSelected = $false
    try { $gradeSelected = [System.Windows.Automation.SelectionItemPattern]$gradeTabs[0].GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern); $gradeSelected = $gradeSelected.Current.IsSelected } catch { }
    if (-not $gradeSelected) {
        $gradeAction = Invoke-VerifiedUiaControlClick $gradeTabs[0] "M2" ([IntPtr]$process.MainWindowHandle) $process "sub-prep-grade-selection" "M2" "sub_prep" "sub_prep"
        Start-Sleep -Milliseconds 150
        $verifiedTab = @((Get-UiaDescendants (Get-UiaElementById (Get-MainWindow $process) "subPrepGradeTabs" "ControlType.Tab")) | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::TabItem -and $_.Current.Name -ceq "M2" })
        if ($verifiedTab.Count -ne 1 -or -not ([System.Windows.Automation.SelectionItemPattern]$verifiedTab[0].GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)).Current.IsSelected) { throw "UIA did not select the M2 grade tab." }
        $gradeAction.verifiedTabSelected = $true
        Add-JsonLine $UiaActionsPath $gradeAction
    }
    $saturdayFilter = Enable-SubPrepSaturdaySummaryFilter $process
    $selected96 = Invoke-SubPrepPageClick $process "96"
    $selectedMetrics = $selected96.record.page.sub_prep_runtime_metrics
    if ([Int64]$selectedMetrics.summary_class_count -ne 96 -or [Int64]$selectedMetrics.teacher_summary_count -ne 24) {
        throw "$processId Saturday summary filter did not expose the full 96-class / 24-teacher fixture before selection."
    }
    $saturdayFilter.verifiedAppEvent = $true
    $saturdayFilter.verifiedByAppEventLogIndex = [Int64]$selected96.logIndex
    $saturdayFilter.verifiedByAppEvent = $selected96.record
    $saturdayFilter.summaryClassCountAfter = [Int64]$selectedMetrics.summary_class_count
    Add-JsonLine $UiaActionsPath $saturdayFilter
    $refresh = Invoke-Refresh $process
    $runManifest.explicitRefreshCount++
    $generation = Invoke-OutputGeneration $process $OutputRoot $UserName
    $runManifest.generation = [ordered]@{ started = $generation.started.record; completed = $generation.completed.record; postGenerationSnapshots = [ordered]@{}; outputDirectory = $generation.outputDirectory; documentCount = $generation.documents.Count }
    $generationPeak = $generation.completed.record.memory.peak_working_set_bytes
    if ($null -eq $generationPeak -or [Int64]$generationPeak -le 0) { throw "$processId generation checkpoint has no process peak working set metric." }
    $runManifest.generation.processPeakWorkingSetBytesAtCompletion = [Int64]$generationPeak
    $runManifest.generation.exceedsAdvisoryCeiling = ([Int64]$generationPeak -gt [Int64]536870912)
    foreach ($delay in @("immediate", "1s", "5s")) {
        $event = $generation.postGeneration[$delay]
        $runManifest.generation.postGenerationSnapshots[$delay] = $event.externalSample
    }
    $runManifest.outputDirectory = $generation.outputDirectory
    $runManifest.generation.outputDirectory = $generation.outputDirectory
    $runManifest.generation.outputFiles = @($generation.documents)

    if ($NavigationSmokeOnly) {
        $runManifest.smokeGeneratedPackage = [ordered]@{ outputDirectory = $generation.outputDirectory; documentCount = $generation.documents.Count; documents = @($generation.documents); generationOutcome = [string]$generation.completed.record.outcome; requestedClassCount = [Int64]$generation.started.record.details.requestedClassCount }
        $process.Refresh()
        $process.CloseMainWindow() | Out-Null
        if (-not $process.WaitForExit(15000)) { $process.Kill(); $process.WaitForExit() }
        Receive-ProcessOutput $process
        $runManifest.processExitCode = [int]$process.ExitCode
        if ($runManifest.processExitCode -ne 0) { throw "UIA route/generation smoke app exited with code $($process.ExitCode)." }
        $runManifest.completedAtUtc = Get-UtcTimestamp
        $runManifest.fixtureHashAtExit = Get-FileSha256 $FixturePath
        $runManifest.fixtureShapeAtExit = Get-FixtureShapeAtExit $FixturePath $processId
        return $runManifest
    }

    $routeOutCycle = if ($RetentionDiagnosticOnly) { 1 } else { 0 }
    $routeOut = Invoke-PageRoute $process "sub_prep" "my_workspace" "My Workspace" $routeOutCycle
    $release = Wait-PageAction $process $routeOut.actionStartLogIndex "page-suspended" { param($r) $r.outcome -eq "completed" } "initial Sub Prep suspension"
    $initialCycle = if ($RetentionDiagnosticOnly) { 1 } else { 0 }
    $releaseSample = Add-PageReleaseSample $process $release $initialCycle
    $release1 = Wait-PageAction $process $routeOut.actionStartLogIndex "post-page-release-snapshot" { param($r) $r.outcome -eq "completed" -and $r.details.delay -ceq "1s" } "initial 1-second release snapshot" 10
    $release1Sample = Add-PageReleaseSample $process $release1 $initialCycle
    $release5 = Wait-PageAction $process $routeOut.actionStartLogIndex "post-page-release-snapshot" { param($r) $r.outcome -eq "completed" -and $r.details.delay -ceq "5s" } "initial 5-second release snapshot" 10
    $release5Sample = Add-PageReleaseSample $process $release5 $initialCycle
    $runManifest.suspensionCount++
    $runManifest.returnWorkingSetBytes += [Int64]$routeOut.processSample.currentWorkingSetBytes
    $normalReturnSamples = [Collections.Generic.List[object]]::new(); $normalReturnSamples.Add($routeOut.processSample)
    foreach ($sample in @($releaseSample, $release1Sample, $release5Sample)) { $normalReturnSamples.Add($sample) }

    for ($cycle = 1; $cycle -le $RepeatedCycles; $cycle++) {
        $cycleOrdinal = if ($RetentionDiagnosticOnly) { $cycle + 1 } else { $cycle }
        $routeCycle = if ($RetentionDiagnosticOnly) { $cycleOrdinal } else { 0 }
        $cycleEntryStart = $context.logEntries.Count
        $in = Invoke-PageRoute $process "my_workspace" "sub_prep" "Sub Prep" $routeCycle
        $cycleSummaryPair = Wait-SubPrepSummaryReady $process $cycleEntryStart "cycle $cycle summary ready"
        $cycleSummary = $cycleSummaryPair.action
        $runManifest.summaryLifecycleSnapshots += [ordered]@{
            summaryActionLogIndex = [Int64]$cycleSummary.logIndex
            summaryActionObservedAtUtc = [string]$cycleSummary.observedAtUtc
            activationSnapshotLogIndex = [Int64]$cycleSummaryPair.activation.logIndex
            activationSnapshotObservedAtUtc = [string]$cycleSummaryPair.activation.observedAtUtc
            pageLifecycleSnapshot = $cycleSummaryPair.activation.record
        }
        $gradeTab = Get-UiaElementById (Get-MainWindow $process) "subPrepGradeTabs" "ControlType.Tab"
        $m2 = @((Get-UiaDescendants $gradeTab) | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::TabItem -and $_.Current.Name -ceq "M2" })
        if ($m2.Count -ne 1) { throw "$processId cycle $cycle M2 tab is unavailable." }
        $m2Selected = $false
        try { $m2Selected = ([System.Windows.Automation.SelectionItemPattern]$m2[0].GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)).Current.IsSelected } catch { }
        if (-not $m2Selected) {
            $gradeClick = Invoke-VerifiedUiaControlClick $m2[0] "M2" ([IntPtr]$process.MainWindowHandle) $process "sub-prep-grade-selection" "M2" "sub_prep" "sub_prep"
            Start-Sleep -Milliseconds 150
            $gradeClick.verifiedTabSelected = $true; Add-JsonLine $UiaActionsPath $gradeClick
        }
        $select96 = Invoke-SubPrepPageClick $process "96"
        $refreshEvent = Invoke-Refresh $process
        $runManifest.explicitRefreshCount++
        $selectionMetrics = $select96.record.page.sub_prep_runtime_metrics
        if ([int]$selectionMetrics.selected_detail_note_editor_count -ne 2 -or [int]$selectionMetrics.preparation_editor_count -ne 4 -or
            [int]$selectionMetrics.active_grade_row_count -gt 96 -or [int]$selectionMetrics.summary_class_count -ne 96 -or [int]$selectionMetrics.teacher_summary_count -ne 24) { throw "$processId cycle $cycle violates Sub Prep object bounds." }
        if ([int]$refreshEvent.record.page.widget_count -ne [int]$select96.record.page.widget_count) { throw "$processId cycle $cycle widget count changed across class selection and refresh." }
        $out = Invoke-PageRoute $process "sub_prep" "my_workspace" "My Workspace" $routeCycle
        $suspend = Wait-PageAction $process $out.actionStartLogIndex "page-suspended" { param($r) $r.outcome -eq "completed" } "cycle $cycleOrdinal suspension"
        $out.processSample.cycle = $cycleOrdinal
        $suspendSample = Add-PageReleaseSample $process $suspend $cycleOrdinal
        $post1 = Wait-PageAction $process $out.actionStartLogIndex "post-page-release-snapshot" { param($r) $r.outcome -eq "completed" -and $r.details.delay -ceq "1s" } "cycle $cycle 1-second release" 10
        $post1Sample = Add-PageReleaseSample $process $post1 $cycleOrdinal
        $post5 = Wait-PageAction $process $out.actionStartLogIndex "post-page-release-snapshot" { param($r) $r.outcome -eq "completed" -and $r.details.delay -ceq "5s" } "cycle $cycle 5-second release" 10
        $post5Sample = Add-PageReleaseSample $process $post5 $cycleOrdinal
        $runManifest.suspensionCount++
        $runManifest.returnWorkingSetBytes += [Int64]$out.processSample.currentWorkingSetBytes
        $cycleSamples = [Collections.Generic.List[object]]::new(); $cycleSamples.Add($out.processSample)
        foreach ($sample in @($suspendSample, $post1Sample, $post5Sample)) { $cycleSamples.Add($sample) }
        foreach ($sample in $cycleSamples) { $normalReturnSamples.Add($sample) }
    }
    $runManifest.navigationTransitionCount = $context.navigationIndex
    $runManifest.classSelectionCount = $context.classSelectionCount
    $expectedCycles = if ($RetentionDiagnosticOnly) { $DiagnosticTotalCycles } else { 11 }
    $runManifest.totalSubPrepCycles = $expectedCycles
    if ($context.navigationIndex -ne (2 * $expectedCycles) -or $context.classSelectionCount -ne $expectedCycles -or $runManifest.explicitRefreshCount -ne $expectedCycles -or $runManifest.suspensionCount -ne $expectedCycles) { throw "$processId did not complete $expectedCycles authentic Sub Prep generation/navigation cycles." }
    $returnValues = @($runManifest.returnWorkingSetBytes | ForEach-Object { [Int64]$_ })
    $runManifest.returnWorkingSetMaximumBytes = [Int64](($returnValues | Measure-Object -Maximum).Maximum)
    $runManifest.returnWorkingSetAboveNormalThresholdCount = @($returnValues | Where-Object { $_ -ge $ThresholdBytes }).Count

    $idleRows = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt $IdleSampleCount; $index++) {
        $sample = New-ProcessSample $process "idle" $index
        $idleRows.Add($sample); Add-JsonLine $ProcessMetricsPath $sample
        if (-not $RetentionDiagnosticOnly -and [Int64]$sample.currentWorkingSetBytes -ge $ThresholdBytes) { $runManifest.budgetViolations += [ordered]@{ phase = "idle"; sampleIndex = $index; workingSetBytes = [Int64]$sample.currentWorkingSetBytes; thresholdBytes = $ThresholdBytes; metricSource = "external-process-snapshot" } }
        if ($index -lt ($IdleSampleCount - 1)) { Start-Sleep -Seconds $SampleIntervalSeconds }
    }
    $idleValues = @($idleRows | ForEach-Object { [Int64]$_.currentWorkingSetBytes })
    $idleMax = ($idleValues | Measure-Object -Maximum).Maximum
    $idleMedianPrivate = [Int64](($idleRows | ForEach-Object { [Int64]$_.privateBytes } | Sort-Object)[[int][Math]::Floor($IdleSampleCount / 2)])
    $runManifest.idleSampleCount = $idleRows.Count
    $runManifest.idleWorkingSetMaximumBytes = [Int64]$idleMax
    $runManifest.idlePrivateBytesMedian = $idleMedianPrivate
    if (-not $RetentionDiagnosticOnly) { $runManifest.normalUseBudgetPassed = ($runManifest.budgetViolations.Count -eq 0) }
    $runManifest.idleStartedAtUtc = [string]$idleRows[0].timestampUtc
    $runManifest.idleCompletedAtUtc = [string]$idleRows[-1].timestampUtc
    $runManifest.idleSamples = @($idleRows)
    $process.Refresh()
    $process.CloseMainWindow() | Out-Null
    if (-not $process.WaitForExit(15000)) { $process.Kill(); $process.WaitForExit() }
    Receive-ProcessOutput $process
    $runManifest.processExitCode = [int]$process.ExitCode
    $runManifest.completedAtUtc = Get-UtcTimestamp
    $runManifest.fixtureHashAtExit = Get-FileSha256 $FixturePath
    $runManifest.fixtureShapeAtExit = Get-FixtureShapeAtExit $FixturePath $processId
    if ($runManifest.processExitCode -ne 0) { throw "$processId exited with nonzero code $($runManifest.processExitCode)." }
    $runManifest
}

$script:CurrentProcesses = [Collections.Generic.List[object]]::new()
$script:GeneratedFiles = [Collections.Generic.List[object]]::new()
$script:CurrentProcess = $null
$script:CurrentPackage = $null

try {
    if ($PSVersionTable.PSVersion.Major -lt 7) { throw "P9-08 requires PowerShell 7 or later." }
    if (-not $IsWindows -or -not [Environment]::Is64BitOperatingSystem) { throw "P9-08 requires native Windows x64." }
    $selfProcess = [Diagnostics.Process]::GetCurrentProcess()
    try {
        $queriedSelfPath = [IO.Path]::GetFullPath([P908Native]::GetProcessImagePath($selfProcess.Handle))
        $expectedSelfPath = [IO.Path]::GetFullPath($selfProcess.MainModule.FileName)
        if (-not [string]::Equals($queriedSelfPath, $expectedSelfPath, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Native process image path query returned '$queriedSelfPath' for PowerShell executable '$expectedSelfPath'."
        }
    } finally { $selfProcess.Dispose() }
    if (-not (Test-Path -LiteralPath $BuildDirectory -PathType Container)) { throw "Release build directory does not exist: $BuildDirectory" }
    $revisionOutput = & git -C $RepositoryRoot rev-parse --verify HEAD 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Could not read local Git HEAD." }
    $revision = ([string]($revisionOutput | Select-Object -Last 1)).Trim().ToLowerInvariant()
    if ($revision -notmatch '^(?:[0-9a-f]{40}|[0-9a-f]{64})$') { throw "Local HEAD is not a complete Git object ID." }
    $SourceAtStart = Get-CandidateSourceIdentity $RepositoryRoot $revision
    $Manifest.revision = $revision
    $Manifest.candidateSource = [ordered]@{ revision = $revision; sha256 = $SourceAtStart.sha256; includedRoots = $SourceAtStart.includedRoots; fileCount = $SourceAtStart.fileCount; inventoryFile = "source-files.json"; sha256AtRunEnd = $null; stableDuringRun = $false }
    if ($PreparedReleaseEvidenceDirectory) {
        if ($script:RunnerIntegrityLevel -ne "standard") { throw "Verified prebuilt reuse is reserved for a standard-integrity UIA launch." }
        $preparedManifestPath = Join-Path $PreparedReleaseEvidenceDirectory "prepared-release.json"
        $preparedSourcePath = Join-Path $PreparedReleaseEvidenceDirectory "source-files.json"
        $preparedPackageInventoryPath = Join-Path $PreparedReleaseEvidenceDirectory "package-files.json"
        $script:PreparedBuildLogPath = Join-Path $PreparedReleaseEvidenceDirectory "cmake-build.log"
        $script:PreparedPackageDirectory = Join-Path $PreparedReleaseEvidenceDirectory "package"
        $script:PreparedFixturePath = Join-Path $PreparedReleaseEvidenceDirectory "representative-96-class-master.tps"
        foreach ($requiredPath in @($preparedManifestPath, $preparedSourcePath, $preparedPackageInventoryPath, $script:PreparedBuildLogPath, $script:PreparedFixturePath)) {
            if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) { throw "Prepared Release evidence file is missing: $requiredPath" }
        }
        if (-not (Test-Path -LiteralPath $script:PreparedPackageDirectory -PathType Container)) { throw "Prepared Release package directory is missing." }
        $script:PreparedRelease = Get-Content -LiteralPath $preparedManifestPath -Raw | ConvertFrom-Json -ErrorAction Stop
        $preparedSourceDocument = Get-Content -LiteralPath $preparedSourcePath -Raw | ConvertFrom-Json -ErrorAction Stop
        $preparedPackageDocument = Get-Content -LiteralPath $preparedPackageInventoryPath -Raw | ConvertFrom-Json -ErrorAction Stop
        $script:PreparedSourceFiles = @($preparedSourceDocument.files)
        $script:PreparedPackageFiles = @($preparedPackageDocument.files)
        $preparedManifestCopyPath = Join-Path $EvidenceDirectory "prepared-release.json"
        Copy-Item -LiteralPath $preparedManifestPath -Destination $preparedManifestCopyPath
        $Manifest.files.preparedReleaseAttestation = "prepared-release.json"
        if ($script:PreparedRelease.schema -cne "classmngr-phase9-p908-prepared-release-v1" -or $script:PreparedRelease.integrityLevel -cne "elevated" -or $null -eq $script:PreparedRelease.integrityLevelEvidence) { throw "Prepared Release attestation is missing or was not produced at elevated integrity." }
        if ($script:PreparedRelease.integrityLevelEvidence.level -cne "elevated" -or $script:PreparedRelease.integrityLevelEvidence.kind -notin @("windows-token-observed", "main-agent-confirmed-elevated-build") -or [string]::IsNullOrWhiteSpace([string]$script:PreparedRelease.integrityLevelEvidence.reference)) { throw "Prepared Release integrity-level provenance is incomplete." }
        $preparedSourceSorted = Sort-InventoryRowsOrdinal $script:PreparedSourceFiles
        $preparedSourceCanonical = @($preparedSourceSorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
        if ($script:PreparedRelease.source.sha256 -cne (Get-StringSha256 ($preparedSourceCanonical + "`n"))) { throw "Prepared source inventory does not match its attested canonical digest." }
        $script:ReuseSourceDifferences = @(Compare-PreparedSourceInventory @($SourceAtStart.files) $script:PreparedSourceFiles)
        if (@($script:ReuseSourceDifferences | Where-Object { -not ([string]$_.path).StartsWith("scripts/phase9/", [StringComparison]::OrdinalIgnoreCase) }).Count -gt 0) { throw "Prepared Release source differs outside the allowed Phase 9 runner inventory." }
        $Manifest.candidateSource.preparedSourceFingerprint = [string]$script:PreparedRelease.source.sha256
        $Manifest.candidateSource.reuseDifferences = @($script:ReuseSourceDifferences)
        Write-JsonFile $SourceFilesPath ([ordered]@{ files = @($SourceAtStart.files); preparedFiles = @($script:PreparedSourceFiles) })
    } else {
        Write-JsonFile $SourceFilesPath ([ordered]@{ files = @($SourceAtStart.files) })
    }

    $cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { throw "Release CMakeCache.txt is missing." }
    $cacheHash = Get-FileSha256 $cachePath
    $cacheLines = @(Get-Content -LiteralPath $cachePath)
    if ([IO.Path]::GetFullPath((Get-RequiredCacheValue $cacheLines "CMAKE_HOME_DIRECTORY")) -cne $RepositoryRoot) { throw "Release CMakeCache source root differs from repository." }
    if ((Get-RequiredCacheValue $cacheLines "CLASSMNGR_NEXT_RELEASE_PACKAGE") -ne "ON") { throw "Release package option is not enabled." }
    $compiler = Get-ReleaseCompilerConfiguration $cacheLines $BuildDirectory
    $cmakePath = [IO.Path]::GetFullPath((Get-RequiredCacheValue $cacheLines "CMAKE_COMMAND"))
    if (-not (Test-Path -LiteralPath $cmakePath -PathType Leaf)) { throw "CMake executable recorded in cache is missing." }
    $Manifest.configuration = [ordered]@{ buildDirectory = $BuildDirectory; cmakeCachePath = [IO.Path]::GetFullPath($cachePath); cmakeCacheSha256 = $cacheHash; releaseCompiler = $compiler; packageOption = "CLASSMNGR_NEXT_RELEASE_PACKAGE=ON"; runnerIntegrityLevel = $script:RunnerIntegrityLevel }

    $targets = @("ClassMngrNextResourcePackage", "ClassMngrPhase7NextBlankFixtureTests", "ClassMngrStartupPerformanceTests")
    if ($PreparedReleaseEvidenceDirectory) {
        $preparedBuild = $script:PreparedRelease.build
        if ($preparedBuild.succeeded -ne $true -or [int]$preparedBuild.exitCode -ne 0 -or (([string[]]$preparedBuild.targets -join "`n") -cne ($targets -join "`n"))) { throw "Prepared Release evidence does not prove successful completion of all required targets." }
        if ([string]$script:PreparedRelease.cache.sha256 -cne $cacheHash) { throw "Prepared Release CMakeCache hash differs from the current standard tree." }
        if ((ConvertTo-Json -InputObject $script:PreparedRelease.compiler -Depth 12 -Compress) -cne (ConvertTo-Json -InputObject $compiler -Depth 12 -Compress)) { throw "Prepared Release compiler/project configuration differs from the current generated Release tree." }
        $buildLogHash = Get-FileSha256 $script:PreparedBuildLogPath
        if ($buildLogHash -cne [string]$preparedBuild.logSha256) { throw "Prepared elevated Release build log hash does not match its attestation." }
        $buildLogText = Get-Content -LiteralPath $script:PreparedBuildLogPath -Raw
        foreach ($requiredTarget in $targets) { if ($buildLogText -notmatch [regex]::Escape($requiredTarget)) { throw "Prepared elevated build log omits required target $requiredTarget." } }
        if ($buildLogText -notmatch '(?m)^exitCode: 0\s*$') { throw "Prepared elevated Release build log has no successful exit marker." }
        Copy-Item -LiteralPath $script:PreparedBuildLogPath -Destination $BuildLogPath
        $Manifest.configuration.packageBuild = [ordered]@{ mode = "verified-prebuilt-reuse"; targets = $targets; exitCode = 0; succeeded = $true; integrityLevel = "elevated"; runnerIntegrityLevel = $script:RunnerIntegrityLevel; integrityLevelsDifferent = $true; logPath = [IO.Path]::GetFullPath($BuildLogPath); logSha256 = $buildLogHash; reuse = [ordered]@{ preparedEvidenceDirectory = $PreparedReleaseEvidenceDirectory; preparedManifestPath = [IO.Path]::GetFullPath($preparedManifestCopyPath); preparedManifestSha256 = Get-FileSha256 $preparedManifestCopyPath; integrityLevelEvidence = $script:PreparedRelease.integrityLevelEvidence; preparedSourceSha256 = [string]$script:PreparedRelease.source.sha256; preparedPackageSha256 = [string]$script:PreparedRelease.package.sha256; preparedApplicationSha256 = [string]$script:PreparedRelease.application.sha256; preparedFixtureGeneratorSha256 = [string]$script:PreparedRelease.fixtureGenerator.sha256; preparedFixtureSha256 = [string]$script:PreparedRelease.fixture.sha256; preparedCMakeCacheSha256 = [string]$script:PreparedRelease.cache.sha256; phase9OnlySourceDifferences = @($script:ReuseSourceDifferences) } }
    } else {
        $buildStarted = Get-UtcTimestamp
        $buildOutput = @(& $cmakePath --build $BuildDirectory --config Release --verbose --target @targets 2>&1 | ForEach-Object { [string]$_ })
        $buildExit = $LASTEXITCODE
        [IO.File]::WriteAllLines($BuildLogPath, @("command: cmake --build `"$BuildDirectory`" --config Release --verbose --target $($targets -join ' ')", "startedAtUtc: $buildStarted", $buildOutput, "exitCode: $buildExit", "completedAtUtc: $(Get-UtcTimestamp)"), [Text.UTF8Encoding]::new($false))
        $Manifest.configuration.packageBuild = [ordered]@{ mode = "local-build"; targets = $targets; exitCode = $buildExit; succeeded = ($buildExit -eq 0); integrityLevel = $script:RunnerIntegrityLevel; runnerIntegrityLevel = $script:RunnerIntegrityLevel; integrityLevelsDifferent = $false; logPath = [IO.Path]::GetFullPath($BuildLogPath); logSha256 = Get-FileSha256 $BuildLogPath }
        if ($buildExit -ne 0) { throw "Required Release package/fixture targets failed; see cmake-build.log." }
        $compilerAfterBuild = Get-ReleaseCompilerConfiguration @(Get-Content -LiteralPath $cachePath) $BuildDirectory
        if ($compilerAfterBuild.projectSha256 -ne $compiler.projectSha256 -or $compilerAfterBuild.projectOptimizationSetting -ne $compiler.projectOptimizationSetting) { throw "Generated Release project optimization changed during package build." }
    }

    $qt6Dir = [IO.Path]::GetFullPath((Get-RequiredCacheValue $cacheLines "Qt6_DIR"))
    $derivedPrefix = [IO.Path]::GetFullPath((Join-Path $qt6Dir "..\..\.."))
    if ([string]::IsNullOrWhiteSpace($QtPrefix)) { $QtPrefix = $derivedPrefix } else { $QtPrefix = [IO.Path]::GetFullPath($QtPrefix); if ($QtPrefix -ine $derivedPrefix) { throw "QtPrefix does not match Qt6_DIR in CMakeCache." } }
    $qtBin = Join-Path $QtPrefix "bin"; $windeployqt = Join-Path $qtBin "windeployqt.exe"
    if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) { throw "windeployqt is missing from the cache-bound Qt installation." }
    $qtVersion = Get-QtVersion (Join-Path $qtBin "Qt6Core.dll")
    if ((Get-QtVersion $windeployqt) -ne $qtVersion) { throw "windeployqt version differs from Qt6Core." }
    $Manifest.qtInstallation = [ordered]@{ version = $qtVersion; prefix = $QtPrefix; qt6Directory = $qt6Dir; coreDllPath = (Join-Path $qtBin "Qt6Core.dll"); coreDllSha256 = Get-FileSha256 (Join-Path $qtBin "Qt6Core.dll"); windeployqtPath = $windeployqt; windeployqtSha256 = Get-FileSha256 $windeployqt; runtimeFiles = @(); fixtureGenerators = @() }

    $releaseExe = Join-Path $BuildDirectory "next-package-build\Release\ClassMngr.exe"
    if (-not (Test-Path -LiteralPath $releaseExe -PathType Leaf) -or (Get-PeArchitecture $releaseExe) -ne "x64") { throw "Packaged Release output is missing or not x64: $releaseExe" }
    if ($PreparedReleaseEvidenceDirectory) {
        $currentReleaseHash = Get-FileSha256 $releaseExe
        if ([IO.Path]::GetFullPath([string]$script:PreparedRelease.application.path) -ine [IO.Path]::GetFullPath($releaseExe) -or [string]$script:PreparedRelease.application.sha256 -cne $currentReleaseHash -or [string]$script:PreparedRelease.application.architecture -cne "x64") { throw "Prepared application path/hash/architecture differs from the current optimized Release executable." }
        $fixtureGeneratorCheck = Join-Path $BuildDirectory "Release\ClassMngrStartupPerformanceTests.exe"
        if (-not (Test-Path -LiteralPath $fixtureGeneratorCheck -PathType Leaf) -or [string]$script:PreparedRelease.fixtureGenerator.path -ine [IO.Path]::GetFullPath($fixtureGeneratorCheck) -or [string]$script:PreparedRelease.fixtureGenerator.sha256 -cne (Get-FileSha256 $fixtureGeneratorCheck) -or [string]$script:PreparedRelease.fixtureGenerator.architecture -cne "x64") { throw "Prepared fixture-generator identity differs from the current Release build." }
        if ((Get-FileSha256 $script:PreparedFixturePath) -cne [string]$script:PreparedRelease.fixture.sha256 -or [int]$script:PreparedRelease.fixture.classCount -ne 96 -or [int]$script:PreparedRelease.fixture.teacherCount -ne 24 -or [int]$script:PreparedRelease.fixture.rosterCellCount -ne 7200) { throw "Prepared 96-class fixture identity/shape is invalid." }
        if ([string]$script:PreparedRelease.qt.prefix -ine $QtPrefix -or [string]$script:PreparedRelease.qt.version -cne $qtVersion) { throw "Prepared package Qt installation differs from the current CMakeCache-bound Qt runtime." }
    }
    $packageDirectory = Join-Path $EvidenceDirectory "package"
    if (Test-Path -LiteralPath $packageDirectory) { throw "Refusing to overwrite package stage." }
    [void][IO.Directory]::CreateDirectory($packageDirectory)
    if ($PreparedReleaseEvidenceDirectory) {
        $preparedPackageInventory = Get-PackageInventory $script:PreparedPackageDirectory
        if ($preparedPackageInventory.sha256 -cne [string]$script:PreparedRelease.package.sha256 -or $preparedPackageInventory.fileCount -ne [int]$script:PreparedRelease.package.fileCount) { throw "Prepared package directory differs from the elevated package inventory attestation." }
        $preparedPackageSorted = Sort-InventoryRowsOrdinal $script:PreparedPackageFiles
        $preparedPackageCanonical = @($preparedPackageSorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
        if ((Get-StringSha256 ($preparedPackageCanonical + "`n")) -cne $preparedPackageInventory.sha256) { throw "Prepared package inventory file does not match the actual package directory." }
        Get-ChildItem -LiteralPath $script:PreparedPackageDirectory -Force | Copy-Item -Destination $packageDirectory -Recurse -Force
    } else {
        Invoke-Checked $cmakePath @("--install", $BuildDirectory, "--config", "Release", "--prefix", $packageDirectory, "--component", "ClassMngrNextResources") "Installing Next package resources"
    }
    $catalogPaths = @(Get-ChildItem -LiteralPath $packageDirectory -Filter "resource-catalog.json" -File -Recurse)
    if ($catalogPaths.Count -ne 1) { throw "Expected one resource catalog in installed package." }
    $resourceDirectory = Split-Path -Parent $catalogPaths[0].FullName
    $resourceCatalog = Get-Content -LiteralPath $catalogPaths[0].FullName -Raw | ConvertFrom-Json -ErrorAction Stop
    $fontEntries = [Collections.Generic.List[object]]::new()
    foreach ($resourceId in $ExpectedInterFontResourceIds) {
        $records = @($resourceCatalog.records | Where-Object { $_.logical_id -ceq $resourceId })
        if ($records.Count -ne 1) { throw "Package resource catalog does not have exactly one $resourceId record." }
        $record = $records[0]; $sourceRelative = [string]$record.provenance.source.path; $packageRelative = [string]$record.package_relative_path
        $sourceFont = Join-Path $RepositoryRoot ($sourceRelative.Replace('/', '\')); $packagedFont = Join-Path $resourceDirectory ($packageRelative.Replace('/', '\'))
        if ($record.resource_kind -ne "required_ui_font" -or $record.expected_type -ne "ttf" -or -not (Test-Path -LiteralPath $sourceFont -PathType Leaf) -or -not (Test-Path -LiteralPath $packagedFont -PathType Leaf)) { throw "Invalid cataloged Inter resource $resourceId." }
        $sourceHash = Get-FileSha256 $sourceFont; $packageHash = Get-FileSha256 $packagedFont
        if ($sourceHash -cne [string]$record.provenance.source.sha256 -or $packageHash -cne [string]$record.provenance.baked.sha256 -or $sourceHash -cne $packageHash) { throw "Inter source/package font hash mismatch: $resourceId." }
        $fontEntries.Add([ordered]@{ resourceId = $resourceId; sourcePath = $sourceRelative; packageRelativePath = $packageRelative; sizeBytes = [Int64](Get-Item $sourceFont).Length; sha256 = $sourceHash })
    }
    $licenseSource = Join-Path $RepositoryRoot "licenses\fonts\inter\LICENSE.txt"
    $licensePackage = Join-Path $packageDirectory "licenses\fonts\inter\LICENSE.txt"
    if (-not (Test-Path -LiteralPath $licensePackage -PathType Leaf) -or (Get-FileSha256 $licenseSource) -cne (Get-FileSha256 $licensePackage)) { throw "Packaged Inter OFL license identity is missing or changed." }
    $applicationPath = Join-Path $packageDirectory "ClassMngrNext.exe"
    if (-not (Test-Path -LiteralPath $applicationPath -PathType Leaf)) { Copy-Item -LiteralPath $releaseExe -Destination $applicationPath }
    if ((Get-FileSha256 $applicationPath) -cne (Get-FileSha256 $releaseExe)) { throw "Staged executable differs from Release package target." }
    $oldPath = [Environment]::GetEnvironmentVariable("PATH", "Process")
    try {
        [Environment]::SetEnvironmentVariable("PATH", "$qtBin;$oldPath", "Process")
        if (-not $PreparedReleaseEvidenceDirectory) { Invoke-Checked $windeployqt @("--qmldir", (Join-Path $RepositoryRoot "src\features\calendar\ui\qml"), "--skip-plugin-types", "qmltooling", "--include-plugins", "qoffscreen,qsqlite", "--exclude-plugins", "qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql", "--translations", "en,ko", "--compiler-runtime", "--dir", $packageDirectory, $applicationPath) "Deploying Qt runtime" }
    } finally { [Environment]::SetEnvironmentVariable("PATH", $oldPath, "Process") }
    foreach ($runtime in @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Sql.dll", "platforms\qwindows.dll", "sqldrivers\qsqlite.dll")) { if (-not (Test-Path -LiteralPath (Join-Path $packageDirectory $runtime) -PathType Leaf)) { throw "Missing staged Qt runtime $runtime." } }
    $Manifest.qtInstallation.runtimeFiles = @(Get-PackagedQtRuntimeIdentity $packageDirectory $QtPrefix)
    $packageInventory = Get-PackageInventory $packageDirectory
    Write-JsonFile $PackageFilesPath ([ordered]@{ files = @($packageInventory.files) })
    $appHash = Get-FileSha256 $applicationPath
    $Manifest.application = [ordered]@{ canonicalPath = [IO.Path]::GetFullPath($applicationPath); sha256 = $appHash; architecture = "x64"; releaseExecutablePath = [IO.Path]::GetFullPath($releaseExe); releaseExecutableSha256 = Get-FileSha256 $releaseExe; candidateSourceSha256 = $SourceAtStart.sha256 }
    $Manifest.package = [ordered]@{ directory = [IO.Path]::GetFullPath($packageDirectory); sha256 = $packageInventory.sha256; fileCount = $packageInventory.fileCount; applicationRelativePath = "ClassMngrNext.exe"; applicationSha256 = $appHash; candidateSourceSha256 = $SourceAtStart.sha256; sourceFingerprintAtBuild = if ($script:PreparedReleaseEvidenceDirectory) { [string]$script:PreparedRelease.source.sha256 } else { $SourceAtStart.sha256 }; sourceIdentityMode = if ($script:PreparedReleaseEvidenceDirectory) { "verified-prebuilt-non-phase9-equivalence" } else { "current-source-build" }; resourceCatalogRelativePath = [IO.Path]::GetRelativePath($packageDirectory, $catalogPaths[0].FullName).Replace('\', '/'); interFontCatalogEntries = @($fontEntries); interFontLicense = [ordered]@{ sourcePath = "licenses/fonts/inter/LICENSE.txt"; packagePath = "licenses/fonts/inter/LICENSE.txt"; sizeBytes = [Int64](Get-Item $licenseSource).Length; sha256 = Get-FileSha256 $licenseSource } }
    $script:CurrentPackage = [ordered]@{ qtPrefix = $QtPrefix; qtBin = $qtBin; stageDirectory = $packageDirectory }

    $fixtureGenerator = Join-Path $BuildDirectory "Release\ClassMngrStartupPerformanceTests.exe"
    if (-not (Test-Path -LiteralPath $fixtureGenerator -PathType Leaf) -or (Get-PeArchitecture $fixtureGenerator) -ne "x64") { throw "Optimized representative fixture generator is missing or not x64." }
    $generatorDirectory = Split-Path -Parent $fixtureGenerator
    $colocatedQtDlls = [Collections.Generic.List[object]]::new()
    foreach ($dll in @(Get-ChildItem -LiteralPath $generatorDirectory -Filter "Qt6*.dll" -File)) {
        $sameQtDll = Join-Path $qtBin $dll.Name
        if (-not (Test-Path -LiteralPath $sameQtDll -PathType Leaf) -or (Get-FileSha256 $dll.FullName) -cne (Get-FileSha256 $sameQtDll)) { throw "Fixture generator's colocated Qt DLL does not match the CMakeCache-bound Qt installation: $($dll.Name)." }
        $colocatedQtDlls.Add([ordered]@{ fileName = $dll.Name; sizeBytes = [Int64]$dll.Length; sha256 = Get-FileSha256 $dll.FullName })
    }
    $Manifest.qtInstallation.fixtureGenerators = @([ordered]@{ id = "representative-96-class-fixture-generator"; executablePath = [IO.Path]::GetFullPath($fixtureGenerator); executableSha256 = Get-FileSha256 $fixtureGenerator; runtimeSearchPathPrefix = [IO.Path]::GetFullPath($qtBin); colocatedQtDlls = @($colocatedQtDlls); verifiedSameQtInstallation = $true })
    $fixtureDirectory = Join-Path $EvidenceDirectory "fixtures"
    [void][IO.Directory]::CreateDirectory($fixtureDirectory)
    $masterFixture = Join-Path $fixtureDirectory "representative-96-class-master.tps"
    if ($PreparedReleaseEvidenceDirectory) {
        Copy-Item -LiteralPath $script:PreparedFixturePath -Destination $masterFixture
        if ((Get-FileSha256 $masterFixture) -cne [string]$script:PreparedRelease.fixture.sha256) { throw "Copied prepared representative fixture differs from its elevated attestation." }
    } else {
        Invoke-FixtureGenerator $fixtureGenerator $masterFixture $qtBin
    }
    $fixtureCode = @'
import json, sqlite3, sys
connection = sqlite3.connect(sys.argv[1])
tables = ("classes", "teachers", "roster_data")
counts = {name: connection.execute(f"select count(*) from {name}").fetchone()[0] for name in tables}
print(json.dumps(counts, sort_keys=True))
connection.close()
'@
    $shapeOutput = & python -c $fixtureCode $masterFixture
    if ($LASTEXITCODE -ne 0) { throw "Could not verify representative fixture database shape." }
    $shape = [string]($shapeOutput | Select-Object -Last 1) | ConvertFrom-Json -ErrorAction Stop
    if ([int]$shape.classes -ne 96 -or [int]$shape.teachers -ne 24 -or [int]$shape.roster_data -ne 7200) { throw "Representative fixture did not match the required 96/24/7200 shape." }
    $masterHash = Get-FileSha256 $masterFixture
    $Manifest.fixture = [ordered]@{ id = "synthetic-large-startup-96-classes"; masterPath = [IO.Path]::GetFullPath($masterFixture); sha256 = $masterHash; classCount = [int]$shape.classes; teacherCount = [int]$shape.teachers; rosterCellCount = [int]$shape.roster_data; generatorPath = [IO.Path]::GetFullPath($fixtureGenerator); generatorSha256 = Get-FileSha256 $fixtureGenerator; generatorFunction = "largeStartupFixtureIsCompleteAndDeterministic"; perProcessCopies = @() }
    $fixturePreparationHelper = Join-Path $PSScriptRoot "prepare_sub_prep_output_fixture.py"
    if (-not (Test-Path -LiteralPath $fixturePreparationHelper -PathType Leaf)) { throw "Source-bound weekday package fixture transform is missing: $fixturePreparationHelper" }
    $fixturePreparationHelperHash = Get-FileSha256 $fixturePreparationHelper
    $Manifest.fixture.outputPreparation = [ordered]@{ transformId = "large-sub-prep-output-weekday-fixture-v1"; helperPath = [IO.Path]::GetFullPath($fixturePreparationHelper); helperSha256 = $fixturePreparationHelperHash; selectedPackageDays = @("Monday", "Tuesday", "Wednesday", "Thursday", "Friday"); weekdayClassCount = 30; saturdayClassCount = 66; requestedPackageClassCount = 30; conflictsInSelectedDays = 0 }

    $processCount = if ($RouteSmokeOnly -or $RetentionDiagnosticOnly) { 1 } else { $ExpectedProcesses }
    for ($runIndex = 1; $runIndex -le $processCount; $runIndex++) {
        $processFixtureDirectory = Join-Path $fixtureDirectory ("process-{0:D2}" -f $runIndex)
        [void][IO.Directory]::CreateDirectory($processFixtureDirectory)
        $processFixture = Join-Path $processFixtureDirectory "representative-96-class.tps"
        Copy-Item -LiteralPath $masterFixture -Destination $processFixture
        if ((Get-FileSha256 $processFixture) -cne $masterHash) { throw "Per-process fixture copy differs from the identical master fixture." }
        $preparationOutput = & python $fixturePreparationHelper --fixture $processFixture --expected-source-sha256 $masterHash 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Could not prepare the source-bound weekday output fixture: $($preparationOutput -join ' ')" }
        $preparation = [string]($preparationOutput | Select-Object -Last 1) | ConvertFrom-Json -ErrorAction Stop
        $preparedFixtureHash = Get-FileSha256 $processFixture
        if ([string]$preparation.transformId -cne "large-sub-prep-output-weekday-fixture-v1" -or
            [string]$preparation.sourceSha256 -cne $masterHash -or
            [string]$preparation.preparedSha256 -cne $preparedFixtureHash -or
            [int]$preparation.classCount -ne 96 -or [int]$preparation.teacherCount -ne 24 -or
            [int]$preparation.rosterCellCount -ne 7200 -or [int]$preparation.weekdayClassCount -ne 30 -or
            [int]$preparation.saturdayClassCount -ne 66 -or [int]$preparation.requestedPackageClassCount -ne 30 -or
            [int]$preparation.weekdayTimeConflictCount -ne 0) { throw "Per-process output fixture transform identity/shape is invalid." }
        $preLaunchFixtureSnapshotPath = Join-Path $processFixtureDirectory "representative-96-class.pre-launch.tps"
        if (Test-Path -LiteralPath $preLaunchFixtureSnapshotPath) { throw "Refusing to overwrite pre-launch fixture snapshot $preLaunchFixtureSnapshotPath." }
        Copy-Item -LiteralPath $processFixture -Destination $preLaunchFixtureSnapshotPath
        $preLaunchFixtureSnapshotHash = Get-FileSha256 $preLaunchFixtureSnapshotPath
        if ($preLaunchFixtureSnapshotHash -cne $preparedFixtureHash) { throw "Preserved pre-launch fixture snapshot differs from the transformed app fixture." }
        $processOutputRoot = Join-Path (Join-Path $EvidenceDirectory "outputs") ("process-{0:D2}" -f $runIndex)
        if (Test-Path -LiteralPath $processOutputRoot) { throw "Refusing to overwrite unique output root $processOutputRoot." }
        $processOutputName = "P908 Run {0:D2}" -f $runIndex
        $Manifest.fixture.perProcessCopies += [ordered]@{ processInstanceId = ("process-{0:D2}" -f $runIndex); path = [IO.Path]::GetFullPath($processFixture); sourceFixtureSha256 = $masterHash; sha256BeforeLaunch = $preparedFixtureHash; preLaunchSnapshotPath = [IO.Path]::GetFullPath($preLaunchFixtureSnapshotPath); preLaunchSnapshotSha256 = $preLaunchFixtureSnapshotHash; outputPreparation = $preparation }
        $processResult = Start-MeasuredProcess $applicationPath $appHash $processFixture $preLaunchFixtureSnapshotPath $processOutputRoot $processOutputName $runIndex -NavigationSmokeOnly:$RouteSmokeOnly
        if ($RouteSmokeOnly) {
            if ($processResult.navigationRouteSmoke.authenticEventCount -ne 1 -or $processResult.smokeGeneratedPackage.generationOutcome -cne "completed" -or
                [Int64]$processResult.smokeGeneratedPackage.requestedClassCount -ne 30 -or [Int64]$processResult.smokeGeneratedPackage.documentCount -le 0 -or
                $processResult.processExitCode -ne 0 -or $processResult.fixtureShapeAtExit.classCount -ne 96 -or
                $processResult.fixtureShapeAtExit.teacherCount -ne 24 -or $processResult.fixtureShapeAtExit.rosterCellCount -ne 7200 -or
                $processResult.fixtureShapeAtExit.integrityCheck -cne "ok") { throw "UIA route smoke did not verify one authentic Sub Prep route, a successful weekday package, and the preserved 96/24/7200 workspace shape." }
            Write-JsonFile (Join-Path $EvidenceDirectory "route-smoke.json") ([ordered]@{ process = $processResult; generatedFiles = @($script:GeneratedFiles); runnerIntegrityLevel = $script:RunnerIntegrityLevel; buildIntegrityLevel = $Manifest.configuration.packageBuild.integrityLevel; integrityLevelsDifferent = $Manifest.configuration.packageBuild.integrityLevelsDifferent })
        }
        Write-JsonFile $ManifestPath $Manifest
    }
    Write-JsonFile $GeneratedFilesPath ([ordered]@{ packages = @($script:GeneratedFiles) })

    $sourceAtEnd = Get-CandidateSourceIdentity $RepositoryRoot $revision
    $Manifest.candidateSource.sha256AtRunEnd = $sourceAtEnd.sha256
    $Manifest.candidateSource.stableDuringRun = ($SourceAtStart.sha256 -ceq $sourceAtEnd.sha256)
    if (-not $Manifest.candidateSource.stableDuringRun) { throw "Candidate source inputs changed during measurement." }
    $cacheHashAtEnd = Get-FileSha256 $cachePath
    $Manifest.configuration.cmakeCacheSha256AtRunEnd = $cacheHashAtEnd
    $Manifest.configuration.cmakeCacheStableDuringRun = ($cacheHashAtEnd -ceq $cacheHash)
    if (-not $Manifest.configuration.cmakeCacheStableDuringRun) { throw "Release CMakeCache changed during measurement." }
    $Manifest.status = if ($RouteSmokeOnly) { "smoke-completed" } elseif ($RetentionDiagnosticOnly) { "diagnostic-completed" } else { "completed" }
    $Manifest.completedAtUtc = Get-UtcTimestamp
    Write-JsonFile $ManifestPath $Manifest
    if ($RetentionDiagnosticOnly) {
        $diagnosticSummary = [ordered]@{
            schema = "classmngr-phase9-p908-retention-diagnostic-v1"
            taskId = $TaskId
            runId = $RunId
            status = "diagnostic-only"
            budgetEvaluation = "not-evaluated"
            totalSubPrepCycles = $DiagnosticTotalCycles
            packageGenerationCount = @($script:GeneratedFiles).Count
            candidateSourceSha256 = [string]$Manifest.candidateSource.sha256
            packageSha256 = [string]$Manifest.package.sha256
            applicationSha256 = [string]$Manifest.application.sha256
            fixtureSha256 = [string]$Manifest.fixture.sha256
            processRuns = @($Manifest.processRuns)
            generatedFiles = @($script:GeneratedFiles)
        }
        Write-JsonFile $RetentionDiagnosticPath $diagnosticSummary
    }
} catch {
    $RunnerFailure = $_.Exception.Message
    $Manifest.status = "failed"
    $Manifest.failure = $RunnerFailure
    $Manifest.completedAtUtc = Get-UtcTimestamp
    try { if ($null -ne $SourceAtStart -and $null -ne $Manifest.candidateSource) { $endSource = Get-CandidateSourceIdentity $RepositoryRoot $Manifest.revision; $Manifest.candidateSource.sha256AtRunEnd = $endSource.sha256; $Manifest.candidateSource.stableDuringRun = ($endSource.sha256 -ceq $SourceAtStart.sha256) } } catch { }
    try { Write-JsonFile $ManifestPath $Manifest } catch { }
    Write-Error "$TaskId failed; all artifacts are retained in $EvidenceDirectory. $RunnerFailure" -ErrorAction Continue
} finally {
    try { if ($null -ne $script:CurrentProcess -and $null -ne $script:CurrentProcess.process -and -not $script:CurrentProcess.process.HasExited) { $script:CurrentProcess.process.Kill(); $script:CurrentProcess.process.WaitForExit() } } catch { }
    try { Write-JsonFile $GeneratedFilesPath ([ordered]@{ packages = @($script:GeneratedFiles) }) } catch { }
}

Write-Host "Task ID: $TaskId"
Write-Host "Evidence run: $EvidenceDirectory"
Write-Host "Evidence manifest: $ManifestPath"
Write-Host "Process metrics: $ProcessMetricsPath"
Write-Host "Startup log: $StartupLogPath"
if ($null -ne $RunnerFailure) { exit 1 }
if ($RouteSmokeOnly) {
    Write-Host "UIA route/output smoke: completed; full lifecycle validator intentionally not run for smoke-only evidence."
    exit 0
}
$validatorPath = if ($RetentionDiagnosticOnly) { Join-Path $PSScriptRoot "validate_p908_retention_diagnostic.py" } else { Join-Path $PSScriptRoot "validate_packaged_next_sub_prep_lifecycle.py" }
$validatorArguments = if ($RetentionDiagnosticOnly) { @("--run-directory", $EvidenceDirectory, "--expected-cycles", [string]$DiagnosticTotalCycles) } else { @("--run-directory", $EvidenceDirectory) }
$validatorOutput = & python $validatorPath @validatorArguments 2>&1
$validatorExit = $LASTEXITCODE
[IO.File]::WriteAllLines($ValidatorOutputPath, [string[]]$validatorOutput, [Text.UTF8Encoding]::new($false))
$validatorOutput | ForEach-Object { Write-Host ([string]$_) }
exit $validatorExit
