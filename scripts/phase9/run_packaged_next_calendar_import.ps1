<#
.SYNOPSIS
Runs packaged Next Calendar Import success and parser-error routes on Windows.

.DESCRIPTION
Builds and stages the current-source x64 Release package, seeds two byte-identical
large-startup workspaces, and performs one authentic UIA import per fresh process.
Every fixture response is served by an in-process loopback server. Evidence and
failed attempts are retained under the run-specific evidence directory.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = "",
    [string]$BuildDirectory = "build\windows-x64-release",
    [string]$EvidenceRoot = "artifacts\phase9\packaged-next-calendar-import",
    [switch]$RouteAOnly,
    [string]$PrebuiltBuildReceiptPath = "",
    [ValidateRange(30, 300)][int]$ReadinessTimeoutSeconds = 120,
    [ValidateRange(1, 30)][int]$IdleTailSamples = 10
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$TaskId = "P9_10_PACKAGED_CALENDAR_IMPORT_ACCEPTANCE"
$ManifestSchema = "classmngr-phase9-packaged-next-calendar-import-v1"
$WorkingSetLimit = [Int64]262144000
$OperationPeakLimit = [Int64]536870912
$SuccessFixtureRelative = "docs\qt-rewrite\visual-baseline\release\large-calendar-import-boundary\generated-large-calendar-import.xlsx"
$ErrorFixtureRelative = "docs\qt-rewrite\visual-baseline\release\large-calendar-import-error-boundary\malformed-calendar-import-response.bin"
$SeedSqlRelative = "tests\fixtures\workspaces\large_startup.sql"
$SourceRoots = @(
    "CMakeLists.txt", "CMakePresets.json", "cmake", "resources", "scripts\phase9",
    "src", "tests",
    "docs\qt-rewrite\visual-baseline\release\large-calendar-import-boundary",
    "docs\qt-rewrite\visual-baseline\release\large-calendar-import-error-boundary"
)
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
foreach ($directory in @("package", "fixtures", "logs", "uia", "workspace-snapshots", "runtime")) {
    [void][IO.Directory]::CreateDirectory((Join-Path $EvidenceDirectory $directory))
}

$ManifestPath = Join-Path $EvidenceDirectory "manifest.json"
$SourceInventoryPath = Join-Path $EvidenceDirectory "source-files.json"
$PackageInventoryPath = Join-Path $EvidenceDirectory "package-files.json"
$BuildLogPath = Join-Path (Join-Path $EvidenceDirectory "logs") "release-build.log"
$ConfigureLogPath = Join-Path (Join-Path $EvidenceDirectory "logs") "release-configure.log"
$DeployLogPath = Join-Path (Join-Path $EvidenceDirectory "logs") "windeployqt.log"
$ServerLogPath = Join-Path $EvidenceDirectory "loopback-server.jsonl"
$ProcessMetricsPath = Join-Path $EvidenceDirectory "process-metrics.jsonl"
$UiaActionsPath = Join-Path $EvidenceDirectory "uia-actions.jsonl"
$StartupLogPath = Join-Path $EvidenceDirectory "startup-log.jsonl"
$ValidatorOutputPath = Join-Path (Join-Path $EvidenceDirectory "logs") "validator-output.txt"
$Manifest = [ordered]@{
    schema = $ManifestSchema
    taskId = $TaskId
    runId = $RunId
    status = "not-evaluated"
    routeMode = "current-source-packaged-next-on-local-windows"
    routePlan = if ($RouteAOnly) { "route-a-diagnostic-only" } else { "two-route-acceptance" }
    networkPolicy = "loopback-only; startup updater disabled in the test package"
    startedAtUtc = [DateTimeOffset]::UtcNow.ToString("o")
    completedAtUtc = $null
    repositoryRoot = $RepositoryRoot
    revision = $null
    runnerIdentity = $null
    validatorIdentity = $null
    source = $null
    sourceInventoryPath = [IO.Path]::GetFullPath($SourceInventoryPath)
    packageInventoryPath = [IO.Path]::GetFullPath($PackageInventoryPath)
    package = $null
    application = $null
    build = $null
    qt = $null
    fixtures = @()
    workspaceSeed = $null
    serverLogPath = [IO.Path]::GetFullPath($ServerLogPath)
    processMetricsPath = [IO.Path]::GetFullPath($ProcessMetricsPath)
    uiaActionsPath = [IO.Path]::GetFullPath($UiaActionsPath)
    startupLogPath = [IO.Path]::GetFullPath($StartupLogPath)
    thresholds = [ordered]@{
        normalUseWorkingSetBytesExclusive = $WorkingSetLimit
        operationPeakWorkingSetBytesAdvisoryExclusive = $OperationPeakLimit
        operationPeakEvaluation = "advisory-only; no private-byte return allowance is defined"
        privateBytesReturnAllowance = $null
        strictAtOrAboveFails = $true
    }
    routes = @()
    failure = $null
    validation = $null
}
$script:Processes = [Collections.Generic.List[object]]::new()
$script:QtPrefix = ""
$script:QtBin = ""
$script:RepositoryRoot = $RepositoryRoot
$script:BuildDirectory = $BuildDirectory
$script:EvidenceDirectory = $EvidenceDirectory
$script:Manifest = $Manifest
$script:UiaActionsPath = $UiaActionsPath
$script:ProcessMetricsPath = $ProcessMetricsPath
$script:StartupLogPath = $StartupLogPath
$script:VerifiedResourceInputsRoot = ""
$script:BuildSourceSha256 = ""
$script:LastShutdownObservability = $null

Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Reflection;
using System.Collections.Generic;
using System.IO;

public sealed class P910LoopbackFixtureServer : IDisposable
{
    private readonly TcpListener listener;
    private readonly byte[] payload;
    private readonly Thread worker;
    private volatile bool disposed;
    private readonly object gate = new object();
    private TcpClient activeClient;
    private bool aborted;
    private int requestCount;
    private int responseBytesSent;
    private string requestPath = "";
    private string remoteAddress = "";
    private string error = "";

    public readonly ManualResetEventSlim RequestSeen = new ManualResetEventSlim(false);
    public readonly ManualResetEventSlim ResponseCompleted = new ManualResetEventSlim(false);

    public P910LoopbackFixtureServer(byte[] body)
    {
        payload = (byte[])body.Clone();
        listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        Port = ((IPEndPoint)listener.LocalEndpoint).Port;
        Url = "http://127.0.0.1:" + Port.ToString() + "/calendar-import";
        worker = new Thread(Serve) { IsBackground = true, Name = "P9-10 loopback fixture server" };
        worker.Start();
    }

    public int Port { get; private set; }
    public string Url { get; private set; }
    public int RequestCount { get { lock (gate) return requestCount; } }
    public int ResponseBytesSent { get { lock (gate) return responseBytesSent; } }
    public string RequestPath { get { lock (gate) return requestPath; } }
    public string RemoteAddress { get { lock (gate) return remoteAddress; } }
    public string Error { get { lock (gate) return error; } }
    public bool IsAborted { get { lock (gate) return aborted; } }
    public string ResponseSha256
    {
        get
        {
            using (SHA256 sha = SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(payload)).Replace("-", "").ToLowerInvariant();
        }
    }

    private void Serve()
    {
        TcpClient client = null;
        try
        {
            client = listener.AcceptTcpClient();
            lock (gate) activeClient = client;
            using (client)
            using (NetworkStream stream = client.GetStream())
            {
                client.ReceiveTimeout = 10000;
                client.SendTimeout = 10000;
                StringBuilder headers = new StringBuilder();
                int matched = 0;
                while (headers.Length < 32768)
                {
                    int value = stream.ReadByte();
                    if (value < 0) break;
                    headers.Append((char)value);
                    matched = matched == 0 && value == '\r' ? 1
                        : matched == 1 && value == '\n' ? 2
                        : matched == 2 && value == '\r' ? 3
                        : matched == 3 && value == '\n' ? 4
                        : value == '\r' ? 1 : 0;
                    if (matched == 4) break;
                }
                string[] lines = headers.ToString().Split(new[] { "\r\n" }, StringSplitOptions.None);
                string[] request = lines.Length == 0 ? new string[0] : lines[0].Split(' ');
                string path = request.Length >= 2 ? request[1] : "";
                string remote = ((IPEndPoint)client.Client.RemoteEndPoint).Address.ToString();
                lock (gate)
                {
                    ++requestCount;
                    requestPath = path;
                    remoteAddress = remote;
                }
                RequestSeen.Set();

                if (request.Length >= 3 && request[0] == "GET" && path == "/calendar-import")
                {
                    byte[] responseHeader = Encoding.ASCII.GetBytes(
                        "HTTP/1.1 200 OK\r\nContent-Type: application/vnd.openxmlformats-officedocument.spreadsheetml.sheet\r\n" +
                        "Content-Length: " + payload.Length.ToString() + "\r\nConnection: close\r\n\r\n");
                    stream.Write(responseHeader, 0, responseHeader.Length);
                    const int chunkSize = 4096;
                    int sent = 0;
                    while (sent < payload.Length)
                    {
                        if (IsAborted) return;
                        int count = Math.Min(chunkSize, payload.Length - sent);
                        stream.Write(payload, sent, count);
                        stream.Flush();
                        sent += count;
                        lock (gate) responseBytesSent = sent;
                        Thread.Sleep(15);
                    }
                    client.Client.Shutdown(SocketShutdown.Send);
                }
                else
                {
                    byte[] responseHeader = Encoding.ASCII.GetBytes(
                        "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    stream.Write(responseHeader, 0, responseHeader.Length);
                    stream.Flush();
                }
            }
        }
        catch (Exception exception)
        {
            lock (gate)
            {
                if (!aborted) error = exception.ToString();
            }
        }
        finally
        {
            lock (gate) activeClient = null;
            ResponseCompleted.Set();
        }
    }

    public void Abort()
    {
        TcpClient client;
        lock (gate)
        {
            if (aborted) return;
            aborted = true;
            client = activeClient;
        }
        try { if (client != null) client.Close(); } catch { }
        try { listener.Stop(); } catch { }
    }

    public void Dispose()
    {
        if (disposed) return;
        disposed = true;
        if (!ResponseCompleted.IsSet) Abort();
        try { listener.Stop(); } catch { }
        try { if (worker.IsAlive) worker.Join(5000); } catch { }
        RequestSeen.Dispose();
        ResponseCompleted.Dispose();
    }
}

public sealed class P910TopLevelWindowInfo
{
    public long Handle { get; set; }
    public int ProcessId { get; set; }
    public string Title { get; set; }
    public string ClassName { get; set; }
    public bool Visible { get; set; }
    public bool Enabled { get; set; }
}

public static class P910Native
{
    private const ushort ImageFileMachineUnknown = 0x0000;
    private const ushort ImageFileMachineAmd64 = 0x8664;
    private const uint MouseEventLeftDown = 0x0002;
    private const uint MouseEventLeftUp = 0x0004;
    private const uint GetAncestorRoot = 2;
    private const uint WmMouseMove = 0x0200;
    private const uint WmLeftButtonDown = 0x0201;
    private const uint WmLeftButtonUp = 0x0202;
    private const uint MiniDumpWithThreadInfo = 0x00001000;
    private delegate bool EnumWindowsCallback(IntPtr window, IntPtr parameter);

    [StructLayout(LayoutKind.Sequential)]
    private struct Point { public int X; public int Y; }

