Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:P914WorkingSetStrictExclusive = [Int64]262144000
$script:P914OperationPeakAdvisoryExclusive = [Int64]536870912
$script:P914RunnerGuardExclusive = [Int64]805306368
$script:P914NewSourcePaths = @(
    'scripts/phase9/p9_14_packaged_lifecycle_common.ps1',
    'scripts/phase9/prepare_p9_14_staff_ai_workspaces.py',
    'scripts/phase9/run_packaged_next_p9_14_staff_directory_lifecycle.ps1',
    'scripts/phase9/run_packaged_next_p9_14_ai_batch_review_lifecycle.ps1',
    'scripts/phase9/validate_packaged_next_p9_14_lifecycle.py',
    'scripts/phase9/test_validate_packaged_next_p9_14_lifecycle.py'
)

function Get-P914Utc { [DateTimeOffset]::UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss.fff'Z'") }
function Get-P914FileSha256([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Get-P914StringSha256([string]$Value) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value)))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Write-P914Json([string]$Path, [object]$Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 55), [Text.UTF8Encoding]::new($false))
}
function Add-P914JsonLine([string]$Path, [object]$Value) {
    [IO.File]::AppendAllText($Path, (($Value | ConvertTo-Json -Depth 30 -Compress) + [Environment]::NewLine), [Text.UTF8Encoding]::new($false))
}
function Get-P914InventoryFingerprint([object[]]$Rows) {
    $ordered = [Collections.Generic.List[object]]::new()
    foreach ($row in $Rows) {
        $ordered.Add([ordered]@{ path = [string]$row.path; sizeBytes = [Int64]$row.sizeBytes; sha256 = ([string]$row.sha256).ToLowerInvariant() })
    }
    $ordered.Sort([System.Comparison[object]]{ param($a, $b) [StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(), ([string]$b.path).ToLowerInvariant()) })
    $canonical = @($ordered.ToArray() | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
    Get-P914StringSha256 ($canonical + "`n")
}
function Get-P914CanonicalInventory([string]$Root) {
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd([char[]]@('\', '/'))
    $prefix = $fullRoot + [IO.Path]::DirectorySeparatorChar
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($file in @(Get-ChildItem -LiteralPath $fullRoot -File -Recurse)) {
        if ($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @('.pyc', '.pyo')) { continue }
        $path = [IO.Path]::GetFullPath($file.FullName)
        if (-not $path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "Inventory path escaped root: $path" }
        $rows.Add([ordered]@{ path = $path.Substring($prefix.Length).Replace('\', '/'); sizeBytes = [Int64]$file.Length; sha256 = Get-P914FileSha256 $path })
    }
    $files = @($rows.ToArray())
    [ordered]@{ root = $fullRoot; sha256 = Get-P914InventoryFingerprint $files; fileCount = $files.Count; files = $files }
}
function Get-P914SourceInventory([string]$RepositoryRoot, [string[]]$IncludedRoots) {
    $prefix = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($relative in $IncludedRoots) {
        $target = Join-Path $RepositoryRoot $relative
        if (-not (Test-Path -LiteralPath $target)) { throw "Source inventory root is missing: $relative" }
        $files = if (Test-Path -LiteralPath $target -PathType Leaf) { @(Get-Item -LiteralPath $target) } else { @(Get-ChildItem -LiteralPath $target -File -Recurse) }
        foreach ($file in $files) {
            if ($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @('.pyc', '.pyo')) { continue }
            $path = [IO.Path]::GetFullPath($file.FullName)
            if (-not $path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "Source path escaped repository: $path" }
            $rows.Add([ordered]@{ path = $path.Substring($prefix.Length).Replace('\', '/'); sizeBytes = [Int64]$file.Length; sha256 = Get-P914FileSha256 $path })
        }
    }
    $list = [Collections.Generic.List[object]]::new()
    foreach ($row in $rows) { $list.Add($row) }
    $list.Sort([System.Comparison[object]]{ param($a, $b) [StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(), ([string]$b.path).ToLowerInvariant()) })
    $files = @($list.ToArray())
    [ordered]@{ repositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot); includedRoots = $IncludedRoots; sha256 = Get-P914InventoryFingerprint $files; fileCount = $files.Count; files = $files }
}
function Read-P914Json([string]$Path) { Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop }

function Initialize-P914PackageProof([string]$RepositoryRoot, [string]$PriorRunManifestPath, [string]$EvidenceDirectory, [string]$Mode) {
    $priorPath = if ([IO.Path]::IsPathRooted($PriorRunManifestPath)) { [IO.Path]::GetFullPath($PriorRunManifestPath) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $PriorRunManifestPath)) }
    if (-not (Test-Path -LiteralPath $priorPath -PathType Leaf)) { throw "Accepted P9-13 manifest is missing: $priorPath" }
    $priorHash = Get-P914FileSha256 $priorPath
    $prior = Read-P914Json $priorPath
    if ([string]$prior.taskId -cne 'P9_13_NEXT_SPEAKING_REPORT_PACKAGED_LIFECYCLE' -or [string]$prior.status -cne 'passed') { throw 'Package ancestry is not a passed P9-13 packaged Release run.' }
    $priorRoot = Split-Path -Parent $priorPath
    $priorSourcePath = Join-Path $priorRoot ([string]$prior.source.inventoryPath)
    $priorDerivedPath = Join-Path $priorRoot ([string]$prior.source.derivedReceiptPath)
    $priorPackageInventoryPath = Join-Path $priorRoot ([string]$prior.package.inventoryPath)
    foreach ($path in @($priorSourcePath, $priorDerivedPath, $priorPackageInventoryPath)) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "P9-13 provenance input is missing: $path" } }
    if ((Get-P914FileSha256 $priorSourcePath) -cne [string]$prior.source.inventoryFileSha256 -or (Get-P914FileSha256 $priorDerivedPath) -cne [string]$prior.source.derivedReceiptSha256 -or (Get-P914FileSha256 $priorPackageInventoryPath) -cne [string]$prior.package.inventoryFileSha256) { throw 'A P9-13 provenance inventory or source derivation receipt changed.' }
    $priorSource = Read-P914Json $priorSourcePath
    $priorDerived = Read-P914Json $priorDerivedPath
    $priorPackage = Read-P914Json $priorPackageInventoryPath
    if ([string]$priorDerived.status -cne 'verified-script-only-derivation' -or [string]$priorDerived.source.productSourceSha256 -cne [string]$prior.source.productSourceSha256) { throw 'P9-13 product-source derivation is not internally consistent.' }
    if ((Get-P914InventoryFingerprint @($priorPackage.files)) -cne [string]$prior.package.sha256 -or [int]$priorPackage.fileCount -ne [int]$prior.package.fileCount) { throw 'Accepted P9-13 package inventory fingerprint is inconsistent.' }

    $currentSource = Get-P914SourceInventory $RepositoryRoot @($prior.source.includedRoots)
    $oldMap = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    $currentMap = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($row in $priorSource.files) { $oldMap.Add([string]$row.path, $row) }
    foreach ($row in $currentSource.files) { $currentMap.Add([string]$row.path, $row) }
    foreach ($path in $oldMap.Keys) {
        if (-not $currentMap.ContainsKey($path)) { throw "A P9-13 inventoried source file is missing: $path" }
        $before = $oldMap[$path]; $now = $currentMap[$path]
        if ([Int64]$before.sizeBytes -ne [Int64]$now.sizeBytes -or [string]$before.sha256 -cne [string]$now.sha256) { throw "A source file captured by P9-13 changed: $path" }
    }
    $added = @($currentMap.Keys | Where-Object { -not $oldMap.ContainsKey($_) } | ForEach-Object { ([string]$_).Replace('\', '/') } | Sort-Object)
    $expectedAdded = @($script:P914NewSourcePaths | Sort-Object)
    if (($added -join "`n") -cne ($expectedAdded -join "`n")) { throw "Unexpected source additions relative to P9-13: $($added -join ', ')" }
    $allowed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $priorDerived.source.allowedPaths) { [void]$allowed.Add([string]$path) }
    foreach ($path in $script:P914NewSourcePaths) { [void]$allowed.Add([string]$path) }
    $productRows = @($currentSource.files | Where-Object { -not $allowed.Contains([string]$_.path) })
    $productSha = Get-P914InventoryFingerprint $productRows
    if ($productSha -cne [string]$prior.source.productSourceSha256) { throw 'Current product-source fingerprint differs from the P9-13/P9-12 accepted Release package.' }
    $nextImplementationProof = $null
    $staffImplementationProof = $null
    if ($Mode -ceq 'ai-batch-review') {
        $nextDialogSourcePath = 'src/next/ui/speaking_evaluation_ai_dialogs.cpp'
        $nextPageSourcePath = 'src/next/ui/speaking_evaluation_page.cpp'
        $legacyDialogSourcePath = 'src/features/speaking_eval/ui/speaking_eval_ai_batch_dialog.cpp'
        $nextDialogSource = @($currentSource.files | Where-Object { [string]$_.path -ceq $nextDialogSourcePath })
        $nextPageSource = @($currentSource.files | Where-Object { [string]$_.path -ceq $nextPageSourcePath })
        $legacyDialogSource = @($currentSource.files | Where-Object { [string]$_.path -ceq $legacyDialogSourcePath })
        if ($nextDialogSource.Count -ne 1 -or $nextPageSource.Count -ne 1 -or $legacyDialogSource.Count -ne 1) { throw 'Packaged AI route source attribution cannot uniquely identify Next and Legacy dialog implementations.' }
        $nextImplementationProof = [ordered]@{
            routePageKey = 'speaking_evaluation'
            nextPageSource = [ordered]@{ path = $nextPageSourcePath; sha256 = [string]$nextPageSource[0].sha256 }
            nextDialogSource = [ordered]@{ path = $nextDialogSourcePath; sha256 = [string]$nextDialogSource[0].sha256; className = 'SpeakingEvaluationAiBatchDialog'; objectName = 'speakingEvaluationAiBatchDialog' }
            legacyDialogSource = [ordered]@{ path = $legacyDialogSourcePath; sha256 = [string]$legacyDialogSource[0].sha256; className = 'SpeakingEvalAiBatchDialog'; objectName = 'speakingEvalAiBatchDialog' }
            legacyDialogInvoked = $false; forbiddenLifecyclePageKey = 'lifecycle-speaking-evaluation'
        }
    } elseif ($Mode -ceq 'staff-directory') {
        $staffSources = [ordered]@{
            page = 'src/next/ui/shared_staff_directory_page.cpp'
            pageHeader = 'src/next/ui/shared_staff_directory_page.h'
            tableModel = 'src/next/ui/shared_staff_directory_table_model.cpp'
            tableModelHeader = 'src/next/ui/shared_staff_directory_table_model.h'
            routeRegistration = 'src/next/bootstrap/application_bootstrap.cpp'
        }
        $staffFiles = [ordered]@{}
        foreach ($entry in $staffSources.GetEnumerator()) {
            $matches = @($currentSource.files | Where-Object { [string]$_.path -ceq [string]$entry.Value })
            if ($matches.Count -ne 1) { throw "Packaged Staff route source attribution cannot uniquely identify $($entry.Value)." }
            $staffFiles[[string]$entry.Key] = [ordered]@{ path = [string]$entry.Value; sha256 = [string]$matches[0].sha256 }
        }
        $staffImplementationProof = [ordered]@{
            routePageKeys = @('native_english_teachers', 'gs_team')
            routeMode = 'current-next-staff-directory-ui-packaged-windows-x64-release-local'
            sharedPageClassName = 'SharedStaffDirectoryPage'
            tableModelClassName = 'SharedStaffDirectoryTableModel'
            tableAutomationIds = @('nativeEnglishTeachersTable', 'gsTeamTable')
            routeRegistrationContract = 'application_bootstrap.cpp registers sharedStaffDirectoryPageDescriptor for both route page keys'
            sourceAttribution = $staffFiles
        }
    } else { throw "Unsupported P9-14 package proof mode: $Mode" }
    $sourceInventoryPath = Join-Path $EvidenceDirectory 'source-files.json'
    Write-P914Json $sourceInventoryPath $currentSource

    $packageRoot = [IO.Path]::GetFullPath([string]$prior.package.root)
    $actualPackage = Get-P914CanonicalInventory $packageRoot
    if ($actualPackage.sha256 -cne [string]$prior.package.sha256 -or [int]$actualPackage.fileCount -ne [int]$prior.package.fileCount) { throw 'The current P9-13 packaged Release differs from its recorded package fingerprint.' }
    foreach ($row in $priorPackage.files) {
        $actual = @($actualPackage.files | Where-Object { [string]$_.path -ceq [string]$row.path })
        if (@($actual).Count -ne 1 -or [Int64]$actual[0].sizeBytes -ne [Int64]$row.sizeBytes -or [string]$actual[0].sha256 -cne [string]$row.sha256) { throw "P9-13 package file is missing or changed: $($row.path)" }
    }
    $packageInventoryPath = Join-Path $EvidenceDirectory 'package-files.json'
    Write-P914Json $packageInventoryPath $actualPackage
    $releaseReceiptPath = [IO.Path]::GetFullPath([string]$prior.releaseBuildReceipt.path)
    if (-not (Test-Path -LiteralPath $releaseReceiptPath -PathType Leaf) -or (Get-P914FileSha256 $releaseReceiptPath) -cne [string]$prior.releaseBuildReceipt.sha256) { throw 'Accepted P9-13 Release build receipt changed or is missing.' }
    $releaseReceipt = Read-P914Json $releaseReceiptPath
    if ([string]$releaseReceipt.status -cne 'succeeded' -or [string]$releaseReceipt.configuration -cne 'Release' -or [string]$releaseReceipt.platform -cne 'x64' -or [int]$releaseReceipt.build.exitCode -ne 0) { throw 'P9-13 ancestry does not prove a successful Windows x64 Release build.' }
    $cachePath = [IO.Path]::GetFullPath([string]$prior.buildProvenance.cachePath)
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf) -or (Get-P914FileSha256 $cachePath) -cne [string]$prior.buildProvenance.cacheSha256After -or (Get-P914FileSha256 $cachePath) -cne [string]$releaseReceipt.cacheSha256After) { throw 'Configured Release CMake cache differs from the accepted package provenance.' }
    $appName = Split-Path -Leaf ([string]$prior.application.path)
    if ($appName -cne 'ClassMngrNext.exe') { throw "Accepted P9-13 package application name is unexpected: $appName" }
    $appPath = Join-Path $packageRoot $appName
    if ([IO.Path]::GetFullPath($appPath) -cne [IO.Path]::GetFullPath([string]$prior.application.path)) { throw 'P9-13 package application path does not resolve beneath its recorded package root.' }
    if (-not (Test-Path -LiteralPath $appPath -PathType Leaf) -or (Get-P914FileSha256 $appPath) -cne [string]$prior.application.sha256 -or [string]$prior.application.sha256 -cne [string]$prior.releaseBuildReceipt.releaseExecutableSha256) { throw 'Packaged executable bytes do not match the accepted x64 Release receipt.' }
    if ((Get-P914PeMachine $appPath) -ne 0x8664) { throw 'Packaged executable is not Windows x64.' }
    $resourceRoot = [IO.Path]::GetFullPath([string]$prior.package.resourceRoot)
    $resourceInventory = Get-P914CanonicalInventory $resourceRoot
    if ($resourceInventory.sha256 -cne [string]$prior.package.resourceInventorySha256 -or $resourceInventory.sha256 -cne [string]$releaseReceipt.generatedResources.stagedResourceInputs.sha256) { throw 'Staged Release resource inputs differ from the P9-13 package receipt.' }
    $resourceInventoryPath = Join-Path $EvidenceDirectory 'resource-files.json'
    Write-P914Json $resourceInventoryPath $resourceInventory
    return [ordered]@{
        reusedPackage = $true; reuseBasis = 'current product-source, package-file, x64 executable, resource, Release receipt, and standard configured cache fingerprints match passed P9-13/P9-12 ancestry'
        priorRunManifestPath = $priorPath; priorRunManifestSha256 = $priorHash; priorRunId = [string]$prior.runId
        priorTaskId = [string]$prior.taskId; packageRoot = $packageRoot; packageSha256 = [string]$actualPackage.sha256
        packageFileCount = [int]$actualPackage.fileCount; packageInventoryPath = 'package-files.json'; packageInventorySha256 = (Get-P914FileSha256 $packageInventoryPath)
        applicationPath = [IO.Path]::GetFullPath($appPath); applicationSha256 = Get-P914FileSha256 $appPath; architecture = 'x64'
        productSourceSha256 = $productSha; currentSourceSha256 = [string]$currentSource.sha256; currentSourceInventoryPath = 'source-files.json'
        currentSourceInventorySha256 = (Get-P914FileSha256 $sourceInventoryPath); acceptedReleaseReceiptPath = $releaseReceiptPath
        acceptedReleaseReceiptSha256 = Get-P914FileSha256 $releaseReceiptPath; configuration = 'Release'; platform = 'x64'
        generator = [string]$prior.buildProvenance.generator; configuredCachePath = $cachePath
        configuredCacheSha256 = Get-P914FileSha256 $cachePath; resourceRoot = $resourceRoot
        resourceInventoryPath = 'resource-files.json'; resourceInventorySha256 = [string]$resourceInventory.sha256
        resourceInventoryFileSha256 = Get-P914FileSha256 $resourceInventoryPath; freshExactTreeRebuild = $false
        nextImplementationProof = $nextImplementationProof; staffImplementationProof = $staffImplementationProof
    }
}
function Get-P914PeMachine([string]$Path) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try { $reader = [IO.BinaryReader]::new($stream); $stream.Position = 0x3c; $offset = $reader.ReadInt32(); if ($offset -lt 0 -or $offset -gt ($stream.Length - 6)) { throw 'PE header offset is outside the executable.' }; $stream.Position = $offset + 4; [Int32]$reader.ReadUInt16() }
    finally { $stream.Dispose() }
}

if (-not ('P914Native' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Concurrent;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

public static class P914Native
{
    public delegate bool EnumWindowsCallback(IntPtr window, IntPtr data);
    [StructLayout(LayoutKind.Sequential)] private struct NativeRect { public int Left; public int Top; public int Right; public int Bottom; }
    [StructLayout(LayoutKind.Sequential)] private struct NativePoint { public int X; public int Y; }
    [DllImport("user32.dll", SetLastError=true)] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool GetWindowRect(IntPtr window, out NativeRect rectangle);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool ScreenToClient(IntPtr window, ref NativePoint point);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool PrintWindow(IntPtr window, IntPtr deviceContext, uint flags);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool SetWindowPos(IntPtr window, IntPtr insertAfter, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool EnumWindows(EnumWindowsCallback callback, IntPtr data);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)] private static extern int GetWindowText(IntPtr window, StringBuilder text, int maximumCharacters);

    public static uint GetWindowProcessId(IntPtr window) { uint pid; if (window == IntPtr.Zero || GetWindowThreadProcessId(window, out pid) == 0) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowThreadProcessId failed."); return pid; }
    public static IntPtr[] GetTopLevelWindows(uint expectedPid) {
        var result = new System.Collections.Generic.List<IntPtr>();
        EnumWindowsCallback callback = delegate(IntPtr window, IntPtr data) { uint pid; if (GetWindowThreadProcessId(window, out pid) != 0 && pid == expectedPid) result.Add(window); return true; };
        if (!EnumWindows(callback, IntPtr.Zero)) throw new Win32Exception(Marshal.GetLastWin32Error(), "EnumWindows failed.");
        GC.KeepAlive(callback); return result.ToArray();
    }
    public static string GetCaption(IntPtr window, uint expectedPid) { if (GetWindowProcessId(window) != expectedPid) throw new InvalidOperationException("Caption HWND belongs to another process."); var text = new StringBuilder(512); GetWindowText(window, text, text.Capacity); return text.ToString(); }
    public static bool IsVisible(IntPtr window) { return window != IntPtr.Zero && IsWindowVisible(window); }
    public static int[] GetBounds(IntPtr window, uint expectedPid) { if (GetWindowProcessId(window) != expectedPid) throw new InvalidOperationException("Window HWND belongs to another process."); NativeRect r; if (!GetWindowRect(window, out r)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect failed."); return new int[] { r.Left, r.Top, r.Right-r.Left, r.Bottom-r.Top }; }
    public static void RaiseWithoutActivation(IntPtr window, uint expectedPid) { if (GetWindowProcessId(window) != expectedPid) throw new InvalidOperationException("Window HWND belongs to another process."); if (!SetWindowPos(window, IntPtr.Zero, 0, 0, 0, 0, 0x0001|0x0002|0x0010)) throw new Win32Exception(Marshal.GetLastWin32Error(), "SetWindowPos failed."); }
    public static void ClickScreenPoint(IntPtr window, uint expectedPid, int x, int y) {
        if (GetWindowProcessId(window) != expectedPid) throw new InvalidOperationException("Click HWND belongs to another process.");
        NativeRect r; if (!GetWindowRect(window, out r)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect failed.");
        if (x < r.Left || x >= r.Right || y < r.Top || y >= r.Bottom) throw new InvalidOperationException("UIA-derived click lies outside its verified owner window.");
        RaiseWithoutActivation(window, expectedPid); NativePoint p = new NativePoint { X=x, Y=y };
        if (!ScreenToClient(window, ref p)) throw new Win32Exception(Marshal.GetLastWin32Error(), "ScreenToClient failed.");
        IntPtr l = new IntPtr(unchecked((int)(((uint)(ushort)p.Y << 16) | (ushort)p.X)));
        if (!PostMessage(window, 0x0200, IntPtr.Zero, l) || !PostMessage(window, 0x0201, new IntPtr(1), l) || !PostMessage(window, 0x0202, IntPtr.Zero, l)) throw new Win32Exception(Marshal.GetLastWin32Error(), "UIA-derived mouse messages failed.");
    }
    public static bool RenderWindow(IntPtr window, uint expectedPid, IntPtr dc, uint flags) { if (GetWindowProcessId(window) != expectedPid) throw new InvalidOperationException("Screenshot HWND belongs to another process."); return PrintWindow(window, dc, flags); }
}

public sealed class P914Sample
{
    public long sampleIndex; public string capturedAtUtc; public string category; public string operation;
    public long processId; public long workingSetBytes; public long privateBytesDiagnosticOnly; public long peakWorkingSetBytes;
}

public sealed class P914ProcessSampler : IDisposable
{
    private readonly Process process; private readonly ConcurrentQueue<P914Sample> samples = new ConcurrentQueue<P914Sample>();
    private readonly Timer timer; private long index; private volatile string category = "normal-use"; private volatile string operation = "unlabeled";
    public P914ProcessSampler(Process measured) { process = measured; timer = new Timer(Sample, null, 0, 50); }
    public void SetContext(string nextCategory, string nextOperation) { category = nextCategory; operation = nextOperation; Sample(null); }
    private void Sample(object state) {
        try { process.Refresh(); if (process.HasExited) return; var s = new P914Sample(); s.sampleIndex = Interlocked.Increment(ref index)-1; s.capturedAtUtc = DateTimeOffset.UtcNow.ToString("o"); s.category = category; s.operation = operation; s.processId = process.Id; s.workingSetBytes = process.WorkingSet64; s.privateBytesDiagnosticOnly = process.PrivateMemorySize64; s.peakWorkingSetBytes = process.PeakWorkingSet64; samples.Enqueue(s); }
        catch { }
    }
    public P914Sample[] Snapshot() { return samples.ToArray(); }
    public void Dispose() { timer.Dispose(); Sample(null); }
}
'@
}

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing

function New-P914EvidenceDirectory([string]$RepositoryRoot, [string]$EvidenceRoot) {
    $root = if ([IO.Path]::IsPathRooted($EvidenceRoot)) { [IO.Path]::GetFullPath($EvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
    $runId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + '-' + [Guid]::NewGuid().ToString('N')
    $directory = Join-Path $root $runId
    if (Test-Path -LiteralPath $directory) { throw "Refusing to overwrite evidence directory $directory." }
    foreach ($name in @('', 'logs', 'runtime', 'runtime/settings', 'runtime/local-app-data', 'runtime/app-data', 'runtime/temp', 'runtime/diagnostics', 'workspace', 'screenshots', 'uia', 'output')) {
        $targetDirectory = $directory
        if (-not [string]::IsNullOrEmpty($name)) { $targetDirectory = Join-Path $directory $name }
        [void][IO.Directory]::CreateDirectory($targetDirectory)
    }
    return [ordered]@{ runId = $runId; evidenceDirectory = $directory }
}

function Initialize-P914Settings([string]$Mode, [string]$EvidenceDirectory) {
    $settingsRoot = Join-Path (Join-Path $EvidenceDirectory 'runtime') 'settings'
    # SettingsManager reads QSettings(<settingsRoot>/PaperCloud/ClassMngr.ini).
    $profilePath = Join-Path (Join-Path $settingsRoot 'PaperCloud') 'ClassMngr.ini'
    if ($Mode -ceq 'ai-batch-review') {
        [void][IO.Directory]::CreateDirectory((Split-Path -Parent $profilePath))
        [IO.File]::WriteAllText($profilePath, "[options]`r`nmaximumRosterRows=100`r`nrosterRowDisplayMode=0`r`n", [Text.UTF8Encoding]::new($false))
    }
    $inventory = Get-P914CanonicalInventory $settingsRoot
    $inventoryPath = Join-Path (Join-Path $EvidenceDirectory 'runtime') 'settings-before-launch-inventory.json'
    Write-P914Json $inventoryPath $inventory
    $maximum = if ($Mode -ceq 'ai-batch-review') { 100 } else { $null }
    if ($Mode -ceq 'ai-batch-review') {
        if ([IO.File]::ReadAllText($profilePath, [Text.Encoding]::UTF8) -cne "[options]`r`nmaximumRosterRows=100`r`nrosterRowDisplayMode=0`r`n") { throw 'Isolated AI settings profile does not encode the pinned 100-row maximum.' }
        if ([int]$inventory.fileCount -ne 1) { throw 'AI settings profile must contain exactly its single pinned preference file before launch.' }
    } elseif ([int]$inventory.fileCount -ne 0) { throw 'Fresh Staff Directory settings profile unexpectedly contains saved preferences.' }
    [ordered]@{ root = [IO.Path]::GetFullPath($settingsRoot); beforeLaunchInventoryPath = 'runtime/settings-before-launch-inventory.json'; beforeLaunchInventorySha256 = Get-P914FileSha256 $inventoryPath; beforeLaunchInventorySha256Content = [string]$inventory.sha256; beforeLaunchFileCount = [int]$inventory.fileCount; preferenceFile = if ($maximum) { 'runtime/settings/PaperCloud/ClassMngr.ini' } else { $null }; preferenceFileSha256 = if ($maximum) { Get-P914FileSha256 $profilePath } else { $null }; maximumRosterRows = $maximum; rosterRowDisplayMode = if ($maximum) { 0 } else { $null }; source = if ($maximum) { 'isolated QSettings INI profile at SettingsManager ORG/APP path; default setting unchanged' } else { 'fresh isolated QSettings profile; app default' } }
}

function Start-P914App([string]$Mode, [string]$EvidenceDirectory, [string]$WorkspacePath, [string]$ApplicationPath) {
    $runtime = Join-Path $EvidenceDirectory 'runtime'
    $tracePath = Join-Path (Join-Path $runtime 'diagnostics') 'application-shutdown.jsonl'
    $stdoutPath = Join-Path (Join-Path $EvidenceDirectory 'logs') 'process-stdout.log'
    $stderrPath = Join-Path (Join-Path $EvidenceDirectory 'logs') 'process-stderr.log'
    [IO.File]::WriteAllText($stdoutPath, '', [Text.UTF8Encoding]::new($false)); [IO.File]::WriteAllText($stderrPath, '', [Text.UTF8Encoding]::new($false))
    $envMap = [ordered]@{
        CLASSMNGR_SETTINGS_ROOT = (Join-Path $runtime 'settings'); CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH = [IO.Path]::GetFullPath($tracePath)
        LOCALAPPDATA = (Join-Path $runtime 'local-app-data'); APPDATA = (Join-Path $runtime 'app-data')
        TEMP = (Join-Path $runtime 'temp'); TMP = (Join-Path $runtime 'temp'); QT_FORCE_STDERR_LOGGING = '1'; PATH = "$((Split-Path -Parent $ApplicationPath));$env:PATH"
    }
    $oldEnvironment = [ordered]@{}
    foreach ($pair in $envMap.GetEnumerator()) { $oldEnvironment[[string]$pair.Key] = [Environment]::GetEnvironmentVariable([string]$pair.Key, 'Process'); [Environment]::SetEnvironmentVariable([string]$pair.Key, [string]$pair.Value, 'Process') }
    try { $process = Start-Process -FilePath $ApplicationPath -WorkingDirectory (Split-Path -Parent $ApplicationPath) -ArgumentList ('"' + [IO.Path]::GetFullPath($WorkspacePath) + '"') -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -PassThru }
    finally { foreach ($pair in $oldEnvironment.GetEnumerator()) { [Environment]::SetEnvironmentVariable([string]$pair.Key, $pair.Value, 'Process') } }
    if ($null -eq $process) { throw 'Unable to start the measured packaged Next x64 Release process.' }
    $observedPath = [IO.Path]::GetFullPath([string]$process.MainModule.FileName)
    if ($observedPath -cne [IO.Path]::GetFullPath($ApplicationPath) -or (Get-P914FileSha256 $observedPath) -cne (Get-P914FileSha256 $ApplicationPath) -or (Get-P914PeMachine $observedPath) -ne 0x8664) { throw 'Live process executable path/hash/architecture differs from the verified package.' }
    [ordered]@{
        process = $process; stdoutPath = $stdoutPath; stderrPath = $stderrPath; seen = [ordered]@{ stdout = 0; stderr = 0 }; arrival = 0
        mainWindowHandle = 0L; mainWindowRoot = $null; ownerHandles = [Collections.Generic.Dictionary[string, Int64]]::new([StringComparer]::Ordinal)
        routeEvents = [Collections.Generic.List[object]]::new(); lifecycleEvents = [Collections.Generic.List[object]]::new(); outputLines = [Collections.Generic.List[object]]::new()
        forcedTermination = $false; closeAction = $null; processExit = $null; sampler = $null
        tempRoot = (Join-Path $runtime 'temp'); processImagePath = $observedPath; processImageSha256 = Get-P914FileSha256 $observedPath; workspacePath = [IO.Path]::GetFullPath($WorkspacePath)
    }
}

function Drain-P914Output([object]$Context, [string]$EvidenceDirectory) {
    foreach ($stream in @(@{ name = 'stdout'; path = $Context.stdoutPath }, @{ name = 'stderr'; path = $Context.stderrPath })) {
        if (-not (Test-Path -LiteralPath $stream.path)) { continue }
        $lines = @(Get-Content -LiteralPath $stream.path -Encoding UTF8)
        for ($index = [int]$Context.seen[$stream.name]; $index -lt $lines.Count; $index++) {
            $line = [string]$lines[$index]; $Context.arrival++; $arrival = [Int64]$Context.arrival
            $record = [ordered]@{ arrivalIndex = $arrival; stream = $stream.name; observedAtUtc = Get-P914Utc; line = $line }
            $Context.outputLines.Add($record); Add-P914JsonLine (Join-Path $EvidenceDirectory 'application-output.jsonl') $record
            if ($line -match '^ClassMngrNext navigation event=page-route outcome=completed cause=[^\s]+ from=([^\s]+) to=([^\s]+)(?:\s|$)') {
                $Context.routeEvents.Add([ordered]@{ arrivalIndex = $arrival; fromPageId = [string]$Matches[1]; targetPageId = [string]$Matches[2]; stream = $stream.name; line = $line; processId = [Int64]$Context.process.Id; observedAtUtc = Get-P914Utc })
            }
            if ($line -match '^ClassMngrNext page lifecycle checkpoint=(\{.*\})$') {
                try { $payload = $Matches[1] | ConvertFrom-Json -ErrorAction Stop; $Context.lifecycleEvents.Add([ordered]@{ arrivalIndex = $arrival; stream = $stream.name; line = $line; record = $payload }) }
                catch { Add-P914JsonLine (Join-Path (Join-Path $EvidenceDirectory 'logs') 'lifecycle-parse-errors.jsonl') ([ordered]@{ arrivalIndex = $arrival; line = $line; error = $_.Exception.Message }) }
            }
        }
        $Context.seen[$stream.name] = $lines.Count
    }
}

function Get-P914MainRoot([object]$Context) {
    $process = $Context.process; $process.Refresh(); $handle = $process.MainWindowHandle
    if ($handle -eq [IntPtr]::Zero) { return $null }
    if ([uint32][P914Native]::GetWindowProcessId($handle) -ne [uint32]$process.Id) { throw 'Main HWND is not owned by the measured process.' }
    $root = [System.Windows.Automation.AutomationElement]::FromHandle($handle); $current = $root.Current
    if ([Int64]$current.ProcessId -ne [Int64]$process.Id -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.Window' -or [string]$current.AutomationId -cne 'QApplication.classMngrNextApplicationShell') { throw 'Main UIA root does not match the measured process and exact Next shell identity.' }
    $Context.mainWindowHandle = [Int64]$handle.ToInt64(); $Context.mainWindowRoot = $root
    $Context.ownerHandles[(Get-P914RuntimeKey $root)] = [Int64]$handle.ToInt64()
    return $root
}
function Get-P914RuntimeKey([System.Windows.Automation.AutomationElement]$Element) { [string]::Join(',', [string[]]@($Element.GetRuntimeId() | ForEach-Object { [string]$_ })) }
function Wait-P914MainRoot([object]$Context, [string]$EvidenceDirectory, [int]$TimeoutSeconds) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "Packaged app exited during startup with code $($Context.process.ExitCode)." }
        $root = Get-P914MainRoot $Context
        if ($null -ne $root) { return $root }
        Start-Sleep -Milliseconds 100
    }
    throw "No PID-bound Next main window appeared within $TimeoutSeconds seconds."
}
function Get-P914TopLevelRoot([object]$Context, [string]$AutomationId) {
    $measuredProcessId = [uint32]$Context.process.Id
    foreach ($handle in [P914Native]::GetTopLevelWindows($measuredProcessId)) {
        if ([Int64]$handle.ToInt64() -eq [Int64]$Context.mainWindowHandle -or -not [P914Native]::IsVisible($handle)) { continue }
        if ([uint32][P914Native]::GetWindowProcessId($handle) -ne $measuredProcessId) { throw 'Top-level window ownership changed during enumeration.' }
        $root = [System.Windows.Automation.AutomationElement]::FromHandle($handle); $current = $root.Current
        if ([uint32]$current.ProcessId -eq $measuredProcessId -and [string]$current.ControlType.ProgrammaticName -ceq 'ControlType.Window' -and [string]$current.AutomationId -ceq $AutomationId) {
            $Context.ownerHandles[(Get-P914RuntimeKey $root)] = [Int64]$handle.ToInt64()
            return $root
        }
    }
    return $null
}
function Get-P914VisibleTopLevelWindows([object]$Context) {
    $measuredProcessId = [uint32]$Context.process.Id
    $windows = [Collections.Generic.List[object]]::new()
    foreach ($handle in [P914Native]::GetTopLevelWindows($measuredProcessId)) {
        if (-not [P914Native]::IsVisible($handle)) { continue }
        if ([uint32][P914Native]::GetWindowProcessId($handle) -ne $measuredProcessId) { throw 'Visible top-level window owner changed during enumeration.' }
        $root = [System.Windows.Automation.AutomationElement]::FromHandle($handle)
        $current = $root.Current
        if ([uint32]$current.ProcessId -ne $measuredProcessId -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.Window') { throw 'Visible top-level UIA window does not belong to the measured process.' }
        $Context.ownerHandles[(Get-P914RuntimeKey $root)] = [Int64]$handle.ToInt64()
        $bounds = [P914Native]::GetBounds($handle, $measuredProcessId)
        $windows.Add([ordered]@{ processId = [Int64]$current.ProcessId; windowHandle = [Int64]$handle.ToInt64(); automationId = [string]$current.AutomationId; className = [string]$current.ClassName; name = [string]$current.Name; caption = [P914Native]::GetCaption($handle, $measuredProcessId); rectangle = [ordered]@{ x = $bounds[0]; y = $bounds[1]; width = $bounds[2]; height = $bounds[3] } })
    }
    return $windows.ToArray()
}
function Get-P914OwnerRoot([object]$Context, [string]$Container) {
    if ($Container -ceq 'main') { $root = Get-P914MainRoot $Context; if ($null -eq $root) { return $null }; return $root }
    return Get-P914TopLevelRoot $Context $Container
}
function Get-P914Control([object]$Context, [string]$Container, [string]$AutomationId, [string]$ControlType = '', [string]$Name = '') {
    $root = Get-P914OwnerRoot $Context $Container
    if ($null -eq $root) { return $null }
    $conditions = [Collections.Generic.List[System.Windows.Automation.Condition]]::new()
    if (-not [string]::IsNullOrEmpty($AutomationId)) { $conditions.Add([System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::AutomationIdProperty, $AutomationId)) }
    if (-not [string]::IsNullOrEmpty($ControlType)) { $type = [System.Windows.Automation.ControlType]::$($ControlType.Replace('ControlType.', '')); $conditions.Add([System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty, $type)) }
    if (-not [string]::IsNullOrEmpty($Name)) { $conditions.Add([System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty, $Name)) }
    if ($conditions.Count -eq 0) { throw 'Unbounded UIA query is not allowed.' }
    $condition = if ($conditions.Count -eq 1) { $conditions[0] } else { [System.Windows.Automation.AndCondition]::new([System.Windows.Automation.Condition[]]$conditions.ToArray()) }
    $currentRoot = $root.Current
    if ($conditions.Count -eq 3 -and [string]$currentRoot.AutomationId -ceq $AutomationId -and [string]$currentRoot.ControlType.ProgrammaticName -ceq $ControlType -and [string]$currentRoot.Name -ceq $Name) { return $root }
    $element = $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $condition)
    if ($null -eq $element) { return $null }
    $current = $element.Current
    if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { throw 'UIA query returned an element outside the measured PID.' }
    if (-not [string]::IsNullOrEmpty($AutomationId) -and [string]$current.AutomationId -cne $AutomationId) { throw 'UIA AutomationId post-query identity check failed.' }
    if (-not [string]::IsNullOrEmpty($ControlType) -and [string]$current.ControlType.ProgrammaticName -cne $ControlType) { throw 'UIA ControlType post-query identity check failed.' }
    if (-not [string]::IsNullOrEmpty($Name) -and [string]$current.Name -cne $Name) { throw 'UIA Name post-query identity check failed.' }
    $rootKey = Get-P914RuntimeKey $root
    if (-not $Context.ownerHandles.ContainsKey($rootKey)) { throw 'UIA parent root is missing its measured HWND mapping.' }
    $Context.ownerHandles[(Get-P914RuntimeKey $element)] = [Int64]$Context.ownerHandles[$rootKey]
    return $element
}
function Get-P914Table([object]$Context, [string]$Container, [string]$AutomationId) {
    $table = Get-P914Control $Context $Container $AutomationId '' ''
    if ($null -eq $table) { return $null }
    $controlType = [string]$table.Current.ControlType.ProgrammaticName
    if ($controlType -notin @('ControlType.Table', 'ControlType.DataGrid')) { throw "Exact UIA AutomationId $AutomationId resolved to unexpected $controlType." }
    $grid = Get-P914GridMetrics $table
    if (-not [bool]$grid.available) { throw "UIA table $AutomationId does not expose GridPattern: $($grid.failure)" }
    return $table
}
function Wait-P914Table([object]$Context, [string]$EvidenceDirectory, [string]$Container, [string]$AutomationId, [int]$TimeoutSeconds) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "Measured process exited while waiting for UIA table $AutomationId." }
        $table = Get-P914Table $Context $Container $AutomationId
        if ($null -ne $table) { return $table }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for PID-bound UIA table $AutomationId."
}
function Find-P914NamedControl([object]$Context, [string]$Container, [string]$ControlType, [string]$Name, [string]$ParentAutomationId = '') {
    if ([string]::IsNullOrEmpty($Name) -or [string]::IsNullOrEmpty($ControlType)) { throw 'Named-control UIA query requires an exact name and control type.' }
    $root = Get-P914OwnerRoot $Context $Container
    if ($null -eq $root) { return $null }
    $type = [System.Windows.Automation.ControlType]::$($ControlType.Replace('ControlType.', ''))
    $conditions = [Collections.Generic.List[System.Windows.Automation.Condition]]::new()
    $conditions.Add([System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty, $type))
    $conditions.Add([System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty, $Name))
    if (-not [string]::IsNullOrEmpty($ParentAutomationId)) { $conditions.Add([System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::AutomationIdProperty, $ParentAutomationId)) }
    $condition = [System.Windows.Automation.AndCondition]::new([System.Windows.Automation.Condition[]]$conditions.ToArray())
    $element = $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $condition)
    if ($null -eq $element) { return $null }
    $current = $element.Current
    if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.ControlType.ProgrammaticName -cne $ControlType -or [string]$current.Name -cne $Name) { throw 'Exact named-control UIA identity check failed.' }
    $rootKey = Get-P914RuntimeKey $root
    $Context.ownerHandles[(Get-P914RuntimeKey $element)] = [Int64]$Context.ownerHandles[$rootKey]
    return $element
}
function Get-P914ControlSnapshot([object]$Context, [System.Windows.Automation.AutomationElement]$Element) {
    $current = $Element.Current; $runtimeKey = Get-P914RuntimeKey $Element
    if (-not $Context.ownerHandles.ContainsKey($runtimeKey)) { throw 'UIA element has no bounded owner-window provenance.' }
    $handle = [Int64]$Context.ownerHandles[$runtimeKey]
    $windowPid = [Int64][P914Native]::GetWindowProcessId([IntPtr]$handle)
    if ($windowPid -ne [Int64]$Context.process.Id) { throw 'UIA element owner HWND belongs to a different process.' }
    $rect = $current.BoundingRectangle
    $value = $null
    try { $pattern = [System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern); $value = [string]$pattern.Current.Value } catch { }
    [ordered]@{ name = [string]$current.Name; automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; className = [string]$current.ClassName; processId = [Int64]$current.ProcessId; ownerWindowHandle = $handle; ownerWindowProcessId = $windowPid; isEnabled = [bool]$current.IsEnabled; isOffscreen = [bool]$current.IsOffscreen; isKeyboardFocusable = [bool]$current.IsKeyboardFocusable; value = $value; rectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height } }
}
function Get-P914ControlText([System.Windows.Automation.AutomationElement]$Element) {
    $current = $Element.Current
    if (-not [string]::IsNullOrEmpty([string]$current.Name)) { return [string]$current.Name }
    try { $value = [System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern); return [string]$value.Current.Value } catch { }
    try { $text = [System.Windows.Automation.TextPattern]$Element.GetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern); return [string]$text.DocumentRange.GetText(-1) } catch { }
    return ''
}
function Get-P914EditValue([System.Windows.Automation.AutomationElement]$Element) {
    try { $value = [System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern); $valueText = [string]$value.Current.Value; if (-not [string]::IsNullOrWhiteSpace($valueText)) { return $valueText } } catch { }
    try { $text = [System.Windows.Automation.TextPattern]$Element.GetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern); $textValue = [string]$text.DocumentRange.GetText(-1); if (-not [string]::IsNullOrWhiteSpace($textValue)) { return $textValue } } catch { }
    return (Get-P914ControlText $Element)
}
function Wait-P914Control([object]$Context, [string]$EvidenceDirectory, [string]$Container, [string]$AutomationId, [string]$ControlType, [string]$Name, [int]$TimeoutSeconds) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "Measured process exited while waiting for UIA control $AutomationId." }
        $element = Get-P914Control $Context $Container $AutomationId $ControlType $Name
        if ($null -ne $element) { return $element }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for PID-bound UIA control $AutomationId."
}
function Wait-P914NamedControl([object]$Context, [string]$EvidenceDirectory, [string]$Container, [string]$ControlType, [string]$Name, [int]$TimeoutSeconds) {
    if ([string]::IsNullOrWhiteSpace($ControlType) -or [string]::IsNullOrWhiteSpace($Name)) { throw 'Named UIA control wait requires exact ControlType and Name properties.' }
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "Measured process exited while waiting for named UIA control $ControlType '$Name'." }
        $element = Find-P914NamedControl $Context $Container $ControlType $Name
        if ($null -ne $element) { return $element }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for PID-bound UIA control $ControlType with exact Name '$Name'."
}
function Wait-P914Route([object]$Context, [string]$EvidenceDirectory, [string]$TargetPageId, [int]$AfterArrival, [int]$TimeoutSeconds) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory
        foreach ($route in $Context.routeEvents) { if ([Int64]$route.arrivalIndex -gt $AfterArrival -and [string]$route.targetPageId -ceq $TargetPageId) { return $route } }
        $Context.process.Refresh(); if ($Context.process.HasExited) { throw "Packaged Next exited before page-route to $TargetPageId." }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for page-route to $TargetPageId."
}
function Add-P914Action([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$ActionKind, [System.Windows.Automation.AutomationElement]$Element, [string]$Method, [string]$TargetPageId = '') {
    $snapshot = Get-P914ControlSnapshot $Context $Element
    if (-not $snapshot.isEnabled -or $snapshot.isOffscreen -or [Int64]$snapshot.processId -ne [Int64]$Context.process.Id) { throw "UIA action target failed PID/enabled/visible checks: $ActionKind" }
    $action = [ordered]@{ actionKind = $ActionKind; method = $Method; targetPageId = if ($TargetPageId) { $TargetPageId } else { $null }; issuedAtUtc = Get-P914Utc; target = $snapshot }
    $Manifest.route.actions.Add($action); Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $action
    return $action
}
function Invoke-P914Button([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$ActionKind, [System.Windows.Automation.AutomationElement]$Element) {
    $action = Add-P914Action $Context $Manifest $EvidenceDirectory $ActionKind $Element 'InvokePattern.Invoke'
    $pattern = [System.Windows.Automation.InvokePattern]$Element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern); $pattern.Invoke()
    $action.invocationReturnedAtUtc = Get-P914Utc
}
function Ensure-P914TreeItemVisible([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$TreeId, [string]$Name) {
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        $item = Find-P914NamedControl $Context 'main' 'ControlType.TreeItem' $Name $TreeId
        if ($null -eq $item) { throw "Exact navigation tree item is not exposed by UIA: $Name" }
        $current = $item.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { throw "Navigation item belongs to a different PID: $Name" }
        if (-not [bool]$current.IsOffscreen -and $current.BoundingRectangle.Width -gt 0 -and $current.BoundingRectangle.Height -gt 0) { return $item }
        $scrollItem = $null
        try { $scrollItem = [System.Windows.Automation.ScrollItemPattern]$item.GetCurrentPattern([System.Windows.Automation.ScrollItemPattern]::Pattern) } catch { $scrollItem = $null }
        if ($null -ne $scrollItem) {
            $action = [ordered]@{ actionKind = 'scroll-navigation-item-into-view'; method = 'ScrollItemPattern.ScrollIntoView'; targetPageId = $null; itemName = $Name; target = Get-P914ControlSnapshot $Context $item; issuedAtUtc = Get-P914Utc }
            $Manifest.route.actions.Add($action); Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $action
            $scrollItem.ScrollIntoView()
        } else {
            $tree = Get-P914Control $Context 'main' $TreeId 'ControlType.Tree' ''
            if ($null -eq $tree) { throw "Navigation tree is not exposed for bounded scrolling: $TreeId" }
            $scroll = $null
            try { $scroll = [System.Windows.Automation.ScrollPattern]$tree.GetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern) } catch { $scroll = $null }
            if ($null -eq $scroll -or -not [bool]$scroll.Current.VerticallyScrollable) { throw "Offscreen navigation item has no UIA scroll pattern: $Name" }
            $action = [ordered]@{ actionKind = 'scroll-navigation-tree'; method = 'ScrollPattern.Scroll'; targetPageId = $null; itemName = $Name; target = Get-P914ControlSnapshot $Context $tree; issuedAtUtc = Get-P914Utc }
            $Manifest.route.actions.Add($action); Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $action
            $scroll.Scroll([System.Windows.Automation.ScrollAmount]::NoAmount, [System.Windows.Automation.ScrollAmount]::LargeIncrement)
        }
        Start-Sleep -Milliseconds 75
    }
    throw "Navigation item stayed offscreen after bounded UIA scroll attempts: $Name"
}
function Ensure-P914CampusStaffGroupExpanded([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$TreeId) {
    $group = Ensure-P914TreeItemVisible $Context $Manifest $EvidenceDirectory $TreeId 'Campus Staff'
    $pattern = $null
    try { $pattern = [System.Windows.Automation.ExpandCollapsePattern]$group.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern) } catch { throw 'Campus Staff navigation group does not expose UIA ExpandCollapsePattern.' }
    if ($pattern.Current.ExpandCollapseState -eq [System.Windows.Automation.ExpandCollapseState]::Collapsed) {
        $action = Add-P914Action $Context $Manifest $EvidenceDirectory 'expand-campus-staff-navigation-group' $group 'ExpandCollapsePattern.Expand'
        $pattern.Expand()
        $stateWatch = [Diagnostics.Stopwatch]::StartNew()
        while ($stateWatch.Elapsed.TotalSeconds -lt 1.0) {
            $group = Find-P914NamedControl $Context 'main' 'ControlType.TreeItem' 'Campus Staff' $TreeId
            if ($null -eq $group) { throw 'Campus Staff UIA tree item disappeared after ExpandCollapsePattern.Expand.' }
            $pattern = [System.Windows.Automation.ExpandCollapsePattern]$group.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
            if ($pattern.Current.ExpandCollapseState -eq [System.Windows.Automation.ExpandCollapseState]::Expanded) { break }
            Start-Sleep -Milliseconds 100
        }
        if ($pattern.Current.ExpandCollapseState -eq [System.Windows.Automation.ExpandCollapseState]::Collapsed) {
            # Qt's UIA ExpandCollapsePattern can return without changing the actual
            # QTreeWidget item. Capture the live navigation window so the disclosure
            # affordance can be inspected, then require an operator-derived point
            # inside the freshly measured UIA parent Tree bounds.
            $group = Find-P914NamedControl $Context 'main' 'ControlType.TreeItem' 'Campus Staff' $TreeId
            $snapshot = Get-P914ControlSnapshot $Context $group
            $current = $group.Current; $rectangle = $current.BoundingRectangle
            if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [bool]$current.IsOffscreen -or $rectangle.Width -le 0 -or $rectangle.Height -le 0) { throw 'Campus Staff UIA click fallback target is not a visible measured TreeItem.' }
            $handle = [Int64]$snapshot.ownerWindowHandle; $measuredPid = [uint32]$Context.process.Id
            if ($handle -ne [Int64]$Context.mainWindowHandle -or [uint32][P914Native]::GetWindowProcessId([IntPtr]$handle) -ne $measuredPid) { throw 'Campus Staff UIA click fallback HWND is not the measured main window.' }
            $windowBounds = [P914Native]::GetBounds([IntPtr]$handle, $measuredPid)
            if ($rectangle.X -lt $windowBounds[0] -or $rectangle.Y -lt $windowBounds[1] -or ($rectangle.X + $rectangle.Width) -gt ($windowBounds[0] + $windowBounds[2]) -or ($rectangle.Y + $rectangle.Height) -gt ($windowBounds[1] + $windowBounds[3])) { throw 'Campus Staff UIA click fallback bounds leave the measured main HWND.' }
            $windowBoundsRecord = [ordered]@{ x = [int]$windowBounds[0]; y = [int]$windowBounds[1]; width = [int]$windowBounds[2]; height = [int]$windowBounds[3] }
            $tree = Get-P914Control $Context 'main' $TreeId 'ControlType.Tree' ''
            if ($null -eq $tree) { throw 'Measured navigation parent Tree is not exposed by UIA before Campus Staff disclosure inspection.' }
            $treeSnapshot = Get-P914ControlSnapshot $Context $tree
            $treeCurrent = $tree.Current; $treeRectangle = $treeCurrent.BoundingRectangle
            if ([uint32]$treeCurrent.ProcessId -ne $measuredPid -or [bool]$treeCurrent.IsOffscreen -or [string]$treeCurrent.ControlType.ProgrammaticName -cne 'ControlType.Tree' -or [string]$treeCurrent.AutomationId -cne $TreeId -or [Int64]$treeSnapshot.ownerWindowHandle -ne $handle -or $treeRectangle.Width -le 0 -or $treeRectangle.Height -le 0) { throw 'Navigation parent Tree failed its measured PID/HWND/type/bounds checks.' }
            if ($treeRectangle.X -lt $windowBounds[0] -or $treeRectangle.Y -lt $windowBounds[1] -or ($treeRectangle.X + $treeRectangle.Width) -gt ($windowBounds[0] + $windowBounds[2]) -or ($treeRectangle.Y + $treeRectangle.Height) -gt ($windowBounds[1] + $windowBounds[3])) { throw 'UIA parent Tree bounds leave the measured main HWND.' }
            $disclosureScreenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'campus-staff-navigation-before-disclosure-click' $Context.mainWindowRoot
            $treeImageBounds = [ordered]@{ x = [double]$treeRectangle.X - [double]$windowBoundsRecord.x; y = [double]$treeRectangle.Y - [double]$windowBoundsRecord.y; width = [double]$treeRectangle.Width; height = [double]$treeRectangle.Height }
            $itemImageBounds = [ordered]@{ x = [double]$rectangle.X - [double]$windowBoundsRecord.x; y = [double]$rectangle.Y - [double]$windowBoundsRecord.y; width = [double]$rectangle.Width; height = [double]$rectangle.Height }
            Write-Host ('Inspect ' + $disclosureScreenshot.relativePath + '. Current image=' + $disclosureScreenshot.width + 'x' + $disclosureScreenshot.height + ', screen origin=(' + $windowBoundsRecord.x + ',' + $windowBoundsRecord.y + '), UIA Tree image bounds=' + ($treeImageBounds | ConvertTo-Json -Compress) + ', Campus Staff TreeItem image bounds=' + ($itemImageBounds | ConvertTo-Json -Compress) + '. Enter the current visible disclosure glyph pixel x,y; blank input stops without a click.')
            $enteredPoint = Read-Host 'Measured disclosure pixel x,y'
            if ([string]::IsNullOrWhiteSpace($enteredPoint) -or $enteredPoint -notmatch '^\s*(\d+)\s*,\s*(\d+)\s*$') { throw 'No valid screenshot-derived Campus Staff disclosure point was supplied; navigation stopped without a click.' }
            $imageX = [int]$Matches[1]; $imageY = [int]$Matches[2]
            if ($imageX -lt 0 -or $imageY -lt 0 -or $imageX -ge [int]$disclosureScreenshot.width -or $imageY -ge [int]$disclosureScreenshot.height) { throw 'Campus Staff disclosure point is outside the captured PrintWindow image.' }
            $x = [int]$windowBounds[0] + $imageX; $y = [int]$windowBounds[1] + $imageY
            $group = Find-P914NamedControl $Context 'main' 'ControlType.TreeItem' 'Campus Staff' $TreeId
            if ($null -eq $group) { throw 'Campus Staff UIA TreeItem disappeared after screenshot inspection.' }
            $current = $group.Current; $rectangle = $current.BoundingRectangle
            $freshSnapshot = Get-P914ControlSnapshot $Context $group
            if ([uint32]$current.ProcessId -ne $measuredPid -or [bool]$current.IsOffscreen -or [Int64]$freshSnapshot.ownerWindowHandle -ne $handle -or [uint32][P914Native]::GetWindowProcessId([IntPtr]$handle) -ne $measuredPid) { throw 'Campus Staff UIA target or measured HWND identity changed after screenshot inspection.' }
            if ([double]$rectangle.X -ne [double]$snapshot.rectangle.x -or [double]$rectangle.Y -ne [double]$snapshot.rectangle.y -or [double]$rectangle.Width -ne [double]$snapshot.rectangle.width -or [double]$rectangle.Height -ne [double]$snapshot.rectangle.height) { throw 'Campus Staff UIA bounds changed after screenshot inspection; the inspected point is stale.' }
            $tree = Get-P914Control $Context 'main' $TreeId 'ControlType.Tree' ''
            if ($null -eq $tree) { throw 'Navigation parent Tree disappeared after screenshot inspection.' }
            $freshTreeSnapshot = Get-P914ControlSnapshot $Context $tree
            $treeCurrent = $tree.Current; $treeRectangle = $treeCurrent.BoundingRectangle
            if ([uint32]$treeCurrent.ProcessId -ne $measuredPid -or [bool]$treeCurrent.IsOffscreen -or [string]$treeCurrent.ControlType.ProgrammaticName -cne 'ControlType.Tree' -or [string]$treeCurrent.AutomationId -cne $TreeId -or [Int64]$freshTreeSnapshot.ownerWindowHandle -ne $handle) { throw 'Navigation parent Tree identity changed after screenshot inspection.' }
            if ([double]$treeRectangle.X -ne [double]$treeSnapshot.rectangle.x -or [double]$treeRectangle.Y -ne [double]$treeSnapshot.rectangle.y -or [double]$treeRectangle.Width -ne [double]$treeSnapshot.rectangle.width -or [double]$treeRectangle.Height -ne [double]$treeSnapshot.rectangle.height) { throw 'Navigation parent Tree bounds changed after screenshot inspection; the inspected point is stale.' }
            $freshWindowBounds = [P914Native]::GetBounds([IntPtr]$handle, $measuredPid)
            if (($freshWindowBounds -join ',') -cne ($windowBounds -join ',')) { throw 'Measured main window bounds changed after screenshot inspection; the inspected point is stale.' }
            if ($x -lt $treeRectangle.X -or $x -ge ($treeRectangle.X + $treeRectangle.Width) -or $y -lt $treeRectangle.Y -or $y -ge ($treeRectangle.Y + $treeRectangle.Height)) { throw 'Screenshot-derived Campus Staff disclosure point is outside the current UIA parent Tree bounds.' }
            if ($x -ge $rectangle.X -or $y -lt $rectangle.Y -or $y -ge ($rectangle.Y + $rectangle.Height)) { throw 'Screenshot-derived point is not in the current navigation Tree disclosure region adjacent to the Campus Staff row.' }
            if ($x -lt $windowBounds[0] -or $x -ge ($windowBounds[0] + $windowBounds[2]) -or $y -lt $windowBounds[1] -or $y -ge ($windowBounds[1] + $windowBounds[3])) { throw 'Screenshot-derived Campus Staff disclosure point is outside the measured main HWND.' }
            $freshTreeRectangle = [ordered]@{ x = [double]$treeRectangle.X; y = [double]$treeRectangle.Y; width = [double]$treeRectangle.Width; height = [double]$treeRectangle.Height }
            $freshItemRectangle = [ordered]@{ x = [double]$rectangle.X; y = [double]$rectangle.Y; width = [double]$rectangle.Width; height = [double]$rectangle.Height }
            $fallbackAction = [ordered]@{ actionKind = 'expand-campus-staff-navigation-group-uia-click-fallback'; method = 'PrintWindow-inspected disclosure indicator with screenshot-derived point bounded by UIA parent Tree and measured main HWND; bounded WM mouse click'; targetPageId = $null; target = $freshSnapshot; pointerAction = [ordered]@{ actionType = 'WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP'; screenX = $x; screenY = $y; sourceTarget = [ordered]@{ name = [string]$current.Name; automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; rectangle = $freshItemRectangle }; parentTreeTarget = $freshTreeSnapshot; targetProcessId = [Int64]$measuredPid; windowHandle = $handle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$handle); measuredMainWindowBounds = $windowBoundsRecord; disclosurePointDerivation = [ordered]@{ method = 'operator-inspected fresh PrintWindow pixel converted through captured main-window screen origin and checked against the fresh UIA parent Tree bounds'; screenshotRelativePath = [string]$disclosureScreenshot.relativePath; screenshotSha256 = [string]$disclosureScreenshot.sha256; screenshotCapturedAtUtc = [string]$disclosureScreenshot.capturedAtUtc; screenshotProcessId = [Int64]$disclosureScreenshot.processId; screenshotWindowHandle = [Int64]$disclosureScreenshot.windowHandle; screenshotWindowProcessId = [Int64]$disclosureScreenshot.windowProcessId; screenshotWidth = [int]$disclosureScreenshot.width; screenshotHeight = [int]$disclosureScreenshot.height; screenshotScreenBounds = $windowBoundsRecord; selectedImagePixel = [ordered]@{ x = $imageX; y = $imageY }; uiaTreeBoundsAtInspection = $freshTreeRectangle; uiaTreeItemBoundsAtInspection = $freshItemRectangle } }; expansionStateBefore = 'Collapsed'; issuedAtUtc = Get-P914Utc }
            $Manifest.route.staffDisclosureInspection = [ordered]@{ screenshotRelativePath = [string]$disclosureScreenshot.relativePath; screenshotSha256 = [string]$disclosureScreenshot.sha256; screenshotProcessId = [Int64]$disclosureScreenshot.processId; screenshotWindowHandle = [Int64]$disclosureScreenshot.windowHandle; parentTreeTarget = $freshTreeSnapshot; treeItemTarget = $freshSnapshot; selectedImagePixel = [ordered]@{ x = $imageX; y = $imageY }; screenPoint = [ordered]@{ x = $x; y = $y }; actionKind = [string]$fallbackAction.actionKind; method = [string]$fallbackAction.method }
            $Manifest.route.actions.Add($fallbackAction); Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $fallbackAction
            [P914Native]::ClickScreenPoint([IntPtr]$handle, $measuredPid, $x, $y)
            $stateWatch = [Diagnostics.Stopwatch]::StartNew()
            while ($stateWatch.Elapsed.TotalSeconds -lt 2.0) {
                $group = Find-P914NamedControl $Context 'main' 'ControlType.TreeItem' 'Campus Staff' $TreeId
                if ($null -eq $group) { throw 'Campus Staff UIA tree item disappeared after its bounded click fallback.' }
                $pattern = [System.Windows.Automation.ExpandCollapsePattern]$group.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
                if ($pattern.Current.ExpandCollapseState -eq [System.Windows.Automation.ExpandCollapseState]::Expanded) { $fallbackAction.expansionStateAfter = 'Expanded'; $fallbackAction.completedAtUtc = Get-P914Utc; break }
                Start-Sleep -Milliseconds 100
            }
        }
        if ($pattern.Current.ExpandCollapseState -ne [System.Windows.Automation.ExpandCollapseState]::Expanded) { throw "Campus Staff UIA ExpandCollapse action and one screenshot-derived bounded disclosure click did not produce an Expanded state: $($pattern.Current.ExpandCollapseState)." }
    } elseif ($pattern.Current.ExpandCollapseState -ne [System.Windows.Automation.ExpandCollapseState]::Expanded) {
        throw 'Campus Staff navigation group is in a UIA state other than Collapsed or Expanded.'
    }
    Start-Sleep -Milliseconds 100
    $tree = Get-P914Control $Context 'main' $TreeId 'ControlType.Tree' ''
    if ($null -eq $tree) { throw 'Navigation tree disappeared after expanding Campus Staff.' }
    $treeSnapshot = Save-P914UiaTreeSnapshot $Context $Manifest $EvidenceDirectory 'navigation-after-campus-staff-expand' $tree
    $childItems = @()
    foreach ($child in @([ordered]@{ name = 'Native English Teachers'; pageId = 'native_english_teachers' }, [ordered]@{ name = 'GS Team'; pageId = 'gs_team' })) {
        $childName = [string]$child.name
        $childElement = Find-P914NamedControl $Context 'main' 'ControlType.TreeItem' $childName $TreeId
        if ($null -eq $childElement) { throw "Expanded Campus Staff tree does not expose required child TreeItem: $childName" }
        $childCurrent = $childElement.Current
        if ([uint32]$childCurrent.ProcessId -ne [uint32]$Context.process.Id -or [bool]$childCurrent.IsOffscreen -or $childCurrent.BoundingRectangle.Width -le 0 -or $childCurrent.BoundingRectangle.Height -le 0) { throw "Expanded Campus Staff child TreeItem is not visible and PID-bound: $childName" }
        $childItems += [ordered]@{ name = [string]$childCurrent.Name; pageId = [string]$child.pageId; automationId = [string]$childCurrent.AutomationId; controlType = [string]$childCurrent.ControlType.ProgrammaticName; processId = [Int64]$childCurrent.ProcessId; ownerWindowHandle = [Int64](Get-P914ControlSnapshot $Context $childElement).ownerWindowHandle; rectangle = [ordered]@{ x = [double]$childCurrent.BoundingRectangle.X; y = [double]$childCurrent.BoundingRectangle.Y; width = [double]$childCurrent.BoundingRectangle.Width; height = [double]$childCurrent.BoundingRectangle.Height } }
    }
    $Manifest.route.navigationTreeAfterCampusStaffExpand = [ordered]@{ treeSnapshotRelativePath = [string]$treeSnapshot.manifestRecord.relativePath; visibleTreeItems = @($treeSnapshot.record.treeItems); campusStaffExpanded = [string]$pattern.Current.ExpandCollapseState; requiredChildren = $childItems; source = 'bounded UIA ControlView descendants of the measured navigation tree and live PID-bound child TreeItems' }
}
function Select-P914Route([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Name, [string]$TargetPageId) {
    $treeId = 'QApplication.classMngrNextApplicationShell.QWidget.classMngrNextNavigationPanel.classMngrNextNavigationTree'
    if ($TargetPageId -in @('native_english_teachers', 'gs_team')) { Ensure-P914CampusStaffGroupExpanded $Context $Manifest $EvidenceDirectory $treeId }
    $null = Ensure-P914TreeItemVisible $Context $Manifest $EvidenceDirectory $treeId $Name
    # Qt's QTreeWidget UIA SelectionItemPattern.Select can update selection
    # without dispatching the page navigation signal. Use the same verified
    # UIA-derived row click path as the accepted P9-13 Next route runner.
    $item = Find-P914NamedControl $Context 'main' 'ControlType.TreeItem' $Name $treeId
    if ($null -eq $item) { throw "Navigation item disappeared before the bounded UIA click: $Name" }
    $current = $item.Current
    if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or -not [bool]$current.IsEnabled -or [bool]$current.IsOffscreen) { throw "Navigation item failed measured PID, enabled, or visibility checks before input: $Name" }
    $snapshot = Get-P914ControlSnapshot $Context $item
    $handle = [Int64]$Context.mainWindowHandle
    $measuredProcessId = [uint32]$Context.process.Id
    if ($handle -le 0 -or [Int64]$snapshot.ownerWindowHandle -ne $handle -or [Int64]$snapshot.ownerWindowProcessId -ne [Int64]$measuredProcessId -or [uint32][P914Native]::GetWindowProcessId([IntPtr]$handle) -ne $measuredProcessId) { throw "Navigation item owner HWND is not the measured main window: $Name" }
    $rectangle = $current.BoundingRectangle
    if ([double]::IsInfinity($rectangle.X) -or [double]::IsInfinity($rectangle.Y) -or [double]::IsNaN($rectangle.X) -or [double]::IsNaN($rectangle.Y) -or $rectangle.Width -le 0 -or $rectangle.Height -le 0) { throw "Navigation item has no nonempty UIA BoundingRectangle: $Name" }
    $windowBounds = [P914Native]::GetBounds([IntPtr]$handle, $measuredProcessId)
    if ($rectangle.X -lt $windowBounds[0] -or $rectangle.Y -lt $windowBounds[1] -or ($rectangle.X + $rectangle.Width) -gt ($windowBounds[0] + $windowBounds[2]) -or ($rectangle.Y + $rectangle.Height) -gt ($windowBounds[1] + $windowBounds[3])) { throw "Navigation item UIA bounds leave the measured main HWND: $Name" }
    $x = [int][Math]::Round($rectangle.X + $rectangle.Width / 2)
    $y = [int][Math]::Round($rectangle.Y + $rectangle.Height / 2)
    if ($x -lt $windowBounds[0] -or $x -ge ($windowBounds[0] + $windowBounds[2]) -or $y -lt $windowBounds[1] -or $y -ge ($windowBounds[1] + $windowBounds[3])) { throw "UIA-derived navigation point leaves the measured main HWND: $Name" }
    $arrivalBefore = [Int64]$Context.arrival
    $action = [ordered]@{
        actionKind = 'navigate-' + $TargetPageId
        method = 'UIA-targeted window click'
        targetPageId = $TargetPageId
        target = $snapshot
        pointerAction = [ordered]@{
            kind = 'WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP at the center of the reacquired UIA TreeItem BoundingRectangle'
            actionType = 'WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP'
            screenX = $x; screenY = $y
            sourceTarget = [ordered]@{ name = [string]$current.Name; automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; rectangle = [ordered]@{ x = [double]$rectangle.X; y = [double]$rectangle.Y; width = [double]$rectangle.Width; height = [double]$rectangle.Height } }
            targetProcessId = [Int64]$measuredProcessId; windowHandle = $handle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$handle)
            measuredMainWindowBounds = [ordered]@{ x = [Int64]$windowBounds[0]; y = [Int64]$windowBounds[1]; width = [Int64]$windowBounds[2]; height = [Int64]$windowBounds[3] }
        }
        issuedAtUtc = Get-P914Utc
    }
    $Manifest.route.actions.Add($action)
    [P914Native]::ClickScreenPoint([IntPtr]$handle, $measuredProcessId, $x, $y)
    $route = Wait-P914Route $Context $EvidenceDirectory $TargetPageId $arrivalBefore 30
    $action.pageRouteEvent = $route; $action.verifiedByFollowingPageRoute = $true; $action.pointerInputReturnedAtUtc = Get-P914Utc; $action.completedAtUtc = Get-P914Utc
    Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $action
    return $route
}
function Wait-P914Lifecycle([object]$Context, [string]$EvidenceDirectory, [string]$PageKey, [string]$State, [int]$AfterArrival, [int]$TimeoutSeconds = 30) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory
        $matches = @($Context.lifecycleEvents | Where-Object { [Int64]$_.arrivalIndex -gt $AfterArrival -and [string]$_.record.event -ceq 'page-lifecycle' -and [string]$_.record.page_key -ceq $PageKey -and [string]$_.record.state -ceq $State })
        if ($matches.Count -gt 0) { return $matches[$matches.Count - 1] }
        $Context.process.Refresh(); if ($Context.process.HasExited) { throw "Process exited before lifecycle event $PageKey/$State." }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for page lifecycle $PageKey/$State."
}
function Get-P914PageModelCounts([object]$LifecycleRecord) {
    $models = @($LifecycleRecord.record.page.item_models)
    $counts = [Collections.Generic.List[object]]::new()
    foreach ($model in $models) { $counts.Add([ordered]@{ className = [string]$model.class_name; objectName = [string]$model.object_name; rootRows = [Int64]$model.root_rows }) }
    [ordered]@{ sampleAvailable = [bool]$LifecycleRecord.record.sample_available; pageInstanceAvailable = [bool]$LifecycleRecord.record.page_instance_available; itemModelCount = if ($null -ne $LifecycleRecord.record.page.item_model_count) { [Int64]$LifecycleRecord.record.page.item_model_count } else { $null }; itemModelRootRows = if ($null -ne $LifecycleRecord.record.page.item_model_root_rows) { [Int64]$LifecycleRecord.record.page.item_model_root_rows } else { $null }; itemModels = $counts.ToArray(); capturedLifecycleArrivalIndex = [Int64]$LifecycleRecord.arrivalIndex }
}
function Get-P914GridMetrics([System.Windows.Automation.AutomationElement]$Element) {
    try {
        $grid = [System.Windows.Automation.GridPattern]$Element.GetCurrentPattern([System.Windows.Automation.GridPattern]::Pattern)
        return [ordered]@{ available = $true; rowCount = [int]$grid.Current.RowCount; columnCount = [int]$grid.Current.ColumnCount; pattern = 'GridPattern'; failure = $null }
    } catch { return [ordered]@{ available = $false; rowCount = $null; columnCount = $null; pattern = $null; failure = $_.Exception.Message } }
}
function Save-P914UiaSnapshot([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Name, [System.Windows.Automation.AutomationElement]$Element, [object]$StaffTableEvidence = $null) {
    $snapshot = Get-P914ControlSnapshot $Context $Element
    $grid = Get-P914GridMetrics $Element
    $record = [ordered]@{ name = $Name; capturedAtUtc = Get-P914Utc; control = $snapshot; gridPattern = $grid }
    if ($null -ne $StaffTableEvidence) { $record.staffTableEvidence = $StaffTableEvidence }
    $path = Join-Path (Join-Path $EvidenceDirectory 'uia') ($Name + '.json')
    Write-P914Json $path $record
    $file = Get-Item -LiteralPath $path
    $manifestRecord = [ordered]@{ relativePath = 'uia/' + $Name + '.json'; sha256 = Get-P914FileSha256 $path; sizeBytes = [Int64]$file.Length; capturedAtUtc = $record.capturedAtUtc; processId = [Int64]$Context.process.Id; windowHandle = [Int64]$snapshot.ownerWindowHandle }
    $Manifest.route.uiaSnapshots.Add($manifestRecord)
    return [ordered]@{ path = $path; record = $record; manifestRecord = $manifestRecord }
}
function Save-P914UiaTreeSnapshot([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Name, [System.Windows.Automation.AutomationElement]$Element) {
    $rootSnapshot = Get-P914ControlSnapshot $Context $Element
    if ([Int64]$rootSnapshot.processId -ne [Int64]$Context.process.Id) { throw "UIA tree root belongs to a different process: $Name" }
    $nodes = [Collections.Generic.List[object]]::new()
    $descendants = $Element.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($node in $descendants) {
        $current = $node.Current
        if ([Int64]$current.ProcessId -ne [Int64]$Context.process.Id) { throw "UIA descendant belongs to a different process in $Name." }
        if ([string]$current.ControlType.ProgrammaticName -cne 'ControlType.TreeItem') { continue }
        $rect = $current.BoundingRectangle
        $nodes.Add([ordered]@{ name = [string]$current.Name; automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; processId = [Int64]$current.ProcessId; isEnabled = [bool]$current.IsEnabled; isOffscreen = [bool]$current.IsOffscreen; rectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height } })
    }
    $record = [ordered]@{ name = $Name; capturedAtUtc = Get-P914Utc; root = $rootSnapshot; treeItems = $nodes.ToArray() }
    $path = Join-Path (Join-Path $EvidenceDirectory 'uia') ($Name + '.json')
    Write-P914Json $path $record
    $manifestRecord = [ordered]@{ relativePath = 'uia/' + $Name + '.json'; sha256 = Get-P914FileSha256 $path; sizeBytes = [Int64](Get-Item -LiteralPath $path).Length; capturedAtUtc = $record.capturedAtUtc; processId = [Int64]$Context.process.Id; windowHandle = [Int64]$rootSnapshot.ownerWindowHandle }
    $Manifest.route.uiaSnapshots.Add($manifestRecord)
    return [ordered]@{ path = $path; record = $record; manifestRecord = $manifestRecord }
}
function Save-P914UiaDescendantSnapshot([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Name, [System.Windows.Automation.AutomationElement]$Element) {
    $root = Get-P914ControlSnapshot $Context $Element
    $nodes = [Collections.Generic.List[object]]::new()
    $descendants = $Element.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($node in $descendants) {
        $current = $node.Current
        if ([Int64]$current.ProcessId -ne [Int64]$Context.process.Id) { throw "UIA descendant belongs to a different process in $Name." }
        $rect = $current.BoundingRectangle
        $grid = $null
        try { $pattern = [System.Windows.Automation.GridPattern]$node.GetCurrentPattern([System.Windows.Automation.GridPattern]::Pattern); $grid = [ordered]@{ available = $true; rowCount = [int]$pattern.Current.RowCount; columnCount = [int]$pattern.Current.ColumnCount } } catch { $grid = [ordered]@{ available = $false; rowCount = $null; columnCount = $null } }
        $nodes.Add([ordered]@{ name = [string]$current.Name; automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; className = [string]$current.ClassName; processId = [Int64]$current.ProcessId; isEnabled = [bool]$current.IsEnabled; isOffscreen = [bool]$current.IsOffscreen; rectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height }; gridPattern = $grid; runtimeId = (Get-P914RuntimeKey $node) })
    }
    $record = [ordered]@{ name = $Name; capturedAtUtc = Get-P914Utc; root = $root; descendants = $nodes.ToArray() }
    $path = Join-Path (Join-Path $EvidenceDirectory 'uia') ($Name + '.json')
    Write-P914Json $path $record
    $manifestRecord = [ordered]@{ relativePath = 'uia/' + $Name + '.json'; sha256 = Get-P914FileSha256 $path; sizeBytes = [Int64](Get-Item -LiteralPath $path).Length; capturedAtUtc = $record.capturedAtUtc; processId = [Int64]$Context.process.Id; windowHandle = [Int64]$root.ownerWindowHandle }
    $Manifest.route.uiaSnapshots.Add($manifestRecord)
    return [ordered]@{ path = $path; record = $record; manifestRecord = $manifestRecord }
}
function Capture-P914Screenshot([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Name, [System.Windows.Automation.AutomationElement]$WindowRoot) {
    $rootCurrent = $WindowRoot.Current
    if ([uint32]$rootCurrent.ProcessId -ne [uint32]$Context.process.Id -or [string]$rootCurrent.ControlType.ProgrammaticName -cne 'ControlType.Window') { throw "Screenshot root is not a measured-process Window: $Name" }
    $runtimeKey = Get-P914RuntimeKey $WindowRoot
    if (-not $Context.ownerHandles.ContainsKey($runtimeKey)) {
        $handle = [Int64]$rootCurrent.NativeWindowHandle
        if ($handle -le 0) { throw "Screenshot root has no native HWND: $Name" }
        $Context.ownerHandles[$runtimeKey] = $handle
    }
    $handle = [Int64]$Context.ownerHandles[$runtimeKey]
    $measuredProcessId = [uint32]$Context.process.Id
    if ([uint32][P914Native]::GetWindowProcessId([IntPtr]$handle) -ne $measuredProcessId) { throw "Screenshot HWND owner changed before capture: $Name" }
    [P914Native]::RaiseWithoutActivation([IntPtr]$handle, $measuredProcessId)
    $bounds = [P914Native]::GetBounds([IntPtr]$handle, $measuredProcessId); $width = [int]$bounds[2]; $height = [int]$bounds[3]
    if ($width -le 0 -or $height -le 0) { throw "Screenshot window bounds are invalid: $Name" }
    $bitmap = [Drawing.Bitmap]::new($width, $height); $graphics = [Drawing.Graphics]::FromImage($bitmap); $dc = [IntPtr]::Zero; $method = 'PrintWindow(PW_RENDERFULLCONTENT)'
    $path = Join-Path (Join-Path $EvidenceDirectory 'screenshots') ($Name + '.png')
    try {
        $dc = $graphics.GetHdc()
        if (-not [P914Native]::RenderWindow([IntPtr]$handle, $measuredProcessId, $dc, 2)) { $method = 'PrintWindow(PW_CLIENTONLY)'; if (-not [P914Native]::RenderWindow([IntPtr]$handle, $measuredProcessId, $dc, 0)) { throw "PrintWindow failed for measured UIA window $Name." } }
        $graphics.ReleaseHdc($dc); $dc = [IntPtr]::Zero; $bitmap.Save($path, [Drawing.Imaging.ImageFormat]::Png)
    } finally { if ($dc -ne [IntPtr]::Zero) { $graphics.ReleaseHdc($dc) }; $graphics.Dispose(); $bitmap.Dispose() }
    if ([uint32][P914Native]::GetWindowProcessId([IntPtr]$handle) -ne $measuredProcessId) { throw "Screenshot HWND owner changed after capture: $Name" }
    $file = Get-Item -LiteralPath $path
    $record = [ordered]@{ name = $Name; relativePath = 'screenshots/' + $Name + '.png'; sha256 = Get-P914FileSha256 $path; sizeBytes = [Int64]$file.Length; width = $width; height = $height; captureMethod = $method; processId = [Int64]$Context.process.Id; windowHandle = $handle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$handle); screenBounds = [ordered]@{ x = [int]$bounds[0]; y = [int]$bounds[1]; width = [int]$width; height = [int]$height }; capturedAtUtc = Get-P914Utc }
    $Manifest.route.screenshots.Add($record)
    return $record
}
function Add-P914StableCheckpoint([object]$Context, [object]$Manifest, [string]$Name) {
    $Context.sampler.SetContext('normal-use', $Name)
    $Context.process.Refresh(); if ($Context.process.HasExited) { throw "Cannot capture stable checkpoint after process exit: $Name" }
    $sample = [ordered]@{ point = $Name; category = 'normal-use'; capturedAtUtc = Get-P914Utc; processId = [Int64]$Context.process.Id; workingSetBytes = [Int64]$Context.process.WorkingSet64; privateBytesDiagnosticOnly = [Int64]$Context.process.PrivateMemorySize64; handles = [Int64]$Context.process.HandleCount; threadCount = [Int64]$Context.process.Threads.Count; strictExclusiveLimitBytes = $script:P914WorkingSetStrictExclusive }
    if ([Int64]$sample.workingSetBytes -ge $script:P914WorkingSetStrictExclusive) { throw "Stable working-set checkpoint reached the strict exclusive cap at ${Name}: $($sample.workingSetBytes) bytes." }
    $Manifest.memory.stableCheckpoints.Add($sample)
    return $sample
}
function Set-P914SamplerContext([object]$Context, [string]$Category, [string]$Operation) { $Context.sampler.SetContext($Category, $Operation) }
function Get-P914SamplerSummary([object]$Context, [string]$EvidenceDirectory) {
    $samples = @($Context.sampler.Snapshot())
    $path = Join-Path $EvidenceDirectory 'process-metrics.jsonl'
    [IO.File]::WriteAllText($path, '', [Text.UTF8Encoding]::new($false))
    $operationPeak = [Int64]0; $normalMax = [Int64]0; $index = 0
    foreach ($sample in $samples) {
        $line = [ordered]@{ sampleIndex = $index; capturedAtUtc = [string]$sample.capturedAtUtc; category = [string]$sample.category; operation = [string]$sample.operation; pid = [Int64]$sample.processId; workingSetBytes = [Int64]$sample.workingSetBytes; privateBytesDiagnosticOnly = [Int64]$sample.privateBytesDiagnosticOnly; peakWorkingSetBytes = [Int64]$sample.peakWorkingSetBytes }
        Add-P914JsonLine $path $line
        if ($line.category -ceq 'operation') { $operationPeak = [Math]::Max($operationPeak, [Int64]$line.workingSetBytes) }
        if ($line.category -ceq 'normal-use') { $normalMax = [Math]::Max($normalMax, [Int64]$line.workingSetBytes) }
        $index++
    }
    [ordered]@{ path = 'process-metrics.jsonl'; sha256 = Get-P914FileSha256 $path; sampleCount = $index; normalUsePeakWorkingSetBytes = $normalMax; operationPeakWorkingSetBytes = $operationPeak; operationPeakThresholdBytesExclusiveAdvisory = $script:P914OperationPeakAdvisoryExclusive; operationPeakAdvisoryExceeded = ($operationPeak -ge $script:P914OperationPeakAdvisoryExclusive); privateBytesDiagnosticOnly = $true; stableCheckpointLimitBytesExclusive = $script:P914WorkingSetStrictExclusive; returnToBaselineAllowance = 'none defined; no baseline return allowance is asserted' }
}
function Get-P914LifecycleSummary([object]$Context) {
    $records = [Collections.Generic.List[object]]::new()
    foreach ($entry in $Context.lifecycleEvents) {
        $record = $entry.record
        $propertyNames = @($record.PSObject.Properties | ForEach-Object { [string]$_.Name })
        $records.Add([ordered]@{ arrivalIndex = [Int64]$entry.arrivalIndex; stream = [string]$entry.stream; event = if ($propertyNames -contains 'event') { [string]$record.event } else { $null }; pageKey = if ($propertyNames -contains 'page_key') { [string]$record.page_key } else { $null }; state = if ($propertyNames -contains 'state') { [string]$record.state } else { $null }; pageInstanceAvailable = if ($propertyNames -contains 'page_instance_available') { [bool]$record.page_instance_available } else { $null }; sampleAvailable = if ($propertyNames -contains 'sample_available') { [bool]$record.sample_available } else { $null }; page = if ($propertyNames -contains 'page') { $record.page } else { $null }; rawLine = [string]$entry.line })
    }
    return $records.ToArray()
}
function Get-P914RouteSummary([object]$Context) { @($Context.routeEvents | ForEach-Object { $_ }) }
function Save-P914RouteManifest([object]$Manifest, [string]$EvidenceDirectory) {
    $path = Join-Path $EvidenceDirectory 'route-manifest.json'
    Write-P914Json $path $Manifest.route
    $Manifest.routeManifest = [ordered]@{ path = 'route-manifest.json'; sha256 = Get-P914FileSha256 $path }
}
function Wait-P914ProcessExit([object]$Context, [string]$EvidenceDirectory, [int]$TimeoutSeconds) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) { Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh(); if ($Context.process.HasExited) { return [ordered]@{ observed = $true; normalExit = $true; exitCode = [int]$Context.process.ExitCode; closeMethod = 'WindowPattern.Close'; forcedTermination = $false; exitedAtUtc = Get-P914Utc } }; Start-Sleep -Milliseconds 100 }
    return [ordered]@{ observed = $false; normalExit = $false; exitCode = $null; closeMethod = 'WindowPattern.Close'; forcedTermination = $false; timeoutSeconds = $TimeoutSeconds; observedAtUtc = Get-P914Utc }
}
function Invoke-P914NaturalClose([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [int]$TimeoutSeconds) {
    $Context.process.Refresh()
    if ($Context.process.HasExited) { return [ordered]@{ observed = $true; normalExit = $true; exitCode = [int]$Context.process.ExitCode; closeMethod = 'already-exited-naturally'; forcedTermination = $false; exitedAtUtc = Get-P914Utc } }
    $root = Get-P914MainRoot $Context
    if ($null -eq $root) { throw 'Natural close could not resolve the measured main window.' }
    $snapshot = Get-P914ControlSnapshot $Context $root
    $closeAction = [ordered]@{ actionKind = 'close-main-window-naturally'; method = 'WindowPattern.Close'; issuedAtUtc = Get-P914Utc; target = $snapshot; forcedTermination = $false }
    $Manifest.route.actions.Add($closeAction)
    try {
        $pattern = [System.Windows.Automation.WindowPattern]$root.GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern)
        $pattern.Close()
        $closeAction.closeReturnedAtUtc = Get-P914Utc
        $closeAction.completedAtUtc = $closeAction.closeReturnedAtUtc
        $exit = Wait-P914ProcessExit $Context $EvidenceDirectory $TimeoutSeconds
        $closeAction.processExitEvidence = $exit
        $Context.closeAction = $closeAction; $Context.processExit = $exit
    } catch {
        $Context.closeAction = $closeAction
        Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $closeAction
        throw
    }
    Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $closeAction
    if (-not [bool]$exit.normalExit -or $null -eq $exit.exitCode -or [int]$exit.exitCode -ne 0) { throw 'Packaged app did not exit naturally with code 0 after WindowPattern.Close.' }
    return $exit
}
function Get-P914RuntimeCleanup([object]$Context, [string]$EvidenceDirectory) {
    $runtime = Join-Path $EvidenceDirectory 'runtime'
    $settings = Get-P914CanonicalInventory (Join-Path $runtime 'settings')
    $settingsInventoryPath = Join-Path $runtime 'settings-after-exit-inventory.json'
    Write-P914Json $settingsInventoryPath $settings
    $temp = Get-P914CanonicalInventory (Join-Path $runtime 'temp')
    $diagnostics = Join-Path $runtime 'diagnostics'
    $sidecars = @()
    foreach ($suffix in @('-wal', '-shm', '-journal')) { $path = [string]$Context.workspacePath + $suffix; if (Test-Path -LiteralPath $path) { $sidecars += [IO.Path]::GetFullPath($path) } }
    [ordered]@{ capturedAtUtc = Get-P914Utc; processExited = [bool]$Context.process.HasExited; forcedTermination = [bool]$Context.forcedTermination; workspaceSidecarFilesAfterExit = $sidecars; isolatedTempInventoryAfterExit = [ordered]@{ fileCount = [int]$temp.fileCount; sha256 = [string]$temp.sha256; paths = @($temp.files | ForEach-Object { [string]$_.path }) }; settingsInventoryAfterExit = [ordered]@{ relativePath = 'runtime/settings-after-exit-inventory.json'; inventoryFileSha256 = Get-P914FileSha256 $settingsInventoryPath; fileCount = [int]$settings.fileCount; sha256 = [string]$settings.sha256; files = @($settings.files) }; shutdownTracePath = if (Test-Path -LiteralPath (Join-Path $diagnostics 'application-shutdown.jsonl')) { 'runtime/diagnostics/application-shutdown.jsonl' } else { $null }; returnToBaselineAllowance = 'none defined; only captured return and cleanup observations are reported' }
}

