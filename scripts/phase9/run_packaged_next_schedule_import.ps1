<#
.SYNOPSIS
Runs the two packaged Next Schedule Import P9-09 routes on local Windows.

.DESCRIPTION
Builds the current-source x64 Release app in the existing build tree, stages
its Qt package, and runs a conflict-cancel route and an applicable apply route
in separate processes and fresh workspaces. UI actions are bound to verified
UIA elements and process/window identities. All attempts keep their evidence;
successful attempts remove only their per-route temporary runtime directories.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = "",
    [string]$BuildDirectory = "build\windows-x64-release",
    [string]$EvidenceRoot = "artifacts\phase9\packaged-next-schedule-import",
    [ValidateRange(30, 300)][int]$ReadinessTimeoutSeconds = 120,
    [ValidateRange(1, 30)][int]$IdleTailSamples = 10
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$TaskId = "P9_09_PACKAGED_SCHEDULE_IMPORT"
$ManifestSchema = "classmngr-phase9-packaged-next-schedule-import-v1"
$ThresholdBytes = [Int64]262144000
$PeakAdvisoryBytes = [Int64]536870912
$NormalFixtureRoot = "docs\qt-rewrite\visual-baseline\release\normal-schedule-import-next"
$ConflictFixtureName = "generated-large-schedule-import.xlsx"
$ApplicableFixtureName = "generated-large-schedule-import-applicable.xlsx"
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Join-Path $PSScriptRoot "..\.."
}
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$BuildDirectory = if ([IO.Path]::IsPathRooted($BuildDirectory)) {
    [IO.Path]::GetFullPath($BuildDirectory)
} else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $BuildDirectory)) }
$EvidenceRoot = if ([IO.Path]::IsPathRooted($EvidenceRoot)) {
    [IO.Path]::GetFullPath($EvidenceRoot)
} else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
$RunId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + "-" + [Guid]::NewGuid().ToString("N")
$EvidenceDirectory = Join-Path $EvidenceRoot $RunId
if (Test-Path -LiteralPath $EvidenceDirectory) { throw "Refusing to overwrite evidence directory $EvidenceDirectory." }
[void][IO.Directory]::CreateDirectory($EvidenceDirectory)
foreach ($directory in @("package", "fixtures", "logs", "traces", "uia", "workspace-snapshots")) {
    [void][IO.Directory]::CreateDirectory((Join-Path $EvidenceDirectory $directory))
}

$ManifestPath = Join-Path $EvidenceDirectory "manifest.json"
$SourceInventoryPath = Join-Path $EvidenceDirectory "source-files.json"
$PackageInventoryPath = Join-Path $EvidenceDirectory "package-files.json"
$StartupLogPath = Join-Path $EvidenceDirectory "startup-log.jsonl"
$ProcessMetricsPath = Join-Path $EvidenceDirectory "process-metrics.jsonl"
$UiaActionsPath = Join-Path $EvidenceDirectory "uia-actions.jsonl"
$BuildLogPath = Join-Path $EvidenceDirectory "cmake-build.log"
$SummaryPath = Join-Path $EvidenceDirectory "summary.json"
$ValidatorOutputPath = Join-Path $EvidenceDirectory "validator-output.txt"
$script:CurrentContext = $null
$script:Processes = [Collections.Generic.List[object]]::new()
$script:Manifest = [ordered]@{
    schema = $ManifestSchema
    taskId = $TaskId
    runId = $RunId
    status = "not-evaluated"
    startedAtUtc = [DateTimeOffset]::UtcNow.ToString("o")
    completedAtUtc = $null
    repositoryRoot = $RepositoryRoot
    revision = $null
    source = $null
    build = $null
    qt = $null
    application = $null
    package = $null
    fixtures = @()
    thresholds = [ordered]@{
        normalUseWorkingSetBytesExclusive = $ThresholdBytes
        operationPeakWorkingSetBytesAdvisoryExclusive = $PeakAdvisoryBytes
        operationPeakEvaluation = "advisory-only; no private-byte return allowance is defined"
        strictAtOrAboveFails = $true
    }
    routes = @()
    files = [ordered]@{
        sourceInventory = "source-files.json"
        packageInventory = "package-files.json"
        startupLog = "startup-log.jsonl"
        processMetrics = "process-metrics.jsonl"
        uiaActions = "uia-actions.jsonl"
        buildLog = "cmake-build.log"
    }
    cleanup = @()
    budgetEvaluation = "not-evaluated"
    failure = $null
}

Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

public sealed class P909MemoryCounters
{
    public ulong workingSetBytes;
    public ulong privateBytes;
    public ulong commitBytes;
    public ulong peakWorkingSetBytes;
    public ulong peakCommitBytes;
}

public sealed class P909WindowRect
{
    public int left;
    public int top;
    public int right;
    public int bottom;
}

public sealed class P909PointerResult
{
    public long windowHandle;
    public uint expectedProcessId;
    public uint windowProcessId;
    public uint foregroundProcessId;
    public long foregroundWindowHandle;
    public int screenX;
    public int screenY;
    public uint waitForInputIdleResult;
    public bool foregroundVerified;
    public uint inputEventsSent;
}

public sealed class P909MessageClickResult
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
    public bool waitForInputIdleSucceeded;
    public bool mouseMoveSent;
    public bool leftButtonDownSent;
    public bool leftButtonUpSent;
    public bool secondLeftButtonUpSent;
    public bool leftButtonDoubleClickSent;
    public uint messagesSent;
}