    [StructLayout(LayoutKind.Sequential)]
    private struct Rect { public int Left; public int Top; public int Right; public int Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    private struct MouseInput
    {
        public int dx; public int dy; public uint mouseData; public uint dwFlags;
        public uint time; public IntPtr dwExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Input { public uint type; public MouseInput mouseInput; }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct ProcessMemoryCountersEx
    {
        public uint cb; public uint pageFaultCount; public UIntPtr peakWorkingSetSize;
        public UIntPtr workingSetSize; public UIntPtr quotaPeakPagedPoolUsage;
        public UIntPtr quotaPagedPoolUsage; public UIntPtr quotaPeakNonPagedPoolUsage;
        public UIntPtr quotaNonPagedPoolUsage; public UIntPtr pagefileUsage;
        public UIntPtr peakPagefileUsage; public UIntPtr privateUsage;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool IsWow64Process2(IntPtr process, out ushort processMachine, out ushort nativeMachine);
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool QueryFullProcessImageNameW(IntPtr process, uint flags, StringBuilder imageName, ref int size);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("kernel32.dll")]
    private static extern uint GetCurrentThreadId();
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool IsWindowEnabled(IntPtr window);
    [DllImport("user32.dll", SetLastError = true, EntryPoint = "EnumWindows")]
    private static extern bool EnumWindowsNative(EnumWindowsCallback callback, IntPtr parameter);
    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "GetWindowTextW")]
    private static extern int GetWindowTextNative(IntPtr window, StringBuilder text, int maximumCount);
    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "GetClassNameW")]
    private static extern int GetClassNameNative(IntPtr window, StringBuilder className, int maximumCount);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr WindowFromPoint(Point point);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr GetAncestor(IntPtr window, uint flags);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool AttachThreadInput(uint attachThreadId, uint attachToThreadId, bool attach);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool ScreenToClient(IntPtr window, ref Point point);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool GetClientRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll", SetLastError = true, EntryPoint = "SendMessageW")]
    private static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint SendInput(uint inputCount, Input[] inputs, int inputSize);

    [DllImport("dbghelp.dll", SetLastError = true, EntryPoint = "MiniDumpWriteDump")]
    private static extern bool MiniDumpWriteDumpNative(
        IntPtr processHandle,
        uint processId,
        IntPtr dumpFileHandle,
        uint dumpType,
        IntPtr exceptionParam,
        IntPtr userStreamParam,
        IntPtr callbackParam);

    public static long WriteNormalThreadInfoMiniDump(
        IntPtr processHandle, int processId, string path)
    {
        if (processHandle == IntPtr.Zero || processId <= 0)
            throw new ArgumentException("A live measured process handle and PID are required.");
        if (String.IsNullOrWhiteSpace(path))
            throw new ArgumentException("A unique dump artifact path is required.");

        using (FileStream dumpFile = new FileStream(
            path, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
        {
            if (!MiniDumpWriteDumpNative(
                processHandle,
                unchecked((uint)processId),
                dumpFile.SafeFileHandle.DangerousGetHandle(),
                MiniDumpWithThreadInfo,
                IntPtr.Zero,
                IntPtr.Zero,
                IntPtr.Zero))
            {
                int error = Marshal.GetLastWin32Error();
                throw new Win32Exception(
                    error,
                    "MiniDumpWriteDump(MiniDumpNormal | MiniDumpWithThreadInfo) failed.");
            }
            dumpFile.Flush(true);
            return dumpFile.Length;
        }
    }

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
    public static int GetWindowProcessId(IntPtr window)
    {
        uint processId;
        if (GetWindowThreadProcessId(window, out processId) == 0)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowThreadProcessId failed.");
        return unchecked((int)processId);
    }
    public static bool IsWindowVisibleForProcess(IntPtr window) { return IsWindowVisible(window); }
    public static string[] GetUiaWindowInfoWithTimeout(long windowHandle, int timeoutMilliseconds, string automationAssemblyPath)
    {
        string[] result = new string[] { "completed", "", "", "", "" };
        Task probe = Task.Factory.StartNew(delegate
        {
            try
            {
                Assembly automationAssembly = Assembly.LoadFrom(automationAssemblyPath);
                Type automationElementType = automationAssembly.GetType("System.Windows.Automation.AutomationElement", true);
                MethodInfo fromHandle = automationElementType.GetMethod("FromHandle", BindingFlags.Public | BindingFlags.Static, null, new Type[] { typeof(IntPtr) }, null);
                object element = fromHandle.Invoke(null, new object[] { new IntPtr(windowHandle) });
                object current = element.GetType().GetProperty("Current").GetValue(element, null);
                Type currentType = current.GetType();
                object controlType = currentType.GetProperty("ControlType").GetValue(current, null);
                result[1] = Convert.ToString(currentType.GetProperty("Name").GetValue(current, null));
                result[2] = Convert.ToString(controlType.GetType().GetProperty("ProgrammaticName").GetValue(controlType, null));
                result[3] = Convert.ToString(currentType.GetProperty("AutomationId").GetValue(current, null));
                result[4] = Convert.ToString(currentType.GetProperty("ClassName").GetValue(current, null));
            }
            catch (Exception exception)
            {
                result[0] = "error";
                result[4] = exception.GetType().FullName + ": " + exception.Message;
            }
        });
        try
        {
            if (!probe.Wait(timeoutMilliseconds))
                return new string[] { "timed-out", "", "", "", "UI Automation window query exceeded " + timeoutMilliseconds.ToString() + " ms." };
            return result;
        }
        catch (AggregateException exception)
        {
            return new string[] { "error", "", "", "", exception.GetType().FullName + ": " + exception.Message };
        }
    }
    public static P910TopLevelWindowInfo[] GetTopLevelWindowsForProcess(int targetProcessId)
    {
        List<P910TopLevelWindowInfo> windows = new List<P910TopLevelWindowInfo>();
        EnumWindowsCallback callback = delegate(IntPtr window, IntPtr parameter)
        {
            uint processId;
            if (GetWindowThreadProcessId(window, out processId) != 0 && processId == (uint)targetProcessId)
            {
                StringBuilder title = new StringBuilder(1024);
                StringBuilder className = new StringBuilder(512);
                GetWindowTextNative(window, title, title.Capacity);
                GetClassNameNative(window, className, className.Capacity);
                windows.Add(new P910TopLevelWindowInfo
                {
                    Handle = window.ToInt64(), ProcessId = targetProcessId,
                    Title = title.ToString(), ClassName = className.ToString(),
                    Visible = IsWindowVisible(window), Enabled = IsWindowEnabled(window)
                });
            }
            return true;
        };
        EnumWindowsNative(callback, IntPtr.Zero);
        return windows.ToArray();
    }
    public static long ClickVerifiedPoint(int x, int y, int expectedProcessId, long expectedMainWindowHandle)
    {
        IntPtr expectedMain = new IntPtr(expectedMainWindowHandle);
        if (expectedMain == IntPtr.Zero || GetWindowProcessId(expectedMain) != expectedProcessId)
            throw new InvalidOperationException("Measured app HWND no longer belongs to the expected process.");
        Point clientPoint = new Point { X = x, Y = y };
        Rect clientRect = new Rect();
        if (!ScreenToClient(expectedMain, ref clientPoint)
            || !GetClientRect(expectedMain, out clientRect)
            || clientPoint.X < clientRect.Left || clientPoint.X >= clientRect.Right
            || clientPoint.Y < clientRect.Top || clientPoint.Y >= clientRect.Bottom)
            throw new InvalidOperationException(String.Format(
                "UIA-derived click point lies outside the measured HWND client area (screen={0},{1}, client={2},{3}, bounds={4},{5},{6},{7}).",
                x, y, clientPoint.X, clientPoint.Y,
                clientRect.Left, clientRect.Top, clientRect.Right, clientRect.Bottom));
        IntPtr packedPoint = new IntPtr((clientPoint.Y << 16) | (clientPoint.X & 0xffff));
        SendMessage(expectedMain, WmMouseMove, IntPtr.Zero, packedPoint);
        SendMessage(expectedMain, WmLeftButtonDown, new IntPtr(1), packedPoint);
        SendMessage(expectedMain, WmLeftButtonUp, IntPtr.Zero, packedPoint);
        return expectedMainWindowHandle;
    }
}
'@

function Get-UtcTimestamp { [DateTimeOffset]::UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss.fff'Z'") }
function Get-FileSha256 { param([string]$Path) (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Write-JsonFile { param([string]$Path, [object]$Value) [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 40), [Text.UTF8Encoding]::new($false)) }
function Add-JsonLine { param([string]$Path, [object]$Value) [IO.File]::AppendAllText($Path, ($Value | ConvertTo-Json -Depth 40 -Compress) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false)) }
function Get-StringSha256 {
    param([string]$Value)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value)))).Replace("-", "").ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Get-CanonicalInventory {
    param([string]$Root)
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd([char[]]@('\', '/'))
    $prefix = $rootFull + [IO.Path]::DirectorySeparatorChar
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($file in @(Get-ChildItem -LiteralPath $rootFull -File -Recurse)) {
        if ($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @('.pyc', '.pyo')) { continue }
        $canonical = [IO.Path]::GetFullPath($file.FullName)
        if (-not $canonical.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "Inventory path escaped root: $canonical" }
        $relative = $canonical.Substring($prefix.Length).Replace('\', '/')
        $rows.Add([ordered]@{ path = $relative; sizeBytes = [Int64]$file.Length; sha256 = Get-FileSha256 $canonical })
    }
    $rows.Sort([System.Comparison[object]]{ param($a, $b) [StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(), ([string]$b.path).ToLowerInvariant()) })
    $sorted = @($rows.ToArray())
    $canonicalRows = @($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    [ordered]@{ root = $rootFull; fileCount = $sorted.Count; sha256 = Get-StringSha256 ($canonicalRows + "`n"); files = $sorted }
}
function Get-SourceInventory {
    param([string]$Root)
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd([char[]]@('\', '/'))
    $prefix = $rootFull + [IO.Path]::DirectorySeparatorChar
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($relativeRoot in $SourceRoots) {
        $path = Join-Path $rootFull $relativeRoot
        if (-not (Test-Path -LiteralPath $path)) { throw "Source inventory root is missing: $relativeRoot" }
        $files = if (Test-Path -LiteralPath $path -PathType Leaf) { @(Get-Item -LiteralPath $path) } else { @(Get-ChildItem -LiteralPath $path -File -Recurse) }
        foreach ($file in $files) {
            if ($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @('.pyc', '.pyo')) { continue }
            $canonical = [IO.Path]::GetFullPath($file.FullName)
            if (-not $canonical.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "Source inventory path escaped repository: $canonical" }
            $relative = $canonical.Substring($prefix.Length).Replace('\', '/')
            $rows.Add([ordered]@{ path = $relative; sizeBytes = [Int64]$file.Length; sha256 = Get-FileSha256 $canonical })
        }
    }
    $rows.Sort([System.Comparison[object]]{ param($a, $b) [StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(), ([string]$b.path).ToLowerInvariant()) })
    $sorted = @($rows.ToArray())
    $canonicalRows = @($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    [ordered]@{ root = $rootFull; includedRoots = $SourceRoots; fileCount = $sorted.Count; sha256 = Get-StringSha256 ($canonicalRows + "`n"); files = $sorted }
}
function Get-CacheValue {
    param([string[]]$Lines, [string]$Name)
    $matches = @($Lines | Where-Object { $_ -match "^$([regex]::Escape($Name)):[^=]+=" })
    if ($matches.Count -ne 1) { throw "Expected one CMake cache entry '$Name', found $($matches.Count)." }
    ($matches[0] -split "=", 2)[1]
}
function Get-PeArchitecture {
    param([string]$Path)
    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Bad MZ header: $Path" }
        $stream.Position = 0x3c; $offset = $reader.ReadInt32(); $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550) { throw "Bad PE header: $Path" }
        switch ($reader.ReadUInt16()) { 0x8664 { "x64" } 0xaa64 { "arm64" } default { "unknown" } }
    } finally { $stream.Dispose() }
}
function ConvertTo-WindowsProcessArgument {
    param([AllowEmptyString()][string]$Value)
    $builder = [Text.StringBuilder]::new()
    [void]$builder.Append([char]'"')
    $backslashCount = 0
    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq [char]'\') {
            ++$backslashCount
            continue
        }
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
function Invoke-CapturedCommand {
    param([string]$Path, [string[]]$Arguments, [string]$LogPath, [string]$Description)
    $canonicalPath = [IO.Path]::GetFullPath($Path)
    $canonicalLogPath = [IO.Path]::GetFullPath($LogPath)
    $stdoutLogPath = $canonicalLogPath + ".stdout.txt"
    $stderrLogPath = $canonicalLogPath + ".stderr.txt"
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $canonicalPath
    $startInfo.Arguments = (@($Arguments | ForEach-Object { ConvertTo-WindowsProcessArgument ([string]$_) }) -join " ")
    $startInfo.WorkingDirectory = [IO.Directory]::GetCurrentDirectory()
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) { throw "$Description could not be started: $canonicalPath" }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $exitCode = [int]$process.ExitCode
        $stdout = [string]$stdoutTask.Result
        $stderr = [string]$stderrTask.Result
    } finally {
        $process.Dispose()
    }

    [IO.File]::WriteAllText($stdoutLogPath, $stdout, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($stderrLogPath, $stderr, [Text.UTF8Encoding]::new($false))
    $commandLine = (@((ConvertTo-WindowsProcessArgument $canonicalPath)) + @($Arguments | ForEach-Object { ConvertTo-WindowsProcessArgument ([string]$_) })) -join " "
    $summaryLines = @(
        "command: $commandLine",
        "exitCode: $exitCode",
        "stdoutPath: $stdoutLogPath",
        "stdoutSha256: $(Get-FileSha256 $stdoutLogPath)",
        "stderrPath: $stderrLogPath",
        "stderrSha256: $(Get-FileSha256 $stderrLogPath)"
    )
    [IO.File]::WriteAllLines($canonicalLogPath, $summaryLines, [Text.UTF8Encoding]::new($false))
    if ($exitCode -ne 0) { throw "$Description failed with exit code $exitCode; see $canonicalLogPath, $stdoutLogPath, and $stderrLogPath." }
    [ordered]@{
        logPath = $canonicalLogPath; exitCode = $exitCode; logSha256 = Get-FileSha256 $canonicalLogPath
        stdoutLogPath = $stdoutLogPath; stdoutLogSha256 = Get-FileSha256 $stdoutLogPath
        stderrLogPath = $stderrLogPath; stderrLogSha256 = Get-FileSha256 $stderrLogPath
    }
}
function Invoke-PythonJson {
    param([string]$ScriptPath, [string[]]$Arguments, [string]$LogPath, [string]$Description)
    $output = @(& python $ScriptPath @Arguments 2>&1 | ForEach-Object { [string]$_ })
    $exitCode = $LASTEXITCODE
    [IO.File]::WriteAllLines($LogPath, $output, [Text.UTF8Encoding]::new($false))
    if ($exitCode -ne 0) { throw "$Description failed with exit code $exitCode; see $LogPath. $($output -join ' ')" }
    $jsonLine = @($output | Where-Object { $_.TrimStart().StartsWith('{') } | Select-Object -Last 1)
    if ($jsonLine.Count -ne 1) { throw "$Description returned no JSON result." }
    $jsonLine[0] | ConvertFrom-Json -ErrorAction Stop
}
function Get-ProcessLogText {
    param([object]$Context)
    $parts = [Collections.Generic.List[string]]::new()
    foreach ($path in @($Context.stdoutPath, $Context.stderrPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        $stream = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        try { $reader = [IO.StreamReader]::new($stream); try { $parts.Add($reader.ReadToEnd()) } finally { $reader.Dispose() } }
        finally { $stream.Dispose() }
    }
    $parts -join "`n"
}
function Get-ProcessLogLines {
    param([object]$Context)
    @((Get-ProcessLogText $Context) -split "`r?`n")
}
function Get-StartupCheckpoint {
    param([object]$Context, [string]$Stage)
    $pattern = "ClassMngrNext startup stage=$([regex]::Escape($Stage)) outcome=(?<outcome>[a-z-]+) diagnostic=(?<diagnostic>[a-z-]+) diagnostic_stage=(?<diagnosticStage>[a-z-]+) checkpoint=(?<checkpoint>\{.*\})$"
    $found = [Collections.Generic.List[object]]::new()
    $lineIndex = 0
    foreach ($line in @(Get-ProcessLogLines $Context)) {
        if ($line -match $pattern) {
            $checkpoint = $Matches.checkpoint | ConvertFrom-Json -ErrorAction Stop
            $found.Add([ordered]@{ outcome = $Matches.outcome; diagnostic = $Matches.diagnostic; diagnosticStage = $Matches.diagnosticStage; record = $checkpoint; lineIndex = $lineIndex; observedAtUtc = Get-UtcTimestamp; rawLine = $line })
        }
        ++$lineIndex
    }
    if ($found.Count -ne 1) { return $null }
    $found[0]
}
function Get-LifecycleRecords {
    param([object]$Context)
    $records = [Collections.Generic.List[object]]::new()
    $lineIndex = 0
    foreach ($line in @(Get-ProcessLogLines $Context)) {
        $match = [regex]::Match($line, 'ClassMngrNext page lifecycle checkpoint=(\{.*\})')
        if ($match.Success) {
            try {
                $record = $match.Groups[1].Value | ConvertFrom-Json -ErrorAction Stop
                $record | Add-Member -NotePropertyName processId -NotePropertyValue ([Int64]$Context.process.Id) -Force
                $record | Add-Member -NotePropertyName mainWindowHandle -NotePropertyValue ([Int64]$Context.mainWindowHandle) -Force
                $record | Add-Member -NotePropertyName observedLogIndex -NotePropertyValue $lineIndex -Force
                $records.Add($record)
            } catch { }
        }
        ++$lineIndex
    }
    @($records.ToArray())
}
function Get-CalendarImportCheckpoints {
    param([object]$Context)
    $records = [Collections.Generic.List[object]]::new()
    $lineIndex = 0
    foreach ($line in @(Get-ProcessLogLines $Context)) {
        $match = [regex]::Match($line, 'ClassMngrNext calendar import checkpoint=(\{.*\})')
        if ($match.Success) {
            try {
                $record = $match.Groups[1].Value | ConvertFrom-Json -ErrorAction Stop
                $record | Add-Member -NotePropertyName processId -NotePropertyValue ([Int64]$Context.process.Id) -Force
                $record | Add-Member -NotePropertyName mainWindowHandle -NotePropertyValue ([Int64]$Context.mainWindowHandle) -Force
                $record | Add-Member -NotePropertyName observedLogIndex -NotePropertyValue $lineIndex -Force
                $records.Add($record)
            } catch { }
        }
        ++$lineIndex
    }
    @($records.ToArray())
}
function Wait-ForCondition {
    param([object]$Context, [scriptblock]$Predicate, [string]$Description, [int]$TimeoutSeconds = 20)
    $deadline = [DateTimeOffset]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTimeOffset]::UtcNow -lt $deadline) {
        $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "$($Context.routeId) exited while waiting for $Description (exit $($Context.process.ExitCode))." }
        $value = & $Predicate $Context
        if ($value) { return $value }
        Start-Sleep -Milliseconds 100
    }
    throw "$($Context.routeId) timed out waiting for $Description."
}
function Wait-ForLifecycle {
    param([object]$Context, [string]$PageKey, [string]$State, [int]$AfterLogIndex, [int]$TimeoutSeconds = 20)
    Wait-ForCondition $Context { param($ctx)
        $matches = @(Get-LifecycleRecords $ctx | Where-Object { $_.page_key -ceq $PageKey -and $_.state -ceq $State -and [int]$_.observedLogIndex -gt $AfterLogIndex })
        if ($matches.Count -gt 0) { return $matches[-1] }
        return $null
    } "$PageKey page lifecycle state $State" $TimeoutSeconds
}
function Wait-ForCalendarImportCheckpoint {
    param([object]$Context, [string]$Observation, [int]$AfterLogIndex, [int]$TimeoutSeconds = 30)
    Wait-ForCondition $Context { param($ctx)
        $matches = @(Get-CalendarImportCheckpoints $ctx | Where-Object {
            $_.event -ceq "calendar-import-checkpoint" -and
            $_.page_key -ceq "calendar" -and
            $_.observation -ceq $Observation -and
            [int]$_.observedLogIndex -gt $AfterLogIndex -and
            [bool]$_.calendar_runtime_metrics
        })
        if ($matches.Count -gt 0) { return $matches[-1] }
        return $null
    } "Calendar import checkpoint $Observation" $TimeoutSeconds
}
function Add-TeardownCheckpoint {
    param([object]$Context, [object]$Route, [string]$Name)
    $sample = New-ProcessSample $Context ("teardown-" + $Name)
    if (-not $Route.Contains("teardownCheckpoints")) { $Route.teardownCheckpoints = @() }
    $Route.teardownCheckpoints += ,([ordered]@{
        name = $Name; capturedAtUtc = Get-UtcTimestamp; processSample = $sample
    })
}
function Assert-StrictWorkingSetCheckpoint {
    param([object]$Sample, [string]$Description)
    if ([Int64]$Sample.workingSetBytes -ge $WorkingSetLimit) {
        throw "$Description working set $($Sample.workingSetBytes) is not strictly below $WorkingSetLimit bytes."
    }
}
function New-ProcessSample {
    param([object]$Context, [string]$Point, [int]$Index = 0)
    $process = $Context.process
    $process.Refresh()
    if ($process.HasExited) { throw "$($Context.routeId) exited before metric point $Point." }
    $sample = [ordered]@{
        routeId = $Context.routeId; processInstanceId = $Context.routeId; point = $Point; sampleIndex = $Index
        timestampUtc = Get-UtcTimestamp; pid = [Int64]$process.Id; processCreatedAtUtc = $Context.createdAtUtc
        canonicalExecutablePath = $Context.executablePath; architecture = "x64"; sha256 = $Context.executableSha256
        workingSetBytes = [Int64]$process.WorkingSet64; currentWorkingSetBytes = [Int64]$process.WorkingSet64
        privateBytes = [Int64]$process.PrivateMemorySize64; commitBytes = [Int64]$process.PagedMemorySize64
        peakWorkingSetBytes = [Int64]$process.PeakWorkingSet64; peakCommitBytes = [Int64]$process.PeakPagedMemorySize64
        handles = [Int64]$process.HandleCount; threads = [Int64]$process.Threads.Count
    }
    Add-JsonLine $ProcessMetricsPath $sample
    $sample
}
function Get-AppTopWindows {
    param([int]$ProcessId)
    $root = [System.Windows.Automation.AutomationElement]::RootElement
    @($root.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition) | Where-Object { $_.Current.ProcessId -eq $ProcessId -and $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window })
}
function Get-AppElements {
    param([object]$Context)
    $elements = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach ($window in @(Get-AppTopWindows ([int]$Context.process.Id))) {
        $elements.Add($window)
        try {
            foreach ($element in @($window.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition))) { $elements.Add($element) }
        } catch { }
    }
    @($elements.ToArray())
}
function Find-UiaById {
    param([object]$Context, [string]$AutomationId, [string]$ExpectedType = "")
    @((Get-AppElements $Context) | Where-Object {
        $current = $_.Current
        $id = [string]$current.AutomationId
        ($id -ceq $AutomationId -or $id.EndsWith(".$AutomationId", [StringComparison]::Ordinal)) -and
        ([string]::IsNullOrWhiteSpace($ExpectedType) -or $current.ControlType.ProgrammaticName -ceq $ExpectedType) -and
        $current.ProcessId -eq $Context.process.Id
    })
}
function Get-UiaElementById {
    param([object]$Context, [string]$AutomationId, [string]$ExpectedType = "")
    $matches = @(Find-UiaById $Context $AutomationId $ExpectedType)
    if ($matches.Count -ne 1) { throw "UIA expected one '$AutomationId' for process $($Context.process.Id), found $($matches.Count)." }
    $matches[0]
}
function Find-UiaByName {
    param([object]$Context, [string[]]$Names, [string]$ExpectedType = "")
    @((Get-AppElements $Context) | Where-Object {
        $current = $_.Current
        $typeMatches = [string]::IsNullOrWhiteSpace($ExpectedType) -or $current.ControlType.ProgrammaticName -ceq $ExpectedType
        $current.ProcessId -eq $Context.process.Id -and $typeMatches -and $Names -ccontains [string]$current.Name
    })
}
function Get-UiaPropertySnapshot {
    param([System.Windows.Automation.AutomationElement]$Element)
    $current = $Element.Current
    $rect = $current.BoundingRectangle
    $value = $null
    $selected = $null
    try { $value = ([System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)).Current.Value } catch { }
    try { $selected = [bool]([System.Windows.Automation.SelectionItemPattern]$Element.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)).Current.IsSelected } catch { }
    [ordered]@{
        automationId = [string]$current.AutomationId; name = [string]$current.Name
        controlType = [string]$current.ControlType.ProgrammaticName; className = [string]$current.ClassName
        processId = [Int64]$current.ProcessId; nativeWindowHandle = [Int64]$current.NativeWindowHandle
        isEnabled = [bool]$current.IsEnabled; isOffscreen = [bool]$current.IsOffscreen; isSelected = $selected; value = $value
        rectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height }
    }
}
function Save-UiaTreeSnapshot {
    param([object]$Context, [string]$Path)
    $elements = [Collections.Generic.List[object]]::new()
    foreach ($element in @(Get-AppElements $Context)) {
        try {
            $snapshot = Get-UiaPropertySnapshot $element
            $patterns = [Collections.Generic.List[string]]::new()
            foreach ($pattern in @(
                [System.Windows.Automation.InvokePattern]::Pattern,
                [System.Windows.Automation.SelectionItemPattern]::Pattern,
                [System.Windows.Automation.ExpandCollapsePattern]::Pattern,
                [System.Windows.Automation.ValuePattern]::Pattern)) {
                try {
                    if ($element.GetCurrentPattern($pattern)) { $patterns.Add([string]$pattern.ProgrammaticName) }
                } catch { }
            }
            $snapshot.patterns = @($patterns.ToArray())
            $elements.Add($snapshot)
        } catch { }
    }
    $document = [ordered]@{
        routeId = $Context.routeId; processId = [Int64]$Context.process.Id
        mainWindowHandle = [Int64]$Context.mainWindowHandle; observedAtUtc = Get-UtcTimestamp
        elements = @($elements.ToArray())
    }
    Write-JsonFile $Path $document
    [ordered]@{ path = [IO.Path]::GetFullPath($Path); elementCount = $elements.Count; sha256 = Get-FileSha256 $Path }
}
function Get-ElementWindowHandle {
    param([object]$Context, [System.Windows.Automation.AutomationElement]$Element)
    $walker = [System.Windows.Automation.TreeWalker]::ControlViewWalker
    $current = $Element
    for ($depth = 0; $depth -lt 48 -and $null -ne $current; $depth++) {
        try {
            if ($current.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window) {
                $candidate = [IntPtr]$current.Current.NativeWindowHandle
                if ($candidate -ne [IntPtr]::Zero -and [P910Native]::GetWindowProcessId($candidate) -eq [int]$Context.process.Id) { return $candidate }
            }
            $current = $walker.GetParent($current)
        } catch { break }
    }
    [IntPtr]$Context.mainWindowHandle
}
function Add-UiaAction {
    param([object]$Context, [System.Windows.Automation.AutomationElement]$Element, [string]$ActionKind, [string]$FromPage, [string]$ToPage)
    $identity = Get-UiaPropertySnapshot $Element
    if ([Int64]$identity.processId -ne [Int64]$Context.process.Id) { throw "UIA action $ActionKind target PID mismatch." }
    $window = Get-ElementWindowHandle $Context $Element
    $windowPid = [P910Native]::GetWindowProcessId($window)
    if ($windowPid -ne [int]$Context.process.Id) { throw "UIA action $ActionKind window PID mismatch." }
    $method = ""
    if ($identity.controlType -ceq "ControlType.TreeItem") {
        try {
            ([System.Windows.Automation.SelectionItemPattern]$Element.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)).Select()
            $method = "uia-selection-item-pattern"
        } catch {
            try {
                ([System.Windows.Automation.InvokePattern]$Element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)).Invoke()
                $method = "uia-invoke-pattern"
            } catch { throw "UIA target does not support an authentic select/invoke action for $ActionKind." }
        }
    } else {
        try {
            ([System.Windows.Automation.InvokePattern]$Element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)).Invoke()
            $method = "uia-invoke-pattern"
        } catch {
            try {
                ([System.Windows.Automation.SelectionItemPattern]$Element.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)).Select()
                $method = "uia-selection-item-pattern"
            } catch {
                try {
                    ([System.Windows.Automation.ExpandCollapsePattern]$Element.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)).Expand()
                    $method = "uia-expand-collapse-pattern"
                } catch { throw "UIA target does not support an authentic invoke/select action for $ActionKind." }
            }
        }
    }
    $record = [ordered]@{
        routeId = $Context.routeId; processInstanceId = $Context.routeId; actionKind = $ActionKind; method = $method
        fromPage = $FromPage; toPage = $ToPage; targetAutomationId = [string]$identity.automationId
        targetAccessibleName = [string]$identity.name; targetControlType = [string]$identity.controlType
        elementProcessId = [Int64]$identity.processId; windowHandle = [Int64]$window.ToInt64()
        windowProcessId = [Int64]$windowPid; mainWindowHandle = [Int64]$Context.mainWindowHandle
        applicationProcessId = [Int64]$Context.process.Id; issuedAtUtc = Get-UtcTimestamp
    }
    Add-JsonLine $UiaActionsPath $record
    $Context.actions += ,$record
    $record
}
function Add-UiaClickAction {
    param([object]$Context, [System.Windows.Automation.AutomationElement]$Element, [string]$ActionKind, [string]$FromPage, [string]$ToPage)
    $identity = Get-UiaPropertySnapshot $Element
    if ([Int64]$identity.processId -ne [Int64]$Context.process.Id) { throw "UIA click target PID mismatch for $ActionKind." }
    if (-not $identity.isEnabled -or $identity.isOffscreen) { throw "UIA click target is disabled or offscreen for $ActionKind." }
    $rect = $identity.rectangle
    $x = [int][Math]::Round([double]$rect.x + ([double]$rect.width / 2.0))
    $y = [int][Math]::Round([double]$rect.y + ([double]$rect.height / 2.0))
    $messageTargetHandle = [P910Native]::ClickVerifiedPoint($x, $y, [int]$Context.process.Id, [Int64]$Context.mainWindowHandle)
    $record = [ordered]@{
        routeId = $Context.routeId; processInstanceId = $Context.routeId; actionKind = $ActionKind
        method = "uia-resolved-verified-hwnd-message-click"; fromPage = $FromPage; toPage = $ToPage
        targetAutomationId = [string]$identity.automationId; targetAccessibleName = [string]$identity.name
        targetControlType = [string]$identity.controlType; elementProcessId = [Int64]$identity.processId
        windowHandle = [Int64]$Context.mainWindowHandle; windowProcessId = [Int64]$Context.process.Id
        mainWindowHandle = [Int64]$Context.mainWindowHandle; applicationProcessId = [Int64]$Context.process.Id
        targetRectangle = $rect; clickPoint = [ordered]@{ x = $x; y = $y }
        messageTargetWindowHandle = $messageTargetHandle; targetWasSelectedBeforeClick = $identity.isSelected
        issuedAtUtc = Get-UtcTimestamp
    }
    Add-JsonLine $UiaActionsPath $record
    $Context.actions += ,$record
    $record
}
function Wait-UiaById {
    param([object]$Context, [string]$AutomationId, [string]$ExpectedType = "", [int]$TimeoutSeconds = 15)
    Wait-ForCondition $Context { param($ctx)
        $matches = @(Find-UiaById $ctx $AutomationId $ExpectedType)
        if ($matches.Count -eq 1) { return $matches[0] }
        return $null
    } "UIA element $AutomationId" $TimeoutSeconds
}
function Get-ListItemCount {
    param([System.Windows.Automation.AutomationElement]$List)
    try {
        $items = $List.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
        @($items | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::ListItem }).Count
    } catch { 0 }
}
function Get-CalendarLoadState {
    param([object]$Context)
    $pageMatches = @(Find-UiaById $Context "nextCalendarPage")
    if ($pageMatches.Count -ne 1) { return $null }
    $page = $pageMatches[0]
    $listMatches = @(Find-UiaById $Context "calendarUpcomingEvents" "ControlType.List")
    if ($listMatches.Count -ne 1) { return $null }
    $list = $listMatches[0]
    $statusMatches = @(Find-UiaById $Context "calendarPageStatus")
    $statusText = if ($statusMatches.Count -eq 1) { [string]$statusMatches[0].Current.Name } else { "" }
    $loadingNames = @((Get-AppElements $Context) | Where-Object { $_.Current.Name -match '(?i)Loading (upcoming )?events' -and -not $_.Current.IsOffscreen })
    [ordered]@{
        page = Get-UiaPropertySnapshot $page
        list = Get-UiaPropertySnapshot $list
        upcomingItemCount = Get-ListItemCount $list
        statusText = $statusText
        loadingIndicatorsRemaining = $loadingNames.Count
        loadingNames = @($loadingNames | ForEach-Object { [string]$_.Current.Name })
        completed = ($loadingNames.Count -eq 0 -and $statusText -notmatch '(?i)Loading (upcoming )?events')
    }
}
function Wait-ForCalendarLoad {
    param([object]$Context, [int]$MinimumUpcomingItems, [int]$TimeoutSeconds = 30)
    Wait-ForCondition $Context { param($ctx)
        $state = Get-CalendarLoadState $ctx
        if ($null -ne $state -and $state.completed -and [int]$state.upcomingItemCount -ge $MinimumUpcomingItems) { return $state }
        return $null
    } "Calendar cache/UI load completion" $TimeoutSeconds
}
function Save-UiaControlSnapshot {
    param([object]$Context, [string]$Path, [string[]]$AutomationIds)
    $controls = [Collections.Generic.List[object]]::new()
    foreach ($automationId in $AutomationIds) {
        $expectedType = if ($automationId -ceq "calendarUpcomingEvents") { "ControlType.List" } else { "" }
        $matches = @(Find-UiaById $Context $automationId $expectedType)
        if ($matches.Count -gt 1) { throw "UIA snapshot found duplicate '$automationId'." }
        if ($matches.Count -eq 1) {
            $snapshot = Get-UiaPropertySnapshot $matches[0]
            if ($automationId -ceq "calendarUpcomingEvents") { $snapshot.upcomingItemCount = Get-ListItemCount $matches[0] }
            $controls.Add($snapshot)
        }
    }
    $document = [ordered]@{
        routeId = $Context.routeId; processId = [Int64]$Context.process.Id
        mainWindowHandle = [Int64]$Context.mainWindowHandle; windowProcessId = [Int64][P910Native]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle)
        observedAtUtc = Get-UtcTimestamp; controls = @($controls.ToArray())
    }
    Write-JsonFile $Path $document
    [ordered]@{ path = [IO.Path]::GetFullPath($Path); sha256 = Get-FileSha256 $Path; processId = $document.processId; mainWindowHandle = $document.mainWindowHandle; windowProcessId = $document.windowProcessId }
}
function Get-PageLifecycleByState {
    param([object]$Context, [string]$PageKey, [string]$State, [int]$AfterLogIndex = -1)
    @(Get-LifecycleRecords $Context | Where-Object { $_.page_key -ceq $PageKey -and $_.state -ceq $State -and [int]$_.observedLogIndex -gt $AfterLogIndex })
}
function Get-UiaByNamesExactly {
    param([object]$Context, [string[]]$Names, [string]$ExpectedType = "ControlType.TreeItem")
    $matches = @(Find-UiaByName $Context $Names $ExpectedType)
    if ($matches.Count -ne 1) { throw "UIA expected one navigation item for '$($Names -join '|')', found $($matches.Count)." }
    $matches[0]
}
function Get-PageButtonById {
    param([object]$Context, [string]$AutomationId)
    Get-UiaElementById $Context $AutomationId "ControlType.Button"
}
function Get-TempFiles {
    param([string]$Directory)
    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) { return @() }
    @(Get-ChildItem -LiteralPath $Directory -File -Force | ForEach-Object { [ordered]@{ path = [IO.Path]::GetFullPath($_.FullName); sizeBytes = [Int64]$_.Length; lastWriteTimeUtc = $_.LastWriteTimeUtc.ToString("o") } })
}
function Get-UiaControlText {
    param([object]$Context, [string]$AutomationId)
    $matches = @(Find-UiaById $Context $AutomationId)
    if ($matches.Count -ne 1) { return $null }
    [string]$matches[0].Current.Name
}
function Get-ProcessPathAndIdentity {
    param([Diagnostics.Process]$Process)
    $path = [IO.Path]::GetFullPath([P910Native]::GetProcessImagePath($Process.Handle))
    [ordered]@{ path = $path; sha256 = Get-FileSha256 $path; architectureBits = if ([P910Native]::IsNativeX64($Process.Handle)) { 64 } else { 32 } }
}
function Get-ProcessWindowSnapshot {
    param([Diagnostics.Process]$Process, [Int64]$MainWindowHandle)
    $uiaAssemblyPath = ""
    try {
        Add-Type -AssemblyName UIAutomationClient
        Add-Type -AssemblyName UIAutomationTypes
        $uiaAssemblyPath = [System.Windows.Automation.AutomationElement].Assembly.Location
    } catch { }
    $nativeWindows = @([P910Native]::GetTopLevelWindowsForProcess([int]$Process.Id))
    $windows = [Collections.Generic.List[object]]::new()
    foreach ($window in $nativeWindows) {
        $uiaInfo = [P910Native]::GetUiaWindowInfoWithTimeout([Int64]$window.Handle, 350, $uiaAssemblyPath)
        $windows.Add([ordered]@{
            handle = [Int64]$window.Handle; processId = [int]$window.ProcessId
            nativeTitle = [string]$window.Title; nativeClassName = [string]$window.ClassName
            visible = [bool]$window.Visible; enabled = [bool]$window.Enabled
            uiaProbeStatus = [string]$uiaInfo[0]; uiaTitle = [string]$uiaInfo[1]
            uiaControlType = [string]$uiaInfo[2]; uiaAutomationId = [string]$uiaInfo[3]
            uiaClassName = [string]$uiaInfo[4]
        })
    }
    $process.Refresh()
    $mainHandlePresent = $false
    $mainWindowVisible = $false
    foreach ($window in $nativeWindows) {
        if ([Int64]$window.Handle -eq $MainWindowHandle) {
            $mainHandlePresent = $true
            $mainWindowVisible = [bool]$window.Visible
            break
        }
    }
    [ordered]@{
        capturedAtUtc = Get-UtcTimestamp
        processId = [Int64]$Process.Id
        processHasExited = [bool]$Process.HasExited
        processExitCode = if ($Process.HasExited) { [int]$Process.ExitCode } else { $null }
        mainWindowHandle = $MainWindowHandle
        mainWindowTitle = [string]$Process.MainWindowTitle
        mainWindowHandlePresent = $mainHandlePresent
        mainWindowVisible = $mainWindowVisible
        mainWindowState = if ($Process.HasExited) { "process-exited" } elseif (-not $mainHandlePresent) { "main-window-not-enumerated" } elseif ($mainWindowVisible) { "visible" } else { "hidden" }
        uiaProbeTimeoutMillisecondsPerWindow = 350
        topLevelWindowCount = $windows.Count
        topLevelWindows = @($windows.ToArray())
    }
}