function Invoke-P914StaffWorkflow([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [int]$TimeoutSeconds) {
    $route = $Manifest.route
    $route.staffImplementation = [ordered]@{ routePageKeys = @($Manifest.packageProvenance.staffImplementationProof.routePageKeys); routeMode = [string]$Manifest.routeMode; implementationProof = $Manifest.packageProvenance.staffImplementationProof }
    $preparation = Read-P914Json (Join-Path (Join-Path $EvidenceDirectory 'workspace') 'preparation-receipt.json')
    $scenario = [Collections.Generic.List[object]]::new()
    $steps = @(
        @{ name = 'Native English Teachers'; page = 'native_english_teachers'; table = 'nativeEnglishTeachersTable'; step = 'native-english-first' },
        @{ name = 'GS Team'; page = 'gs_team'; table = 'gsTeamTable'; step = 'gs-team-first' },
        @{ name = 'Native English Teachers'; page = 'native_english_teachers'; table = 'nativeEnglishTeachersTable'; step = 'native-english-reentry' },
        @{ name = 'GS Team'; page = 'gs_team'; table = 'gsTeamTable'; step = 'gs-team-reentry' }
    )
    $previousPageKey = ''
    foreach ($step in $steps) {
        $priorArrival = [Int64]$Context.arrival
        Set-P914SamplerContext $Context 'operation' $step.step
        $routeEvent = Select-P914Route $Context $Manifest $EvidenceDirectory ([string]$step.name) ([string]$step.page)
        $pageKey = [string]$step.page
        $activate = Wait-P914Lifecycle $Context $EvidenceDirectory $pageKey 'activated' $priorArrival $TimeoutSeconds
        $suspendedPrevious = $null
        if ($previousPageKey -in @('native_english_teachers', 'gs_team')) { $suspendedPrevious = Wait-P914Lifecycle $Context $EvidenceDirectory $previousPageKey 'suspended' $priorArrival $TimeoutSeconds }
        $postRoutePageSnapshot = $null
        if ($step.step -ceq 'native-english-first') {
            $postRoutePageSnapshot = Save-P914UiaDescendantSnapshot $Context $Manifest $EvidenceDirectory 'native-english-page-main-after-route' $Context.mainWindowRoot
            $route.nativeEnglishPostRouteUiaSnapshotRelativePath = [string]$postRoutePageSnapshot.manifestRecord.relativePath
        }
        $title = Wait-P914NamedControl $Context $EvidenceDirectory 'main' 'ControlType.Text' ([string]$step.name) $TimeoutSeconds
        $titleSnapshot = Save-P914UiaSnapshot $Context $Manifest $EvidenceDirectory ([string]$step.step + '-page-title') $title
        if ([string]$titleSnapshot.record.control.name -cne [string]$step.name -or [string]$titleSnapshot.record.control.controlType -cne 'ControlType.Text' -or [Int64]$titleSnapshot.record.control.processId -ne [Int64]$Context.process.Id -or [Int64]$titleSnapshot.record.control.ownerWindowProcessId -ne [Int64]$Context.process.Id -or [Int64]$titleSnapshot.record.control.ownerWindowHandle -ne [Int64]$Context.mainWindowHandle) { throw "Runtime UIA page-title identity is not measured and exact for ${pageKey}." }
        $titleAutomationId = [string]$titleSnapshot.record.control.automationId
        if ($titleAutomationId -notmatch '\.pageTitle$') { throw "Runtime UIA page-title AutomationId has no exact pageTitle leaf for ${pageKey}: '$titleAutomationId'." }
        $tableId = $titleAutomationId.Substring(0, $titleAutomationId.Length - '.pageTitle'.Length) + '.' + [string]$step.table
        $table = Wait-P914Table $Context $EvidenceDirectory 'main' $tableId $TimeoutSeconds
        $runtimeTableId = [string]$table.Current.AutomationId
        if ($runtimeTableId -cne $tableId -or [string]$table.Current.ControlType.ProgrammaticName -notin @('ControlType.Table', 'ControlType.DataGrid') -or [uint32]$table.Current.ProcessId -ne [uint32]$Context.process.Id) { throw "Runtime UIA table identity differs from the exact route table ID ${tableId}." }
        $expectedHeaders = if ($pageKey -ceq 'native_english_teachers') { @('Name', 'Position', 'Phone Number', 'Email', 'Birthday', 'Nationality') } else { @('Name', 'Korean Name', 'Position', 'Phone Number', 'Birthday') }
        $expectedRows = if ($pageKey -ceq 'native_english_teachers') { @($preparation.nativeEnglish.rows) } else { @($preparation.gsTeam.rows) }
        $screenshot = $null
        if ($step.step -in @('native-english-first', 'gs-team-first')) { $screenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory $step.table $Context.mainWindowRoot }
        $tableEvidence = Get-P914StaffTableEvidence $Context $Manifest $EvidenceDirectory $table $tableId $expectedHeaders $expectedRows
        $tableSnapshot = Save-P914UiaSnapshot $Context $Manifest $EvidenceDirectory ([string]$step.step + '-table') $table $tableEvidence
        $modelCounts = Get-P914PageModelCounts $activate
        if (-not [bool]$tableSnapshot.record.gridPattern.available) { throw "UIA GridPattern is required for staff table ${pageKey}." }
        $tableRowCount = [int]$tableSnapshot.record.gridPattern.rowCount
        $modelMatches = @($modelCounts.itemModels | Where-Object { [string]$_.className -match 'TableModel' -and [Int64]$_.rootRows -eq 96 })
        if ($modelMatches.Count -ne 1 -or -not [bool]$modelCounts.sampleAvailable -or -not [bool]$modelCounts.pageInstanceAvailable) { throw "Packaged page lifecycle diagnostics did not expose exactly one active 96-row table model for ${pageKey}." }
        if ($tableRowCount -ne 96) { throw "UIA GridPattern row count differs from the 96-row fixture on ${pageKey}: $tableRowCount" }
        if ([int]$tableEvidence.columnCount -ne $expectedHeaders.Count -or @($tableEvidence.headers).Count -ne $expectedHeaders.Count -or (@($tableEvidence.headers | ForEach-Object { [string]$_.text }) -join "`n") -cne ($expectedHeaders -join "`n")) { throw "UIA TablePattern header columns differ from the exact current Next model headers for ${pageKey}." }
        if (@($tableEvidence.identityCellChecks).Count -ne 96 -or -not [bool]$tableEvidence.allGridRowsExposed) { throw "UIA GridPattern did not expose every identity cell for ${pageKey}." }
        if (@($tableEvidence.renderedIdentitySamples).Count -ne 3 -or @($tableEvidence.renderedIdentitySamples | Where-Object { -not [bool]$_.isOffscreen -and [double]$_.rectangle.width -gt 0 -and [double]$_.rectangle.height -gt 0 }).Count -ne 3) { throw "First, middle, and last Staff identity cells were not visibly rendered through UIA for ${pageKey}." }
        $checkpoint = Add-P914StableCheckpoint $Context $Manifest $step.step
        $observation = [ordered]@{ step = [string]$step.step; pageId = $pageKey; routeEventArrivalIndex = [Int64]$routeEvent.arrivalIndex; activationLifecycle = $modelCounts; previousStaffPageSuspension = if ($null -ne $suspendedPrevious) { Get-P914PageModelCounts $suspendedPrevious } else { $null }; postRoutePageUiaSnapshotRelativePath = if ($null -ne $postRoutePageSnapshot) { [string]$postRoutePageSnapshot.manifestRecord.relativePath } else { $null }; pageTitleSnapshotRelativePath = [string]$titleSnapshot.manifestRecord.relativePath; pageTitleControl = $titleSnapshot.record.control; tableSnapshotRelativePath = [string]$tableSnapshot.manifestRecord.relativePath; tableRuntimeAutomationId = $runtimeTableId; tableGridPattern = $tableSnapshot.record.gridPattern; tableColumnCount = [int]$tableEvidence.columnCount; tableHeaders = @($tableEvidence.headers | ForEach-Object { [string]$_.text }); identityCellCount = @($tableEvidence.identityCellChecks).Count; renderedIdentitySamples = @($tableEvidence.renderedIdentitySamples | ForEach-Object { $_.row }); lifecycleModelRowCount = [Int64]$modelMatches[0].rootRows; stableMemoryCheckpoint = $checkpoint; screenshot = if ($null -ne $screenshot) { $screenshot.relativePath } else { $null } }
        $scenario.Add($observation)
        $route.observations.Add($observation)
        Set-P914SamplerContext $Context 'normal-use' 'between-staff-routes'
        $previousPageKey = $pageKey
    }
    $priorArrival = [Int64]$Context.arrival
    Set-P914SamplerContext $Context 'operation' 'staff-return-to-my-workspace'
    $returnEvent = Select-P914Route $Context $Manifest $EvidenceDirectory 'My Workspace' 'my_workspace'
    $null = Wait-P914Lifecycle $Context $EvidenceDirectory 'gs_team' 'suspended' $priorArrival $TimeoutSeconds
    $workspaceTitle = Wait-P914NamedControl $Context $EvidenceDirectory 'main' 'ControlType.Text' 'My Workspace' $TimeoutSeconds
    $workspaceTitleSnapshot = Save-P914UiaSnapshot $Context $Manifest $EvidenceDirectory 'staff-return-my-workspace-page-title' $workspaceTitle
    if ([string]$workspaceTitleSnapshot.record.control.name -cne 'My Workspace' -or [string]$workspaceTitleSnapshot.record.control.controlType -cne 'ControlType.Text' -or [Int64]$workspaceTitleSnapshot.record.control.processId -ne [Int64]$Context.process.Id -or [Int64]$workspaceTitleSnapshot.record.control.ownerWindowProcessId -ne [Int64]$Context.process.Id -or [Int64]$workspaceTitleSnapshot.record.control.ownerWindowHandle -ne [Int64]$Context.mainWindowHandle) { throw 'Runtime UIA My Workspace page-title identity is not measured and exact.' }
    $returnCheckpoint = Add-P914StableCheckpoint $Context $Manifest 'staff-return-to-my-workspace'
    $route.returnObservation = [ordered]@{ targetPageId = [string]$returnEvent.targetPageId; routeEventArrivalIndex = [Int64]$returnEvent.arrivalIndex; workspaceTitle = $workspaceTitleSnapshot.record.control; workspaceTitleSnapshotRelativePath = [string]$workspaceTitleSnapshot.manifestRecord.relativePath; stableMemoryCheckpoint = $returnCheckpoint; pageLifecycleEvidence = 'captured native_english_teachers and gs_team activated/suspended events, including both re-entry activations; no baseline return allowance is defined' }
    $route.staffObservations = $scenario.ToArray()
    Set-P914SamplerContext $Context 'normal-use' 'staff-route-ready-to-close'
}

function Get-P914Grid([System.Windows.Automation.AutomationElement]$Element) {
    try { return [System.Windows.Automation.GridPattern]$Element.GetCurrentPattern([System.Windows.Automation.GridPattern]::Pattern) }
    catch { throw "UIA GridPattern is unavailable for required table $(Get-P914ControlText $Element)." }
}
function Get-P914GridCell([object]$Context, [string]$Container, [string]$TableId, [int]$Row, [int]$Column) {
    $table = Get-P914Control $Context $Container $TableId '' ''
    if ($null -eq $table) { throw "UIA table disappeared before grid read: $TableId" }
    $grid = Get-P914Grid $table
    if ($Row -lt 0 -or $Row -ge [int]$grid.Current.RowCount -or $Column -lt 0 -or $Column -ge [int]$grid.Current.ColumnCount) { throw "Grid coordinate is outside exposed UIA bounds: $Row,$Column" }
    $cell = $grid.GetItem($Row, $Column)
    $ownerKey = Get-P914RuntimeKey $table
    if (-not $Context.ownerHandles.ContainsKey($ownerKey)) { throw 'UIA table has no measured owner-window mapping for its GridPattern cell.' }
    $Context.ownerHandles[(Get-P914RuntimeKey $cell)] = [Int64]$Context.ownerHandles[$ownerKey]
    return [pscustomobject]@{ table = $table; grid = $grid; cell = $cell }
}
function Ensure-P914StaffGridCellVisible([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$TableId, [int]$Row) {
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        $entry = Get-P914GridCell $Context 'main' $TableId $Row 0
        $current = $entry.cell.Current
        $rectangle = $current.BoundingRectangle
        $visible = -not [bool]$current.IsOffscreen -and $rectangle.Width -gt 0 -and $rectangle.Height -gt 0
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { throw "Staff identity cell belongs to a different PID at row $Row." }
        if ($visible) { return $entry }

        $scrollItem = $null
        try { $scrollItem = [System.Windows.Automation.ScrollItemPattern]$entry.cell.GetCurrentPattern([System.Windows.Automation.ScrollItemPattern]::Pattern) } catch [System.InvalidOperationException] { $scrollItem = $null }
        if ($null -ne $scrollItem) {
            $action = [ordered]@{ actionKind = 'scroll-staff-identity-cell-into-view'; method = 'ScrollItemPattern.ScrollIntoView'; row = $Row; column = 0; target = Get-P914ControlSnapshot $Context $entry.cell; issuedAtUtc = Get-P914Utc }
            $Manifest.route.actions.Add($action); Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $action
            $scrollItem.ScrollIntoView()
            Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $action
        } else {
            $scroll = $null
            try { $scroll = [System.Windows.Automation.ScrollPattern]$entry.table.GetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern) } catch [System.InvalidOperationException] { $scroll = $null }
            if ($null -eq $scroll -or -not [bool]$scroll.Current.VerticallyScrollable) { throw "Staff identity row $Row is offscreen and UIA exposes no bounded row or table scroll pattern." }
            $action = [ordered]@{ actionKind = 'scroll-staff-table-to-identity-row'; method = 'ScrollPattern.Scroll'; row = $Row; column = 0; target = Get-P914ControlSnapshot $Context $entry.table; issuedAtUtc = Get-P914Utc }
            $Manifest.route.actions.Add($action); Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $action
            $scroll.Scroll([System.Windows.Automation.ScrollAmount]::NoAmount, [System.Windows.Automation.ScrollAmount]::LargeIncrement)
            Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $action
        }
        Start-Sleep -Milliseconds 100
    }
    throw "Staff identity cell remained offscreen after bounded UIA scrolling at row $Row."
}
function Get-P914StaffTableEvidence([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [System.Windows.Automation.AutomationElement]$Table, [string]$TableId, [string[]]$ExpectedHeaders, [object[]]$ExpectedRows) {
    $grid = Get-P914Grid $Table
    $rowCount = [int]$grid.Current.RowCount
    $columnCount = [int]$grid.Current.ColumnCount
    if ($rowCount -ne 96 -or $columnCount -ne $ExpectedHeaders.Count -or $ExpectedRows.Count -ne 96) { throw "Staff UIA grid dimensions differ from the exact fixture at ${TableId}: rows=$rowCount columns=$columnCount fixtureRows=$($ExpectedRows.Count)." }

    $tablePattern = $null
    try { $tablePattern = [System.Windows.Automation.TablePattern]$Table.GetCurrentPattern([System.Windows.Automation.TablePattern]::Pattern) }
    catch [System.InvalidOperationException] { throw "UIA TablePattern is unavailable for required Staff header verification at $TableId." }
    $headerElements = @($tablePattern.Current.GetColumnHeaders())
    if ($headerElements.Count -ne $columnCount) { throw "UIA TablePattern returned $($headerElements.Count) column headers for $columnCount columns at $TableId." }
    $tableKey = Get-P914RuntimeKey $Table
    if (-not $Context.ownerHandles.ContainsKey($tableKey)) { throw "Staff table has no measured owner HWND mapping at $TableId." }
    $headers = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt $headerElements.Count; $index++) {
        $header = $headerElements[$index]
        $headerCurrent = $header.Current
        if ([uint32]$headerCurrent.ProcessId -ne [uint32]$Context.process.Id) { throw "Staff table column header belongs to a different PID at column $index." }
        $Context.ownerHandles[(Get-P914RuntimeKey $header)] = [Int64]$Context.ownerHandles[$tableKey]
        $text = Get-P914ControlText $header
        if ([string]::IsNullOrWhiteSpace($text)) { throw "UIA Staff column header has no accessible text at column $index." }
        $headers.Add([ordered]@{ column = $index; text = $text; control = Get-P914ControlSnapshot $Context $header })
    }
    $actualHeaders = @($headers | ForEach-Object { [string]$_.text })
    if (($actualHeaders -join "`n") -cne ($ExpectedHeaders -join "`n")) { throw "UIA Staff table headers differ from the exact Next model at ${TableId}: $($actualHeaders -join ', ')" }

    $expectedIndexByName = [Collections.Generic.Dictionary[string, int]]::new([StringComparer]::Ordinal)
    for ($index = 0; $index -lt $ExpectedRows.Count; $index++) {
        $fixtureName = [string]$ExpectedRows[$index].name
        if ([string]::IsNullOrWhiteSpace($fixtureName) -or $expectedIndexByName.ContainsKey($fixtureName)) { throw "Pinned Staff fixture identities are blank or duplicated at ${TableId} fixture row $index." }
        $expectedIndexByName.Add($fixtureName, $index)
    }
    $seenIdentityNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)

    $sampleRows = @(0, 48, 95)
    $renderedSamples = [Collections.Generic.List[object]]::new()
    foreach ($row in $sampleRows) {
        $entry = Ensure-P914StaffGridCellVisible $Context $Manifest $EvidenceDirectory $TableId $row
        $current = $entry.cell.Current
        $text = Get-P914ControlText $entry.cell
        if (-not $expectedIndexByName.ContainsKey($text)) { throw "Rendered Staff identity cell text is not in the pinned fixture at table row $row for ${TableId}: observed '$text'." }
        $rect = $current.BoundingRectangle
        if ([bool]$current.IsOffscreen -or $rect.Width -le 0 -or $rect.Height -le 0) { throw "Rendered Staff identity cell lacks a visible nonempty UIA rectangle at row $row for $TableId." }
        $renderedSamples.Add([ordered]@{ row = $row; fixtureRowIndex = [int]$expectedIndexByName[$text]; expectedIdentity = $text; text = $text; isOffscreen = [bool]$current.IsOffscreen; rectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height }; processId = [Int64]$current.ProcessId; runtimeId = Get-P914RuntimeKey $entry.cell })
    }

    $identityChecks = [Collections.Generic.List[object]]::new()
    $exposedRows = 0
    for ($row = 0; $row -lt $rowCount; $row++) {
        $entry = Get-P914GridCell $Context 'main' $TableId $row 0
        $current = $entry.cell.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { throw "Staff GridPattern identity cell belongs to a different PID at row $row." }
        $text = Get-P914ControlText $entry.cell
        if (-not $expectedIndexByName.ContainsKey($text)) { throw "Staff GridPattern identity is not in the pinned fixture at ${TableId} table row ${row}: observed '$text'." }
        if (-not $seenIdentityNames.Add($text)) { throw "Staff GridPattern exposed a duplicate fixture identity at ${TableId} table row ${row}: '$text'." }
        $fixtureRowIndex = [int]$expectedIndexByName[$text]
        $rect = $current.BoundingRectangle
        $identityChecks.Add([ordered]@{ row = $row; fixtureRowIndex = $fixtureRowIndex; column = 0; expectedIdentity = $text; text = $text; controlType = [string]$current.ControlType.ProgrammaticName; processId = [Int64]$current.ProcessId; automationId = [string]$current.AutomationId; runtimeId = Get-P914RuntimeKey $entry.cell; isOffscreen = [bool]$current.IsOffscreen; rectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height } })
        $exposedRows++
    }
    if ($exposedRows -ne $rowCount -or $seenIdentityNames.Count -ne $expectedIndexByName.Count) { throw "UIA GridPattern did not expose the exact unique 96-name fixture identity set at ${TableId}." }
    [ordered]@{ rowCount = $rowCount; columnCount = $columnCount; headers = $headers.ToArray(); identityCellChecks = $identityChecks.ToArray(); allGridRowsExposed = ($exposedRows -eq $rowCount); fixtureIdentitySetVerified = ($seenIdentityNames.Count -eq $expectedIndexByName.Count); renderedIdentitySamples = $renderedSamples.ToArray(); identityColumn = 0; identityField = 'name'; identityOrder = 'runtime GridPattern row order is retained; exact unique identity set is compared against the pinned fixture'; source = 'UIA GridPattern.GetItem row identity plus UIA TablePattern.GetColumnHeaders' }
}
function Get-P914CellCheckState([System.Windows.Automation.AutomationElement]$Cell) {
    $toggle = $null
    try { $toggle = [System.Windows.Automation.TogglePattern]$Cell.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern) } catch [System.InvalidOperationException] { $toggle = $null }
    if ($null -ne $toggle) { return [ordered]@{ available = $true; state = [string]$toggle.Current.ToggleState; source = 'TogglePattern' } }
    $legacy = $null
    try { $legacy = [System.Windows.Automation.LegacyIAccessiblePattern]$Cell.GetCurrentPattern([System.Windows.Automation.LegacyIAccessiblePattern]::Pattern) } catch [System.InvalidOperationException] { $legacy = $null }
    if ($null -ne $legacy) { $state = [int]$legacy.Current.State; return [ordered]@{ available = $true; state = if (($state -band 0x10) -ne 0) { 'On' } else { 'Off' }; source = 'LegacyIAccessiblePattern.State' } }
    return [ordered]@{ available = $false; state = $null; source = $null }
}
function Get-P914VerifiedCellTarget([object]$Context, [object]$Entry, [int]$Row, [int]$Column) {
    $cell = $Entry.cell; $current = $cell.Current
    if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { throw "UIA checkbox cell PID differs from the measured process at row $Row." }
    $cellSnapshot = Get-P914ControlSnapshot $Context $cell
    $handle = [Int64]$cellSnapshot.ownerWindowHandle
    $measuredProcessId = [uint32]$Context.process.Id
    if ([Int64]$cellSnapshot.ownerWindowProcessId -ne [Int64]$measuredProcessId -or [uint32][P914Native]::GetWindowProcessId([IntPtr]$handle) -ne $measuredProcessId) { throw "UIA checkbox owner HWND is not bound to the measured PID at row $Row." }
    $windowRoot = [System.Windows.Automation.AutomationElement]::FromHandle([IntPtr]$handle)
    if ([string]$windowRoot.Current.AutomationId -cne 'QApplication.speakingEvaluationAiBatchDialog' -or [uint32]$windowRoot.Current.ProcessId -ne $measuredProcessId) { throw 'Checkbox input HWND is not the measured Next AI batch dialog.' }
    $rectangle = $current.BoundingRectangle
    if ([double]::IsInfinity($rectangle.X) -or [double]::IsInfinity($rectangle.Y) -or [double]::IsNaN($rectangle.X) -or [double]::IsNaN($rectangle.Y) -or $rectangle.Width -le 0 -or $rectangle.Height -le 0 -or [bool]$current.IsOffscreen) { throw "UIA review cell has no nonempty visible bounding rectangle at row $Row." }
    $windowBounds = [P914Native]::GetBounds([IntPtr]$handle, $measuredProcessId)
    if ($rectangle.X -lt $windowBounds[0] -or $rectangle.Y -lt $windowBounds[1] -or ($rectangle.X + $rectangle.Width) -gt ($windowBounds[0] + $windowBounds[2]) -or ($rectangle.Y + $rectangle.Height) -gt ($windowBounds[1] + $windowBounds[3])) { throw "UIA review cell bounds leave the measured dialog HWND at row $Row." }
    $x = [int][Math]::Round($rectangle.X + $rectangle.Width / 2)
    $y = [int][Math]::Round($rectangle.Y + $rectangle.Height / 2)
    if ($x -lt $windowBounds[0] -or $x -ge ($windowBounds[0] + $windowBounds[2]) -or $y -lt $windowBounds[1] -or $y -ge ($windowBounds[1] + $windowBounds[3])) { throw "UIA-derived centered indicator point is outside the measured dialog HWND at row $Row." }
    [ordered]@{
        row = $Row; column = $Column; cellIdentity = [ordered]@{ name = [string]$current.Name; automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; processId = [Int64]$current.ProcessId; runtimeId = (Get-P914RuntimeKey $cell) }
        cellControl = $cellSnapshot; cellRectangle = [ordered]@{ x = [double]$rectangle.X; y = [double]$rectangle.Y; width = [double]$rectangle.Width; height = [double]$rectangle.Height; isOffscreen = [bool]$current.IsOffscreen }
        inputPoint = [ordered]@{ x = $x; y = $y; basis = 'center of UIA GridPattern cell; Next custom delegate paints its check indicator at the centered cell position' }
        ownerWindow = [ordered]@{ processId = [Int64]$measuredProcessId; windowHandle = $handle; automationId = [string]$windowRoot.Current.AutomationId; bounds = [ordered]@{ x = [int]$windowBounds[0]; y = [int]$windowBounds[1]; width = [int]$windowBounds[2]; height = [int]$windowBounds[3] } }
    }
}
function Ensure-P914GridCellVisible([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Container, [string]$TableId, [int]$Row, [int]$Column) {
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        $entry = Get-P914GridCell $Context $Container $TableId $Row $Column
        $current = $entry.cell.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { throw "UIA review cell PID differs from the measured process at row $Row." }
        $rect = $current.BoundingRectangle
        $isUsable = -not [bool]$current.IsOffscreen -and $rect.Width -gt 0 -and $rect.Height -gt 0 -and -not [double]::IsInfinity($rect.X) -and -not [double]::IsNaN($rect.X)
        if ($isUsable) { $target = Get-P914VerifiedCellTarget $Context $entry $Row $Column; return [pscustomobject]@{ entry = $entry; target = $target } }
        $scrollItem = $null
        try { $scrollItem = [System.Windows.Automation.ScrollItemPattern]$entry.cell.GetCurrentPattern([System.Windows.Automation.ScrollItemPattern]::Pattern) } catch { $scrollItem = $null }
        if ($null -ne $scrollItem) {
            $target = Get-P914ControlSnapshot $Context $entry.table
            $action = [ordered]@{ actionKind = 'scroll-ai-review-cell-into-view'; method = 'ScrollItemPattern.ScrollIntoView'; row = $Row; column = $Column; cellIdentity = [ordered]@{ automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; processId = [Int64]$current.ProcessId; runtimeId = (Get-P914RuntimeKey $entry.cell) }; target = $target; issuedAtUtc = Get-P914Utc }
            $Manifest.route.actions.Add($action); Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $action
            $scrollItem.ScrollIntoView()
        } else {
            $scrollPattern = $null
            try { $scrollPattern = [System.Windows.Automation.ScrollPattern]$entry.table.GetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern) } catch { $scrollPattern = $null }
            if ($null -eq $scrollPattern -or -not [bool]$scrollPattern.Current.VerticallyScrollable) { throw "UIA exposes neither a ScrollItemPattern nor a vertically scrollable table for review row $Row." }
            $action = [ordered]@{ actionKind = 'scroll-ai-review-table'; method = 'ScrollPattern.Scroll'; row = $Row; column = $Column; target = Get-P914ControlSnapshot $Context $entry.table; targetProcessId = [Int64]$Context.process.Id; issuedAtUtc = Get-P914Utc }
            $Manifest.route.actions.Add($action); Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $action
            $scrollPattern.Scroll([System.Windows.Automation.ScrollAmount]::NoAmount, [System.Windows.Automation.ScrollAmount]::LargeIncrement)
        }
        Start-Sleep -Milliseconds 75
    }
    throw "UIA review cell remained offscreen after bounded UIA scroll attempts at row $Row."
}
function Invoke-P914GridCheckbox([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$ActionKind, [string]$Container, [string]$TableId, [int]$Row, [int]$Column, [string]$ExpectedState) {
    $visibleTarget = Ensure-P914GridCellVisible $Context $Manifest $EvidenceDirectory $Container $TableId $Row $Column
    $entry = $visibleTarget.entry
    $stateBefore = Get-P914CellCheckState $entry.cell
    if (-not [bool]$stateBefore.available) { throw "UIA did not expose a check state for review row $Row; refusing an unverifiable toggle." }
    if ([string]$stateBefore.state -cne 'Off') { throw "Review row $Row did not start unchecked (Off)." }
    if ([string]$stateBefore.state -ceq $ExpectedState) { throw "Review row $Row already has unexpected check state $ExpectedState." }
    # Reacquire the row and its hit target immediately before performing the action.
    $entry = (Get-P914GridCell $Context $Container $TableId $Row $Column)
    $targetBefore = Get-P914VerifiedCellTarget $Context $entry $Row $Column
    $stateBefore = Get-P914CellCheckState $entry.cell
    if (-not [bool]$stateBefore.available -or [string]$stateBefore.state -cne 'Off') { throw "Review row $Row state changed before input; expected an observable unchecked value." }
    $toggle = $null
    try { $toggle = [System.Windows.Automation.TogglePattern]$entry.cell.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern) } catch [System.InvalidOperationException] { $toggle = $null }
    $method = ''
    $toggleReturnedWithoutTransition = $false
    $pointerAction = $null
    $stateAfter = $null
    $targetAfter = $null
    if ($null -ne $toggle) {
        $method = 'TogglePattern.Toggle'
        # A thrown Toggle() is intentionally allowed to fail closed. Only a returned
        # pattern call that leaves the measured state unchanged may proceed to the
        # separately verified, UIA-derived cell hit target.
        $toggle.Toggle()
        $toggleWait = [Diagnostics.Stopwatch]::StartNew()
        while ($toggleWait.Elapsed.TotalSeconds -lt 1.0) {
            $after = Get-P914GridCell $Context $Container $TableId $Row $Column
            $stateAfter = Get-P914CellCheckState $after.cell
            if (-not [bool]$stateAfter.available) { throw "TogglePattern.Toggle returned but UIA check state became unavailable for review row $Row." }
            if ([string]$stateAfter.state -ceq $ExpectedState) { $targetAfter = Get-P914VerifiedCellTarget $Context $after $Row $Column; break }
            if ([string]$stateAfter.state -cne [string]$stateBefore.state) { throw "TogglePattern.Toggle returned an unexpected state for review row $Row; refusing a second input." }
            Start-Sleep -Milliseconds 100
        }
        if ($null -eq $targetAfter) {
            $toggleReturnedWithoutTransition = $true
            $method = 'UIA-derived cell BoundingRectangle bounded WM mouse click after TogglePattern produced no state transition'
            # Bounds, visibility, PID, HWND, identity, and input point are reacquired
            # after the pattern call and immediately before the one fallback click.
            $entry = Get-P914GridCell $Context $Container $TableId $Row $Column
            $fallbackState = Get-P914CellCheckState $entry.cell
            if (-not [bool]$fallbackState.available -or [string]$fallbackState.state -cne [string]$stateBefore.state) { throw "TogglePattern.Toggle returned without a transition but review row $Row changed before the bounded click fallback." }
            $targetBefore = Get-P914VerifiedCellTarget $Context $entry $Row $Column
        }
    } else {
        $method = 'UIA-derived cell BoundingRectangle bounded WM mouse click'
    }
    if ($null -eq $targetAfter) {
        $handle = [Int64]$targetBefore.ownerWindow.windowHandle
        $x = [int]$targetBefore.inputPoint.x; $y = [int]$targetBefore.inputPoint.y
        $windowBounds = $targetBefore.ownerWindow.bounds
        $pointerAction = [ordered]@{ actionType = 'WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP'; screenX = $x; screenY = $y; sourceTarget = $targetBefore.cellIdentity; sourceRectangle = $targetBefore.cellRectangle; targetProcessId = [Int64]$Context.process.Id; windowHandle = $handle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$handle); measuredDialogBounds = $windowBounds }
        if ([Int64]$pointerAction.windowProcessId -ne [Int64]$Context.process.Id) { throw "Review row $Row fallback HWND owner changed before click." }
        [P914Native]::ClickScreenPoint([IntPtr]$handle, [uint32]$Context.process.Id, $x, $y)
        $clickWait = [Diagnostics.Stopwatch]::StartNew()
        while ($clickWait.Elapsed.TotalSeconds -lt 2.0) {
            $after = Get-P914GridCell $Context $Container $TableId $Row $Column
            $stateAfter = Get-P914CellCheckState $after.cell
            if ([bool]$stateAfter.available -and [string]$stateAfter.state -ceq $ExpectedState) { $targetAfter = Get-P914VerifiedCellTarget $Context $after $Row $Column; break }
            if ([bool]$stateAfter.available -and [string]$stateAfter.state -cne [string]$stateBefore.state) { throw "UIA-derived click produced an unexpected check state for review row $Row." }
            Start-Sleep -Milliseconds 100
        }
    }
    if (-not [bool]$stateAfter.available -or [string]$stateAfter.state -cne $ExpectedState -or $null -eq $targetAfter) { throw "UIA review row $Row did not expose the expected post-toggle state $ExpectedState." }
    if ($null -eq $pointerAction) { $targetAfter = Get-P914VerifiedCellTarget $Context (Get-P914GridCell $Context $Container $TableId $Row $Column) $Row $Column }
    $action = [ordered]@{ actionKind = $ActionKind; method = $method; row = $Row; column = $Column; stateBefore = $stateBefore; stateAfter = $stateAfter; target = $targetAfter.cellControl; cellTargetBefore = $targetBefore; cellTargetAfter = $targetAfter; togglePatternAvailable = ($null -ne $toggle); toggleReturned = ($null -ne $toggle); toggleReturnedWithoutTransition = $toggleReturnedWithoutTransition; pointerAction = $pointerAction; issuedAtUtc = Get-P914Utc; completedAtUtc = Get-P914Utc }
    $Manifest.route.actions.Add($action); Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $action
    return $action
}
function Invoke-P914AiWorkflow([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$WorkspacePreparationReceiptPath, [int]$TimeoutSeconds) {
    $route = $Manifest.route
    $initial = @($Context.routeEvents | Where-Object { [string]$_.targetPageId -ceq 'my_workspace' } | Select-Object -Last 1)
    if ($initial.Count -ne 1) { throw 'Initial My Workspace route event was not captured.' }
    $priorArrival = [Int64]$Context.arrival
    Set-P914SamplerContext $Context 'operation' 'ai-speaking-evaluation-route-activation'
    $speakingRoute = Select-P914Route $Context $Manifest $EvidenceDirectory 'Speaking Evaluation' 'speaking_evaluation'
    $activation = Wait-P914Lifecycle $Context $EvidenceDirectory 'speaking_evaluation' 'activated' $priorArrival $TimeoutSeconds
    $title = Wait-P914Control $Context $EvidenceDirectory 'main' 'QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationTitle' 'ControlType.Text' 'Speaking Evaluation' $TimeoutSeconds
    $classListId = 'QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationContentSplitter.speakingEvaluationClassList'
    $classItem = Find-P914NamedControl $Context 'main' 'ControlType.ListItem' 'Heavy Fixture Class 001' $classListId
    if ($null -eq $classItem) { throw 'Speaking Evaluation class list did not expose the exact fixture class row.' }
    $classAction = Add-P914Action $Context $Manifest $EvidenceDirectory 'select-ai-fixture-class' $classItem 'SelectionItemPattern.Select'
    $selection = [System.Windows.Automation.SelectionItemPattern]$classItem.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)
    $selection.Select(); $classAction.completedAtUtc = Get-P914Utc
    $matrixId = 'QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationContentSplitter.speakingEvaluationMatrix'
    $matrix = Wait-P914Table $Context $EvidenceDirectory 'main' $matrixId $TimeoutSeconds
    $matrixSnapshot = Save-P914UiaSnapshot $Context $Manifest $EvidenceDirectory 'speaking-evaluation-100-row-matrix' $matrix
    $activationModels = Get-P914PageModelCounts $activation
    $matrixModels = @($activationModels.itemModels | Where-Object { [string]$_.className -match 'TableModel' -and [Int64]$_.rootRows -eq 100 })
    if (-not [bool]$activationModels.sampleAvailable -or $matrixModels.Count -ne 1) { throw 'AI route activation did not expose exactly one 100-row speaking-evaluation model in packaged lifecycle diagnostics.' }
    if (-not [bool]$matrixSnapshot.record.gridPattern.available -or [int]$matrixSnapshot.record.gridPattern.rowCount -ne 100) { throw 'UIA GridPattern matrix row count differs from the isolated 100-row setting.' }
    $mainCheckpoint = Add-P914StableCheckpoint $Context $Manifest 'speaking-evaluation-100-row-matrix'
    $route.aiMatrix = [ordered]@{ routeEventArrivalIndex = [Int64]$speakingRoute.arrivalIndex; title = Get-P914ControlSnapshot $Context $title; matrixSnapshotRelativePath = [string]$matrixSnapshot.manifestRecord.relativePath; modelCounts = $activationModels; modelRowCount = [Int64]$matrixModels[0].rootRows; isolatedMaximumRosterRows = [int]$Manifest.settings.maximumRosterRows; stableMemoryCheckpoint = $mainCheckpoint }

    $batchButtonId = 'QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationAiBatchComments'
    $batchButton = Wait-P914Control $Context $EvidenceDirectory 'main' $batchButtonId 'ControlType.Button' '' $TimeoutSeconds
    Set-P914SamplerContext $Context 'operation' 'open-ai-batch-review-dialog'
    Invoke-P914Button $Context $Manifest $EvidenceDirectory 'open-ai-batch-dialog' $batchButton
    $dialogId = 'QApplication.speakingEvaluationAiBatchDialog'
    $dialogWait = [Diagnostics.Stopwatch]::StartNew(); $dialog = $null
    while ($dialogWait.Elapsed.TotalSeconds -lt $TimeoutSeconds) { Drain-P914Output $Context $EvidenceDirectory; $dialog = Get-P914OwnerRoot $Context $dialogId; if ($null -ne $dialog) { break }; Start-Sleep -Milliseconds 100 }
    if ($null -eq $dialog) { throw 'AI batch review dialog did not appear as a PID-bound top-level window.' }
    $dialogCurrent = $dialog.Current
    if ([string]$dialogCurrent.AutomationId -cne $dialogId -or [uint32]$dialogCurrent.ProcessId -ne [uint32]$Context.process.Id) { throw 'Active AI modal root is not the current Next speakingEvaluationAiBatchDialog.' }
    $visibleWindows = @(Get-P914VisibleTopLevelWindows $Context)
    $visibleAiDialogs = [Collections.Generic.List[object]]::new(); $visibleLegacyDialogs = [Collections.Generic.List[object]]::new()
    foreach ($window in $visibleWindows) {
        if ([string]$window.automationId -ceq 'QApplication.speakingEvaluationAiBatchDialog') { $visibleAiDialogs.Add($window) }
        if ([string]$window.automationId -ceq 'QApplication.speakingEvalAiBatchDialog') { $visibleLegacyDialogs.Add($window) }
    }
    if ($visibleAiDialogs.Count -ne 1 -or $visibleLegacyDialogs.Count -ne 0) { throw "Visible app-owned dialog inventory does not uniquely prove Next dialog use: next=$($visibleAiDialogs.Count), legacy=$($visibleLegacyDialogs.Count)." }
    $dialogSnapshot = Save-P914UiaSnapshot $Context $Manifest $EvidenceDirectory 'next-ai-batch-dialog-root' $dialog
    $dialogDescendants = Save-P914UiaDescendantSnapshot $Context $Manifest $EvidenceDirectory 'next-ai-batch-dialog-descendants' $dialog
    $lifecyclePageKeys = @($Context.lifecycleEvents | ForEach-Object { [string]$_.record.page_key })
    if ($lifecyclePageKeys -contains 'lifecycle-speaking-evaluation') { throw 'Forbidden Legacy lifecycle-speaking-evaluation event was observed in the Next UI route.' }
    $route.aiImplementation = [ordered]@{ routePageKey = 'speaking_evaluation'; routeMode = [string]$Manifest.routeMode; applicationName = 'ClassMngrNext.exe'; dialogRootAutomationId = [string]$dialogCurrent.AutomationId; dialogRootClassName = [string]$dialogCurrent.ClassName; dialogRootProcessId = [Int64]$dialogCurrent.ProcessId; dialogRootWindowHandle = [Int64]$dialogCurrent.NativeWindowHandle; dialogRootSnapshotRelativePath = [string]$dialogSnapshot.manifestRecord.relativePath; dialogDescendantSnapshotRelativePath = [string]$dialogDescendants.manifestRecord.relativePath; visibleAppOwnedTopLevelWindows = $visibleWindows; visibleNextDialogCount = $visibleAiDialogs.Count; visibleLegacyDialogCount = $visibleLegacyDialogs.Count; implementationProof = $Manifest.packageProvenance.nextImplementationProof; lifecyclePageKeys = $lifecyclePageKeys }
    $selectionTableId = 'QApplication.speakingEvaluationAiBatchDialog.speakingEvaluationAiBatchTabs.qt_tabwidget_stackedwidget.QWidget.speakingEvaluationAiBatchSelectionTable'
    $selectionTable = Get-P914Table $Context $dialogId $selectionTableId
    if ($null -eq $selectionTable) { throw 'AI candidate selection table was not exposed by the modal UIA tree.' }
    $selectionGrid = Get-P914GridMetrics $selectionTable
    $statusId = 'QApplication.speakingEvaluationAiBatchDialog.speakingEvaluationAiBatchTabs.qt_tabwidget_stackedwidget.QWidget.speakingEvaluationAiBatchStatus'
    $batchStatus = Get-P914Control $Context $dialogId $statusId 'ControlType.Text' ''
    if ($null -eq $batchStatus) { throw 'AI candidate status label was not exposed to UIA.' }
    $statusText = Get-P914ControlText $batchStatus
    if ($statusText -notmatch '^25 of 100 students selected\.' -or -not [bool]$selectionGrid.available -or [int]$selectionGrid.rowCount -ne 100) { throw "AI dialog did not prove 25 selected of 100 UI candidates: status='$statusText', grid=$($selectionGrid | ConvertTo-Json -Compress)." }
    $selectionSnapshot = Save-P914UiaSnapshot $Context $Manifest $EvidenceDirectory 'ai-candidate-selection-table' $selectionTable
    $route.aiCandidates = [ordered]@{ candidateCount = 100; selectedForPrompt = 25; remainingCandidates = 75; statusLabel = $statusText; selectionTableSnapshotRelativePath = [string]$selectionSnapshot.manifestRecord.relativePath; gridPattern = $selectionGrid; selectionBoundaryEvidence = 'UIA status text and dialog table; maximum 25 is also asserted by source-backed fixture contract' }
    Add-P914StableCheckpoint $Context $Manifest 'ai-batch-100-candidates-25-selected'

    $createPromptId = 'QApplication.speakingEvaluationAiBatchDialog.speakingEvaluationAiBatchTabs.qt_tabwidget_stackedwidget.QWidget.speakingEvaluationAiBatchCreatePrompt'
    $createButton = Wait-P914Control $Context $EvidenceDirectory $dialogId $createPromptId 'ControlType.Button' '' $TimeoutSeconds
    $copyPromptId = 'QApplication.speakingEvaluationAiBatchDialog.speakingEvaluationAiBatchTabs.qt_tabwidget_stackedwidget.QWidget.speakingEvaluationAiBatchCopyPrompt'
    $copyPromptBefore = Get-P914Control $Context $dialogId $copyPromptId 'ControlType.Button' ''
    if ($null -eq $copyPromptBefore) { throw 'UIA did not expose Copy Prompt before generating the local class prompt.' }
    $copyPromptBeforeSnapshot = Get-P914ControlSnapshot $Context $copyPromptBefore
    if ([bool]$copyPromptBeforeSnapshot.isEnabled) { throw 'Copy Prompt was already enabled before Create Class Prompt; prompt-generation state is ambiguous.' }
    Set-P914SamplerContext $Context 'operation' 'create-local-ai-batch-prompt'
    Invoke-P914Button $Context $Manifest $EvidenceDirectory 'create-class-prompt' $createButton
    $promptWait = [Diagnostics.Stopwatch]::StartNew(); $copyPromptAfter = $null; $copyPromptAfterSnapshot = $null
    while ($promptWait.Elapsed.TotalSeconds -lt 1.5) {
        Drain-P914Output $Context $EvidenceDirectory
        $copyPromptAfter = Get-P914Control $Context $dialogId $copyPromptId 'ControlType.Button' ''
        if ($null -ne $copyPromptAfter) {
            $copyPromptAfterSnapshot = Get-P914ControlSnapshot $Context $copyPromptAfter
            if ([bool]$copyPromptAfterSnapshot.isEnabled) { break }
        }
        Start-Sleep -Milliseconds 100
    }
    $promptClickAction = $null
    $creationMethod = 'InvokePattern.Invoke verified by the Copy Prompt enabled-state transition'
    if ($null -eq $copyPromptAfterSnapshot -or -not [bool]$copyPromptAfterSnapshot.isEnabled) {
        # Qt exposed InvokePattern but it did not dispatch this particular button
        # activation. Reacquire the exact button's UIA bounds and deliver one
        # PID/HWND-bound center click only after the disabled state persisted.
        $clickButton = Get-P914Control $Context $dialogId $createPromptId 'ControlType.Button' ''
        if ($null -eq $clickButton) { throw 'Create Class Prompt disappeared before the bounded UIA click fallback.' }
        $clickCurrent = $clickButton.Current
        if ([uint32]$clickCurrent.ProcessId -ne [uint32]$Context.process.Id -or -not [bool]$clickCurrent.IsEnabled -or [bool]$clickCurrent.IsOffscreen) { throw 'Create Class Prompt fallback target failed measured PID/enabled/visibility checks.' }
        $clickSnapshot = Get-P914ControlSnapshot $Context $clickButton
        $dialogHandle = [Int64]$Context.ownerHandles[(Get-P914RuntimeKey $dialog)]
        $measuredProcessId = [uint32]$Context.process.Id
        if ($dialogHandle -le 0 -or [Int64]$clickSnapshot.ownerWindowHandle -ne $dialogHandle -or [Int64]$clickSnapshot.ownerWindowProcessId -ne [Int64]$measuredProcessId -or [uint32][P914Native]::GetWindowProcessId([IntPtr]$dialogHandle) -ne $measuredProcessId -or [Int64]$dialogCurrent.NativeWindowHandle -ne $dialogHandle) { throw 'Create Class Prompt fallback UIA target is not bound to the measured Next AI dialog HWND.' }
        $clickRect = $clickCurrent.BoundingRectangle
        if ([double]::IsInfinity($clickRect.X) -or [double]::IsInfinity($clickRect.Y) -or [double]::IsNaN($clickRect.X) -or [double]::IsNaN($clickRect.Y) -or $clickRect.Width -le 0 -or $clickRect.Height -le 0) { throw 'Create Class Prompt fallback target has an empty UIA BoundingRectangle.' }
        $dialogBounds = [P914Native]::GetBounds([IntPtr]$dialogHandle, $measuredProcessId)
        if ($clickRect.X -lt $dialogBounds[0] -or $clickRect.Y -lt $dialogBounds[1] -or ($clickRect.X + $clickRect.Width) -gt ($dialogBounds[0] + $dialogBounds[2]) -or ($clickRect.Y + $clickRect.Height) -gt ($dialogBounds[1] + $dialogBounds[3])) { throw 'Create Class Prompt fallback UIA bounds leave the measured Next AI dialog HWND.' }
        $clickX = [int][Math]::Round($clickRect.X + $clickRect.Width / 2); $clickY = [int][Math]::Round($clickRect.Y + $clickRect.Height / 2)
        if ($clickX -lt $dialogBounds[0] -or $clickX -ge ($dialogBounds[0] + $dialogBounds[2]) -or $clickY -lt $dialogBounds[1] -or $clickY -ge ($dialogBounds[1] + $dialogBounds[3])) { throw 'Create Class Prompt fallback derived point leaves the measured Next AI dialog HWND.' }
        $promptClickAction = [ordered]@{ actionKind = 'create-class-prompt-uia-bounded-click-fallback'; method = 'UIA-derived Button BoundingRectangle bounded WM mouse click'; target = $clickSnapshot; pointerAction = [ordered]@{ actionType = 'WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP'; screenX = $clickX; screenY = $clickY; targetProcessId = [Int64]$measuredProcessId; windowHandle = $dialogHandle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$dialogHandle); sourceTarget = [ordered]@{ name = [string]$clickCurrent.Name; automationId = [string]$clickCurrent.AutomationId; controlType = [string]$clickCurrent.ControlType.ProgrammaticName; rectangle = [ordered]@{ x = [double]$clickRect.X; y = [double]$clickRect.Y; width = [double]$clickRect.Width; height = [double]$clickRect.Height } }; measuredDialogBounds = [ordered]@{ x = [Int64]$dialogBounds[0]; y = [Int64]$dialogBounds[1]; width = [Int64]$dialogBounds[2]; height = [Int64]$dialogBounds[3] } }; issuedAtUtc = Get-P914Utc; observedNoTransitionAfterInvokeSeconds = [Math]::Round($promptWait.Elapsed.TotalSeconds, 3) }
        [P914Native]::ClickScreenPoint([IntPtr]$dialogHandle, $measuredProcessId, $clickX, $clickY)
        $creationMethod = 'UIA-derived bounded click after InvokePattern produced no verified state transition'
        $clickWait = [Diagnostics.Stopwatch]::StartNew()
        while ($clickWait.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
            Drain-P914Output $Context $EvidenceDirectory
            $copyPromptAfter = Get-P914Control $Context $dialogId $copyPromptId 'ControlType.Button' ''
            if ($null -ne $copyPromptAfter) {
                $copyPromptAfterSnapshot = Get-P914ControlSnapshot $Context $copyPromptAfter
                if ([bool]$copyPromptAfterSnapshot.isEnabled) { break }
            }
            Start-Sleep -Milliseconds 100
        }
    }
    $promptScreenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'ai-class-prompt-created' $dialog
    $promptStatusId = 'QApplication.speakingEvaluationAiBatchDialog.speakingEvaluationAiBatchTabs.qt_tabwidget_stackedwidget.QWidget.speakingEvaluationAiBatchStatus'
    $promptStatus = Get-P914Control $Context $dialogId $promptStatusId 'ControlType.Text' ''
    $promptStatusText = if ($null -ne $promptStatus) { Get-P914ControlText $promptStatus } else { '' }
    if ($null -eq $copyPromptAfterSnapshot -or -not [bool]$copyPromptAfterSnapshot.isEnabled) { throw "Create Class Prompt did not produce a verified Copy Prompt state transition after UIA activation and bounded click; UIA status='$promptStatusText'." }
    if ($null -ne $promptClickAction) { $promptClickAction.stateBefore = [ordered]@{ copyPromptEnabled = $false }; $promptClickAction.stateAfter = [ordered]@{ copyPromptEnabled = [bool]$copyPromptAfterSnapshot.isEnabled }; $promptClickAction.verifiedByFollowingUiAStateTransition = $true; $promptClickAction.completedAtUtc = Get-P914Utc; $route.actions.Add($promptClickAction); Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $promptClickAction }
    $route.aiPrompt = [ordered]@{ creationActionKind = 'create-class-prompt'; creationMethod = $creationMethod; fallbackActionKind = if ($null -ne $promptClickAction) { [string]$promptClickAction.actionKind } else { $null }; responseInputRequiresLocalPrompt = $true; generatedPromptTextUiAExposed = $false; copyPromptButtonBefore = $copyPromptBeforeSnapshot; copyPromptButtonAfter = $copyPromptAfterSnapshot; screenshotRelativePath = [string]$promptScreenshot.relativePath; providerUsed = $false; clipboardUsed = $false; promptGenerationEvidence = 'Create Class Prompt UIA activation followed by Copy Prompt changing from disabled to enabled; screenshot captures the local generated prompt state' }
    $response = [Text.StringBuilder]::new()
    for ($index = 1; $index -le 25; $index++) {
        $id = 'STUDENT_{0:d2}' -f $index; $name = Get-P914AiCandidateName $index
        $body = "P9-14 review for ${name}: your speaking shows a clear central idea and steady organization. Next, add one precise supporting detail and explain how it connects to your main point. This will help your listener follow the reasoning and understand why the example matters."
        if ($body.Length -lt 100 -or $body.Length -gt 420) { throw "Deterministic response body length is outside 100 to 420 characters for ${id}." }
        [void]$response.Append("<<<$id>>>`n$body`n<<<END_$id>>>`n")
    }
    $responseText = $response.ToString().TrimEnd("`n")
    $responsePath = Join-Path (Join-Path $EvidenceDirectory 'output') 'deterministic-ai-response.txt'
    [IO.File]::WriteAllText($responsePath, $responseText + "`n", [Text.UTF8Encoding]::new($false))
    $responseId = 'QApplication.speakingEvaluationAiBatchDialog.speakingEvaluationAiBatchTabs.qt_tabwidget_stackedwidget.QWidget.speakingEvaluationAiBatchResponse'
    $tabs = Find-P914NamedControl $Context $dialogId 'ControlType.TabItem' '2. Paste and Review'
    if ($null -eq $tabs) { throw 'UIA did not expose the exact Paste and Review tab.' }
    $tabAction = Add-P914Action $Context $Manifest $EvidenceDirectory 'open-ai-review-tab' $tabs 'SelectionItemPattern.Select'
    $tabSelection = [System.Windows.Automation.SelectionItemPattern]$tabs.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern); $tabSelection.Select(); $tabAction.completedAtUtc = Get-P914Utc
    $responseEditor = Wait-P914Control $Context $EvidenceDirectory $dialogId $responseId 'ControlType.Edit' '' $TimeoutSeconds
    $responseSnapshot = Get-P914ControlSnapshot $Context $responseEditor
    if (-not $responseSnapshot.isEnabled -or $responseSnapshot.isOffscreen) { throw 'Response edit control is not enabled and visible after opening the review tab.' }
    $responseValue = [System.Windows.Automation.ValuePattern]$responseEditor.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $responseValue.SetValue($responseText)
    if ((Get-P914EditValue $responseEditor) -cne $responseText) { throw 'UIA ValuePattern did not retain the locally supplied deterministic response.' }
    $responseAction = [ordered]@{ actionKind = 'enter-deterministic-response-locally'; method = 'ValuePattern.SetValue'; valueSha256 = Get-P914FileSha256 $responsePath; valueCharacterCount = $responseText.Length; blockCount = 25; providerUsed = $false; clipboardUsed = $false; target = Get-P914ControlSnapshot $Context $responseEditor; issuedAtUtc = Get-P914Utc; verifiedAtUtc = Get-P914Utc }
    $route.actions.Add($responseAction); Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $responseAction
    $parseButtonId = 'QApplication.speakingEvaluationAiBatchDialog.speakingEvaluationAiBatchTabs.qt_tabwidget_stackedwidget.QWidget.speakingEvaluationAiBatchParse'
    $parseButton = Wait-P914Control $Context $EvidenceDirectory $dialogId $parseButtonId 'ControlType.Button' '' $TimeoutSeconds
    Set-P914SamplerContext $Context 'operation' 'parse-ai-batch-response'
    Invoke-P914Button $Context $Manifest $EvidenceDirectory 'parse-ai-response' $parseButton
    $parseSummaryId = 'QApplication.speakingEvaluationAiBatchDialog.speakingEvaluationAiBatchTabs.qt_tabwidget_stackedwidget.QWidget.speakingEvaluationAiBatchParseSummary'
    $parseSummary = Wait-P914Control $Context $EvidenceDirectory $dialogId $parseSummaryId 'ControlType.Text' '' $TimeoutSeconds
    $expectedParseSummary = '^25 of 25 selected students were parsed\.'
    $summaryText = Get-P914ControlText $parseSummary
    $parseInvokeWait = [Diagnostics.Stopwatch]::StartNew()
    while ($parseInvokeWait.Elapsed.TotalSeconds -lt 1.5 -and $summaryText -notmatch $expectedParseSummary) {
        Drain-P914Output $Context $EvidenceDirectory
        Start-Sleep -Milliseconds 100
        $parseSummary = Get-P914Control $Context $dialogId $parseSummaryId 'ControlType.Text' ''
        if ($null -ne $parseSummary) { $summaryText = Get-P914ControlText $parseSummary }
    }
    $parseClickAction = $null
    if ($summaryText -notmatch $expectedParseSummary) {
        $clickButton = Get-P914Control $Context $dialogId $parseButtonId 'ControlType.Button' ''
        if ($null -eq $clickButton) { throw 'Parse Response button disappeared before the verified UIA-bound click fallback.' }
        $clickCurrent = $clickButton.Current
        if ([uint32]$clickCurrent.ProcessId -ne [uint32]$measuredProcessId -or -not [bool]$clickCurrent.IsEnabled -or [bool]$clickCurrent.IsOffscreen) { throw 'Parse Response click fallback target failed measured PID, enabled, or visibility validation.' }
        $clickRect = $clickCurrent.BoundingRectangle
        if ($clickRect.IsEmpty -or $clickRect.Width -le 0 -or $clickRect.Height -le 0) { throw 'Parse Response click fallback target has empty UIA bounds.' }
        $dialogHandle = [Int64]$Context.ownerHandles[$dialogId]
        $dialogWindowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$dialogHandle)
        if ($dialogWindowProcessId -ne [Int64]$measuredProcessId) { throw 'Parse Response click fallback dialog HWND does not belong to the measured process.' }
        $dialogBounds = [P914Native]::GetWindowBounds([IntPtr]$dialogHandle)
        if ($dialogBounds.Count -ne 4 -or $dialogBounds[2] -le 0 -or $dialogBounds[3] -le 0) { throw 'Parse Response click fallback dialog HWND has invalid measured bounds.' }
        if ($clickRect.X -lt $dialogBounds[0] -or $clickRect.Y -lt $dialogBounds[1] -or ($clickRect.X + $clickRect.Width) -gt ($dialogBounds[0] + $dialogBounds[2]) -or ($clickRect.Y + $clickRect.Height) -gt ($dialogBounds[1] + $dialogBounds[3])) { throw 'Parse Response button UIA bounds are not wholly contained in the measured dialog HWND.' }
        $clickX = [int][Math]::Floor($clickRect.X + ($clickRect.Width / 2.0)); $clickY = [int][Math]::Floor($clickRect.Y + ($clickRect.Height / 2.0))
        if ($clickX -lt $dialogBounds[0] -or $clickX -ge ($dialogBounds[0] + $dialogBounds[2]) -or $clickY -lt $dialogBounds[1] -or $clickY -ge ($dialogBounds[1] + $dialogBounds[3])) { throw 'Parse Response fallback derived point leaves the measured Next AI dialog HWND.' }
        $parseClickAction = [ordered]@{ actionKind = 'parse-ai-response-uia-bounded-click-fallback'; method = 'UIA-derived Button BoundingRectangle bounded WM mouse click'; target = Get-P914ControlSnapshot $Context $clickButton; pointerAction = [ordered]@{ actionType = 'WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP'; screenX = $clickX; screenY = $clickY; sourceTarget = [ordered]@{ name = [string]$clickCurrent.Name; automationId = [string]$clickCurrent.AutomationId; controlType = [string]$clickCurrent.ControlType.ProgrammaticName; rectangle = [ordered]@{ x = [double]$clickRect.X; y = [double]$clickRect.Y; width = [double]$clickRect.Width; height = [double]$clickRect.Height } }; targetProcessId = [Int64]$measuredProcessId; windowHandle = $dialogHandle; windowProcessId = $dialogWindowProcessId; measuredDialogBounds = [ordered]@{ x = [Int64]$dialogBounds[0]; y = [Int64]$dialogBounds[1]; width = [Int64]$dialogBounds[2]; height = [Int64]$dialogBounds[3] } }; issuedAtUtc = Get-P914Utc; observedNoTransitionAfterInvokeSeconds = [Math]::Round($parseInvokeWait.Elapsed.TotalSeconds, 3) }
        [P914Native]::ClickScreenPoint([IntPtr]$dialogHandle, $measuredProcessId, $clickX, $clickY)
        $parseClickWait = [Diagnostics.Stopwatch]::StartNew()
        while ($parseClickWait.Elapsed.TotalSeconds -lt $TimeoutSeconds -and $summaryText -notmatch $expectedParseSummary) {
            Drain-P914Output $Context $EvidenceDirectory
            Start-Sleep -Milliseconds 100
            $parseSummary = Get-P914Control $Context $dialogId $parseSummaryId 'ControlType.Text' ''
            if ($null -ne $parseSummary) { $summaryText = Get-P914ControlText $parseSummary }
        }
    }
    if ($null -ne $parseClickAction) {
        $parseClickAction.stateBefore = [ordered]@{ parseSummary = '' }
        $parseClickAction.stateAfter = [ordered]@{ parseSummary = $summaryText }
        $parseClickAction.verifiedByFollowingUiAStateTransition = ($summaryText -match $expectedParseSummary)
        $parseClickAction.completedAtUtc = Get-P914Utc
        $route.actions.Add($parseClickAction)
        Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $parseClickAction
    }
    if ($summaryText -notmatch '^25 of 25 selected students were parsed\.') { throw "AI parse summary does not prove 25/25 valid response blocks: '$summaryText'." }
    $reviewTableId = 'QApplication.speakingEvaluationAiBatchDialog.speakingEvaluationAiBatchTabs.qt_tabwidget_stackedwidget.QWidget.speakingEvaluationAiBatchReviewTable'
    $reviewTable = Get-P914Table $Context $dialogId $reviewTableId
    if ($null -eq $reviewTable) { throw 'AI review table is not exposed by the modal UIA tree.' }
    $reviewGrid = Get-P914Grid $reviewTable
    if ([int]$reviewGrid.Current.RowCount -ne 25 -or [int]$reviewGrid.Current.ColumnCount -lt 5) { throw "UIA review GridPattern shape is not exactly 25 rows by at least 5 columns: $($reviewGrid.Current.RowCount)x$($reviewGrid.Current.ColumnCount)." }
    $reviewRows = [Collections.Generic.List[object]]::new()
    for ($row = 0; $row -lt 25; $row++) {
        $cells = @(); for ($column = 0; $column -lt 5; $column++) { $entry = Get-P914GridCell $Context $dialogId $reviewTableId $row $column; $cells += ,$entry.cell }
        $applyState = Get-P914CellCheckState $cells[0]
        if (-not [bool]$applyState.available -or [string]$applyState.state -cne 'Off') { throw "Review checkbox row $row was not exposed in its expected unchecked state Off." }
        $studentName = Get-P914ControlText $cells[1]; $status = Get-P914ControlText $cells[2]; $characterCountText = Get-P914ControlText $cells[3]; $commentText = Get-P914ControlText $cells[4]
        $expectedName = Get-P914AiCandidateName ($row + 1)
        $expectedComment = Get-P914DeterministicComment ($row + 1)
        if ($studentName -cne $expectedName -or $status -notmatch 'Ready|Valid|Good' -or [int]$characterCountText -ne $expectedComment.Length -or $commentText -cne $expectedComment) { throw "Parsed review row $row does not match its exact source candidate/comment or valid status." }
        $reviewRows.Add([ordered]@{ row = $row; studentName = $studentName; status = $status; characterCount = [int]$characterCountText; comment = $commentText; initiallyChecked = [string]$applyState.state })
    }
    $route.aiReview = [ordered]@{ parseSummary = $summaryText; parsedValidCount = 25; reviewModelRowCount = [int]$reviewGrid.Current.RowCount; reviewModelColumnCount = [int]$reviewGrid.Current.ColumnCount; rows = $reviewRows.ToArray(); responsePath = 'output/deterministic-ai-response.txt'; responseSha256 = Get-P914FileSha256 $responsePath; responseCharacterCount = $responseText.Length }
    $checkboxObservations = [Collections.Generic.List[object]]::new()
    Set-P914SamplerContext $Context 'operation' 'check-all-valid-ai-review-rows'
    for ($row = 0; $row -lt 25; $row++) { $checkboxObservations.Add((Invoke-P914GridCheckbox $Context $Manifest $EvidenceDirectory ('check-ai-review-row-' + $row) $dialogId $reviewTableId $row 0 'On')) }
    $route.aiReview.checkedRowCount = $checkboxObservations.Count
    $route.aiReview.checkboxActions = $checkboxObservations.ToArray()
    $applyButtonId = 'QApplication.speakingEvaluationAiBatchDialog.QDialogButtonBox.speakingEvaluationAiBatchApply'
    $applyButton = Wait-P914Control $Context $EvidenceDirectory $dialogId $applyButtonId 'ControlType.Button' '' $TimeoutSeconds
    if (-not [bool]$applyButton.Current.IsEnabled) { throw 'Apply Selected Comments is disabled after all 25 valid review rows were checked.' }
    $reviewSnapshot = Save-P914UiaSnapshot $Context $Manifest $EvidenceDirectory 'ai-batch-review-25-valid-comments' $reviewTable
    $route.aiReview.screenshot = Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'ai-batch-review' $dialog
    $route.aiReview.tableSnapshotRelativePath = [string]$reviewSnapshot.manifestRecord.relativePath
    $route.aiReview.applyButton = Get-P914ControlSnapshot $Context $applyButton
    Add-P914StableCheckpoint $Context $Manifest 'ai-review-25-checked-before-apply'
    Set-P914SamplerContext $Context 'operation' 'apply-25-ai-comments'
    Invoke-P914Button $Context $Manifest $EvidenceDirectory 'apply-25-selected-ai-comments' $applyButton
    $dialogWait = [Diagnostics.Stopwatch]::StartNew()
    while ($dialogWait.Elapsed.TotalSeconds -lt $TimeoutSeconds -and $null -ne (Get-P914OwnerRoot $Context $dialogId)) { Drain-P914Output $Context $EvidenceDirectory; Start-Sleep -Milliseconds 100 }
    if ($null -ne (Get-P914OwnerRoot $Context $dialogId)) { throw 'AI batch dialog remained open after Apply Selected Comments.' }
    $saveButtonId = 'QApplication.classMngrNextApplicationShell.QWidget.QWidget.speakingEvaluationSave'
    $saveButton = Get-P914Control $Context 'main' $saveButtonId 'ControlType.Button' 'Save Changes'
    if ($null -eq $saveButton) {
        $saveButtonId = 'QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationSave'
        $saveButton = Get-P914Control $Context 'main' $saveButtonId 'ControlType.Button' 'Save Changes'
    }
    if ($null -eq $saveButton) { throw 'Speaking Evaluation Save Changes button was not exposed after applying AI comments.' }
    if (-not [bool]$saveButton.Current.IsEnabled) { throw 'Speaking Evaluation Save Changes button is disabled after AI batch apply.' }
    $saveButtonSnapshot = Get-P914ControlSnapshot $Context $saveButton
    Invoke-P914Button $Context $Manifest $EvidenceDirectory 'save-speaking-evaluation-changes' $saveButton
    $statusId = 'QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationStatus'
    $saveStatus = Wait-P914Control $Context $EvidenceDirectory 'main' $statusId 'ControlType.Text' '' $TimeoutSeconds
    $saveStatusTimer = [Diagnostics.Stopwatch]::StartNew()
    while ($saveStatusTimer.Elapsed.TotalSeconds -lt $TimeoutSeconds -and (Get-P914ControlText $saveStatus) -notmatch 'Speaking evaluation saved\.') { Drain-P914Output $Context $EvidenceDirectory; Start-Sleep -Milliseconds 100; $saveStatus = Get-P914Control $Context 'main' $statusId 'ControlType.Text' '' }
    if ((Get-P914ControlText $saveStatus) -notmatch 'Speaking evaluation saved\.') { throw "Speaking Evaluation save confirmation was not visible: $(Get-P914ControlText $saveStatus)" }
    $saveCheckpoint = Add-P914StableCheckpoint $Context $Manifest 'ai-25-comments-saved'
    $route.aiApply = [ordered]@{ appliedCount = 25; saveButton = $saveButtonSnapshot; saveConfirmation = Get-P914ControlText $saveStatus; stableMemoryCheckpoint = $saveCheckpoint }

    $awayArrival = [Int64]$Context.arrival
    $null = Select-P914Route $Context $Manifest $EvidenceDirectory 'My Workspace' 'my_workspace'
    $suspended = Wait-P914Lifecycle $Context $EvidenceDirectory 'speaking_evaluation' 'suspended' $awayArrival $TimeoutSeconds
    Add-P914StableCheckpoint $Context $Manifest 'ai-speaking-evaluation-left-after-save'
    $reopenArrival = [Int64]$Context.arrival
    Set-P914SamplerContext $Context 'operation' 'reopen-saved-speaking-evaluation'
    $reopenedRoute = Select-P914Route $Context $Manifest $EvidenceDirectory 'Speaking Evaluation' 'speaking_evaluation'
    $reopenedActivation = Wait-P914Lifecycle $Context $EvidenceDirectory 'speaking_evaluation' 'activated' $reopenArrival $TimeoutSeconds
    $reopenedClass = Find-P914NamedControl $Context 'main' 'ControlType.ListItem' 'Heavy Fixture Class 001' $classListId
    if ($null -eq $reopenedClass) { throw 'Reopened Speaking Evaluation class list did not expose the exact saved fixture class row.' }
    $reopenedClassAction = Add-P914Action $Context $Manifest $EvidenceDirectory 'reselect-ai-fixture-class-after-save' $reopenedClass 'SelectionItemPattern.Select'
    $reopenedSelection = [System.Windows.Automation.SelectionItemPattern]$reopenedClass.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)
    $reopenedSelection.Select(); $reopenedClassAction.completedAtUtc = Get-P914Utc
    $reopenedMainRoot = Get-P914MainRoot $Context
    $reopenedMainSnapshot = Save-P914UiaDescendantSnapshot $Context $Manifest $EvidenceDirectory 'speaking-evaluation-reopened-main-descendants' $reopenedMainRoot
    $reopenedMatrixId = 'QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationContentSplitter.speakingEvaluationMatrix'
    $reopenedMatrix = Wait-P914Table $Context $EvidenceDirectory 'main' $reopenedMatrixId $TimeoutSeconds
    $reopenedGrid = Get-P914Grid $reopenedMatrix
    if ([int]$reopenedGrid.Current.RowCount -ne 100) { throw 'Reopened persisted Speaking Evaluation does not expose 100 matrix rows.' }
    for ($row = 0; $row -lt 100; $row++) {
        $cell = (Get-P914GridCell $Context 'main' $reopenedMatrixId $row 9).cell
        $actualComment = Get-P914ControlText $cell
        $expected = if ($row -lt 25) { (Get-P914DeterministicComment ($row + 1)) } else { '' }
        if ($actualComment -cne $expected) { throw "Reopened Speaking Evaluation UI comment mismatch at row $row." }
    }
    $reopenSnapshot = Save-P914UiaSnapshot $Context $Manifest $EvidenceDirectory 'speaking-evaluation-reopened-after-save' $reopenedMatrix
    $reopenCheckpoint = Add-P914StableCheckpoint $Context $Manifest 'ai-speaking-evaluation-reopened-from-store'
    $route.aiReopen = [ordered]@{ routeEventArrivalIndex = [Int64]$reopenedRoute.arrivalIndex; lifecycle = Get-P914PageModelCounts $reopenedActivation; lifecycleSuspendedArrivalIndex = [Int64]$suspended.arrivalIndex; mainDescendantSnapshotRelativePath = [string]$reopenedMainSnapshot.manifestRecord.relativePath; matrixSnapshotRelativePath = [string]$reopenSnapshot.manifestRecord.relativePath; reopenedGridPattern = Get-P914GridMetrics $reopenedMatrix; persistedVisibleCommentsVerified = 25; preservedBlankRowsVerified = 75; stableMemoryCheckpoint = $reopenCheckpoint }
    Set-P914SamplerContext $Context 'normal-use' 'saved-ai-batch-ready-to-close'
}
function Get-P914AiCandidateName([int]$Index) {
    if ($Index -lt 1 -or $Index -gt 100) { throw "AI candidate name index is outside the fixture range: $Index" }
    $letters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'; $zeroBased = $Index - 1
    return 'AI Candidate ' + [string]$letters[[int][Math]::Floor($zeroBased / 26)] + [string]$letters[$zeroBased % 26]
}
function Get-P914DeterministicComment([int]$Index) {
    $name = Get-P914AiCandidateName $Index
    "P9-14 review for ${name}: your speaking shows a clear central idea and steady organization. Next, add one precise supporting detail and explain how it connects to your main point. This will help your listener follow the reasoning and understand why the example matters."
}

