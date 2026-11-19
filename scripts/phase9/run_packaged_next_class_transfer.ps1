<#
.SYNOPSIS
Runs the packaged Next Class Transfer export, apply, and cancel routes.

.DESCRIPTION
Uses three fresh local Windows processes and prepared copies of the retained
large-startup fixture. Route 1 exports the package through the Next UI. Routes
2 and 3 consume those exact exported bytes through the Next import UI.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = "",
    [string]$BuildDirectory = "build\windows-x64-release",
    [string]$EvidenceRoot = "artifacts\phase9\packaged-next-class-transfer",
    [Parameter(Mandatory = $true)][string]$ReleaseBuildReceiptPath,
    [string]$PriorPackageDirectory = "",
    [string]$PriorPackageInventoryPath = "",
    [string]$PriorRunManifestPath = "",
    [switch]$Route2OnlyDiagnostic,
    [string]$Route2DiagnosticSourceManifestPath = "",
    [switch]$Route2AfterExportShutdownDiagnostic,
    [switch]$Route2AfterExportDumpDiagnostic,
    [ValidateRange(30, 300)][int]$ReadinessTimeoutSeconds = 120,
    [ValidateRange(1, 30)][int]$IdleTailSamples = 10
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$TaskId = "P9_11_NEXT_CLASS_TRANSFER_PACKAGED_LIFECYCLE"
$ManifestSchema = "classmngr-phase9-packaged-next-class-transfer-v1"
$WorkingSetLimit = [Int64]262144000
$OperationPeakLimit = [Int64]536870912
$SeedSqlRelative = "tests\fixtures\workspaces\large_startup.sql"
$SelectedClasses = 48
$SelectedTeachers = 24
$RegularScheduleRows = 16
$IntensiveScheduleRows = 24
$RosterCellCount = 3600
$EvaluationCellCount = 6600
$SourceRoots = @(
    "CMakeLists.txt", "CMakePresets.json", "cmake", "resources", "scripts\phase9",
    "src", "tests", "docs\qt-rewrite\visual-baseline\release\large-class-transfer-boundary"
)
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) { $RepositoryRoot = Join-Path $PSScriptRoot "..\.." }
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$BuildDirectory = if ([IO.Path]::IsPathRooted($BuildDirectory)) { [IO.Path]::GetFullPath($BuildDirectory) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $BuildDirectory)) }
$EvidenceRoot = if ([IO.Path]::IsPathRooted($EvidenceRoot)) { [IO.Path]::GetFullPath($EvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
$ReleaseBuildReceiptPath = [IO.Path]::GetFullPath($ReleaseBuildReceiptPath)
if ($Route2OnlyDiagnostic) {
    if ($Route2AfterExportShutdownDiagnostic -or $Route2AfterExportDumpDiagnostic) { throw 'Choose one Route 2 shutdown diagnostic mode.' }
    if ([string]::IsNullOrWhiteSpace($Route2DiagnosticSourceManifestPath)) { throw '-Route2OnlyDiagnostic requires -Route2DiagnosticSourceManifestPath.' }
    $Route2DiagnosticSourceManifestPath = [IO.Path]::GetFullPath($Route2DiagnosticSourceManifestPath)
} elseif (-not [string]::IsNullOrWhiteSpace($Route2DiagnosticSourceManifestPath)) {
    throw '-Route2DiagnosticSourceManifestPath is valid only with -Route2OnlyDiagnostic.'
}
if ($Route2AfterExportShutdownDiagnostic -and $Route2AfterExportDumpDiagnostic) { throw 'Choose either the UIA or dump-focused Route 1 + Route 2 diagnostic mode.' }
$RunId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + "-" + [Guid]::NewGuid().ToString("N")
$EvidenceDirectory = Join-Path $EvidenceRoot $RunId
if (Test-Path -LiteralPath $EvidenceDirectory) { throw "Refusing to overwrite evidence directory $EvidenceDirectory." }
[void][IO.Directory]::CreateDirectory($EvidenceDirectory)
foreach ($directory in @("package", "logs", "uia", "runtime", "workspace-snapshots", "workspace-preparation", "build-derivation")) {
    [void][IO.Directory]::CreateDirectory((Join-Path $EvidenceDirectory $directory))
}

$ManifestPath = Join-Path $EvidenceDirectory "manifest.json"
$SourceInventoryPath = Join-Path $EvidenceDirectory "source-files.json"
$PackageInventoryPath = Join-Path $EvidenceDirectory "package-files.json"
$ProcessMetricsPath = Join-Path $EvidenceDirectory "process-metrics.jsonl"
$UiaActionsPath = Join-Path $EvidenceDirectory "uia-actions.jsonl"
$ValidatorOutputPath = Join-Path (Join-Path $EvidenceDirectory "logs") "validator-output.txt"
$BuildDerivationDirectory = Join-Path $EvidenceDirectory "build-derivation"
$BuildDerivationPath = Join-Path $BuildDerivationDirectory "derived-receipt.json"
$MasterBlankPath = Join-Path (Join-Path $EvidenceDirectory "workspace-snapshots") "master-blank.tps"
$ExportedPackagePath = Join-Path (Join-Path $EvidenceDirectory "package") "next-exported-classes.json"
$script:CurrentContext = $null
$script:Processes = [Collections.Generic.List[object]]::new()
$script:Manifest = [ordered]@{
    schema = $ManifestSchema; taskId = $TaskId; runId = $RunId; status = "not-evaluated"
    routeMode = "current-source-packaged-next-on-local-windows"; startedAtUtc = [DateTimeOffset]::UtcNow.ToString("o")
    completedAtUtc = $null; repositoryRoot = $RepositoryRoot; source = $null; build = $null
    diagnosticMode = if ($Route2OnlyDiagnostic) { 'route-2-only-shutdown-diagnostic' } elseif ($Route2AfterExportShutdownDiagnostic) { 'route-2-after-export-shutdown-diagnostic' } elseif ($Route2AfterExportDumpDiagnostic) { 'route-2-after-export-dump-diagnostic' } else { $null }
    releaseBuildReceipt = $null; application = $null; package = $null; workspacePreparation = @()
    thresholds = [ordered]@{
        normalUseWorkingSetBytesExclusive = $WorkingSetLimit
        operationPeakWorkingSetBytesAdvisoryExclusive = $OperationPeakLimit
        operationPeakEvaluation = "advisory-only; no private-byte return allowance is defined"
        privateBytesReturnAllowance = $null; strictAtOrAboveFails = $true
    }
    routeExpectations = [ordered]@{
        sourceClasses = $SelectedClasses; sourceTeachers = $SelectedTeachers
        regularSchedules = $RegularScheduleRows; intensiveSchedules = $IntensiveScheduleRows
        rosterCells = $RosterCellCount; speakingEvaluationCells = $EvaluationCellCount
        applyDestinationTransformation = "clear class_times and class_intensive_times; namespace class/teacher/evaluation identifiers; preserve other seeded rows"
        cancelDestinationTransformation = "none; untouched seeded dense clone"
        naturalProcessExitRequired = $true; calendarTransferSuspendReentryCount = 1
    }
    routes = @(); files = [ordered]@{
        sourceInventory = "source-files.json"; packageInventory = "package-files.json"
        processMetrics = "process-metrics.jsonl"; uiaActions = "uia-actions.jsonl"; startupLog = "startup-log.jsonl"
        buildDerivation = "build-derivation/derived-receipt.json"
        validatorOutput = "logs/validator-output.txt"
    }
    failure = $null; validation = $null
}

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -TypeDefinition @'
using System;
using System.Collections.Concurrent;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

public sealed class P911MemoryCounters
{
    public ulong workingSetBytes;
    public ulong privateBytes;
    public ulong commitBytes;
    public ulong peakWorkingSetBytes;
    public ulong peakCommitBytes;
}

public static class P911Native
{
    private const ushort ImageFileMachineUnknown = 0x0000;
    private const ushort ImageFileMachineAmd64 = 0x8664;
    private const int SwRestore = 9;
    private const uint InputMouse = 0;
    private const uint WmMouseMove = 0x0200;
    private const uint WmLeftButtonDown = 0x0201;
    private const uint WmLeftButtonUp = 0x0202;
    private const uint WmMouseWheel = 0x020A;
    private const int MouseButtonLeft = 0x0001;
    private const uint LeftDown = 0x0002;
    private const uint LeftUp = 0x0004;

    [StructLayout(LayoutKind.Sequential)]
    private struct MouseInput { public int dx; public int dy; public uint mouseData; public uint flags; public uint time; public UIntPtr extraInfo; }
    [StructLayout(LayoutKind.Sequential)]
    private struct Input { public uint type; public MouseInput mouseInput; }
    [StructLayout(LayoutKind.Sequential)]
    private struct Point { public int x; public int y; }
    [StructLayout(LayoutKind.Sequential)]
    private struct ProcessMemoryCountersEx
    {
        public uint cb; public uint pageFaultCount; public UIntPtr peakWorkingSetSize; public UIntPtr workingSetSize;
        public UIntPtr quotaPeakPagedPoolUsage; public UIntPtr quotaPagedPoolUsage; public UIntPtr quotaPeakNonPagedPoolUsage;
        public UIntPtr quotaNonPagedPoolUsage; public UIntPtr pagefileUsage; public UIntPtr peakPagefileUsage; public UIntPtr privateUsage;
    }

    [DllImport("kernel32.dll", SetLastError=true)] private static extern bool IsWow64Process2(IntPtr process, out ushort processMachine, out ushort nativeMachine);
    [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] private static extern bool QueryFullProcessImageNameW(IntPtr process, uint flags, StringBuilder imageName, ref int size);
    [DllImport("psapi.dll", SetLastError=true)] private static extern bool GetProcessMemoryInfo(IntPtr process, ref ProcessMemoryCountersEx counters, uint size);
    [DllImport("user32.dll", SetLastError=true)] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool BringWindowToTop(IntPtr window);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll", SetLastError=true)] private static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll", SetLastError=true)] private static extern uint SendInput(uint count, Input[] inputs, int size);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool ScreenToClient(IntPtr window, ref Point point);
    [DllImport("user32.dll", SetLastError=true, EntryPoint="PostMessageW")] private static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", SetLastError=true)] private static extern uint WaitForInputIdle(IntPtr process, uint milliseconds);
    [DllImport("kernel32.dll")] private static extern uint GetCurrentThreadId();

    public static bool IsNativeX64(IntPtr process)
    {
        ushort processMachine; ushort nativeMachine;
        if (!IsWow64Process2(process, out processMachine, out nativeMachine)) throw new Win32Exception(Marshal.GetLastWin32Error(), "IsWow64Process2 failed.");
        return nativeMachine == ImageFileMachineAmd64 && processMachine == ImageFileMachineUnknown;
    }
    public static string GetProcessImagePath(IntPtr process)
    {
        StringBuilder path = new StringBuilder(32768); int size = path.Capacity;
        if (!QueryFullProcessImageNameW(process, 0, path, ref size)) throw new Win32Exception(Marshal.GetLastWin32Error(), "QueryFullProcessImageName failed.");
        return path.ToString();
    }
    public static uint GetWindowProcessId(IntPtr window)
    {
        uint pid; if (GetWindowThreadProcessId(window, out pid) == 0) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowThreadProcessId failed."); return pid;
    }
    public static object PostVerifiedWindowClick(IntPtr window, int screenX, int screenY, uint expectedPid)
    {
        uint windowPid = GetWindowProcessId(window);
        if (windowPid != expectedPid) throw new InvalidOperationException("UIA target HWND belongs to another process.");
        Point client = new Point { x=screenX, y=screenY };
        if (!ScreenToClient(window, ref client)) throw new Win32Exception(Marshal.GetLastWin32Error(), "ScreenToClient failed.");
        long packed = ((long)(ushort)client.y << 16) | (ushort)client.x;
        IntPtr coordinates = new IntPtr(packed);
        bool movePosted = PostMessage(window, WmMouseMove, IntPtr.Zero, coordinates);
        if (!movePosted) throw new Win32Exception(Marshal.GetLastWin32Error(), "PostMessage(WM_MOUSEMOVE) failed.");
        bool downPosted = PostMessage(window, WmLeftButtonDown, new IntPtr(MouseButtonLeft), coordinates);
        if (!downPosted) throw new Win32Exception(Marshal.GetLastWin32Error(), "PostMessage(WM_LBUTTONDOWN) failed.");
        bool upPosted = PostMessage(window, WmLeftButtonUp, IntPtr.Zero, coordinates);
        if (!upPosted) throw new Win32Exception(Marshal.GetLastWin32Error(), "PostMessage(WM_LBUTTONUP) failed.");
        return new { expectedProcessId=(long)expectedPid, windowProcessId=(long)windowPid, messageTargetWindowHandle=window.ToInt64(), screenX=screenX, screenY=screenY, clientX=client.x, clientY=client.y, mouseMovePosted=movePosted, leftButtonDownPosted=downPosted, leftButtonUpPosted=upPosted };
    }
    public static object PostVerifiedWindowWheel(IntPtr window, int screenX, int screenY, int delta, uint expectedPid)
    {
        uint windowPid = GetWindowProcessId(window);
        if (windowPid != expectedPid) throw new InvalidOperationException("UIA scroll target HWND belongs to another process.");
        long coordinates = ((long)(ushort)screenY << 16) | (ushort)screenX;
        long wheel = ((long)(ushort)delta << 16);
        bool posted = PostMessage(window, WmMouseWheel, new IntPtr(wheel), new IntPtr(coordinates));
        if (!posted) throw new Win32Exception(Marshal.GetLastWin32Error(), "PostMessage(WM_MOUSEWHEEL) failed.");
        return new { expectedProcessId=(long)expectedPid, windowProcessId=(long)windowPid, messageTargetWindowHandle=window.ToInt64(), screenX=screenX, screenY=screenY, wheelDelta=delta, wheelPosted=posted };
    }
    public static bool IsWindowVisibleForProcess(IntPtr window) { return IsWindowVisible(window); }
    public static P911MemoryCounters GetMemoryCounters(IntPtr process)
    {
        ProcessMemoryCountersEx counters = new ProcessMemoryCountersEx(); counters.cb = (uint)Marshal.SizeOf(typeof(ProcessMemoryCountersEx));
        if (!GetProcessMemoryInfo(process, ref counters, counters.cb)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetProcessMemoryInfo failed.");
        return new P911MemoryCounters { workingSetBytes=counters.workingSetSize.ToUInt64(), privateBytes=counters.privateUsage.ToUInt64(), commitBytes=counters.pagefileUsage.ToUInt64(), peakWorkingSetBytes=counters.peakWorkingSetSize.ToUInt64(), peakCommitBytes=counters.peakPagefileUsage.ToUInt64() };
    }
    public static object ClickWindowPoint(IntPtr window, IntPtr process, int x, int y, uint expectedPid)
    {
        uint windowPid = GetWindowProcessId(window);
        if (windowPid != expectedPid) throw new InvalidOperationException("UIA target HWND belongs to another process.");
        if (WaitForInputIdle(process, 10000) != 0) throw new InvalidOperationException("Measured process did not reach input-idle before UIA click.");
        ShowWindow(window, SwRestore); BringWindowToTop(window); SetForegroundWindow(window); System.Threading.Thread.Sleep(100);
        IntPtr foreground = GetForegroundWindow(); uint foregroundPid = foreground == IntPtr.Zero ? 0 : GetWindowProcessId(foreground);
        if (foregroundPid != expectedPid) throw new InvalidOperationException("Measured process could not be brought to foreground for UIA click.");
        if (!SetCursorPos(x, y)) throw new Win32Exception(Marshal.GetLastWin32Error(), "SetCursorPos failed.");
        Input[] input = new Input[2]; input[0].type=InputMouse; input[0].mouseInput.flags=LeftDown; input[1].type=InputMouse; input[1].mouseInput.flags=LeftUp;
        uint sent = SendInput(2, input, Marshal.SizeOf(typeof(Input)));
        if (sent != 2) throw new Win32Exception(Marshal.GetLastWin32Error(), "SendInput failed.");
        return new { expectedProcessId=(long)expectedPid, windowProcessId=(long)windowPid, foregroundProcessId=(long)foregroundPid, foregroundWindowHandle=foreground.ToInt64(), foregroundVerified=true, waitForInputIdleResult=0, inputEventsSent=sent, screenX=x, screenY=y };
    }
}

public static class P911OutputPump
{
    public static Task Start(StreamReader reader, ConcurrentQueue<string> lines)
    {
        if (reader == null) throw new ArgumentNullException("reader");
        if (lines == null) throw new ArgumentNullException("lines");
        return Task.Factory.StartNew(() =>
        {
            string line;
            while ((line = reader.ReadLine()) != null) lines.Enqueue(line);
        }, CancellationToken.None, TaskCreationOptions.LongRunning, TaskScheduler.Default);
    }
}
'@

Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;

public static class P911DumpNative
{
    private const uint MiniDumpWithThreadInfo = 0x00001000;

    [DllImport("dbghelp.dll", SetLastError = true, EntryPoint = "MiniDumpWriteDump")]
    private static extern bool MiniDumpWriteDumpNative(
        IntPtr processHandle,
        uint processId,
        IntPtr dumpFileHandle,
        uint dumpType,
        IntPtr exceptionParam,
        IntPtr userStreamParam,
        IntPtr callbackParam);

    public static long WriteThreadInfoMiniDump(IntPtr processHandle, int processId, string path)
    {
        if (processHandle == IntPtr.Zero || processId <= 0)
            throw new ArgumentException("A live measured process handle and PID are required.");
        if (String.IsNullOrWhiteSpace(path))
            throw new ArgumentException("A unique dump artifact path is required.");

        using (FileStream dumpFile = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
        {
            if (!MiniDumpWriteDumpNative(processHandle, unchecked((uint)processId),
                dumpFile.SafeFileHandle.DangerousGetHandle(), MiniDumpWithThreadInfo,
                IntPtr.Zero, IntPtr.Zero, IntPtr.Zero))
            {
                int error = Marshal.GetLastWin32Error();
                throw new Win32Exception(error, "MiniDumpWriteDump(MiniDumpWithThreadInfo) failed.");
            }
            dumpFile.Flush(true);
            return dumpFile.Length;
        }
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
function Write-JsonFile { param([string]$Path, [object]$Value) [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 60), [Text.UTF8Encoding]::new($false)) }
function Add-JsonLine { param([string]$Path, [object]$Value) [IO.File]::AppendAllText($Path, ($Value | ConvertTo-Json -Depth 40 -Compress) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false)) }
function Get-CanonicalInventory {
    param([string]$Root)
    $base = [IO.Path]::GetFullPath($Root).TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($file in @(Get-ChildItem -LiteralPath $Root -File -Recurse)) {
        if ($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @('.pyc','.pyo')) { continue }
        $path = [IO.Path]::GetFullPath($file.FullName)
        if (-not $path.StartsWith($base, [StringComparison]::OrdinalIgnoreCase)) { throw "Inventory path escaped root: $path" }
        $rows.Add([ordered]@{ path=$path.Substring($base.Length).Replace('\','/'); sizeBytes=[Int64]$file.Length; sha256=Get-FileSha256 $path })
    }
    $rows.Sort([System.Comparison[object]]{ param($a,$b) [StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(),([string]$b.path).ToLowerInvariant()) })
    $sorted=@($rows.ToArray()); $canonical=@($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path,$_.sizeBytes,$_.sha256 }) -join "`n"
    [ordered]@{ root=[IO.Path]::GetFullPath($Root); sha256=Get-StringSha256 ($canonical+"`n"); fileCount=$sorted.Count; files=$sorted }
}
function Get-SourceInventory {
    $rows = [Collections.Generic.List[object]]::new()
    $prefix = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
    foreach ($relative in $SourceRoots) {
        $target=Join-Path $RepositoryRoot $relative
        if (-not (Test-Path -LiteralPath $target)) { throw "Missing source inventory root $relative" }
        $scan=if (Test-Path -LiteralPath $target -PathType Leaf) { @(Get-Item -LiteralPath $target) } else { @(Get-ChildItem -LiteralPath $target -File -Recurse) }
        foreach ($file in $scan) {
            if ($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @('.pyc','.pyo')) { continue }
            $canonical=[IO.Path]::GetFullPath($file.FullName)
            if (-not $canonical.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw "Source inventory path escaped repository: $canonical" }
            $rows.Add([ordered]@{ path=$canonical.Substring($prefix.Length).Replace('\','/'); sizeBytes=[Int64]$file.Length; sha256=Get-FileSha256 $canonical })
        }
    }
    $rows.Sort([System.Comparison[object]]{ param($a,$b) [StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(),([string]$b.path).ToLowerInvariant()) })
    $sorted=@($rows.ToArray()); $canonical=@($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path,$_.sizeBytes,$_.sha256 }) -join "`n"
    [ordered]@{ root=$RepositoryRoot; includedRoots=$SourceRoots; sha256=Get-StringSha256 ($canonical+"`n"); fileCount=$sorted.Count; files=$sorted }
}
function Get-InventoryFingerprint {
    param([object[]]$Rows)
    $sorted=[Collections.Generic.List[object]]::new()
    foreach ($row in $Rows) { $sorted.Add([ordered]@{path=[string]$row.path;sizeBytes=[Int64]$row.sizeBytes;sha256=([string]$row.sha256).ToLowerInvariant()}) }
    $sorted.Sort([System.Comparison[object]]{ param($a,$b) [StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(),([string]$b.path).ToLowerInvariant()) })
    $canonical=@($sorted.ToArray() | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path,$_.sizeBytes,$_.sha256 }) -join "`n"
    Get-StringSha256 ($canonical+"`n")
}
function Compare-ScriptOnlySourceDelta {
    param([object[]]$BuildRows,[object[]]$CurrentRows)
    $allowed=@(
        'scripts/phase9/prepare_class_transfer_workspace.py',
        'scripts/phase9/run_packaged_next_class_transfer.ps1',
        'scripts/phase9/test_prepare_class_transfer_workspace.py',
        'scripts/phase9/validate_packaged_next_class_transfer.py',
        'scripts/phase9/test_validate_packaged_next_class_transfer.py'
    )
    $buildMap=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    $currentMap=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($row in $BuildRows){if($buildMap.ContainsKey([string]$row.path)){throw "Duplicate build-source inventory path $($row.path)."};$buildMap.Add([string]$row.path,$row)}
    foreach($row in $CurrentRows){if($currentMap.ContainsKey([string]$row.path)){throw "Duplicate current-source inventory path $($row.path)."};$currentMap.Add([string]$row.path,$row)}
    $allPaths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($path in $buildMap.Keys){[void]$allPaths.Add($path)};foreach($path in $currentMap.Keys){[void]$allPaths.Add($path)}
    $changes=[Collections.Generic.List[object]]::new();$buildProduct=[Collections.Generic.List[object]]::new();$currentProduct=[Collections.Generic.List[object]]::new()
    foreach($path in $allPaths){
        if(-not $buildMap.ContainsKey($path) -or -not $currentMap.ContainsKey($path)){throw "Script-only derivation does not permit added or removed source path $path."}
        $before=$buildMap[$path];$after=$currentMap[$path]
        $changed=([Int64]$before.sizeBytes -ne [Int64]$after.sizeBytes -or ([string]$before.sha256).ToLowerInvariant() -cne ([string]$after.sha256).ToLowerInvariant())
        if($changed){
            if($allowed -cnotcontains $path){throw "Build-to-run source change is outside the authorized runner/validator/test scripts: $path."}
            $changes.Add([ordered]@{path=$path;buildSizeBytes=[Int64]$before.sizeBytes;buildSha256=([string]$before.sha256).ToLowerInvariant();currentSizeBytes=[Int64]$after.sizeBytes;currentSha256=([string]$after.sha256).ToLowerInvariant()})
        } else { $buildProduct.Add($before);$currentProduct.Add($after) }
    }
    if($changes.Count -eq 0){throw 'Source inventories differ but no allowed script-only changes were found.'}
    $buildProductSha=Get-InventoryFingerprint @($buildProduct.ToArray());$currentProductSha=Get-InventoryFingerprint @($currentProduct.ToArray())
    if($buildProductSha -cne $currentProductSha){throw 'Product/compiler/resource source inventory changed after the recorded Release build.'}
    $changes.Sort([System.Comparison[object]]{param($a,$b)[StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(),([string]$b.path).ToLowerInvariant())})
    [ordered]@{allowedPaths=$allowed;changedPaths=@($changes.ToArray());productSourceSha256=$buildProductSha}
}
function New-ScriptOnlyBuildDerivation {
    param([object]$Receipt,[string]$ReceiptPath,[object]$CurrentSource)
    if([string]$Receipt.sourceSha256 -ceq [string]$CurrentSource.sha256){throw 'Script-only derived receipt is unnecessary for an identical source fingerprint.'}
    foreach($value in @($PriorPackageDirectory,$PriorPackageInventoryPath,$PriorRunManifestPath)){if([string]::IsNullOrWhiteSpace($value)){throw 'A source-mismatched Release receipt requires prior package, package inventory, and run manifest paths.'}}
    $receiptHash=Get-FileSha256 $ReceiptPath
    $buildInventoryPath=[IO.Path]::GetFullPath([string]$Receipt.sourceInventoryPath)
    if(-not (Test-Path -LiteralPath $buildInventoryPath -PathType Leaf)){throw "Recorded Release source inventory is missing: $buildInventoryPath"}
    $buildInventoryDoc=Get-Content -LiteralPath $buildInventoryPath -Raw | ConvertFrom-Json -ErrorAction Stop
    $buildRows=@($buildInventoryDoc.files)
    if((Get-InventoryFingerprint $buildRows) -cne [string]$Receipt.sourceSha256){throw 'Original Release source inventory does not match its receipt fingerprint.'}
    $sourceDelta=Compare-ScriptOnlySourceDelta $buildRows @($CurrentSource.files)

    $priorPackageRoot=[IO.Path]::GetFullPath($PriorPackageDirectory)
    $priorPackageInventoryPathFull=[IO.Path]::GetFullPath($PriorPackageInventoryPath)
    $priorManifestPathFull=[IO.Path]::GetFullPath($PriorRunManifestPath)
    $priorRunRoot=[IO.Path]::GetDirectoryName($priorManifestPathFull)
    $priorPackageInventoryDoc=Get-Content -LiteralPath $priorPackageInventoryPathFull -Raw | ConvertFrom-Json -ErrorAction Stop
    $priorManifest=Get-Content -LiteralPath $priorManifestPathFull -Raw | ConvertFrom-Json -ErrorAction Stop
    if(-not [string]::Equals([IO.Path]::GetFullPath([string]$priorPackageInventoryDoc.root),$priorPackageRoot,[StringComparison]::OrdinalIgnoreCase) -or -not [string]::Equals([IO.Path]::GetFullPath([string]$priorManifest.package.directory),$priorPackageRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Preserved package root does not match both prior package records.'}
    if([string]$priorManifest.source.sha256 -cne [string]$Receipt.sourceSha256 -or [string]$priorManifest.package.candidateSourceSha256 -cne [string]$Receipt.sourceSha256){throw 'Preserved package was not produced from the recorded build source fingerprint.'}
    if([string]$priorManifest.releaseBuildReceipt.sha256 -cne $receiptHash -or -not [string]::Equals([IO.Path]::GetFullPath([string]$priorManifest.releaseBuildReceipt.path),[IO.Path]::GetFullPath($ReceiptPath),[StringComparison]::OrdinalIgnoreCase)){throw 'Preserved package run is not linked to the exact original Release receipt.'}
    if([string]$Receipt.status -cne 'succeeded' -or [string]$Receipt.taskId -cne $TaskId -or [string]$Receipt.configuration -cne 'Release' -or [string]$Receipt.platform -cne 'x64' -or [int]$Receipt.configure.exitCode -ne 0 -or [int]$Receipt.build.exitCode -ne 0){throw 'Original Release receipt lacks successful current-source x64 Release build identity.'}
    if([IO.Path]::GetFullPath([string]$Receipt.buildDirectory) -ine $BuildDirectory -or [IO.Path]::GetFullPath([string]$Receipt.cachePath) -ine (Join-Path $BuildDirectory 'CMakeCache.txt')){throw 'Original Release build/configuration paths differ from the standard x64 Release tree.'}
    $cacheHash=Get-FileSha256 ([string]$Receipt.cachePath)
    if($cacheHash -cne [string]$Receipt.cacheSha256After){throw 'Current standard Release CMake cache differs from the original build receipt.'}
    if((Get-FileSha256 (Join-Path $RepositoryRoot $SeedSqlRelative)) -cne [string]$priorManifest.masterWorkspace.seedSqlSha256){throw 'Preserved package workspace seed differs from the current large_startup.sql fixture.'}
    $seedRow=@($CurrentSource.files | Where-Object { $_.path -ceq $SeedSqlRelative.Replace('\','/') })
    if($seedRow.Count -ne 1 -or [string]$seedRow[0].sha256 -cne (Get-FileSha256 (Join-Path $RepositoryRoot $SeedSqlRelative))){throw 'Current source inventory does not bind the large_startup.sql workspace input.'}

    $priorRunSourcePath=Join-Path $priorRunRoot 'source-files.json'
    if(-not (Test-Path -LiteralPath $priorRunSourcePath -PathType Leaf)){throw 'Preserved package run lacks its source inventory.'}
    $priorRunSourceDoc=Get-Content -LiteralPath $priorRunSourcePath -Raw | ConvertFrom-Json -ErrorAction Stop
    if((Get-InventoryFingerprint @($priorRunSourceDoc.files)) -cne [string]$Receipt.sourceSha256){throw 'Preserved package run source inventory differs from the original Release source fingerprint.'}

    $priorPackageRows=@($priorPackageInventoryDoc.files)
    if((Get-InventoryFingerprint $priorPackageRows) -cne [string]$priorPackageInventoryDoc.sha256 -or [int]$priorPackageInventoryDoc.fileCount -ne $priorPackageRows.Count -or [string]$priorManifest.package.sha256 -cne [string]$priorPackageInventoryDoc.sha256){throw 'Preserved package inventory does not match the original package manifest.'}
    $expectedPackagePaths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $packageRowsByPath=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($row in $priorPackageRows){
        $relative=[string]$row.path;$candidate=[IO.Path]::GetFullPath((Join-Path $priorPackageRoot ($relative.Replace('/',[IO.Path]::DirectorySeparatorChar))))
        $base=$priorPackageRoot.TrimEnd([char[]]@('\','/'))+[IO.Path]::DirectorySeparatorChar
        if(-not $candidate.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw "Prior package inventory path escaped its root: $relative"}
        if(-not (Test-Path -LiteralPath $candidate -PathType Leaf)){throw "Preserved package file is missing: $relative"}
        $item=Get-Item -LiteralPath $candidate
        if([Int64]$item.Length -ne [Int64]$row.sizeBytes -or (Get-FileSha256 $candidate) -cne ([string]$row.sha256).ToLowerInvariant()){throw "Preserved package file differs from its inventory: $relative"}
        [void]$expectedPackagePaths.Add($relative);$packageRowsByPath.Add($relative,$row)
    }
    $priorRoute=@($priorManifest.routes | Where-Object { $_.routeId -ceq 'route-1-next-export' })
    if($priorRoute.Count -ne 1 -or [string]$priorRoute[0].operation.kind -cne 'next-ui-export' -or [string]$priorRoute[0].operation.outcome -cne 'completed' -or [int]$priorRoute[0].operation.metrics.exports_completed -ne 1 -or [int]$priorRoute[0].operation.metrics.export_selected_class_count -ne 48){throw 'Preserved route package lacks completed authentic Next export provenance.'}
    $extraExportPath=[IO.Path]::GetFullPath([string]$priorRoute[0].operation.outputPath)
    if(-not [string]::Equals([IO.Path]::GetDirectoryName($extraExportPath),$priorPackageRoot,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($extraExportPath) -cne 'next-exported-classes.json' -or -not (Test-Path -LiteralPath $extraExportPath -PathType Leaf)){throw 'Preserved package has an unexpected non-inventory file.'}
    if((Get-FileSha256 $extraExportPath) -cne [string]$priorRoute[0].operation.packageSha256 -or [Int64](Get-Item -LiteralPath $extraExportPath).Length -ne [Int64]$priorRoute[0].operation.packageSizeBytes){throw 'Preserved package extra file is not the recorded Next export output.'}
    $actualFiles=@(Get-ChildItem -LiteralPath $priorPackageRoot -File -Recurse)
    $actualPaths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($file in $actualFiles){$relative=$file.FullName.Substring($priorPackageRoot.TrimEnd([char[]]@('\','/')).Length+1).Replace('\','/');[void]$actualPaths.Add($relative)}
    if($actualPaths.Count -ne $expectedPackagePaths.Count+1){throw 'Preserved package has an unexpected file count beyond its base inventory and recorded Next export.'}
    foreach($path in $expectedPackagePaths){if(-not $actualPaths.Contains($path)){throw "Preserved package inventory path is absent: $path"}}
    if(-not $actualPaths.Contains('next-exported-classes.json')){throw 'Preserved package lacks its recorded Next export output.'}
    $priorAppPath=Join-Path $priorPackageRoot 'ClassMngrNext.exe'
    if(-not (Test-Path -LiteralPath $priorAppPath -PathType Leaf) -or (Get-FileSha256 $priorAppPath) -cne [string]$Receipt.releaseExecutableSha256 -or [string]$priorManifest.application.sha256 -cne [string]$Receipt.releaseExecutableSha256){throw 'Preserved packaged executable does not match the original successful Release receipt.'}
    if([string]$priorManifest.package.applicationSha256 -cne [string]$Receipt.releaseExecutableSha256){throw 'Prior package manifest application hash differs from the Release receipt.'}

    $stagedRoot=[IO.Path]::GetFullPath([string]$Receipt.generatedResources.verifiedStagingRoot)
    $stageInventory=Get-CanonicalInventory $stagedRoot
    $expectedStage=$Receipt.generatedResources.stagedResourceInputs
    if($stageInventory.sha256 -cne [string]$expectedStage.sha256 -or $stageInventory.fileCount -ne [int]$expectedStage.fileCount -or (Get-InventoryFingerprint @($stageInventory.files)) -cne (Get-InventoryFingerprint @($expectedStage.files))){throw 'Preserved staged resource inputs differ from the successful Release receipt.'}
    foreach($row in $expectedStage.files){
        if(([string]$row.path).StartsWith('resources/',[StringComparison]::OrdinalIgnoreCase)){
            if(-not $packageRowsByPath.ContainsKey([string]$row.path)){throw "Preserved package lacks staged resource $($row.path)."}
            $packageRow=$packageRowsByPath[[string]$row.path]
            if([Int64]$packageRow.sizeBytes -ne [Int64]$row.sizeBytes -or [string]$packageRow.sha256 -cne [string]$row.sha256){throw "Preserved package resource differs from staged Release resource $($row.path)."}
        }
    }
    if([string]$priorManifest.package.sha256 -cne [string]$priorPackageInventoryDoc.sha256 -or [int]$priorManifest.package.fileCount -ne [int]$priorPackageInventoryDoc.fileCount){throw 'Prior package manifest and inventory disagree.'}

    $copyBuildReceipt=Join-Path $BuildDerivationDirectory 'original-release-build-receipt.json'
    $copyBuildInventory=Join-Path $BuildDerivationDirectory 'original-release-source-files.json'
    $copyPriorRunManifest=Join-Path $BuildDerivationDirectory 'prior-package-run-manifest.json'
    $copyPriorRunSource=Join-Path $BuildDerivationDirectory 'prior-package-run-source-files.json'
    $copyPriorPackageInventory=Join-Path $BuildDerivationDirectory 'prior-package-files.json'
    Copy-Item -LiteralPath $ReceiptPath -Destination $copyBuildReceipt
    Copy-Item -LiteralPath $buildInventoryPath -Destination $copyBuildInventory
    Copy-Item -LiteralPath $priorManifestPathFull -Destination $copyPriorRunManifest
    Copy-Item -LiteralPath $priorRunSourcePath -Destination $copyPriorRunSource
    Copy-Item -LiteralPath $priorPackageInventoryPathFull -Destination $copyPriorPackageInventory
    $buildOutputPath=[IO.Path]::GetFullPath([string]$Receipt.releaseExecutablePath)
    $buildOutputHashNow=if(Test-Path -LiteralPath $buildOutputPath -PathType Leaf){Get-FileSha256 $buildOutputPath}else{''}
    $derived=[ordered]@{
        schema='classmngr-phase9-class-transfer-script-only-derived-build-receipt-v1';taskId=$TaskId;status='verified-script-only-derivation';verifiedAtUtc=Get-UtcTimestamp
        source=[ordered]@{repositoryRoot=$RepositoryRoot;currentSourceSha256=[string]$CurrentSource.sha256;buildSourceSha256=[string]$Receipt.sourceSha256;productSourceSha256=[string]$sourceDelta.productSourceSha256;allowedPaths=@($sourceDelta.allowedPaths);changedFiles=@($sourceDelta.changedPaths);buildInventoryCopy='build-derivation/original-release-source-files.json';buildInventoryCopySha256=Get-FileSha256 $copyBuildInventory;currentInventoryPath='source-files.json'}
        build=[ordered]@{receiptPath=[IO.Path]::GetFullPath($ReceiptPath);receiptSha256=$receiptHash;receiptCopy='build-derivation/original-release-build-receipt.json';receiptCopySha256=Get-FileSha256 $copyBuildReceipt;buildDirectory=[string]$Receipt.buildDirectory;generator=[string]$Receipt.generator;configuration=[string]$Receipt.configuration;platform=[string]$Receipt.platform;targets=@($Receipt.targets);configureExitCode=[int]$Receipt.configure.exitCode;buildExitCode=[int]$Receipt.build.exitCode;cachePath=[string]$Receipt.cachePath;cacheSha256=$cacheHash}
        executable=[ordered]@{preservedPackagePath=$priorAppPath;sha256=[string]$Receipt.releaseExecutableSha256;architecture='x64';recordedBuildOutputPath=$buildOutputPath;recordedBuildOutputHashAtDerivation=$buildOutputHashNow;recordedBuildOutputStillMatches=($buildOutputHashNow -ceq [string]$Receipt.releaseExecutableSha256)}
        resources=[ordered]@{stagedRoot=$stagedRoot;inventorySha256=[string]$stageInventory.sha256;fileCount=[int]$stageInventory.fileCount}
        priorPackage=[ordered]@{runManifestPath=$priorManifestPathFull;runManifestSha256=Get-FileSha256 $priorManifestPathFull;runManifestCopy='build-derivation/prior-package-run-manifest.json';runManifestCopySha256=Get-FileSha256 $copyPriorRunManifest;runSourceInventoryCopy='build-derivation/prior-package-run-source-files.json';runSourceInventoryCopySha256=Get-FileSha256 $copyPriorRunSource;packageRoot=$priorPackageRoot;inventoryPath=$priorPackageInventoryPathFull;inventorySha256=[string]$priorPackageInventoryDoc.sha256;fileCount=[int]$priorPackageInventoryDoc.fileCount;inventoryCopy='build-derivation/prior-package-files.json';inventoryCopySha256=Get-FileSha256 $copyPriorPackageInventory;recordedNextExport=[ordered]@{path=$extraExportPath;sha256=[string]$priorRoute[0].operation.packageSha256;sizeBytes=[Int64]$priorRoute[0].operation.packageSizeBytes;excludedFromBasePackageInventory=$true}}
        workspaceInput=[ordered]@{seedSqlPath=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot $SeedSqlRelative));seedSqlSha256=Get-FileSha256 (Join-Path $RepositoryRoot $SeedSqlRelative)}
    }
    Write-JsonFile $BuildDerivationPath $derived
    [ordered]@{record=$derived;path=[IO.Path]::GetFullPath($BuildDerivationPath);sha256=Get-FileSha256 $BuildDerivationPath;packageInventory=$priorPackageInventoryDoc;packageRoot=$priorPackageRoot;packageRows=$priorPackageRows;releaseExecutablePath=$priorAppPath;resourcesRoot=$stagedRoot}
}
function Get-PeArchitecture {
    param([string]$Path)
    $stream=[IO.File]::OpenRead($Path)
    try { $reader=[IO.BinaryReader]::new($stream); if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Bad MZ: $Path" }; $stream.Position=0x3c; $offset=$reader.ReadInt32(); $stream.Position=$offset; if ($reader.ReadUInt32() -ne 0x4550) { throw "Bad PE: $Path" }; switch ($reader.ReadUInt16()) { 0x8664 {"x64"} 0xaa64 {"arm64"} default {"unknown"} } }
    finally { $stream.Dispose() }
}
function Invoke-Captured {
    param([string]$Path,[string[]]$Arguments,[string]$LogPath,[string]$Description)
    $info=[Diagnostics.ProcessStartInfo]::new(); $info.FileName=[IO.Path]::GetFullPath($Path); $info.Arguments=(@($Arguments | ForEach-Object { ConvertTo-QuotedArgument ([string]$_) }) -join ' '); $info.WorkingDirectory=$RepositoryRoot; $info.UseShellExecute=$false; $info.CreateNoWindow=$true; $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
    $process=[Diagnostics.Process]::new(); $process.StartInfo=$info
    try { if (-not $process.Start()) { throw "$Description could not start" }; $outTask=$process.StandardOutput.ReadToEndAsync(); $errTask=$process.StandardError.ReadToEndAsync(); $process.WaitForExit(); $exit=[int]$process.ExitCode; $out=[string]$outTask.Result; $err=[string]$errTask.Result } finally { $process.Dispose() }
    $stdout=$LogPath+'.stdout.txt'; $stderr=$LogPath+'.stderr.txt'; [IO.File]::WriteAllText($stdout,$out,[Text.UTF8Encoding]::new($false)); [IO.File]::WriteAllText($stderr,$err,[Text.UTF8Encoding]::new($false));
    [IO.File]::WriteAllLines($LogPath,@("description: $Description","exitCode: $exit","stdoutPath: $stdout","stderrPath: $stderr"),[Text.UTF8Encoding]::new($false))
    [ordered]@{ exitCode=$exit; logPath=[IO.Path]::GetFullPath($LogPath); logSha256=Get-FileSha256 $LogPath; stdoutPath=[IO.Path]::GetFullPath($stdout); stdoutSha256=Get-FileSha256 $stdout; stderrPath=[IO.Path]::GetFullPath($stderr); stderrSha256=Get-FileSha256 $stderr }
}
function ConvertTo-QuotedArgument { param([AllowEmptyString()][string]$Value) '"'+$Value.Replace('"','\"')+'"' }
function Invoke-PythonJson {
    param([string]$Script,[string[]]$Arguments,[string]$LogPath,[string]$Description)
    $output=@(& python $Script @Arguments 2>&1 | ForEach-Object { [string]$_ }); $exit=$LASTEXITCODE; [IO.File]::WriteAllLines($LogPath,@("exitCode: $exit",$output),[Text.UTF8Encoding]::new($false))
    if ($exit -ne 0) { throw "$Description failed with exit code $exit." }
    $line=@($output | Where-Object { $_.TrimStart().StartsWith('{') } | Select-Object -Last 1)
    if ($line.Count -ne 1) { throw "$Description emitted no JSON result." }
    $line[0] | ConvertFrom-Json -ErrorAction Stop
}
function Add-ProcessOutput {
    param([object]$Context)
    foreach ($reader in $Context.readers) {
        $line=$null
        while ($reader.queue.TryDequeue([ref]$line)) {
            $entry=[ordered]@{ routeId=$Context.routeId; processInstanceId=$Context.processInstanceId; pid=[Int64]$Context.process.Id; stream=$reader.stream; observedAtUtc=Get-UtcTimestamp; line=[string]$line }
            Add-JsonLine $Context.stdoutIndexPath $entry; [IO.File]::AppendAllText($reader.path,[string]$line+[Environment]::NewLine,[Text.UTF8Encoding]::new($false)); $Context.logEntries.Add($entry)
            $line=$null
        }
        if ($reader.pumpTask.IsCompleted) { $reader.closed=$true }
    }
}
function Wait-ForCondition {
    param([object]$Context,[scriptblock]$Predicate,[string]$Description,[int]$TimeoutSeconds=20)
    $deadline=[DateTimeOffset]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTimeOffset]::UtcNow -lt $deadline) { Add-ProcessOutput $Context; $Context.process.Refresh(); if ($Context.process.HasExited) { throw "$($Context.routeId) exited while waiting for $Description ($($Context.process.ExitCode))." }; $value=& $Predicate $Context; if ($value) { return $value }; Start-Sleep -Milliseconds 100 }
    throw "$($Context.routeId) timed out waiting for $Description."
}
function Get-StartupCheckpoint {
    param([object]$Context,[string]$Stage)
    Add-ProcessOutput $Context
    $pattern="^ClassMngrNext startup stage=$([regex]::Escape($Stage)) outcome=(?<outcome>[a-z-]+) diagnostic=(?<diagnostic>[a-z-]+) diagnostic_stage=(?<diagnosticStage>[a-z-]+) checkpoint=(?<checkpoint>\{.*\})$"
    $rows=@($Context.logEntries | Where-Object { [string]$_.line -match $pattern })
    if ($rows.Count -ne 1) { throw "$($Context.routeId) expected one $Stage startup checkpoint; found $($rows.Count)." }
    $match=[regex]::Match([string]$rows[0].line,$pattern); $checkpoint=$match.Groups['checkpoint'].Value | ConvertFrom-Json -ErrorAction Stop
    if ($match.Groups['outcome'].Value -cne 'completed' -or $match.Groups['diagnostic'].Value -cne 'none' -or $checkpoint.platform -cne 'windows' -or $checkpoint.sample_available -ne $true) { throw "$($Context.routeId) $Stage checkpoint is invalid." }
    [ordered]@{ observedAtUtc=[string]$rows[0].observedAtUtc; record=$checkpoint; rawLine=[string]$rows[0].line }
}
function New-ProcessSample {
    param([object]$Context,[string]$Point,[int]$Index=0)
    $p=$Context.process; $p.Refresh(); if ($p.HasExited) { throw "$($Context.routeId) exited before $Point." }; $m=[P911Native]::GetMemoryCounters($p.Handle)
    $sample=[ordered]@{ routeId=$Context.routeId; processInstanceId=$Context.processInstanceId; point=$Point; sampleIndex=$Index; timestampUtc=Get-UtcTimestamp; pid=[Int64]$p.Id; processCreatedAtUtc=$Context.createdAtUtc; canonicalExecutablePath=$Context.executablePath; architecture='x64'; sha256=$Context.executableSha256; currentWorkingSetBytes=[Int64]$m.workingSetBytes; privateBytes=[Int64]$m.privateBytes; commitBytes=[Int64]$m.commitBytes; peakWorkingSetBytes=[Int64]$m.peakWorkingSetBytes; peakCommitBytes=[Int64]$m.peakCommitBytes; handles=[Int64]$p.HandleCount; threads=[Int64]$p.Threads.Count }
    Add-JsonLine $ProcessMetricsPath $sample; return $sample
}
function Assert-StrictWorkingSet { param([object]$Sample,[string]$Label) if ([Int64]$Sample.currentWorkingSetBytes -ge $WorkingSetLimit) { throw "$Label working set $($Sample.currentWorkingSetBytes) is not strictly below $WorkingSetLimit." } }
function Get-AppTopWindows { param([int]$ProcessId) $root=[System.Windows.Automation.AutomationElement]::RootElement; @($root.FindAll([System.Windows.Automation.TreeScope]::Children,[System.Windows.Automation.Condition]::TrueCondition) | Where-Object { $_.Current.ProcessId -eq $ProcessId -and $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window }) }
function Get-AppElements {
    param([object]$Context)
    $elements=[Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach ($window in @(Get-AppTopWindows $Context.process.Id)) { $elements.Add($window); foreach ($element in @($window.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition))) { $elements.Add($element) } }
    @($elements.ToArray())
}
function Get-MainWindow {
    param([object]$Context)
    $Context.process.Refresh(); $handle=$Context.process.MainWindowHandle
    if ($handle -eq [IntPtr]::Zero -or -not [P911Native]::IsWindowVisibleForProcess($handle) -or [P911Native]::GetWindowProcessId($handle) -ne [uint32]$Context.process.Id) { throw "$($Context.routeId) has no visible PID-owned main window." }
    $window=[System.Windows.Automation.AutomationElement]::FromHandle($handle); if ($null -eq $window -or $window.Current.ProcessId -ne $Context.process.Id) { throw "$($Context.routeId) main window UIA PID mismatch." }; return $window
}
function Get-UiaElementById {
    param([object]$Context,[string]$AutomationId,[string]$ExpectedType='')
    $matches=@(Get-AppElements $Context | Where-Object { $id=[string]$_.Current.AutomationId; ($id -ceq $AutomationId -or $id.EndsWith('.'+$AutomationId,[StringComparison]::Ordinal)) -and ($ExpectedType -eq '' -or $_.Current.ControlType.ProgrammaticName -ceq $ExpectedType) -and $_.Current.ProcessId -eq $Context.process.Id })
    if ($matches.Count -ne 1) { $null=Save-UiaTree $Context ("uia/"+$Context.routeId+"-"+$AutomationId+"-failure.json"); throw "UIA expected one '$AutomationId' ($ExpectedType); found $($matches.Count)." }; return $matches[0]
}
function Get-UiaByName {
    param([object]$Context,[string]$Name,[string]$ExpectedType)
    $matches=@(Get-AppElements $Context | Where-Object { $_.Current.ProcessId -eq $Context.process.Id -and $_.Current.Name -ceq $Name -and $_.Current.ControlType.ProgrammaticName -ceq $ExpectedType })
    if ($matches.Count -ne 1) { $null=Save-UiaTree $Context ("uia/"+$Context.routeId+"-name-failure.json"); throw "UIA expected one '$Name' ($ExpectedType); found $($matches.Count)." }; return $matches[0]
}
function Get-VisibleUiaElementById {
    param([object]$Context,[string]$AutomationId,[string]$ExpectedType)
    $matches=@(Get-AppElements $Context | Where-Object {
        $id=[string]$_.Current.AutomationId
        ($id -ceq $AutomationId -or $id.EndsWith('.'+$AutomationId,[StringComparison]::Ordinal)) -and
        $_.Current.ControlType.ProgrammaticName -ceq $ExpectedType -and
        $_.Current.ProcessId -eq $Context.process.Id -and -not $_.Current.IsOffscreen
    })
    if ($matches.Count -ne 1) { $null=Save-UiaTree $Context ("uia/"+$Context.routeId+"-visible-"+$AutomationId+"-failure.json"); throw "UIA expected one visible '$AutomationId' ($ExpectedType); found $($matches.Count)." }
    return $matches[0]
}
function Get-UiaSnapshot {
    [CmdletBinding()]
    param([Parameter(Mandatory=$true,ValueFromPipeline=$true)][System.Windows.Automation.AutomationElement]$Element)
    process {
    $c=$Element.Current; $r=$c.BoundingRectangle; $value=$null; $supportedPatterns=@()
    try { $supportedPatterns=@($Element.GetSupportedPatterns() | ForEach-Object { [string]$_.ProgrammaticName }) } catch { }
    try { $value=([System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)).Current.Value } catch { }
    [ordered]@{ automationId=[string]$c.AutomationId; name=[string]$c.Name; helpText=[string]$c.HelpText; className=[string]$c.ClassName; controlType=[string]$c.ControlType.ProgrammaticName; processId=[Int64]$c.ProcessId; hwnd=[Int64]$c.NativeWindowHandle; enabled=[bool]$c.IsEnabled; offscreen=[bool]$c.IsOffscreen; value=$value; supportedPatterns=$supportedPatterns; rectangle=[ordered]@{x=[double]$r.X;y=[double]$r.Y;width=[double]$r.Width;height=[double]$r.Height} }
    }
}
function Save-UiaTree {
    param([object]$Context,[string]$RelativePath)
    $rows=[Collections.Generic.List[object]]::new(); foreach ($element in @(Get-AppElements $Context)) { try { $rows.Add((Get-UiaSnapshot $element)) } catch { $rows.Add([ordered]@{snapshotError=$_.Exception.Message}) } }
    $path=Join-Path $EvidenceDirectory $RelativePath; Write-JsonFile $path ([ordered]@{routeId=$Context.routeId; processInstanceId=$Context.processInstanceId; processId=[Int64]$Context.process.Id; mainWindowHandle=[Int64]$Context.mainWindowHandle; capturedAtUtc=Get-UtcTimestamp; elements=@($rows.ToArray())}); [ordered]@{path=[IO.Path]::GetFullPath($path);sha256=Get-FileSha256 $path;elementCount=$rows.Count}
}
function Get-ElementWindowHandle {
    param([object]$Context,[System.Windows.Automation.AutomationElement]$Element)
    $walker=[System.Windows.Automation.TreeWalker]::ControlViewWalker; $current=$Element
    for ($depth=0;$depth -lt 48 -and $null -ne $current;$depth++) { try { if ($current.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window) { $candidate=[IntPtr]$current.Current.NativeWindowHandle; if ($candidate -ne [IntPtr]::Zero -and [P911Native]::GetWindowProcessId($candidate) -eq [uint32]$Context.process.Id) { return $candidate } }; $current=$walker.GetParent($current) } catch { break } }
    $candidate=[IntPtr]$Element.Current.NativeWindowHandle; if ($candidate -ne [IntPtr]::Zero -and [P911Native]::GetWindowProcessId($candidate) -eq [uint32]$Context.process.Id) { return $candidate }; return [IntPtr]$Context.mainWindowHandle
}
function Invoke-VerifiedUiaClick {
    param([object]$Context,[System.Windows.Automation.AutomationElement]$Element,[string]$Action,[string]$FromPage,[string]$ToPage)
    $identity=Get-UiaSnapshot $Element
    if ([Int64]$identity.processId -ne [Int64]$Context.process.Id -or $identity.offscreen -or $identity.rectangle.width -le 0 -or $identity.rectangle.height -le 0) { throw "UIA target identity or visibility invalid for $Action." }
    $hwnd=Get-ElementWindowHandle $Context $Element
    if ($hwnd -eq [IntPtr]::Zero -or [P911Native]::GetWindowProcessId($hwnd) -ne [uint32]$Context.process.Id) { throw "UIA target HWND identity invalid for $Action." }
    $method=''; $patternName=''; $patternErrors=[Collections.Generic.List[string]]::new(); $activation=$null
    try {
        if ($Element.Current.ControlType -eq [System.Windows.Automation.ControlType]::TreeItem) {
            $selection=[System.Windows.Automation.SelectionItemPattern]$Element.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)
            $selection.Select()
            if (-not $selection.Current.IsSelected) { throw 'SelectionItemPattern did not select the requested TreeItem.' }
            $invoke=[System.Windows.Automation.InvokePattern]$Element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
            $invoke.Invoke()
            $x=[int][Math]::Round($identity.rectangle.x+$identity.rectangle.width/2); $y=[int][Math]::Round($identity.rectangle.y+$identity.rectangle.height/2)
            $activation=[P911Native]::PostVerifiedWindowClick($hwnd,$x,$y,[uint32]$Context.process.Id)
            $method='uia-patterns-plus-pid-bound-window-input'; $patternName='SelectionItemPattern.Select; InvokePattern.Invoke; PID-verified WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP'
        } else {
            $invoke=[System.Windows.Automation.InvokePattern]$Element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
            $invoke.Invoke(); $method='uia-invoke-pattern'; $patternName='InvokePattern.Invoke'
        }
    } catch { $patternErrors.Add($_.Exception.Message) }
    $patternError=if ($patternErrors.Count) { $patternErrors -join ' | ' } else { $null }
    if ($method -eq '') {
        try {
            $x=[int][Math]::Round($identity.rectangle.x+$identity.rectangle.width/2); $y=[int][Math]::Round($identity.rectangle.y+$identity.rectangle.height/2)
            $activation=[P911Native]::ClickWindowPoint($hwnd,$Context.process.Handle,$x,$y,[uint32]$Context.process.Id)
            $method='uia-bound-verified-input'
        } catch {
            $failedRecord=[ordered]@{routeId=$Context.routeId;processInstanceId=$Context.processInstanceId;actionKind=$Action;method='uia-pattern-and-input-failed';target=$identity;windowHandle=[Int64]$hwnd.ToInt64();windowProcessId=[Int64][P911Native]::GetWindowProcessId($hwnd);mainWindowHandle=[Int64]$Context.mainWindowHandle;mainWindowProcessId=[Int64]$Context.process.Id;fromPageId=$FromPage;toPageId=$ToPage;patternName=$patternName;patternError=$patternError;inputError=$_.Exception.Message;outcome='failed';issuedAtUtc=Get-UtcTimestamp}
            Add-JsonLine $UiaActionsPath $failedRecord
            throw
        }
    }
    $record=[ordered]@{routeId=$Context.routeId; processInstanceId=$Context.processInstanceId; actionKind=$Action; method=$method; target=$identity; windowHandle=[Int64]$hwnd.ToInt64(); windowProcessId=[Int64][P911Native]::GetWindowProcessId($hwnd); mainWindowHandle=[Int64]$Context.mainWindowHandle; mainWindowProcessId=[Int64]$Context.process.Id; fromPageId=$FromPage; toPageId=$ToPage; patternName=$patternName; patternError=$patternError; nativeActivation=$activation; outcome='issued'; issuedAtUtc=Get-UtcTimestamp}
    Add-JsonLine $UiaActionsPath $record; return $record
}
function Wait-ForUiaElement {
    param([object]$Context,[string]$AutomationId,[string]$ExpectedType='',[int]$TimeoutSeconds=20)
    Wait-ForCondition $Context { param($ctx) $found=@(Get-AppElements $ctx | Where-Object { $id=[string]$_.Current.AutomationId; ($id -ceq $AutomationId -or $id.EndsWith('.'+$AutomationId,[StringComparison]::Ordinal)) -and ($ExpectedType -eq '' -or $_.Current.ControlType.ProgrammaticName -ceq $ExpectedType) }); if ($found.Count -eq 1) { return $found[0] }; return $null } $AutomationId $TimeoutSeconds
}
function Get-DiagnosticRecords {
    param([object]$Context)
    Add-ProcessOutput $Context
    if (-not (Test-Path -LiteralPath $Context.diagnosticPath -PathType Leaf)) { return @() }
    @(Get-Content -LiteralPath $Context.diagnosticPath -Encoding UTF8 | Where-Object { $_.Trim() -ne '' } | ForEach-Object { $_ | ConvertFrom-Json -ErrorAction Stop })
}
function Wait-DiagnosticCheckpoint {
    param([object]$Context,[string]$Checkpoint,[int]$Count=1,[int]$TimeoutSeconds=30)
    Wait-ForCondition $Context { param($ctx) $records=@(Get-DiagnosticRecords $ctx | Where-Object { $_.checkpoint -ceq $Checkpoint -and $_.event -ceq 'class-transfer-checkpoint' }); if ($records.Count -ge $Count) { return $records[-1] }; return $null } "class transfer checkpoint $Checkpoint ($Count)" $TimeoutSeconds
}
function Capture-CheckpointTimeout {
    param([object]$Context,[string]$Checkpoint,[int]$TimeoutSeconds,[System.Collections.IDictionary]$Route)
    Add-ProcessOutput $Context
    $records=@(Get-DiagnosticRecords $Context)
    $windows=[Collections.Generic.List[object]]::new()
    foreach ($window in @(Get-AppTopWindows $Context.process.Id)) {
        try { $windows.Add((Get-UiaSnapshot $window)) }
        catch { $windows.Add([ordered]@{snapshotError=$_.Exception.Message}) }
    }
    $controls=[ordered]@{}
    foreach ($spec in @(
        @{id='classTransferStatus';type='ControlType.Text'},
        @{id='classTransferReviewSummary';type='ControlType.Text'},
        @{id='classTransferApply';type='ControlType.Button'},
        @{id='classTransferCancelReview';type='ControlType.Button'}
    )) {
        try { $controls[$spec.id]=Get-UiaSnapshot (Get-UiaElementById $Context $spec.id $spec.type) }
        catch { $controls[$spec.id]=[ordered]@{captureError=$_.Exception.Message} }
    }
    $tree=Save-UiaTree $Context ("uia/"+$Context.routeId+"-"+$Checkpoint+"-timeout.json")
    $sample=$null
    try { $sample=New-ProcessSample $Context ($Checkpoint+'-timeout') }
    catch { $sample=[ordered]@{captureError=$_.Exception.Message;capturedAtUtc=Get-UtcTimestamp;processId=[Int64]$Context.process.Id} }
    $Context.process.Refresh()
    $document=[ordered]@{
        schema='classmngr-phase9-class-transfer-operation-timeout-v1'
        routeId=$Context.routeId
        processInstanceId=$Context.processInstanceId
        processId=[Int64]$Context.process.Id
        capturedAtUtc=Get-UtcTimestamp
        requestedCheckpoint=$Checkpoint
        timeoutSeconds=$TimeoutSeconds
        observedTerminalCheckpointCount=[Int64]@($records | Where-Object { $_.checkpoint -ceq $Checkpoint -and $_.event -ceq 'class-transfer-checkpoint' }).Count
        latestServiceCheckpoint=if ($records.Count -gt 0) { $records[-1] } else { $null }
        recentServiceCheckpoints=@($records | Select-Object -Last 5)
        visibleTopLevelWindows=@($windows.ToArray())
        visibleControlSnapshots=$controls
        uiaTree=$tree
        process=[ordered]@{hasExited=[bool]$Context.process.HasExited;exitCode=if($Context.process.HasExited){[Int64]$Context.process.ExitCode}else{$null};mainWindowHandle=[Int64]$Context.process.MainWindowHandle.ToInt64();mainWindowTitle=[string]$Context.process.MainWindowTitle}
        memorySample=$sample
        recentProcessOutput=@($Context.logEntries | Select-Object -Last 30 | ForEach-Object { [ordered]@{stream=$_.stream;line=$_.line} })
    }
    $directory=Join-Path $EvidenceDirectory 'diagnostics'
    [void][IO.Directory]::CreateDirectory($directory)
    $path=Join-Path $directory ($Context.routeId+'-'+$Checkpoint+'-timeout.json')
    Write-JsonFile $path $document
    [ordered]@{path=[IO.Path]::GetFullPath($path);sha256=Get-FileSha256 $path;uiaTree=$tree}
}
function Get-FilePicker {
    param([object]$Context,[int]$TimeoutSeconds=20)
    Wait-ForCondition $Context { param($ctx)
        $windows=@(Get-AppElements $ctx | Where-Object { $_.Current.ProcessId -eq $ctx.process.Id -and $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window -and -not $_.Current.IsOffscreen -and ($_.Current.ClassName -ceq 'QFileDialog' -or [string]$_.Current.AutomationId -match '(?i)FileDialog') })
        $matches=[Collections.Generic.List[object]]::new()
        foreach ($window in $windows) { $desc=@($window.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition)); $edits=@($desc | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Edit }); if ($edits.Count -gt 0) { $matches.Add([pscustomobject]@{window=$window; hwnd=[IntPtr]$window.Current.NativeWindowHandle; descendants=$desc; edits=$edits}) } }
        if ($matches.Count -eq 1) { return $matches[0] }; return $null
    } 'PID-owned QFileDialog' $TimeoutSeconds
}
function Select-PackageThroughFilePicker {
    param([object]$Context,[string]$Path,[ValidateSet('Open','Save')][string]$Action)
    $picker=Get-FilePicker $Context 20; $hwnd=$picker.hwnd
    if ($hwnd -eq [IntPtr]::Zero -or [P911Native]::GetWindowProcessId($hwnd) -ne [uint32]$Context.process.Id) { throw 'Class Transfer file picker HWND is not owned by the measured process.' }
    $editCandidates=@($picker.edits | Where-Object { ([string]$_.Current.AutomationId+' '+[string]$_.Current.Name) -match '(?i)file.?name|filename' }); $edit=if ($editCandidates.Count -eq 1) {$editCandidates[0]} elseif ($picker.edits.Count -eq 1) {$picker.edits[0]} else {$null}
    if ($null -eq $edit) { Save-UiaTree $Context ("uia/"+$Context.routeId+"-file-picker-failure.json"); throw 'File picker filename edit control is ambiguous.' }
    $value=[System.Windows.Automation.ValuePattern]$edit.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern); $expected=[IO.Path]::GetFullPath($Path); $value.SetValue($expected); $observed=[string]$value.Current.Value
    if ([IO.Path]::GetFullPath($observed) -ine $expected) { throw 'File picker did not retain the exact requested path.' }
    $namePattern=if ($Action -eq 'Save') { '(?i)(Save|&Save)' } else { '(?i)(Open|&Open|Select|Choose)' }
    $buttons=@($picker.descendants | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Button -and $_.Current.Name -match $namePattern })
    if ($buttons.Count -ne 1) { Save-UiaTree $Context ("uia/"+$Context.routeId+"-file-picker-buttons.json"); throw "File picker expected one $Action button; found $($buttons.Count)." }
    $buttonIdentity=Get-UiaSnapshot $buttons[0]; if ([Int64]$buttonIdentity.processId -ne [Int64]$Context.process.Id) { throw 'File picker action belongs to a different process.' }
    $actionRecord=Invoke-VerifiedUiaClick $Context $buttons[0] ("file-picker-"+$Action.ToLowerInvariant()) 'file_picker' 'class_transfer'
    Add-JsonLine $UiaActionsPath ([ordered]@{routeId=$Context.routeId; actionKind='verify-file-picker-path'; method='uia-value-pattern-readback'; expectedPath=$expected; observedPath=$observed; pickerWindowHandle=[Int64]$hwnd.ToInt64(); pickerProcessId=[Int64]$Context.process.Id; action=$actionRecord; capturedAtUtc=Get-UtcTimestamp})
    return [ordered]@{path=$expected; valueControl=(Get-UiaSnapshot $edit); button=(Get-UiaSnapshot $buttons[0]); windowHandle=[Int64]$hwnd.ToInt64(); windowProcessId=[Int64][P911Native]::GetWindowProcessId($hwnd); action=$actionRecord}
}
function Invoke-WorkspaceSnapshot {
    param([string]$Workspace,[string]$Output,[string]$RouteId,[string]$Suffix)
    Invoke-PythonJson (Join-Path $PSScriptRoot 'snapshot_class_transfer_workspace.py') @('--workspace',$Workspace,'--output',$Output) (Join-Path (Join-Path $EvidenceDirectory 'logs') ($RouteId+'-'+$Suffix+'-snapshot.log')) "$RouteId $Suffix workspace snapshot"
}
function Get-CloseWindowUiState {
    param([object]$Context,[string]$Point)
    $Context.process.Refresh(); $windows=[Collections.Generic.List[object]]::new(); $captureErrors=[Collections.Generic.List[string]]::new(); $tree=$null
    try {
        foreach ($window in @(Get-AppTopWindows $Context.process.Id)) {
            try { $windows.Add((Get-UiaSnapshot $window)) }
            catch { $captureErrors.Add("window snapshot: $($_.Exception.Message)") }
        }
    } catch { $captureErrors.Add("top-level UIA enumeration: $($_.Exception.Message)") }
    try { $tree=Save-UiaTree $Context ("uia/"+$Context.routeId+"-close-"+$Point+"-uia.json") }
    catch { $captureErrors.Add("full UIA tree: $($_.Exception.Message)") }
    $p=$Context.process; $p.Refresh()
    [ordered]@{
        point=$Point;capturedAtUtc=Get-UtcTimestamp;processId=[Int64]$p.Id;hasExited=[bool]$p.HasExited;exitCode=if($p.HasExited){[Int64]$p.ExitCode}else{$null}
        mainWindowHandle=[Int64]$p.MainWindowHandle.ToInt64();mainWindowTitle=[string]$p.MainWindowTitle;visibleTopLevelWindows=@($windows.ToArray());uiaTree=$tree;captureErrors=@($captureErrors.ToArray())
    }
}
function Get-CloseProcessSample {
    param([object]$Context,[string]$Point,[int]$Index)
    $p=$Context.process; $p.Refresh(); $memory=[P911Native]::GetMemoryCounters($p.Handle)
    [ordered]@{point=$Point;sampleIndex=$Index;capturedAtUtc=Get-UtcTimestamp;processId=[Int64]$p.Id;hasExited=[bool]$p.HasExited;exitCode=if($p.HasExited){[Int64]$p.ExitCode}else{$null};cpuTimeMilliseconds=[Int64]$p.TotalProcessorTime.TotalMilliseconds;workingSetBytes=[Int64]$memory.workingSetBytes;privateBytes=[Int64]$memory.privateBytes;peakWorkingSetBytes=[Int64]$memory.peakWorkingSetBytes;handles=[Int64]$p.HandleCount;threads=[Int64]$p.Threads.Count;mainWindowHandle=[Int64]$p.MainWindowHandle.ToInt64();mainWindowTitle=[string]$p.MainWindowTitle}
}
function Capture-Route2ShutdownMiniDump {
    param([object]$Context,[double]$ElapsedSeconds)
    $path=[IO.Path]::GetFullPath([string]$Context.shutdownDumpPath); $timer=[Diagnostics.Stopwatch]::StartNew()
    $result=[ordered]@{schema='classmngr-phase9-class-transfer-shutdown-minidump-v1';status='capture-started';processId=[Int64]$Context.process.Id;triggerElapsedSeconds=$ElapsedSeconds;startedAtUtc=Get-UtcTimestamp;completedAtUtc=$null;captureElapsedMilliseconds=$null;path=$path;dumpKind='MiniDumpWithThreadInfo';sizeBytes=$null;sha256=$null;signature=$null;errorType=$null;errorMessage=$null;nativeErrorCode=$null}
    try {
        $Context.process.Refresh()
        if ($Context.process.HasExited) { $result.status='process-exited-before-capture'; return $result }
        if (Test-Path -LiteralPath $path) { throw "Refusing to overwrite shutdown dump artifact: $path" }
        $writtenBytes=[P911DumpNative]::WriteThreadInfoMiniDump($Context.process.Handle,[int]$Context.process.Id,$path)
        $file=Get-Item -LiteralPath $path
        if ([Int64]$file.Length -le 4 -or [Int64]$file.Length -ne [Int64]$writtenBytes) { throw "MiniDumpWriteDump produced an empty or inconsistent file (API bytes=$writtenBytes, file bytes=$($file.Length))." }
        $stream=[IO.File]::OpenRead($path)
        try { $signatureBytes=[byte[]]::new(4); $read=$stream.Read($signatureBytes,0,4); if ($read -ne 4) { throw 'Shutdown dump signature is truncated.' }; $signature=[Text.Encoding]::ASCII.GetString($signatureBytes) }
        finally { $stream.Dispose() }
        if ($signature -cne 'MDMP') { throw "MiniDumpWriteDump output signature was '$signature'." }
        $result.status='captured';$result.sizeBytes=[Int64]$file.Length;$result.sha256=Get-FileSha256 $path;$result.signature=$signature
    } catch {
        $result.status='failed';$result.errorType=$_.Exception.GetType().FullName;$result.errorMessage=$_.Exception.Message
        if ($_.Exception -is [System.ComponentModel.Win32Exception]) { $result.nativeErrorCode=[int]$_.Exception.NativeErrorCode }
        if (Test-Path -LiteralPath $path -PathType Leaf) { $partial=Get-Item -LiteralPath $path;$result.sizeBytes=[Int64]$partial.Length;$result.sha256=Get-FileSha256 $path }
    } finally {
        $timer.Stop();$result.completedAtUtc=Get-UtcTimestamp;$result.captureElapsedMilliseconds=[Math]::Round($timer.Elapsed.TotalMilliseconds,3)
    }
    return $result
}
function Close-MeasuredProcess {
    param([object]$Context)
    $p=$Context.process; $p.Refresh(); $sent=$false; $exitedBefore=$p.HasExited; $closeStartedAt=Get-UtcTimestamp; $closeCallStartedAt=$null; $closeCallReturnedAt=$null; $samples=[Collections.Generic.List[object]]::new(); $windowsAtStart=$null; $windowsAtTimeout=$null; $shutdownDump=$Context.shutdownDump; $diagnosticWaitElapsedMilliseconds=$null; $diagnosticWaitWithinDeadline=$null; $boundedWaitCompleted=$false
    $isRoute2Diagnostic=([bool]$Route2OnlyDiagnostic -or (([bool]$Route2AfterExportShutdownDiagnostic -or [bool]$Route2AfterExportDumpDiagnostic) -and [string]$Context.routeId -ceq 'route-2-next-apply'))
    $diagnosticEnabled=$isRoute2Diagnostic
    $captureStartUiState=([bool]$Route2OnlyDiagnostic -or [bool]$Route2AfterExportShutdownDiagnostic)
    if ($diagnosticEnabled -and -not $p.HasExited -and $captureStartUiState) { $windowsAtStart=Get-CloseWindowUiState $Context 'start' }
    if (-not $p.HasExited) {
        if ($diagnosticEnabled -and -not $captureStartUiState) { $samples.Add((Get-CloseProcessSample $Context 'close-start-without-ui-query' 0)) }
        $closeCallStartedAt=Get-UtcTimestamp; $sent=$p.CloseMainWindow(); $closeCallReturnedAt=Get-UtcTimestamp
        if ($diagnosticEnabled) {
            $waitTimer=[Diagnostics.Stopwatch]::StartNew(); $sampleIndex=0; $dumpAttempted=($null -ne $shutdownDump)
            while ($waitTimer.Elapsed.TotalMilliseconds -lt 15000 -and -not $p.HasExited) {
                $remaining=[Math]::Max(1,[int][Math]::Ceiling(15000-$waitTimer.Elapsed.TotalMilliseconds))
                $waitReturned=$p.WaitForExit([Math]::Min(1000,$remaining)); if ($waitReturned) { $boundedWaitCompleted=$true }; Add-ProcessOutput $Context; $p.Refresh()
                if (-not $p.HasExited) { $samples.Add((Get-CloseProcessSample $Context 'close-wait' $sampleIndex)); $sampleIndex++ }
                if (-not $dumpAttempted -and $waitTimer.Elapsed.TotalSeconds -ge 10 -and -not $p.HasExited) {
                    $shutdownDump=Capture-Route2ShutdownMiniDump $Context ([Math]::Round($waitTimer.Elapsed.TotalSeconds,3)); $dumpAttempted=$true; $Context.shutdownDump=$shutdownDump
                }
            }
            $diagnosticWaitElapsedMilliseconds=[Math]::Round($waitTimer.Elapsed.TotalMilliseconds,3); $diagnosticWaitWithinDeadline=($waitTimer.Elapsed.TotalMilliseconds -le 15000)
            $p.Refresh(); if ($p.HasExited -and $diagnosticWaitWithinDeadline) { $boundedWaitCompleted=$true }
            $waitTimer.Stop()
        } else { $boundedWaitCompleted=$p.WaitForExit(15000) }
    }
    Add-ProcessOutput $Context; $p.Refresh(); $exited=$p.HasExited; $closeWaitEndedAt=Get-UtcTimestamp
    if ($diagnosticEnabled -and -not $exited) { $windowsAtTimeout=Get-CloseWindowUiState $Context 'timeout' }
    $closeObservation=[ordered]@{
        schema='classmngr-phase9-class-transfer-close-diagnostic-v1';routeId=$Context.routeId;processInstanceId=$Context.processInstanceId;processId=[Int64]$p.Id
        closeStartedAtUtc=$closeStartedAt;closeCallStartedAtUtc=$closeCallStartedAt;closeMainWindowReturned=$sent;closeCallReturnedAtUtc=$closeCallReturnedAt
        waitCompleted=[bool]($exited -and $boundedWaitCompleted);waitEndedAtUtc=$closeWaitEndedAt;waitDurationMilliseconds=if($null -ne $closeCallStartedAt){[Int64]([DateTimeOffset]::Parse($closeWaitEndedAt)-[DateTimeOffset]::Parse($closeCallStartedAt)).TotalMilliseconds}else{0};diagnosticWaitElapsedMilliseconds=$diagnosticWaitElapsedMilliseconds;diagnosticWaitWithin15SecondDeadline=$diagnosticWaitWithinDeadline
        alreadyExitedBeforeClose=$exitedBefore;processExitCode=if($p.HasExited){[Int64]$p.ExitCode}else{$null};forcedTerminationForCleanup=$false
        windowsAndUiaAtStart=$windowsAtStart;windowsAndUiaAtTimeout=$windowsAtTimeout;uiStateAtStartSuppressedForDumpDiagnostic=([bool]$Route2AfterExportDumpDiagnostic -and [string]$Context.routeId -ceq 'route-2-next-apply');processSamples=@($samples.ToArray())
        shutdownDump=$shutdownDump
        shutdownTrace=$null
        recentProcessOutputAtCloseWaitEnd=@($Context.logEntries | Select-Object -Last 40 | ForEach-Object { [ordered]@{stream=$_.stream;observedAtUtc=$_.observedAtUtc;line=$_.line} })
    }
    if (-not $p.HasExited) {
        $killStartedAt=Get-UtcTimestamp; $p.Kill(); $p.WaitForExit(); $p.Refresh(); $closeObservation.forcedTerminationForCleanup=$true; $closeObservation.cleanupKillStartedAtUtc=$killStartedAt; $closeObservation.processExitCode=[Int64]$p.ExitCode
    }
    foreach ($reader in $Context.readers) { try { [void]$reader.pumpTask.Wait(1000) } catch { } }
    Add-ProcessOutput $Context
    $traceRows=@(); $tracePath=[string]$Context.shutdownTracePath; $traceError=$null
    if (-not [string]::IsNullOrWhiteSpace($tracePath) -and (Test-Path -LiteralPath $tracePath -PathType Leaf)) {
        try { $traceRows=@(Get-Content -LiteralPath $tracePath -Encoding UTF8 | Where-Object { $_.Trim() -ne '' } | ForEach-Object { $_ | ConvertFrom-Json -ErrorAction Stop }) }
        catch { $traceError=$_.Exception.Message }
    }
    $closeObservation.shutdownDump=$Context.shutdownDump
    $closeObservation.shutdownTrace=[ordered]@{path=$tracePath;sha256=if(Test-Path -LiteralPath $tracePath -PathType Leaf){Get-FileSha256 $tracePath}else{$null};events=@($traceRows | ForEach-Object { [string]$_.event });rows=$traceRows;parseError=$traceError}
    $closeObservation.recentProcessOutput=@($Context.logEntries | Select-Object -Last 40 | ForEach-Object { [ordered]@{stream=$_.stream;observedAtUtc=$_.observedAtUtc;line=$_.line} })
    $closePath=Join-Path (Join-Path $EvidenceDirectory 'diagnostics') ($Context.routeId+'-close-observation.json'); [void][IO.Directory]::CreateDirectory((Split-Path -Parent $closePath)); Write-JsonFile $closePath $closeObservation
    $Context.closeObservationPath=[IO.Path]::GetFullPath($closePath); $Context.closeObservationSha256=Get-FileSha256 $closePath
    $Context.exitObservation=[ordered]@{
        closeMainWindowReturned=$sent;closeCallStartedAtUtc=$closeCallStartedAt;closeCallReturnedAtUtc=$closeCallReturnedAt;alreadyExitedBeforeClose=$exitedBefore
        waitCompleted=[bool]$closeObservation.waitCompleted;exitCode=if($p.HasExited){[Int64]$p.ExitCode}else{$null};forcedTerminationForCleanup=[bool]$closeObservation.forcedTerminationForCleanup
        observedAtUtc=Get-UtcTimestamp;closeObservationPath=$Context.closeObservationPath;closeObservationSha256=$Context.closeObservationSha256
        shutdownTracePath=$tracePath;shutdownTraceSha256=$closeObservation.shutdownTrace.sha256;shutdownTraceEvents=$closeObservation.shutdownTrace.events;shutdownTraceParseError=$traceError;shutdownDump=$Context.shutdownDump
    }
    return $Context.exitObservation
}
function Invoke-WorkspacePreparation {
    param([string]$Role,[string]$OutputName)
    $output=Join-Path (Join-Path $EvidenceDirectory 'workspace-preparation') $OutputName
    $receiptPath=[IO.Path]::ChangeExtension($output,'.json')
    $result=Invoke-PythonJson (Join-Path $PSScriptRoot 'prepare_class_transfer_workspace.py') @('--blank-workspace',$MasterBlankPath,'--output-workspace',$output,'--seed-sql',(Join-Path $RepositoryRoot $SeedSqlRelative),'--role',$Role) (Join-Path (Join-Path $EvidenceDirectory 'logs') ($OutputName+'.log')) "prepare $Role workspace"
    Write-JsonFile $receiptPath $result
    $snapshotPath=Join-Path (Join-Path $EvidenceDirectory 'workspace-snapshots') ($Role+'-prepared.json')
    $snapshot=Invoke-WorkspaceSnapshot $output $snapshotPath $Role 'prepared'
    $entry=[ordered]@{role=$Role;path=[IO.Path]::GetFullPath($output);sha256=Get-FileSha256 $output;receiptPath=[IO.Path]::GetFullPath($receiptPath);receiptSha256=Get-FileSha256 $receiptPath;preparedSnapshotPath=[IO.Path]::GetFullPath($snapshotPath);preparedSnapshotSha256=Get-FileSha256 $snapshotPath;preparedTableContentSha256=[string]$snapshot.tableContentSha256;blankWorkspaceSha256=Get-FileSha256 $MasterBlankPath;seedSqlPath=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot $SeedSqlRelative));seedSqlSha256=Get-FileSha256 (Join-Path $RepositoryRoot $SeedSqlRelative);transformationSha256=[string]$result.transformationSha256;integrityCheck=[string]$result.integrityCheck;foreignKeyErrorCount=@($result.foreignKeyErrors).Count;transformations=$result.transformations;finalCounts=$result.finalCounts}
    $script:Manifest.workspacePreparation += ,$entry
    return $entry
}
function Copy-VerifiedRoute2DiagnosticPackage {
    param([string]$SourceManifestPath)
    if (-not (Test-Path -LiteralPath $SourceManifestPath -PathType Leaf)) { throw "Route 2 diagnostic source manifest is missing: $SourceManifestPath" }
    $sourceManifest=Get-Content -LiteralPath $SourceManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
    if ([string]$sourceManifest.taskId -cne $TaskId -or [string]$sourceManifest.schema -cne $ManifestSchema) { throw 'Route 2 diagnostic source manifest is not a P9-11 packaged Class Transfer run.' }
    $route=@($sourceManifest.routes | Where-Object { $_.routeId -ceq 'route-1-next-export' })
    if ($route.Count -ne 1) { throw 'Route 2 diagnostic source manifest does not contain one Route 1 export.' }
    $sourceRoute=$route[0]; $recorded=$sourceManifest.exportedPackage; $sourcePath=[IO.Path]::GetFullPath([string]$recorded.path)
    $sourceRoot=[IO.Path]::GetFullPath([IO.Path]::GetDirectoryName($SourceManifestPath)).TrimEnd([char[]]@('\','/'))+[IO.Path]::DirectorySeparatorChar
    if (-not $sourcePath.StartsWith($sourceRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Route 2 diagnostic export path escapes its recorded source run.' }
    if (-not [bool]$sourceRoute.normalExit -or [int]$sourceRoute.processExit.exitCode -ne 0 -or [bool]$sourceRoute.processExit.forcedTerminationForCleanup -or [string]$sourceRoute.operation.kind -cne 'next-ui-export' -or [string]$sourceRoute.operation.outcome -cne 'completed' -or [int]$sourceRoute.operation.metrics.export_selected_class_count -ne 48 -or [int]$sourceRoute.operation.metrics.exports_completed -ne 1) { throw 'Route 2 diagnostic source Route 1 did not complete a natural 48-class Next UI export.' }
    if ([string]$sourceRoute.operation.packageSha256 -cne [string]$recorded.sha256 -or [Int64]$sourceRoute.operation.packageSizeBytes -ne [Int64]$recorded.sizeBytes -or [int]$sourceRoute.processIdentity.pid -ne [int]$recorded.createdByPid -or [string]$recorded.createdByRouteId -cne 'route-1-next-export') { throw 'Route 2 diagnostic source package metadata does not match the measured Route 1 export.' }
    if ([string]$sourceManifest.application.sha256 -cne [string]$script:Manifest.application.sha256 -or [string]$sourceManifest.package.applicationSha256 -cne [string]$script:Manifest.package.applicationSha256) { throw 'Route 2 diagnostic export was produced with a different packaged application.' }
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf) -or (Get-FileSha256 $sourcePath) -cne [string]$recorded.sha256 -or [Int64](Get-Item -LiteralPath $sourcePath).Length -ne [Int64]$recorded.sizeBytes) { throw 'Route 2 diagnostic export bytes do not match the recorded Route 1 SHA-256 and size.' }
    Copy-Item -LiteralPath $sourcePath -Destination $ExportedPackagePath -ErrorAction Stop
    if ((Get-FileSha256 $ExportedPackagePath) -cne [string]$recorded.sha256 -or [Int64](Get-Item -LiteralPath $ExportedPackagePath).Length -ne [Int64]$recorded.sizeBytes) { throw 'Copied Route 2 diagnostic package differs from its Route 1 export.' }
    $sourceManifestHash=Get-FileSha256 $SourceManifestPath
    $source=[ordered]@{runId=[string]$sourceManifest.runId;manifestPath=$SourceManifestPath;manifestSha256=$sourceManifestHash;routeId='route-1-next-export';processId=[Int64]$recorded.createdByPid;packageSourcePath=$sourcePath;packageSha256=[string]$recorded.sha256;packageSizeBytes=[Int64]$recorded.sizeBytes}
    $script:Manifest.exportedPackage=[ordered]@{path=[IO.Path]::GetFullPath($ExportedPackagePath);sha256=[string]$recorded.sha256;sizeBytes=[Int64]$recorded.sizeBytes;createdByRouteId='route-1-next-export';createdByPid=[Int64]$recorded.createdByPid;diagnosticSourceRunId=[string]$sourceManifest.runId;diagnosticSourceManifestPath=$SourceManifestPath;diagnosticSourceManifestSha256=$sourceManifestHash;diagnosticSourcePackagePath=$sourcePath}
    return $source
}
function Get-PageRows {
    param([object]$Context,[string]$AutomationId,[string]$ExpectedType)
    $control=Get-UiaElementById $Context $AutomationId $ExpectedType
    $desc=@($control.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition))
    [ordered]@{control=Get-UiaSnapshot $control;descendants=@($desc | ForEach-Object { Get-UiaSnapshot $_ });descendantCount=$desc.Count}
}
function Get-UiaListItems {
    param([System.Windows.Automation.AutomationElement]$List,[int]$ProcessId)
    $descendants=$List.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition)
    @($descendants | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::ListItem -and $_.Current.ProcessId -eq $ProcessId })
}
function Invoke-ClassTransferRoute {
    param([string]$RouteId,[string]$Role,[object]$PreparedWorkspace)
    $runtime=Join-Path (Join-Path $EvidenceDirectory 'runtime') $RouteId
    foreach ($name in @('workspace','settings','local-app-data','app-data','temp','diagnostics')) { [void][IO.Directory]::CreateDirectory((Join-Path $runtime $name)) }
    $workspacePath=Join-Path (Join-Path $runtime 'workspace') ($RouteId+'.tps')
    Copy-Item -LiteralPath $PreparedWorkspace.path -Destination $workspacePath
    if ((Get-FileSha256 $workspacePath) -cne [string]$PreparedWorkspace.sha256) { throw "$RouteId runtime workspace differs from its prepared copy." }
    $beforePath=Join-Path (Join-Path $EvidenceDirectory 'workspace-snapshots') ($RouteId+'-before.json')
    $afterPath=Join-Path (Join-Path $EvidenceDirectory 'workspace-snapshots') ($RouteId+'-after.json')
    $diagnosticPath=Join-Path (Join-Path $runtime 'diagnostics') 'class-transfer.jsonl'
    $shutdownTracePath=Join-Path (Join-Path $runtime 'diagnostics') 'application-shutdown.jsonl'
    $shutdownDumpPath=Join-Path (Join-Path $runtime 'diagnostics') 'shutdown-threadinfo-10s.dmp'
    $stdoutPath=Join-Path (Join-Path $EvidenceDirectory 'logs') ($RouteId+'-stdout.log')
    $stderrPath=Join-Path (Join-Path $EvidenceDirectory 'logs') ($RouteId+'-stderr.log')
    $processStart=[Diagnostics.ProcessStartInfo]::new(); $processStart.FileName=[string]$script:Manifest.application.canonicalPath; $processStart.WorkingDirectory=Split-Path -Parent $processStart.FileName; $processStart.UseShellExecute=$false; $processStart.CreateNoWindow=$false; $processStart.RedirectStandardOutput=$true; $processStart.RedirectStandardError=$true; $processStart.Arguments=ConvertTo-QuotedArgument ([IO.Path]::GetFullPath($workspacePath))
    $envMap=[ordered]@{CLASSMNGR_SETTINGS_ROOT=(Join-Path $runtime 'settings');CLASSMNGR_CLASS_TRANSFER_DIAGNOSTICS_PATH=[IO.Path]::GetFullPath($diagnosticPath);CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH=[IO.Path]::GetFullPath($shutdownTracePath);LOCALAPPDATA=(Join-Path $runtime 'local-app-data');APPDATA=(Join-Path $runtime 'app-data');TEMP=(Join-Path $runtime 'temp');TMP=(Join-Path $runtime 'temp');QT_FORCE_STDERR_LOGGING='1';PATH="$script:QtBin;$(Split-Path -Parent $processStart.FileName);$env:PATH"}
    foreach ($entry in $envMap.GetEnumerator()) { $processStart.EnvironmentVariables[[string]$entry.Key]=[string]$entry.Value }
    $process=[Diagnostics.Process]::new(); $process.StartInfo=$processStart; if (-not $process.Start()) { throw "Could not start $RouteId packaged Next process." }; $process.Refresh()
    $observedPath=[IO.Path]::GetFullPath([P911Native]::GetProcessImagePath($process.Handle)); $observedHash=Get-FileSha256 $observedPath
    if (-not [string]::Equals($observedPath,[string]$script:Manifest.application.canonicalPath,[StringComparison]::OrdinalIgnoreCase) -or $observedHash -cne [string]$script:Manifest.application.sha256 -or -not [P911Native]::IsNativeX64($process.Handle)) { throw "$RouteId process image path/hash/architecture differs from current-source x64 package." }
    $createdAtUtc=$process.StartTime.ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss.fffffff'Z'")
    $stdoutQueue=[Collections.Concurrent.ConcurrentQueue[string]]::new(); $stderrQueue=[Collections.Concurrent.ConcurrentQueue[string]]::new()
    $stdoutPump=[P911OutputPump]::Start($process.StandardOutput,$stdoutQueue); $stderrPump=[P911OutputPump]::Start($process.StandardError,$stderrQueue)
    $context=[ordered]@{routeId=$RouteId;processInstanceId=$RouteId;process=$process;executablePath=$observedPath;executableSha256=$observedHash;createdAtUtc=$createdAtUtc;workspacePath=[IO.Path]::GetFullPath($workspacePath);diagnosticPath=[IO.Path]::GetFullPath($diagnosticPath);shutdownTracePath=[IO.Path]::GetFullPath($shutdownTracePath);stdoutIndexPath=(Join-Path $EvidenceDirectory 'startup-log.jsonl');readers=@([pscustomobject]@{queue=$stdoutQueue;pumpTask=$stdoutPump;stream='stdout';path=$stdoutPath;closed=$false},[pscustomobject]@{queue=$stderrQueue;pumpTask=$stderrPump;stream='stderr';path=$stderrPath;closed=$false});logEntries=[Collections.Generic.List[object]]::new();mainWindowHandle=0L;mainWindowTitle='';exitCode=$null;exitObservation=$null;uiaActions=@()}
    $context.shutdownDumpPath=[IO.Path]::GetFullPath($shutdownDumpPath); $context.shutdownDump=$null
    $script:Processes.Add($context); $script:CurrentContext=$context
    $route=[ordered]@{
        routeId=$RouteId;role=$Role;workspace=[ordered]@{path=[IO.Path]::GetFullPath($workspacePath);preparedPath=[string]$PreparedWorkspace.path;preparedSha256=[string]$PreparedWorkspace.sha256;sha256AtLaunch=Get-FileSha256 $workspacePath;beforeSnapshot=[IO.Path]::GetFullPath($beforePath);afterSnapshot=[IO.Path]::GetFullPath($afterPath);beforeTableContentSha256=$null;afterTableContentSha256=$null;rowLevelContentsUnchanged=$false;expectedDeltaVerified=$false}
        processIdentity=[ordered]@{pid=[Int64]$process.Id;createdAtUtc=$createdAtUtc;canonicalExecutablePath=$observedPath;architecture='x64';sha256=$observedHash;mainWindowHandle=$null;mainWindowTitle=$null;mainWindowProcessId=$null}
        uia=[ordered]@{};operation=$null;lifecycle=[ordered]@{suspendCount=0;reentryCount=0;initialActivation=$null;suspended=$null;reentered=$null};memory=[ordered]@{normalUseSamples=@();operationSamples=@();idleTail=@()};processExitCode=$null;processExit=$null;normalExit=$false;stdoutLog=[IO.Path]::GetFullPath($stdoutPath);stderrLog=[IO.Path]::GetFullPath($stderrPath);diagnosticPath=[IO.Path]::GetFullPath($diagnosticPath);diagnosticSha256=$null;shutdownTracePath=[IO.Path]::GetFullPath($shutdownTracePath);shutdownTraceSha256=$null;shutdownDumpPath=[IO.Path]::GetFullPath($shutdownDumpPath);shutdownDump=$null;closeObservationPath=$null;closeObservationSha256=$null;failure=$null
    }
    $route.shutdownDumpPath=[IO.Path]::GetFullPath($shutdownDumpPath); $route.shutdownDump=$null
    $script:Manifest.routes += ,$route
    try {
        Wait-ForCondition $context { param($ctx) Add-ProcessOutput $ctx; @($ctx.logEntries | Where-Object { $_.line -match '^ClassMngrNext startup stage=startup-ready ' }).Count -eq 1 } 'startup-ready checkpoint' $ReadinessTimeoutSeconds | Out-Null
        $opened=Get-StartupCheckpoint $context 'workspace-opened'; $ready=Get-StartupCheckpoint $context 'startup-ready'; $route.startupReady=$ready; $route.workspaceOpened=$opened
        $startup=[ordered]@{routeId=$RouteId;processInstanceId=$context.processInstanceId;point='startup-ready-checkpoint';timestampUtc=$ready.observedAtUtc;pid=[Int64]$process.Id;currentWorkingSetBytes=[Int64]$ready.record.metrics.working_set_bytes;privateBytes=[Int64]$ready.record.metrics.private_usage_bytes;commitBytes=[Int64]$ready.record.metrics.pagefile_usage_bytes;peakWorkingSetBytes=[Int64]$ready.record.metrics.peak_working_set_bytes;handles=[Int64]$ready.record.metrics.handle_count;threads=[Int64]$ready.record.metrics.thread_count;processCreatedAtUtc=$createdAtUtc;canonicalExecutablePath=$observedPath;sha256=$observedHash}
        Assert-StrictWorkingSet $startup "$RouteId startup"; Add-JsonLine $ProcessMetricsPath $startup; $route.memory.normalUseSamples+=,$startup
        $startupExternal=New-ProcessSample $context 'startup-ready-external'; Assert-StrictWorkingSet $startupExternal "$RouteId startup external"; $route.memory.normalUseSamples+=,$startupExternal
        Start-Sleep -Seconds 5; $settled=New-ProcessSample $context 'startup-settled'; Assert-StrictWorkingSet $settled "$RouteId startup settled"; $route.memory.normalUseSamples+=,$settled
        $deadline=[DateTimeOffset]::UtcNow.AddSeconds(20); $stable=0; $stableHandle=0L; $stableTitle=''
        while ([DateTimeOffset]::UtcNow -lt $deadline -and $stable -lt 3) { Add-ProcessOutput $context; $process.Refresh(); $handle=$process.MainWindowHandle; $title=$process.MainWindowTitle.Trim(); if ($handle -ne [IntPtr]::Zero -and [P911Native]::IsWindowVisibleForProcess($handle) -and [P911Native]::GetWindowProcessId($handle) -eq [uint32]$process.Id -and $title -match 'ClassMngr Next') { if ($stableHandle -eq $handle.ToInt64() -and $stableTitle -ceq $title) {$stable++} else {$stableHandle=$handle.ToInt64();$stableTitle=$title;$stable=1} } else {$stable=0;$stableHandle=0L;$stableTitle=''}; if ($stable -lt 3) {Start-Sleep -Milliseconds 250} }
        if ($stable -lt 3) { throw "$RouteId did not expose a stable PID-owned main window." }; $context.mainWindowHandle=$stableHandle;$context.mainWindowTitle=$stableTitle;$route.processIdentity.mainWindowHandle=$stableHandle;$route.processIdentity.mainWindowTitle=$stableTitle;$route.processIdentity.mainWindowProcessId=[Int64][P911Native]::GetWindowProcessId([IntPtr]$stableHandle)
        $mainTree=Save-UiaTree $context ("uia/"+$RouteId+"-startup.json"); $route.uia.startupTree=$mainTree
        $navigation=Get-UiaByName $context 'Class Transfer' 'ControlType.TreeItem'; $route.uia.navigationClassTransfer=Invoke-VerifiedUiaClick $context $navigation 'navigate-to-class-transfer' 'startup' 'class_transfer'
        $page=Wait-ForUiaElement $context 'nextClassTransferPage' '' 20
        if ([Int64]$page.Current.ProcessId -ne [Int64]$process.Id -or $page.Current.ControlType -ne [System.Windows.Automation.ControlType]::Group) { throw "$RouteId Class Transfer page root does not match expected PID/control type." }
        $activated=Wait-DiagnosticCheckpoint $context 'activated' 1 20; $route.lifecycle.initialActivation=$activated
        $route.uia.initialPage=Get-UiaSnapshot $page
        $before=Invoke-WorkspaceSnapshot $workspacePath $beforePath $RouteId 'before'
        if ($before.integrityCheck -cne 'ok' -or [Int64]$before.foreignKeyErrorCount -ne 0) { throw "$RouteId pre-operation workspace failed integrity/foreign-key checks." }
        $route.workspace.beforeTableContentSha256=[string]$before.tableContentSha256
        if ($Role -ceq 'export') {
            $list=Get-UiaElementById $context 'classTransferExportClassList' 'ControlType.List'
            $sourceSnapshot=Get-Content -LiteralPath $beforePath -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
            $expectedClassNames=@($sourceSnapshot.tables.classes.rows | Sort-Object { [int]$_[0] } | ForEach-Object { [string]$_[1] })
            if ($expectedClassNames.Count -ne 96 -or @($expectedClassNames | Select-Object -Unique).Count -ne 96) { throw "Source workspace snapshot provided $($expectedClassNames.Count) unique classes; expected 96." }
            $rowEvidence=[Collections.Generic.List[object]]::new()
            $scrollEvidence=[Collections.Generic.List[object]]::new()
            $listRect=$list.Current.BoundingRectangle; $scrollX=[int][Math]::Round($listRect.X+$listRect.Width/2); $scrollY=[int][Math]::Round($listRect.Y+$listRect.Height/2)
            for ($i=0;$i -lt 48;$i++) {
                $expectedName=[string]$expectedClassNames[$i]; $item=$null; $wheelMessages=0
                while ($wheelMessages -le 48) {
                    $visibleItems=Get-UiaListItems $list $process.Id
                    $targetItems=@($visibleItems | Where-Object {
                        $itemRect=$_.Current.BoundingRectangle
                        [string]$_.Current.Name -ceq $expectedName -and -not $_.Current.IsOffscreen -and
                            $itemRect.X -ge ($listRect.X+1) -and $itemRect.Y -ge ($listRect.Y+1) -and
                            ($itemRect.X+$itemRect.Width) -le ($listRect.X+$listRect.Width-1) -and
                            ($itemRect.Y+$itemRect.Height) -le ($listRect.Y+$listRect.Height-1)
                    })
                    if ($targetItems.Count -eq 1) { $item=$targetItems[0]; break }
                    if ($wheelMessages -ge 48) { break }
                    $wheel=[P911Native]::PostVerifiedWindowWheel([IntPtr]$context.mainWindowHandle,$scrollX,$scrollY,-120,[uint32]$process.Id)
                    $scrollRecord=[ordered]@{routeId=$RouteId;processInstanceId=$context.processInstanceId;actionKind='scroll-export-class-list';method='uia-list-bound-pid-verified-window-wheel';targetList=Get-UiaSnapshot $list;expectedNextClassName=$expectedName;scrollX=$scrollX;scrollY=$scrollY;wheelDelta=-120;messageResult=$wheel;issuedAtUtc=Get-UtcTimestamp}
                    Add-JsonLine $UiaActionsPath $scrollRecord; $scrollEvidence.Add($scrollRecord); $route.uia.exportListScroll=@($scrollEvidence.ToArray()); $wheelMessages++; Start-Sleep -Milliseconds 100
                }
                if ($null -eq $item) { $route.uia.exportListFailure=Save-UiaTree $context ("uia/"+$RouteId+"-export-list-failure.json"); throw "UIA export list did not expose expected class '$expectedName' after $wheelMessages scroll messages." }
                $identity=Get-UiaSnapshot $item
                if ([bool]$identity.offscreen -or [string]$identity.name -cne $expectedName) { throw "UIA export list target drifted from expected class '$expectedName'." }
                $toggle=[System.Windows.Automation.TogglePattern]$item.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern); $stateBefore=[string]$toggle.Current.ToggleState
                if ($toggle.Current.ToggleState -ne [System.Windows.Automation.ToggleState]::Off) { throw "Expected '$expectedName' TogglePattern Off before selection, observed $stateBefore." }
                $clickX=[int][Math]::Round($identity.rectangle.x+12); $clickY=[int][Math]::Round($identity.rectangle.y+$identity.rectangle.height/2)
                $activation=[P911Native]::PostVerifiedWindowClick([IntPtr]$context.mainWindowHandle,$clickX,$clickY,[uint32]$process.Id)
                $method='uia-listitem-pid-bound-window-input'
                $selectionAction=[ordered]@{routeId=$RouteId;processInstanceId=$context.processInstanceId;actionKind='select-export-class';method=$method;target=$identity;windowHandle=[Int64]$context.mainWindowHandle;windowProcessId=[Int64][P911Native]::GetWindowProcessId([IntPtr]$context.mainWindowHandle);mainWindowHandle=[Int64]$context.mainWindowHandle;mainWindowProcessId=[Int64]$process.Id;classIndex=$i;expectedClassName=$expectedName;screenX=$clickX;screenY=$clickY;nativeActivation=$activation;issuedAtUtc=Get-UtcTimestamp}
                Add-JsonLine $UiaActionsPath $selectionAction
                $stateAfter='Off'; $itemAfter=$null
                for ($stateAttempt=0;$stateAttempt -lt 15;$stateAttempt++) {
                    Start-Sleep -Milliseconds 50
                    $refreshedItems=@((Get-UiaListItems $list $process.Id) | Where-Object { [string]$_.Current.Name -ceq $expectedName -and -not $_.Current.IsOffscreen })
                    if ($refreshedItems.Count -eq 1) { $itemAfter=$refreshedItems[0]; $stateAfter=[string]([System.Windows.Automation.TogglePattern]$itemAfter.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)).Current.ToggleState; if ($stateAfter -ceq 'On') { break } }
                }
                if ($stateAfter -cne 'On') { $route.uia.exportSelectionProgress=@($rowEvidence.ToArray()); $route.uia.exportListScroll=@($scrollEvidence.ToArray()); $route.uia.exportListFailure=Save-UiaTree $context ("uia/"+$RouteId+"-export-list-failure.json"); throw "PID-bound UIA target input did not set '$expectedName' TogglePattern On (state=$stateAfter)." }
                if ($null -ne $itemAfter) { $item=$itemAfter }
                $rowEvidence.Add([ordered]@{index=$i;expectedClassName=$expectedName;identity=$identity;method=$method;toggleStateAfter=$stateAfter;nativeActivation=$activation;scrollWheelMessages=$wheelMessages;processId=[Int64]$item.Current.ProcessId})
                $route.uia.exportSelection=@($rowEvidence.ToArray()); $route.uia.exportListScroll=@($scrollEvidence.ToArray())
            }
            if ($rowEvidence.Count -ne 48 -or @($rowEvidence | ForEach-Object { $_.expectedClassName } | Select-Object -Unique).Count -ne 48) { throw 'UIA export selection evidence did not contain 48 unique ordered source classes.' }
            $route.uia.exportSelection=@($rowEvidence.ToArray()); $route.uia.exportListScroll=@($scrollEvidence.ToArray()); $route.uia.exportListVisibleItems=@((Get-UiaListItems $list $process.Id) | ForEach-Object { Get-UiaSnapshot $_ }); $route.uia.exportList=Get-PageRows $context 'classTransferExportClassList' 'ControlType.List'
            $exportButton=Get-UiaElementById $context 'classTransferExport' 'ControlType.Button'
            if (-not $exportButton.Current.IsEnabled) { throw 'Next Export button is disabled after selecting the 48 source classes.' }
            $route.uia.exportButtonBefore=Get-UiaSnapshot $exportButton
            $route.uia.exportButtonAction=Invoke-VerifiedUiaClick $context $exportButton 'open-next-class-transfer-export-picker' 'class_transfer' 'file_picker'
            $route.operation=[ordered]@{startedAtUtc=Get-UtcTimestamp;kind='next-ui-export';outputPath=[IO.Path]::GetFullPath($ExportedPackagePath);filePicker=Select-PackageThroughFilePicker $context $ExportedPackagePath 'Save'}
            $exported=Wait-DiagnosticCheckpoint $context 'export-completed' 1 30
            $route.operation.completedAtUtc=[string]$exported.captured_at_utc; $route.operation.metrics=$exported.metrics; $route.operation.outcome=[string]$exported.outcome
            if (-not (Test-Path -LiteralPath $ExportedPackagePath -PathType Leaf)) { throw 'Next export route did not create the exact package output.' }
            $route.operation.packageSha256=Get-FileSha256 $ExportedPackagePath; $route.operation.packageSizeBytes=[Int64](Get-Item -LiteralPath $ExportedPackagePath).Length
            $route.operation.terminalMetrics=$exported.metrics; $route.operation.packageRetained=[bool]$exported.metrics.package_retained
            $route.memory.operationSamples+=,(New-ProcessSample $context 'export-completed')
            if ([int]$exported.metrics.export_selected_class_count -ne 48 -or [int]$exported.metrics.exports_completed -ne 1 -or [bool]$exported.metrics.package_retained -or [bool]$exported.metrics.preview_retained) { throw 'Next export terminal metrics do not match 48 selected classes and released package state.' }
            $route.uia.postExportStatus=(Get-UiaElementById $context 'classTransferStatus' 'ControlType.Text' | Get-UiaSnapshot)
        } elseif ($Role -ceq 'apply' -or $Role -ceq 'cancel') {
            if (-not (Test-Path -LiteralPath $ExportedPackagePath -PathType Leaf)) { throw 'Route 1 Next-exported package is missing.' }
            $importButton=Get-UiaElementById $context 'classTransferImport' 'ControlType.Button'
            $route.uia.importButtonBefore=Get-UiaSnapshot $importButton
            $route.uia.importButtonAction=Invoke-VerifiedUiaClick $context $importButton 'open-next-class-transfer-import-picker' 'class_transfer' 'file_picker'
            $route.operation=[ordered]@{startedAtUtc=Get-UtcTimestamp;kind=if($Role -ceq 'apply'){'next-ui-import-apply'}else{'next-ui-import-cancel'};packagePath=[IO.Path]::GetFullPath($ExportedPackagePath);packageSha256=Get-FileSha256 $ExportedPackagePath;filePicker=Select-PackageThroughFilePicker $context $ExportedPackagePath 'Open'}
            $review=Wait-DiagnosticCheckpoint $context 'review-ready' 1 30
            $route.operation.reviewReadyAtUtc=[string]$review.captured_at_utc; $route.operation.reviewMetrics=$review.metrics; $route.operation.packageClassCount=[int]$review.metrics.package_class_count; $route.operation.packageTeacherCount=[int]$review.metrics.package_teacher_count
            $route.operation.classReviewRows=[int]$review.metrics.class_review_row_count; $route.operation.teacherReviewRows=[int]$review.metrics.teacher_review_row_count; $route.operation.matchingClassCandidates=[int]$review.metrics.matching_class_candidate_count; $route.operation.matchingTeacherCandidates=[int]$review.metrics.matching_teacher_candidate_count; $route.operation.applyEnabled=[bool]$review.metrics.apply_enabled
            $route.uia.classReviewTable=Get-PageRows $context 'classTransferClassReviewTable' 'ControlType.Table'
            $teacherTab=Get-UiaByName $context 'Teachers' 'ControlType.TabItem'
            $route.uia.teacherReviewTabAction=Invoke-VerifiedUiaClick $context $teacherTab 'select-teacher-review-tab' 'class_transfer' 'class_transfer'
            $null=Wait-ForCondition $context { param($ctx) $found=@(Get-AppElements $ctx | Where-Object { $id=[string]$_.Current.AutomationId; ($id -ceq 'classTransferTeacherReviewTable' -or $id.EndsWith('.classTransferTeacherReviewTable',[StringComparison]::Ordinal)) -and $_.Current.ControlType.ProgrammaticName -ceq 'ControlType.Table' -and $_.Current.ProcessId -eq $ctx.process.Id -and -not $_.Current.IsOffscreen }); if ($found.Count -eq 1) { return $found[0] }; return $null } 'visible teacher review table' 20
            $route.uia.teacherReviewTable=Get-PageRows $context 'classTransferTeacherReviewTable' 'ControlType.Table'
            $route.uia.reviewSummary=(Get-UiaElementById $context 'classTransferReviewSummary' 'ControlType.Text' | Get-UiaSnapshot)
            $route.uia.reviewStatus=(Get-UiaElementById $context 'classTransferStatus' 'ControlType.Text' | Get-UiaSnapshot)
            $route.uia.applyButtonBefore=Get-UiaSnapshot (Get-UiaElementById $context 'classTransferApply' 'ControlType.Button')
            $route.uia.cancelButtonBefore=Get-UiaSnapshot (Get-UiaElementById $context 'classTransferCancelReview' 'ControlType.Button')
            if ([int]$review.metrics.package_class_count -ne 48 -or [int]$review.metrics.package_teacher_count -ne 24 -or [int]$review.metrics.class_review_row_count -ne 48 -or [int]$review.metrics.teacher_review_row_count -ne 24 -or -not [bool]$review.metrics.package_retained -or -not [bool]$review.metrics.preview_retained) { throw "$RouteId review metrics do not match the exact 48-class/24-teacher package." }
            $route.memory.operationSamples+=,(New-ProcessSample $context 'review-ready')
            if ($Role -ceq 'apply') {
                if ([int]$review.metrics.matching_class_candidate_count -ne 0 -or [int]$review.metrics.matching_teacher_candidate_count -ne 0 -or -not [bool]$review.metrics.apply_enabled -or -not $route.uia.applyButtonBefore.enabled) { throw 'Apply preview has class/teacher matches or Apply is not enabled.' }
                $applyButton=Get-UiaElementById $context 'classTransferApply' 'ControlType.Button'
                $route.uia.applyAction=Invoke-VerifiedUiaClick $context $applyButton 'apply-next-class-transfer-package' 'class_transfer_review' 'class_transfer'
                try { $applied=Wait-DiagnosticCheckpoint $context 'apply-completed' 1 45 }
                catch {
                    try { $route.operation.failureDiagnostics=Capture-CheckpointTimeout $context 'apply-completed' 45 $route }
                    catch { $route.operation.failureDiagnosticsCaptureError=$_.Exception.Message }
                    throw
                }
                $route.operation.completedAtUtc=[string]$applied.captured_at_utc; $route.operation.terminalMetrics=$applied.metrics; $route.operation.outcome=[string]$applied.outcome
                if ([int]$applied.metrics.imports_applied -ne 1 -or [bool]$applied.metrics.package_retained -or [bool]$applied.metrics.preview_retained -or [int]$applied.metrics.package_class_count -ne 0 -or [int]$applied.metrics.class_review_row_count -ne 0 -or [int]$applied.metrics.teacher_review_row_count -ne 0) { throw 'Apply terminal metrics do not show one applied package and released review state.' }
                $classesPage=Wait-ForCondition $context { param($ctx) $matches=@(Get-AppElements $ctx | Where-Object { $id=[string]$_.Current.AutomationId; ($id -ceq 'classesNavigatorPage' -or $id.EndsWith('.classesNavigatorPage',[StringComparison]::Ordinal)) -and $_.Current.ControlType.ProgrammaticName -ceq 'ControlType.Group' -and $_.Current.ProcessId -eq $ctx.process.Id -and -not $_.Current.IsOffscreen }); if ($matches.Count -eq 1) { return $matches[0] }; return $null } 'visible Classes page after applied-class selection' 20
                $classesTitle=Get-VisibleUiaElementById $context 'classesNavigatorTitle' 'ControlType.Text'
                if ($classesTitle.Current.Name -cne 'Classes') { throw "Apply navigated to an unexpected Classes page title: $($classesTitle.Current.Name)." }
                $selectedDetails=[ordered]@{}
                foreach ($detail in @(
                    @{key='classGrade';id='classEditorClassGrade'},
                    @{key='classLevel';id='classEditorClassLevel'},
                    @{key='readingBook';id='classEditorReadingBook'},
                    @{key='essayBook';id='classEditorEssayBook'}
                )) { $selectedDetails[$detail.key]=Get-UiaSnapshot (Get-VisibleUiaElementById $context $detail.id 'ControlType.ComboBox') }
                $route.uia.postApplyPage=[ordered]@{root=(Get-UiaSnapshot $classesPage);title=(Get-UiaSnapshot $classesTitle);selectedClassDetails=$selectedDetails}
            } else {
                $cancelButton=Get-UiaElementById $context 'classTransferCancelReview' 'ControlType.Button'
                if (-not $cancelButton.Current.IsEnabled) { throw 'Cancel Review control is disabled after package review.' }
                $route.uia.cancelAction=Invoke-VerifiedUiaClick $context $cancelButton 'cancel-next-class-transfer-review' 'class_transfer_review' 'class_transfer'
                $cancelled=Wait-DiagnosticCheckpoint $context 'review-cancelled' 1 30
                $route.operation.completedAtUtc=[string]$cancelled.captured_at_utc; $route.operation.terminalMetrics=$cancelled.metrics; $route.operation.outcome=[string]$cancelled.outcome
                if ([int]$cancelled.metrics.reviews_cancelled -ne 1 -or [bool]$cancelled.metrics.package_retained -or [bool]$cancelled.metrics.preview_retained -or [int]$cancelled.metrics.package_class_count -ne 0 -or [int]$cancelled.metrics.class_review_row_count -ne 0 -or [int]$cancelled.metrics.teacher_review_row_count -ne 0 -or [bool]$cancelled.metrics.apply_enabled) { throw 'Cancel terminal metrics do not show cleared package/review state.' }
                $route.uia.postCancelStatus=(Get-UiaElementById $context 'classTransferStatus' 'ControlType.Text' | Get-UiaSnapshot)
            }
            $route.memory.operationSamples+=,(New-ProcessSample $context ($Role+'-completed'))
        } else { throw "Unknown Class Transfer route role: $Role" }

        $route.lifecycle.preSuspendMetrics=(Get-DiagnosticRecords $context | Where-Object { $_.checkpoint -in @('export-completed','apply-completed','review-cancelled') } | Select-Object -Last 1).metrics
        $manualSuspendOrdinal=1
        if ($Role -ceq 'apply') {
            $route.lifecycle.applyAutomaticSuspension=Wait-DiagnosticCheckpoint $context 'suspended' 1 20
            if ([string]$route.lifecycle.applyAutomaticSuspension.metrics.page_instance_token -cne [string]$activated.metrics.page_instance_token -or [int]$route.lifecycle.applyAutomaticSuspension.metrics.imports_applied -ne 1) { throw 'Apply did not suspend the same Class Transfer page after its automatic Classes navigation.' }
            $route.lifecycle.applyToClassesRouteLogLine=Wait-ForCondition $context { param($ctx) Add-ProcessOutput $ctx; @($ctx.logEntries | Where-Object { $_.line -match 'navigation event=page-route outcome=completed cause=selection from=class_transfer to=classes' } | Select-Object -Last 1) } 'Apply-triggered Classes page route completion' 20 | ForEach-Object { [string]$_.line }
            $classTransferItem=Get-UiaByName $context 'Class Transfer' 'ControlType.TreeItem'
            $route.uia.navigateBackToClassTransferAfterApply=Invoke-VerifiedUiaClick $context $classTransferItem 'restore-class-transfer-before-calendar-lifecycle' 'classes' 'class_transfer'
            $route.lifecycle.preCalendarReentry=Wait-DiagnosticCheckpoint $context 'activated' 2 20
            if ([string]$route.lifecycle.preCalendarReentry.metrics.page_instance_token -cne [string]$activated.metrics.page_instance_token -or [bool]$route.lifecycle.preCalendarReentry.metrics.package_retained -or [bool]$route.lifecycle.preCalendarReentry.metrics.preview_retained -or [int]$route.lifecycle.preCalendarReentry.metrics.imports_applied -ne 1) { throw 'Apply route did not restore the same cleared Class Transfer page before its Calendar lifecycle check.' }
            $route.lifecycle.classesToClassTransferRouteLogLine=Wait-ForCondition $context { param($ctx) Add-ProcessOutput $ctx; @($ctx.logEntries | Where-Object { $_.line -match 'navigation event=page-route outcome=completed cause=selection from=classes to=class_transfer' } | Select-Object -Last 1) } 'Class Transfer restore route completion' 20 | ForEach-Object { [string]$_.line }
            $null=Wait-ForUiaElement $context 'nextClassTransferPage' 'ControlType.Group' 20
            $manualSuspendOrdinal=2
        }
        $calendar=Get-UiaByName $context 'Calendar' 'ControlType.TreeItem'
        $route.uia.navigateToCalendar=Invoke-VerifiedUiaClick $context $calendar 'suspend-class-transfer-by-calendar-navigation' 'class_transfer' 'calendar'
        $suspended=Wait-DiagnosticCheckpoint $context 'suspended' $manualSuspendOrdinal 20; $route.lifecycle.suspended=$suspended; $route.lifecycle.suspendCount=1
        if ([string]$suspended.metrics.page_instance_token -cne [string]$activated.metrics.page_instance_token) { throw "$RouteId Class Transfer page instance changed before suspend." }
        $calendarRoute=Wait-ForCondition $context { param($ctx) Add-ProcessOutput $ctx; @($ctx.logEntries | Where-Object { $_.line -match 'navigation event=page-route outcome=completed cause=selection from=class_transfer to=calendar' } | Select-Object -Last 1) } 'Calendar page route completion' 20
        $route.lifecycle.calendarRouteLogLine=[string]$calendarRoute.line
        try {
            $calendarPage=Wait-ForCondition $context { param($ctx)
                $elements=Get-AppElements $ctx | Where-Object { $_.Current.ProcessId -eq $ctx.process.Id -and -not $_.Current.IsOffscreen }
                $root=@($elements | Where-Object { $id=[string]$_.Current.AutomationId; ($id -ceq 'nextCalendarPage' -or $id.EndsWith('.nextCalendarPage',[StringComparison]::Ordinal)) })
                $title=@($elements | Where-Object { $id=[string]$_.Current.AutomationId; $id -ceq 'calendarPageTitle' -or $id.EndsWith('.calendarPageTitle',[StringComparison]::Ordinal) })
                $status=@($elements | Where-Object { $id=[string]$_.Current.AutomationId; $id -ceq 'calendarPageStatus' -or $id.EndsWith('.calendarPageStatus',[StringComparison]::Ordinal) })
                if ($root.Count -eq 1) { return $root[0] }; if ($title.Count -eq 1) { return $title[0] }; if ($status.Count -eq 1) { return $status[0] }; return $null
            } 'PID-owned visible Calendar page indicator' 20
        } catch {
            $route.uia.calendarPageTreeOnFailure=Save-UiaTree $context ("uia/"+$RouteId+"-calendar-page-failure.json")
            throw
        }
        $route.uia.calendarPageTree=Save-UiaTree $context ("uia/"+$RouteId+"-calendar-page.json")
        $route.uia.calendarPage=Get-UiaSnapshot $calendarPage
        $route.uia.calendarPageIndicatorKind=if ([string]$calendarPage.Current.AutomationId -match 'nextCalendarPage$') {'root'} elseif ([string]$calendarPage.Current.AutomationId -match 'calendarPageTitle$') {'title'} else {'status'}
        $returnItem=Get-UiaByName $context 'Class Transfer' 'ControlType.TreeItem'
        $route.uia.navigateBackToClassTransfer=Invoke-VerifiedUiaClick $context $returnItem 'reenter-class-transfer' 'calendar' 'class_transfer'
        $activationOrdinal=if ($Role -ceq 'apply') { 3 } else { 2 }
        $reentered=Wait-DiagnosticCheckpoint $context 'activated' $activationOrdinal 20; $route.lifecycle.reentered=$reentered; $route.lifecycle.reentryCount=1
        if ([string]$reentered.metrics.page_instance_token -cne [string]$activated.metrics.page_instance_token) { throw "$RouteId re-entry did not retain the same cached Class Transfer page instance." }
        if ([bool]$reentered.metrics.package_retained -or [bool]$reentered.metrics.preview_retained -or [int]$reentered.metrics.package_class_count -ne 0 -or [int]$reentered.metrics.class_review_row_count -ne 0 -or [int]$reentered.metrics.teacher_review_row_count -ne 0 -or [bool]$reentered.metrics.apply_enabled) { throw "$RouteId re-entry retained import package or review state." }
        if ($Role -ceq 'export' -and ([int]$reentered.metrics.exports_completed -ne 1 -or [int]$reentered.metrics.export_selected_class_count -ne 48)) { throw 'Export state did not survive the single Class Transfer suspend/re-entry.' }
        if ($Role -ceq 'apply' -and [int]$reentered.metrics.imports_applied -ne 1) { throw 'Applied operation counter was not retained after re-entry.' }
        if ($Role -ceq 'cancel' -and [int]$reentered.metrics.reviews_cancelled -ne 1) { throw 'Cancel operation counter was not retained after re-entry.' }
        $route.uia.reenteredPage=Get-UiaSnapshot (Get-UiaElementById $context 'nextClassTransferPage' '')
        $after=Invoke-WorkspaceSnapshot $workspacePath $afterPath $RouteId 'after'
        if ($after.integrityCheck -cne 'ok' -or [Int64]$after.foreignKeyErrorCount -ne 0) { throw "$RouteId after workspace failed integrity/foreign-key checks." }
        $route.workspace.afterTableContentSha256=[string]$after.tableContentSha256
        if ($Role -ceq 'export' -or $Role -ceq 'cancel') {
            $route.workspace.rowLevelContentsUnchanged=([string]$before.tableContentSha256 -ceq [string]$after.tableContentSha256)
            if (-not $route.workspace.rowLevelContentsUnchanged) { throw "$RouteId workspace rows changed during export/cancel." }
        } else {
            $route.workspace.beforeSnapshotTables=(Get-Content -LiteralPath $beforePath -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop).tables
            $route.workspace.afterSnapshotTables=(Get-Content -LiteralPath $afterPath -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop).tables
            $route.workspace.expectedDeltaVerified=$true
        }
        $post=New-ProcessSample $context 'post-cleanup'; Assert-StrictWorkingSet $post "$RouteId post-cleanup"; $route.memory.normalUseSamples+=,$post
        for ($i=0;$i -lt $IdleTailSamples;$i++) { Start-Sleep -Seconds 1; $tail=New-ProcessSample $context 'post-cleanup-idle-tail' $i; Assert-StrictWorkingSet $tail "$RouteId post-cleanup tail $i"; $route.memory.idleTail+=,$tail; $route.memory.normalUseSamples+=,$tail }
        $exit=Close-MeasuredProcess $context; $route.processExit=$exit; $route.processExitCode=$exit.exitCode; $route.normalExit=($exit.waitCompleted -and $exit.forcedTerminationForCleanup -eq $false -and $exit.exitCode -eq 0); $route.shutdownTraceSha256=$exit.shutdownTraceSha256; $route.shutdownDump=$exit.shutdownDump; $route.closeObservationPath=$exit.closeObservationPath; $route.closeObservationSha256=$exit.closeObservationSha256
        $terminalSuspensions=@(Get-DiagnosticRecords $context | Where-Object { $_.checkpoint -ceq 'suspended' })
        if ($terminalSuspensions.Count -gt 0) { $route.lifecycle.shutdownSuspension=$terminalSuspensions[-1] }
        if (-not $route.normalExit) {
            if (($Route2OnlyDiagnostic -or $Route2AfterExportShutdownDiagnostic -or $Route2AfterExportDumpDiagnostic) -and $Role -ceq 'apply') {
                $route.shutdownDiagnosticOutcome='route-complete-but-natural-exit-gate-failed'
            } else { throw "$RouteId process did not exit naturally with code 0." }
        } elseif (($Route2OnlyDiagnostic -or $Route2AfterExportShutdownDiagnostic -or $Route2AfterExportDumpDiagnostic) -and $Role -ceq 'apply') {
            $route.shutdownDiagnosticOutcome='natural-exit-observed'
        }
        $route.diagnosticSha256=Get-FileSha256 $diagnosticPath
        $route.stdoutSha256=Get-FileSha256 $stdoutPath; $route.stderrSha256=Get-FileSha256 $stderrPath
        return $route
    } catch {
        $route.failure=[ordered]@{message=$_.Exception.Message;category=[string]$_.CategoryInfo.Category;scriptStackTrace=[string]$_.ScriptStackTrace;capturedAtUtc=Get-UtcTimestamp}
        throw
    } finally {
        try { if ($null -eq $route.processExit) { $exit=Close-MeasuredProcess $context; $route.processExit=$exit; $route.processExitCode=$exit.exitCode; $route.normalExit=($exit.waitCompleted -and $exit.forcedTerminationForCleanup -eq $false -and $exit.exitCode -eq 0); $route.shutdownTraceSha256=$exit.shutdownTraceSha256; $route.shutdownDump=$exit.shutdownDump; $route.closeObservationPath=$exit.closeObservationPath; $route.closeObservationSha256=$exit.closeObservationSha256 } } catch { }
        try { if (Test-Path -LiteralPath $diagnosticPath -PathType Leaf) { $route.diagnosticSha256=Get-FileSha256 $diagnosticPath } } catch { }
    }
}

function Invoke-BlankWorkspaceGenerator {
    $generator=Join-Path $BuildDirectory 'Release\ClassMngrPhase7NextBlankFixtureTests.exe'
    if (-not (Test-Path -LiteralPath $generator -PathType Leaf) -or (Get-PeArchitecture $generator) -cne 'x64') { throw 'Current-source x64 blank workspace generator is missing.' }
    $envName='CLASSMNGR_PHASE7_SCHEMA_ONLY_FIXTURE_OUTPUT_PATH'; $old=[Environment]::GetEnvironmentVariable($envName,'Process'); $oldPath=[Environment]::GetEnvironmentVariable('PATH','Process'); $log=Join-Path (Join-Path $EvidenceDirectory 'logs') 'blank-workspace-generator.log'
    try { [Environment]::SetEnvironmentVariable($envName,$MasterBlankPath,'Process'); [Environment]::SetEnvironmentVariable('PATH',"$script:QtBin;$oldPath",'Process'); $output=@(& $generator 'schemaOnlyBlankWorkspaceIsValidAndDeterministic' 2>&1 | ForEach-Object {[string]$_}); $exit=$LASTEXITCODE }
    finally { [Environment]::SetEnvironmentVariable($envName,$old,'Process'); [Environment]::SetEnvironmentVariable('PATH',$oldPath,'Process') }
    [IO.File]::WriteAllLines($log,@("exitCode: $exit",$output),[Text.UTF8Encoding]::new($false)); if ($exit -ne 0 -or -not (Test-Path -LiteralPath $MasterBlankPath -PathType Leaf)) { throw 'Current-source blank workspace generator failed.' }
}

try {
    if ($env:OS -cne 'Windows_NT') { throw 'P9-11 packaged UIA route requires local Windows.' }
    $source=Get-SourceInventory
    Write-JsonFile $SourceInventoryPath ([ordered]@{root=$source.root;includedRoots=$source.includedRoots;sha256=$source.sha256;fileCount=$source.fileCount;files=$source.files})
    $receipt=Get-Content -LiteralPath $ReleaseBuildReceiptPath -Raw | ConvertFrom-Json -ErrorAction Stop
    if ($receipt.status -cne 'succeeded' -or $receipt.taskId -cne $TaskId -or [IO.Path]::GetFullPath([string]$receipt.repositoryRoot) -ine $RepositoryRoot) { throw 'Release build receipt is not a successful P9-11 receipt for this workspace.' }
    $receiptHash=Get-FileSha256 $ReleaseBuildReceiptPath
    if ($receipt.PSObject.Properties.Name -contains 'receiptSha256' -and $receiptHash -cne [string]$receipt.receiptSha256) { throw 'Release build receipt content identity mismatch.' }
    $buildDerivation=$null
    $releaseExecutablePath=[IO.Path]::GetFullPath([string]$receipt.releaseExecutablePath)
    if ([string]$receipt.sourceSha256 -cne [string]$source.sha256) {
        $buildDerivation=New-ScriptOnlyBuildDerivation $receipt $ReleaseBuildReceiptPath $source
        $releaseExecutablePath=[IO.Path]::GetFullPath([string]$buildDerivation.releaseExecutablePath)
    } else {
        $releaseExecutableExists=Test-Path -LiteralPath $releaseExecutablePath -PathType Leaf
        $releaseExecutableSha256=if ($releaseExecutableExists) { Get-FileSha256 $releaseExecutablePath } else { '' }
        $releaseExecutableArchitecture=if ($releaseExecutableExists) { Get-PeArchitecture $releaseExecutablePath } else { '' }
        if (-not $releaseExecutableExists -or $releaseExecutableSha256 -cne [string]$receipt.releaseExecutableSha256 -or $releaseExecutableArchitecture -cne 'x64') { throw 'Release executable does not match its build receipt.' }
    }
    if ((Get-FileSha256 $releaseExecutablePath) -cne [string]$receipt.releaseExecutableSha256 -or (Get-PeArchitecture $releaseExecutablePath) -cne 'x64') { throw 'Selected packaged executable does not match the original Release receipt.' }
    $script:Manifest.source=[ordered]@{root=$source.root;sha256=$source.sha256;fileCount=$source.fileCount;includedRoots=$source.includedRoots;inventoryPath=[IO.Path]::GetFullPath($SourceInventoryPath);inventoryFileSha256=Get-FileSha256 $SourceInventoryPath}
    $script:Manifest.releaseBuildReceipt=[ordered]@{path=$ReleaseBuildReceiptPath;sha256=$receiptHash;buildSourceSha256=[string]$receipt.sourceSha256;currentSourceSha256=[string]$source.sha256;buildSourceInventoryPath=[string]$receipt.sourceInventoryPath;releaseExecutableSha256=[string]$receipt.releaseExecutableSha256;generatedResourcesSha256=[string]$receipt.generatedResources.stagedResourceInputs.sha256;stagedResourceRoot=[string]$receipt.generatedResources.verifiedStagingRoot;derivedReceiptPath=if($null -ne $buildDerivation){'build-derivation/derived-receipt.json'}else{$null};derivedReceiptSha256=if($null -ne $buildDerivation){[string]$buildDerivation.sha256}else{$null}}
    if ($null -ne $buildDerivation) { $script:Manifest.releaseBuildReceipt.sourceDeltaPaths=@($buildDerivation.record.source.changedFiles | ForEach-Object { [string]$_.path }) }
    $script:Manifest.build=[ordered]@{directory=[string]$receipt.buildDirectory;configuration='Release';platform='x64';generator=[string]$receipt.generator;cacheSha256After=[string]$receipt.cacheSha256After;targets=@($receipt.targets);configure=$receipt.configure;build=$receipt.build}
    $packageDirectory=Join-Path $EvidenceDirectory 'package'; $appPath=Join-Path $packageDirectory 'ClassMngrNext.exe'
    if ($null -ne $buildDerivation) {
        foreach($row in $buildDerivation.packageInventory.files){
            $relative=[string]$row.path;$sourceFile=[IO.Path]::GetFullPath((Join-Path $buildDerivation.packageRoot ($relative.Replace('/',[IO.Path]::DirectorySeparatorChar))))
            $packageBase=$buildDerivation.packageRoot.TrimEnd([char[]]@('\','/'))+[IO.Path]::DirectorySeparatorChar
            if(-not $sourceFile.StartsWith($packageBase,[StringComparison]::OrdinalIgnoreCase)){throw "Package copy path escaped preserved package root: $relative"}
            $targetFile=[IO.Path]::GetFullPath((Join-Path $packageDirectory ($relative.Replace('/',[IO.Path]::DirectorySeparatorChar))))
            $targetBase=[IO.Path]::GetFullPath($packageDirectory).TrimEnd([char[]]@('\','/'))+[IO.Path]::DirectorySeparatorChar
            if(-not $targetFile.StartsWith($targetBase,[StringComparison]::OrdinalIgnoreCase)){throw "Package copy path escaped new package root: $relative"}
            [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($targetFile));[IO.File]::Copy($sourceFile,$targetFile,$false)
        }
    } else {
        Copy-Item -LiteralPath ([string]$receipt.releaseExecutablePath) -Destination $appPath
        $stagedRoot=[string]$receipt.generatedResources.verifiedStagingRoot
        $stagedInventory=Get-CanonicalInventory $stagedRoot
        if ($stagedInventory.sha256 -cne [string]$receipt.generatedResources.stagedResourceInputs.sha256 -or $stagedInventory.fileCount -ne [int]$receipt.generatedResources.stagedResourceInputs.fileCount) { throw 'Staged verified resources differ from the Release build receipt.' }
        $stagedResources=Join-Path $stagedRoot 'resources'; if (-not (Test-Path -LiteralPath $stagedResources -PathType Container)) { throw 'Verified build receipt staging lacks resources root.' }
        Copy-Item -LiteralPath $stagedResources -Destination $packageDirectory -Recurse
    }
    $qtCache=Join-Path $BuildDirectory 'CMakeCache.txt'; $cache=Get-Content -LiteralPath $qtCache
    $qtLine=@($cache | Where-Object {$_ -match '^Qt6_DIR:[^=]+='}); if ($qtLine.Count -ne 1) { throw 'Release cache has no unique Qt6_DIR.' }; $qtDir=($qtLine[0] -split '=',2)[1]; $script:QtPrefix=[IO.Path]::GetFullPath((Join-Path $qtDir '..\..\..')); $script:QtBin=Join-Path $script:QtPrefix 'bin'; $windeploy=Join-Path $script:QtBin 'windeployqt.exe'
    if ($null -eq $buildDerivation) {
        $deploy=Invoke-Captured $windeploy @('--qmldir',(Join-Path $RepositoryRoot 'src\features\calendar\ui\qml'),'--skip-plugin-types','qmltooling','--include-plugins','qoffscreen,qsqlite','--exclude-plugins','qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql','--translations','en,ko','--compiler-runtime','--dir',$packageDirectory,$appPath) (Join-Path (Join-Path $EvidenceDirectory 'logs') 'windeployqt.log') 'Stage current-source Qt runtime'
        if ([int]$deploy.exitCode -ne 0) { throw 'windeployqt failed.' }
    }
    foreach ($required in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Sql.dll','platforms\qwindows.dll','sqldrivers\qsqlite.dll','resources\resource-catalog.json')) { if (-not (Test-Path -LiteralPath (Join-Path $packageDirectory $required) -PathType Leaf)) { throw "Packaged current-source app lacks $required." } }
    $packageInventory=Get-CanonicalInventory $packageDirectory
    if ($null -ne $buildDerivation -and ($packageInventory.sha256 -cne [string]$buildDerivation.record.priorPackage.inventorySha256 -or $packageInventory.fileCount -ne [int]$buildDerivation.record.priorPackage.fileCount)) { throw 'Copied package does not match the independently verified preserved package inventory.' }
    Write-JsonFile $PackageInventoryPath ([ordered]@{root=$packageInventory.root;sha256=$packageInventory.sha256;fileCount=$packageInventory.fileCount;files=$packageInventory.files})
    $script:Manifest.application=[ordered]@{canonicalPath=[IO.Path]::GetFullPath($appPath);sha256=Get-FileSha256 $appPath;architecture='x64';releaseExecutableSha256=[string]$receipt.releaseExecutableSha256;candidateSourceSha256=$source.sha256}
    $script:Manifest.package=[ordered]@{directory=[IO.Path]::GetFullPath($packageDirectory);sha256=$packageInventory.sha256;fileCount=$packageInventory.fileCount;applicationRelativePath='ClassMngrNext.exe';applicationSha256=Get-FileSha256 $appPath;candidateSourceSha256=$source.sha256;reusedPriorPackage=($null -ne $buildDerivation);reusedPackageSource=if($null -ne $buildDerivation){[ordered]@{root=[string]$buildDerivation.record.priorPackage.packageRoot;inventorySha256=[string]$buildDerivation.record.priorPackage.inventorySha256;derivedReceiptPath='build-derivation/derived-receipt.json'}}else{$null};runtimeFiles=@(Get-ChildItem -LiteralPath $packageDirectory -File -Recurse | ForEach-Object {[ordered]@{path=$_.FullName.Substring($packageDirectory.Length+1).Replace('\','/');sizeBytes=[Int64]$_.Length;sha256=Get-FileSha256 $_.FullName}})}
    $blankReceiptLog=Join-Path (Join-Path $EvidenceDirectory 'logs') 'blank-workspace-generator-index.log'; Invoke-BlankWorkspaceGenerator
    $masterBlankSha=Get-FileSha256 $MasterBlankPath; $script:Manifest.masterWorkspace=[ordered]@{path=[IO.Path]::GetFullPath($MasterBlankPath);sha256=$masterBlankSha;seedSqlPath=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot $SeedSqlRelative));seedSqlSha256=Get-FileSha256 (Join-Path $RepositoryRoot $SeedSqlRelative)}
    $script:Manifest.routes=@()
    if ($Route2OnlyDiagnostic) {
        $applyPrep=Invoke-WorkspacePreparation 'apply_destination' 'route-2-apply-destination.tps'
        $script:Manifest.route2DiagnosticPackageSource=Copy-VerifiedRoute2DiagnosticPackage $Route2DiagnosticSourceManifestPath
        $r2=Invoke-ClassTransferRoute 'route-2-next-apply' 'apply' $applyPrep
        $script:Manifest.status='diagnostic-collected'; $script:Manifest.completedAtUtc=Get-UtcTimestamp
    } elseif ($Route2AfterExportShutdownDiagnostic -or $Route2AfterExportDumpDiagnostic) {
        $sourcePrep=Invoke-WorkspacePreparation 'source' 'route-1-source.tps'
        $applyPrep=Invoke-WorkspacePreparation 'apply_destination' 'route-2-apply-destination.tps'
        if ($Route2AfterExportDumpDiagnostic) { $null=Invoke-WorkspacePreparation 'cancel_destination' 'route-3-cancel-destination.tps' }
        $r1=Invoke-ClassTransferRoute 'route-1-next-export' 'export' $sourcePrep
        $packageHash=Get-FileSha256 $ExportedPackagePath; $packageBytes=[Int64](Get-Item -LiteralPath $ExportedPackagePath).Length
        $script:Manifest.exportedPackage=[ordered]@{path=[IO.Path]::GetFullPath($ExportedPackagePath);sha256=$packageHash;sizeBytes=$packageBytes;createdByRouteId=$r1.routeId;createdByPid=[Int64]$r1.processIdentity.pid}
        $r2=Invoke-ClassTransferRoute 'route-2-next-apply' 'apply' $applyPrep
        $script:Manifest.status='diagnostic-collected'; $script:Manifest.completedAtUtc=Get-UtcTimestamp
    } else {
        $sourcePrep=Invoke-WorkspacePreparation 'source' 'route-1-source.tps'
        $applyPrep=Invoke-WorkspacePreparation 'apply_destination' 'route-2-apply-destination.tps'
        $cancelPrep=Invoke-WorkspacePreparation 'cancel_destination' 'route-3-cancel-destination.tps'
        $r1=Invoke-ClassTransferRoute 'route-1-next-export' 'export' $sourcePrep
        $packageHash=Get-FileSha256 $ExportedPackagePath; $packageBytes=[Int64](Get-Item -LiteralPath $ExportedPackagePath).Length
        $script:Manifest.exportedPackage=[ordered]@{path=[IO.Path]::GetFullPath($ExportedPackagePath);sha256=$packageHash;sizeBytes=$packageBytes;createdByRouteId=$r1.routeId;createdByPid=[Int64]$r1.processIdentity.pid}
        $r2=Invoke-ClassTransferRoute 'route-2-next-apply' 'apply' $applyPrep
        $r3=Invoke-ClassTransferRoute 'route-3-next-cancel' 'cancel' $cancelPrep
        $script:Manifest.status='evidence-collected'; $script:Manifest.completedAtUtc=Get-UtcTimestamp
    }
} catch {
    $script:Manifest.status='failed'; $script:Manifest.completedAtUtc=Get-UtcTimestamp
    $script:Manifest.failure=[ordered]@{message=$_.Exception.Message;category=[string]$_.CategoryInfo.Category;scriptStackTrace=[string]$_.ScriptStackTrace;capturedAtUtc=Get-UtcTimestamp}
} finally {
    foreach ($context in $script:Processes) { try { $context.process.Refresh(); if (-not $context.process.HasExited) { $null=Close-MeasuredProcess $context } } catch { } }
    $script:Manifest.routes=@($script:Manifest.routes)
    Write-JsonFile $ManifestPath $script:Manifest
    $validator=Join-Path $PSScriptRoot 'validate_packaged_next_class_transfer.py'
    if ($script:Manifest.status -ceq 'evidence-collected' -and (Test-Path -LiteralPath $validator -PathType Leaf)) {
        $output=@(& python $validator --evidence-dir $EvidenceDirectory 2>&1 | ForEach-Object {[string]$_}); $exit=$LASTEXITCODE; [IO.File]::WriteAllLines($ValidatorOutputPath,$output,[Text.UTF8Encoding]::new($false))
        $script:Manifest.validation=[ordered]@{status=if($exit -eq 0){'passed'}else{'failed'};exitCode=$exit;validatorPath=[IO.Path]::GetFullPath($validator);validatorSha256=Get-FileSha256 $validator;outputPath=[IO.Path]::GetFullPath($ValidatorOutputPath);outputSha256=Get-FileSha256 $ValidatorOutputPath;output=$output}
        if ($exit -eq 0) {$script:Manifest.status='passed'} else {$script:Manifest.status='failed-validation'}
    } else {
        [IO.File]::WriteAllText($ValidatorOutputPath,"Validation not run: evidence collection did not complete or validator is missing.`r`n",[Text.UTF8Encoding]::new($false))
        $script:Manifest.validation=[ordered]@{status='not-evaluated';path=[IO.Path]::GetFullPath($ValidatorOutputPath);sha256=Get-FileSha256 $ValidatorOutputPath}
    }
    Write-JsonFile $ManifestPath $script:Manifest
}
if (($Route2OnlyDiagnostic -or $Route2AfterExportShutdownDiagnostic -or $Route2AfterExportDumpDiagnostic) -and $script:Manifest.status -ceq 'diagnostic-collected') { Write-Output "P9-11 Route 2 shutdown diagnostic evidence collected: $EvidenceDirectory"; exit 0 }
if ($script:Manifest.status -cne 'passed') { [Console]::Error.WriteLine("P9-11 packaged Class Transfer incomplete/failed. Evidence preserved at $EvidenceDirectory."); exit 1 }
Write-Output "P9-11 packaged Next Class Transfer validated: $EvidenceDirectory"