function Get-ProcessExitStatusSample {
    param([Diagnostics.Process]$Process, [Int64]$MainWindowHandle, [double]$ElapsedSeconds, [string]$Stage)
    $Process.Refresh()
    $hasExited = [bool]$Process.HasExited
    $nativeWindows = @([P910Native]::GetTopLevelWindowsForProcess([int]$Process.Id))
    $windowCount = [Int64]@($nativeWindows).Count
    $threadCount = [Int64]0
    if (-not $hasExited) {
        try {
            $threadCount = [Int64]@($Process.Threads).Count
        } catch {
            $Process.Refresh()
            if (-not $Process.HasExited) { throw }
            # A process that exits between the first status check and the
            # thread collection read has no remaining threads to sample.
            $threadCount = [Int64]0
        }
    }
    $mainPresent = $false
    $mainVisible = $false
    foreach ($window in $nativeWindows) {
        if ([Int64]$window.Handle -eq $MainWindowHandle) { $mainPresent = $true; $mainVisible = [bool]$window.Visible; break }
    }
    [ordered]@{
        stage = $Stage; elapsedSeconds = [Math]::Round($ElapsedSeconds, 3); capturedAtUtc = Get-UtcTimestamp
        processId = [Int64]$Process.Id; processHasExited = $hasExited
        processExitCode = if ($hasExited) { [int]$Process.ExitCode } else { $null }
        cpuTimeMilliseconds = [double]$Process.TotalProcessorTime.TotalMilliseconds
        workingSetBytes = [Int64]$Process.WorkingSet64
        privateBytes = [Int64]$Process.PrivateMemorySize64
        handles = [Int64]$Process.HandleCount
        threads = $threadCount
        topLevelWindowCount = $windowCount
        mainWindowHandlePresent = $mainPresent; mainWindowVisible = $mainVisible
    }
}

function Save-ApplicationShutdownTrace {
    param([object]$Route, [string]$Path, [string]$RouteId)
    $trace = [ordered]@{
        schema = "classmngr-phase9-application-shutdown-trace-link-v1"
        routeId = $RouteId
        processId = if ($null -ne $Route.processIdentity) { [Int64]$Route.processIdentity.pid } else { $null }
        status = "missing"
        path = [IO.Path]::GetFullPath($Path)
        sha256 = $null
        sizeBytes = 0
        eventCount = 0
        firstSequence = $null
        lastSequence = $null
        events = @()
        parseError = $null
    }
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        $item = Get-Item -LiteralPath $Path
        $trace.sizeBytes = [Int64]$item.Length
        $trace.sha256 = Get-FileSha256 $Path
        try {
            $records = [Collections.Generic.List[object]]::new()
            foreach ($line in [IO.File]::ReadAllLines($Path, [Text.UTF8Encoding]::new($false))) {
                if ([string]::IsNullOrWhiteSpace($line)) { continue }
                $records.Add(($line | ConvertFrom-Json -ErrorAction Stop))
            }
            $trace.eventCount = $records.Count
            $trace.events = @($records | ForEach-Object { [string]$_.event })
            if ($records.Count -gt 0) {
                $trace.firstSequence = [Int64]$records[0].sequence
                $trace.lastSequence = [Int64]$records[-1].sequence
                $trace.status = "captured"
                foreach ($record in $records) {
                    if ([Int64]$record.process_id -ne [Int64]$trace.processId) {
                        $trace.status = "invalid-process-identity"
                        $trace.parseError = "trace contains a record for a different process"
                        break
                    }
                }
            } else {
                $trace.status = "empty"
            }
        } catch {
            $trace.status = "parse-error"
            $trace.parseError = $_.Exception.Message
        }
    }
    $Route.diagnostics.applicationShutdownTrace = $trace
    return $trace
}