function Invoke-P914PackagedLifecycle([string]$Mode, [string]$RepositoryRoot, [string]$EvidenceRoot, [string]$PriorRunManifestPath, [int]$ReadinessTimeoutSeconds = 120, [int]$ExitTimeoutSeconds = 60) {
    $RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
    $id = New-P914EvidenceDirectory $RepositoryRoot $EvidenceRoot
    $EvidenceDirectory = [string]$id.evidenceDirectory; $RunId = [string]$id.runId
    foreach ($name in @('manifest.json', 'route-manifest.json')) { $path = Join-Path $EvidenceDirectory $name; if (Test-Path -LiteralPath $path) { throw "Evidence output already exists: $path" } }
    $kind = if ($Mode -ceq 'staff-directory') { 'staff-directory' } elseif ($Mode -ceq 'ai-batch-review') { 'speaking-evaluation-ai-batch' } else { throw "Unrecognized P9-14 workflow mode $Mode" }
    $TaskId = if ($Mode -ceq 'staff-directory') { 'P9_14_STAFF_DIRECTORY_PACKAGED_LIFECYCLE' } else { 'P9_14_AI_BATCH_REVIEW_PACKAGED_LIFECYCLE' }
    $manifestPath = Join-Path $EvidenceDirectory 'manifest.json'
    $preparationPath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'preparation-receipt.json'
    $workspacePath = Join-Path (Join-Path $EvidenceDirectory 'workspace') ($Mode + '.tps')
    $beforeSnapshotPath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'before-snapshot.json'
    $afterSnapshotPath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'after-snapshot.json'
    $staffFixturePath = Join-Path $RepositoryRoot 'docs/qt-rewrite/visual-baseline/release/large-staff-directory-boundary/generated-large-staff-directory.tps'
    $baseWorkspacePath = Join-Path $RepositoryRoot 'artifacts/phase9/packaged-next-class-transfer/20261119T155840703Z-892ae1169ba945f5be16ef08fa0a3fb1/workspace-snapshots/master-blank.tps'
    $seedSqlPath = Join-Path $RepositoryRoot 'tests/fixtures/workspaces/large_startup.sql'
    $settings = $null; $package = $null; $context = $null; $failure = $null; $collected = $false
    $manifest = [ordered]@{
        schema = 'classmngr-phase9-p9-14-packaged-lifecycle-v1'; taskId = $TaskId; runId = $RunId; mode = $Mode; status = 'running'
        routeMode = if ($Mode -ceq 'staff-directory') { 'current-next-staff-directory-ui-packaged-windows-x64-release-local' } else { 'current-next-speaking-evaluation-ai-batch-ui-packaged-windows-x64-release-local' }
        startedAtUtc = Get-P914Utc; completedAtUtc = $null; repositoryRoot = $RepositoryRoot; packageProvenance = $null
        fixture = [ordered]@{ kind = $kind; workspacePath = [IO.Path]::GetFullPath($workspacePath); preparationReceipt = $null; beforeSnapshot = $null; afterSnapshot = $null }
        settings = $null; process = $null; processExit = $null; routeManifest = $null; route = [ordered]@{ workflow = $Mode; startedAtUtc = Get-P914Utc; actions = [Collections.Generic.List[object]]::new(); observations = [Collections.Generic.List[object]]::new(); routeEvents = @(); lifecycleEvents = @(); screenshots = [Collections.Generic.List[object]]::new(); uiaSnapshots = [Collections.Generic.List[object]]::new(); completedAtUtc = $null }
        memory = [ordered]@{ stableCheckpoints = [Collections.Generic.List[object]]::new(); processMetrics = $null; thresholds = [ordered]@{ stableWorkingSetExclusiveBytes = $script:P914WorkingSetStrictExclusive; strictAtOrAboveFails = $true; operationPeakAdvisoryExclusiveBytes = $script:P914OperationPeakAdvisoryExclusive; privateBytesDiagnosticOnly = $true; returnToBaselineAllowance = 'none defined; no baseline return allowance is asserted' } }
        cleanup = $null; failure = $null; validation = $null
    }
    try {
        $package = Initialize-P914PackageProof $RepositoryRoot $PriorRunManifestPath $EvidenceDirectory $Mode
        $manifest.packageProvenance = $package
        $preparer = Join-Path $PSScriptRoot 'prepare_p9_14_staff_ai_workspaces.py'
        $prepOutputPath = Join-Path (Join-Path $EvidenceDirectory 'logs') 'workspace-preparation-output.txt'
        if ($Mode -ceq 'staff-directory') {
            if ((Get-P914FileSha256 $staffFixturePath) -cne 'd60176721613ef7eef127856a95e51805c7a9b12671353b35289a5e1e8ee7363') { throw 'Accepted 96-row Staff Directory source fixture identity is missing or changed.' }
            $manifest.fixture.sourceWorkspacePath = [IO.Path]::GetFullPath($staffFixturePath); $manifest.fixture.sourceWorkspaceSha256 = Get-P914FileSha256 $staffFixturePath
            $prepArgs = @($preparer, 'prepare-staff', '--source-workspace', $staffFixturePath, '--expected-source-sha256', [string]$manifest.fixture.sourceWorkspaceSha256, '--output', $workspacePath, '--receipt', $preparationPath)
        } else {
            if ((Get-P914FileSha256 $baseWorkspacePath) -cne '85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9' -or (Get-P914FileSha256 $seedSqlPath) -cne '1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3') { throw 'Accepted P9-11 blank workspace or large_startup.sql identity is missing or changed.' }
            $manifest.fixture.baseWorkspacePath = [IO.Path]::GetFullPath($baseWorkspacePath); $manifest.fixture.baseWorkspaceSha256 = Get-P914FileSha256 $baseWorkspacePath
            $manifest.fixture.seedSqlPath = [IO.Path]::GetFullPath($seedSqlPath); $manifest.fixture.seedSqlSha256 = Get-P914FileSha256 $seedSqlPath
            $prepArgs = @($preparer, 'prepare-ai', '--base-workspace', $baseWorkspacePath, '--seed-sql', $seedSqlPath, '--output', $workspacePath, '--receipt', $preparationPath, '--expected-base-sha256', [string]$manifest.fixture.baseWorkspaceSha256, '--expected-seed-sha256', [string]$manifest.fixture.seedSqlSha256)
        }
        $prepLines = @(& python @prepArgs 2>&1 | ForEach-Object { [string]$_ }); $prepExit = $LASTEXITCODE
        [IO.File]::WriteAllLines($prepOutputPath, $prepLines, [Text.UTF8Encoding]::new($false))
        if ($prepExit -ne 0) { throw "P9-14 workspace preparation failed with exit code $prepExit." }
        $preparation = Read-P914Json $preparationPath
        if ([string]$preparation.schema -cne 'classmngr-phase9-p9-14-workspace-v1' -or [string]$preparation.status -cne 'passed' -or [string]$preparation.kind -cne $kind) { throw 'Fixture preparation receipt identity/status is invalid.' }
        $manifest.fixture.preparationReceipt = [ordered]@{ relativePath = 'workspace/preparation-receipt.json'; sha256 = Get-P914FileSha256 $preparationPath; workspaceSha256 = [string]$preparation.workspaceSha256; rowCounts = if ($Mode -ceq 'staff-directory') { [ordered]@{ nativeEnglish = 96; gsTeam = 96 } } else { [ordered]@{ candidates = 100; selectedForPrompt = 25; remaining = 75 } } }
        $beforeArgs = @($preparer, 'snapshot', '--kind', $kind, '--phase', 'before', '--workspace', $workspacePath, '--preparation-receipt', $preparationPath, '--output', $beforeSnapshotPath)
        $beforeLines = @(& python @beforeArgs 2>&1 | ForEach-Object { [string]$_ }); $beforeExit = $LASTEXITCODE
        [IO.File]::WriteAllLines((Join-Path (Join-Path $EvidenceDirectory 'logs') 'before-snapshot-output.txt'), $beforeLines, [Text.UTF8Encoding]::new($false))
        if ($beforeExit -ne 0) { throw "P9-14 before-snapshot inspection failed with exit code $beforeExit." }
        $beforeSnapshot = Read-P914Json $beforeSnapshotPath
        $manifest.fixture.beforeSnapshot = [ordered]@{ relativePath = 'workspace/before-snapshot.json'; sha256 = Get-P914FileSha256 $beforeSnapshotPath; phase = [string]$beforeSnapshot.phase; rowCounts = if ($Mode -ceq 'staff-directory') { [ordered]@{ nativeEnglish = 96; gsTeam = 96 } } else { [ordered]@{ candidates = [int]$beforeSnapshot.rowCount } } }
        $settings = Initialize-P914Settings $Mode $EvidenceDirectory; $manifest.settings = $settings
        $manifest.status = 'prepared'; Write-P914Json $manifestPath $manifest
        $context = Start-P914App $Mode $EvidenceDirectory $workspacePath ([string]$package.applicationPath)
        $context.sampler = [P914ProcessSampler]::new($context.process)
        $context.process.Refresh()
        $main = Wait-P914MainRoot $context $EvidenceDirectory $ReadinessTimeoutSeconds
        Drain-P914Output $context $EvidenceDirectory
        $manifest.process = [ordered]@{ processId = [Int64]$context.process.Id; processImagePath = [string]$context.processImagePath; processImageSha256 = [string]$context.processImageSha256; architecture = 'x64'; startedAtUtc = $context.process.StartTime.ToUniversalTime().ToString('o'); workspacePath = [string]$context.workspacePath; commandLine = [ordered]@{ executable = [string]$package.applicationPath; workspaceArgument = [string]$context.workspacePath }; stdoutPath = 'logs/process-stdout.log'; stderrPath = 'logs/process-stderr.log'; mainWindowHandle = [Int64]$context.mainWindowHandle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$context.mainWindowHandle); hwndProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$context.mainWindowHandle); closeMethod = $null; forcedTermination = $false }
        $context.sampler.SetContext('normal-use', 'startup-ready')
        $startupRouteTimer = [Diagnostics.Stopwatch]::StartNew()
        while ($startupRouteTimer.Elapsed.TotalSeconds -lt $ReadinessTimeoutSeconds -and @($context.routeEvents | Where-Object { [string]$_.targetPageId -ceq 'my_workspace' }).Count -eq 0) { Drain-P914Output $context $EvidenceDirectory; $context.process.Refresh(); if ($context.process.HasExited) { throw 'App exited before initial My Workspace route.' }; Start-Sleep -Milliseconds 100 }
        if (@($context.routeEvents | Where-Object { [string]$_.targetPageId -ceq 'my_workspace' }).Count -eq 0) { throw 'Initial route did not reach My Workspace.' }
        if ($Mode -ceq 'staff-directory') { Invoke-P914StaffWorkflow $context $manifest $EvidenceDirectory $ReadinessTimeoutSeconds }
        else { Invoke-P914AiWorkflow $context $manifest $EvidenceDirectory $preparationPath $ReadinessTimeoutSeconds }
        Set-P914SamplerContext $context 'normal-use' 'ready-for-natural-process-close'
        $exit = Invoke-P914NaturalClose $context $manifest $EvidenceDirectory $ExitTimeoutSeconds
        $manifest.processExit = $exit; $manifest.process.closeMethod = [string]$exit.closeMethod; $manifest.process.forcedTermination = $false
        $manifest.cleanup = Get-P914RuntimeCleanup $context $EvidenceDirectory
        if (@($manifest.cleanup.workspaceSidecarFilesAfterExit).Count -ne 0) { throw 'Workspace SQLite sidecar files remain after natural process exit.' }
        $afterArgs = @($preparer, 'snapshot', '--kind', $kind, '--phase', 'after', '--workspace', $workspacePath, '--preparation-receipt', $preparationPath, '--output', $afterSnapshotPath)
        $afterLines = @(& python @afterArgs 2>&1 | ForEach-Object { [string]$_ }); $afterExit = $LASTEXITCODE
        [IO.File]::WriteAllLines((Join-Path (Join-Path $EvidenceDirectory 'logs') 'after-snapshot-output.txt'), $afterLines, [Text.UTF8Encoding]::new($false))
        if ($afterExit -ne 0) { throw "P9-14 after-snapshot verification failed with exit code $afterExit." }
        $afterSnapshot = Read-P914Json $afterSnapshotPath
        $manifest.fixture.afterSnapshot = [ordered]@{ relativePath = 'workspace/after-snapshot.json'; sha256 = Get-P914FileSha256 $afterSnapshotPath; phase = [string]$afterSnapshot.phase; rowCounts = if ($Mode -ceq 'staff-directory') { [ordered]@{ nativeEnglish = 96; gsTeam = 96 } } else { [ordered]@{ candidates = [int]$afterSnapshot.rowCount; persistedComments = 25; preservedBlankRows = 75 } } }
        $collected = $true
    } catch { $failure = [ordered]@{ message = $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = $_.CategoryInfo.Category; fullyQualifiedErrorId = $_.FullyQualifiedErrorId; position = $_.InvocationInfo.PositionMessage; scriptStackTrace = $_.ScriptStackTrace } }
    finally {
        if ($null -ne $context) {
            try { Drain-P914Output $context $EvidenceDirectory } catch { }
            if ($null -ne $context.sampler) {
                try {
                    $context.process.Refresh()
                    if (-not $context.process.HasExited) {
                        try { $null = Invoke-P914NaturalClose $context $manifest $EvidenceDirectory $ExitTimeoutSeconds; $manifest.processExit = $context.processExit; if ($null -ne $manifest.process) { $manifest.process.closeMethod = [string]$context.processExit.closeMethod } }
                        catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Natural cleanup close failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup'; fullyQualifiedErrorId = $_.FullyQualifiedErrorId } } }
                    }
                    $context.sampler.Dispose()
                    $manifest.memory.processMetrics = Get-P914SamplerSummary $context $EvidenceDirectory
                    if ($null -ne $manifest.cleanup) { $manifest.cleanup = Get-P914RuntimeCleanup $context $EvidenceDirectory }
                } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Final process evidence collection failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup'; fullyQualifiedErrorId = $_.FullyQualifiedErrorId } } }
            }
            $manifest.route.routeEvents = Get-P914RouteSummary $context
            $manifest.route.lifecycleEvents = Get-P914LifecycleSummary $context
            $manifest.route.completedAtUtc = Get-P914Utc
            $manifest.routeManifest = $null
            try { Save-P914RouteManifest $manifest $EvidenceDirectory } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Route manifest serialization failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'evidence'; fullyQualifiedErrorId = $_.FullyQualifiedErrorId } } }
        }
        $manifest.failure = $failure
        $manifest.status = if ($collected -and $null -eq $failure -and $null -ne $manifest.processExit -and [bool]$manifest.processExit.normalExit -and [int]$manifest.processExit.exitCode -eq 0) { 'evidence-collected' } else { 'failed' }
        $manifest.completedAtUtc = Get-P914Utc
        try { Write-P914Json $manifestPath $manifest } catch { $manifest.status = 'failed'; if ($null -eq $manifest.failure) { $manifest.failure = [ordered]@{ message = 'Final manifest could not be serialized: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc } } }
        $validatorPath = Join-Path $PSScriptRoot 'validate_packaged_next_p9_14_lifecycle.py'
        $validationLog = Join-Path (Join-Path $EvidenceDirectory 'logs') 'validator-output.txt'
        try {
            $validationLines = @(& python $validatorPath --evidence-dir $EvidenceDirectory 2>&1 | ForEach-Object { [string]$_ }); $validationExit = $LASTEXITCODE
            [IO.File]::WriteAllLines($validationLog, $validationLines, [Text.UTF8Encoding]::new($false))
            $jsonLine = @($validationLines | Where-Object { $_.TrimStart().StartsWith('{') } | Select-Object -Last 1)
            $validationResult = if ($jsonLine.Count -eq 1) { $jsonLine[0] | ConvertFrom-Json -ErrorAction Stop } else { $null }
            $manifest.validation = [ordered]@{ status = if ($validationExit -eq 0) { 'passed' } else { 'failed' }; exitCode = $validationExit; validatorPath = [IO.Path]::GetFullPath($validatorPath); validatorSha256 = Get-P914FileSha256 $validatorPath; result = $validationResult; logPath = 'logs/validator-output.txt'; logSha256 = Get-P914FileSha256 $validationLog }
        } catch { $manifest.validation = [ordered]@{ status = 'failed'; error = $_.Exception.Message } }
        $manifest.status = if ([string]$manifest.status -ceq 'evidence-collected' -and [string]$manifest.validation.status -ceq 'passed') { 'passed' } else { 'failed' }
        $manifest.completedAtUtc = Get-P914Utc
        try { Write-P914Json $manifestPath $manifest } catch { }
    }
    if ([string]$manifest.status -cne 'passed' -or [string]$manifest.validation.status -cne 'passed') { [Console]::Error.WriteLine("P9-14 packaged $Mode lifecycle failed. Evidence preserved at $EvidenceDirectory."); exit 1 }
    Write-Output "P9-14 packaged $Mode lifecycle validated: $EvidenceDirectory"
}