public static class P909Native
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
    private const uint WmLeftButtonDoubleClick = 0x0203;
    private const uint MouseKeyLeftButton = 0x0001;
    private const uint SmtoAbortIfHung = 0x0002;

    [StructLayout(LayoutKind.Sequential)]
    private struct Point { public int x; public int y; }
    [StructLayout(LayoutKind.Sequential)]
    private struct Rect { public int left; public int top; public int right; public int bottom; }
    [StructLayout(LayoutKind.Sequential)]
    private struct MouseInput
    {
        public int dx; public int dy; public uint mouseData; public uint flags;
        public uint time; public UIntPtr extraInfo;
    }
    [StructLayout(LayoutKind.Sequential)]
    private struct Input { public uint type; public MouseInput mouseInput; }
    [StructLayout(LayoutKind.Sequential)]
    private struct ProcessMemoryCountersEx
    {
        public uint cb; public uint pageFaultCount;
        public UIntPtr peakWorkingSetSize; public UIntPtr workingSetSize;
        public UIntPtr quotaPeakPagedPoolUsage; public UIntPtr quotaPagedPoolUsage;
        public UIntPtr quotaPeakNonPagedPoolUsage; public UIntPtr quotaNonPagedPoolUsage;
        public UIntPtr pagefileUsage; public UIntPtr peakPagefileUsage; public UIntPtr privateUsage;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool IsWow64Process2(IntPtr process, out ushort processMachine, out ushort nativeMachine);
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool QueryFullProcessImageNameW(IntPtr process, uint flags, StringBuilder imageName, ref int size);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint WaitForInputIdle(IntPtr process, uint milliseconds);
    [DllImport("psapi.dll", SetLastError = true)]
    private static extern bool GetProcessMemoryInfo(IntPtr process, ref ProcessMemoryCountersEx counters, uint size);
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
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool PrintWindow(IntPtr window, IntPtr deviceContext, uint flags);
    [DllImport("user32.dll", EntryPoint = "SendMessageTimeoutW", SetLastError = true)]
    private static extern IntPtr SendMessageTimeoutW(IntPtr window, uint message, UIntPtr wParam,
        IntPtr lParam, uint flags, uint timeoutMilliseconds, out IntPtr result);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint SendInput(uint count, Input[] inputs, int size);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr GetWindow(IntPtr window, uint command);

    public static bool IsNativeX64(IntPtr process)
    {
        ushort processMachine; ushort nativeMachine;
        if (!IsWow64Process2(process, out processMachine, out nativeMachine))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "IsWow64Process2 failed.");
        return nativeMachine == ImageFileMachineAmd64 && processMachine == ImageFileMachineUnknown;
    }
    public static string GetProcessImagePath(IntPtr process)
    {
        StringBuilder path = new StringBuilder(32768); int size = path.Capacity;
        if (!QueryFullProcessImageNameW(process, 0, path, ref size))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "QueryFullProcessImageName failed.");
        return path.ToString();
    }
    public static uint GetWindowProcessId(IntPtr window)
    {
        uint pid;
        if (GetWindowThreadProcessId(window, out pid) == 0)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowThreadProcessId failed.");
        return pid;
    }
    public static long GetOwnerHandle(IntPtr window) { return GetWindow(window, 4).ToInt64(); }
    public static bool IsWindowVisibleForProcess(IntPtr window) { return IsWindowVisible(window); }
    public static bool RenderWindowToDeviceContext(IntPtr window, IntPtr process, IntPtr deviceContext, uint expectedPid)
    {
        if (GetWindowProcessId(window) != expectedPid)
            throw new InvalidOperationException("Screenshot HWND belongs to another process.");
        if (WaitForInputIdle(process, 10000) != 0)
            throw new InvalidOperationException("Measured process did not reach input-idle before screenshot capture.");
        return PrintWindow(window, deviceContext, 2);
    }
    public static P909WindowRect GetWindowRectangle(IntPtr window)
    {
        Rect rect;
        if (!GetWindowRect(window, out rect)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect failed.");
        return new P909WindowRect { left = rect.left, top = rect.top, right = rect.right, bottom = rect.bottom };
    }
    public static P909MemoryCounters GetMemoryCounters(IntPtr process)
    {
        ProcessMemoryCountersEx counters = new ProcessMemoryCountersEx();
        counters.cb = (uint)Marshal.SizeOf(typeof(ProcessMemoryCountersEx));
        if (!GetProcessMemoryInfo(process, ref counters, counters.cb))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetProcessMemoryInfo failed.");
        return new P909MemoryCounters {
            workingSetBytes = counters.workingSetSize.ToUInt64(),
            privateBytes = counters.privateUsage.ToUInt64(),
            commitBytes = counters.pagefileUsage.ToUInt64(),
            peakWorkingSetBytes = counters.peakWorkingSetSize.ToUInt64(),
            peakCommitBytes = counters.peakPagefileUsage.ToUInt64()
        };
    }
    public static P909PointerResult ClickWindowPoint(IntPtr window, IntPtr process, int x, int y, uint expectedPid)
    {
        P909PointerResult result = new P909PointerResult();
        result.windowHandle = window.ToInt64(); result.expectedProcessId = expectedPid;
        result.windowProcessId = GetWindowProcessId(window); result.screenX = x; result.screenY = y;
        if (result.windowProcessId != expectedPid)
            throw new InvalidOperationException("The UIA target HWND belongs to another process.");
        result.waitForInputIdleResult = WaitForInputIdle(process, 10000);
        if (result.waitForInputIdleResult != 0)
            throw new InvalidOperationException("Measured process did not reach input-idle before UIA click.");
        AllowSetForegroundWindow(expectedPid); ShowWindow(window, SwRestore);
        uint targetThread = GetWindowThreadProcessId(window, out result.windowProcessId);
        uint callerThread = GetCurrentThreadId(); IntPtr prior = GetForegroundWindow(); uint priorPid;
        uint foregroundThread = prior == IntPtr.Zero ? 0 : GetWindowThreadProcessId(prior, out priorPid);
        bool targetAttached = false; bool foregroundAttached = false;
        try
        {
            if (targetThread != callerThread) {
                targetAttached = AttachThreadInput(callerThread, targetThread, true);
                if (!targetAttached) throw new Win32Exception(Marshal.GetLastWin32Error(), "AttachThreadInput(target) failed.");
            }
            if (foregroundThread != 0 && foregroundThread != callerThread && foregroundThread != targetThread)
                foregroundAttached = AttachThreadInput(callerThread, foregroundThread, true);
            BringWindowToTop(window); SetActiveWindow(window); SetFocus(window); SetForegroundWindow(window);
            System.Threading.Thread.Sleep(100);
            IntPtr foreground = GetForegroundWindow(); result.foregroundWindowHandle = foreground.ToInt64();
            result.foregroundProcessId = foreground == IntPtr.Zero ? 0 : GetWindowProcessId(foreground);
            result.foregroundVerified = foreground == window && result.foregroundProcessId == expectedPid;
            if (!result.foregroundVerified) return result;
            if (!SetCursorPos(x, y)) throw new Win32Exception(Marshal.GetLastWin32Error(), "SetCursorPos failed.");
            Input[] input = new Input[2];
            input[0].type = InputMouse; input[0].mouseInput.flags = LeftDown;
            input[1].type = InputMouse; input[1].mouseInput.flags = LeftUp;
            result.inputEventsSent = SendInput(2, input, Marshal.SizeOf(typeof(Input)));
            if (result.inputEventsSent != 2) throw new Win32Exception(Marshal.GetLastWin32Error(), "SendInput failed.");
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
    private static void SendMessageChecked(IntPtr hwnd, uint message, uint keys, IntPtr coordinates, out bool sent)
    {
        IntPtr ignored; IntPtr result = SendMessageTimeoutW(hwnd, message, new UIntPtr(keys), coordinates,
            SmtoAbortIfHung, 2000, out ignored);
        sent = result != IntPtr.Zero;
        if (!sent) throw new Win32Exception(Marshal.GetLastWin32Error(), "SendMessageTimeout failed.");
    }
    public static P909MessageClickResult ClickWindowMessage(IntPtr window, IntPtr process, int x, int y, uint expectedPid)
    {
        P909MessageClickResult result = new P909MessageClickResult();
        result.windowHandle = window.ToInt64(); result.expectedProcessId = expectedPid;
        result.windowProcessId = GetWindowProcessId(window); result.screenX = x; result.screenY = y;
        if (result.windowProcessId != expectedPid) throw new InvalidOperationException("UIA click HWND belongs to another process.");
        result.waitForInputIdleSucceeded = WaitForInputIdle(process, 10000) == 0;
        if (!result.waitForInputIdleSucceeded) throw new InvalidOperationException("Measured process did not reach input-idle before message click.");
        Point point = new Point { x = x, y = y };
        if (!ScreenToClient(window, ref point)) throw new Win32Exception(Marshal.GetLastWin32Error(), "ScreenToClient failed.");
        Rect rect; if (!GetClientRect(window, out rect)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetClientRect failed.");
        if (point.x < rect.left || point.x >= rect.right || point.y < rect.top || point.y >= rect.bottom)
            throw new InvalidOperationException("UIA point is outside measured window client bounds.");
        result.clientX = point.x; result.clientY = point.y;
        result.clientLeft = rect.left; result.clientTop = rect.top; result.clientRight = rect.right; result.clientBottom = rect.bottom;
        IntPtr packed = PackCoordinates(point.x, point.y);
        SendMessageChecked(window, WmMouseMove, 0, packed, out result.mouseMoveSent);
        SendMessageChecked(window, WmLeftButtonDown, MouseKeyLeftButton, packed, out result.leftButtonDownSent);
        SendMessageChecked(window, WmLeftButtonUp, 0, packed, out result.leftButtonUpSent);
        result.messagesSent = 3; return result;
    }
    public static P909MessageClickResult DoubleClickWindowMessage(IntPtr window, IntPtr process, int x, int y, uint expectedPid)
    {
        P909MessageClickResult result = new P909MessageClickResult();
        result.windowHandle = window.ToInt64(); result.expectedProcessId = expectedPid;
        result.windowProcessId = GetWindowProcessId(window); result.screenX = x; result.screenY = y;
        if (result.windowProcessId != expectedPid) throw new InvalidOperationException("UIA double-click HWND belongs to another process.");
        result.waitForInputIdleSucceeded = WaitForInputIdle(process, 10000) == 0;
        if (!result.waitForInputIdleSucceeded) throw new InvalidOperationException("Measured process did not reach input-idle before message double-click.");
        Point point = new Point { x = x, y = y };
        if (!ScreenToClient(window, ref point)) throw new Win32Exception(Marshal.GetLastWin32Error(), "ScreenToClient failed.");
        Rect rect; if (!GetClientRect(window, out rect)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetClientRect failed.");
        if (point.x < rect.left || point.x >= rect.right || point.y < rect.top || point.y >= rect.bottom)
            throw new InvalidOperationException("UIA double-click point is outside measured window client bounds.");
        result.clientX = point.x; result.clientY = point.y;
        result.clientLeft = rect.left; result.clientTop = rect.top; result.clientRight = rect.right; result.clientBottom = rect.bottom;
        IntPtr packed = PackCoordinates(point.x, point.y);
        SendMessageChecked(window, WmMouseMove, 0, packed, out result.mouseMoveSent);
        SendMessageChecked(window, WmLeftButtonDown, MouseKeyLeftButton, packed, out result.leftButtonDownSent);
        SendMessageChecked(window, WmLeftButtonUp, 0, packed, out result.leftButtonUpSent);
        SendMessageChecked(window, WmLeftButtonDoubleClick, MouseKeyLeftButton, packed, out result.leftButtonDoubleClickSent);
        SendMessageChecked(window, WmLeftButtonUp, 0, packed, out result.secondLeftButtonUpSent);
        result.messagesSent = 5; return result;
    }
}
'@

function Get-UtcTimestamp { [DateTimeOffset]::UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss.fff'Z'") }
function Get-FileSha256 { param([string]$Path) (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Get-StringSha256 {
    param([string]$Value)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value)))).Replace("-", "").ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Write-JsonFile {
    param([string]$Path, [object]$Value)
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 32), [Text.UTF8Encoding]::new($false))
}
function Add-JsonLine {
    param([string]$Path, [object]$Value)
    [IO.File]::AppendAllText($Path, ($Value | ConvertTo-Json -Depth 32 -Compress) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
}
function Get-CanonicalInventory {
    param([string]$Root)
    $canonicalRoot = [IO.Path]::GetFullPath($Root).TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    $items = [Collections.Generic.List[object]]::new()
    foreach ($file in @(Get-ChildItem -LiteralPath $Root -File -Recurse)) {
        if ($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @(".pyc", ".pyo")) { continue }
        $canonicalPath = [IO.Path]::GetFullPath($file.FullName)
        if (-not $canonicalPath.StartsWith($canonicalRoot, [StringComparison]::OrdinalIgnoreCase)) { throw "Inventory file escaped its root: $canonicalPath" }
        $relative = $canonicalPath.Substring($canonicalRoot.Length).Replace("\", "/")
        $items.Add([ordered]@{ path = $relative; sizeBytes = [Int64]$file.Length; sha256 = Get-FileSha256 $file.FullName })
    }
    # Python's independent validator sorts paths by Unicode casefold order.
    # Lowercase then use ordinal ordering; PowerShell Sort-Object and
    # OrdinalIgnoreCase use punctuation collation for these file names.
    $items.Sort([System.Comparison[object]]{
        param($left, $right)
        [StringComparer]::Ordinal.Compare(([string]$left.path).ToLowerInvariant(), ([string]$right.path).ToLowerInvariant())
    })
    $sorted = @($items.ToArray())
    $canonical = @($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    [ordered]@{ root = [IO.Path]::GetFullPath($Root); sha256 = Get-StringSha256 ($canonical + "`n"); fileCount = $sorted.Count; files = $sorted }
}
function Get-SourceInventory {
    param([string]$Root)
    $canonicalRoot = [IO.Path]::GetFullPath($Root).TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    $files = [Collections.Generic.List[object]]::new()
    $paths = @("CMakeLists.txt", "CMakePresets.json", "cmake", "docs/qt-rewrite/visual-baseline/release/normal-schedule-import-next", "resources", "scripts/phase9", "src", "tests")
    foreach ($relativeRoot in $paths) {
        $rootPath = Join-Path $Root ($relativeRoot.Replace("/", "\"))
        if (-not (Test-Path -LiteralPath $rootPath)) { throw "Required source inventory path is missing: $relativeRoot" }
        if (Test-Path -LiteralPath $rootPath -PathType Leaf) { $scan = @(Get-Item -LiteralPath $rootPath) }
        else { $scan = @(Get-ChildItem -LiteralPath $rootPath -File -Recurse) }
        foreach ($file in $scan) {
            if ($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @(".pyc", ".pyo")) { continue }
            $canonicalPath = [IO.Path]::GetFullPath($file.FullName)
            if (-not $canonicalPath.StartsWith($canonicalRoot, [StringComparison]::OrdinalIgnoreCase)) { throw "Source inventory file escaped repository root: $canonicalPath" }
            $relative = $canonicalPath.Substring($canonicalRoot.Length).Replace("\", "/")
            $files.Add([ordered]@{ path = $relative; sizeBytes = [Int64]$file.Length; sha256 = Get-FileSha256 $file.FullName })
        }
    }
    $files.Sort([System.Comparison[object]]{
        param($left, $right)
        [StringComparer]::Ordinal.Compare(([string]$left.path).ToLowerInvariant(), ([string]$right.path).ToLowerInvariant())
    })
    $sorted = @($files.ToArray())
    $canonical = @($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    [ordered]@{ root = [IO.Path]::GetFullPath($Root); sha256 = Get-StringSha256 ($canonical + "`n"); includedRoots = $paths; fileCount = $sorted.Count; files = $sorted }
}
function Get-RequiredCacheValue {
    param([string[]]$Lines, [string]$Name)
    $matches = @($Lines | Where-Object { $_ -match "^$([regex]::Escape($Name)):[^=]+=" })
    if ($matches.Count -ne 1) { throw "Expected one cache entry $Name, found $($matches.Count)." }
    ($matches[0] -split "=", 2)[1]
}
function Get-PeArchitecture {
    param([string]$Path)
    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Bad executable MZ header: $Path" }
        $stream.Position = 0x3c; $offset = $reader.ReadInt32(); $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550) { throw "Bad executable PE header: $Path" }
        switch ($reader.ReadUInt16()) { 0x8664 { "x64" } 0xaa64 { "arm64" } default { "unknown" } }
    } finally { $stream.Dispose() }
}
function Get-PackagedRuntimeInventory {
    param([string]$PackageRoot, [string]$QtPrefix)
    $runtime = [Collections.Generic.List[object]]::new()
    foreach ($name in @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Sql.dll")) {
        $packagePath = Join-Path $PackageRoot $name
        $qtPath = Join-Path (Join-Path $QtPrefix "bin") $name
        if (-not (Test-Path -LiteralPath $packagePath -PathType Leaf) -or (Get-FileSha256 $packagePath) -cne (Get-FileSha256 $qtPath)) { throw "Packaged Qt runtime differs from cache-bound $name." }
        $runtime.Add([ordered]@{ packagePath = $name; qtPrefixPath = [IO.Path]::GetFullPath($qtPath); sizeBytes = [Int64](Get-Item $packagePath).Length; sha256 = Get-FileSha256 $packagePath })
    }
    foreach ($pair in @(@("platforms/qwindows.dll", "plugins/platforms/qwindows.dll"), @("sqldrivers/qsqlite.dll", "plugins/sqldrivers/qsqlite.dll"))) {
        $packagePath = Join-Path $PackageRoot ($pair[0].Replace("/", "\")); $qtPath = Join-Path $QtPrefix ($pair[1].Replace("/", "\"))
        if (-not (Test-Path -LiteralPath $packagePath -PathType Leaf) -or (Get-FileSha256 $packagePath) -cne (Get-FileSha256 $qtPath)) { throw "Packaged Qt plugin differs from cache-bound $($pair[0])." }
        $runtime.Add([ordered]@{ packagePath = $pair[0]; qtPrefixPath = [IO.Path]::GetFullPath($qtPath); sizeBytes = [Int64](Get-Item $packagePath).Length; sha256 = Get-FileSha256 $packagePath })
    }
    @($runtime.ToArray())
}
function Invoke-Checked {
    param([string]$Path, [string[]]$Arguments, [string]$Description)
    & $Path @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Description failed with exit code $LASTEXITCODE." }
}
function Add-ProcessOutput {
    param([object]$Context)
    foreach ($reader in $Context.readers) {
        while ($reader.ReadTask.IsCompleted) {
            $line = $reader.ReadTask.GetAwaiter().GetResult()
            if ($null -eq $line) { $reader.closed = $true; break }
            $entry = [ordered]@{ routeId = $Context.routeId; processInstanceId = $Context.processInstanceId; pid = [Int64]$Context.process.Id; stream = $reader.stream; observedAtUtc = Get-UtcTimestamp; line = [string]$line }
            Add-JsonLine $StartupLogPath $entry
            [IO.File]::AppendAllText($reader.path, [string]$line + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
            $Context.logEntries.Add($entry)
            $reader.ReadTask = $reader.reader.ReadLineAsync()
        }
    }
}
function Wait-ForCondition {
    param([object]$Context, [scriptblock]$Predicate, [string]$Description, [int]$TimeoutSeconds = 20)
    $deadline = [DateTimeOffset]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTimeOffset]::UtcNow -lt $deadline) {
        Add-ProcessOutput $Context
        $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "$($Context.processInstanceId) exited while waiting for $Description (exit $($Context.process.ExitCode))." }
        $value = & $Predicate $Context
        if ($value) { return $value }
        Start-Sleep -Milliseconds 100
    }
    throw "$($Context.processInstanceId) timed out waiting for $Description."
}
function Get-StartupCheckpoint {
    param([object]$Context, [string]$Stage)
    $prefix = "^ClassMngrNext startup stage=$([regex]::Escape($Stage)) outcome=(?<outcome>[a-z-]+) diagnostic=(?<diagnostic>[a-z-]+) diagnostic_stage=(?<diagnosticStage>[a-z-]+) checkpoint=(?<checkpoint>\{.*\})$"
    $rows = @($Context.logEntries | Where-Object { [string]$_.line -match $prefix })
    if ($rows.Count -ne 1) { throw "$($Context.processInstanceId) expected one $Stage checkpoint, found $($rows.Count)." }
    $match = [regex]::Match([string]$rows[0].line, $prefix)
    $checkpoint = $match.Groups["checkpoint"].Value | ConvertFrom-Json -ErrorAction Stop
    if ($match.Groups["outcome"].Value -cne "completed" -or $match.Groups["diagnostic"].Value -cne "none" -or $match.Groups["diagnosticStage"].Value -cne "none" -or $checkpoint.outcome -cne "completed" -or $checkpoint.platform -cne "windows" -or $checkpoint.sample_available -ne $true) { throw "$($Context.processInstanceId) $Stage checkpoint was not a completed Windows sample." }
    foreach ($key in @("working_set_bytes", "private_usage_bytes", "peak_working_set_bytes", "handle_count", "thread_count")) {
        if ($null -eq $checkpoint.metrics.$key -or [Int64]$checkpoint.metrics.$key -le 0) { throw "$($Context.processInstanceId) $Stage checkpoint metric $key is missing." }
    }
    [ordered]@{ logIndex = [Int64]$Context.logEntries.IndexOf($rows[0]); observedAtUtc = [string]$rows[0].observedAtUtc; rawLine = [string]$rows[0].line; record = $checkpoint }
}
function New-ProcessSample {
    param([object]$Context, [string]$Point, [int]$Index = 0)
    $process = $Context.process; $process.Refresh()
    if ($process.HasExited) { throw "$($Context.processInstanceId) exited before metric point $Point." }
    $memory = [P909Native]::GetMemoryCounters($process.Handle)
    $sample = [ordered]@{
        routeId = $Context.routeId; processInstanceId = $Context.processInstanceId; point = $Point; sampleIndex = $Index
        timestampUtc = Get-UtcTimestamp; pid = [Int64]$process.Id; processCreatedAtUtc = $Context.createdAtUtc
        canonicalExecutablePath = $Context.executablePath; architecture = "x64"; sha256 = $Context.executableSha256
        currentWorkingSetBytes = [Int64]$memory.workingSetBytes; privateBytes = [Int64]$memory.privateBytes
        commitBytes = [Int64]$memory.commitBytes; peakWorkingSetBytes = [Int64]$memory.peakWorkingSetBytes
        peakCommitBytes = [Int64]$memory.peakCommitBytes; handles = [Int64]$process.HandleCount
        threads = [Int64]$process.Threads.Count
    }
    Add-JsonLine $ProcessMetricsPath $sample
    return $sample
}
function Get-AppTopWindows {
    param([int]$ProcessId)
    $root = [System.Windows.Automation.AutomationElement]::RootElement
    @($root.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition) | Where-Object { $_.Current.ProcessId -eq $ProcessId -and $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window })
}
function Get-AppElements {
    param([object]$Context)
    $elements = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach ($window in @(Get-AppTopWindows $Context.process.Id)) {
        $elements.Add($window)
        foreach ($element in @($window.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition))) { $elements.Add($element) }
    }
    @($elements.ToArray())
}
function Get-MainWindow {
    param([object]$Context)
    $Context.process.Refresh(); $handle = $Context.process.MainWindowHandle
    if ($handle -eq [IntPtr]::Zero -or -not [P909Native]::IsWindowVisibleForProcess($handle) -or [P909Native]::GetWindowProcessId($handle) -ne [uint32]$Context.process.Id) { throw "$($Context.processInstanceId) has no verified visible main window." }
    $window = [System.Windows.Automation.AutomationElement]::FromHandle($handle)
    if ($null -eq $window -or $window.Current.ProcessId -ne $Context.process.Id) { throw "$($Context.processInstanceId) UIA main window PID mismatch." }
    return $window
}
function Get-UiaElementById {
    param([object]$Context, [string]$AutomationId, [string]$ExpectedType = "")
    $matches = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach ($element in @(Get-AppElements $Context)) {
        $current = $element.Current; $id = [string]$current.AutomationId
        $idMatches = $id -ceq $AutomationId -or $id.EndsWith(".$AutomationId", [StringComparison]::Ordinal)
        $typeMatches = [string]::IsNullOrWhiteSpace($ExpectedType) -or $current.ControlType.ProgrammaticName -ceq $ExpectedType
        if ($idMatches -and $typeMatches -and $current.ProcessId -eq $Context.process.Id) { $matches.Add($element) }
    }
    if ($matches.Count -ne 1) {
        Save-UiaTree $Context ("uia/tree-" + $Context.routeId + "-" + $AutomationId + ".json")
        throw "UIA expected one '$AutomationId' in process $($Context.process.Id); found $($matches.Count)."
    }
    $matches[0]
}
function Get-UiaByName {
    param([object]$Context, [string]$Name, [string]$ExpectedType)
    $matches = @((Get-AppElements $Context) | Where-Object { $_.Current.ProcessId -eq $Context.process.Id -and $_.Current.Name -ceq $Name -and $_.Current.ControlType.ProgrammaticName -ceq $ExpectedType })
    if ($matches.Count -ne 1) { Save-UiaTree $Context ("uia/tree-" + $Context.routeId + "-name.json"); throw "UIA expected one '$Name' ($ExpectedType), found $($matches.Count)." }
    $matches[0]
}
function Get-UiaPropertySnapshot {
    param([System.Windows.Automation.AutomationElement]$Element)
    $current = $Element.Current; $rect = $current.BoundingRectangle
    $controlTypeName = ""
    try { $controlTypeName = [string]$current.ControlType.ProgrammaticName } catch { }
    $value = $null
    try { $value = ([System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)).Current.Value } catch { }
    $selection = @()
    try { $selection = @(([System.Windows.Automation.SelectionPattern]$Element.GetCurrentPattern([System.Windows.Automation.SelectionPattern]::Pattern)).Current.GetSelection() | ForEach-Object { [string]$_.Current.Name }) } catch { }
    [ordered]@{
        automationId = [string]$current.AutomationId; name = [string]$current.Name
        controlType = $controlTypeName; className = [string]$current.ClassName
        frameworkId = [string]$current.FrameworkId; processId = [Int64]$current.ProcessId
        nativeWindowHandle = [Int64]$current.NativeWindowHandle; isEnabled = [bool]$current.IsEnabled
        isOffscreen = [bool]$current.IsOffscreen; helpText = [string]$current.HelpText
        value = $value; selection = $selection
        rectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height }
    }
}
function Save-UiaTree {
    param([object]$Context, [string]$RelativePath)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($element in @(Get-AppElements $Context)) {
        try { $rows.Add((Get-UiaPropertySnapshot $element)) }
        catch { $rows.Add([ordered]@{ snapshotError = $_.Exception.Message }) }
    }
    Write-JsonFile (Join-Path $EvidenceDirectory $RelativePath) ([ordered]@{ routeId = $Context.routeId; processInstanceId = $Context.processInstanceId; observedAtUtc = Get-UtcTimestamp; elements = @($rows.ToArray()) })
}
function Get-ElementWindowHandle {
    param([object]$Context, [System.Windows.Automation.AutomationElement]$Element)
    $walker = [System.Windows.Automation.TreeWalker]::ControlViewWalker
    $current = $Element
    for ($depth = 0; $depth -lt 48 -and $null -ne $current; $depth++) {
        try {
            if ($current.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window) {
                $candidate = [IntPtr]$current.Current.NativeWindowHandle
                if ($candidate -ne [IntPtr]::Zero -and [P909Native]::GetWindowProcessId($candidate) -eq [uint32]$Context.process.Id) { return $candidate }
            }
            $current = $walker.GetParent($current)
        } catch { break }
    }
    $candidate = [IntPtr]$Element.Current.NativeWindowHandle
    if ($candidate -ne [IntPtr]::Zero) { try { if ([P909Native]::GetWindowProcessId($candidate) -eq [uint32]$Context.process.Id) { return $candidate } } catch { } }
    return [IntPtr]$Context.mainWindowHandle
}
function Invoke-VerifiedUiaClick {
    param([object]$Context, [System.Windows.Automation.AutomationElement]$Element, [string]$Action, [string]$FromPage, [string]$ToPage, [switch]$SkipLog)
    $identity = Get-UiaPropertySnapshot $Element
    if ([Int64]$identity.processId -ne [Int64]$Context.process.Id -or $identity.isOffscreen -or [double]$identity.rectangle.width -le 0 -or [double]$identity.rectangle.height -le 0) { throw "UIA target identity/visibility invalid for $Action." }
    $x = [int][Math]::Round([double]$identity.rectangle.x + ([double]$identity.rectangle.width / 2))
    $y = [int][Math]::Round([double]$identity.rectangle.y + ([double]$identity.rectangle.height / 2))
    $window = Get-ElementWindowHandle $Context $Element
    $record = [ordered]@{
        routeId = $Context.routeId; processInstanceId = $Context.processInstanceId; actionKind = $Action
        method = "uia-bound-verified-input"; targetAutomationId = [string]$identity.automationId
        targetAccessibleName = [string]$identity.name; targetControlType = [string]$identity.controlType
        elementProcessId = [Int64]$identity.processId; windowHandle = [Int64]$window.ToInt64()
        windowProcessId = [Int64][P909Native]::GetWindowProcessId($window)
        mainWindowHandle = [Int64]$Context.mainWindowHandle; mainWindowProcessId = [Int64]$Context.process.Id
        fromPageId = $FromPage; toPageId = $ToPage; rectangle = $identity.rectangle
        issuedAtUtc = Get-UtcTimestamp; nativeActivation = $null; verifiedApplicationState = $false
    }
    $pointer = [P909Native]::ClickWindowPoint($window, $Context.process.Handle, $x, $y, [uint32]$Context.process.Id)
    $record.nativeActivation = [ordered]@{
        kind = "foreground-pointer"; expectedProcessId = [Int64]$pointer.expectedProcessId
        windowProcessId = [Int64]$pointer.windowProcessId; foregroundProcessId = [Int64]$pointer.foregroundProcessId
        foregroundWindowHandle = [Int64]$pointer.foregroundWindowHandle; foregroundVerified = [bool]$pointer.foregroundVerified
        waitForInputIdleResult = [Int64]$pointer.waitForInputIdleResult
        inputEventsSent = [Int64]$pointer.inputEventsSent; screenX = $x; screenY = $y
    }
    if (-not $pointer.foregroundVerified) {
        $message = [P909Native]::ClickWindowMessage($window, $Context.process.Handle, $x, $y, [uint32]$Context.process.Id)
        $record.method = "uia-bound-verified-window-message"
        $record.nativeActivation = [ordered]@{
            kind = "verified-window-message"; expectedProcessId = [Int64]$message.expectedProcessId
            windowProcessId = [Int64]$message.windowProcessId; waitForInputIdleSucceeded = [bool]$message.waitForInputIdleSucceeded
            clientX = [int]$message.clientX; clientY = [int]$message.clientY
            clientBounds = [ordered]@{ left = $message.clientLeft; top = $message.clientTop; right = $message.clientRight; bottom = $message.clientBottom }
            mouseMoveSent = [bool]$message.mouseMoveSent; leftButtonDownSent = [bool]$message.leftButtonDownSent
            leftButtonUpSent = [bool]$message.leftButtonUpSent; messagesSent = [Int64]$message.messagesSent
        }
    }
    if (-not $SkipLog) { Add-JsonLine $UiaActionsPath $record }
    return $record
}
function Invoke-VerifiedUiaDoubleClick {
    param([object]$Context, [System.Windows.Automation.AutomationElement]$Element, [string]$Action, [string]$FromPage, [string]$ToPage)
    $identity = Get-UiaPropertySnapshot $Element
    if ([Int64]$identity.processId -ne [Int64]$Context.process.Id -or $identity.isOffscreen -or [double]$identity.rectangle.width -le 0 -or [double]$identity.rectangle.height -le 0) { throw "UIA target identity/visibility invalid for $Action." }
    $x = [int][Math]::Round([double]$identity.rectangle.x + ([double]$identity.rectangle.width / 2))
    $y = [int][Math]::Round([double]$identity.rectangle.y + ([double]$identity.rectangle.height / 2))
    $window = Get-ElementWindowHandle $Context $Element
    $message = [P909Native]::DoubleClickWindowMessage($window, $Context.process.Handle, $x, $y, [uint32]$Context.process.Id)
    $record = [ordered]@{
        routeId = $Context.routeId; processInstanceId = $Context.processInstanceId; actionKind = $Action
        method = "uia-bound-verified-window-double-click"; targetAutomationId = [string]$identity.automationId
        targetAccessibleName = [string]$identity.name; targetControlType = [string]$identity.controlType
        elementProcessId = [Int64]$identity.processId; windowHandle = [Int64]$window.ToInt64()
        windowProcessId = [Int64][P909Native]::GetWindowProcessId($window)
        mainWindowHandle = [Int64]$Context.mainWindowHandle; mainWindowProcessId = [Int64]$Context.process.Id
        fromPageId = $FromPage; toPageId = $ToPage; rectangle = $identity.rectangle; issuedAtUtc = Get-UtcTimestamp
        nativeActivation = [ordered]@{
            kind = "verified-window-message-double-click"; expectedProcessId = [Int64]$message.expectedProcessId
            windowProcessId = [Int64]$message.windowProcessId; waitForInputIdleSucceeded = [bool]$message.waitForInputIdleSucceeded
            clientX = [int]$message.clientX; clientY = [int]$message.clientY
            clientBounds = [ordered]@{ left = $message.clientLeft; top = $message.clientTop; right = $message.clientRight; bottom = $message.clientBottom }
            mouseMoveSent = [bool]$message.mouseMoveSent; leftButtonDownSent = [bool]$message.leftButtonDownSent
            firstLeftButtonUpSent = [bool]$message.leftButtonUpSent; secondLeftButtonUpSent = [bool]$message.secondLeftButtonUpSent
            leftButtonDoubleClickSent = [bool]$message.leftButtonDoubleClickSent
            messagesSent = [Int64]$message.messagesSent
        }
    }
    Add-JsonLine $UiaActionsPath $record
    return $record
}
function Invoke-VerifiedUiaValueSet {
    param([object]$Context, [System.Windows.Automation.AutomationElement]$Element, [string]$Value, [string]$Action, [int]$ExpectedOwnerPid = -1)
    $identity = Get-UiaPropertySnapshot $Element
    if ($ExpectedOwnerPid -ge 0 -and [Int64]$identity.processId -ne $ExpectedOwnerPid) { throw "UIA value target has unexpected process identity for $Action." }
    $pattern = [System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $pattern.SetValue($Value)
    Add-JsonLine $UiaActionsPath ([ordered]@{
        routeId = $Context.routeId; processInstanceId = $Context.processInstanceId; actionKind = $Action
        method = "uia-value-pattern-set"; targetAutomationId = [string]$identity.automationId
        targetAccessibleName = [string]$identity.name; elementProcessId = [Int64]$identity.processId
        applicationProcessId = [Int64]$Context.process.Id; windowHandle = [Int64]$identity.nativeWindowHandle
        observedValueAfter = [string]$pattern.Current.Value; issuedAtUtc = Get-UtcTimestamp
    })
}
function Wait-ForUiaElement {
    param([object]$Context, [string]$AutomationId, [string]$ExpectedType = "", [int]$TimeoutSeconds = 20)
    Wait-ForCondition $Context { param($ctx)
        $found = @((Get-AppElements $ctx) | Where-Object {
            $id = [string]$_.Current.AutomationId
            ($id -ceq $AutomationId -or $id.EndsWith(".$AutomationId", [StringComparison]::Ordinal)) -and
            ([string]::IsNullOrWhiteSpace($ExpectedType) -or $_.Current.ControlType.ProgrammaticName -ceq $ExpectedType)
        })
        if ($found.Count -eq 1) { return $found[0] }
        return $null
    } $AutomationId $TimeoutSeconds
}
function Wait-ForDialog {
    param([object]$Context, [int]$TimeoutSeconds = 20)
    Wait-ForCondition $Context { param($ctx)
        $elements = @(Get-AppElements $ctx | Where-Object { ([string]$_.Current.AutomationId).EndsWith("nextScheduleImportSourceStatus", [StringComparison]::Ordinal) })
        if ($elements.Count -eq 1) { return $elements[0] }
        return $null
    } "Schedule Import dialog" $TimeoutSeconds
}
function Wait-ForTraceEvent {
    param([object]$Context, [string]$EventName, [int]$TimeoutSeconds = 30)
    Wait-ForCondition $Context { param($ctx)
        if (-not (Test-Path -LiteralPath $ctx.tracePath -PathType Leaf)) { return $null }
        try { $events = @(Get-Content -LiteralPath $ctx.tracePath -Encoding UTF8 | ForEach-Object { $_ | ConvertFrom-Json -ErrorAction Stop }) } catch { return $null }
        $matches = @($events | Where-Object { $_.event -ceq $EventName })
        if ($matches.Count -ge 1) { return $matches[-1] }
        return $null
    } "Schedule Import diagnostic event '$EventName'" $TimeoutSeconds
}
function Find-FilePicker {
    param([object]$Context, [System.Windows.Automation.AutomationElement]$ImportStatusElement)
    $importHwnd = Get-ElementWindowHandle $Context $ImportStatusElement
    $mainHwnd = [IntPtr]$Context.mainWindowHandle
    $candidates = [Collections.Generic.List[object]]::new()
    $windows = @((Get-AppElements $Context) | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window -and $_.Current.ProcessId -eq $Context.process.Id })
    foreach ($window in $windows) {
        $current = $window.Current; $hwnd = [IntPtr]$current.NativeWindowHandle
        if ($hwnd -eq [IntPtr]::Zero -or $hwnd -eq $mainHwnd -or $hwnd -eq $importHwnd -or -not [P909Native]::IsWindowVisibleForProcess($hwnd)) { continue }
        if ($current.ClassName -cne "QFileDialog" -and ([string]$current.AutomationId) -notmatch "(?i)FileDialog") { continue }
        $owner = [IntPtr][P909Native]::GetOwnerHandle($hwnd)
        $ownerRelation = "win32-owner"
        if ($owner -eq [IntPtr]::Zero) {
            # Qt exposes its non-native QFileDialog under the measured workflow
            # dialog in UIA even when USER32 does not report an owner HWND.
            $owner = $importHwnd
            $ownerRelation = "uiA-workflow-dialog"
        }
        $ownerPid = if ($owner -ne [IntPtr]::Zero) { [P909Native]::GetWindowProcessId($owner) } else { 0 }
        if ($ownerPid -ne $Context.process.Id -or [P909Native]::GetWindowProcessId($hwnd) -ne $Context.process.Id) { continue }
        $desc = @($window.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition))
        $edits = @($desc | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Edit })
        if ($edits.Count -gt 0) { $candidates.Add([pscustomobject]@{ window = $window; hwnd = $hwnd; ownerHwnd = $owner; ownerPid = $ownerPid; ownerRelation = $ownerRelation; edits = $edits; descendants = $desc }) }
    }
    if ($candidates.Count -ne 1) {
        Save-UiaTree $Context ("uia/tree-" + $Context.routeId + "-file-picker.json")
        $desktopWindows = @($root.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition) | ForEach-Object {
            $current = $_.Current
            $window = [IntPtr]$current.NativeWindowHandle
            $owner = if ($window -ne [IntPtr]::Zero) { [IntPtr][P909Native]::GetOwnerHandle($window) } else { [IntPtr]::Zero }
            $ownerPid = if ($owner -ne [IntPtr]::Zero) { [Int64][P909Native]::GetWindowProcessId($owner) } else { 0 }
            [ordered]@{ processId = [Int64]$current.ProcessId; nativeWindowHandle = [Int64]$window.ToInt64(); ownerHandle = [Int64]$owner.ToInt64(); ownerProcessId = $ownerPid; name = [string]$current.Name; automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; className = [string]$current.ClassName; isOffscreen = [bool]$current.IsOffscreen }
        })
        Write-JsonFile (Join-Path $EvidenceDirectory ("uia/tree-" + $Context.routeId + "-desktop-windows.json")) ([ordered]@{ routeId = $Context.routeId; processId = [Int64]$Context.process.Id; observedAtUtc = Get-UtcTimestamp; windows = $desktopWindows })
        throw "Expected one owner-bound file picker for process $($Context.process.Id); found $($candidates.Count)."
    }
    return $candidates[0]
}
function Select-WorkbookThroughFilePicker {
    param([object]$Context, [System.Windows.Automation.AutomationElement]$ImportStatusElement, [string]$WorkbookPath)
    $picker = Find-FilePicker $Context $ImportStatusElement
    $editCandidates = @($picker.edits | Where-Object { ([string]$_.Current.AutomationId + " " + [string]$_.Current.Name) -match '(?i)file.?name|filename' })
    $fileNameEdit = if ($editCandidates.Count -eq 1) { $editCandidates[0] } elseif ($picker.edits.Count -eq 1) { $picker.edits[0] } else { $null }
    if ($null -eq $fileNameEdit) { throw "File picker did not expose one unambiguous filename edit control." }
    $before = Get-UiaPropertySnapshot $fileNameEdit
    $value = [System.Windows.Automation.ValuePattern]$fileNameEdit.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $expectedPath = [IO.Path]::GetFullPath($WorkbookPath)
    $value.SetValue($expectedPath)
    $enteredPath = [string]$value.Current.Value
    if ([IO.Path]::GetFullPath($enteredPath) -ine $expectedPath) { throw "File picker filename ValuePattern did not retain the exact workbook path." }
    $buttons = @($picker.descendants | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Button -and $_.Current.Name -match '^(Open|Select|Choose|OK|열기|선택)$' })
    if ($buttons.Count -ne 1) { throw "File picker did not expose one expected Open/Select button; found $($buttons.Count)." }
    $buttonIdentity = Get-UiaPropertySnapshot $buttons[0]
    if ([Int64]$buttonIdentity.processId -ne [P909Native]::GetWindowProcessId($picker.hwnd) -or $picker.ownerPid -ne $Context.process.Id) { throw "File picker UIA target is not owner-bound to the measured process." }
    $buttonAction = Invoke-VerifiedUiaClick $Context $buttons[0] "accept-selected-workbook" "file_picker" "schedule_import_source" -SkipLog
    Add-JsonLine $UiaActionsPath ([ordered]@{
        routeId = $Context.routeId; processInstanceId = $Context.processInstanceId; actionKind = "choose-workbook-file"
        method = "uia-value-pattern-and-verified-button-input"; applicationProcessId = [Int64]$Context.process.Id
        pickerProcessId = [Int64][P909Native]::GetWindowProcessId($picker.hwnd); ownerProcessId = [Int64]$picker.ownerPid
        pickerWindowHandle = [Int64]$picker.hwnd.ToInt64(); ownerWindowHandle = [Int64]$picker.ownerHwnd.ToInt64()
        pickerAutomationId = [string]$picker.window.Current.AutomationId; pickerAccessibleName = [string]$picker.window.Current.Name
        pickerClassName = [string]$picker.window.Current.ClassName; ownerRelation = [string]$picker.ownerRelation
        filenameEdit = $before; filenameValueAfter = $enteredPath; selectedWorkbookPath = $expectedPath
        openButton = $buttonIdentity; openButtonInteraction = $buttonAction; issuedAtUtc = Get-UtcTimestamp
    })
    Wait-ForCondition $Context { param($ctx)
        $remaining = @((Get-AppElements $ctx) | Where-Object { [string]$_.Current.AutomationId -match "(?i)classmngrNextFileDialog" })
        if ($remaining.Count -eq 0) { return $true }; return $false
    } "file picker dismissal after Open" 10 | Out-Null
}
function Get-WorkbookFixture {
    param([string]$FixtureName)
    $source = Join-Path (Join-Path $RepositoryRoot $NormalFixtureRoot) $FixtureName
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Required workbook fixture is missing: $source" }
    $copy = Join-Path (Join-Path $EvidenceDirectory "fixtures") $FixtureName
    Copy-Item -LiteralPath $source -Destination $copy
    $sourceHash = Get-FileSha256 $source; $copyHash = Get-FileSha256 $copy
    if ($sourceHash -cne $copyHash) { throw "Evidence workbook copy hash differs from its source: $FixtureName" }
    [ordered]@{ name = $FixtureName; sourcePath = [IO.Path]::GetFullPath($source); sourceSha256 = $sourceHash; evidencePath = [IO.Path]::GetFullPath($copy); sha256 = $copyHash; sizeBytes = [Int64](Get-Item $copy).Length }
}
function Invoke-BlankWorkspaceGenerator {
    param([string]$WorkspacePath, [string]$RouteId)
    $fixtureGenerator = Join-Path $BuildDirectory "Release\ClassMngrPhase7NextBlankFixtureTests.exe"
    if (-not (Test-Path -LiteralPath $fixtureGenerator -PathType Leaf) -or (Get-PeArchitecture $fixtureGenerator) -cne "x64") { throw "Current-source x64 blank workspace generator is missing." }
    if (Test-Path -LiteralPath $WorkspacePath) { throw "Refusing to overwrite fresh route workspace $WorkspacePath." }
    $envName = "CLASSMNGR_PHASE7_SCHEMA_ONLY_FIXTURE_OUTPUT_PATH"
    $oldOutput = [Environment]::GetEnvironmentVariable($envName, "Process")
    $oldPath = [Environment]::GetEnvironmentVariable("PATH", "Process")
    try {
        [Environment]::SetEnvironmentVariable($envName, $WorkspacePath, "Process")
        [Environment]::SetEnvironmentVariable("PATH", "$script:QtBin;$oldPath", "Process")
        $log = Join-Path $EvidenceDirectory ("logs/" + $RouteId + "-workspace-generator.log")
        & $fixtureGenerator schemaOnlyBlankWorkspaceIsValidAndDeterministic *> $log
        if ($LASTEXITCODE -ne 0) { throw "Blank workspace fixture generator failed; see $log." }
    } finally {
        [Environment]::SetEnvironmentVariable($envName, $oldOutput, "Process")
        [Environment]::SetEnvironmentVariable("PATH", $oldPath, "Process")
    }
    if (-not (Test-Path -LiteralPath $WorkspacePath -PathType Leaf)) { throw "Blank workspace generator produced no file." }
    [ordered]@{ generatorPath = [IO.Path]::GetFullPath($fixtureGenerator); generatorSha256 = Get-FileSha256 $fixtureGenerator; workspacePath = [IO.Path]::GetFullPath($WorkspacePath); workspaceSha256 = Get-FileSha256 $WorkspacePath; workspaceSizeBytes = [Int64](Get-Item $WorkspacePath).Length; logPath = [IO.Path]::GetFullPath($log) }
}
function Invoke-WorkspaceSnapshot {
    param([string]$WorkspacePath, [string]$OutputPath)
    $helper = Join-Path $PSScriptRoot "snapshot_schedule_import_workspace.py"
    if (-not (Test-Path -LiteralPath $helper -PathType Leaf)) { throw "Workspace snapshot helper is missing." }
    $output = & python $helper --workspace $WorkspacePath --output $OutputPath 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Could not capture complete SQLite workspace snapshot: $($output -join ' ')" }
    Get-Content -LiteralPath $OutputPath -Raw | ConvertFrom-Json -ErrorAction Stop
}
function Get-UIASelectionName {
    param([System.Windows.Automation.AutomationElement]$Element)
    $selected = @()
    try { $selected = @(([System.Windows.Automation.SelectionPattern]$Element.GetCurrentPattern([System.Windows.Automation.SelectionPattern]::Pattern)).Current.GetSelection()) } catch { }
    if ($selected.Count -eq 1) { return [string]$selected[0].Current.Name }
    try {
        $valuePattern = [System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
        return [string]$valuePattern.Current.Value
    } catch { }
    return [string]$Element.Current.Name
}
function Get-TraceEvents {
    param([object]$Context)
    if (-not (Test-Path -LiteralPath $Context.tracePath -PathType Leaf)) { return @() }
    @(Get-Content -LiteralPath $Context.tracePath -Encoding UTF8 | ForEach-Object { $_ | ConvertFrom-Json -ErrorAction Stop })
}
function Get-GridMetrics {
    param([System.Windows.Automation.AutomationElement]$Element)
    $result = [ordered]@{ supported = $false; rowCount = $null; columnCount = $null; visibleItems = 0 }
    try {
        $grid = [System.Windows.Automation.GridPattern]$Element.GetCurrentPattern([System.Windows.Automation.GridPattern]::Pattern)
        $result.supported = $true; $result.rowCount = [int]$grid.Current.RowCount; $result.columnCount = [int]$grid.Current.ColumnCount
    } catch { }
    try { $result.visibleItems = @($Element.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)).Count } catch { }
    $result
}
function Invoke-ReviewChoices {
    param([object]$Context, [string]$TableAutomationId, [string]$ChoiceLabel, [int]$ExpectedRows, [string]$TabName = "")
    if ($TabName) {
        $tab = Get-UiaByName $Context $TabName "ControlType.TabItem"
        Invoke-VerifiedUiaClick $Context $tab "select-review-tab" "schedule_import_review" "schedule_import_review" | Out-Null
        Start-Sleep -Milliseconds 250
    }
    $table = Get-UiaElementById $Context $TableAutomationId "ControlType.Table"
    $grid = $null
    try { $grid = [System.Windows.Automation.GridPattern]$table.GetCurrentPattern([System.Windows.Automation.GridPattern]::Pattern) } catch { }
    if ($null -eq $grid -or [int]$grid.Current.RowCount -ne $ExpectedRows -or [int]$grid.Current.ColumnCount -lt 3) {
        Save-UiaTree $Context ("uia/tree-" + $Context.routeId + "-" + $TableAutomationId + ".json")
        throw "Review table '$TableAutomationId' does not expose the exact expected UIA grid ($ExpectedRows rows, action column 2)."
    }
    $methodSummary = [Collections.Generic.List[object]]::new()
    for ($rowIndex = 0; $rowIndex -lt $ExpectedRows; $rowIndex++) {
        $cell = $grid.GetItem($rowIndex, 2)
        $identity = Get-UiaPropertySnapshot $cell
        if ($identity.isOffscreen) {
            try { ([System.Windows.Automation.ScrollItemPattern]$cell.GetCurrentPattern([System.Windows.Automation.ScrollItemPattern]::Pattern)).ScrollIntoView() } catch {
                $scroll = $null; try { $scroll = [System.Windows.Automation.ScrollPattern]$table.GetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern) } catch { }
                if ($null -eq $scroll) { throw "Review row $rowIndex is offscreen and has no UIA scroll path." }
                $scroll.Scroll([System.Windows.Automation.ScrollAmount]::NoAmount, [System.Windows.Automation.ScrollAmount]::LargeIncrement)
            }
            Start-Sleep -Milliseconds 100
            $cell = $grid.GetItem($rowIndex, 2); $identity = Get-UiaPropertySnapshot $cell
        }
        if ($identity.isOffscreen -or [Int64]$identity.processId -ne [Int64]$Context.process.Id -or [double]$identity.rectangle.width -le 0) { throw "Review action cell $rowIndex lacks a visible exact UIA identity." }
        if ([string]$identity.name -ceq $ChoiceLabel) {
            # The normal review mapper preselects Create/Create new when there
            # are no existing matches. Verify that visible UIA value per row;
            # do not open an editor and rewrite an already-correct decision.
            $window = Get-ElementWindowHandle $Context $cell
            $windowPid = [Int64][P909Native]::GetWindowProcessId($window)
            if ($windowPid -ne [Int64]$Context.process.Id) { throw "Review action cell $rowIndex resolved to another process window." }
            $methodSummary.Add([ordered]@{ rowIndex = $rowIndex; actionColumnIndex = 2; cell = $identity; selectedChoice = $ChoiceLabel; observedChoice = [string]$identity.name; selectionMethod = "already-selected-uiavalue" })
            Add-JsonLine $UiaActionsPath ([ordered]@{
                routeId = $Context.routeId; processInstanceId = $Context.processInstanceId
                actionKind = "verify-review-choice"; method = "uia-griditem-current-choice-verified"
                tableAutomationId = $TableAutomationId; rowIndex = $rowIndex; actionColumnIndex = 2
                choice = $ChoiceLabel; observedChoice = [string]$identity.name
                targetAutomationId = [string]$identity.automationId; targetControlType = [string]$identity.controlType
                elementProcessId = [Int64]$identity.processId; windowHandle = [Int64]$window.ToInt64()
                windowProcessId = $windowPid; mainWindowHandle = [Int64]$Context.mainWindowHandle
                mainWindowProcessId = [Int64]$Context.process.Id; issuedAtUtc = Get-UtcTimestamp
            })
            continue
        }
        # The product's QTableView edit trigger is DoubleClicked. Use the
        # verified UIA cell's measured window and dispatch the corresponding
        # native double-click sequence to that exact target.
        Invoke-VerifiedUiaDoubleClick $Context $cell "open-review-choice-editor-double-click" "schedule_import_review" "schedule_import_review" | Out-Null
        try { $editor = Wait-ForUiaElement $Context "scheduleImportChoiceEditor" "ControlType.ComboBox" 8 }
        catch {
            Save-UiaTree $Context ("uia/tree-" + $Context.routeId + "-" + $TableAutomationId + "-row-" + $rowIndex + "-editor-timeout.json")
            throw
        }
        $selected = Get-UIASelectionName $editor
        $editorValue = $null
        try { $editorValue = [System.Windows.Automation.ValuePattern]$editor.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern) } catch { }
        if ($selected -cne $ChoiceLabel) {
            $expanded = $false
            try { ([System.Windows.Automation.ExpandCollapsePattern]$editor.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)).Expand(); $expanded = $true } catch { }
            if ($expanded) {
                $choice = Wait-ForCondition $Context { param($ctx)
                    $items = @((Get-AppElements $ctx) | Where-Object { $_.Current.Name -ceq $ChoiceLabel -and $_.Current.ControlType -in @([System.Windows.Automation.ControlType]::ListItem, [System.Windows.Automation.ControlType]::MenuItem) })
                    if ($items.Count -eq 1) { return $items[0] }; return $null
                } "review choice '$ChoiceLabel' for row $rowIndex" 6
                try { ([System.Windows.Automation.SelectionItemPattern]$choice.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)).Select() }
                catch { Invoke-VerifiedUiaClick $Context $choice "select-review-choice" "schedule_import_review" "schedule_import_review" | Out-Null }
            } elseif ($null -ne $editorValue) { $editorValue.SetValue($ChoiceLabel) }
            else { throw "Choice editor for row $rowIndex has no UIA selection, expansion, or value pattern." }
        }
        Start-Sleep -Milliseconds 100
        $methodSummary.Add([ordered]@{ rowIndex = $rowIndex; actionColumnIndex = 2; cell = $identity; selectedChoice = $ChoiceLabel; observedChoice = $selected; editorValue = if ($null -ne $editorValue) { [string]$editorValue.Current.Value } else { $null } })
        Add-JsonLine $UiaActionsPath ([ordered]@{ routeId = $Context.routeId; processInstanceId = $Context.processInstanceId; actionKind = "set-review-choice"; method = "uia-griditem-and-selection"; tableAutomationId = $TableAutomationId; rowIndex = $rowIndex; actionColumnIndex = 2; choice = $ChoiceLabel; elementProcessId = [Int64]$identity.processId; issuedAtUtc = Get-UtcTimestamp })
    }
    @($methodSummary.ToArray())
}
function Save-AppScreenshot {
    param([object]$Context, [string]$Path)
    Add-Type -AssemblyName System.Drawing
    $rect = [P909Native]::GetWindowRectangle([IntPtr]$Context.mainWindowHandle)
    $width = $rect.right - $rect.left; $height = $rect.bottom - $rect.top
    if ($width -le 0 -or $height -le 0) { throw "Measured main window has invalid screenshot bounds." }
    $bitmap = [Drawing.Bitmap]::new($width, $height)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $captureMethod = "print-window-render-full-content"
    $rendered = $false
    try {
        $hdc = $graphics.GetHdc()
        try { $rendered = [P909Native]::RenderWindowToDeviceContext([IntPtr]$Context.mainWindowHandle, $Context.process.Handle, $hdc, [uint32]$Context.process.Id) }
        finally { $graphics.ReleaseHdc($hdc) }
        if (-not $rendered) {
            $captureMethod = "screen-copy-fallback"
            $graphics.CopyFromScreen($rect.left, $rect.top, 0, 0, [Drawing.Size]::new($width, $height), [Drawing.CopyPixelOperation]::SourceCopy)
        }
        $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $graphics.Dispose(); $bitmap.Dispose() }
    [ordered]@{ path = [IO.Path]::GetFullPath($Path); sha256 = Get-FileSha256 $Path; width = $width; height = $height; capturedAtUtc = Get-UtcTimestamp; windowHandle = [Int64]$Context.mainWindowHandle; windowProcessId = [Int64]$Context.process.Id; captureMethod = $captureMethod; printWindowReturnedContent = [bool]$rendered }
}
function Save-RefreshedScheduleUiaEvidence {
    param([object]$Context, [System.Windows.Automation.AutomationElement]$Page, [System.Windows.Automation.AutomationElement]$Status, [System.Windows.Automation.AutomationElement]$Table, [string]$Path)
    $tableId = [string]$Table.Current.AutomationId
    $tableItems = @($Table.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition) |
        Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::DataItem } |
        ForEach-Object { Get-UiaPropertySnapshot $_ })
    $visibleItems = @($tableItems | Where-Object { -not [bool]$_.isOffscreen })
    $pageSnapshot = Get-UiaPropertySnapshot $Page
    $statusSnapshot = Get-UiaPropertySnapshot $Status
    $tableSnapshot = Get-UiaPropertySnapshot $Table
    $payload = [ordered]@{
        routeId = $Context.routeId; processInstanceId = $Context.processInstanceId
        capturedAtUtc = Get-UtcTimestamp; processId = [Int64]$Context.process.Id
        mainWindowHandle = [Int64]$Context.mainWindowHandle; windowProcessId = [Int64][P909Native]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle)
        page = $pageSnapshot; status = $statusSnapshot; table = $tableSnapshot; tableMetrics = Get-GridMetrics $Table
        tableAutomationId = $tableId; tableDataItems = $tableItems; tableDataItemCount = $tableItems.Count
        visibleDataItems = $visibleItems; visibleDataItemCount = $visibleItems.Count
    }
    Write-JsonFile $Path $payload
    [ordered]@{ path = [IO.Path]::GetFullPath($Path); sha256 = Get-FileSha256 $Path; capturedAtUtc = [string]$payload.capturedAtUtc; processId = [Int64]$Context.process.Id; windowHandle = [Int64]$Context.mainWindowHandle; windowProcessId = [Int64]$payload.windowProcessId; tableAutomationId = $tableId; tableUia = $payload.tableMetrics; tableDataItemCount = [int]$tableItems.Count; visibleDataItemCount = [int]$visibleItems.Count }
}
function Close-MeasuredProcess {
    param([object]$Context)
    $process = $Context.process; $process.Refresh()
    if (-not $process.HasExited) {
        [void]$process.CloseMainWindow()
        if (-not $process.WaitForExit(15000)) { $process.Kill(); $process.WaitForExit() }
    }
    Add-ProcessOutput $Context
    $Context.process.Refresh()
    $Context.exitCode = if ($Context.process.HasExited) { [int]$Context.process.ExitCode } else { $null }
}
function Invoke-Route {
    param([string]$RouteId, [string]$FixtureName, [string]$FixtureRole)
    $fixture = $script:Manifest.fixtures | Where-Object { $_.name -ceq $FixtureName }
    if ($null -eq $fixture) { throw "Fixture identity was not recorded: $FixtureName" }
    $routeRuntime = Join-Path (Join-Path $EvidenceDirectory "runtime") $RouteId
    foreach ($directory in @("settings", "local-app-data", "app-data", "temp", "workspace")) { [void][IO.Directory]::CreateDirectory((Join-Path $routeRuntime $directory)) }
    $workspacePath = Join-Path (Join-Path $routeRuntime "workspace") ($RouteId + ".tps")
    $workspaceGenerator = Invoke-BlankWorkspaceGenerator $workspacePath $RouteId
    $workspaceBeforePath = Join-Path (Join-Path $EvidenceDirectory "workspace-snapshots") ($RouteId + "-before.json")
    $workspaceAfterPath = Join-Path (Join-Path $EvidenceDirectory "workspace-snapshots") ($RouteId + "-after.json")
    $tracePath = Join-Path (Join-Path $EvidenceDirectory "traces") ($RouteId + "-schedule-import.jsonl")
    $routeLogs = Join-Path $EvidenceDirectory "logs"
    $stdoutPath = Join-Path $routeLogs ($RouteId + "-stdout.log"); $stderrPath = Join-Path $routeLogs ($RouteId + "-stderr.log")
    $settingsRoot = Join-Path $routeRuntime "settings"; $localAppData = Join-Path $routeRuntime "local-app-data"
    $appData = Join-Path $routeRuntime "app-data"; $runtimeTemp = Join-Path $routeRuntime "temp"
    $processStart = [Diagnostics.ProcessStartInfo]::new()
    $processStart.FileName = [string]$script:Manifest.application.canonicalPath
    $processStart.WorkingDirectory = Split-Path -Parent $processStart.FileName
    $processStart.UseShellExecute = $false; $processStart.CreateNoWindow = $false
    $processStart.RedirectStandardOutput = $true; $processStart.RedirectStandardError = $true
    $processStart.Arguments = '"' + [IO.Path]::GetFullPath($workspacePath) + '"'
    $processStart.EnvironmentVariables["CLASSMNGR_SETTINGS_ROOT"] = $settingsRoot
    $processStart.EnvironmentVariables["CLASSMNGR_NEXT_SCHEDULE_IMPORT_TRACE_PATH"] = $tracePath
    $processStart.EnvironmentVariables["LOCALAPPDATA"] = $localAppData
    $processStart.EnvironmentVariables["APPDATA"] = $appData
    $processStart.EnvironmentVariables["TEMP"] = $runtimeTemp
    $processStart.EnvironmentVariables["TMP"] = $runtimeTemp
    $processStart.EnvironmentVariables["QT_FORCE_STDERR_LOGGING"] = "1"
    $processStart.EnvironmentVariables["PATH"] = "$script:QtBin;$(Split-Path -Parent $processStart.FileName);$env:PATH"
    $process = [Diagnostics.Process]::new(); $process.StartInfo = $processStart
    if (-not $process.Start()) { throw "Could not start $RouteId packaged application." }
    $process.Refresh()
    $observedPath = [IO.Path]::GetFullPath([P909Native]::GetProcessImagePath($process.Handle))
    $observedHash = Get-FileSha256 $observedPath
    if (-not [string]::Equals($observedPath, [string]$script:Manifest.application.canonicalPath, [StringComparison]::OrdinalIgnoreCase) -or $observedHash -cne [string]$script:Manifest.application.sha256 -or -not [P909Native]::IsNativeX64($process.Handle)) { throw "$RouteId process image path/hash/architecture does not match the staged package." }
    $createdAtUtc = $process.StartTime.ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss.fffffff'Z'")
    $context = [ordered]@{
        routeId = $RouteId; fixtureRole = $FixtureRole; processInstanceId = $RouteId; process = $process
        executablePath = $observedPath; executableSha256 = $observedHash; createdAtUtc = $createdAtUtc
        workspacePath = [IO.Path]::GetFullPath($workspacePath); workspaceSha256AtLaunch = Get-FileSha256 $workspacePath
        workbookPath = [IO.Path]::GetFullPath([string]$fixture.evidencePath); workbookSha256 = [string]$fixture.sha256
        tracePath = [IO.Path]::GetFullPath($tracePath); readers = @(
            [pscustomobject]@{ reader = $process.StandardOutput; stream = "stdout"; path = $stdoutPath; closed = $false; readTask = $process.StandardOutput.ReadLineAsync() },
            [pscustomobject]@{ reader = $process.StandardError; stream = "stderr"; path = $stderrPath; closed = $false; readTask = $process.StandardError.ReadLineAsync() }
        ); logEntries = [Collections.Generic.List[object]]::new(); startupReady = $null; workspaceOpened = $null
        mainWindowHandle = 0L; mainWindowTitle = ""; exitCode = $null; normalUseSamples = @(); operationSamples = @(); actions = @()
        workspaceBeforeSnapshotPath = [IO.Path]::GetFullPath($workspaceBeforePath); workspaceAfterSnapshotPath = [IO.Path]::GetFullPath($workspaceAfterPath)
        workspaceGenerator = $workspaceGenerator; fixture = $fixture; cleanup = $null
    }
    $script:Processes.Add($context)
    $script:CurrentContext = $context
    $route = [ordered]@{
        routeId = $RouteId; fixtureRole = $FixtureRole; fixture = $fixture
        workspace = [ordered]@{ path = [IO.Path]::GetFullPath($workspacePath); sha256AtLaunch = $context.workspaceSha256AtLaunch; workspaceGenerator = $workspaceGenerator; beforeSnapshot = [IO.Path]::GetFullPath($workspaceBeforePath); afterSnapshot = [IO.Path]::GetFullPath($workspaceAfterPath); removedAfterEvidenceCapture = $false }
        processIdentity = [ordered]@{ pid = [Int64]$process.Id; createdAtUtc = $createdAtUtc; canonicalExecutablePath = $observedPath; architecture = "x64"; sha256 = $observedHash; mainWindowHandle = $null; mainWindowTitle = $null }
        startupReady = $null; workspaceOpened = $null; memory = [ordered]@{ startupExternal = $null; settled = $null; postCleanup = $null; idleTail = @(); operationSamples = @() }
        parse = $null; review = $null; apply = $null; refreshedSchedule = $null; uiaActionSequence = @(); processExitCode = $null; cleanup = $null
    }
    $script:Manifest.routes += $route

    Wait-ForCondition $context { param($ctx) @($ctx.logEntries | Where-Object { $_.line -match '^ClassMngrNext startup stage=startup-ready ' }).Count -eq 1 } "startup-ready checkpoint" $ReadinessTimeoutSeconds | Out-Null
    Add-ProcessOutput $context
    $context.workspaceOpened = Get-StartupCheckpoint $context "workspace-opened"
    $context.startupReady = Get-StartupCheckpoint $context "startup-ready"
    if ([Int64]$context.workspaceOpened.logIndex -ge [Int64]$context.startupReady.logIndex) { throw "$RouteId workspace-opened did not precede startup-ready." }
    $route.workspaceOpened = $context.workspaceOpened
    $route.startupReady = $context.startupReady
    $startup = New-ProcessSample $context "startup-ready-external"
    $startupAppWs = [Int64]$context.startupReady.record.metrics.working_set_bytes
    $startup.appCheckpointWorkingSetBytes = $startupAppWs
    $route.memory.startupExternal = $startup
    $context.normalUseSamples += ,([ordered]@{ point = "startup-ready-checkpoint"; workingSetBytes = $startupAppWs; privateBytes = [Int64]$context.startupReady.record.metrics.private_usage_bytes; commitBytes = [Int64]$context.startupReady.record.metrics.private_usage_bytes; timestampUtc = $context.startupReady.observedAtUtc })
    $context.normalUseSamples += ,([ordered]@{ point = "startup-ready-external"; workingSetBytes = $startup.currentWorkingSetBytes; privateBytes = $startup.privateBytes; commitBytes = $startup.commitBytes; timestampUtc = $startup.timestampUtc })
    Start-Sleep -Seconds 6
    $settled = New-ProcessSample $context "startup-settled"
    $route.memory.settled = $settled
    $context.normalUseSamples += ,([ordered]@{ point = "startup-settled"; workingSetBytes = $settled.currentWorkingSetBytes; privateBytes = $settled.privateBytes; commitBytes = $settled.commitBytes; timestampUtc = $settled.timestampUtc })

    $windowDeadline = [DateTimeOffset]::UtcNow.AddSeconds(15); $stable = 0; $stableHandle = 0L; $stableTitle = ""
    while ([DateTimeOffset]::UtcNow -lt $windowDeadline -and $stable -lt 3) {
        Add-ProcessOutput $context; $process.Refresh(); $handle = $process.MainWindowHandle; $title = $process.MainWindowTitle.Trim()
        if ($handle -ne [IntPtr]::Zero -and [P909Native]::IsWindowVisibleForProcess($handle) -and $title.Contains("ClassMngr Next") -and [P909Native]::GetWindowProcessId($handle) -eq [uint32]$process.Id) {
            if ($stableHandle -eq $handle.ToInt64() -and $stableTitle -ceq $title) { $stable++ } else { $stableHandle = $handle.ToInt64(); $stableTitle = $title; $stable = 1 }
        } else { $stable = 0; $stableHandle = 0L; $stableTitle = "" }
        if ($stable -lt 3) { Start-Sleep -Milliseconds 250 }
    }
    if ($stable -lt 3) { throw "$RouteId has no stable visible verified main window." }
    $context.mainWindowHandle = $stableHandle; $context.mainWindowTitle = $stableTitle
    $route.processIdentity.mainWindowHandle = $stableHandle; $route.processIdentity.mainWindowTitle = $stableTitle

    $scheduleItem = Get-UiaByName $context "Schedule" "ControlType.TreeItem"
    $action = Invoke-VerifiedUiaClick $context $scheduleItem "navigate-to-schedule" "my_workspace" "schedule"
    $route.uiaActionSequence += $action
    $schedulePage = Wait-ForUiaElement $context "nextSchedulePage" "ControlType.Group" 15
    # Opening Schedule can persist its display preference. Establish the route
    # baseline only after that ordinary page initialization has settled.
    $beforeSnapshot = Invoke-WorkspaceSnapshot $workspacePath $workspaceBeforePath
    if ($beforeSnapshot.integrityCheck -cne "ok") { throw "$RouteId workspace failed integrity check before import." }
    $route.workspace.beforeSnapshotSha256 = Get-FileSha256 $workspaceBeforePath
    $route.workspace.beforeTableContentSha256 = [string]$beforeSnapshot.tableContentSha256
    $prefs = Get-UiaElementById $context "nextSchedulePreferences" "ControlType.Button"
    $action = Invoke-VerifiedUiaClick $context $prefs "open-schedule-preferences" "schedule" "schedule"
    $route.uiaActionSequence += $action
    $importMenuItem = Wait-ForCondition $context { param($ctx)
        $items = @((Get-AppElements $ctx) | Where-Object { $_.Current.Name -ceq "Import Schedule" -and $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::MenuItem })
        if ($items.Count -eq 1) { return $items[0] }; return $null
    } "Import Schedule menu item" 10
    $action = Invoke-VerifiedUiaClick $context $importMenuItem "open-schedule-import-workflow" "schedule" "schedule_import_source"
    $route.uiaActionSequence += $action
    $importStatus = Wait-ForDialog $context 15
    $route.dialogProcessId = [Int64]$importStatus.Current.ProcessId
    if ([Int64]$route.dialogProcessId -ne [Int64]$process.Id) { throw "$RouteId import dialog belongs to another process." }
    $browse = Get-UiaElementById $context "nextScheduleImportBrowse" "ControlType.Button"
    $action = Invoke-VerifiedUiaClick $context $browse "open-workbook-file-picker" "schedule_import_source" "file_picker"
    $route.uiaActionSequence += $action
    Select-WorkbookThroughFilePicker $context $importStatus $context.workbookPath
    $source = Get-UiaElementById $context "nextScheduleImportSourcePath" "ControlType.Edit"
    $sourceValue = [string]$source.Current.Name
    $sourceValuePattern = $null
    try {
        $sourceValuePattern = [System.Windows.Automation.ValuePattern]$source.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
        $sourceValue = [string]$sourceValuePattern.Current.Value
    } catch { }
    $readButton = Get-UiaElementById $context "nextScheduleImportPrimary" "ControlType.Button"
    $readName = [string]$readButton.Current.Name
    if ($readName -cnotmatch "Read workbook" -or -not $readButton.Current.IsEnabled) {
        Save-UiaTree $context ("uia/tree-" + $RouteId + "-source-selection.json")
        throw "$RouteId workflow primary action is not enabled Read workbook ($readName); source UIA snapshot: $((Get-UiaPropertySnapshot $source | ConvertTo-Json -Compress -Depth 5))."
    }
    $parseStart = Get-UtcTimestamp
    $parseTimer = [Diagnostics.Stopwatch]::StartNew()
    $action = Invoke-VerifiedUiaClick $context $readButton "read-workbook" "schedule_import_source" "schedule_import_selection"
    $route.uiaActionSequence += $action
    $parseTrace = Wait-ForTraceEvent $context "parse-completed" 35
    $parseTimer.Stop()
    $route.parse = [ordered]@{
        requestedAtUtc = $parseStart; completedAtUtc = [string]$parseTrace.timestampUtc; elapsedMilliseconds = $parseTimer.ElapsedMilliseconds
        visibleSheetCount = [int]$parseTrace.visibleSheetCount; scheduleBlockCount = [int]$parseTrace.scheduleBlockCount; candidateCount = [int]$parseTrace.candidateCount
        selectedSheet = [string]$parseTrace.selectedSheet; selectedBlock = [string]$parseTrace.selectedBlock; status = [string]$parseTrace.status
        sourcePath = [string]$parseTrace.sourcePath; workbookSha256 = $context.workbookSha256
    }
    $parsedSourcePath = [string]$parseTrace.sourcePath
    $parsedSourceMatches = $false
    if (-not [string]::IsNullOrWhiteSpace($parsedSourcePath)) {
        try { $parsedSourceMatches = [IO.Path]::GetFullPath($parsedSourcePath) -ieq $context.workbookPath } catch { }
    }
    if (-not $parsedSourceMatches) { throw "$RouteId parser consumed a source path other than the exact UIA-selected workbook (observed '$parsedSourcePath')." }
    Add-JsonLine $UiaActionsPath ([ordered]@{ routeId = $RouteId; processInstanceId = $RouteId; actionKind = "verify-workbook-selection"; method = "uia-value-read-and-parse-trace"; expectedPath = $context.workbookPath; observedPath = $sourceValue; parseTraceSourcePath = $parsedSourcePath; elementProcessId = [Int64]$source.Current.ProcessId; applicationProcessId = [Int64]$context.process.Id; issuedAtUtc = Get-UtcTimestamp })
    $route.memory.operationSamples += ,(New-ProcessSample $context "parse-completed")
    if ($FixtureRole -ceq "conflict-cancel" -and ([int]$parseTrace.candidateCount -ne 96 -or [int]$parseTrace.scheduleBlockCount -ne 5)) { throw "Conflict workbook parser totals differ from 96 candidates / five blocks." }
    if ($FixtureRole -ceq "applicable-apply" -and ([int]$parseTrace.candidateCount -ne 20 -or [int]$parseTrace.scheduleBlockCount -ne 1)) { throw "Applicable workbook parser totals differ from 20 candidates / one block." }
    $reviewButton = Get-UiaElementById $context "nextScheduleImportPrimary" "ControlType.Button"
    if ($reviewButton.Current.Name -cnotmatch "Review selected schedule") { throw "$RouteId primary action did not advance to Review selected schedule." }
    $reviewStart = Get-UtcTimestamp
    $reviewTimer = [Diagnostics.Stopwatch]::StartNew()
    $action = Invoke-VerifiedUiaClick $context $reviewButton "continue-to-review" "schedule_import_selection" "schedule_import_review"
    $route.uiaActionSequence += $action
    $reviewTrace = Wait-ForTraceEvent $context "review-ready" 20
    $reviewTimer.Stop()
    $reviewButton = Get-UiaElementById $context "nextScheduleImportPrimary" "ControlType.Button"
    $reviewStatus = Get-UiaElementById $context "nextScheduleImportReviewStatus" "ControlType.Text"
    $reviewTable = Get-UiaElementById $context "nextScheduleImportClasses" "ControlType.Table"
    $previewTable = Get-UiaElementById $context "nextScheduleImportPreviewTable" "ControlType.Table"
    # The conflict review opens on the Classes tab, so Qt does not expose the
    # Korean teachers table in the UIA tree until that tab is selected. The
    # lifecycle trace reports the full model row count for this state; record
    # the UIA grid only when it is actually exposed instead of failing Route A
    # on a deliberately unselected tab.
    $teacherTables = @((Get-AppElements $context) | Where-Object {
        $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Table -and
        ([string]$_.Current.AutomationId).EndsWith("nextScheduleImportTeachers", [StringComparison]::Ordinal)
    })
    $teacherTableUia = [ordered]@{ supported = $false; rowCount = $null; columnCount = $null; visibleItems = 0; unavailableReason = "tab-not-selected" }
    if ($teacherTables.Count -eq 1) {
        $teacherTableUia = Get-GridMetrics $teacherTables[0]
    } elseif ($teacherTables.Count -gt 1) {
        throw "UIA exposed multiple schedule import teachers tables."
    }
    $route.review = [ordered]@{
        selectedAtUtc = $reviewStart; readyAtUtc = [string]$reviewTrace.timestampUtc; elapsedMilliseconds = $reviewTimer.ElapsedMilliseconds
        selectedSheet = [string]$reviewTrace.selectedSheet; selectedBlock = [string]$reviewTrace.selectedBlock; sourcePath = [string]$reviewTrace.sourcePath
        classRows = [int]$reviewTrace.candidateRows; teacherRows = [int]$reviewTrace.teacherRows; previewRows = [int]$reviewTrace.previewRows
        reviewWidgetCount = [int]$reviewTrace.reviewWidgetCount; applyEnabled = [bool]$reviewTrace.applyEnabled; conflictStatus = [string]$reviewTrace.conflictStatus
        uiApplyEnabled = [bool]$reviewButton.Current.IsEnabled; uiStatus = [string]$reviewStatus.Current.Name
        classTableUia = Get-GridMetrics $reviewTable; teacherTableUia = $teacherTableUia; previewTableUia = Get-GridMetrics $previewTable
    }
    if ($FixtureRole -ceq "conflict-cancel") {
        $reviewScreenPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-conflict-review.json")
        Save-UiaTree $context ("uia/" + $RouteId + "-conflict-review-tree.json")
        if ($route.review.classRows -ne 20 -or $route.review.previewRows -ne 20 -or $route.review.applyEnabled -or $route.review.uiApplyEnabled -or $route.review.reviewWidgetCount -ge 200 -or $route.review.conflictStatus -notmatch '(?i)overlap|conflict|겹') { throw "Conflict review did not expose 20 rows, conflict warning, bounded widgets, and disabled Apply." }
        $conflictCancelButton = Get-UiaElementById $context "nextScheduleImportCancel" "ControlType.Button"
        if ($conflictCancelButton.Current.Name -cne "Cancel") { throw "Expected exact UIA Cancel action in conflict review." }
        # Capture the complete persisted table contents immediately before the
        # authentic UIA Cancel action. Route A must leave every row unchanged.
        $beforeSnapshot = Invoke-WorkspaceSnapshot $workspacePath $workspaceBeforePath
        if ($beforeSnapshot.integrityCheck -cne "ok") { throw "$RouteId workspace failed integrity check immediately before Cancel." }
        $route.workspace.beforeSnapshotSha256 = Get-FileSha256 $workspaceBeforePath
        $route.workspace.beforeTableContentSha256 = [string]$beforeSnapshot.tableContentSha256
        $action = Invoke-VerifiedUiaClick $context $conflictCancelButton "cancel-conflict-review" "schedule_import_review" "schedule"
        $route.uiaActionSequence += $action
        $releaseTrace = Wait-ForTraceEvent $context "retained-data-released" 10
        $route.review.release = $releaseTrace
        $traceEvents = @(Get-TraceEvents $context)
        if (@($traceEvents | Where-Object { $_.event -ceq "apply-request" }).Count -ne 0) { throw "Route A issued an apply request." }
        $dialogGone = $false
        Wait-ForCondition $context { param($ctx)
            $still = @((Get-AppElements $ctx) | Where-Object { $_.Current.AutomationId -match "nextScheduleImportSourceStatus|nextScheduleImportReviewStatus" })
            if ($still.Count -eq 0) { $script:DialogGoneObserved = $true; return $true }; return $false
        } "conflict import dialog destruction" 10 | Out-Null
        $dialogGone = [bool]$script:DialogGoneObserved; $script:DialogGoneObserved = $false
        if (-not $dialogGone) { throw "Route A import dialog was not destroyed after Cancel." }
        $afterSnapshot = Invoke-WorkspaceSnapshot $workspacePath $workspaceAfterPath
        if ([string]$beforeSnapshot.tableContentSha256 -cne [string]$afterSnapshot.tableContentSha256) { throw "Route A workspace table contents changed across cancellation." }
        $route.workspace.afterSnapshotSha256 = Get-FileSha256 $workspaceAfterPath
        $route.workspace.afterTableContentSha256 = [string]$afterSnapshot.tableContentSha256
        $route.workspace.rowLevelContentsUnchanged = $true
        $route.memory.postCleanup = New-ProcessSample $context "post-cleanup"
        $context.normalUseSamples += ,([ordered]@{ point = "post-cleanup"; workingSetBytes = $route.memory.postCleanup.currentWorkingSetBytes; privateBytes = $route.memory.postCleanup.privateBytes; commitBytes = $route.memory.postCleanup.commitBytes; timestampUtc = $route.memory.postCleanup.timestampUtc })
        for ($sample = 0; $sample -lt $IdleTailSamples; $sample++) {
            Start-Sleep -Seconds 1
            $tail = New-ProcessSample $context "post-cleanup-idle-tail" $sample
            $route.memory.idleTail += ,$tail
            $context.normalUseSamples += ,([ordered]@{ point = "post-cleanup-idle-tail"; sampleIndex = $sample; workingSetBytes = $tail.currentWorkingSetBytes; privateBytes = $tail.privateBytes; commitBytes = $tail.commitBytes; timestampUtc = $tail.timestampUtc })
        }
    } else {
        if ($route.review.classRows -ne 20 -or $route.review.previewRows -ne 20 -or $route.review.reviewWidgetCount -ge 200) { throw "Applicable review did not expose exactly 20 class and preview rows with bounded widgets." }
        $teacherChoices = Invoke-ReviewChoices $context "nextScheduleImportTeachers" "Create" 5 "Korean teachers"
        $classChoices = Invoke-ReviewChoices $context "nextScheduleImportClasses" "Create new" 20 "Classes"
        $route.review.teacherChoiceActions = $teacherChoices; $route.review.classChoiceActions = $classChoices
        $applyReady = Wait-ForCondition $context { param($ctx)
            $button = Get-UiaElementById $ctx "nextScheduleImportPrimary" "ControlType.Button"
            if ($button.Current.IsEnabled -and $button.Current.Name -match "Apply schedule") { return $button }; return $null
        } "Apply schedule readiness" 20
        $stateRows = @(Get-TraceEvents $context | Where-Object { $_.event -ceq "review-state" })
        if ($stateRows.Count -eq 0 -or -not [bool]$stateRows[-1].applyEnabled) { throw "Final review choices did not produce an enabled Apply action." }
        $applyStart = Get-UtcTimestamp
        $applyTimer = [Diagnostics.Stopwatch]::StartNew()
        $action = Invoke-VerifiedUiaClick $context $applyReady "apply-selected-schedule" "schedule_import_review" "schedule"
        $route.uiaActionSequence += $action
        $applyTrace = Wait-ForTraceEvent $context "apply-succeeded" 30
        $applyTimer.Stop()
        $events = @(Get-TraceEvents $context)
        $requestTrace = @($events | Where-Object { $_.event -ceq "apply-request" })
        if ($requestTrace.Count -ne 1) { throw "Route B did not issue exactly one recorded apply request." }
        $route.apply = [ordered]@{
            requestedAtUtc = $applyStart; completedAtUtc = [string]$applyTrace.timestampUtc; elapsedMilliseconds = $applyTimer.ElapsedMilliseconds
            request = $requestTrace[0]; result = [ordered]@{ teachersCreated = [int]$applyTrace.teachersCreated; teachersUpdated = [int]$applyTrace.teachersUpdated; classesCreated = [int]$applyTrace.classesCreated; classesUpdated = [int]$applyTrace.classesUpdated; classesSkipped = [int]$applyTrace.classesSkipped; schedulesCleared = [int]$applyTrace.schedulesCleared }
        }
        $route.memory.operationSamples += ,(New-ProcessSample $context "apply-completed")
        $releaseTrace = Wait-ForTraceEvent $context "retained-data-released" 10
        $route.apply.release = $releaseTrace
        $dialogGone = $false
        Wait-ForCondition $context { param($ctx)
            $still = @((Get-AppElements $ctx) | Where-Object { $_.Current.AutomationId -match "nextScheduleImportSourceStatus|nextScheduleImportReviewStatus" })
            if ($still.Count -eq 0) { $script:DialogGoneObserved = $true; return $true }; return $false
        } "applied import dialog destruction" 10 | Out-Null
        $dialogGone = [bool]$script:DialogGoneObserved; $script:DialogGoneObserved = $false
        if (-not $dialogGone) { throw "Route B import dialog was not destroyed after apply." }
        $schedulePage = Get-UiaElementById $context "nextSchedulePage" "ControlType.Group"
        $status = Get-UiaElementById $context "nextScheduleStatus" "ControlType.Text"
        $table = Get-UiaElementById $context "nextScheduleTable" "ControlType.Table"
        $statusText = [string]$status.Current.Name
        if ($statusText -notmatch "Schedule import completed" -or $statusText -notmatch "20 classes created") { throw "Refreshed Schedule page did not expose the applied 20-class summary: $statusText" }
        $refreshedUiaPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-schedule-refreshed.json")
        $refreshedUiaEvidence = Save-RefreshedScheduleUiaEvidence $context $schedulePage $status $table $refreshedUiaPath
        Save-UiaTree $context ("uia/" + $RouteId + "-schedule-refreshed-tree.json")
        $refreshedScreenshot = Save-AppScreenshot $context (Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-schedule-refreshed.png"))
        $route.refreshedSchedule = [ordered]@{
            capturedAtUtc = Get-UtcTimestamp; pageAutomationId = [string]$schedulePage.Current.AutomationId
            pageAccessibleName = [string]$schedulePage.Current.Name; status = $statusText; tableUia = Get-GridMetrics $table
            screenshot = $refreshedScreenshot; uiaSnapshot = $refreshedUiaEvidence
        }
        $afterSnapshot = Invoke-WorkspaceSnapshot $workspacePath $workspaceAfterPath
        $route.workspace.afterSnapshotSha256 = Get-FileSha256 $workspaceAfterPath
        $route.workspace.afterTableContentSha256 = [string]$afterSnapshot.tableContentSha256
        $route.apply.persistedSnapshot = [ordered]@{ integrityCheck = [string]$afterSnapshot.integrityCheck; tableContentSha256 = [string]$afterSnapshot.tableContentSha256; workspaceFileSha256 = [string]$afterSnapshot.workspaceFileSha256 }
        $route.memory.postCleanup = New-ProcessSample $context "post-cleanup"
        $context.normalUseSamples += ,([ordered]@{ point = "post-cleanup"; workingSetBytes = $route.memory.postCleanup.currentWorkingSetBytes; privateBytes = $route.memory.postCleanup.privateBytes; commitBytes = $route.memory.postCleanup.commitBytes; timestampUtc = $route.memory.postCleanup.timestampUtc })
        for ($sample = 0; $sample -lt $IdleTailSamples; $sample++) {
            Start-Sleep -Seconds 1
            $tail = New-ProcessSample $context "post-cleanup-idle-tail" $sample
            $route.memory.idleTail += ,$tail
            $context.normalUseSamples += ,([ordered]@{ point = "post-cleanup-idle-tail"; sampleIndex = $sample; workingSetBytes = $tail.currentWorkingSetBytes; privateBytes = $tail.privateBytes; commitBytes = $tail.commitBytes; timestampUtc = $tail.timestampUtc })
        }
    }

    $route.memory.normalUseSamples = @($context.normalUseSamples)
    $route.memory.operationSamples += @($context.operationSamples)
    Close-MeasuredProcess $context
    $route.processExitCode = $context.exitCode
    $route.stdoutLog = [IO.Path]::GetFullPath($stdoutPath); $route.stderrLog = [IO.Path]::GetFullPath($stderrPath)
    if ($context.exitCode -ne 0) { throw "$RouteId packaged app exited with code $($context.exitCode)." }
    $events = @(Get-TraceEvents $context)
    $route.dialogTrace = @($events)
    $route.tracePath = [IO.Path]::GetFullPath($context.tracePath)
    $route.traceSha256 = Get-FileSha256 $context.tracePath
    $route.fixtureHashAtExit = Get-FileSha256 $context.workbookPath
    $route.workspace.sha256AtExit = Get-FileSha256 $workspacePath
    $cleanupPaths = @(Get-ChildItem -LiteralPath $routeRuntime -File -Recurse | ForEach-Object { [IO.Path]::GetFullPath($_.FullName) })
    $runtimeResolved = [IO.Path]::GetFullPath($routeRuntime)
    if (-not $runtimeResolved.StartsWith([IO.Path]::GetFullPath((Join-Path $EvidenceDirectory "runtime")) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "Refusing cleanup outside this run's runtime directory." }
    Remove-Item -LiteralPath $runtimeResolved -Recurse -Force
    if (Test-Path -LiteralPath $runtimeResolved) { throw "$RouteId temporary workspace/runtime cleanup did not complete." }
    $route.workspace.removedAfterEvidenceCapture = $true
    $route.cleanup = [ordered]@{ runtimeDirectory = $runtimeResolved; removed = $true; removedFileCount = $cleanupPaths.Count; removedPaths = $cleanupPaths; workbookEvidenceCopyPreserved = $true; evidenceCapturedAtUtc = Get-UtcTimestamp }
    $script:Manifest.cleanup += ,([ordered]@{ routeId = $RouteId; runtimeDirectory = $runtimeResolved; removed = $true; removedFileCount = $cleanupPaths.Count; removedPaths = $cleanupPaths; sourceWorkbookEvidenceCopyPreserved = $true })
    return $route
}

try {
    if ($env:OS -cne "Windows_NT") { throw "P9-09 packaged UIA route requires local Windows." }
    $source = Get-SourceInventory $RepositoryRoot
    $script:Manifest.revision = (& git -C $RepositoryRoot rev-parse HEAD 2>$null | Select-Object -First 1)
    $script:Manifest.source = [ordered]@{ root = $source.root; sha256 = $source.sha256; fileCount = $source.fileCount; includedRoots = $source.includedRoots; inventoryPath = [IO.Path]::GetFullPath($SourceInventoryPath) }
    Write-JsonFile $SourceInventoryPath ([ordered]@{ files = @($source.files) })
    $cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { throw "Existing Release build CMakeCache.txt is missing." }
    $cacheLines = @(Get-Content -LiteralPath $cachePath)
    if ([IO.Path]::GetFullPath((Get-RequiredCacheValue $cacheLines "CMAKE_HOME_DIRECTORY")) -ine $RepositoryRoot) { throw "Release build cache points at a different source tree." }
    if ((Get-RequiredCacheValue $cacheLines "CLASSMNGR_NEXT_RELEASE_PACKAGE") -cne "ON") { throw "Existing Release build does not enable the Next package." }
    $generator = Get-RequiredCacheValue $cacheLines "CMAKE_GENERATOR"
    if ($generator -notmatch '^Visual Studio' -or (Get-RequiredCacheValue $cacheLines "CMAKE_GENERATOR_PLATFORM") -notin @("x64", "")) { throw "Existing build is not the expected Windows x64 Visual Studio tree." }
    $cmake = [IO.Path]::GetFullPath((Get-RequiredCacheValue $cacheLines "CMAKE_COMMAND"))
    $qt6Dir = [IO.Path]::GetFullPath((Get-RequiredCacheValue $cacheLines "Qt6_DIR"))
    $script:QtPrefix = [IO.Path]::GetFullPath((Join-Path $qt6Dir "..\..\.."))
    $script:QtBin = Join-Path $script:QtPrefix "bin"
    $windeployqt = Join-Path $script:QtBin "windeployqt.exe"
    if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) { throw "Cache-bound windeployqt is missing." }
    $script:Manifest.build = [ordered]@{ directory = $BuildDirectory; cachePath = [IO.Path]::GetFullPath($cachePath); cacheSha256AtStart = Get-FileSha256 $cachePath; generator = $generator; configuration = "Release"; platform = "x64"; packageOption = "CLASSMNGR_NEXT_RELEASE_PACKAGE=ON"; cmakePath = $cmake; targets = @("ClassMngrNext", "ClassMngrNextNormalScheduleImportIntegrationTests", "ClassMngrPhase7NextBlankFixtureTests") }
    $qtCore = Join-Path $script:QtBin "Qt6Core.dll"; $qtVersionInfo = [Diagnostics.FileVersionInfo]::GetVersionInfo($qtCore)
    $script:Manifest.qt = [ordered]@{ prefix = $script:QtPrefix; qt6Directory = $qt6Dir; version = $qtVersionInfo.ProductVersion; coreDllPath = $qtCore; coreDllSha256 = Get-FileSha256 $qtCore; windeployqtPath = $windeployqt; windeployqtSha256 = Get-FileSha256 $windeployqt }
    $buildStart = Get-UtcTimestamp
    $oldPath = [Environment]::GetEnvironmentVariable("PATH", "Process")
    try {
        [Environment]::SetEnvironmentVariable("PATH", "$script:QtBin;$oldPath", "Process")
        $buildOutput = @(& $cmake --build $BuildDirectory --config Release --verbose --target ClassMngrNext ClassMngrNextNormalScheduleImportIntegrationTests ClassMngrPhase7NextBlankFixtureTests 2>&1 | ForEach-Object { [string]$_ })
        $buildExit = $LASTEXITCODE
    } finally { [Environment]::SetEnvironmentVariable("PATH", $oldPath, "Process") }
    [IO.File]::WriteAllLines($BuildLogPath, @("command: cmake --build `"$BuildDirectory`" --config Release --verbose --target ClassMngrNext ClassMngrNextNormalScheduleImportIntegrationTests ClassMngrPhase7NextBlankFixtureTests", "startedAtUtc: $buildStart", $buildOutput, "exitCode: $buildExit", "completedAtUtc: $(Get-UtcTimestamp)"), [Text.UTF8Encoding]::new($false))
    $script:Manifest.build.exitCode = $buildExit; $script:Manifest.build.succeeded = ($buildExit -eq 0); $script:Manifest.build.logPath = [IO.Path]::GetFullPath($BuildLogPath); $script:Manifest.build.logSha256 = Get-FileSha256 $BuildLogPath
    if ($buildExit -ne 0) { throw "Required current-source Release targets failed; see cmake-build.log." }
    $script:Manifest.build.cacheSha256After = Get-FileSha256 $cachePath
    $releaseExe = Join-Path $BuildDirectory "next-package-build\Release\ClassMngr.exe"
    if (-not (Test-Path -LiteralPath $releaseExe -PathType Leaf) -or (Get-PeArchitecture $releaseExe) -cne "x64") { throw "Current-source optimized Next Release executable is missing or not x64." }
    $packageDirectory = Join-Path $EvidenceDirectory "package"
    $resourceInstall = @(& $cmake --install $BuildDirectory --config Release --prefix $packageDirectory --component ClassMngrNextResources 2>&1 | ForEach-Object { [string]$_ })
    if ($LASTEXITCODE -ne 0) { [IO.File]::WriteAllLines((Join-Path $EvidenceDirectory "logs/resource-install.log"), $resourceInstall, [Text.UTF8Encoding]::new($false)); throw "Could not install current-source Next package resources." }
    $appPath = Join-Path $packageDirectory "ClassMngrNext.exe"
    Copy-Item -LiteralPath $releaseExe -Destination $appPath
    $oldPath = [Environment]::GetEnvironmentVariable("PATH", "Process")
    $oldErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        [Environment]::SetEnvironmentVariable("PATH", "$script:QtBin;$oldPath", "Process")
        $deployOutput = @(& $windeployqt --qmldir (Join-Path $RepositoryRoot "src\features\calendar\ui\qml") --skip-plugin-types qmltooling --include-plugins qoffscreen,qsqlite --exclude-plugins qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql --translations en,ko --compiler-runtime --dir $packageDirectory $appPath 2>&1 | ForEach-Object { [string]$_ })
        $deployExit = $LASTEXITCODE
    } finally {
        [Environment]::SetEnvironmentVariable("PATH", $oldPath, "Process")
        $ErrorActionPreference = $oldErrorActionPreference
    }
    [IO.File]::WriteAllLines((Join-Path $EvidenceDirectory "logs/windeployqt.log"), $deployOutput, [Text.UTF8Encoding]::new($false))
    if ($deployExit -ne 0) { throw "windeployqt failed with exit code $deployExit." }
    foreach ($runtimeFile in @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Sql.dll", "platforms\qwindows.dll", "sqldrivers\qsqlite.dll")) { if (-not (Test-Path -LiteralPath (Join-Path $packageDirectory $runtimeFile) -PathType Leaf)) { throw "Staged package is missing $runtimeFile." } }
    $packageInventory = Get-CanonicalInventory $packageDirectory
    Write-JsonFile $PackageInventoryPath ([ordered]@{ files = @($packageInventory.files) })
    $script:Manifest.application = [ordered]@{ canonicalPath = [IO.Path]::GetFullPath($appPath); sha256 = Get-FileSha256 $appPath; architecture = "x64"; releaseExecutablePath = [IO.Path]::GetFullPath($releaseExe); releaseExecutableSha256 = Get-FileSha256 $releaseExe; candidateSourceSha256 = $source.sha256 }
    $script:Manifest.package = [ordered]@{ directory = [IO.Path]::GetFullPath($packageDirectory); sha256 = $packageInventory.sha256; fileCount = $packageInventory.fileCount; applicationRelativePath = "ClassMngrNext.exe"; applicationSha256 = Get-FileSha256 $appPath; candidateSourceSha256 = $source.sha256; sourceIdentityMode = "current-source-build-and-package"; resourceCatalogRelativePath = "resources/resource-catalog.json"; runtimeFiles = @(Get-PackagedRuntimeInventory $packageDirectory $script:QtPrefix) }
    if (-not (Test-Path -LiteralPath (Join-Path $packageDirectory "resources\resource-catalog.json") -PathType Leaf)) { throw "Staged current-source package is missing resource catalog." }
    $script:Manifest.fixtures = @((Get-WorkbookFixture $ConflictFixtureName), (Get-WorkbookFixture $ApplicableFixtureName))
    Add-Type -AssemblyName UIAutomationClient
    Add-Type -AssemblyName UIAutomationTypes
    $routeA = Invoke-Route "route-a-conflict-cancel" $ConflictFixtureName "conflict-cancel"
    $routeB = Invoke-Route "route-b-applicable-apply" $ApplicableFixtureName "applicable-apply"
    $script:Manifest.completedAtUtc = Get-UtcTimestamp
    $script:Manifest.status = "evidence-collected"
    $script:Manifest.budgetEvaluation = "pending-independent-validation"
} catch {
    $script:Manifest.status = "failed"
    $script:Manifest.budgetEvaluation = "not-evaluated-invalid-or-incomplete-attempt"
    $script:Manifest.failure = [ordered]@{ timestampUtc = Get-UtcTimestamp; message = $_.Exception.Message; category = [string]$_.CategoryInfo.Category; scriptStackTrace = [string]$_.ScriptStackTrace }
} finally {
    foreach ($context in $script:Processes) {
        try { $context.process.Refresh(); if (-not $context.process.HasExited) { Close-MeasuredProcess $context } } catch { }
    }
    $script:Manifest.routes = @($script:Manifest.routes)
    Write-JsonFile $ManifestPath $script:Manifest
    $validator = Join-Path $PSScriptRoot "validate_packaged_next_schedule_import.py"
    if ((Test-Path -LiteralPath $validator -PathType Leaf) -and $script:Manifest.status -ceq "evidence-collected") {
        $oldErrorActionPreference = $ErrorActionPreference
        try { $ErrorActionPreference = "Continue"; $validatorOutput = @(& python $validator --evidence-dir $EvidenceDirectory 2>&1 | ForEach-Object { [string]$_ }); $validatorExit = $LASTEXITCODE }
        finally { $ErrorActionPreference = $oldErrorActionPreference }
        [IO.File]::WriteAllLines($ValidatorOutputPath, $validatorOutput, [Text.UTF8Encoding]::new($false))
        if ($validatorExit -eq 0 -and $script:Manifest.status -eq "evidence-collected") {
            $script:Manifest.status = "passed"; $script:Manifest.budgetEvaluation = "evaluated-by-independent-validator"
        } elseif ($validatorExit -ne 0 -and $script:Manifest.status -eq "evidence-collected") {
            $script:Manifest.status = "failed-validation"; $script:Manifest.budgetEvaluation = "invalid-or-incomplete-evidence"
            $script:Manifest.failure = [ordered]@{ timestampUtc = Get-UtcTimestamp; message = "Independent evidence validation failed."; validatorExitCode = $validatorExit }
        }
        Write-JsonFile $ManifestPath $script:Manifest
    } elseif (-not (Test-Path -LiteralPath $validator -PathType Leaf)) {
        $script:Manifest.status = "failed-validation"; $script:Manifest.failure = [ordered]@{ timestampUtc = Get-UtcTimestamp; message = "Independent validator is missing." }
        Write-JsonFile $ManifestPath $script:Manifest
    } else {
        [IO.File]::WriteAllText($ValidatorOutputPath, "Validation skipped because this was an invalid or incomplete attempt; its memory budget was not evaluated.`n", [Text.UTF8Encoding]::new($false))
    }
}

if ($script:Manifest.status -cne "passed") {
    Write-Error "P9-09 route failed. Evidence preserved at $EvidenceDirectory. See manifest.json and validator-output.txt."
    exit 1
}
Write-Output "P9-09 packaged Schedule Import route passed: $EvidenceDirectory"