function Stop-MeasuredProcess {
    param([object]$Context)
    $process = $Context.process
    $routeId = if ($Context.Contains("routeId")) { [string]$Context.routeId } else { "pid-$($process.Id)" }
    $mainHandle = if ($Context.Contains("mainWindowHandle")) { [Int64]$Context.mainWindowHandle } else { [Int64]0 }
    $graceSeconds = 45
    $dumpTriggerSeconds = 12
    $dumpPath = if ($Context.Contains("shutdownDumpPath")) {
        [IO.Path]::GetFullPath([string]$Context.shutdownDumpPath)
    } else {
        [IO.Path]::GetFullPath((Join-Path (Join-Path (Join-Path $EvidenceDirectory "runtime") $routeId) "diagnostics\shutdown-threadinfo-12s.dmp"))
    }
    $dumpEvidence = [ordered]@{
        schema = "classmngr-phase9-shutdown-minidump-v1"
        status = "waiting-for-trigger"
        processId = [Int64]$process.Id
        triggerElapsedSeconds = $dumpTriggerSeconds
        captureElapsedSeconds = $null
        captureStartedAtUtc = $null
        captureCompletedAtUtc = $null
        captureElapsedMilliseconds = $null
        path = $dumpPath
        sizeBytes = 0
        sha256 = $null
        dumpKind = "MiniDumpNormal | MiniDumpWithThreadInfo"
        signature = $null
        errorType = $null
        errorMessage = $null
        nativeErrorCode = $null
    }
    $dumpAttempted = $false
    $dumpCaptureFailed = $false
    $observability = [ordered]@{
        schema = "classmngr-phase9-process-shutdown-observability-v1"
        routeId = $routeId; processId = [Int64]$process.Id
        processCreatedAtUtc = if ($Context.Contains("createdAtUtc")) { [string]$Context.createdAtUtc } else { $null }
        requestedAtUtc = Get-UtcTimestamp; gracefulCloseTimeoutSeconds = $graceSeconds
        closeMainWindowReturned = $null; closeMainWindowRequestedAtUtc = $null
        exitProgressObservedAtUtc = $null; exitProgressObservation = "not-observed"
        processExitCompletedAtUtc = $null; processExitCode = $null
        gracefulExitCompleted = $false; gracefulExitTimedOut = $false
        forcedTerminationRequestedAtUtc = $null; forcedTerminationCompletedAtUtc = $null
        shutdownMiniDump = $dumpEvidence
        shutdownStopReason = "graceful-exit-observation"
        snapshots = @(); exitStatusSamples = @(); samplingErrors = @()
    }
    $snapshots = [Collections.Generic.List[object]]::new()
    $exitStatusSamples = [Collections.Generic.List[object]]::new()
    try {
        $process.Refresh()
        $alreadyExited = [bool]$process.HasExited
        $snapshots.Add([ordered]@{ stage = "before-close-request"; elapsedSeconds = 0; state = Get-ProcessWindowSnapshot $process $mainHandle })
        try { $exitStatusSamples.Add((Get-ProcessExitStatusSample $process $mainHandle 0 "before-close-request")) }
        catch { $observability.samplingErrors += ,([ordered]@{ elapsedSeconds = 0; message = $_.Exception.Message }) }
        if ($alreadyExited) {
            $observability.exitProgressObservedAtUtc = Get-UtcTimestamp
            $observability.exitProgressObservation = "process-already-exited-before-close-request"
            $observability.processExitCompletedAtUtc = $observability.exitProgressObservedAtUtc
            $observability.processExitCode = [int]$process.ExitCode
            $observability.gracefulExitCompleted = ([int]$process.ExitCode -eq 0)
            $dumpEvidence.status = "process-exited-before-trigger"
        } else {
            $observability.closeMainWindowRequestedAtUtc = Get-UtcTimestamp
            $observability.closeMainWindowReturned = [bool]$process.CloseMainWindow()
            $graceTimer = [Diagnostics.Stopwatch]::StartNew()
            try {
                $firstStatusSample = Get-ProcessExitStatusSample $process $mainHandle $graceTimer.Elapsed.TotalSeconds "after-close-request"
                $exitStatusSamples.Add($firstStatusSample)
                if (-not $firstStatusSample.processHasExited -and -not $firstStatusSample.mainWindowHandlePresent -and $mainHandle -ne 0) {
                    $observability.exitProgressObservedAtUtc = [string]$firstStatusSample.capturedAtUtc
                    $observability.exitProgressObservation = "main-window-disappeared-process-still-running"
                }
            }
            catch { $observability.samplingErrors += ,([ordered]@{ elapsedSeconds = 0; message = $_.Exception.Message }) }
            while ($graceTimer.Elapsed.TotalSeconds -lt $graceSeconds) {
                if (-not $dumpAttempted -and $graceTimer.Elapsed.TotalSeconds -ge $dumpTriggerSeconds) {
                    $dumpAttempted = $true
                    $process.Refresh()
                    if ($process.HasExited) {
                        $dumpEvidence.status = "process-exited-before-trigger"
                    } else {
                        $dumpEvidence.captureStartedAtUtc = Get-UtcTimestamp
                        $dumpEvidence.captureElapsedSeconds = [Math]::Round($graceTimer.Elapsed.TotalSeconds, 3)
                        $dumpTimer = [Diagnostics.Stopwatch]::StartNew()
                        try {
                            if (Test-Path -LiteralPath $dumpPath) { throw "Refusing to overwrite shutdown dump artifact: $dumpPath" }
                            $capturedBytes = [P910Native]::WriteNormalThreadInfoMiniDump(
                                $process.Handle,
                                [int]$process.Id,
                                $dumpPath
                                )
                            $dumpTimer.Stop()
                            $dumpEvidence.captureElapsedMilliseconds = [Math]::Round($dumpTimer.Elapsed.TotalMilliseconds, 3)
                            $dumpEvidence.captureCompletedAtUtc = Get-UtcTimestamp
                            $dumpFile = Get-Item -LiteralPath $dumpPath
                            $dumpEvidence.sizeBytes = [Int64]$dumpFile.Length
                            $dumpEvidence.sha256 = Get-FileSha256 $dumpPath
                            if ($dumpEvidence.sizeBytes -le 4 -or $dumpEvidence.sizeBytes -ne [Int64]$capturedBytes) {
                                throw "MiniDumpWriteDump produced an empty or inconsistent dump file (API bytes=$capturedBytes, file bytes=$($dumpEvidence.sizeBytes))."
                            }
                            $signatureBytes = [IO.File]::ReadAllBytes($dumpPath)[0..3]
                            $dumpEvidence.signature = [Text.Encoding]::ASCII.GetString($signatureBytes)
                            if ($dumpEvidence.signature -cne "MDMP") { throw "MiniDumpWriteDump output does not have the MDMP signature." }
                            $dumpEvidence.status = "captured"
                        } catch {
                            $dumpTimer.Stop()
                            $dumpEvidence.captureElapsedMilliseconds = [Math]::Round($dumpTimer.Elapsed.TotalMilliseconds, 3)
                            $dumpEvidence.captureCompletedAtUtc = Get-UtcTimestamp
                            $dumpEvidence.status = "failed"
                            $dumpEvidence.errorType = $_.Exception.GetType().FullName
                            $dumpEvidence.errorMessage = $_.Exception.Message
                            if ($_.Exception -is [ComponentModel.Win32Exception]) {
                                $dumpEvidence.nativeErrorCode = [int]$_.Exception.NativeErrorCode
                            }
                            if (Test-Path -LiteralPath $dumpPath -PathType Leaf) {
                                $partialDump = Get-Item -LiteralPath $dumpPath
                                $dumpEvidence.sizeBytes = [Int64]$partialDump.Length
                                $dumpEvidence.sha256 = Get-FileSha256 $dumpPath
                            }
                            $dumpCaptureFailed = $true
                            $observability.shutdownStopReason = "shutdown-dump-capture-failed"
                            break
                        }
                    }
                    $observability.shutdownMiniDump = $dumpEvidence
                }
                $remainingMilliseconds = [int][Math]::Max(1, [Math]::Min(1000, ($graceSeconds - $graceTimer.Elapsed.TotalSeconds) * 1000))
                if ($process.WaitForExit($remainingMilliseconds)) {
                    $process.Refresh()
                    if (-not $dumpAttempted) {
                        $dumpAttempted = $true
                        $dumpEvidence.status = "process-exited-before-trigger"
                        $observability.shutdownMiniDump = $dumpEvidence
                    }
                    try { $exitStatusSamples.Add((Get-ProcessExitStatusSample $process $mainHandle $graceTimer.Elapsed.TotalSeconds "process-exit-observed")) }
                    catch { $observability.samplingErrors += ,([ordered]@{ elapsedSeconds = [Math]::Round($graceTimer.Elapsed.TotalSeconds, 3); message = $_.Exception.Message }) }
                    if ($null -eq $observability.exitProgressObservedAtUtc) {
                        $observability.exitProgressObservedAtUtc = Get-UtcTimestamp
                        $observability.exitProgressObservation = "process-exit-observed"
                    }
                    $observability.processExitCompletedAtUtc = Get-UtcTimestamp
                    $observability.processExitCode = [int]$process.ExitCode
                    $observability.gracefulExitCompleted = ($observability.processExitCode -eq 0)
                    break
                }
                try {
                    $sample = Get-ProcessExitStatusSample $process $mainHandle $graceTimer.Elapsed.TotalSeconds "graceful-close-in-progress"
                    $exitStatusSamples.Add($sample)
                    if ($null -eq $observability.exitProgressObservedAtUtc -and -not $sample.mainWindowHandlePresent -and $mainHandle -ne 0) {
                        $observability.exitProgressObservedAtUtc = [string]$sample.capturedAtUtc
                        $observability.exitProgressObservation = "main-window-disappeared-process-still-running"
                    }
                } catch { $observability.samplingErrors += ,([ordered]@{ elapsedSeconds = [Math]::Round($graceTimer.Elapsed.TotalSeconds, 3); message = $_.Exception.Message }) }
            }
            $process.Refresh()
            if (-not $process.HasExited) {
                try {
                    $state = Get-ProcessWindowSnapshot $process $mainHandle
                    $snapshotStage = if ($dumpCaptureFailed) { "shutdown-dump-capture-failed" } else { "graceful-close-timeout" }
                    $snapshots.Add([ordered]@{ stage = $snapshotStage; elapsedSeconds = [Math]::Round($graceTimer.Elapsed.TotalSeconds, 3); state = $state })
                } catch { $observability.samplingErrors += ,([ordered]@{ elapsedSeconds = [Math]::Round($graceTimer.Elapsed.TotalSeconds, 3); message = $_.Exception.Message }) }
                $lastSampleStage = if ($dumpCaptureFailed) { "shutdown-dump-capture-failed" } else { "graceful-close-timeout" }
                try { $exitStatusSamples.Add((Get-ProcessExitStatusSample $process $mainHandle $graceTimer.Elapsed.TotalSeconds $lastSampleStage)) }
                catch { $observability.samplingErrors += ,([ordered]@{ elapsedSeconds = [Math]::Round($graceTimer.Elapsed.TotalSeconds, 3); message = $_.Exception.Message }) }
                if (-not $dumpCaptureFailed) { $observability.gracefulExitTimedOut = $true }
                if (-not $dumpCaptureFailed) { $observability.shutdownStopReason = "graceful-exit-timeout" }
                $observability.forcedTerminationRequestedAtUtc = Get-UtcTimestamp
                $process.Kill()
                [void]$process.WaitForExit(10000)
                $process.Refresh()
                if ($process.HasExited) {
                    $observability.forcedTerminationCompletedAtUtc = Get-UtcTimestamp
                    $observability.processExitCompletedAtUtc = $observability.forcedTerminationCompletedAtUtc
                    $observability.processExitCode = [int]$process.ExitCode
                }
            }
        }
    } catch {
        $observability.samplingErrors += ,([ordered]@{ elapsedSeconds = $null; message = $_.Exception.Message })
        try { $process.Refresh(); if ($process.HasExited) { $observability.processExitCode = [int]$process.ExitCode; $observability.processExitCompletedAtUtc = Get-UtcTimestamp } } catch { }
    }
    $observability.snapshots = @($snapshots.ToArray())
    $observability.exitStatusSamples = @($exitStatusSamples.ToArray())
    $observability.completedAtUtc = Get-UtcTimestamp
    $diagnosticPath = Join-Path (Join-Path $EvidenceDirectory "logs") ($routeId + "-shutdown-observability.json")
    Write-JsonFile $diagnosticPath $observability
    $script:LastShutdownObservability = [ordered]@{
        path = [IO.Path]::GetFullPath($diagnosticPath); sha256 = Get-FileSha256 $diagnosticPath
        routeId = $routeId; closeMainWindowReturned = $observability.closeMainWindowReturned
        gracefulExitCompleted = $observability.gracefulExitCompleted
        gracefulExitTimedOut = $observability.gracefulExitTimedOut
        shutdownMiniDump = $observability.shutdownMiniDump
        shutdownStopReason = $observability.shutdownStopReason
        exitProgressObservedAtUtc = $observability.exitProgressObservedAtUtc
        exitProgressObservation = $observability.exitProgressObservation
        processExitCompletedAtUtc = $observability.processExitCompletedAtUtc
        processExitCode = $observability.processExitCode
    }
    if ($null -ne $observability.processExitCode) { return [int]$observability.processExitCode }
    $null
}
function Invoke-VerifiedRoute {
    param([string]$RouteId, [string]$FixtureName, [string]$FixtureRole, [string]$RouteDirectory, [object]$Fixture, [string]$WorkspaceTemplateSha256)
    $runtimeRoot = Join-Path (Join-Path $EvidenceDirectory "runtime") $RouteId
    $workspaceDirectory = Join-Path $runtimeRoot "workspace"
    $settingsRoot = Join-Path $runtimeRoot "settings"
    $localAppData = Join-Path $runtimeRoot "local-app-data"
    $appData = Join-Path $runtimeRoot "app-data"
    $tempDirectory = Join-Path $runtimeRoot "temp"
    $shutdownTraceDirectory = Join-Path $runtimeRoot "diagnostics"
    foreach ($directory in @($workspaceDirectory, $settingsRoot, $localAppData, $appData, $tempDirectory, $shutdownTraceDirectory)) { [void][IO.Directory]::CreateDirectory($directory) }
    $shutdownTracePath = Join-Path $shutdownTraceDirectory "application-shutdown.jsonl"
    $shutdownDumpPath = Join-Path $shutdownTraceDirectory "shutdown-threadinfo-12s.dmp"
    $workspacePath = Join-Path $workspaceDirectory ($RouteId + ".tps")
    if (Test-Path -LiteralPath $workspacePath) { throw "Refusing to overwrite route workspace $workspacePath." }
    Copy-Item -LiteralPath (Join-Path (Join-Path $EvidenceDirectory "workspace-snapshots") "seeded-template.tps") -Destination $workspacePath
    if ((Get-FileSha256 $workspacePath) -cne $WorkspaceTemplateSha256) { throw "$RouteId workspace copy differs from the identical seed template." }
    $beforeSnapshotPath = Join-Path (Join-Path $EvidenceDirectory "workspace-snapshots") ($RouteId + "-before.json")
    $afterSnapshotPath = Join-Path (Join-Path $EvidenceDirectory "workspace-snapshots") ($RouteId + "-after.json")
    $stdoutPath = Join-Path (Join-Path $EvidenceDirectory "logs") ($RouteId + "-stdout.log")
    $stderrPath = Join-Path (Join-Path $EvidenceDirectory "logs") ($RouteId + "-stderr.log")
    $evidenceFixturePath = [IO.Path]::GetFullPath([string]$Fixture.evidencePath)
    $payload = [IO.File]::ReadAllBytes($evidenceFixturePath)
    $server = [P910LoopbackFixtureServer]::new($payload)
    $appPath = [string]$script:Manifest.application.canonicalPath
    $process = $null
    $context = $null
    $route = [ordered]@{
        routeId = $RouteId
        fixtureRole = $FixtureRole
        fixture = $Fixture
        server = $null
        processIdentity = $null
        startupReady = $null
        workspaceOpened = $null
        calendarBaseline = $null
        calendarLoadWait = $null
        import = $null
        lifecycle = [ordered]@{ suspendCount = 0; reentryCount = 0; calendarActivated = $null; calendarSuspended = $null; calendarReentered = $null }
        teardownCheckpoints = @()
        diagnostics = [ordered]@{ applicationShutdownTrace = $null; shutdownMiniDump = $null }
        workspace = [ordered]@{
            path = [IO.Path]::GetFullPath($workspacePath)
            sha256AtLaunch = Get-FileSha256 $workspacePath
            seedTemplateSha256 = $WorkspaceTemplateSha256
            beforeSnapshot = [IO.Path]::GetFullPath($beforeSnapshotPath)
            afterSnapshot = [IO.Path]::GetFullPath($afterSnapshotPath)
            beforeTableContentSha256 = $null
            afterTableContentSha256 = $null
            beforeCalendarEventCount = $null
            afterCalendarEventCount = $null
            rowLevelContentsUnchanged = $false
            afterCountMatchesExpectedTotal = $false
            runtimeDirectory = [IO.Path]::GetFullPath($runtimeRoot)
        }
        memory = [ordered]@{ normalUseSamples = @(); operationSamples = @(); idleTail = @() }
        processExitCode = $null
        stdoutLog = [IO.Path]::GetFullPath($stdoutPath)
        stderrLog = [IO.Path]::GetFullPath($stderrPath)
        uia = [ordered]@{}
    }
    $script:Manifest.routes += ,$route
    try {
        $environment = [ordered]@{
            CLASSMNGR_SETTINGS_ROOT = $settingsRoot
            CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL = [string]$server.Url
            LOCALAPPDATA = $localAppData
            APPDATA = $appData
            TEMP = $tempDirectory
            TMP = $tempDirectory
            QT_FORCE_STDERR_LOGGING = "1"
            CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH = [IO.Path]::GetFullPath($shutdownTracePath)
            NO_PROXY = "127.0.0.1,localhost"
            PATH = "$script:QtBin;$(Split-Path -Parent $appPath);$env:PATH"
        }
        $oldEnvironment = @{}
        foreach ($entry in $environment.GetEnumerator()) { $oldEnvironment[$entry.Key] = [Environment]::GetEnvironmentVariable($entry.Key, "Process"); [Environment]::SetEnvironmentVariable($entry.Key, [string]$entry.Value, "Process") }
        try {
            $process = Start-Process -FilePath $appPath -ArgumentList ('"' + [IO.Path]::GetFullPath($workspacePath) + '"') -WorkingDirectory (Split-Path -Parent $appPath) -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
        } finally {
            foreach ($entry in $environment.GetEnumerator()) { [Environment]::SetEnvironmentVariable($entry.Key, $oldEnvironment[$entry.Key], "Process") }
        }
        $process.Refresh()
        $processIdentity = Get-ProcessPathAndIdentity $process
        if (-not [string]::Equals($processIdentity.path, $appPath, [StringComparison]::OrdinalIgnoreCase) -or $processIdentity.sha256 -cne [string]$script:Manifest.application.sha256 -or $processIdentity.architectureBits -ne 64) { throw "$RouteId launched a different image than the staged current-source x64 package." }
        $createdAtUtc = $process.StartTime.ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss.fffffff'Z'")
        $context = [ordered]@{
            routeId = $RouteId; processInstanceId = $RouteId; process = $process; createdAtUtc = $createdAtUtc
            executablePath = $processIdentity.path; executableSha256 = $processIdentity.sha256
            stdoutPath = $stdoutPath; stderrPath = $stderrPath; mainWindowHandle = 0L; mainWindowTitle = ""
            tempDirectory = [IO.Path]::GetFullPath($tempDirectory); shutdownTracePath = [IO.Path]::GetFullPath($shutdownTracePath); shutdownDumpPath = [IO.Path]::GetFullPath($shutdownDumpPath); actions = @()
        }
        $script:Processes.Add($context)
        $script:CurrentContext = $context
        $route.processIdentity = [ordered]@{
            pid = [Int64]$process.Id; createdAtUtc = $createdAtUtc; canonicalExecutablePath = $processIdentity.path
            architecture = "x64"; architectureBits = 64; sha256 = $processIdentity.sha256
            mainWindowHandle = $null; mainWindowTitle = $null; mainWindowProcessId = $null
        }
        $route.server = [ordered]@{
            implementation = "in-process loopback TCP HTTP fixture server"
            host = "127.0.0.1"; port = [int]$server.Port; url = [string]$server.Url
            requestCount = 0; requestPath = ""; remoteAddress = ""
            responseBytesSent = 0; responseSha256 = $server.ResponseSha256
            responseMode = "full-fixture-response-with-no-staging-visibility-gate"
            fixtureName = $FixtureName; fixtureSha256 = [string]$Fixture.sha256
        }

        $ready = Wait-ForCondition $context { param($ctx) Get-StartupCheckpoint $ctx "startup-ready" } "startup-ready checkpoint" $ReadinessTimeoutSeconds
        $opened = Wait-ForCondition $context { param($ctx) Get-StartupCheckpoint $ctx "workspace-opened" } "workspace-opened checkpoint" $ReadinessTimeoutSeconds
        foreach ($checkpoint in @($ready, $opened)) {
            if ($checkpoint.outcome -cne "completed" -or $checkpoint.diagnostic -cne "none" -or $checkpoint.diagnosticStage -cne "none" -or $checkpoint.record.outcome -cne "completed" -or $checkpoint.record.platform -cne "windows" -or $checkpoint.record.sample_available -ne $true) { throw "$RouteId startup checkpoint was not a completed Windows sample." }
        }
        $route.startupReady = $ready
        $route.workspaceOpened = $opened
        Add-JsonLine $StartupLogPath ([ordered]@{ routeId = $RouteId; pid = [Int64]$process.Id; stage = "workspace-opened"; checkpoint = $opened.record; capturedAtUtc = $opened.observedAtUtc })
        Add-JsonLine $StartupLogPath ([ordered]@{ routeId = $RouteId; pid = [Int64]$process.Id; stage = "startup-ready"; checkpoint = $ready.record; capturedAtUtc = $ready.observedAtUtc })
        $startupCheckpoint = [ordered]@{
            routeId = $RouteId; processInstanceId = $RouteId; point = "startup-ready-checkpoint"; sampleIndex = 0
            timestampUtc = $ready.observedAtUtc; pid = [Int64]$process.Id; processCreatedAtUtc = $createdAtUtc
            canonicalExecutablePath = $processIdentity.path; architecture = "x64"; sha256 = $processIdentity.sha256
            workingSetBytes = [Int64]$ready.record.metrics.working_set_bytes; currentWorkingSetBytes = [Int64]$ready.record.metrics.working_set_bytes
            privateBytes = [Int64]$ready.record.metrics.private_usage_bytes; commitBytes = [Int64]$ready.record.metrics.pagefile_usage_bytes
            peakWorkingSetBytes = [Int64]$ready.record.metrics.peak_working_set_bytes
            handles = [Int64]$ready.record.metrics.handle_count; threads = [Int64]$ready.record.metrics.thread_count
        }
        Assert-StrictWorkingSetCheckpoint $startupCheckpoint "$RouteId startup-ready"
        Add-JsonLine $ProcessMetricsPath $startupCheckpoint
        $route.memory.normalUseSamples += ,$startupCheckpoint
        $startupExternal = New-ProcessSample $context "startup-ready-external"
        Assert-StrictWorkingSetCheckpoint $startupExternal "$RouteId startup-ready external"
        $route.memory.normalUseSamples += ,$startupExternal
        Start-Sleep -Seconds 5
        $startupSettled = New-ProcessSample $context "startup-settled"
        Assert-StrictWorkingSetCheckpoint $startupSettled "$RouteId startup-settled"
        $route.memory.normalUseSamples += ,$startupSettled

        $deadline = [DateTimeOffset]::UtcNow.AddSeconds(20); $stable = 0; $stableHandle = 0L; $stableTitle = ""
        while ([DateTimeOffset]::UtcNow -lt $deadline -and $stable -lt 3) {
            $process.Refresh(); $handle = $process.MainWindowHandle; $title = $process.MainWindowTitle.Trim()
            if ($handle -ne [IntPtr]::Zero -and [P910Native]::IsWindowVisibleForProcess($handle) -and [P910Native]::GetWindowProcessId($handle) -eq [int]$process.Id -and $title -match "ClassMngr Next") {
                if ($stableHandle -eq $handle.ToInt64() -and $stableTitle -ceq $title) { ++$stable } else { $stableHandle = $handle.ToInt64(); $stableTitle = $title; $stable = 1 }
            } else { $stable = 0; $stableHandle = 0L; $stableTitle = "" }
            if ($stable -lt 3) { Start-Sleep -Milliseconds 250 }
        }
        if ($stable -lt 3) { throw "$RouteId did not expose a stable verified main window." }
        $context.mainWindowHandle = $stableHandle; $context.mainWindowTitle = $stableTitle
        $route.processIdentity.mainWindowHandle = $stableHandle
        $route.processIdentity.mainWindowTitle = $stableTitle
        $route.processIdentity.mainWindowProcessId = [Int64][P910Native]::GetWindowProcessId([IntPtr]$stableHandle)

        $calendarNames = @("Calendar", (-join [char[]]@(0xB2EC, 0xB825)), (-join [char[]]@(0xCE98, 0xB9B0, 0xB354)))
        $calendarNavigation = Get-UiaByNamesExactly $context $calendarNames
        $navigationBeforePath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-before-calendar-navigation.json")
        $route.uia.navigationBefore = Save-UiaTreeSnapshot $context $navigationBeforePath
        $preCalendarRecords = @(Get-LifecycleRecords $context)
        $preCalendarIndex = if ($preCalendarRecords.Count -gt 0) { [int]$preCalendarRecords[-1].observedLogIndex } else { -1 }
        $null = Add-UiaAction $context $calendarNavigation "navigate-to-calendar" "startup" "calendar"
        $navigationAfterPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-after-calendar-navigation.json")
        $route.uia.navigationAfterAction = Save-UiaTreeSnapshot $context $navigationAfterPath
        $activated = $null
        $usedNavigationFallback = $false
        try { $activated = Wait-ForLifecycle $context "calendar" "activated" $preCalendarIndex 1 } catch { }
        if ($null -eq $activated) {
            # Qt's tree-item UIA selection reports success on this runtime without
            # delivering QTreeWidget's currentItemChanged event. Keep UIA as the
            # authoritative target, then dispatch at that exact element point
            # only after verifying it lies in this PID/HWND client area.
            $usedNavigationFallback = $true
            $route.uia.navigationFallback = Add-UiaClickAction $context $calendarNavigation "navigate-to-calendar" "startup" "calendar"
            $navigationAfterClickPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-after-calendar-click.json")
            $route.uia.navigationAfterClick = Save-UiaTreeSnapshot $context $navigationAfterClickPath
            try { $activated = Wait-ForLifecycle $context "calendar" "activated" $preCalendarIndex 30 }
            catch {
            $navigationTimeoutPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-calendar-navigation-timeout.json")
            $route.uia.navigationAtTimeout = Save-UiaTreeSnapshot $context $navigationTimeoutPath
            throw
            }
        }
        $route.uia.navigationActivatedBySelectionPattern = (-not $usedNavigationFallback)
        $route.lifecycle.calendarActivated = $activated
        $baselineLoad = Wait-ForCalendarLoad $context 0 30
        $baselineUiaPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-calendar-baseline.json")
        $baselineUia = Save-UiaControlSnapshot $context $baselineUiaPath @("nextCalendarPage", "calendarPreferences", "calendarUpcomingEvents", "calendarPageStatus")
        $route.calendarBaseline = [ordered]@{
            capturedAtUtc = Get-UtcTimestamp; pageAutomationId = [string]$baselineLoad.page.automationId
            processId = [Int64]$process.Id; windowProcessId = [Int64][P910Native]::GetWindowProcessId([IntPtr]$context.mainWindowHandle)
            lifecycleRecord = $activated; calendarRuntimeMetrics = $activated.page.calendar_runtime_metrics
            cacheLoadWait = $baselineLoad; uiaSnapshot = $baselineUia
        }
        if ($route.calendarBaseline.windowProcessId -ne [Int64]$process.Id) { throw "$RouteId Calendar baseline HWND is not bound to the measured PID." }

        $beforeSnapshot = Invoke-PythonJson (Join-Path $PSScriptRoot "snapshot_calendar_import_workspace.py") @("--workspace", $workspacePath, "--output", $beforeSnapshotPath) (Join-Path (Join-Path $EvidenceDirectory "logs") ($RouteId + "-before-snapshot.log")) "$RouteId before workspace snapshot"
        $beforeDocument = Get-Content -LiteralPath $beforeSnapshotPath -Raw | ConvertFrom-Json -ErrorAction Stop
        if ($beforeDocument.integrityCheck -cne "ok" -or [int]$beforeDocument.tables.calendar_events.rowCount -ne 180) { throw "$RouteId seeded before snapshot failed integrity/count checks." }
        $route.workspace.beforeTableContentSha256 = [string]$beforeDocument.tableContentSha256
        $route.workspace.beforeCalendarEventCount = [int]$beforeDocument.tables.calendar_events.rowCount

        $preferencesButton = Get-PageButtonById $context "calendarPreferences"
        $null = Add-UiaAction $context $preferencesButton "open-calendar-preferences" "calendar" "calendar_preferences"
        $dialog = Wait-UiaById $context "nextCalendarPreferencesDialog" "ControlType.Window" 20
        $dialogPid = [Int64]$dialog.Current.ProcessId
        if ($dialogPid -ne [Int64]$process.Id) { throw "$RouteId Calendar preferences dialog belongs to another PID." }
        $dialogHandle = [IntPtr]$dialog.Current.NativeWindowHandle
        if ($dialogHandle -eq [IntPtr]::Zero -or [P910Native]::GetWindowProcessId($dialogHandle) -ne [int]$process.Id) { throw "$RouteId Calendar preferences dialog HWND identity is invalid." }
        $importButton = Get-PageButtonById $context "calendarImportEvents"
        if (-not $importButton.Current.IsEnabled) { throw "$RouteId import control was not enabled before the request." }
        $isSuccess = $FixtureRole -ceq "success"
        $importRequestedAtUtc = Get-UtcTimestamp
        $importCheckpointStartLogIndex = @(Get-ProcessLogLines $context).Count - 1
        $route.import = [ordered]@{
            requestedAtUtc = $importRequestedAtUtc
            stagingDirectory = [IO.Path]::GetFullPath($tempDirectory)
        }
        $null = Add-UiaAction $context $importButton "start-calendar-import" "calendar_preferences" "calendar_import"
        if (-not $server.RequestSeen.Wait(10000)) { throw "$RouteId local server did not receive the UIA import request." }
        if (-not $server.ResponseCompleted.Wait(10000)) { throw "$RouteId loopback server did not complete the exact fixture response." }
        if (-not [string]::IsNullOrWhiteSpace($server.Error)) { throw "$RouteId loopback fixture server failed: $($server.Error)" }
        if ([int]$server.RequestCount -ne 1 -or $server.RequestPath -cne "/calendar-import" -or $server.RemoteAddress -cne "127.0.0.1" -or [int]$server.ResponseBytesSent -ne $payload.Length) { throw "$RouteId loopback server request/response identity differs from its exact fixture." }
        $route.memory.operationSamples += ,(New-ProcessSample $context "fixture-response-completed")

        $terminalStatus = Wait-ForCondition $context { param($ctx)
            $buttonMatches = @(Find-UiaById $ctx "calendarImportEvents" "ControlType.Button")
            $text = Get-UiaControlText $ctx "calendarImportStatus"
            if ($buttonMatches.Count -eq 1 -and $buttonMatches[0].Current.IsEnabled -and -not [string]::IsNullOrWhiteSpace($text)) {
                if ($isSuccess -and $text -match "261" -and $text -match "104") { return $text }
                if (-not $isSuccess -and $text -match "(?i)Calendar import failed") { return $text }
            }
            return $null
        } "Calendar import terminal UI result" 60
        $terminalCheckpoint = Wait-ForCalendarImportCheckpoint $context "terminal" $importCheckpointStartLogIndex 15
        $terminalRuntimeMetrics = $terminalCheckpoint.calendar_runtime_metrics
        $terminalServiceMetrics = $terminalRuntimeMetrics.calendar_import
        if ($terminalCheckpoint.processId -ne [Int64]$process.Id -or $terminalCheckpoint.mainWindowHandle -ne [Int64]$context.mainWindowHandle) { throw "$RouteId terminal import checkpoint is not bound to the measured PID/HWND." }
        if ($terminalServiceMetrics.terminal_status -cne $(if ($isSuccess) { "completed" } else { "failed" })) { throw "$RouteId terminal service status differs from its visible UI result." }
        if ([Int64]$terminalServiceMetrics.downloaded_bytes -ne [Int64]$payload.Length) { throw "$RouteId service download byte count differs from the complete fixture." }
        if (-not $terminalServiceMetrics.staged_workbook_released_before_apply -or -not $terminalServiceMetrics.staged_workbook_removed_at_terminal) { throw "$RouteId service did not prove staged workbook removal before apply and at terminal." }
        if ($isSuccess) {
            if ([int]$terminalServiceMetrics.imported_event_count -ne 261 -or [int]$terminalServiceMetrics.skipped_event_count -ne 104) { throw "$RouteId service terminal counters differ from 261 imported and 104 skipped." }
            if ([int]$terminalServiceMetrics.typed_candidate_count_at_apply -ne 261 -or [int]$terminalServiceMetrics.parsed_event_count -ne 261) { throw "$RouteId service parse/apply candidate counters differ from fixture expectations." }
            if (-not $terminalServiceMetrics.used_device_parser) { throw "$RouteId service did not use the workbook device parser." }
        } else {
            if ([int]$terminalServiceMetrics.parsed_event_count -ne 0 -or [int]$terminalServiceMetrics.typed_candidate_count_at_apply -ne 0 -or [int]$terminalServiceMetrics.imported_event_count -ne 0 -or [int]$terminalServiceMetrics.skipped_event_count -ne 0) { throw "$RouteId parser-error route reported parsed or applied events." }
            if ($terminalRuntimeMetrics.calendar_import_cache_refresh_count -ne 0) { throw "$RouteId parser-error route unexpectedly refreshed the Calendar event cache." }
        }
        $cacheSettledCheckpoint = Wait-ForCalendarImportCheckpoint $context "cache-settled" ([int]$terminalCheckpoint.observedLogIndex) 40
        $settledRuntimeMetrics = $cacheSettledCheckpoint.calendar_runtime_metrics
        $expectedRefreshCount = if ($isSuccess) { 1 } else { 0 }
        if ($cacheSettledCheckpoint.processId -ne [Int64]$process.Id -or $cacheSettledCheckpoint.mainWindowHandle -ne [Int64]$context.mainWindowHandle) { throw "$RouteId settled cache checkpoint is not bound to the measured PID/HWND." }
        if ($settledRuntimeMetrics.cache_loading -ne $false -or $settledRuntimeMetrics.upcoming_range_loaded -ne $true -or [int]$settledRuntimeMetrics.calendar_import_cache_refresh_count -ne $expectedRefreshCount) { throw "$RouteId cache did not reach the expected settled post-import state." }
        $route.import.serviceTerminalCheckpoint = $terminalCheckpoint
        $route.import.cacheSettledCheckpoint = $cacheSettledCheckpoint
        $route.import.runtimeMetrics = $terminalServiceMetrics
        $route.import.cacheRefreshCountAtTerminal = [int]$terminalRuntimeMetrics.calendar_import_cache_refresh_count
        $route.import.cacheRefreshCountAtSettled = [int]$settledRuntimeMetrics.calendar_import_cache_refresh_count
        $route.memory.operationSamples += ,(New-ProcessSample $context "cache-settled")

        $serverRecord = [ordered]@{
            routeId = $RouteId; pid = [Int64]$process.Id; implementation = "in-process loopback TCP HTTP fixture server"
            host = "127.0.0.1"; port = [int]$server.Port; url = [string]$server.Url
            requestCount = [int]$server.RequestCount; requestPath = [string]$server.RequestPath; remoteAddress = [string]$server.RemoteAddress
            responseBytesSent = [int]$server.ResponseBytesSent; responseSha256 = [string]$server.ResponseSha256
            responseMode = "full-fixture-response-with-no-staging-visibility-gate"
            responseCompleted = [bool]$server.ResponseCompleted.IsSet; aborted = [bool]$server.IsAborted
            fixtureName = $FixtureName; fixtureSha256 = [string]$Fixture.sha256; completedAtUtc = Get-UtcTimestamp
            error = [string]$server.Error
        }
        $route.server = $serverRecord
        # Final server identity is emitted from the route cleanup block.
        $route.memory.operationSamples += ,(New-ProcessSample $context "parse-completed")

        $importButtonAfter = Get-PageButtonById $context "calendarImportEvents"
        $statusText = [string]$terminalStatus
        $failureText = ""
        if (-not $isSuccess) {
            $failureText = [regex]::Replace($statusText, '^.*?Calendar import failed:\s*', '', [Text.RegularExpressions.RegexOptions]::IgnoreCase)
            if ($statusText -notmatch '(?i)xl/workbook\.xml') { throw "$RouteId visible parser error did not name missing xl/workbook.xml: $statusText" }
        }
        if (-not $importButtonAfter.Current.IsEnabled) { throw "$RouteId import control remained disabled after the terminal result." }
        $tempFilesAfterResult = @(Get-TempFiles $tempDirectory)
        $route.import.tempFilesAfterResult = $tempFilesAfterResult
        $route.import.terminalStatusText = $statusText
        if ($tempFilesAfterResult.Count -ne 0) { throw "$RouteId process TEMP still contains files after import cleanup." }
        $terminalUiaPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-import-terminal.json")
        $terminalUia = Save-UiaControlSnapshot $context $terminalUiaPath @("nextCalendarPreferencesDialog", "calendarImportEvents", "calendarImportStatus")
        Add-TeardownCheckpoint $context $route "before-preferences-close"
        $route.import = [ordered]@{
            requestedAtUtc = $importRequestedAtUtc
            completedAtUtc = Get-UtcTimestamp
            statusText = $statusText
            stagingDirectory = [IO.Path]::GetFullPath($tempDirectory)
            tempFilesAfterResult = $tempFilesAfterResult
            runtimeMetrics = $terminalServiceMetrics
            serviceTerminalCheckpoint = $terminalCheckpoint
            cacheSettledCheckpoint = $cacheSettledCheckpoint
            cacheRefreshCountAtTerminal = [int]$terminalRuntimeMetrics.calendar_import_cache_refresh_count
            cacheRefreshCountAtSettled = [int]$settledRuntimeMetrics.calendar_import_cache_refresh_count
            ui = [ordered]@{
                processId = [Int64]$process.Id; mainWindowHandle = [Int64]$context.mainWindowHandle
                dialogProcessId = $dialogPid; dialogWindowHandle = [Int64]$dialogHandle.ToInt64()
                dialogWindowProcessId = [Int64][P910Native]::GetWindowProcessId($dialogHandle)
                importButtonEnabledAfterResult = [bool]$importButtonAfter.Current.IsEnabled
                statusText = $statusText; failureText = $failureText
                terminalSnapshot = $terminalUia
            }
        }

        $cancelButtons = @(Find-UiaByName $context @("Cancel", (-join [char[]]@(0xCDE8, 0xC18C))) "ControlType.Button")
        if ($cancelButtons.Count -ne 1) { throw "$RouteId preferences dialog did not expose exactly one UIA Cancel button." }
        $null = Add-UiaAction $context $cancelButtons[0] "close-calendar-preferences" "calendar_preferences" "calendar"
        Wait-ForCondition $context { param($ctx) @(Find-UiaById $ctx "nextCalendarPreferencesDialog").Count -eq 0 } "Calendar preferences dialog close" 10 | Out-Null
        Add-TeardownCheckpoint $context $route "after-preferences-close"

        $otherNames = @("My Workspace", (-join [char[]]@(0xB0B4, 0x0020, 0xC791, 0xC5C5, 0xACF5, 0xAC04)))
        $otherNavigation = Get-UiaByNamesExactly $context $otherNames
        $beforeSuspendRecords = @(Get-LifecycleRecords $context)
        $beforeSuspendIndex = if ($beforeSuspendRecords.Count -gt 0) { [int]$beforeSuspendRecords[-1].observedLogIndex } else { -1 }
        Add-TeardownCheckpoint $context $route "before-calendar-suspend"
        $route.uia.suspendSelectionAction = Add-UiaAction $context $otherNavigation "suspend-calendar-to-my-workspace" "calendar" "my_workspace"
        $suspended = $null
        try { $suspended = Wait-ForLifecycle $context "calendar" "suspended" $beforeSuspendIndex 1 } catch { }
        if ($null -eq $suspended) {
            $afterSelectionPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-after-calendar-suspend-selection.json")
            $route.uia.afterSuspendSelection = Save-UiaTreeSnapshot $context $afterSelectionPath
            $otherNavigation = Get-UiaByNamesExactly $context $otherNames
            $route.uia.suspensionFallback = Add-UiaClickAction $context $otherNavigation "suspend-calendar-to-my-workspace" "calendar" "my_workspace"
            $afterClickPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-after-calendar-suspend-click.json")
            $route.uia.afterSuspendClick = Save-UiaTreeSnapshot $context $afterClickPath
            try { $suspended = Wait-ForLifecycle $context "calendar" "suspended" $beforeSuspendIndex 30 }
            catch {
                $timeoutPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-calendar-suspend-timeout.json")
                $route.uia.suspensionAtTimeout = Save-UiaTreeSnapshot $context $timeoutPath
                throw
            }
        }
        $route.lifecycle.calendarSuspended = $suspended
        $route.lifecycle.suspendCount = 1
        $route.import.runtimeMetrics = $suspended.page.calendar_runtime_metrics.calendar_import
        $route.import.cacheRefreshCountAtSuspension = [int]$suspended.page.calendar_runtime_metrics.calendar_import_cache_refresh_count
        Add-TeardownCheckpoint $context $route "after-calendar-suspend"

        $beforeReentryRecords = @(Get-LifecycleRecords $context)
        $beforeReentryIndex = if ($beforeReentryRecords.Count -gt 0) { [int]$beforeReentryRecords[-1].observedLogIndex } else { -1 }
        $calendarNavigation = Get-UiaByNamesExactly $context $calendarNames
        $route.uia.reentrySelectionAction = Add-UiaAction $context $calendarNavigation "reenter-calendar" "my_workspace" "calendar"
        $reentered = $null
        try { $reentered = Wait-ForLifecycle $context "calendar" "activated" $beforeReentryIndex 1 } catch { }
        if ($null -eq $reentered) {
            $afterSelectionPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-after-calendar-reentry-selection.json")
            $route.uia.afterReentrySelection = Save-UiaTreeSnapshot $context $afterSelectionPath
            $calendarNavigation = Get-UiaByNamesExactly $context $calendarNames
            $route.uia.reentryFallback = Add-UiaClickAction $context $calendarNavigation "reenter-calendar" "my_workspace" "calendar"
            $afterClickPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-after-calendar-reentry-click.json")
            $route.uia.afterReentryClick = Save-UiaTreeSnapshot $context $afterClickPath
            try { $reentered = Wait-ForLifecycle $context "calendar" "activated" $beforeReentryIndex 30 }
            catch {
                $timeoutPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-calendar-reentry-timeout.json")
                $route.uia.reentryAtTimeout = Save-UiaTreeSnapshot $context $timeoutPath
                throw
            }
        }
        $route.lifecycle.calendarReentered = $reentered
        $route.lifecycle.reentryCount = 1
        $reentryCacheSettled = $null
        try {
            $reentryCacheSettled = Wait-ForCalendarImportCheckpoint $context "cache-settled" ([int]$reentered.observedLogIndex) 40
        } catch {
            $timeoutUiaPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-calendar-reentry-cache-timeout.json")
            $timeoutUia = Save-UiaControlSnapshot $context $timeoutUiaPath @("nextCalendarPage", "calendarUpcomingEvents", "calendarPageStatus")
            $timeoutState = Get-CalendarLoadState $context
            $postReentryCheckpoints = @(Get-CalendarImportCheckpoints $context | Where-Object { [int]$_.observedLogIndex -gt [int]$reentered.observedLogIndex })
            $route.calendarLoadWait = [ordered]@{
                completed = $false; outcome = "post-reentry-cache-timeout"; waitedAtUtc = Get-UtcTimestamp
                cacheSettledCheckpoint = $null; cacheCheckpointsAfterReentry = $postReentryCheckpoints
                currentUiState = $timeoutState; timeoutUiaSnapshot = $timeoutUia
                cacheLoadingAtActivation = [bool]$reentered.page.calendar_runtime_metrics.cache_loading
                cacheEventCountAtActivation = [int]$reentered.page.calendar_runtime_metrics.cache_event_count
                cacheRefreshCountAtActivation = [int]$reentered.page.calendar_runtime_metrics.calendar_import_cache_refresh_count
            }
            throw
        }
        $reentryCacheMetrics = $reentryCacheSettled.calendar_runtime_metrics
        if ($reentryCacheMetrics.cache_loading -ne $false -or $reentryCacheMetrics.upcoming_range_loaded -ne $true -or [int]$reentryCacheMetrics.calendar_import_cache_refresh_count -ne $expectedRefreshCount) {
            throw "$RouteId Calendar cache was not settled after page re-entry."
        }
        $minimumItems = if ($isSuccess) { 1 } else { 0 }
        $loadWait = $null
        try { $loadWait = Wait-ForCalendarLoad $context $minimumItems 40 }
        catch {
            $timeoutUiaPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-calendar-reentry-ui-timeout.json")
            $timeoutUia = Save-UiaControlSnapshot $context $timeoutUiaPath @("nextCalendarPage", "calendarUpcomingEvents", "calendarPageStatus")
            $timeoutState = Get-CalendarLoadState $context
            $route.calendarLoadWait = [ordered]@{
                completed = $false; outcome = "post-reentry-ui-timeout"; waitedAtUtc = Get-UtcTimestamp
                cacheSettledCheckpoint = $reentryCacheSettled; currentUiState = $timeoutState
                timeoutUiaSnapshot = $timeoutUia
                cacheLoadingAtActivation = [bool]$reentered.page.calendar_runtime_metrics.cache_loading
                cacheLoadingAtSettle = [bool]$reentryCacheMetrics.cache_loading
                cacheEventCountAtActivation = [int]$reentered.page.calendar_runtime_metrics.cache_event_count
                cacheRefreshCountAtActivation = [int]$reentered.page.calendar_runtime_metrics.calendar_import_cache_refresh_count
            }
            throw
        }
        $route.calendarLoadWait = [ordered]@{
            completed = [bool]$loadWait.completed; waitedAtUtc = Get-UtcTimestamp
            cacheSettledCheckpoint = $reentryCacheSettled
            upcomingItemCount = [int]$loadWait.upcomingItemCount
            statusText = [string]$loadWait.statusText
            loadingIndicatorsRemaining = [int]$loadWait.loadingIndicatorsRemaining
            loadingNames = @($loadWait.loadingNames)
            cacheLoadingAtActivation = [bool]$reentered.page.calendar_runtime_metrics.cache_loading
            cacheLoadingAtSettle = [bool]$reentryCacheMetrics.cache_loading
            cacheEventCountAtActivation = [int]$reentered.page.calendar_runtime_metrics.cache_event_count
            cacheRefreshCountAtActivation = [int]$reentered.page.calendar_runtime_metrics.calendar_import_cache_refresh_count
            cacheRefreshCountAtSettle = [int]$reentryCacheMetrics.calendar_import_cache_refresh_count
        }
        $reentryUiaPath = Join-Path (Join-Path $EvidenceDirectory "uia") ($RouteId + "-calendar-reentry.json")
        $reentryUia = Save-UiaControlSnapshot $context $reentryUiaPath @("nextCalendarPage", "calendarUpcomingEvents", "calendarPageStatus")
        $route.calendarLoadWait.uiaSnapshot = $reentryUia
        if ($loadWait.loadingIndicatorsRemaining -ne 0) { throw "$RouteId re-entered Calendar with a visible loading indicator." }
        if ($isSuccess -and [int]$loadWait.upcomingItemCount -le 0) { throw "$RouteId re-entered Calendar without rendering imported events." }

        $afterSnapshot = Invoke-PythonJson (Join-Path $PSScriptRoot "snapshot_calendar_import_workspace.py") @("--workspace", $workspacePath, "--output", $afterSnapshotPath) (Join-Path (Join-Path $EvidenceDirectory "logs") ($RouteId + "-after-snapshot.log")) "$RouteId after workspace snapshot"
        $afterDocument = Get-Content -LiteralPath $afterSnapshotPath -Raw | ConvertFrom-Json -ErrorAction Stop
        if ($afterDocument.integrityCheck -cne "ok") { throw "$RouteId persisted workspace failed SQLite integrity check." }
        $route.workspace.afterTableContentSha256 = [string]$afterDocument.tableContentSha256
        $route.workspace.afterCalendarEventCount = [int]$afterDocument.tables.calendar_events.rowCount
        if ($isSuccess) {
            $route.workspace.afterCountMatchesExpectedTotal = ([int]$route.workspace.afterCalendarEventCount -eq 441)
            if (-not $route.workspace.afterCountMatchesExpectedTotal) { throw "$RouteId persisted Calendar event row count was $($route.workspace.afterCalendarEventCount), expected 441." }
        } else {
            $route.workspace.rowLevelContentsUnchanged = ([string]$route.workspace.beforeTableContentSha256 -ceq [string]$route.workspace.afterTableContentSha256)
            if (-not $route.workspace.rowLevelContentsUnchanged) { throw "$RouteId parser error changed full before/after workspace rows." }
        }

        $postCleanup = New-ProcessSample $context "post-cleanup"
        Assert-StrictWorkingSetCheckpoint $postCleanup "$RouteId post-cleanup"
        $route.memory.normalUseSamples += ,$postCleanup
        for ($sampleIndex = 0; $sampleIndex -lt $IdleTailSamples; $sampleIndex++) {
            Start-Sleep -Seconds 1
            $tail = New-ProcessSample $context "post-cleanup-idle-tail" $sampleIndex
            Assert-StrictWorkingSetCheckpoint $tail "$RouteId post-cleanup idle tail sample $sampleIndex"
            $route.memory.idleTail += ,$tail
            $route.memory.normalUseSamples += ,$tail
        }
        $route.memory.operationSamples = @($route.memory.operationSamples)
        $route.memory.normalUseSamples = @($route.memory.normalUseSamples)
        $route.processExitCode = Stop-MeasuredProcess $context
        if ($null -ne $script:LastShutdownObservability -and $script:LastShutdownObservability.routeId -ceq $RouteId) {
            $route.diagnostics.shutdownObservability = $script:LastShutdownObservability
            $route.diagnostics.shutdownMiniDump = $script:LastShutdownObservability.shutdownMiniDump
        }
        if ($null -ne $route.diagnostics.shutdownMiniDump -and $route.diagnostics.shutdownMiniDump.status -ceq "failed") {
            throw "Shutdown MiniDumpWriteDump capture failed: $($route.diagnostics.shutdownMiniDump.errorMessage)"
        }
        if ($null -ne $route.processExitCode -and [int]$route.processExitCode -ne 0) { throw "$RouteId packaged app exited with code $($route.processExitCode)." }
        $route.workspace.sha256AtExit = Get-FileSha256 $workspacePath
        # Final server state is attached in this function's cleanup block.
        return $route
    } finally {
        try { if (-not $server.ResponseCompleted.IsSet) { $server.Abort() } } catch { }
        try {
            $finalServerRecord = [ordered]@{
                routeId = $RouteId; pid = if ($null -ne $process) { [Int64]$process.Id } else { $null }
                implementation = "in-process loopback TCP HTTP fixture server"
                host = "127.0.0.1"; port = [int]$server.Port; url = [string]$server.Url
                requestCount = [int]$server.RequestCount; requestPath = [string]$server.RequestPath; remoteAddress = [string]$server.RemoteAddress
                responseBytesSent = [int]$server.ResponseBytesSent; responseSha256 = [string]$server.ResponseSha256
                responseMode = "full-fixture-response-with-no-staging-visibility-gate"
                fixtureName = $FixtureName; fixtureSha256 = [string]$Fixture.sha256
                responseCompleted = [bool]$server.ResponseCompleted.IsSet; aborted = [bool]$server.IsAborted
                error = [string]$server.Error; completedAtUtc = Get-UtcTimestamp
            }
            $route.server = $finalServerRecord
            Add-JsonLine $ServerLogPath $finalServerRecord
        } catch { }
        try { $server.Dispose() } catch { }
        if ($null -ne $process) {
            try {
                $process.Refresh()
                if (-not $process.HasExited) {
                    $shutdownContext = if ($null -ne $context) { $context } else { [ordered]@{ routeId = $RouteId; process = $process; mainWindowHandle = [Int64]$route.processIdentity.mainWindowHandle; createdAtUtc = [string]$route.processIdentity.createdAtUtc; shutdownDumpPath = [IO.Path]::GetFullPath($shutdownDumpPath) } }
                    $route.processExitCode = Stop-MeasuredProcess $shutdownContext
                }
            } catch { }
            if ($null -ne $script:LastShutdownObservability -and $script:LastShutdownObservability.routeId -ceq $RouteId) {
                $route.diagnostics.shutdownObservability = $script:LastShutdownObservability
                $route.diagnostics.shutdownMiniDump = $script:LastShutdownObservability.shutdownMiniDump
            }
            try { if ($process.HasExited) { $route.processExitCode = [int]$process.ExitCode } } catch { }
        }
        try { $null = Save-ApplicationShutdownTrace $route $shutdownTracePath $RouteId } catch {
            $route.diagnostics.applicationShutdownTrace = [ordered]@{
                schema = "classmngr-phase9-application-shutdown-trace-link-v1"
                routeId = $RouteId
                processId = if ($null -ne $process) { [Int64]$process.Id } else { $null }
                status = "link-error"
                path = [IO.Path]::GetFullPath($shutdownTracePath)
                error = $_.Exception.Message
            }
        }
    }
}

