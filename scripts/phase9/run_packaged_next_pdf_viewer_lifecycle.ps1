<#
.SYNOPSIS
Runs the packaged Next QtPdf viewer open, close, suspend, reopen lifecycle.

.DESCRIPTION
Uses the preserved x64 Release package only after verifying its source and
resource provenance. Interacts with the current Next UI through PID-bound UIA,
captures raw app logs, screenshots, process metrics, and the shutdown trace.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = "",
    [string]$EvidenceRoot = "artifacts\phase9\packaged-next-pdf-viewer",
    [string]$PriorRunManifestPath = "artifacts\phase9\packaged-next-class-transfer\20261119T160942280Z-4b1cf3bb7da340e1920cb4bcbc7c4ccc\manifest.json",
    [ValidateRange(30, 300)][int]$ReadinessTimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$TaskId = "P9_12_NEXT_QTPDF_PACKAGED_LIFECYCLE"
$ManifestSchema = "classmngr-phase9-packaged-next-pdf-viewer-lifecycle-v1"
$WorkingSetLimit = [Int64]262144000
$OperationPeakLimit = [Int64]536870912
$ExpectedDocumentId = "document_guides_lesson_planning"
$ExpectedDocumentName = "Lesson Planning"
$ExpectedPdfRelativePath = "resources/assets/documents/Guides/DYB Lesson Planning Guide.pdf"
$ExpectedPdfSize = [Int64]3533707
$ExpectedPdfSha256 = "295accab548b41f0b4f56e6ab89958ad5c3347646904c155b16a56ebe926caec"
$ExpectedNewScriptPaths = @(
    "scripts/phase9/run_packaged_next_pdf_viewer_lifecycle.ps1",
    "scripts/phase9/validate_packaged_next_pdf_viewer_lifecycle.py",
    "scripts/phase9/test_validate_packaged_next_pdf_viewer_lifecycle.py"
)

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) { $RepositoryRoot = Join-Path $PSScriptRoot "..\.." }
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$PriorRunManifestPath = if ([IO.Path]::IsPathRooted($PriorRunManifestPath)) { [IO.Path]::GetFullPath($PriorRunManifestPath) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $PriorRunManifestPath)) }
$EvidenceRoot = if ([IO.Path]::IsPathRooted($EvidenceRoot)) { [IO.Path]::GetFullPath($EvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
$RunId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + "-" + [Guid]::NewGuid().ToString("N")
$EvidenceDirectory = Join-Path $EvidenceRoot $RunId
if (Test-Path -LiteralPath $EvidenceDirectory) { throw "Refusing to overwrite evidence directory $EvidenceDirectory." }
[void][IO.Directory]::CreateDirectory($EvidenceDirectory)
foreach ($directory in @("build-derivation", "diagnostics", "logs", "package", "runtime", "screenshots", "uia", "workspace")) {
    [void][IO.Directory]::CreateDirectory((Join-Path $EvidenceDirectory $directory))
}

$ManifestPath = Join-Path $EvidenceDirectory "manifest.json"
$SourceInventoryPath = Join-Path $EvidenceDirectory "source-files.json"
$PackageInventoryPath = Join-Path $EvidenceDirectory "package-files.json"
$ResourceInventoryPath = Join-Path $EvidenceDirectory "resource-files.json"
$ProcessMetricsPath = Join-Path $EvidenceDirectory "process-metrics.jsonl"
$UiaActionsPath = Join-Path $EvidenceDirectory "uia-actions.jsonl"
$StartupLogPath = Join-Path $EvidenceDirectory "startup-log.jsonl"
$ValidatorPath = Join-Path $PSScriptRoot "validate_packaged_next_pdf_viewer_lifecycle.py"
$RunnerPath = $PSCommandPath
$script:Context = $null
$script:Manifest = [ordered]@{
    schema = $ManifestSchema; taskId = $TaskId; runId = $RunId; status = "not-evaluated"
    routeMode = "current-next-ui-packaged-x64-release-on-local-windows"; startedAtUtc = [DateTimeOffset]::UtcNow.ToString("o")
    completedAtUtc = $null; repositoryRoot = $RepositoryRoot; source = $null; buildProvenance = $null
    releaseBuildReceipt = $null; package = $null; application = $null; fixture = $null
    process = $null; processExit = $null; processExitEvidence = $null; catalogReady = $null
    checkpoints = [ordered]@{}; pageHostSnapshots = [ordered]@{}; screenshots = @(); uiaActions = [ordered]@{}; processMetrics = @()
    intermediateNavigation = $null; processMetricsPath = "process-metrics.jsonl"
    shutdownTrace = $null; logs = $null; workspace = $null; thresholds = [ordered]@{
        normalUseWorkingSetBytesExclusive = $WorkingSetLimit
        operationPeakWorkingSetBytesAdvisoryExclusive = $OperationPeakLimit
        operationPeakEvaluation = "advisory only; no private-byte return allowance is defined"
        privateBytesReturnAllowance = $null; strictAtOrAboveFails = $true
    }
    files = [ordered]@{ sourceInventory = "source-files.json"; packageInventory = "package-files.json"; resourceInventory = "resource-files.json"; startupLog = "startup-log.jsonl"; uiaActions = "uia-actions.jsonl"; processMetrics = "process-metrics.jsonl"; validator = "logs/validator-output.json" }
    failure = $null; validation = $null
}

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type -TypeDefinition @'
using System;
using System.Collections.Concurrent;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

public static class P912Native
{
    private const ushort ImageFileMachineUnknown = 0x0000;
    private const ushort ImageFileMachineAmd64 = 0x8664;
    [DllImport("user32.dll", SetLastError=true)] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool BringWindowToTop(IntPtr window);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll", SetLastError=true)] private static extern IntPtr SetFocus(IntPtr window);
    [DllImport("user32.dll", SetLastError=true)] private static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll", SetLastError=true)] private static extern bool PrintWindow(IntPtr window, IntPtr deviceContext, uint flags);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool GetWindowRect(IntPtr window, out NativeRect rectangle);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool ScreenToClient(IntPtr window, ref NativePoint point);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool SetWindowPos(IntPtr window, IntPtr insertAfter, int x, int y, int width, int height, uint flags);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern bool IsWow64Process2(IntPtr process, out ushort processMachine, out ushort nativeMachine);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern uint GetCurrentThreadId();
    [DllImport("user32.dll", SetLastError=true)] private static extern bool AttachThreadInput(uint attach, uint attachTo, bool attachInput);
    [StructLayout(LayoutKind.Sequential)] private struct NativeRect { public int Left; public int Top; public int Right; public int Bottom; }
    [StructLayout(LayoutKind.Sequential)] private struct NativePoint { public int X; public int Y; }
    public static uint GetWindowProcessId(IntPtr window)
    {
        uint pid; if (GetWindowThreadProcessId(window, out pid) == 0) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowThreadProcessId failed."); return pid;
    }
    public static bool ActivateWindow(IntPtr window, uint expectedPid)
    {
        if (GetWindowProcessId(window) != expectedPid) throw new InvalidOperationException("Foreground target belongs to another process.");
        ShowWindow(window, 9);
        uint targetProcessId; uint targetThread = GetWindowThreadProcessId(window, out targetProcessId);
        IntPtr foreground = GetForegroundWindow(); uint foregroundProcessId; uint foregroundThread = foreground == IntPtr.Zero ? 0 : GetWindowThreadProcessId(foreground, out foregroundProcessId);
        uint currentThread = GetCurrentThreadId(); bool attachedToForeground = false; bool attachedToTarget = false;
        try
        {
            if (foregroundThread != 0 && foregroundThread != currentThread) attachedToForeground = AttachThreadInput(currentThread, foregroundThread, true);
            if (targetThread != 0 && targetThread != currentThread) attachedToTarget = AttachThreadInput(currentThread, targetThread, true);
            BringWindowToTop(window); SetForegroundWindow(window); SetFocus(window);
        }
        finally
        {
            if (attachedToTarget) AttachThreadInput(currentThread, targetThread, false);
            if (attachedToForeground) AttachThreadInput(currentThread, foregroundThread, false);
        }
        for (int attempt = 0; attempt < 20; attempt++)
        {
            foreground = GetForegroundWindow();
            if (foreground != IntPtr.Zero && GetWindowProcessId(foreground) == expectedPid) return true;
            Thread.Sleep(50);
        }
        return false;
    }
    public static void RaiseWindowWithoutActivation(IntPtr window, uint expectedPid)
    {
        if (GetWindowProcessId(window) != expectedPid) throw new InvalidOperationException("Window target belongs to another process.");
        if (!SetWindowPos(window, IntPtr.Zero, 0, 0, 0, 0, 0x0001 | 0x0002 | 0x0010)) throw new Win32Exception(Marshal.GetLastWin32Error(), "SetWindowPos failed.");
    }
    public static int[] GetWindowBounds(IntPtr window, uint expectedPid)
    {
        if (GetWindowProcessId(window) != expectedPid) throw new InvalidOperationException("Window target belongs to another process.");
        NativeRect bounds; if (!GetWindowRect(window, out bounds)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect failed.");
        return new int[] { bounds.Left, bounds.Top, bounds.Right - bounds.Left, bounds.Bottom - bounds.Top };
    }
    public static bool RenderWindow(IntPtr window, uint expectedPid, IntPtr deviceContext, uint flags)
    {
        if (GetWindowProcessId(window) != expectedPid) throw new InvalidOperationException("Window render target belongs to another process.");
        return PrintWindow(window, deviceContext, flags);
    }
    public static void ClickScreenPoint(IntPtr window, uint expectedPid, int x, int y)
    {
        if (GetWindowProcessId(window) != expectedPid) throw new InvalidOperationException("Click target belongs to another process.");
        NativeRect bounds; if (!GetWindowRect(window, out bounds)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect failed.");
        if (x < bounds.Left || x >= bounds.Right || y < bounds.Top || y >= bounds.Bottom) throw new InvalidOperationException("UIA click point lies outside the measured main window.");
        RaiseWindowWithoutActivation(window, expectedPid);
        NativePoint point = new NativePoint { X = x, Y = y }; if (!ScreenToClient(window, ref point)) throw new Win32Exception(Marshal.GetLastWin32Error(), "ScreenToClient failed.");
        uint packed = ((uint)(ushort)point.Y << 16) | (ushort)point.X; IntPtr lParam = new IntPtr(unchecked((int)packed));
        if (!PostMessage(window, 0x0200, IntPtr.Zero, lParam) || !PostMessage(window, 0x0201, new IntPtr(1), lParam) || !PostMessage(window, 0x0202, IntPtr.Zero, lParam)) throw new Win32Exception(Marshal.GetLastWin32Error(), "Posting mouse messages to the verified UIA target window failed.");
    }
    public static bool IsNativeX64(IntPtr process)
    {
        ushort processMachine; ushort nativeMachine;
        if (!IsWow64Process2(process, out processMachine, out nativeMachine)) throw new Win32Exception(Marshal.GetLastWin32Error(), "IsWow64Process2 failed.");
        return nativeMachine == ImageFileMachineAmd64 && processMachine == ImageFileMachineUnknown;
    }
}

public static class P912OutputPump
{
    public static Task Start(System.IO.StreamReader reader, ConcurrentQueue<string> lines)
    {
        return Task.Factory.StartNew(() => { string line; while ((line = reader.ReadLine()) != null) lines.Enqueue(line); }, CancellationToken.None, TaskCreationOptions.LongRunning, TaskScheduler.Default);
    }
}
'@

function Get-UtcTimestamp { [DateTimeOffset]::UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss.fff'Z'") }
function Get-RelativePath {
    param([string]$BasePath,[string]$TargetPath)
    $base=[IO.Path]::GetFullPath($BasePath).TrimEnd([char]92,[char]47)+[IO.Path]::DirectorySeparatorChar
    $target=[IO.Path]::GetFullPath($TargetPath)
    if(-not $target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw "Path is not under the requested evidence root: $target"}
    return $target.Substring($base.Length)
}
function Get-FileSha256 { param([string]$Path) (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Get-StringSha256 { param([string]$Value); $sha=[Security.Cryptography.SHA256]::Create(); try { ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value)))).Replace("-","").ToLowerInvariant() } finally { $sha.Dispose() } }
function Write-JsonFile { param([string]$Path,[object]$Value); [IO.File]::WriteAllText($Path,($Value | ConvertTo-Json -Depth 80),[Text.UTF8Encoding]::new($false)) }
function Add-JsonLine { param([string]$Path,[object]$Value); [IO.File]::AppendAllText($Path,($Value | ConvertTo-Json -Depth 50 -Compress)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false)) }
function Read-JsonFile { param([string]$Path); Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop }

function Get-InventoryFingerprint {
    param([object[]]$Rows)
    $sorted=[Collections.Generic.List[object]]::new()
    foreach($row in $Rows){$sorted.Add([ordered]@{path=[string]$row.path;sizeBytes=[Int64]$row.sizeBytes;sha256=([string]$row.sha256).ToLowerInvariant()})}
    $sorted.Sort([System.Comparison[object]]{param($a,$b)[StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(),([string]$b.path).ToLowerInvariant())})
    $canonical=@($sorted.ToArray() | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path,$_.sizeBytes,$_.sha256 }) -join "`n"
    Get-StringSha256 ($canonical+"`n")
}
function Get-CanonicalInventory {
    param([string]$Root)
    $base=[IO.Path]::GetFullPath($Root).TrimEnd([char[]]@('\','/'))+[IO.Path]::DirectorySeparatorChar
    $rows=[Collections.Generic.List[object]]::new()
    foreach($file in @(Get-ChildItem -LiteralPath $Root -File -Recurse)){
        if($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @('.pyc','.pyo')){continue}
        $path=[IO.Path]::GetFullPath($file.FullName); if(-not $path.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw "Inventory path escaped root: $path"}
        $rows.Add([ordered]@{path=$path.Substring($base.Length).Replace('\','/');sizeBytes=[Int64]$file.Length;sha256=Get-FileSha256 $path})
    }
    $rows.Sort([System.Comparison[object]]{param($a,$b)[StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(),([string]$b.path).ToLowerInvariant())})
    $sorted=@($rows.ToArray()); $canonical=@($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path,$_.sizeBytes,$_.sha256 }) -join "`n"
    [ordered]@{root=[IO.Path]::GetFullPath($Root);sha256=Get-StringSha256 ($canonical+"`n");fileCount=$sorted.Count;files=$sorted}
}
function Get-SourceInventory {
    param([string[]]$IncludedRoots)
    $prefix=[IO.Path]::GetFullPath($RepositoryRoot).TrimEnd([char[]]@('\','/'))+[IO.Path]::DirectorySeparatorChar
    $rows=[Collections.Generic.List[object]]::new()
    foreach($relative in $IncludedRoots){
        $target=Join-Path $RepositoryRoot $relative; if(-not (Test-Path -LiteralPath $target)){throw "Missing source inventory root $relative"}
        $scan=if(Test-Path -LiteralPath $target -PathType Leaf){@(Get-Item -LiteralPath $target)}else{@(Get-ChildItem -LiteralPath $target -File -Recurse)}
        foreach($file in $scan){
            if($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @('.pyc','.pyo')){continue}
            $path=[IO.Path]::GetFullPath($file.FullName);if(-not $path.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "Source inventory path escaped repository: $path"}
            $rows.Add([ordered]@{path=$path.Substring($prefix.Length).Replace('\','/');sizeBytes=[Int64]$file.Length;sha256=Get-FileSha256 $path})
        }
    }
    $rows.Sort([System.Comparison[object]]{param($a,$b)[StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(),([string]$b.path).ToLowerInvariant())})
    $sorted=@($rows.ToArray());$canonical=@($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path,$_.sizeBytes,$_.sha256 }) -join "`n"
    [ordered]@{root=$RepositoryRoot;includedRoots=$IncludedRoots;sha256=Get-StringSha256 ($canonical+"`n");fileCount=$sorted.Count;files=$sorted}
}

function Initialize-Provenance {
    if(-not (Test-Path -LiteralPath $PriorRunManifestPath -PathType Leaf)){throw "P9-11 accepted manifest is missing: $PriorRunManifestPath"}
    $prior=Read-JsonFile $PriorRunManifestPath
    if($prior.taskId -cne 'P9_11_NEXT_CLASS_TRANSFER_PACKAGED_LIFECYCLE' -or $prior.status -cne 'passed'){throw 'Package base is not the accepted P9-11 current-source Next run.'}
    $priorRoot=Split-Path -Parent $PriorRunManifestPath
    $priorReceiptPath=Join-Path $priorRoot 'build-derivation\derived-receipt.json'
    $priorReceipt=Read-JsonFile $priorReceiptPath
    if($priorReceipt.status -cne 'verified-script-only-derivation'){throw 'P9-11 derived receipt is not in the verified state.'}
    $sourcePackageRoot=[IO.Path]::GetFullPath([string]$prior.package.directory)
    if(-not (Test-Path -LiteralPath $sourcePackageRoot -PathType Container)){throw "P9-11 packaged Release root is missing: $sourcePackageRoot"}
    $expectedPackageInventory=Read-JsonFile (Join-Path $priorRoot 'package-files.json')
    if((Get-InventoryFingerprint @($expectedPackageInventory.files)) -cne [string]$prior.package.sha256 -or [int]$expectedPackageInventory.fileCount -ne [int]$prior.package.fileCount){throw 'P9-11 package inventory file does not match its accepted manifest.'}
    $expectedPackagePaths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($row in $expectedPackageInventory.files){[void]$expectedPackagePaths.Add([string]$row.path)}
    $sourcePackageFiles=@(Get-ChildItem -LiteralPath $sourcePackageRoot -File -Recurse)
    $unlistedPackageFiles=@($sourcePackageFiles | Where-Object { $rel=(Get-RelativePath $sourcePackageRoot $_.FullName).Replace('\','/'); -not $expectedPackagePaths.Contains($rel) })
    $unlistedRelativePaths=@($unlistedPackageFiles | ForEach-Object {(Get-RelativePath $sourcePackageRoot $_.FullName).Replace('\','/')} | Sort-Object)
    if($unlistedRelativePaths.Count -ne 1 -or [string]$unlistedRelativePaths[0] -cne 'next-exported-classes.json'){throw "P9-11 package root has an unexpected unlisted output; refusing to claim package identity: $(@($unlistedPackageFiles.FullName) -join ', ')"}
    $unlistedPackage=[ordered]@{relativePath='next-exported-classes.json';sizeBytes=[Int64]$unlistedPackageFiles[0].Length;sha256=(Get-FileSha256 $unlistedPackageFiles[0].FullName);interpretation='post-acceptance Next Class Transfer UI export output; preserved in the prior artifact and excluded from the byte-bound Release package copy'}
    $packageRoot=Join-Path $EvidenceDirectory 'package'
    foreach($row in $expectedPackageInventory.files){
        $sourceFile=Join-Path $sourcePackageRoot ([string]$row.path).Replace('/','\')
        if(-not (Test-Path -LiteralPath $sourceFile -PathType Leaf) -or [Int64](Get-Item -LiteralPath $sourceFile).Length -ne [Int64]$row.sizeBytes -or (Get-FileSha256 $sourceFile) -cne [string]$row.sha256){throw "A byte-bound P9-11 package file differs from its original inventory: $($row.path)"}
        $targetFile=Join-Path $packageRoot ([string]$row.path).Replace('/','\');[void][IO.Directory]::CreateDirectory((Split-Path -Parent $targetFile));Copy-Item -LiteralPath $sourceFile -Destination $targetFile
    }
    $packageInventory=Get-CanonicalInventory $packageRoot
    if($packageInventory.sha256 -cne [string]$prior.package.sha256 -or $packageInventory.fileCount -ne [int]$prior.package.fileCount){throw 'Reconstructed byte-bound packaged Release copy differs from the P9-11 inventory.'}
    Write-JsonFile $PackageInventoryPath $packageInventory

    $releaseReceiptPath=[IO.Path]::GetFullPath([string]$prior.releaseBuildReceipt.path)
    if((Get-FileSha256 $releaseReceiptPath) -cne [string]$prior.releaseBuildReceipt.sha256){throw 'Original P9-11 Release build receipt hash changed.'}
    $releaseReceipt=Read-JsonFile $releaseReceiptPath
    if($releaseReceipt.status -cne 'succeeded' -or $releaseReceipt.configuration -cne 'Release' -or $releaseReceipt.platform -cne 'x64' -or [int]$releaseReceipt.build.exitCode -ne 0){throw 'P9-11 preserved Release build receipt is not a successful x64 Release build.'}
    $currentCachePath=Join-Path $RepositoryRoot 'build\windows-x64-release\CMakeCache.txt'
    $currentCacheHash=Get-FileSha256 $currentCachePath
    if($currentCacheHash -cne [string]$prior.build.cacheSha256After -or $currentCacheHash -cne [string]$releaseReceipt.cacheSha256After){throw 'Current standard Release CMake cache differs from the P9-11 verified package build identity.'}
    $appPath=Join-Path $packageRoot 'ClassMngrNext.exe'
    $appHash=Get-FileSha256 $appPath
    if($appHash -cne [string]$prior.application.sha256 -or $appHash -cne [string]$releaseReceipt.releaseExecutableSha256){throw 'Packaged ClassMngrNext.exe does not match its P9-11 build receipt.'}

    $resourceRoot=[IO.Path]::GetFullPath([string]$releaseReceipt.generatedResources.verifiedStagingRoot)
    if(-not (Test-Path -LiteralPath $resourceRoot -PathType Container)){throw "Verified staged resource input tree is unavailable: $resourceRoot"}
    $resourceInventory=Get-CanonicalInventory $resourceRoot
    if($resourceInventory.sha256 -cne [string]$releaseReceipt.generatedResources.stagedResourceInputs.sha256){throw 'Staged Release resource inventory differs from the source-bound receipt.'}
    Write-JsonFile $ResourceInventoryPath $resourceInventory

    $sourceInventory=Get-SourceInventory @($prior.source.includedRoots)
    $priorSourceInventory=Read-JsonFile ([string]$prior.source.inventoryPath)
    if($sourceInventory.files.Count -ne ($priorSourceInventory.files.Count + $ExpectedNewScriptPaths.Count)){throw 'Current source has a different file count than the P9-11 inventory plus the three P9-12 scripts.'}
    $oldMap=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    $nowMap=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($row in $priorSourceInventory.files){$oldMap.Add([string]$row.path,$row)}
    foreach($row in $sourceInventory.files){$nowMap.Add([string]$row.path,$row)}
    foreach($path in $oldMap.Keys){
        if(-not $nowMap.ContainsKey($path)){throw "Current source is missing prior measured file $path"}
        $a=$oldMap[$path];$b=$nowMap[$path]
        if([Int64]$a.sizeBytes -ne [Int64]$b.sizeBytes -or [string]$a.sha256 -cne [string]$b.sha256){throw "A prior measured source file changed after P9-11: $path"}
    }
    $added=@($nowMap.Keys | Where-Object {-not $oldMap.ContainsKey($_)} | ForEach-Object { $_.Replace('\','/') })
    if([string]::Join("`n",@($added | Sort-Object)) -cne [string]::Join("`n",@($ExpectedNewScriptPaths | Sort-Object))){throw "Unexpected P9-12 source delta: $($added -join ', ')"}
    $allowed=@($priorReceipt.source.allowedPaths)+$ExpectedNewScriptPaths
    $productRows=@($sourceInventory.files | Where-Object {$allowed -cnotcontains [string]$_.path})
    $productSha=Get-InventoryFingerprint $productRows
    if($productSha -cne [string]$priorReceipt.source.productSourceSha256){throw 'Current production/configuration/resource source is not equivalent to the accepted P9-11 product source.'}
    Write-JsonFile $SourceInventoryPath $sourceInventory

    $priorManifestHash=Get-FileSha256 $PriorRunManifestPath
    $priorReceiptHash=Get-FileSha256 $priorReceiptPath
    $derivation=[ordered]@{
        schema='classmngr-phase9-source-equivalence-derivation-v1';status='verified-script-only-derivation';taskId=$TaskId
        priorRunManifestPath=$PriorRunManifestPath;priorRunManifestSha256=$priorManifestHash
        priorDerivedReceiptPath=[IO.Path]::GetFullPath($priorReceiptPath);priorDerivedReceiptSha256=$priorReceiptHash
        priorReleaseBuildReceiptPath=$releaseReceiptPath;priorReleaseBuildReceiptSha256=(Get-FileSha256 $releaseReceiptPath)
        packageInventorySha256=$packageInventory.sha256;resourceInventorySha256=$resourceInventory.sha256;sourcePackageRoot=$sourcePackageRoot;sourcePackageUnlistedOutput=$unlistedPackage;reconstructedPackageCopyRoot=$packageRoot
        source=[ordered]@{repositoryRoot=$RepositoryRoot;currentSourceSha256=$sourceInventory.sha256;productSourceSha256=$productSha;buildSourceSha256=[string]$priorReceipt.source.buildSourceSha256;allowedPaths=@($priorReceipt.source.allowedPaths)+$ExpectedNewScriptPaths;addedP912Scripts=@($ExpectedNewScriptPaths);productSourceEquivalent=$true;freshExactTreeRebuild=$false}
        statement='The reused Release package is product-source-equivalent to the passing P9-11 package after exact current-source inventory comparison; this is not a fresh exact-tree rebuild.'
    }
    $derivationPath=Join-Path (Join-Path $EvidenceDirectory 'build-derivation') 'derived-receipt.json'
    Write-JsonFile $derivationPath $derivation
    $script:Manifest.source=[ordered]@{repositoryRoot=$RepositoryRoot;includedRoots=@($prior.source.includedRoots);sha256=$sourceInventory.sha256;fileCount=$sourceInventory.fileCount;inventoryPath='source-files.json';inventoryFileSha256=(Get-FileSha256 $SourceInventoryPath);derivedReceiptPath='build-derivation/derived-receipt.json';derivedReceiptSha256=(Get-FileSha256 $derivationPath);productSourceSha256=$productSha;equivalenceClaim='source-equivalent-to-P9-11-product-source; not a fresh exact-tree rebuild'}
    $script:Manifest.buildProvenance=[ordered]@{reusedPackageFromTaskId=[string]$prior.taskId;priorRunId=[string]$prior.runId;priorRunManifestPath=$PriorRunManifestPath;priorRunManifestSha256=$priorManifestHash;priorBuildSourceSha256=[string]$priorReceipt.source.buildSourceSha256;priorProductSourceSha256=[string]$priorReceipt.source.productSourceSha256;configuration=[string]$prior.build.configuration;platform=[string]$prior.build.platform;generator=[string]$prior.build.generator;cachePath=[IO.Path]::GetFullPath($currentCachePath);cacheSha256Current=$currentCacheHash;cacheSha256After=[string]$prior.build.cacheSha256After;freshExactTreeRebuild=$false}
    $script:Manifest.releaseBuildReceipt=[ordered]@{path=$releaseReceiptPath;sha256=(Get-FileSha256 $releaseReceiptPath);buildSourceSha256=[string]$priorReceipt.source.buildSourceSha256;releaseExecutableSha256=[string]$releaseReceipt.releaseExecutableSha256;generatedResourcesSha256=[string]$releaseReceipt.generatedResources.stagedResourceInputs.sha256;stagedResourceRoot=$resourceRoot}
    $script:Manifest.package=[ordered]@{root=$packageRoot;sha256=$packageInventory.sha256;fileCount=$packageInventory.fileCount;inventoryPath='package-files.json';inventoryFileSha256=(Get-FileSha256 $PackageInventoryPath);priorPackageSha256=[string]$prior.package.sha256;priorPackageFileCount=[int]$prior.package.fileCount;sourceRunManifestPath=$PriorRunManifestPath;sourceRunManifestSha256=$priorManifestHash;originalPackagePath=$sourcePackageRoot;originalPackageUnlistedOutput=$unlistedPackage;reconstructedFromRecordedInventory=$true;resourceRoot=$resourceRoot;resourceInventoryPath='resource-files.json';resourceInventorySha256=$resourceInventory.sha256;resourceInventoryFileSha256=(Get-FileSha256 $ResourceInventoryPath);priorResourceSha256=[string]$releaseReceipt.generatedResources.stagedResourceInputs.sha256}
    $script:Manifest.application=[ordered]@{path=$appPath;sha256=$appHash;architecture='x64';sourceEquivalence='P9-11 preserved ClassMngrNext.exe; source-equivalent package, not freshly rebuilt'}

    $documentJsonPath=Join-Path $packageRoot 'resources\assets\documents\documents.json'
    $documentJsonHash=Get-FileSha256 $documentJsonPath
    $catalog=Read-JsonFile $documentJsonPath
    $entry=@($catalog.documents | Where-Object {[string]$_.id -ceq $ExpectedDocumentId})
    if($entry.Count -ne 1){throw 'Packaged document catalog does not contain one selected PDF identity.'}
    $relative="resources/assets/documents/$($entry[0].pdf.path)/$($entry[0].pdf.fileName)"
    if($relative -cne $ExpectedPdfRelativePath -or [string]$entry[0].sidebarNames.default -cne $ExpectedDocumentName){throw 'Packaged document catalog selection differs from the known PDF fixture.'}
    $pdfPath=Join-Path $packageRoot ($relative.Replace('/','\'))
    $sourcePdfPath=Join-Path $RepositoryRoot ($relative.Replace('/','\'))
    if((Get-Item -LiteralPath $pdfPath).Length -ne $ExpectedPdfSize -or (Get-FileSha256 $pdfPath) -cne $ExpectedPdfSha256){throw 'Packaged PDF body does not match the known complete fixture bytes.'}
    if((Get-FileSha256 $sourcePdfPath) -cne $ExpectedPdfSha256){throw 'Current source PDF fixture does not match the known complete fixture bytes.'}
    $pdfResourceRow=@($resourceInventory.files | Where-Object {[string]$_.path -ceq $relative})
    if($pdfResourceRow.Count -ne 1 -or [string]$pdfResourceRow[0].sha256 -cne $ExpectedPdfSha256){throw 'Staged resource inventory does not contain the selected PDF bytes.'}
    $script:Manifest.fixture=[ordered]@{documentId=$ExpectedDocumentId;displayName=$ExpectedDocumentName;resourceRelativePath=$ExpectedPdfRelativePath;packagedPdfPath=$pdfPath;pdfBytes=$ExpectedPdfSize;pdfSha256=$ExpectedPdfSha256;packageCatalogPath=$documentJsonPath;packageCatalogSha256=$documentJsonHash;sourceCatalogPath=(Join-Path $RepositoryRoot 'resources\assets\documents\documents.json');sourceCatalogSha256=(Get-FileSha256 (Join-Path $RepositoryRoot 'resources\assets\documents\documents.json'));catalogEntry=$entry[0]}
}

function Read-WorkspaceFixture {
    $sourcePath=Join-Path $RepositoryRoot 'docs\qt-rewrite\visual-baseline\release\large-pdf-viewer-visual-states\generated-large-pdf-viewer-visual.tps'
    if(-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)){throw "P9-12 workspace fixture is missing: $sourcePath"}
    $copyPath=Join-Path (Join-Path $EvidenceDirectory 'workspace') 'large-pdf-viewer-visual.tps'
    Copy-Item -LiteralPath $sourcePath -Destination $copyPath
    $sourceHash=Get-FileSha256 $sourcePath;$copyHash=Get-FileSha256 $copyPath
    if($sourceHash -cne $copyHash -or (Get-Item -LiteralPath $sourcePath).Length -ne (Get-Item -LiteralPath $copyPath).Length){throw 'Workspace fixture copy changed its source bytes.'}
    $script:Manifest.workspace=[ordered]@{sourcePath=[IO.Path]::GetFullPath($sourcePath);sourceSizeBytes=[Int64](Get-Item -LiteralPath $sourcePath).Length;sourceSha256=$sourceHash;copyPath=[IO.Path]::GetFullPath($copyPath);copySizeBytes=[Int64](Get-Item -LiteralPath $copyPath).Length;copySha256=$copyHash}
    return $copyPath
}

function ConvertTo-QuotedArgument { param([string]$Value); '"'+$Value.Replace('"','\"')+'"' }
function Start-ViewerProcess {
    param([string]$WorkspacePath)
    $route='route-1-pdf-viewer-lifecycle'
    $runtime=Join-Path (Join-Path $EvidenceDirectory 'runtime') $route
    foreach($name in @('settings','local-app-data','app-data','temp','diagnostics')){[void][IO.Directory]::CreateDirectory((Join-Path $runtime $name))}
    $tracePath=Join-Path (Join-Path $runtime 'diagnostics') 'application-shutdown.jsonl'
    $stdoutPath=Join-Path (Join-Path $EvidenceDirectory 'logs') 'process-stdout.log'
    $stderrPath=Join-Path (Join-Path $EvidenceDirectory 'logs') 'process-stderr.log'
    [IO.File]::WriteAllText($stdoutPath,'',[Text.UTF8Encoding]::new($false));[IO.File]::WriteAllText($stderrPath,'',[Text.UTF8Encoding]::new($false))
    $processStart=[Diagnostics.ProcessStartInfo]::new();$processStart.FileName=[string]$script:Manifest.application.path;$processStart.WorkingDirectory=[string]$script:Manifest.package.root;$processStart.UseShellExecute=$false;$processStart.CreateNoWindow=$false;$processStart.RedirectStandardOutput=$true;$processStart.RedirectStandardError=$true;$processStart.Arguments=ConvertTo-QuotedArgument ([IO.Path]::GetFullPath($WorkspacePath))
    $envMap=[ordered]@{CLASSMNGR_SETTINGS_ROOT=(Join-Path $runtime 'settings');CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH=[IO.Path]::GetFullPath($tracePath);LOCALAPPDATA=(Join-Path $runtime 'local-app-data');APPDATA=(Join-Path $runtime 'app-data');TEMP=(Join-Path $runtime 'temp');TMP=(Join-Path $runtime 'temp');QT_FORCE_STDERR_LOGGING='1';PATH="$($script:Manifest.package.root);$env:PATH"}
    foreach($pair in $envMap.GetEnumerator()){$processStart.EnvironmentVariables[[string]$pair.Key]=[string]$pair.Value}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$processStart
    if(-not $process.Start()){throw 'Could not start the measured packaged Next process.'}
    $process.Refresh();$stdoutQueue=[Collections.Concurrent.ConcurrentQueue[string]]::new();$stderrQueue=[Collections.Concurrent.ConcurrentQueue[string]]::new()
    $stdoutPump=[P912OutputPump]::Start($process.StandardOutput,$stdoutQueue);$stderrPump=[P912OutputPump]::Start($process.StandardError,$stderrQueue)
    $script:Context=[ordered]@{routeId=$route;process=$process;packageRoot=[string]$script:Manifest.package.root;stdoutPath=$stdoutPath;stderrPath=$stderrPath;tracePath=$tracePath;stdoutQueue=$stdoutQueue;stderrQueue=$stderrQueue;stdoutPump=$stdoutPump;stderrPump=$stderrPump;arrival=0;logEntries=[Collections.Generic.List[object]]::new();lifecycleRecords=[Collections.Generic.List[object]]::new();mainWindowHandle=0L;mainWindowTitle='';forcedTermination=$false;processExit=$null;actionIndex=0}
    $observedPath=[IO.Path]::GetFullPath([string]$process.MainModule.FileName)
    if($observedPath -cne [IO.Path]::GetFullPath([string]$script:Manifest.application.path) -or (Get-FileSha256 $observedPath) -cne [string]$script:Manifest.application.sha256 -or -not [P912Native]::IsNativeX64($process.Handle)){throw 'Live process path/hash/architecture differs from the bound packaged x64 Release app.'}
    $script:Manifest.process=[ordered]@{processId=[Int64]$process.Id;processImagePath=$observedPath;processImageSha256=(Get-FileSha256 $observedPath);architecture='x64';startedAtUtc=$process.StartTime.ToUniversalTime().ToString("o");commandLine=[ordered]@{executable=$processStart.FileName;workspaceArgument=[IO.Path]::GetFullPath($WorkspacePath)};workspacePath=[IO.Path]::GetFullPath($WorkspacePath);stdoutPath='logs/process-stdout.log';stderrPath='logs/process-stderr.log';shutdownTracePath=(Get-RelativePath $EvidenceDirectory $tracePath).Replace('\','/');mainWindowHandle=$null;windowProcessId=$null;windowHandleProcessId=$null}
    return $script:Context
}

function Drain-ProcessOutput {
    param([object]$Context)
    foreach($stream in @(@{name='stdout';queue=$Context.stdoutQueue;path=$Context.stdoutPath},@{name='stderr';queue=$Context.stderrQueue;path=$Context.stderrPath})){
        while($true){$line=$null;if(-not $stream.queue.TryDequeue([ref]$line)){break};$Context.arrival++;$entry=[ordered]@{arrivalIndex=[Int64]$Context.arrival;stream=$stream.name;observedAtUtc=Get-UtcTimestamp;line=[string]$line};$Context.logEntries.Add($entry);[IO.File]::AppendAllText($stream.path,[string]$line+[Environment]::NewLine,[Text.UTF8Encoding]::new($false));Add-JsonLine $StartupLogPath $entry
            if([string]$line -match '^ClassMngr lifecycle checkpoint=(\{.*\})$'){try{$record=$Matches[1] | ConvertFrom-Json -ErrorAction Stop;$Context.lifecycleRecords.Add([pscustomobject]@{arrivalIndex=[Int64]$Context.arrival;stream=$stream.name;line=[string]$line;record=$record})}catch{Add-JsonLine (Join-Path (Join-Path $EvidenceDirectory 'logs') 'lifecycle-parse-errors.jsonl') ([ordered]@{arrivalIndex=$Context.arrival;stream=$stream.name;line=$line;error=$_.Exception.Message})}}
        }
    }
}
function Get-AllUiaElements { param([System.Windows.Automation.AutomationElement]$Root); $Root.FindAll([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.Condition]::TrueCondition) }
function Get-UiaSnapshot {
    param([System.Windows.Automation.AutomationElement]$Element)
    $c=$Element.Current;$r=$c.BoundingRectangle;$value=$null
    try{$pattern=[System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern);$value=[string]$pattern.Current.Value}catch{}
    [ordered]@{name=[string]$c.Name;automationId=[string]$c.AutomationId;controlType=[string]$c.ControlType.ProgrammaticName;className=[string]$c.ClassName;frameworkId=[string]$c.FrameworkId;processId=[Int64]$c.ProcessId;nativeWindowHandle=[Int64]$c.NativeWindowHandle;isOffscreen=[bool]$c.IsOffscreen;isEnabled=[bool]$c.IsEnabled;isKeyboardFocusable=[bool]$c.IsKeyboardFocusable;value=$value;rectangle=[ordered]@{x=[double]$r.X;y=[double]$r.Y;width=[double]$r.Width;height=[double]$r.Height}}
}
function Get-UiaById {
    param([System.Windows.Automation.AutomationElement]$Window,[string]$AutomationId,[string]$ControlType='')
    $found=[Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach($element in (Get-AllUiaElements $Window)){$c=$element.Current;$id=[string]$c.AutomationId;$type=[string]$c.ControlType.ProgrammaticName;if(($id -ceq $AutomationId -or $id.EndsWith(".$AutomationId",[StringComparison]::Ordinal)) -and ([string]::IsNullOrEmpty($ControlType) -or $type -ceq $ControlType)){$found.Add($element)}}
    if($found.Count -ne 1){throw "UIA expected one AutomationId '$AutomationId' ControlType '$ControlType'; found $($found.Count)."};return $found[0]
}
function Get-UiaByName {
    param([System.Windows.Automation.AutomationElement]$Window,[string]$Name,[string]$ControlType='')
    $found=[Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    foreach($element in (Get-AllUiaElements $Window)){$c=$element.Current;$type=[string]$c.ControlType.ProgrammaticName;if($c.Name -ceq $Name -and ([string]::IsNullOrEmpty($ControlType) -or $type -ceq $ControlType)){$found.Add($element)}}
    if($found.Count -ne 1){$candidates=@($found | ForEach-Object {Get-UiaSnapshot $_});Write-JsonFile (Join-Path (Join-Path $EvidenceDirectory 'uia') 'ambiguous-selector.json') ([ordered]@{requestedName=$Name;controlType=$ControlType;candidateCount=$found.Count;candidates=$candidates});throw "UIA expected one element named '$Name' ControlType '$ControlType'; found $($found.Count)."};return $found[0]
}
function Get-OptionalUiaById {
    param([System.Windows.Automation.AutomationElement]$Window,[string]$AutomationId)
    $matches=@(Get-AllUiaElements $Window | Where-Object {$id=[string]$_.Current.AutomationId;$id -ceq $AutomationId -or $id.EndsWith(".$AutomationId",[StringComparison]::Ordinal)})
    if($matches.Count -gt 1){throw "UIA found duplicate AutomationId '$AutomationId'."};if($matches.Count -eq 0){return $null};return $matches[0]
}
function Get-MainWindow {
    param([object]$Context)
    $p=$Context.process;$p.Refresh();$handle=$p.MainWindowHandle
    if($handle -eq [IntPtr]::Zero){return $null}
    if([Int64][P912Native]::GetWindowProcessId($handle) -ne [Int64]$p.Id){throw 'Main-window HWND belongs to another process.'}
    $window=[System.Windows.Automation.AutomationElement]::FromHandle($handle)
    if($null -eq $window -or [Int64]$window.Current.ProcessId -ne [Int64]$p.Id){throw 'Main-window UIA root does not bind to the measured PID.'}
    $window
}
function Wait-MainWindow {
    param([object]$Context,[int]$TimeoutSeconds)
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds){Drain-ProcessOutput $Context;$Context.process.Refresh();if($Context.process.HasExited){throw "Packaged Next exited during startup with code $($Context.process.ExitCode)."};$window=Get-MainWindow $Context;if($null -ne $window){$Context.mainWindowHandle=[Int64]$Context.process.MainWindowHandle.ToInt64();$Context.mainWindowTitle=[string]$Context.process.MainWindowTitle;$windowPid=[Int64][P912Native]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle);$script:Manifest.process.mainWindowHandle=$Context.mainWindowHandle;$script:Manifest.process.windowProcessId=$windowPid;$script:Manifest.process.windowHandleProcessId=$windowPid;return $window};Start-Sleep -Milliseconds 100}
    throw "No PID-bound visible main window appeared within $TimeoutSeconds seconds."
}
function Get-PdfRecords {
    param([object]$Context)
    @($Context.lifecycleRecords | Where-Object {$_.record.owner -ceq 'document_viewer'} | ForEach-Object {$_.record})
}
function Get-LatestPdfRecord {
    param([object]$Context)
    $rows=@($Context.lifecycleRecords | Where-Object {$_.record.owner -ceq 'document_viewer' -and $_.record.details.scope_kind -ceq 'viewer_session'})
    if($rows.Count -eq 0){throw 'No current Next QtPdf viewer-session state has been emitted.'};return $rows[$rows.Count-1].record
}
function Get-OptionalDiagnosticValue {
    param([object]$Diagnostic,[string]$PropertyName)
    if($null -eq $Diagnostic){return $null}
    $property=$Diagnostic.PSObject.Properties[$PropertyName]
    if($null -eq $property){return $null}
    return $property.Value
}
function Wait-QpdfRecord {
    param([object]$Context,[Int64]$AfterArrival,[scriptblock]$Predicate,[string]$Description,[int]$TimeoutSeconds)
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds){Drain-ProcessOutput $Context;foreach($entry in $Context.lifecycleRecords){if($entry.arrivalIndex -le $AfterArrival -or $entry.record.owner -cne 'document_viewer'){continue};if(& $Predicate $entry.record){return $entry.record}};$Context.process.Refresh();if($Context.process.HasExited){throw "Packaged app exited while waiting for $Description."};Start-Sleep -Milliseconds 100}
    throw "Timed out waiting for QtPdf lifecycle evidence: $Description."
}
function Wait-LogLine {
    param([object]$Context,[Int64]$AfterArrival,[scriptblock]$Predicate,[string]$Description,[int]$TimeoutSeconds)
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds){Drain-ProcessOutput $Context;foreach($entry in $Context.logEntries){if($entry.arrivalIndex -gt $AfterArrival -and (& $Predicate ([string]$entry.line))){return $entry}};$Context.process.Refresh();if($Context.process.HasExited){throw "Packaged app exited while waiting for $Description."};Start-Sleep -Milliseconds 100}
    throw "Timed out waiting for app log evidence: $Description."
}
function Convert-PdfState {
    param([object]$Record)
    $d=$Record.details;$pageCount=[int]$d.page_count
    [ordered]@{status=[int]$d.status;error=[int]$d.error;pageCount=$pageCount;pageModelRowCount=$pageCount;pageIndex=$d.page_index;pageWidthPoints=(Get-OptionalDiagnosticValue $d 'page_width_points');pageHeightPoints=(Get-OptionalDiagnosticValue $d 'page_height_points');activeStream=[bool]$d.active_stream;documentPresent=[bool]$d.document_present;viewPresent=[bool]$d.view_present;borrowCount=[int]$d.borrow_count;generation=[int]$d.generation;pageSelectionCount=[Int64]$d.page_selection_count;renderObservation=[string]$d.render_observation}
}
function Get-PageHostState {
    param([System.Windows.Automation.AutomationElement]$Window)
    $all=@(Get-AllUiaElements $Window | ForEach-Object {Get-UiaSnapshot $_})
    $viewer=@($all | Where-Object {$_.automationId -ceq 'classMngrNextDocumentViewer' -or $_.automationId.EndsWith('.classMngrNextDocumentViewer',[StringComparison]::Ordinal)})
    $hostRoots=@($all | Where-Object {$_.automationId -ceq 'classMngrNextPageHost' -or $_.automationId.EndsWith('.classMngrNextPageHost',[StringComparison]::Ordinal)})
    $pdfView=@($all | Where-Object {$_.automationId -ceq 'documentViewerPdfView' -or $_.automationId.EndsWith('.documentViewerPdfView',[StringComparison]::Ordinal)})
    [ordered]@{viewerPageInstanceCount=$viewer.Count;viewerPageVisibleCount=@($viewer | Where-Object {-not $_.isOffscreen -and $_.rectangle.width -gt 0 -and $_.rectangle.height -gt 0}).Count;viewerPageOffscreenCount=@($viewer | Where-Object {$_.isOffscreen -or $_.rectangle.width -le 0 -or $_.rectangle.height -le 0}).Count;pageHostRootCount=$hostRoots.Count;pdfViewWidgetCount=$pdfView.Count;viewerPageUiaExposure=if($viewer.Count -eq 0){'not-present-in-control-view'}else{'present-in-control-view'}}
}
function Get-ViewportStats {
    param([System.Drawing.Bitmap]$Bitmap,[System.Windows.Automation.AutomationElement]$Element,[System.Drawing.Rectangle]$ScreenBounds)
    if($null -eq $Element){return [ordered]@{sampledPixels=0;uniqueSampledColors=0;nonWhiteSampledPixels=0}}
    $s=Get-UiaSnapshot $Element;$r=$s.rectangle
    $left=[Math]::Max(0,[int][Math]::Floor($r.x-$ScreenBounds.X));$top=[Math]::Max(0,[int][Math]::Floor($r.y-$ScreenBounds.Y));$right=[Math]::Min($Bitmap.Width,[int][Math]::Ceiling($r.x+$r.width-$ScreenBounds.X));$bottom=[Math]::Min($Bitmap.Height,[int][Math]::Ceiling($r.y+$r.height-$ScreenBounds.Y))
    $colors=[Collections.Generic.HashSet[int]]::new();$sampled=0;$nonWhite=0
    for($y=$top;$y -lt $bottom;$y+=6){for($x=$left;$x -lt $right;$x+=6){$pixel=$Bitmap.GetPixel($x,$y);$packed=($pixel.R -shl 16) -bor ($pixel.G -shl 8) -bor $pixel.B;[void]$colors.Add($packed);$sampled++;if(($pixel.R+$pixel.G+$pixel.B) -lt 735){$nonWhite++}}}
    [ordered]@{sampledPixels=$sampled;uniqueSampledColors=$colors.Count;nonWhiteSampledPixels=$nonWhite}
}
function Capture-Screenshot {
    param([object]$Context,[string]$Checkpoint,[System.Windows.Automation.AutomationElement]$Window)
    [P912Native]::RaiseWindowWithoutActivation([IntPtr]$Context.mainWindowHandle,[uint32]$Context.process.Id)
    $path=Join-Path (Join-Path $EvidenceDirectory 'screenshots') ($Checkpoint+'.png')
    $windowBounds=[P912Native]::GetWindowBounds([IntPtr]$Context.mainWindowHandle,[uint32]$Context.process.Id);$bounds=[System.Drawing.Rectangle]::new([int]$windowBounds[0],[int]$windowBounds[1],[int]$windowBounds[2],[int]$windowBounds[3])
    if($bounds.Width -le 0 -or $bounds.Height -le 0){throw "Measured app window has invalid screenshot bounds at $Checkpoint."}
    $bitmap=[System.Drawing.Bitmap]::new($bounds.Width,$bounds.Height);$graphics=[System.Drawing.Graphics]::FromImage($bitmap);$hdc=[IntPtr]::Zero;$renderMethod='PrintWindow(PW_RENDERFULLCONTENT)'
    try{$hdc=$graphics.GetHdc();if(-not [P912Native]::RenderWindow([IntPtr]$Context.mainWindowHandle,[uint32]$Context.process.Id,$hdc,2)){$renderMethod='PrintWindow(PW_CLIENTONLY)';if(-not [P912Native]::RenderWindow([IntPtr]$Context.mainWindowHandle,[uint32]$Context.process.Id,$hdc,0)){throw "PrintWindow did not render the measured app at $Checkpoint."}};$graphics.ReleaseHdc($hdc);$hdc=[IntPtr]::Zero;$bitmap.Save($path,[System.Drawing.Imaging.ImageFormat]::Png);$viewport=Get-OptionalUiaById $Window 'documentViewerPdfView';$stats=Get-ViewportStats $bitmap $viewport $bounds;$viewportSnapshot=if($viewport){Get-UiaSnapshot $viewport}else{$null}}
    finally{if($hdc -ne [IntPtr]::Zero){$graphics.ReleaseHdc($hdc)};$graphics.Dispose();$bitmap.Dispose()}
    $file=Get-Item -LiteralPath $path
    [ordered]@{checkpoint=$Checkpoint;relativePath=(Get-RelativePath $EvidenceDirectory $path).Replace('\','/');sha256=Get-FileSha256 $path;sizeBytes=[Int64]$file.Length;width=$bounds.Width;height=$bounds.Height;captureMethod=$renderMethod;processId=[Int64]$Context.process.Id;windowHandle=[Int64]$Context.mainWindowHandle;windowProcessId=[Int64][P912Native]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle);capturedAtUtc=Get-UtcTimestamp;pdfViewportRectangle=if($viewportSnapshot){$viewportSnapshot.rectangle}else{$null};pdfViewportStats=$stats}
}
function Save-UiaCheckpoint {
    param([object]$Context,[string]$Checkpoint,[System.Windows.Automation.AutomationElement]$Window)
    $all=@(Get-AllUiaElements $Window | ForEach-Object {Get-UiaSnapshot $_})
    $path=Join-Path (Join-Path $EvidenceDirectory 'uia') ($Checkpoint+'.json')
    Write-JsonFile $path ([ordered]@{checkpoint=$Checkpoint;capturedAtUtc=Get-UtcTimestamp;processId=[Int64]$Context.process.Id;windowHandle=[Int64]$Context.mainWindowHandle;windowProcessId=[Int64][P912Native]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle);elements=$all})
    [ordered]@{relativePath=(Get-RelativePath $EvidenceDirectory $path).Replace('\','/');sha256=Get-FileSha256 $path;elementCount=$all.Count}
}
function Get-OptionalText {
    param([System.Windows.Automation.AutomationElement]$Window,[string]$Id)
    $element=Get-OptionalUiaById $Window $Id;if($null -eq $element){return $null};$s=Get-UiaSnapshot $element;if($s.value){return [string]$s.value};return [string]$s.name
}
function Capture-Checkpoint {
    param([object]$Context,[string]$Name,[System.Windows.Automation.AutomationElement]$Window,[string]$DocumentId,[switch]$Screenshot)
    $Window=Get-MainWindow $Context;if($null -eq $Window){throw "Main window disappeared at checkpoint $Name."}
    $record=Get-LatestPdfRecord $Context;$state=Convert-PdfState $record
    $viewer=Get-OptionalUiaById $Window 'classMngrNextDocumentViewer';$viewerSnapshot=if($viewer){Get-UiaSnapshot $viewer}else{$null}
    $title=Get-OptionalText $Window 'documentViewerTitle';$pageInput=Get-OptionalText $Window 'documentViewerPageInput';$pageTotal=Get-OptionalText $Window 'documentViewerPageTotal'
    $pdfView=Get-OptionalUiaById $Window 'documentViewerPdfView';$pdfViewSnapshot=if($pdfView){Get-UiaSnapshot $pdfView}else{$null}
    $pageHost=Get-PageHostState $Window;$uiaFile=Save-UiaCheckpoint $Context $Name $Window
    $screen=if($Screenshot){Capture-Screenshot $Context $Name $Window}else{$null}
    $checkpoint=[ordered]@{capturedAtUtc=Get-UtcTimestamp;documentId=$DocumentId;viewerSessionId=[Int64]$record.instance_id;pdfState=$state;pdfDocumentWrapperCount=if($state.documentPresent){1}else{0};pdfViewWrapperCount=if($state.viewPresent){1}else{0};pageModelRowCount=[int]$state.pageModelRowCount;pageModelMeaning='QPdfDocument pageCount, with one UI page per loaded PDF page';documentTitle=$title;pageInput=$pageInput;pageTotal=$pageTotal;viewerPage=$viewerSnapshot;pdfView=$pdfViewSnapshot;pageHost=$pageHost;uiaEvidence=$uiaFile;screenshot=$screen}
    $script:Manifest.checkpoints[$Name]=$checkpoint;$script:Manifest.pageHostSnapshots[$Name]=$pageHost
    if($Screenshot){$script:Manifest.screenshots+=,$screen}
    return $checkpoint
}
function Add-ProcessMetric {
    param([object]$Context,[string]$Point)
    $p=$Context.process;$p.Refresh();if($p.HasExited){throw "Cannot sample $Point after the measured process exited."}
    $sample=[ordered]@{point=$Point;sampleIndex=([Int64]@($script:Manifest.processMetrics).Count);capturedAtUtc=Get-UtcTimestamp;pid=[Int64]$p.Id;currentWorkingSetBytes=[Int64]$p.WorkingSet64;privateBytes=[Int64]$p.PrivateMemorySize64;peakWorkingSetBytes=[Int64]$p.PeakWorkingSet64;handles=[Int64]$p.HandleCount;threadCount=[Int64]$p.Threads.Count;cpuTimeMilliseconds=[Int64]$p.TotalProcessorTime.TotalMilliseconds}
    if($sample.currentWorkingSetBytes -ge $WorkingSetLimit){throw "$Point working set $($sample.currentWorkingSetBytes) is not strictly below $WorkingSetLimit."}
    Add-JsonLine $ProcessMetricsPath $sample
    if($null -eq $script:Manifest.processMetrics){$script:Manifest | Add-Member -NotePropertyName processMetrics -NotePropertyValue @()}
    $script:Manifest.processMetrics+=,$sample
    return $sample
}
function Invoke-UiaAction {
    param([object]$Context,[System.Windows.Automation.AutomationElement]$Element,[string]$ActionKind,[string]$Name,[string]$DocumentId='',[string]$TargetPageId='')
    $snapshot=Get-UiaSnapshot $Element;$targetProcessId=[Int64]$Context.process.Id
    if($snapshot.processId -ne $targetProcessId -or -not $snapshot.isEnabled -or $snapshot.isOffscreen){throw "UIA action target PID/enabled/visibility check failed for $Name."}
    $issued=Get-UtcTimestamp;$method='';$pointerAction=$null
    if($snapshot.controlType -ceq 'ControlType.TreeItem'){$x=[int][Math]::Round($snapshot.rectangle.x+$snapshot.rectangle.width/2);$y=[int][Math]::Round($snapshot.rectangle.y+$snapshot.rectangle.height/2);[P912Native]::ClickScreenPoint([IntPtr]$Context.mainWindowHandle,[uint32]$Context.process.Id,$x,$y);$method='UIA-targeted window click';$pointerAction=[ordered]@{kind='window-targeted WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP at same verified UIA target';screenX=$x;screenY=$y;targetProcessId=[Int64]$Context.process.Id;windowHandle=[Int64]$Context.mainWindowHandle}}
    elseif($snapshot.controlType -ceq 'ControlType.Button'){$pattern=[System.Windows.Automation.InvokePattern]$Element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern);$pattern.Invoke();$method='InvokePattern.Invoke'}
    else{throw "Unsupported UIA action target type $($snapshot.controlType) for $ActionKind."}
    $action=[ordered]@{actionKind=$ActionKind;expectedName=$Name;observedName=$snapshot.name;automationId=$snapshot.automationId;controlType=$snapshot.controlType;processId=$targetProcessId;windowHandle=[Int64]$Context.mainWindowHandle;windowProcessId=[Int64][P912Native]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle);documentId=if($DocumentId){$DocumentId}else{$null};targetPageId=if($TargetPageId){$TargetPageId}else{$null};method=$method;pointerAction=$pointerAction;issuedAtUtc=$issued;target=$snapshot;verifiedAppEvent=$false}
    Add-JsonLine $UiaActionsPath $action
    return $action
}
function Wait-ForViewerReady {
    param([object]$Context,[Int64]$After,[int]$TimeoutSeconds)
    Wait-QpdfRecord $Context $After {param($r) $r.event -ceq 'qpdf-document-status' -and $r.details.phase -ceq 'published' -and [int]$r.details.status -eq 2 -and [bool]$r.details.active_stream -and [int]$r.details.page_count -gt 0} 'Ready PDF document with active body stream' $TimeoutSeconds | Out-Null
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds){$Context.process.Refresh();$window=Get-MainWindow $Context;$title=if($window){Get-OptionalText $window 'documentViewerTitle'}else{$null};$total=if($window){Get-OptionalText $window 'documentViewerPageTotal'}else{$null};if($title -ceq $ExpectedDocumentName -and $total -match '^of\s+\d+$'){return $window};Start-Sleep -Milliseconds 100}
    throw 'PDF status became Ready but the Next title/page-total UI did not settle.'
}
function Wait-ForStreamRelease {
    param([object]$Context,[Int64]$After,[int]$TimeoutSeconds)
    Wait-QpdfRecord $Context $After {param($r) $r.event -ceq 'primary-stream-release' -and $r.details.phase -ceq 'after' -and -not [bool]$r.details.active_stream -and [int]$r.details.status -eq 0 -and [int]$r.details.page_count -eq 0} 'released stream and Null empty QPdfDocument' $TimeoutSeconds | Out-Null
    return Get-MainWindow $Context
}
function Get-CloseObservation {
    param([object]$Context)
    $p=$Context.process;$p.Refresh();$closeCall=$null;$waitCompleted=$false;$forced=$false;$exitCode=$null
    if(-not $p.HasExited){$closeCall=$p.CloseMainWindow();$timer=[Diagnostics.Stopwatch]::StartNew();while($timer.Elapsed.TotalSeconds -lt 15 -and -not $p.HasExited){Drain-ProcessOutput $Context;$p.Refresh();if($p.HasExited){$waitCompleted=$true;break};[void]$p.WaitForExit(250)};$p.Refresh();if($p.HasExited){$waitCompleted=$true}else{$forced=$true;$p.Kill();$p.WaitForExit();$p.Refresh()}}
    $exitCode=if($p.HasExited){[Int64]$p.ExitCode}else{$null};$Context.forcedTermination=$forced
    foreach($task in @($Context.stdoutPump,$Context.stderrPump)){try{[void]$task.Wait(3000)}catch{}}
    Drain-ProcessOutput $Context
    $result=[ordered]@{closeMainWindowReturned=$closeCall;waitCompleted=$waitCompleted;exitCode=$exitCode;forcedTerminationForCleanup=$forced;normalExit=($waitCompleted -and -not $forced -and $exitCode -eq 0);observedAtUtc=Get-UtcTimestamp}
    $Context.processExit=$result;$script:Manifest.processExit=$result;return $result
}

function Invoke-Route {
    $workspacePath=Read-WorkspaceFixture
    $context=Start-ViewerProcess $workspacePath
    $window=Wait-MainWindow $context $ReadinessTimeoutSeconds
    [void](Add-ProcessMetric $context 'startup-ready')

    $catalogEntry=Wait-LogLine $context 0 {param($line)$line -match 'ClassMngrNext document-catalog event=projected outcome=completed document_count=\d+ loaded_pdf_body_count=0'} 'metadata-only document catalog readiness' $ReadinessTimeoutSeconds
    $window=Get-MainWindow $context
    $qpdfBefore=@(Get-PdfRecords $context).Count
    $pageHostBefore=Get-PageHostState $window
    $catalogCapture=Save-UiaCheckpoint $context 'catalog-ready' $window
    [void](Add-ProcessMetric $context 'catalog-ready')
    $script:Manifest.catalogReady=[ordered]@{capturedAtUtc=Get-UtcTimestamp;appLine=[string]$catalogEntry.line;loadedPdfBodyCount=0;qpdfEventCount=$qpdfBefore;viewerPageInstanceCount=[int]$pageHostBefore.viewerPageInstanceCount;pageHostRootCount=[int]$pageHostBefore.pageHostRootCount;uiaEvidence=$catalogCapture}
    $script:Manifest.pageHostSnapshots['catalogReady']=$pageHostBefore
    if($qpdfBefore -ne 0 -or $pageHostBefore.viewerPageInstanceCount -ne 0){throw 'A QtPdf body/session or viewer page existed before a document was selected.'}

    $docRow=Get-UiaByName $window $ExpectedDocumentName 'ControlType.TreeItem'
    $script:Context.actionIndex=$context.arrival
    $script:Manifest.uiaActions.firstOpen=Invoke-UiaAction $context $docRow 'select-catalog-document' $ExpectedDocumentName $ExpectedDocumentId
    $window=Wait-ForViewerReady $context $context.actionIndex $ReadinessTimeoutSeconds
    $checkpoint=Capture-Checkpoint $context 'firstOpen' $window $ExpectedDocumentId -Screenshot
    if($checkpoint.pdfState.pageCount -ne 38 -or $checkpoint.pageTotal -cne 'of 38' -or $checkpoint.pageInput -cne '1'){throw 'First rendered Lesson Planning PDF did not expose the expected 38-page first-page UI state.'}
    [void](Add-ProcessMetric $context 'first-open')

    $closeButton=Get-UiaById $window 'documentViewerClose' 'ControlType.Button'
    $script:Manifest.uiaActions.close=Invoke-UiaAction $context $closeButton 'close-pdf' 'Close' ''
    $afterClose=$context.arrival
    $window=Wait-ForStreamRelease $context $afterClose $ReadinessTimeoutSeconds
    $closed=Capture-Checkpoint $context 'closed' $window '' -Screenshot
    if($closed.pdfState.activeStream -or $closed.pdfState.pageCount -ne 0 -or $closed.pdfDocumentWrapperCount -ne 1 -or $closed.pdfViewWrapperCount -ne 1){throw 'Explicit close did not release the PDF body and leave exactly one documented empty wrapper pair.'}
    if($closed.documentTitle -or $closed.pageTotal -cne 'of 0'){throw 'Explicit close did not clear the title/page UI.'}
    [void](Add-ProcessMetric $context 'closed')

    $beforeIntermediate=$context.arrival
    $calendar=Get-UiaByName $window 'Calendar' 'ControlType.TreeItem'
    $script:Manifest.uiaActions.closedNavigateAway=Invoke-UiaAction $context $calendar 'navigate-away-after-explicit-close' 'Calendar' '' 'calendar'
    $routeLine=Wait-LogLine $context $beforeIntermediate {param($line)$line -match 'ClassMngrNext navigation event=page-route outcome=completed cause=selection .*to=calendar .*loaded_pdf_body_count=0'} 'Calendar page navigation with zero loaded PDF bodies' $ReadinessTimeoutSeconds
    $script:Manifest.intermediateNavigation=[ordered]@{purpose='clear the selected document row after explicit close so the same typed document can be selected again';fromPage='document viewer';toPage='calendar';appEventLine=[string]$routeLine.line;activePdfStreamAtTransition=$false;note='This transition occurs after explicit close and is separate from the active-content suspension check below.'}
    $timer=[Diagnostics.Stopwatch]::StartNew();do{Drain-ProcessOutput $context;$window=Get-MainWindow $context;$pageState=Get-PageHostState $window;if($pageState.viewerPageVisibleCount -eq 0){break};Start-Sleep -Milliseconds 100}while($timer.Elapsed.TotalSeconds -lt 10)
    if($pageState.viewerPageVisibleCount -ne 0){throw 'Document viewer page remained visible after navigating to Calendar.'}

    $window=Get-MainWindow $context;$docRow=Get-UiaByName $window $ExpectedDocumentName 'ControlType.TreeItem';$script:Context.actionIndex=$context.arrival
    $script:Manifest.uiaActions.reopen=Invoke-UiaAction $context $docRow 'select-catalog-document' $ExpectedDocumentName $ExpectedDocumentId
    $window=Wait-ForViewerReady $context $context.actionIndex $ReadinessTimeoutSeconds
    $checkpoint=Capture-Checkpoint $context 'reopen' $window $ExpectedDocumentId -Screenshot
    if($checkpoint.pdfState.pageCount -ne 38 -or $checkpoint.pdfState.generation -le $script:Manifest.checkpoints.firstOpen.pdfState.generation){throw 'Reopened Lesson Planning PDF did not advance its session generation or retain the page count.'}
    [void](Add-ProcessMetric $context 'reopen')

    $beforeSuspend=$context.arrival;$workspacePageItem=Get-UiaByName $window 'My Workspace' 'ControlType.TreeItem'
    $script:Manifest.uiaActions.suspend=Invoke-UiaAction $context $workspacePageItem 'suspend-document-viewer' 'My Workspace' '' 'my_workspace'
    $routeLine=Wait-LogLine $context $beforeSuspend {param($line)$line -match 'ClassMngrNext navigation event=page-route outcome=completed cause=selection .*to=my_workspace .*loaded_pdf_body_count=0'} 'My Workspace route after active PDF suspension' $ReadinessTimeoutSeconds
    $window=Get-MainWindow $context
    $suspendRecord=Wait-QpdfRecord $context $beforeSuspend {param($r)$r.event -ceq 'primary-stream-release' -and $r.details.phase -ceq 'after' -and -not [bool]$r.details.active_stream -and [int]$r.details.page_count -eq 0} 'active PDF stream release on page suspension' $ReadinessTimeoutSeconds
    $suspended=Capture-Checkpoint $context 'suspended' $window '' -Screenshot
    $suspended.sessionId=[Int64]$suspendRecord.instance_id
    $suspended.pageHost.retainedViewerSessionId=$suspended.viewerSessionId
    $suspended.pageHost.retentionBasis='same QtPdf lifecycle instance_id before and after PageHost suspension; PageHost caches suspended page instances by source contract'
    if($suspended.pdfState.activeStream -or $suspended.pdfState.pageCount -ne 0){throw 'Page suspension retained active PDF stream/page state.'}
    if($suspended.pageHost.viewerPageInstanceCount -gt 1 -or $suspended.pageHost.viewerPageVisibleCount -ne 0){throw 'PageHost UIA state did not show at most one retained hidden viewer page.'}
    $script:Manifest.intermediateNavigation.activeContentSuspensionAppLine=[string]$routeLine.line
    [void](Add-ProcessMetric $context 'suspended')

    $window=Get-MainWindow $context;$docRow=Get-UiaByName $window $ExpectedDocumentName 'ControlType.TreeItem';$script:Context.actionIndex=$context.arrival
    $script:Manifest.uiaActions.postSuspendReopen=Invoke-UiaAction $context $docRow 'reenter-and-reopen-catalog-document' $ExpectedDocumentName $ExpectedDocumentId
    $window=Wait-ForViewerReady $context $context.actionIndex $ReadinessTimeoutSeconds
    $checkpoint=Capture-Checkpoint $context 'postSuspendReopen' $window $ExpectedDocumentId -Screenshot
    if($checkpoint.pdfState.pageCount -ne 38 -or $checkpoint.pdfState.generation -le $script:Manifest.checkpoints.reopen.pdfState.generation){throw 'Post-suspension document recreation did not advance generation or retain page count.'}
    if($checkpoint.viewerSessionId -ne $script:Manifest.checkpoints.firstOpen.viewerSessionId){throw 'QtPdf session identity changed across close, suspension, and reopen; PageHost retention is not proven.'}
    [void](Add-ProcessMetric $context 'post-suspend-reopen')

    [void](Add-ProcessMetric $context 'pre-shutdown')
    $script:Manifest.uiaActionsLogSha256=Get-FileSha256 $UiaActionsPath
    $exit=Get-CloseObservation $context
    $script:Manifest.processExitEvidence=$exit
    $script:Manifest.shutdownTrace=[ordered]@{relativePath=(Get-RelativePath $EvidenceDirectory $context.tracePath).Replace('\','/');sha256=if(Test-Path -LiteralPath $context.tracePath -PathType Leaf){Get-FileSha256 $context.tracePath}else{$null};events=@()}
    if(Test-Path -LiteralPath $context.tracePath -PathType Leaf){try{$trace=@(Get-Content -LiteralPath $context.tracePath -Encoding UTF8 | Where-Object {$_.Trim()} | ForEach-Object {$_ | ConvertFrom-Json});$script:Manifest.shutdownTrace.events=@($trace | ForEach-Object {[string]$_.event});$script:Manifest.shutdownTrace.rows=$trace}catch{$script:Manifest.shutdownTrace.parseError=$_.Exception.Message}}
    if(-not $exit.normalExit){throw "Packaged Next process did not exit naturally with code 0 within the 15 second close gate (code=$($exit.exitCode), forced=$($exit.forcedTerminationForCleanup))."}
    $script:Manifest.logs=[ordered]@{stdout='logs/process-stdout.log';stdoutSha256=(Get-FileSha256 $context.stdoutPath);stderr='logs/process-stderr.log';stderrSha256=(Get-FileSha256 $context.stderrPath)}
    $script:Manifest.process.processMetrics=@($script:Manifest.processMetrics)
    $script:Manifest.status='captured-awaiting-validation';$script:Manifest.completedAtUtc=Get-UtcTimestamp
}

function Write-FailureAndClose {
    param([object]$ErrorRecord)
    if($null -ne $script:Context -and $null -eq $script:Context.processExit){try{$exit=Get-CloseObservation $script:Context;$script:Manifest.processExitEvidence=$exit}catch{$script:Manifest.processExitEvidence=[ordered]@{closeError=$_.Exception.Message;forcedTerminationForCleanup=$true;normalExit=$false}}}
    $script:Manifest.status='failed';$script:Manifest.completedAtUtc=Get-UtcTimestamp;$script:Manifest.failure=[ordered]@{message=[string]$ErrorRecord.Exception.Message;type=[string]$ErrorRecord.Exception.GetType().FullName;scriptStack=[string]$ErrorRecord.ScriptStackTrace}
    if($null -ne $script:Context){
        try{$script:Manifest.logs=[ordered]@{stdout='logs/process-stdout.log';stdoutSha256=if(Test-Path $script:Context.stdoutPath){Get-FileSha256 $script:Context.stdoutPath}else{$null};stderr='logs/process-stderr.log';stderrSha256=if(Test-Path $script:Context.stderrPath){Get-FileSha256 $script:Context.stderrPath}else{$null}}}catch{}
        try{if(Test-Path $script:Context.tracePath){$trace=@(Get-Content -LiteralPath $script:Context.tracePath -Encoding UTF8 | Where-Object {$_.Trim()} | ForEach-Object {$_ | ConvertFrom-Json});$script:Manifest.shutdownTrace=[ordered]@{relativePath=(Get-RelativePath $EvidenceDirectory $script:Context.tracePath).Replace('\','/');sha256=(Get-FileSha256 $script:Context.tracePath);events=@($trace | ForEach-Object {[string]$_.event});rows=$trace}}}catch{}
    }
    try{Write-JsonFile $ManifestPath $script:Manifest}catch{}
}

try {
    Initialize-Provenance
    Invoke-Route
    Write-JsonFile $ManifestPath $script:Manifest
    $validatorOutputPath=Join-Path (Join-Path $EvidenceDirectory 'logs') 'validator-output.json'
    $pythonOutput=& python $ValidatorPath --evidence-dir $EvidenceDirectory 2>&1
    $validatorExit=$LASTEXITCODE
    [IO.File]::WriteAllText($validatorOutputPath,(@($pythonOutput) -join [Environment]::NewLine)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
    if($validatorExit -ne 0){$script:Manifest.status='validation-failed';$script:Manifest.validation=[ordered]@{exitCode=$validatorExit;outputPath='logs/validator-output.json';outputSha256=(Get-FileSha256 $validatorOutputPath)};Write-JsonFile $ManifestPath $script:Manifest;throw "P9-12 evidence validator failed with exit code $validatorExit."}
    $result=Read-JsonFile $validatorOutputPath
    $script:Manifest.status='passed';$script:Manifest.validation=[ordered]@{status=[string]$result.status;outputPath='logs/validator-output.json';outputSha256=(Get-FileSha256 $validatorOutputPath);validatorPath=[IO.Path]::GetFullPath($ValidatorPath);validatorSha256=(Get-FileSha256 $ValidatorPath);validatedAtUtc=Get-UtcTimestamp}
    $script:Manifest.completedAtUtc=Get-UtcTimestamp
    Write-JsonFile $ManifestPath $script:Manifest
    $pythonOutput=& python $ValidatorPath --evidence-dir $EvidenceDirectory 2>&1
    $validatorExit=$LASTEXITCODE
    [IO.File]::WriteAllText($validatorOutputPath,(@($pythonOutput) -join [Environment]::NewLine)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
    if($validatorExit -ne 0){$script:Manifest.status='validation-failed';Write-JsonFile $ManifestPath $script:Manifest;throw "P9-12 final manifest revalidation failed with exit code $validatorExit."}
    Write-JsonFile $ManifestPath $script:Manifest
    Write-Output (ConvertTo-Json -InputObject ([ordered]@{taskId=$TaskId;status='passed';evidenceDirectory=$EvidenceDirectory;manifestPath=$ManifestPath;manifestSha256=(Get-FileSha256 $ManifestPath);validatorPath=[IO.Path]::GetFullPath($ValidatorPath);validatorSha256=(Get-FileSha256 $ValidatorPath);validatorOutputPath=$validatorOutputPath;validatorOutputSha256=(Get-FileSha256 $validatorOutputPath)} ) -Depth 8)
} catch {
    Write-FailureAndClose $_
    Write-Error "P9-12 route preserved as failed/not evaluated at $EvidenceDirectory : $($_.Exception.Message)"
    exit 1
}