function Copy-VerifiedFixture {
    param([string]$Name, [string]$RelativePath, [string]$ExpectedSha256, [Int64]$ExpectedSize)
    $sourcePath = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $RelativePath))
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) { throw "Required Calendar response fixture is missing: $sourcePath" }
    $evidencePath = Join-Path (Join-Path $EvidenceDirectory "fixtures") $Name
    Copy-Item -LiteralPath $sourcePath -Destination $evidencePath
    $sourceHash = Get-FileSha256 $sourcePath
    $copyHash = Get-FileSha256 $evidencePath
    if ($sourceHash -cne $ExpectedSha256 -or $copyHash -cne $ExpectedSha256 -or [Int64](Get-Item -LiteralPath $sourcePath).Length -ne $ExpectedSize) {
        throw "Calendar response fixture identity differs from accepted bytes: $Name"
    }
    [ordered]@{
        name = $Name; sourcePath = $sourcePath; sourceSha256 = $sourceHash
        evidencePath = [IO.Path]::GetFullPath($evidencePath); sha256 = $copyHash
        sizeBytes = [Int64](Get-Item -LiteralPath $evidencePath).Length
    }
}

function Invoke-BlankWorkspaceGenerator {
    param([string]$WorkspacePath)
    $generatorPath = Join-Path $BuildDirectory "Release\ClassMngrPhase7NextBlankFixtureTests.exe"
    if (-not (Test-Path -LiteralPath $generatorPath -PathType Leaf) -or (Get-PeArchitecture $generatorPath) -cne "x64") { throw "Current-source x64 blank workspace generator is missing." }
    if (Test-Path -LiteralPath $WorkspacePath) { throw "Refusing to overwrite blank workspace $WorkspacePath." }
    $environmentName = "CLASSMNGR_PHASE7_SCHEMA_ONLY_FIXTURE_OUTPUT_PATH"
    $oldValue = [Environment]::GetEnvironmentVariable($environmentName, "Process")
    $oldPath = [Environment]::GetEnvironmentVariable("PATH", "Process")
    $logPath = Join-Path (Join-Path $EvidenceDirectory "logs") "blank-workspace-generator.log"
    try {
        [Environment]::SetEnvironmentVariable($environmentName, $WorkspacePath, "Process")
        [Environment]::SetEnvironmentVariable("PATH", "$script:QtBin;$oldPath", "Process")
        $output = @(& $generatorPath schemaOnlyBlankWorkspaceIsValidAndDeterministic 2>&1 | ForEach-Object { [string]$_ })
        $exitCode = $LASTEXITCODE
        [IO.File]::WriteAllLines($logPath, @("command: $generatorPath schemaOnlyBlankWorkspaceIsValidAndDeterministic", "exitCode: $exitCode", $output), [Text.UTF8Encoding]::new($false))
        if ($exitCode -ne 0) { throw "Current-source blank workspace generator failed with exit code $exitCode." }
    } finally {
        [Environment]::SetEnvironmentVariable($environmentName, $oldValue, "Process")
        [Environment]::SetEnvironmentVariable("PATH", $oldPath, "Process")
    }
    if (-not (Test-Path -LiteralPath $WorkspacePath -PathType Leaf)) { throw "Blank workspace generator produced no file." }
    [ordered]@{
        generatorPath = [IO.Path]::GetFullPath($generatorPath); generatorSha256 = Get-FileSha256 $generatorPath
        workspacePath = [IO.Path]::GetFullPath($WorkspacePath); workspaceSha256 = Get-FileSha256 $WorkspacePath
        logPath = [IO.Path]::GetFullPath($logPath); logSha256 = Get-FileSha256 $logPath
    }
}

function Get-PackageRuntimeFiles {
    param([string]$Directory, [string]$QtPrefix)
    $required = @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Sql.dll", "platforms\qwindows.dll", "sqldrivers\qsqlite.dll")
    foreach ($relative in $required) {
        $path = Join-Path $Directory $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Staged package is missing required Qt runtime file $relative." }
        [ordered]@{ relativePath = $relative; sizeBytes = [Int64](Get-Item -LiteralPath $path).Length; sha256 = Get-FileSha256 $path; sourcePrefix = $QtPrefix }
    }
}

function Set-RunnerFailure {
    param([System.Management.Automation.ErrorRecord]$Record)
    $script:Manifest.status = "failed"
    $script:Manifest.failure = [ordered]@{
        timestampUtc = Get-UtcTimestamp; message = [string]$Record.Exception.Message
        category = [string]$Record.CategoryInfo.Category; scriptStackTrace = [string]$Record.ScriptStackTrace
    }
}

try {
    if ($env:OS -cne "Windows_NT") { throw "P9-10 packaged Calendar Import acceptance requires local Windows." }
    if ($IdleTailSamples -ne 10) { throw "P9-10 requires exactly ten post-cleanup idle-tail samples." }
    $script:Manifest.runnerIdentity = [ordered]@{ path = [IO.Path]::GetFullPath($PSCommandPath); sha256 = Get-FileSha256 $PSCommandPath }
    $validatorPath = Join-Path $PSScriptRoot "validate_packaged_next_calendar_import.py"
    if (-not (Test-Path -LiteralPath $validatorPath -PathType Leaf)) { throw "P9-10 independent validator is missing." }
    $script:Manifest.validatorIdentity = [ordered]@{ path = [IO.Path]::GetFullPath($validatorPath); sha256 = Get-FileSha256 $validatorPath }

    $source = Get-SourceInventory $RepositoryRoot
    $script:Manifest.source = [ordered]@{
        root = $source.root; sha256 = $source.sha256; fileCount = $source.fileCount
        includedRoots = $source.includedRoots; inventoryPath = [IO.Path]::GetFullPath($SourceInventoryPath)
    }
    Write-JsonFile $SourceInventoryPath ([ordered]@{ files = @($source.files) })

    $cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { throw "Existing x64 Release CMake cache is missing: $cachePath" }
    $cacheLines = @(Get-Content -LiteralPath $cachePath)
    if ([IO.Path]::GetFullPath((Get-CacheValue $cacheLines "CMAKE_HOME_DIRECTORY")) -ine $RepositoryRoot) { throw "Release CMake cache points at a different source tree." }
    if ((Get-CacheValue $cacheLines "CLASSMNGR_NEXT_RELEASE_PACKAGE") -cne "ON") { throw "Release CMake tree does not enable the current-source Next package." }
    $generator = Get-CacheValue $cacheLines "CMAKE_GENERATOR"
    if ($generator -notmatch '^Visual Studio' -or (Get-CacheValue $cacheLines "CMAKE_GENERATOR_PLATFORM") -notin @("x64", "")) { throw "Release CMake tree is not a Windows x64 Visual Studio build." }
    $cmake = [IO.Path]::GetFullPath((Get-CacheValue $cacheLines "CMAKE_COMMAND"))
    $qt6Directory = [IO.Path]::GetFullPath((Get-CacheValue $cacheLines "Qt6_DIR"))
    $script:QtPrefix = [IO.Path]::GetFullPath((Join-Path $qt6Directory "..\..\.."))
    $script:QtBin = Join-Path $script:QtPrefix "bin"
    $windeployqt = Join-Path $script:QtBin "windeployqt.exe"
    if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) { throw "Cache-bound Qt windeployqt is missing." }
    $qtCore = Join-Path $script:QtBin "Qt6Core.dll"
    if (-not (Test-Path -LiteralPath $qtCore -PathType Leaf)) { throw "Cache-bound Qt6Core is missing." }
    $qtVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($qtCore).ProductVersion
    if ([Diagnostics.FileVersionInfo]::GetVersionInfo($windeployqt).ProductVersion -ne $qtVersion) { throw "windeployqt version differs from the Qt6Core version." }
    $script:Manifest.qt = [ordered]@{
        prefix = $script:QtPrefix; qt6Directory = $qt6Directory; version = $qtVersion
        coreDllPath = $qtCore; coreDllSha256 = Get-FileSha256 $qtCore
        windeployqtPath = [IO.Path]::GetFullPath($windeployqt); windeployqtSha256 = Get-FileSha256 $windeployqt
    }

    $script:Manifest.build = [ordered]@{
        directory = $BuildDirectory; cachePath = [IO.Path]::GetFullPath($cachePath)
        cacheSha256AtStart = Get-FileSha256 $cachePath; generator = $generator
        configuration = "Release"; platform = "x64"; packageOption = "CLASSMNGR_NEXT_RELEASE_PACKAGE=ON"
        startupUpdateCheck = "OFF"; cmakePath = $cmake
        targets = @("ClassMngrNext", "ClassMngrNextResourcePackage", "ClassMngrPhase7NextBlankFixtureTests")
    }
    if ([string]::IsNullOrWhiteSpace($PrebuiltBuildReceiptPath)) {
        $oldPath = [Environment]::GetEnvironmentVariable("PATH", "Process")
        try {
            [Environment]::SetEnvironmentVariable("PATH", "$script:QtBin;$oldPath", "Process")
            $configureResult = Invoke-CapturedCommand $cmake @(
                "-S", $RepositoryRoot, "-B", $BuildDirectory,
                "-DCLASSMNGR_NEXT_RELEASE_PACKAGE=ON", "-DCLASSMNGR_UPDATE_CHECK_ON_STARTUP=OFF"
            ) $ConfigureLogPath "Release CMake configure"
            $script:Manifest.build.configureLogPath = $configureResult.logPath
            $script:Manifest.build.configureLogSha256 = $configureResult.logSha256
            $script:Manifest.build.configureStdoutLogPath = $configureResult.stdoutLogPath
            $script:Manifest.build.configureStdoutLogSha256 = $configureResult.stdoutLogSha256
            $script:Manifest.build.configureStderrLogPath = $configureResult.stderrLogPath
            $script:Manifest.build.configureStderrLogSha256 = $configureResult.stderrLogSha256
            $buildResult = Invoke-CapturedCommand $cmake @(
                "--build", $BuildDirectory, "--config", "Release", "--verbose",
                "--target", "ClassMngrNext", "ClassMngrNextResourcePackage", "ClassMngrPhase7NextBlankFixtureTests",
                "--", "/m:4", "/p:TrackFileAccess=false"
            ) $BuildLogPath "Current-source packaged Next Release build"
        } finally { [Environment]::SetEnvironmentVariable("PATH", $oldPath, "Process") }
    } else {
        $prebuiltReceiptPath = [IO.Path]::GetFullPath($PrebuiltBuildReceiptPath)
        if (-not (Test-Path -LiteralPath $prebuiltReceiptPath -PathType Leaf)) { throw "Prebuilt Release build receipt is missing: $prebuiltReceiptPath" }
        $prebuiltReceipt = Get-Content -LiteralPath $prebuiltReceiptPath -Raw | ConvertFrom-Json -ErrorAction Stop
        if ($prebuiltReceipt.schema -cne "classmngr-phase9-calendar-import-release-build-receipt-v1" -or $prebuiltReceipt.taskId -cne $TaskId -or $prebuiltReceipt.status -cne "succeeded") { throw "Prebuilt Release build receipt is not a successful P9-10 receipt." }
        $hasPostBuildSourceSha256 = $null -ne $prebuiltReceipt.PSObject.Properties["postBuildSourceSha256"]
        $postBuildSourceSha256 = if ($hasPostBuildSourceSha256) { [string]$prebuiltReceipt.PSObject.Properties["postBuildSourceSha256"].Value } else { "" }
        $receiptCurrentSourceSha256 = if (-not [string]::IsNullOrWhiteSpace($postBuildSourceSha256)) { $postBuildSourceSha256 } else { [string]$prebuiltReceipt.sourceSha256 }
        if ($prebuiltReceipt.repositoryRoot -ine $RepositoryRoot -or $receiptCurrentSourceSha256 -cne $source.sha256 -or $prebuiltReceipt.buildDirectory -ine $BuildDirectory -or $prebuiltReceipt.configuration -cne "Release" -or $prebuiltReceipt.platform -cne "x64" -or $prebuiltReceipt.packageOption -cne "CLASSMNGR_NEXT_RELEASE_PACKAGE=ON" -or $prebuiltReceipt.startupUpdateCheck -cne "OFF" -or @($prebuiltReceipt.targets) -notcontains "ClassMngrNextResourcePackage") { throw "Prebuilt Release build receipt does not match the current source, build tree, package configuration, or resource-package target." }
        if (-not [string]::IsNullOrWhiteSpace($postBuildSourceSha256)) {
            if ([string]$prebuiltReceipt.buildSourceSha256 -cne [string]$prebuiltReceipt.sourceSha256 -or $prebuiltReceipt.reusedWithoutRebuild -ne $true) { throw "Derived Release receipt does not preserve the original build-source fingerprint or reuse status." }
            $allowedPostBuildPaths = @("scripts/phase9/run_packaged_next_calendar_import.ps1", "scripts/phase9/validate_packaged_next_calendar_import.py", "scripts/phase9/test_packaged_calendar_import_harness.ps1")
            $postBuildPaths = @($prebuiltReceipt.postBuildSourceDeltaPaths | ForEach-Object { [string]$_ })
            if ($postBuildPaths.Count -le 0 -or @($postBuildPaths | Where-Object { $allowedPostBuildPaths -cnotcontains $_ }).Count -gt 0) { throw "Derived Release receipt includes a post-build change outside the acceptance runner/validator/harness." }
            $deltaEvidencePath = [IO.Path]::GetFullPath([string]$prebuiltReceipt.postBuildSourceDeltaEvidencePath)
            if (-not (Test-Path -LiteralPath $deltaEvidencePath -PathType Leaf) -or (Get-FileSha256 $deltaEvidencePath) -cne [string]$prebuiltReceipt.postBuildSourceDeltaEvidenceSha256) { throw "Post-build source-delta evidence is missing or changed." }
            $deltaEvidence = Get-Content -LiteralPath $deltaEvidencePath -Raw | ConvertFrom-Json -ErrorAction Stop
            if ([string]$deltaEvidence.rawBuildSourceSha256 -cne [string]$prebuiltReceipt.sourceSha256 -or [string]$deltaEvidence.currentSourceSha256 -cne $source.sha256) { throw "Post-build source-delta evidence does not bind the build and current acceptance source fingerprints." }
            $evidencePaths = @($deltaEvidence.changedPaths | ForEach-Object { [string]$_.path })
            if ((@($evidencePaths | Sort-Object -CaseSensitive) -join "`n") -cne (@($postBuildPaths | Sort-Object -CaseSensitive) -join "`n")) { throw "Derived Release receipt delta paths differ from source-delta evidence." }
            $currentInventoryPath = [IO.Path]::GetFullPath([string]$deltaEvidence.currentInventoryPath)
            if (-not (Test-Path -LiteralPath $currentInventoryPath -PathType Leaf) -or (Get-FileSha256 $currentInventoryPath) -cne [string]$deltaEvidence.currentInventorySha256) { throw "Post-build current-source inventory evidence is missing or changed." }
            $currentInventoryDocument = Get-Content -LiteralPath $currentInventoryPath -Raw | ConvertFrom-Json -ErrorAction Stop
            if (@($currentInventoryDocument.files | ConvertTo-Json -Depth 12 -Compress) -cne @($source.files | ConvertTo-Json -Depth 12 -Compress)) { throw "Post-build current-source inventory rows differ from the runner inventory." }
            $rawBuildReceiptPath = [IO.Path]::GetFullPath([string]$prebuiltReceipt.rawBuildReceiptPath)
            if (-not (Test-Path -LiteralPath $rawBuildReceiptPath -PathType Leaf) -or (Get-FileSha256 $rawBuildReceiptPath) -cne [string]$prebuiltReceipt.rawBuildReceiptSha256) { throw "Raw Release build receipt referenced by the derived receipt is missing or changed." }
            $rawBuildReceipt = Get-Content -LiteralPath $rawBuildReceiptPath -Raw | ConvertFrom-Json -ErrorAction Stop
            if ([string]$rawBuildReceipt.sourceSha256 -cne [string]$prebuiltReceipt.sourceSha256 -or [string]$rawBuildReceipt.releaseExecutableSha256 -cne [string]$prebuiltReceipt.releaseExecutableSha256) { throw "Derived Release receipt changed the original build or executable identity." }
            $script:Manifest.build.reusedWithoutRebuild = $true
            $script:Manifest.build.buildSourceSha256 = [string]$prebuiltReceipt.sourceSha256
            $script:Manifest.build.currentAcceptanceSourceSha256 = [string]$source.sha256
            $script:Manifest.build.postBuildSourceDeltaPaths = $postBuildPaths
            $script:Manifest.build.postBuildSourceDeltaEvidencePath = $deltaEvidencePath
            $script:Manifest.build.postBuildSourceDeltaEvidenceSha256 = Get-FileSha256 $deltaEvidencePath
        } else {
            if ([string]$prebuiltReceipt.sourceSha256 -cne [string]$source.sha256) { throw "Prebuilt Release build receipt does not match the current source inventory." }
            $script:Manifest.build.reusedWithoutRebuild = $false
            $script:Manifest.build.buildSourceSha256 = [string]$prebuiltReceipt.sourceSha256
            $script:Manifest.build.currentAcceptanceSourceSha256 = [string]$source.sha256
        }
        if ($prebuiltReceipt.cachePath -ine [IO.Path]::GetFullPath($cachePath) -or $prebuiltReceipt.cacheSha256After -cne (Get-FileSha256 $cachePath)) { throw "Prebuilt Release build receipt cache identity differs from the current Release tree." }
        $releaseExe = Join-Path $BuildDirectory "next-package-build\Release\ClassMngr.exe"
        if ($prebuiltReceipt.releaseExecutablePath -ine [IO.Path]::GetFullPath($releaseExe) -or $prebuiltReceipt.releaseExecutableSha256 -cne (Get-FileSha256 $releaseExe) -or (Get-PeArchitecture $releaseExe) -cne "x64") { throw "Prebuilt Release executable does not match the receipt identity or x64 architecture." }
        $generatedResources = $prebuiltReceipt.generatedResources
        $expectedGeneratedRoot = Join-Path $BuildDirectory "generated\ClassMngrNext"
        $expectedBakedAssetsRoot = [IO.Path]::GetFullPath((Join-Path $expectedGeneratedRoot "Resources\assets"))
        $expectedTranslationRoot = [IO.Path]::GetFullPath((Join-Path $expectedGeneratedRoot "assets\translations"))
        $expectedCatalogPath = [IO.Path]::GetFullPath((Join-Path $expectedGeneratedRoot "resource-catalog.json"))
        $expectedFontLicensePath = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot "licenses\fonts\inter\LICENSE.txt"))
        if ([IO.Path]::GetFullPath([string]$generatedResources.bakedAssets.root) -ine $expectedBakedAssetsRoot -or [IO.Path]::GetFullPath([string]$generatedResources.translations.root) -ine $expectedTranslationRoot -or [IO.Path]::GetFullPath([string]$generatedResources.catalogPath) -ine $expectedCatalogPath -or [IO.Path]::GetFullPath([string]$generatedResources.fontLicensePath) -ine $expectedFontLicensePath) { throw "Prebuilt Release generated-resource receipt references unexpected paths." }
        if ([int]$generatedResources.bakedAssets.fileCount -le 0 -or [int]$generatedResources.translations.fileCount -le 0 -or [string]$generatedResources.bakedAssets.sha256 -notmatch '^[0-9a-f]{64}$' -or [string]$generatedResources.translations.sha256 -notmatch '^[0-9a-f]{64}$' -or [string]$generatedResources.catalogSha256 -notmatch '^[0-9a-f]{64}$' -or [string]$generatedResources.fontLicenseSha256 -notmatch '^[0-9a-f]{64}$') { throw "Prebuilt Release generated-resource receipt contains empty or malformed inventory identity." }
        $expectedStagingRoot = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $prebuiltReceiptPath) "verified-resource-inputs"))
        if ([IO.Path]::GetFullPath([string]$generatedResources.verifiedStagingRoot) -ine $expectedStagingRoot -or [string]$generatedResources.currentUserSid -cne [string][Security.Principal.WindowsIdentity]::GetCurrent().User.Value) { throw "Prebuilt Release resource staging is not bound to this receipt and current user." }
        $stagedReceiptInventory = $generatedResources.stagedResourceInputs
        if ([IO.Path]::GetFullPath([string]$stagedReceiptInventory.root) -ine $expectedStagingRoot -or [int]$stagedReceiptInventory.fileCount -le 0) { throw "Prebuilt Release staged resource inventory is missing or references an unexpected root." }
        $currentStagedInventory = Get-CanonicalInventory $expectedStagingRoot
        if ($currentStagedInventory.sha256 -cne [string]$stagedReceiptInventory.sha256 -or [int]$currentStagedInventory.fileCount -ne [int]$stagedReceiptInventory.fileCount -or (($currentStagedInventory.files | ConvertTo-Json -Depth 10 -Compress) -cne ($stagedReceiptInventory.files | ConvertTo-Json -Depth 10 -Compress))) { throw "Prebuilt Release verified resource staging changed after the elevated build." }
        $expectedStagedRows = [Collections.Generic.List[object]]::new()
        foreach ($entry in $generatedResources.bakedAssets.files) { $expectedStagedRows.Add([ordered]@{ path = "resources/assets/$($entry.path)"; sizeBytes = [Int64]$entry.sizeBytes; sha256 = [string]$entry.sha256 }) }
        foreach ($entry in $generatedResources.translations.files) { $expectedStagedRows.Add([ordered]@{ path = "resources/assets/translations/$($entry.path)"; sizeBytes = [Int64]$entry.sizeBytes; sha256 = [string]$entry.sha256 }) }
        $expectedStagedRows.Add([ordered]@{ path = "resources/resource-catalog.json"; sizeBytes = [Int64]$generatedResources.catalogSizeBytes; sha256 = [string]$generatedResources.catalogSha256 })
        $expectedStagedRows.Add([ordered]@{ path = "licenses/fonts/inter/LICENSE.txt"; sizeBytes = [Int64]$generatedResources.fontLicenseSizeBytes; sha256 = [string]$generatedResources.fontLicenseSha256 })
        $expectedStagedRows.Sort([System.Comparison[object]]{ param($a, $b) [StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(), ([string]$b.path).ToLowerInvariant()) })
        $expectedStagedFiles = @($expectedStagedRows.ToArray())
        if ([int]$stagedReceiptInventory.fileCount -ne $expectedStagedFiles.Count -or (($stagedReceiptInventory.files | ConvertTo-Json -Depth 10 -Compress) -cne ($expectedStagedFiles | ConvertTo-Json -Depth 10 -Compress))) { throw "Prebuilt Release staged resource inventory does not match the generated build output inventories." }
        $script:VerifiedResourceInputsRoot = $expectedStagingRoot
        $script:Manifest.build.verifiedResourceInputsRoot = $expectedStagingRoot
        $script:Manifest.build.verifiedResourceInputsInventorySha256 = [string]$stagedReceiptInventory.sha256
        $script:Manifest.build.verifiedResourceInputsFileCount = [int]$stagedReceiptInventory.fileCount
        $script:Manifest.build.verifiedResourceInputsUserSid = [string]$generatedResources.currentUserSid
        foreach ($commandEvidence in @($prebuiltReceipt.configure, $prebuiltReceipt.build)) {
            foreach ($fieldPair in @(@("logPath", "logSha256"), @("stdoutLogPath", "stdoutLogSha256"), @("stderrLogPath", "stderrLogSha256"))) {
                $evidencePath = [string]$commandEvidence.($fieldPair[0])
                if (-not (Test-Path -LiteralPath $evidencePath -PathType Leaf) -or (Get-FileSha256 $evidencePath) -cne [string]$commandEvidence.($fieldPair[1])) { throw "Prebuilt Release command evidence is missing or changed: $evidencePath" }
            }
        }
        $prebuiltEvidenceRoot = Join-Path (Join-Path $EvidenceDirectory "logs") "prebuilt-release"
        [void][IO.Directory]::CreateDirectory($prebuiltEvidenceRoot)
        foreach ($commandName in @("configure", "build")) {
            $commandEvidence = $prebuiltReceipt.$commandName
            foreach ($fieldPair in @(@("logPath", "logSha256"), @("stdoutLogPath", "stdoutLogSha256"), @("stderrLogPath", "stderrLogSha256"))) {
                $pathField = [string]$fieldPair[0]
                $hashField = [string]$fieldPair[1]
                $originalPath = [IO.Path]::GetFullPath([string]$commandEvidence.$pathField)
                $localPath = Join-Path $prebuiltEvidenceRoot ("{0}.{1}.txt" -f $commandName, $pathField)
                Copy-Item -LiteralPath $originalPath -Destination $localPath
                if ((Get-FileSha256 $localPath) -cne [string]$commandEvidence.$hashField) { throw "Copied prebuilt $commandName $pathField changed: $originalPath" }
                $commandEvidence | Add-Member -NotePropertyName ("original" + $pathField.Substring(0, 1).ToUpperInvariant() + $pathField.Substring(1)) -NotePropertyValue $originalPath -Force
                $commandEvidence.$pathField = [IO.Path]::GetFullPath($localPath)
            }
        }
        $rawBuildReceiptOriginalPath = [IO.Path]::GetFullPath([string]$prebuiltReceipt.rawBuildReceiptPath)
        $rawBuildReceiptLocalPath = Join-Path $prebuiltEvidenceRoot "raw-release-build-receipt.json"
        Copy-Item -LiteralPath $rawBuildReceiptOriginalPath -Destination $rawBuildReceiptLocalPath
        if ((Get-FileSha256 $rawBuildReceiptLocalPath) -cne [string]$prebuiltReceipt.rawBuildReceiptSha256) { throw "Copied raw Release build receipt changed." }
        $prebuiltReceipt | Add-Member -NotePropertyName "rawBuildReceiptOriginalPath" -NotePropertyValue $rawBuildReceiptOriginalPath -Force
        $prebuiltReceipt.rawBuildReceiptPath = [IO.Path]::GetFullPath($rawBuildReceiptLocalPath)
        $deltaOriginalPath = [IO.Path]::GetFullPath([string]$prebuiltReceipt.postBuildSourceDeltaEvidencePath)
        $deltaLocalPath = Join-Path $prebuiltEvidenceRoot "post-build-source-delta.json"
        Copy-Item -LiteralPath $deltaOriginalPath -Destination $deltaLocalPath
        if ((Get-FileSha256 $deltaLocalPath) -cne [string]$prebuiltReceipt.postBuildSourceDeltaEvidenceSha256) { throw "Copied source-delta evidence changed." }
        $prebuiltReceipt | Add-Member -NotePropertyName "postBuildSourceDeltaOriginalEvidencePath" -NotePropertyValue $deltaOriginalPath -Force
        $prebuiltReceipt.postBuildSourceDeltaEvidencePath = [IO.Path]::GetFullPath($deltaLocalPath)
        $currentInventoryOriginalPath = [IO.Path]::GetFullPath([string]$deltaEvidence.currentInventoryPath)
        $currentInventoryLocalPath = Join-Path $prebuiltEvidenceRoot "post-build-current-source-files.json"
        Copy-Item -LiteralPath $currentInventoryOriginalPath -Destination $currentInventoryLocalPath
        if ((Get-FileSha256 $currentInventoryLocalPath) -cne [string]$deltaEvidence.currentInventorySha256) { throw "Copied current-source inventory changed." }
        $prebuiltReceipt | Add-Member -NotePropertyName "postBuildCurrentInventoryOriginalPath" -NotePropertyValue $currentInventoryOriginalPath -Force
        $prebuiltReceipt | Add-Member -NotePropertyName "postBuildCurrentInventoryPath" -NotePropertyValue ([IO.Path]::GetFullPath($currentInventoryLocalPath)) -Force
        $rawSourceInventoryOriginalPath = [IO.Path]::GetFullPath([string]$rawBuildReceipt.sourceInventoryPath)
        $rawSourceInventoryLocalPath = Join-Path $prebuiltEvidenceRoot "raw-build-source-files.json"
        Copy-Item -LiteralPath $rawSourceInventoryOriginalPath -Destination $rawSourceInventoryLocalPath
        if ((Get-FileSha256 $rawSourceInventoryLocalPath) -cne [string]$rawBuildReceipt.sourceInventoryFileSha256) { throw "Copied raw build-source inventory changed." }
        $prebuiltReceipt | Add-Member -NotePropertyName "rawBuildSourceInventoryOriginalPath" -NotePropertyValue $rawSourceInventoryOriginalPath -Force
        $prebuiltReceipt | Add-Member -NotePropertyName "rawBuildSourceInventoryPath" -NotePropertyValue ([IO.Path]::GetFullPath($rawSourceInventoryLocalPath)) -Force
        $localPrebuiltReceiptPath = Join-Path $prebuiltEvidenceRoot "derived-release-build-receipt.json"
        Write-JsonFile $localPrebuiltReceiptPath $prebuiltReceipt
        $script:Manifest.build.rawPrebuiltBuildReceiptOriginalPath = $rawBuildReceiptOriginalPath
        $script:Manifest.build.rawPrebuiltBuildReceiptOriginalSha256 = [string]$prebuiltReceipt.rawBuildReceiptSha256
        $script:Manifest.build.rawPrebuiltBuildReceiptPath = [IO.Path]::GetFullPath($rawBuildReceiptLocalPath)
        $script:Manifest.build.rawPrebuiltBuildReceiptSha256 = Get-FileSha256 $rawBuildReceiptLocalPath
        $script:Manifest.build.postBuildSourceDeltaEvidencePath = [IO.Path]::GetFullPath($deltaLocalPath)
        $script:Manifest.build.postBuildSourceDeltaEvidenceSha256 = Get-FileSha256 $deltaLocalPath
        $script:Manifest.build.postBuildCurrentInventoryPath = [IO.Path]::GetFullPath($currentInventoryLocalPath)
        $script:Manifest.build.postBuildCurrentInventorySha256 = Get-FileSha256 $currentInventoryLocalPath
        $script:Manifest.build.rawBuildSourceInventoryPath = [IO.Path]::GetFullPath($rawSourceInventoryLocalPath)
        $script:Manifest.build.rawBuildSourceInventorySha256 = Get-FileSha256 $rawSourceInventoryLocalPath
        $configureResult = $prebuiltReceipt.configure
        $buildResult = $prebuiltReceipt.build
        $script:BuildSourceSha256 = [string]$prebuiltReceipt.sourceSha256
        $script:Manifest.build.prebuiltBuildReceiptOriginalPath = [IO.Path]::GetFullPath($PrebuiltBuildReceiptPath)
        $script:Manifest.build.prebuiltBuildReceiptOriginalSha256 = Get-FileSha256 $prebuiltReceiptPath
        $script:Manifest.build.prebuiltBuildReceiptPath = [IO.Path]::GetFullPath($localPrebuiltReceiptPath)
        $script:Manifest.build.prebuiltBuildReceiptSha256 = Get-FileSha256 $localPrebuiltReceiptPath
        $script:Manifest.build.prebuiltBuildRunId = [string]$prebuiltReceipt.runId
    }
    $script:Manifest.build.exitCode = $buildResult.exitCode
    $script:Manifest.build.succeeded = ($buildResult.exitCode -eq 0)
    $script:Manifest.build.logPath = $buildResult.logPath
    $script:Manifest.build.logSha256 = $buildResult.logSha256
    $script:Manifest.build.stdoutLogPath = $buildResult.stdoutLogPath
    $script:Manifest.build.stdoutLogSha256 = $buildResult.stdoutLogSha256
    $script:Manifest.build.stderrLogPath = $buildResult.stderrLogPath
    $script:Manifest.build.stderrLogSha256 = $buildResult.stderrLogSha256
    $script:Manifest.build.cacheSha256After = Get-FileSha256 $cachePath
    if ([string]::IsNullOrWhiteSpace($script:BuildSourceSha256)) { $script:BuildSourceSha256 = [string]$source.sha256; $script:Manifest.build.buildSourceSha256 = $script:BuildSourceSha256; $script:Manifest.build.currentAcceptanceSourceSha256 = [string]$source.sha256; $script:Manifest.build.reusedWithoutRebuild = $false }
    if (-not $script:Manifest.build.succeeded) { throw "Current-source packaged Next Release build failed." }
    if ((Get-CacheValue @(Get-Content -LiteralPath $cachePath) "CLASSMNGR_UPDATE_CHECK_ON_STARTUP") -cne "OFF") { throw "Release package still enables startup update network access." }

    $releaseExe = Join-Path $BuildDirectory "next-package-build\Release\ClassMngr.exe"
    if (-not (Test-Path -LiteralPath $releaseExe -PathType Leaf) -or (Get-PeArchitecture $releaseExe) -cne "x64") { throw "Current-source x64 Next Release executable is missing or has the wrong architecture." }
    $packageDirectory = Join-Path $EvidenceDirectory "package"
    $resourceLog = Join-Path (Join-Path $EvidenceDirectory "logs") "resource-install.log"
    if ([string]::IsNullOrWhiteSpace($script:VerifiedResourceInputsRoot)) {
        $null = Invoke-CapturedCommand $cmake @("--install", $BuildDirectory, "--config", "Release", "--prefix", $packageDirectory, "--component", "ClassMngrNextResources") $resourceLog "Current-source package resource install"
    } else {
        [void][IO.Directory]::CreateDirectory($packageDirectory)
        foreach ($entry in @(Get-ChildItem -LiteralPath $script:VerifiedResourceInputsRoot -Force)) {
            Copy-Item -LiteralPath $entry.FullName -Destination $packageDirectory -Recurse -Force
        }
        $copiedResourceInventory = Get-CanonicalInventory $packageDirectory
        $sourceResourceInventory = Get-CanonicalInventory $script:VerifiedResourceInputsRoot
        if ($copiedResourceInventory.sha256 -cne $sourceResourceInventory.sha256 -or [int]$copiedResourceInventory.fileCount -ne [int]$sourceResourceInventory.fileCount -or (($copiedResourceInventory.files | ConvertTo-Json -Depth 10 -Compress) -cne ($sourceResourceInventory.files | ConvertTo-Json -Depth 10 -Compress))) { throw "Non-elevated package resource copy differs from the verified Release build receipt." }
        [IO.File]::WriteAllLines($resourceLog, @(
            "mode: non-elevated-copy-from-verified-release-build-resource-inputs",
            "sourceRoot: $script:VerifiedResourceInputsRoot",
            "destinationRoot: $packageDirectory",
            "sourceInventorySha256: $($sourceResourceInventory.sha256)",
            "destinationInventorySha256: $($copiedResourceInventory.sha256)",
            "fileCount: $($copiedResourceInventory.fileCount)"
        ), [Text.UTF8Encoding]::new($false))
        $script:Manifest.build.resourceStageLogPath = [IO.Path]::GetFullPath($resourceLog)
        $script:Manifest.build.resourceStageLogSha256 = Get-FileSha256 $resourceLog
    }
    $appPath = Join-Path $packageDirectory "ClassMngrNext.exe"
    Copy-Item -LiteralPath $releaseExe -Destination $appPath
    $oldPath = [Environment]::GetEnvironmentVariable("PATH", "Process")
    try {
        [Environment]::SetEnvironmentVariable("PATH", "$script:QtBin;$oldPath", "Process")
        $null = Invoke-CapturedCommand $windeployqt @(
            "--qmldir", (Join-Path $RepositoryRoot "src\features\calendar\ui\qml"),
            "--skip-plugin-types", "qmltooling", "--include-plugins", "qoffscreen,qsqlite",
            "--exclude-plugins", "qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql",
            "--translations", "en,ko", "--compiler-runtime", "--dir", $packageDirectory, $appPath
        ) $DeployLogPath "Qt runtime deployment"
    } finally { [Environment]::SetEnvironmentVariable("PATH", $oldPath, "Process") }
    if (-not (Test-Path -LiteralPath (Join-Path $packageDirectory "resources\resource-catalog.json") -PathType Leaf)) { throw "Staged package is missing the Next resource catalog." }
    $packageInventory = Get-CanonicalInventory $packageDirectory
    Write-JsonFile $PackageInventoryPath ([ordered]@{ files = @($packageInventory.files) })
    $script:Manifest.application = [ordered]@{
        canonicalPath = [IO.Path]::GetFullPath($appPath); sha256 = Get-FileSha256 $appPath
        architecture = "x64"; releaseExecutablePath = [IO.Path]::GetFullPath($releaseExe)
        releaseExecutableSha256 = Get-FileSha256 $releaseExe; candidateSourceSha256 = $script:BuildSourceSha256
        acceptanceHarnessSourceSha256 = $source.sha256
    }
    $script:Manifest.package = [ordered]@{
        directory = [IO.Path]::GetFullPath($packageDirectory); sha256 = $packageInventory.sha256
        fileCount = $packageInventory.fileCount; applicationRelativePath = "ClassMngrNext.exe"
        applicationSha256 = Get-FileSha256 $appPath; candidateSourceSha256 = $script:BuildSourceSha256
        acceptanceHarnessSourceSha256 = $source.sha256
        sourceIdentityMode = if ($script:Manifest.build.reusedWithoutRebuild) { "current-source-product-build-with-script-only-acceptance-harness-delta" } else { "current-source-build-and-package" }
        resourceCatalogRelativePath = "resources/resource-catalog.json"
        runtimeFiles = @(Get-PackageRuntimeFiles $packageDirectory $script:QtPrefix)
    }

    $successFixture = Copy-VerifiedFixture (Split-Path -Leaf $SuccessFixtureRelative) $SuccessFixtureRelative "25a5bee84b02f97d6ec72f5c21ba0909a5e6f68edc1e10607ade75b2435c73fb" 16284
    $errorFixture = Copy-VerifiedFixture (Split-Path -Leaf $ErrorFixtureRelative) $ErrorFixtureRelative "35cda6bf051eaf8f5d36a39b6a632a3af34b8b697316db6192aa70ca2b1eabaa" 69
    $script:Manifest.fixtures = @($successFixture, $errorFixture)
    $blankWorkspacePath = Join-Path (Join-Path $EvidenceDirectory "workspace-snapshots") "blank-workspace.tps"
    $blankGenerator = Invoke-BlankWorkspaceGenerator $blankWorkspacePath
    $seededTemplatePath = Join-Path (Join-Path $EvidenceDirectory "workspace-snapshots") "seeded-template.tps"
    $seedSqlPath = Join-Path $RepositoryRoot $SeedSqlRelative
    $seedLogPath = Join-Path (Join-Path $EvidenceDirectory "logs") "prepare-seeded-workspace.log"
    $seeded = Invoke-PythonJson (Join-Path $PSScriptRoot "prepare_calendar_import_workspace.py") @(
        "--blank-workspace", $blankWorkspacePath, "--output-workspace", $seededTemplatePath, "--seed-sql", $seedSqlPath
    ) $seedLogPath "Seeded Calendar import workspace preparation"
    if ([int]$seeded.calendarEventCount -ne 180 -or [int]$seeded.teacherCount -ne 24 -or [int]$seeded.classCount -ne 96) { throw "Seeded Calendar import workspace counts differ from the accepted template." }
    $script:Manifest.workspaceSeed = [ordered]@{
        path = [IO.Path]::GetFullPath($seededTemplatePath); sha256 = [string]$seeded.workspaceSha256
        sizeBytes = [Int64]$seeded.workspaceSizeBytes; integrityCheck = [string]$seeded.integrityCheck
        calendarEventCount = [int]$seeded.calendarEventCount; teacherCount = [int]$seeded.teacherCount; classCount = [int]$seeded.classCount
        seedSqlPath = [IO.Path]::GetFullPath($seedSqlPath); seedSqlSha256 = [string]$seeded.seedSqlSha256
        blankWorkspace = $blankGenerator; prepareLogPath = [IO.Path]::GetFullPath($seedLogPath)
    }
    Add-Type -AssemblyName UIAutomationClient
    Add-Type -AssemblyName UIAutomationTypes
    $null = Invoke-VerifiedRoute -RouteId "route-a-large-calendar-success" -FixtureName $successFixture.name -FixtureRole "success" -RouteDirectory $EvidenceDirectory -Fixture $successFixture -WorkspaceTemplateSha256 $seeded.workspaceSha256
    if ($RouteAOnly) {
        $script:Manifest.status = "route-a-diagnostic-passed"
    } else {
        $null = Invoke-VerifiedRoute -RouteId "route-b-malformed-calendar-error" -FixtureName $errorFixture.name -FixtureRole "error" -RouteDirectory $EvidenceDirectory -Fixture $errorFixture -WorkspaceTemplateSha256 $seeded.workspaceSha256
        $script:Manifest.status = "evidence-collected"
    }
    $script:Manifest.completedAtUtc = Get-UtcTimestamp
} catch {
    Set-RunnerFailure $_
} finally {
    foreach ($context in $script:Processes) {
        try {
            $context.process.Refresh()
            if (-not $context.process.HasExited) {
                $null = Stop-MeasuredProcess $context
            }
            $context.process.Refresh()
            foreach ($route in $script:Manifest.routes) {
                if ($route.routeId -ceq $context.routeId -and $null -ne $script:LastShutdownObservability -and $script:LastShutdownObservability.routeId -ceq $context.routeId) {
                    $route.diagnostics.shutdownObservability = $script:LastShutdownObservability
                    $route.diagnostics.shutdownMiniDump = $script:LastShutdownObservability.shutdownMiniDump
                }
            }
            if ($context.process.HasExited) {
                foreach ($route in $script:Manifest.routes) {
                    if ($route.routeId -ceq $context.routeId -and $null -eq $route.processExitCode) {
                        $route.processExitCode = [int]$context.process.ExitCode
                    }
                }
            }
        } catch { }
    }
    foreach ($route in $script:Manifest.routes) {
        if ($null -ne $route.processExitCode -and [int]$route.processExitCode -ne 0) {
            $dumpPath = Join-Path (Join-Path $env:LOCALAPPDATA "CrashDumps") ("ClassMngrNext.exe." + [string]$route.processIdentity.pid + ".dmp")
            if (Test-Path -LiteralPath $dumpPath -PathType Leaf) {
                $dumpFile = Get-Item -LiteralPath $dumpPath
                $route.diagnostics.nativeCrashDump = [ordered]@{
                    kind = "windows-wer-minidump-reference"
                    path = [IO.Path]::GetFullPath($dumpPath)
                    sha256 = Get-FileSha256 $dumpPath
                    sizeBytes = [Int64]$dumpFile.Length
                    lastWriteTimeUtc = $dumpFile.LastWriteTimeUtc.ToString("o")
                    copiedIntoEvidence = $false
                }
            }
        }
        if ($null -eq $route.processExitCode -or [string]::IsNullOrWhiteSpace([string]$route.workspace.path) -or (Test-Path -LiteralPath ([string]$route.workspace.afterSnapshot) -PathType Leaf)) {
            continue
        }
        $diagnosticPath = Join-Path (Join-Path $EvidenceDirectory "workspace-snapshots") ($route.routeId + "-after-termination-diagnostic.json")
        $diagnosticLogPath = Join-Path (Join-Path $EvidenceDirectory "logs") ($route.routeId + "-after-termination-snapshot.log")
        try {
            $diagnostic = Invoke-PythonJson (Join-Path $PSScriptRoot "snapshot_calendar_import_workspace.py") @(
                "--workspace", [string]$route.workspace.path, "--output", $diagnosticPath
            ) $diagnosticLogPath "$($route.routeId) after-termination workspace diagnostic snapshot"
            $route.diagnostics.afterTerminationWorkspaceSnapshot = [ordered]@{
                    kind = "diagnostic-only-after-termination"
                    path = [IO.Path]::GetFullPath($diagnosticPath)
                    sha256 = Get-FileSha256 $diagnosticPath
                    integrityCheck = [string]$diagnostic.integrityCheck
                    tableContentSha256 = [string]$diagnostic.tableContentSha256
                    calendarEventCount = [int]$diagnostic.calendarEventCount
                    tableCount = [int]$diagnostic.tableCount
                    logPath = [IO.Path]::GetFullPath($diagnosticLogPath)
                }
        } catch {
            $route.diagnostics.afterTerminationWorkspaceSnapshot = [ordered]@{
                    kind = "diagnostic-only-after-termination"
                    path = [IO.Path]::GetFullPath($diagnosticPath)
                    logPath = [IO.Path]::GetFullPath($diagnosticLogPath)
                    error = $_.Exception.Message
                }
        }
    }
    $script:Manifest.routes = @($script:Manifest.routes)
    Write-JsonFile $ManifestPath $script:Manifest
    if ($script:Manifest.status -ceq "evidence-collected") {
        try {
            $validationOutput = @(& python $validatorPath --evidence-dir $EvidenceDirectory 2>&1 | ForEach-Object { [string]$_ })
            $validationExit = $LASTEXITCODE
            [IO.File]::WriteAllLines($ValidatorOutputPath, $validationOutput, [Text.UTF8Encoding]::new($false))
            $resultLine = @($validationOutput | Where-Object { $_.TrimStart().StartsWith("{") } | Select-Object -Last 1)
            $validationResult = if ($resultLine.Count -eq 1) { $resultLine[0] | ConvertFrom-Json -ErrorAction Stop } else { $null }
            $script:Manifest.validation = [ordered]@{
                status = if ($validationExit -eq 0) { "passed" } else { "failed" }
                exitCode = $validationExit; validatorPath = [IO.Path]::GetFullPath($validatorPath)
                validatorSha256 = Get-FileSha256 $validatorPath; result = $validationResult
                logPath = [IO.Path]::GetFullPath($ValidatorOutputPath); logSha256 = Get-FileSha256 $ValidatorOutputPath
            }
        } catch {
            $script:Manifest.validation = [ordered]@{ status = "failed"; error = $_.Exception.Message }
            [IO.File]::WriteAllText($ValidatorOutputPath, "Validator execution failed: $($_.Exception.Message)`r`n", [Text.UTF8Encoding]::new($false))
        }
    } elseif ($script:Manifest.status -ceq "route-a-diagnostic-passed") {
        $script:Manifest.validation = [ordered]@{ status = "not-run-route-a-diagnostic-only" }
        [IO.File]::WriteAllText($ValidatorOutputPath, "Validator not run: this artifact contains only the bounded Route A diagnostic check and is not a two-route acceptance attempt.`r`n", [Text.UTF8Encoding]::new($false))
    } else {
        $script:Manifest.validation = [ordered]@{ status = "not-evaluated-invalid-or-incomplete-attempt" }
        [IO.File]::WriteAllText($ValidatorOutputPath, "Validation skipped because evidence collection did not complete. No route attempt was counted as valid.`r`n", [Text.UTF8Encoding]::new($false))
    }
    $script:Manifest.completedAtUtc = Get-UtcTimestamp
    Write-JsonFile $ManifestPath $script:Manifest
}

if ($script:Manifest.status -ceq "route-a-diagnostic-passed" -and $null -eq $script:Manifest.failure) {
    Write-Output "P9-10 Route A diagnostic completed: $EvidenceDirectory"
    exit 0
}
if ($script:Manifest.status -cne "evidence-collected" -or $script:Manifest.validation.status -cne "passed") {
    [Console]::Error.WriteLine("P9-10 packaged Calendar Import failed. Evidence preserved at $EvidenceDirectory; see manifest.json and validator-output.txt.")
    exit 1
}
Write-Output "P9-10 packaged Calendar Import evidence validated: $EvidenceDirectory"
