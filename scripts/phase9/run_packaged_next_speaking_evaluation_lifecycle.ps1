<#
.SYNOPSIS
Captures the current Next speaking-evaluation report preview and batch export lifecycle.

.DESCRIPTION
Uses the preserved P9-12 x64 Release package after independently checking its
package, staged-resource, source-equivalence, and current-source fingerprints.
All application actions are bound to the measured PID and the UIA window that
owns the verified target control. Each attempt receives a new immutable
evidence directory.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = "",
    [string]$EvidenceRoot = "artifacts\phase9\packaged-next-speaking-evaluation",
    [string]$PriorRunManifestPath = "artifacts\phase9\packaged-next-pdf-viewer\20261120T064939281Z-be6b922092554021b6bcc80ecb008d6b\manifest.json",
    [ValidateRange(30, 300)][int]$ReadinessTimeoutSeconds = 120,
    [ValidateRange(60, 900)][int]$ExportTimeoutSeconds = 600,
    [switch]$ProbeOnly,
    [switch]$FinalizeOnly,
    [string]$FinalizeEvidenceDirectory = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$TaskId = "P9_13_NEXT_SPEAKING_REPORT_PACKAGED_LIFECYCLE"
$Schema = "classmngr-phase9-packaged-next-speaking-evaluation-lifecycle-v3"
$WorkingSetLimit = [Int64]262144000
$ExpectedWorkspaceStudentCount = 30
$ExpectedReportCount = 25
$ExpectedNewScriptPaths = @(
    "scripts/phase9/prepare_speaking_evaluation_workspace.py",
    "scripts/phase9/run_packaged_next_speaking_evaluation_lifecycle.ps1",
    "scripts/phase9/validate_packaged_next_speaking_evaluation_lifecycle.py",
    "scripts/phase9/test_validate_packaged_next_speaking_evaluation_lifecycle.py"
)

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) { $RepositoryRoot = Join-Path $PSScriptRoot "..\.." }
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$PriorRunManifestPath = if ([IO.Path]::IsPathRooted($PriorRunManifestPath)) { [IO.Path]::GetFullPath($PriorRunManifestPath) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $PriorRunManifestPath)) }
$EvidenceRoot = if ([IO.Path]::IsPathRooted($EvidenceRoot)) { [IO.Path]::GetFullPath($EvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
$FinalizeSourceDirectory=$null
if($FinalizeOnly){
    if($ProbeOnly){throw 'FinalizeOnly and ProbeOnly cannot be combined.'}
    if([string]::IsNullOrWhiteSpace($FinalizeEvidenceDirectory)){throw 'FinalizeOnly requires -FinalizeEvidenceDirectory pointing to an existing immutable packaged run.'}
    $FinalizeSourceDirectory=if([IO.Path]::IsPathRooted($FinalizeEvidenceDirectory)){[IO.Path]::GetFullPath($FinalizeEvidenceDirectory)}else{[IO.Path]::GetFullPath((Join-Path $RepositoryRoot $FinalizeEvidenceDirectory))}
    $evidenceRootPrefix=$EvidenceRoot.TrimEnd([char]92,[char]47)+[IO.Path]::DirectorySeparatorChar
    if(-not $FinalizeSourceDirectory.StartsWith($evidenceRootPrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'FinalizeOnly source evidence must remain beneath the configured EvidenceRoot.'}
    if(-not [IO.File]::Exists((Join-Path $FinalizeSourceDirectory 'manifest.json'))){throw "FinalizeOnly source manifest is missing: $FinalizeSourceDirectory"}
    $RunId='finalize-'+[DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'")+'-'+[Guid]::NewGuid().ToString('N')
    $EvidenceDirectory=Join-Path (Join-Path $EvidenceRoot 'finalizations') $RunId
}else{
    if(-not [string]::IsNullOrWhiteSpace($FinalizeEvidenceDirectory)){throw '-FinalizeEvidenceDirectory is valid only with -FinalizeOnly.'}
    $RunId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + "-" + [Guid]::NewGuid().ToString("N")
    $EvidenceDirectory = Join-Path $EvidenceRoot $RunId
}
if (Test-Path -LiteralPath $EvidenceDirectory) { throw "Refusing to overwrite evidence directory $EvidenceDirectory." }
[void][IO.Directory]::CreateDirectory($EvidenceDirectory)
$initialDirectories=if($FinalizeOnly){@('logs','runtime')}else{@("build-derivation", "logs", "output", "package", "runtime", "screenshots", "uia", "workspace")}
foreach ($directory in $initialDirectories) {
    [void][IO.Directory]::CreateDirectory((Join-Path $EvidenceDirectory $directory))
}

$ManifestPath = Join-Path $EvidenceDirectory $(if($FinalizeOnly){'reconciled-manifest.json'}else{'manifest.json'})
$SourceInventoryPath = Join-Path $EvidenceDirectory "source-files.json"
$PackageInventoryPath = Join-Path $EvidenceDirectory "package-files.json"
$ResourceInventoryPath = Join-Path $EvidenceDirectory "resource-files.json"
$ProcessMetricsPath = Join-Path $EvidenceDirectory "process-metrics.jsonl"
$UiaActionsPath = Join-Path $EvidenceDirectory "uia-actions.jsonl"
$StartupLogPath = Join-Path $EvidenceDirectory "application-output.jsonl"
$WatcherPath = Join-Path (Join-Path $EvidenceDirectory "runtime") "temp-directory-events.tsv"
$RunnerMemoryPath = Join-Path (Join-Path $EvidenceDirectory "runtime") "runner-memory-checkpoints.jsonl"
$ProcessOutputPath = Join-Path (Join-Path $EvidenceDirectory "logs") "runner-output.txt"
$ValidationPath = Join-Path (Join-Path $EvidenceDirectory "logs") "validation.json"
$ValidationStdoutPath = Join-Path (Join-Path $EvidenceDirectory "logs") "validator-stdout.txt"
$ValidationStderrPath = Join-Path (Join-Path $EvidenceDirectory "logs") "validator-stderr.txt"
$ReconciliationPath = Join-Path $EvidenceDirectory "reconciliation.json"
$PostValidationIntegrityPath = Join-Path (Join-Path $EvidenceDirectory "logs") "post-validation-input-integrity.json"
[IO.File]::WriteAllText($ValidationStdoutPath,'',[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText($ValidationStderrPath,'',[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText($RunnerMemoryPath,'',[Text.UTF8Encoding]::new($false))
$script:Context = $null
$script:Watcher = $null
$script:Manifest = [ordered]@{
    schema = $Schema; taskId = $TaskId; runId = $RunId; status = "not-evaluated"
    routeMode = "current-next-speaking-evaluation-ui-packaged-x64-release-on-local-windows"
    startedAtUtc = [DateTimeOffset]::UtcNow.ToString("o"); completedAtUtc = $null
    repositoryRoot = $RepositoryRoot; source = $null; buildProvenance = $null; releaseBuildReceipt = $null
    package = $null; application = $null; fixture = $null; workspace = $null; output = $null
    process = $null; processExit = $null; processExitEvidence = $null; startupReady = $null; runtimeSettingsProvenance = $null; exportCompletionUiaProbe = $null
    probeOnly = [bool]$ProbeOnly
    runnerMetrics = [Collections.Generic.List[object]]::new()
    uiaQueryMetrics = [Collections.Generic.List[object]]::new()
    uiaWaitMetrics = [Collections.Generic.List[object]]::new()
    uiaAbsenceWaitMetrics = [Collections.Generic.List[object]]::new()
    uiaQueryBurstDiagnostic = $null
    runnerMemoryCheckpoints = $null
    checkpoints = [ordered]@{}; screenshots = @(); uiaSnapshots = [ordered]@{}; uiaActions = [ordered]@{}
    processMetrics = [Collections.Generic.List[object]]::new(); lifecycleEvents = @(); temporaryStaging = $null; logs = $null
    thresholds = [ordered]@{
        normalUseWorkingSetBytesExclusive = $WorkingSetLimit
        strictAtOrAboveFails = $true
        operationPeakWorkingSetEvaluation = "reported separately from stable checkpoints; no operation return allowance is defined"
        privateBytesEvaluation = "diagnostic only; no return allowance is defined"
        harnessWorkingSetGuardDuringUia = [Int64]805306368
    }
    files = [ordered]@{
        sourceInventory = "source-files.json"; packageInventory = "package-files.json"; resourceInventory = "resource-files.json"
        workspaceReceipt = "workspace/preparation-receipt.json"; applicationOutput = "application-output.jsonl"
        uiaActions = "uia-actions.jsonl"; processMetrics = "process-metrics.jsonl"
        temporaryStagingEvents = "runtime/temp-directory-events.tsv"; validator = "logs/validation.json"
        runnerMemoryCheckpoints = "runtime/runner-memory-checkpoints.jsonl"
        settingsBeforeLaunchInventory = "runtime/settings-before-launch-inventory.json"
        settingsAfterExitInventory = "runtime/settings-after-exit-inventory.json"
    }
    failure = $null; validation = $null
}

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Text;

public static class P913ScreenshotNative
{
    public delegate bool EnumWindowsCallback(IntPtr window, IntPtr data);
    [StructLayout(LayoutKind.Sequential)]
    private struct NativeRect { public int Left; public int Top; public int Right; public int Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    private struct NativePoint { public int X; public int Y; }

    [DllImport("user32.dll", SetLastError=true)]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", SetLastError=true)]
    private static extern bool GetWindowRect(IntPtr window, out NativeRect rectangle);
    [DllImport("user32.dll", SetLastError=true)]
    private static extern bool ScreenToClient(IntPtr window, ref NativePoint point);
    [DllImport("user32.dll", SetLastError=true)]
    private static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", SetLastError=true)]
    private static extern bool PrintWindow(IntPtr window, IntPtr deviceContext, uint flags);
    [DllImport("user32.dll", SetLastError=true)]
    private static extern bool SetWindowPos(IntPtr window, IntPtr insertAfter, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll", SetLastError=true)]
    private static extern bool EnumWindows(EnumWindowsCallback callback, IntPtr data);
    [DllImport("user32.dll", SetLastError=true)]
    private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    private static extern int GetWindowText(IntPtr window, StringBuilder text, int maximumCharacters);

    public static uint GetWindowProcessId(IntPtr window)
    {
        if (window == IntPtr.Zero) throw new ArgumentException("Window handle is zero.");
        uint processId;
        if (GetWindowThreadProcessId(window, out processId) == 0)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowThreadProcessId failed.");
        return processId;
    }

    public static IntPtr[] GetTopLevelWindows(uint expectedProcessId)
    {
        List<IntPtr> windows = new List<IntPtr>();
        EnumWindowsCallback callback = delegate(IntPtr window, IntPtr data)
        {
            uint processId;
            if (GetWindowThreadProcessId(window, out processId) != 0
                && processId == expectedProcessId)
            {
                windows.Add(window);
            }
            return true;
        };
        if (!EnumWindows(callback, IntPtr.Zero))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "EnumWindows failed.");
        GC.KeepAlive(callback);
        return windows.ToArray();
    }

    public static string GetWindowCaption(IntPtr window, uint expectedProcessId)
    {
        if (GetWindowProcessId(window) != expectedProcessId)
            throw new InvalidOperationException("Caption HWND belongs to another process.");
        StringBuilder caption = new StringBuilder(512);
        GetWindowText(window, caption, caption.Capacity);
        return caption.ToString();
    }

    public static bool IsVisibleWindow(IntPtr window)
    {
        if (window == IntPtr.Zero) return false;
        return IsWindowVisible(window);
    }

    public static int[] GetWindowBounds(IntPtr window, uint expectedProcessId)
    {
        if (GetWindowProcessId(window) != expectedProcessId)
            throw new InvalidOperationException("Screenshot window belongs to another process.");
        NativeRect bounds;
        if (!GetWindowRect(window, out bounds))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect failed.");
        return new int[] { bounds.Left, bounds.Top, bounds.Right - bounds.Left, bounds.Bottom - bounds.Top };
    }

    public static void RaiseWindowWithoutActivation(IntPtr window, uint expectedProcessId)
    {
        if (GetWindowProcessId(window) != expectedProcessId)
            throw new InvalidOperationException("Screenshot window belongs to another process.");
        if (!SetWindowPos(window, IntPtr.Zero, 0, 0, 0, 0, 0x0001 | 0x0002 | 0x0010))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "SetWindowPos failed.");
    }

    public static void ClickScreenPoint(IntPtr window, uint expectedProcessId, int x, int y)
    {
        if (GetWindowProcessId(window) != expectedProcessId)
            throw new InvalidOperationException("Click target belongs to another process.");
        NativeRect bounds;
        if (!GetWindowRect(window, out bounds))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect failed.");
        if (x < bounds.Left || x >= bounds.Right || y < bounds.Top || y >= bounds.Bottom)
            throw new InvalidOperationException("UIA click point lies outside the measured main window.");
        RaiseWindowWithoutActivation(window, expectedProcessId);
        NativePoint point = new NativePoint { X = x, Y = y };
        if (!ScreenToClient(window, ref point))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "ScreenToClient failed.");
        uint packed = ((uint)(ushort)point.Y << 16) | (ushort)point.X;
        IntPtr lParam = new IntPtr(unchecked((int)packed));
        if (!PostMessage(window, 0x0200, IntPtr.Zero, lParam)
            || !PostMessage(window, 0x0201, new IntPtr(1), lParam)
            || !PostMessage(window, 0x0202, IntPtr.Zero, lParam))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Posting mouse messages to the verified UIA target window failed.");
    }

    public static bool RenderWindow(IntPtr window, uint expectedProcessId, IntPtr deviceContext, uint flags)
    {
        if (GetWindowProcessId(window) != expectedProcessId)
            throw new InvalidOperationException("Screenshot render target belongs to another process.");
        return PrintWindow(window, deviceContext, flags);
    }
}

public sealed class P913JsonReferenceComparer : IEqualityComparer<object>
{
    public new bool Equals(object left, object right) { return Object.ReferenceEquals(left, right); }
    public int GetHashCode(object value) { return RuntimeHelpers.GetHashCode(value); }
}

public static class P913StreamingJsonWriter
{
    private const long MaximumCharacters = 64L * 1024L * 1024L;
    private const long MaximumNodes = 2000000L;
    private static readonly Type PsObjectType = Type.GetType("System.Management.Automation.PSObject, System.Management.Automation", false);
    private static readonly MethodInfo AsPsObjectMethod = PsObjectType == null ? null : PsObjectType.GetMethod("AsPSObject", BindingFlags.Public | BindingFlags.Static, null, new Type[] { typeof(object) }, null);
    private static readonly PropertyInfo PsPropertiesProperty = PsObjectType == null ? null : PsObjectType.GetProperty("Properties", BindingFlags.Public | BindingFlags.Instance);
    private static readonly PropertyInfo PsBaseObjectProperty = PsObjectType == null ? null : PsObjectType.GetProperty("BaseObject", BindingFlags.Public | BindingFlags.Instance);

    private sealed class BoundedTextWriter : TextWriter
    {
        private readonly TextWriter inner;
        private readonly long maximumCharacters;
        private long written;

        public BoundedTextWriter(TextWriter innerWriter, long maximum)
        {
            inner = innerWriter;
            maximumCharacters = maximum;
        }

        public override Encoding Encoding { get { return inner.Encoding; } }

        public override void Write(char value)
        {
            if (++written > maximumCharacters) throw new InvalidOperationException("Streaming JSON exceeded its 64 MiB character bound.");
            inner.Write(value);
        }

        public override void Write(string value)
        {
            if (value == null) return;
            written += value.Length;
            if (written > maximumCharacters) throw new InvalidOperationException("Streaming JSON exceeded its 64 MiB character bound.");
            inner.Write(value);
        }
    }

    private sealed class WriteState
    {
        public BoundedTextWriter Writer;
        public HashSet<object> ActiveReferences;
        public int MaximumDepth;
        public long WorkingSetGuardBytes;
        public long Nodes;
    }

    public static void Write(TextWriter writer, object value, int maximumDepth, long workingSetGuardBytes)
    {
        if (writer == null) throw new ArgumentNullException("writer");
        if (maximumDepth < 1) throw new ArgumentOutOfRangeException("maximumDepth");
        var state = new WriteState
        {
            Writer = new BoundedTextWriter(writer, MaximumCharacters),
            ActiveReferences = new HashSet<object>(new P913JsonReferenceComparer()),
            MaximumDepth = maximumDepth,
            WorkingSetGuardBytes = workingSetGuardBytes,
            Nodes = 0L
        };
        WriteValue(state, value, 0, "$", true);
    }

    private static void CheckBounds(WriteState state, string path)
    {
        state.Nodes++;
        if (state.Nodes > MaximumNodes) throw new InvalidOperationException("Streaming JSON exceeded its 2,000,000 node bound at " + path + ".");
        if ((state.Nodes & 127L) != 0L || state.WorkingSetGuardBytes <= 0L) return;
        using (Process process = Process.GetCurrentProcess())
        {
            if (process.WorkingSet64 >= state.WorkingSetGuardBytes)
                throw new InvalidOperationException("PowerShell harness working set reached its exclusive streaming JSON guard at " + path + ": " + process.WorkingSet64 + " bytes.");
        }
    }

    private static void WriteValue(WriteState state, object value, int depth, string path, bool countNode)
    {
        if (countNode) CheckBounds(state, path);
        if (depth > state.MaximumDepth) throw new InvalidOperationException("Streaming JSON exceeded its depth bound at " + path + ".");
        if (value != null && value.GetType().FullName == "System.Management.Automation.PSObject")
        {
            if (PsBaseObjectProperty == null) throw new InvalidOperationException("PowerShell base-object access is unavailable at " + path + ".");
            object baseObject = PsBaseObjectProperty.GetValue(value, null);
            // ConvertFrom-Json stores its note properties on the PSObject wrapper,
            // while scalar strings/numbers and collections need their base value.
            if (baseObject == null || baseObject.GetType().FullName != "System.Management.Automation.PSCustomObject") value = baseObject;
        }
        if (value == null) { state.Writer.Write("null"); return; }

        if (value is string || value is char) { WriteString(state.Writer, Convert.ToString(value, CultureInfo.InvariantCulture)); return; }
        if (value is bool) { state.Writer.Write((bool)value ? "true" : "false"); return; }
        if (value is DateTime) { WriteString(state.Writer, ((DateTime)value).ToString("o", CultureInfo.InvariantCulture)); return; }
        if (value is DateTimeOffset) { WriteString(state.Writer, ((DateTimeOffset)value).ToString("o", CultureInfo.InvariantCulture)); return; }
        if (value is Guid) { WriteString(state.Writer, ((Guid)value).ToString("D")); return; }
        if (value is byte || value is sbyte || value is short || value is ushort || value is int || value is uint || value is long || value is ulong || value is decimal)
        {
            state.Writer.Write(Convert.ToString(value, CultureInfo.InvariantCulture));
            return;
        }
        if (value is float)
        {
            float number = (float)value;
            if (Single.IsNaN(number) || Single.IsInfinity(number)) throw new InvalidOperationException("Non-finite float at " + path + " is not valid JSON.");
            state.Writer.Write(number.ToString("R", CultureInfo.InvariantCulture));
            return;
        }
        if (value is double)
        {
            double number = (double)value;
            if (Double.IsNaN(number) || Double.IsInfinity(number)) throw new InvalidOperationException("Non-finite double at " + path + " is not valid JSON.");
            state.Writer.Write(number.ToString("R", CultureInfo.InvariantCulture));
            return;
        }

        IDictionary dictionary = value as IDictionary;
        IEnumerable enumerable = value as IEnumerable;
        bool isPowerShellObject = IsPowerShellObject(value);
        if (dictionary == null && enumerable == null && !isPowerShellObject)
            throw new InvalidOperationException("Unsupported streaming JSON value type " + value.GetType().FullName + " at " + path + ".");

        bool trackReference = !value.GetType().IsValueType;
        if (trackReference && !state.ActiveReferences.Add(value)) throw new InvalidOperationException("Streaming JSON reference cycle detected at " + path + ".");
        try
        {
            if (dictionary != null)
            {
                state.Writer.Write('{');
                bool first = true;
                foreach (DictionaryEntry entry in dictionary)
                {
                    if (!first) state.Writer.Write(',');
                    first = false;
                    string key = Convert.ToString(entry.Key, CultureInfo.InvariantCulture);
                    WriteString(state.Writer, key);
                    state.Writer.Write(':');
                    WriteValue(state, entry.Value, depth + 1, path + "." + key, true);
                }
                state.Writer.Write('}');
            }
            else if (isPowerShellObject)
            {
                WritePsObject(state, value, depth, path);
            }
            else
            {
                state.Writer.Write('[');
                bool first = true;
                int index = 0;
                foreach (object item in enumerable)
                {
                    if (!first) state.Writer.Write(',');
                    first = false;
                    WriteValue(state, item, depth + 1, path + "[" + index.ToString(CultureInfo.InvariantCulture) + "]", true);
                    index++;
                }
                state.Writer.Write(']');
            }
        }
        finally
        {
            if (trackReference) state.ActiveReferences.Remove(value);
        }
    }

    private static bool IsPowerShellObject(object value)
    {
        string fullName = value.GetType().FullName;
        return fullName == "System.Management.Automation.PSCustomObject" || fullName == "System.Management.Automation.PSObject";
    }

    private static void WritePsObject(WriteState state, object value, int depth, string path)
    {
        if (AsPsObjectMethod == null || PsPropertiesProperty == null)
            throw new InvalidOperationException("PowerShell object serialization support is unavailable at " + path + ".");
        object wrapper = AsPsObjectMethod.Invoke(null, new object[] { value });
        IEnumerable properties = PsPropertiesProperty.GetValue(wrapper, null) as IEnumerable;
        if (properties == null) throw new InvalidOperationException("PowerShell object has no property collection at " + path + ".");

        state.Writer.Write('{');
        bool first = true;
        foreach (object property in properties)
        {
            Type propertyType = property.GetType();
            string name = Convert.ToString(propertyType.GetProperty("Name").GetValue(property, null), CultureInfo.InvariantCulture);
            object propertyValue = propertyType.GetProperty("Value").GetValue(property, null);
            if (!first) state.Writer.Write(',');
            first = false;
            WriteString(state.Writer, name);
            state.Writer.Write(':');
            WriteValue(state, propertyValue, depth + 1, path + "." + name, true);
        }
        state.Writer.Write('}');
    }

    private static void WriteString(TextWriter writer, string value)
    {
        writer.Write('"');
        foreach (char character in value)
        {
            switch (character)
            {
                case '"': writer.Write("\\\""); break;
                case '\\': writer.Write("\\\\"); break;
                case '\b': writer.Write("\\b"); break;
                case '\f': writer.Write("\\f"); break;
                case '\n': writer.Write("\\n"); break;
                case '\r': writer.Write("\\r"); break;
                case '\t': writer.Write("\\t"); break;
                default:
                    if (character < 0x20) writer.Write("\\u" + ((int)character).ToString("x4", CultureInfo.InvariantCulture));
                    else writer.Write(character);
                    break;
            }
        }
        writer.Write('"');
    }
}
'@
$script:WatcherEvents = @()
$script:EventSourceIds = @()
$script:UiaCatalog = @{
    mainWindow = @{automationId='QApplication.classMngrNextApplicationShell';controlType='ControlType.Window';name='ClassMngr Next';container='main';isRoot=$true}
    classMngrNextNavigationTree = @{automationId='QApplication.classMngrNextApplicationShell.QWidget.classMngrNextNavigationPanel.classMngrNextNavigationTree';controlType='ControlType.Tree';name='ClassMngr Next navigation';container='main'}
    speakingEvaluationNavigationItem = @{automationId='QApplication.classMngrNextApplicationShell.QWidget.classMngrNextNavigationPanel.classMngrNextNavigationTree';controlType='ControlType.TreeItem';name='Speaking Evaluation';container='main'}
    myWorkspaceNavigationItem = @{automationId='QApplication.classMngrNextApplicationShell.QWidget.classMngrNextNavigationPanel.classMngrNextNavigationTree';controlType='ControlType.TreeItem';name='My Workspace';container='main'}
    speakingEvaluationTitle = @{automationId='QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationTitle';controlType='ControlType.Text';name='Speaking Evaluation';container='main'}
    myWorkspaceTitle = @{automationId='QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.pageTitle';controlType='ControlType.Text';name='My Workspace';container='main'}
    speakingEvaluationClassList = @{automationId='QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationContentSplitter.speakingEvaluationClassList';controlType='ControlType.List';name='';container='main'}
    speakingEvaluationClassItem = @{automationId='QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationContentSplitter.speakingEvaluationClassList';controlType='ControlType.ListItem';name='';container='main'}
    speakingEvaluationChoice = @{automationId='QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationChoice';controlType='ControlType.ComboBox';name='';container='main'}
    speakingEvaluationReports = @{automationId='QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationReports';controlType='ControlType.Button';name='Reports';container='main'}
    speakingEvaluationSaveReports = @{automationId='QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationSaveReports';controlType='ControlType.Button';name=('Save Reports As'+[char]0x2026);container='main'}
    nextSpeakingEvaluationReports = @{automationId='QApplication.nextSpeakingEvaluationReports';controlType='ControlType.Window';name='Speaking Evaluation Reports';container='QApplication.nextSpeakingEvaluationReports';isRoot=$true}
    nextReportStudent = @{automationId='QApplication.nextSpeakingEvaluationReports.nextReportStudent';controlType='ControlType.ComboBox';name='';container='QApplication.nextSpeakingEvaluationReports'}
    nextReportClose = @{automationId='QApplication.nextSpeakingEvaluationReports.QDialogButtonBox.nextReportClose';controlType='ControlType.Button';name='Close';container='QApplication.nextSpeakingEvaluationReports'}
    nextReportSavePdf = @{automationId='QApplication.nextSpeakingEvaluationReports.QDialogButtonBox.nextReportSavePdf';controlType='ControlType.Button';name='Save As PDF';container='QApplication.nextSpeakingEvaluationReports'}
    nextSpeakingEvaluationBatchReports = @{automationId='QApplication.nextSpeakingEvaluationBatchReports';controlType='ControlType.Window';name='Save Speaking Reports As';container='QApplication.nextSpeakingEvaluationBatchReports';isRoot=$true}
    nextReportScope = @{automationId='QApplication.nextSpeakingEvaluationBatchReports.nextReportScope';controlType='ControlType.ComboBox';name='Students:';container='QApplication.nextSpeakingEvaluationBatchReports'}
    nextReportRenderer = @{automationId='QApplication.nextSpeakingEvaluationBatchReports.nextReportRenderer';controlType='ControlType.ComboBox';name='Renderer:';container='QApplication.nextSpeakingEvaluationBatchReports'}
    nextReportDirectory = @{automationId='QApplication.nextSpeakingEvaluationBatchReports.nextReportDirectory';controlType='ControlType.Edit';name='';container='QApplication.nextSpeakingEvaluationBatchReports'}
    nextReportKeepPdfs = @{automationId='QApplication.nextSpeakingEvaluationBatchReports.nextReportKeepPdfs';controlType='ControlType.CheckBox';name='Keep individual PDFs after zipping';container='QApplication.nextSpeakingEvaluationBatchReports'}
    nextReportOpenOutput = @{automationId='QApplication.nextSpeakingEvaluationBatchReports.nextReportOpenOutput';controlType='ControlType.CheckBox';name='Open Output Folder after saving';container='QApplication.nextSpeakingEvaluationBatchReports'}
    nextReportBatchPreview = @{automationId='QApplication.nextSpeakingEvaluationBatchReports.QDialogButtonBox.nextReportBatchPreview';controlType='ControlType.Button';name='Preview Reports';container='QApplication.nextSpeakingEvaluationBatchReports'}
    nextReportBatchExport = @{automationId='QApplication.nextSpeakingEvaluationBatchReports.QDialogButtonBox.nextReportBatchExport';controlType='ControlType.Button';name='Save As...';container='QApplication.nextSpeakingEvaluationBatchReports'}
    qt_msgbox_buttonbox = @{automationId='QApplication.QMessageBox.qt_msgbox_buttonbox';controlType='ControlType.Group';name='';container='QApplication.QMessageBox'}
    'qt_msgbox_buttonbox.QPushButton' = @{automationId='QApplication.QMessageBox.qt_msgbox_buttonbox.QPushButton';controlType='ControlType.Button';name='OK';container='QApplication.QMessageBox'}
    reportsReadyDialog = @{automationId='QApplication.QMessageBox';controlType='ControlType.Window';name='Reports Ready';container='QApplication.QMessageBox';isRoot=$true}
}
$script:UiaSnapshotSpecs = @{
    'startup-ui-tree-before-navigation' = @('mainWindow','classMngrNextNavigationTree','myWorkspaceNavigationItem','speakingEvaluationNavigationItem','nextSpeakingEvaluationReports','nextReportStudent','nextSpeakingEvaluationBatchReports')
    'startup-ready' = @('mainWindow','classMngrNextNavigationTree','myWorkspaceNavigationItem','speakingEvaluationNavigationItem','nextSpeakingEvaluationReports','nextReportStudent','nextSpeakingEvaluationBatchReports')
    'speaking-evaluation-page-ui-tree-before-controls' = @('speakingEvaluationTitle','speakingEvaluationClassList','speakingEvaluationChoice','myWorkspaceNavigationItem','speakingEvaluationNavigationItem')
    'speaking-evaluation-page-ready' = @('speakingEvaluationTitle','speakingEvaluationClassList','speakingEvaluationChoice','speakingEvaluationReports','speakingEvaluationSaveReports','myWorkspaceNavigationItem','speakingEvaluationNavigationItem')
    'probe-my-workspace-after-navigation' = @('myWorkspaceTitle','myWorkspaceNavigationItem','speakingEvaluationNavigationItem','speakingEvaluationReports','nextSpeakingEvaluationReports','nextReportStudent')
    'direct-preview-open' = @('nextSpeakingEvaluationReports','nextReportStudent','nextReportClose','nextReportSavePdf')
    'direct-preview-closed' = @('nextSpeakingEvaluationReports','nextReportStudent','nextReportClose','nextReportSavePdf','speakingEvaluationReports')
    'batch-dialog-ready' = @('nextSpeakingEvaluationBatchReports','nextReportScope','nextReportRenderer','nextReportDirectory','nextReportKeepPdfs','nextReportOpenOutput','nextReportBatchPreview','nextReportBatchExport')
    'batch-preview-open' = @('nextSpeakingEvaluationReports','nextReportStudent','nextReportClose','nextReportSavePdf','nextSpeakingEvaluationBatchReports')
    'batch-preview-closed' = @('nextSpeakingEvaluationReports','nextReportStudent','nextReportClose','nextReportSavePdf','nextSpeakingEvaluationBatchReports')
    'export-notice' = @('reportsReadyDialog','qt_msgbox_buttonbox.QPushButton','nextSpeakingEvaluationBatchReports')
    'export-complete' = @('nextSpeakingEvaluationReports','nextReportStudent','nextReportClose','nextReportSavePdf','nextSpeakingEvaluationBatchReports','speakingEvaluationReports')
    'navigated-away' = @('myWorkspaceTitle','myWorkspaceNavigationItem','speakingEvaluationReports','nextSpeakingEvaluationReports','nextReportStudent','nextReportClose','nextReportSavePdf')
}

function Get-UtcTimestamp { [DateTimeOffset]::UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss.fff'Z'") }
function Get-FileSha256 { param([string]$Path) (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Get-StringSha256 { param([string]$Value); $sha=[Security.Cryptography.SHA256]::Create(); try { ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value)))).Replace("-","").ToLowerInvariant() } finally { $sha.Dispose() } }
function Write-RunnerMemoryCheckpoint {
    param([string]$Point,[object]$Context)
    $runner=[Diagnostics.Process]::GetCurrentProcess()
    try {
        $runner.Refresh()
        $appWorkingSet=$null
        if($null -ne $Context -and $null -ne $Context.process){
            $Context.process.Refresh()
            if(-not $Context.process.HasExited){$appWorkingSet=[Int64]$Context.process.WorkingSet64}
        }
        $sample=[ordered]@{
            point=$Point;capturedAtUtc=Get-UtcTimestamp;runnerProcessId=[Int64]$runner.Id
            runnerWorkingSetBytes=[Int64]$runner.WorkingSet64;runnerPrivateBytesDiagnosticOnly=[Int64]$runner.PrivateMemorySize64
            runnerHandles=[Int64]$runner.HandleCount;runnerThreadCount=[Int64]$runner.Threads.Count
            measuredAppProcessId=if($null -ne $Context -and $null -ne $Context.process){[Int64]$Context.process.Id}else{$null}
            appWorkingSetBytes=$appWorkingSet
        }
        $line=ConvertTo-Json -InputObject $sample -Depth 8 -Compress
        [IO.File]::AppendAllText($RunnerMemoryPath,([string]$line+[Environment]::NewLine),[Text.UTF8Encoding]::new($false))
        $sample
    } finally {$runner.Dispose()}
}
function Write-JsonFile {
    param([string]$Path,[object]$Value,[switch]$AllowOverGuard)
    $isManifest=[IO.Path]::GetFullPath($Path) -ceq [IO.Path]::GetFullPath($ManifestPath)
    if($isManifest -and -not $FinalizeOnly){[void](Write-RunnerMemoryCheckpoint 'manifest-serialize-before' $script:Context)}
    $temporaryPath=$Path+'.tmp.'+[Guid]::NewGuid().ToString('N')
    $backupPath=$Path+'.backup.'+[Guid]::NewGuid().ToString('N')
    $stream=$null;$writer=$null;$json=$null
    try {
        $encoding=[Text.UTF8Encoding]::new($false)
        if($isManifest){
            $stream=[IO.FileStream]::new($temporaryPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
            $writer=[IO.StreamWriter]::new($stream,$encoding,65536)
            $workingSetGuard=if($AllowOverGuard){[Int64]0}else{[Int64]805306368}
            [P913StreamingJsonWriter]::Write($writer,$Value,12,$workingSetGuard)
            $writer.Flush();$writer.Dispose();$writer=$null;$stream.Dispose();$stream=$null
            if(-not $FinalizeOnly){
                $sample=Write-RunnerMemoryCheckpoint 'manifest-serialize-after' $script:Context
                if(-not $AllowOverGuard -and [Int64]$sample.runnerWorkingSetBytes -ge [Int64]805306368){throw "PowerShell harness working set reached the 768 MiB safety guard while streaming the manifest: $($sample.runnerWorkingSetBytes) bytes."}
            }
        } else {
            $json=ConvertTo-Json -InputObject $Value -Depth 12 -WarningAction Stop
            [IO.File]::WriteAllText($temporaryPath,[string]$json,$encoding)
            $json=$null
        }
        if([IO.File]::Exists($Path)){
            [IO.File]::Replace($temporaryPath,$Path,$backupPath)
            if([IO.File]::Exists($backupPath)){[IO.File]::Delete($backupPath)}
        }else{[IO.File]::Move($temporaryPath,$Path)}
        if($isManifest -and -not $FinalizeOnly){
            $sample=Write-RunnerMemoryCheckpoint 'manifest-write-after' $script:Context
            if(-not $AllowOverGuard -and [Int64]$sample.runnerWorkingSetBytes -ge [Int64]805306368){throw "PowerShell harness working set reached the 768 MiB safety guard after writing the manifest: $($sample.runnerWorkingSetBytes) bytes."}
        }
    } finally {
        if($null -ne $writer){$writer.Dispose()}
        if($null -ne $stream){$stream.Dispose()}
        if([IO.File]::Exists($temporaryPath)){[IO.File]::Delete($temporaryPath)}
        if([IO.File]::Exists($backupPath)){[IO.File]::Delete($backupPath)}
    }
}
function Add-JsonLine { param([string]$Path,[object]$Value); [IO.File]::AppendAllText($Path,($Value | ConvertTo-Json -Depth 50 -Compress)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false)) }
function Read-JsonFile { param([string]$Path); Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop }
function Get-RelativePath { param([string]$BasePath,[string]$TargetPath); $base=[IO.Path]::GetFullPath($BasePath).TrimEnd([char]92,[char]47)+[IO.Path]::DirectorySeparatorChar; $target=[IO.Path]::GetFullPath($TargetPath); if(-not $target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw "Path escaped evidence directory: $target"}; return $target.Substring($base.Length).Replace('\','/') }

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
        $path=[IO.Path]::GetFullPath($file.FullName);if(-not $path.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw "Inventory path escaped root: $path"}
        $rows.Add([ordered]@{path=$path.Substring($base.Length).Replace('\','/');sizeBytes=[Int64]$file.Length;sha256=Get-FileSha256 $path})
    }
    $sorted=@($rows.ToArray());$sortedArray=[Collections.Generic.List[object]]::new();foreach($row in $sorted){$sortedArray.Add($row)}
    $sortedArray.Sort([System.Comparison[object]]{param($a,$b)[StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(),([string]$b.path).ToLowerInvariant())})
    $sorted=@($sortedArray.ToArray());$canonical=@($sorted | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path,$_.sizeBytes,$_.sha256 }) -join "`n"
    [ordered]@{root=[IO.Path]::GetFullPath($Root);sha256=Get-StringSha256 ($canonical+"`n");fileCount=$sorted.Count;files=$sorted}
}
function Get-SourceInventory {
    param([string[]]$IncludedRoots)
    $prefix=[IO.Path]::GetFullPath($RepositoryRoot).TrimEnd([char[]]@('\','/'))+[IO.Path]::DirectorySeparatorChar
    $rows=[Collections.Generic.List[object]]::new()
    foreach($relative in $IncludedRoots){
        $target=Join-Path $RepositoryRoot $relative;if(-not(Test-Path -LiteralPath $target)){throw "Source inventory root is missing: $relative"}
        $scan=if(Test-Path -LiteralPath $target -PathType Leaf){@(Get-Item -LiteralPath $target)}else{@(Get-ChildItem -LiteralPath $target -File -Recurse)}
        foreach($file in $scan){
            if($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @('.pyc','.pyo')){continue}
            $path=[IO.Path]::GetFullPath($file.FullName);if(-not $path.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "Source inventory path escaped repository: $path"}
            $rows.Add([ordered]@{path=$path.Substring($prefix.Length).Replace('\','/');sizeBytes=[Int64]$file.Length;sha256=Get-FileSha256 $path})
        }
    }
    $sorted=[Collections.Generic.List[object]]::new();foreach($row in $rows){$sorted.Add($row)}
    $sorted.Sort([System.Comparison[object]]{param($a,$b)[StringComparer]::Ordinal.Compare(([string]$a.path).ToLowerInvariant(),([string]$b.path).ToLowerInvariant())})
    $files=@($sorted.ToArray());$canonical=@($files | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path,$_.sizeBytes,$_.sha256 }) -join "`n"
    [ordered]@{root=$RepositoryRoot;includedRoots=$IncludedRoots;sha256=Get-StringSha256 ($canonical+"`n");fileCount=$files.Count;files=$files}
}

function Initialize-Provenance {
    if(-not(Test-Path -LiteralPath $PriorRunManifestPath -PathType Leaf)){throw "Accepted P9-12 manifest is missing: $PriorRunManifestPath"}
    $prior=Read-JsonFile $PriorRunManifestPath
    if($prior.taskId -cne 'P9_12_NEXT_QTPDF_PACKAGED_LIFECYCLE' -or $prior.status -cne 'passed'){throw 'Package base is not the accepted P9-12 packaged Next QtPdf route.'}
    $priorRoot=Split-Path -Parent $PriorRunManifestPath
    $priorReceiptPath=Join-Path $priorRoot ([string]$prior.source.derivedReceiptPath)
    $priorSourcePath=Join-Path $priorRoot ([string]$prior.source.inventoryPath)
    $priorReceipt=Read-JsonFile $priorReceiptPath;$priorSource=Read-JsonFile $priorSourcePath
    if((Get-FileSha256 $priorReceiptPath) -cne [string]$prior.source.derivedReceiptSha256 -or $priorReceipt.status -cne 'verified-script-only-derivation'){throw 'P9-12 source-derivation receipt is missing, changed, or unverified.'}
    if((Get-FileSha256 $PriorRunManifestPath) -cne (Get-FileSha256 ([string]$prior.buildProvenance.priorRunManifestPath)) -and -not(Test-Path -LiteralPath ([string]$prior.buildProvenance.priorRunManifestPath))){throw 'P9-12 derivation ancestry is incomplete.'}

    $sourcePackageRoot=[IO.Path]::GetFullPath([string]$prior.package.root)
    $expectedPackage=Read-JsonFile (Join-Path $priorRoot ([string]$prior.package.inventoryPath))
    if((Get-InventoryFingerprint @($expectedPackage.files)) -cne [string]$prior.package.sha256 -or [int]$expectedPackage.fileCount -ne [int]$prior.package.fileCount){throw 'Accepted P9-12 package inventory fingerprint mismatch.'}
    $packageRoot=Join-Path $EvidenceDirectory 'package'
    $expectedPackagePaths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($row in $expectedPackage.files){
        [void]$expectedPackagePaths.Add([string]$row.path)
        $sourceFile=Join-Path $sourcePackageRoot ([string]$row.path).Replace('/','\')
        if(-not(Test-Path -LiteralPath $sourceFile -PathType Leaf) -or [Int64](Get-Item -LiteralPath $sourceFile).Length -ne [Int64]$row.sizeBytes -or (Get-FileSha256 $sourceFile) -cne [string]$row.sha256){throw "P9-12 package file changed: $($row.path)"}
        $targetFile=Join-Path $packageRoot ([string]$row.path).Replace('/','\');[void][IO.Directory]::CreateDirectory((Split-Path -Parent $targetFile));Copy-Item -LiteralPath $sourceFile -Destination $targetFile
    }
    $unlisted=@(Get-ChildItem -LiteralPath $sourcePackageRoot -File -Recurse | Where-Object {$rel=$_.FullName.Substring($sourcePackageRoot.TrimEnd('\').Length+1).Replace('\','/');-not $expectedPackagePaths.Contains($rel)} | ForEach-Object {[ordered]@{relativePath=$_.FullName.Substring($sourcePackageRoot.TrimEnd('\').Length+1).Replace('\','/');sizeBytes=[Int64]$_.Length;sha256=(Get-FileSha256 $_.FullName)}})
    $packageInventory=Get-CanonicalInventory $packageRoot
    if($packageInventory.sha256 -cne [string]$prior.package.sha256 -or $packageInventory.fileCount -ne [int]$prior.package.fileCount){throw 'Reconstructed packaged Release copy differs from the accepted P9-12 package inventory.'}
    Write-JsonFile $PackageInventoryPath $packageInventory

    $releaseReceiptPath=[IO.Path]::GetFullPath([string]$prior.releaseBuildReceipt.path)
    if((Get-FileSha256 $releaseReceiptPath) -cne [string]$prior.releaseBuildReceipt.sha256){throw 'Accepted Release build receipt hash changed.'}
    $releaseReceipt=Read-JsonFile $releaseReceiptPath
    if($releaseReceipt.status -cne 'succeeded' -or $releaseReceipt.configuration -cne 'Release' -or $releaseReceipt.platform -cne 'x64' -or [int]$releaseReceipt.build.exitCode -ne 0){throw 'P9-12 preserved Release receipt does not describe a successful x64 Release build.'}
    $cachePath=Join-Path $RepositoryRoot 'build\windows-x64-release\CMakeCache.txt';$cacheHash=Get-FileSha256 $cachePath
    if($cacheHash -cne [string]$prior.buildProvenance.cacheSha256After -or $cacheHash -cne [string]$releaseReceipt.cacheSha256After){throw 'Standard Release CMake cache differs from the accepted package build identity.'}
    $appPath=Join-Path $packageRoot 'ClassMngrNext.exe';$appHash=Get-FileSha256 $appPath
    if($appHash -cne [string]$prior.application.sha256 -or $appHash -cne [string]$releaseReceipt.releaseExecutableSha256){throw 'Reconstructed app bytes differ from the accepted package and Release receipt.'}
    $resourceRoot=[IO.Path]::GetFullPath([string]$prior.package.resourceRoot)
    $resourceInventory=Get-CanonicalInventory $resourceRoot
    if($resourceInventory.sha256 -cne [string]$prior.package.resourceInventorySha256 -or $resourceInventory.sha256 -cne [string]$releaseReceipt.generatedResources.stagedResourceInputs.sha256){throw 'Current staged Release resource inputs differ from the P9-12 receipt.'}
    Write-JsonFile $ResourceInventoryPath $resourceInventory

    $sourceInventory=Get-SourceInventory @($prior.source.includedRoots)
    $oldMap=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    $nowMap=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($row in $priorSource.files){$oldMap.Add([string]$row.path,$row)}
    foreach($row in $sourceInventory.files){$nowMap.Add([string]$row.path,$row)}
    foreach($path in $oldMap.Keys){
        if(-not $nowMap.ContainsKey($path)){throw "Current source is missing a P9-12 inventoried file: $path"}
        $a=$oldMap[$path];$b=$nowMap[$path];if([Int64]$a.sizeBytes -ne [Int64]$b.sizeBytes -or [string]$a.sha256 -cne [string]$b.sha256){throw "A P9-12 inventoried file changed: $path"}
    }
    $added=@($nowMap.Keys | Where-Object {-not $oldMap.ContainsKey($_)} | ForEach-Object {$_.Replace('\','/')})
    foreach($path in $ExpectedNewScriptPaths){if(-not $added.Contains($path)){throw "P9-13 source inventory is missing its assigned script: $path"}}
    foreach($path in $added){if($path -notmatch '^scripts/phase9/[a-zA-Z0-9_\-]+\.(ps1|py)$'){throw "Unexpected non-harness source delta after P9-12: $path"}}
    $allowed=@($priorReceipt.source.allowedPaths)+$added
    $allowedSet=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase);foreach($path in $allowed){[void]$allowedSet.Add([string]$path)}
    $productRows=@($sourceInventory.files | Where-Object {-not $allowedSet.Contains([string]$_.path)})
    $productSha=Get-InventoryFingerprint $productRows
    if($productSha -cne [string]$priorReceipt.source.productSourceSha256){throw 'Current product source is not equivalent to the accepted P9-12 product source.'}
    Write-JsonFile $SourceInventoryPath $sourceInventory

    $priorManifestHash=Get-FileSha256 $PriorRunManifestPath;$priorReceiptHash=Get-FileSha256 $priorReceiptPath
    $derivation=[ordered]@{
        schema='classmngr-phase9-source-equivalence-derivation-v1';status='verified-script-only-derivation';taskId=$TaskId
        priorRunManifestPath=$PriorRunManifestPath;priorRunManifestSha256=$priorManifestHash
        priorDerivedReceiptPath=[IO.Path]::GetFullPath($priorReceiptPath);priorDerivedReceiptSha256=$priorReceiptHash
        priorReleaseBuildReceiptPath=$releaseReceiptPath;priorReleaseBuildReceiptSha256=(Get-FileSha256 $releaseReceiptPath)
        packageInventorySha256=$packageInventory.sha256;resourceInventorySha256=$resourceInventory.sha256
        sourcePackageRoot=$sourcePackageRoot;sourcePackageUnlistedOutputs=$unlisted;reconstructedPackageCopyRoot=$packageRoot
        source=[ordered]@{
            repositoryRoot=$RepositoryRoot;currentSourceSha256=$sourceInventory.sha256
            productSourceSha256=$productSha;buildSourceSha256=[string]$priorReceipt.source.buildSourceSha256
            allowedPaths=@($allowed | Sort-Object -Unique);addedP913Scripts=@($ExpectedNewScriptPaths)
            capturedHarnessAdditions=@($added | Sort-Object);productSourceEquivalent=$true;freshExactTreeRebuild=$false
        }
        statement='The preserved x64 Release package is product-source-equivalent to the accepted P9-12 package at the captured source inventory; this is not a fresh exact-tree build.'
    }
    $derivationPath=Join-Path (Join-Path $EvidenceDirectory 'build-derivation') 'derived-receipt.json';Write-JsonFile $derivationPath $derivation
    $script:Manifest.source=[ordered]@{
        repositoryRoot=$RepositoryRoot;includedRoots=@($prior.source.includedRoots);sha256=$sourceInventory.sha256
        fileCount=$sourceInventory.fileCount;inventoryPath='source-files.json';inventoryFileSha256=(Get-FileSha256 $SourceInventoryPath)
        derivedReceiptPath='build-derivation/derived-receipt.json';derivedReceiptSha256=(Get-FileSha256 $derivationPath)
        productSourceSha256=$productSha;equivalenceClaim='product-source-equivalent-to-P9-12-preserved-Release-package; not a fresh exact-tree build'
    }
    $script:Manifest.buildProvenance=[ordered]@{
        reusedPackageFromTaskId=[string]$prior.taskId;priorRunId=[string]$prior.runId
        priorRunManifestPath=$PriorRunManifestPath;priorRunManifestSha256=$priorManifestHash
        priorBuildSourceSha256=[string]$priorReceipt.source.buildSourceSha256;priorProductSourceSha256=[string]$priorReceipt.source.productSourceSha256
        configuration='Release';platform='x64';generator=[string]$prior.buildProvenance.generator
        cachePath=[IO.Path]::GetFullPath($cachePath);cacheSha256Current=$cacheHash;cacheSha256After=[string]$prior.buildProvenance.cacheSha256After
        freshExactTreeRebuild=$false
    }
    $script:Manifest.releaseBuildReceipt=[ordered]@{
        path=$releaseReceiptPath;sha256=(Get-FileSha256 $releaseReceiptPath);buildSourceSha256=[string]$priorReceipt.source.buildSourceSha256
        releaseExecutableSha256=[string]$releaseReceipt.releaseExecutableSha256
        generatedResourcesSha256=[string]$releaseReceipt.generatedResources.stagedResourceInputs.sha256;stagedResourceRoot=$resourceRoot
    }
    $script:Manifest.package=[ordered]@{
        root=$packageRoot;sha256=$packageInventory.sha256;fileCount=$packageInventory.fileCount
        inventoryPath='package-files.json';inventoryFileSha256=(Get-FileSha256 $PackageInventoryPath)
        priorPackageSha256=[string]$prior.package.sha256;sourceRunManifestPath=$PriorRunManifestPath;sourceRunManifestSha256=$priorManifestHash
        originalPackagePath=$sourcePackageRoot;originalPackageUnlistedOutputs=$unlisted;reconstructedFromRecordedInventory=$true
        resourceRoot=$resourceRoot;resourceInventoryPath='resource-files.json';resourceInventorySha256=$resourceInventory.sha256
        resourceInventoryFileSha256=(Get-FileSha256 $ResourceInventoryPath)
    }
    $script:Manifest.application=[ordered]@{
        path=$appPath;sha256=$appHash;architecture='x64';sourceEquivalence='P9-12 preserved ClassMngrNext.exe; product-source-equivalent package, not freshly rebuilt'
    }
    Write-JsonFile $ManifestPath $script:Manifest

    $seedSqlPath=Join-Path $RepositoryRoot 'tests\fixtures\workspaces\large_startup.sql'
    $seedHash=Get-FileSha256 $seedSqlPath
    $seedRows=@($sourceInventory.files | Where-Object {[string]$_.path -ceq 'tests/fixtures/workspaces/large_startup.sql'})
    if($seedRows.Count -ne 1 -or [string]$seedRows[0].sha256 -cne $seedHash){throw 'Captured source inventory does not bind the current large_startup.sql fixture.'}
    $script:FixtureContext=[ordered]@{seedSqlPath=$seedSqlPath;seedSqlSha256=$seedHash;blankWorkspacePath=$null;blankWorkspaceSha256=$null;priorP911ManifestPath=[string]$prior.buildProvenance.priorRunManifestPath}
    if(-not(Test-Path -LiteralPath $script:FixtureContext.priorP911ManifestPath -PathType Leaf)){throw 'P9-12 source-equivalence ancestry does not include the accepted P9-11 manifest.'}
    $p911=Read-JsonFile $script:FixtureContext.priorP911ManifestPath
    if($p911.taskId -cne 'P9_11_NEXT_CLASS_TRANSFER_PACKAGED_LIFECYCLE' -or $p911.status -cne 'passed'){throw 'P9-11 workspace base is not attached to the accepted class-transfer run.'}
    $script:FixtureContext.blankWorkspacePath=[IO.Path]::GetFullPath([string]$p911.masterWorkspace.path)
    $script:FixtureContext.blankWorkspaceSha256=[string]$p911.masterWorkspace.sha256
    if((Get-FileSha256 $script:FixtureContext.blankWorkspacePath) -cne $script:FixtureContext.blankWorkspaceSha256 -or $seedHash -cne [string]$p911.masterWorkspace.seedSqlSha256){throw 'P9-11 blank workspace or large_startup.sql identity differs from its accepted manifest.'}
    $script:Manifest.fixture=[ordered]@{
        baseWorkspacePath=$script:FixtureContext.blankWorkspacePath;baseWorkspaceSha256=$script:FixtureContext.blankWorkspaceSha256
        seedSqlPath=$seedSqlPath;seedSqlSha256=$seedHash;seedIdentityFromP911Manifest=$script:FixtureContext.priorP911ManifestPath
        workspacePreparationScript='scripts/phase9/prepare_speaking_evaluation_workspace.py'
        selectedEvaluationId=1;selectedClassId=1;selectedClassName='Heavy Fixture Class 001';selectedEvaluationName='Winter';seedEvaluationNameBeforeUiMapping='Heavy Term 01'
        expectedStudentCount=$ExpectedWorkspaceStudentCount
        expectedReportCount=$ExpectedReportCount
        expectedSourceRowLabels=@(1..$ExpectedWorkspaceStudentCount | ForEach-Object {"Student {0:d3}" -f $_})
        expectedWorkspaceReportNames=@(1..$ExpectedWorkspaceStudentCount | ForEach-Object {"Heavy Eval 01 (Score {0:d2})" -f $_})
        expectedReportNames=@(1..$ExpectedReportCount | ForEach-Object {"Heavy Eval 01 (Score {0:d2})" -f $_})
    }
}

function Prepare-Workspace {
    $workspacePath=Join-Path (Join-Path $EvidenceDirectory 'workspace') 'large-startup-speaking-evaluation.tps'
    $receiptPath=Join-Path (Join-Path $EvidenceDirectory 'workspace') 'preparation-receipt.json'
    $preparer=Join-Path $PSScriptRoot 'prepare_speaking_evaluation_workspace.py'
    $arguments=@($preparer,'--base-workspace',$script:FixtureContext.blankWorkspacePath,'--seed-sql',$script:FixtureContext.seedSqlPath,'--output',$workspacePath,'--receipt',$receiptPath,'--expected-base-sha256',$script:FixtureContext.blankWorkspaceSha256,'--expected-seed-sha256',$script:FixtureContext.seedSqlSha256)
    $outputText=& python @arguments 2>&1;$exitCode=$LASTEXITCODE
    [IO.File]::WriteAllText((Join-Path (Join-Path $EvidenceDirectory 'logs') 'workspace-preparation-output.txt'),([string]::Join([Environment]::NewLine,@($outputText))+[Environment]::NewLine),[Text.UTF8Encoding]::new($false))
    if($exitCode -ne 0){throw "Speaking-evaluation workspace preparation failed with exit code $exitCode."}
    $receipt=Read-JsonFile $receiptPath
    $nameMapping=$receipt.evaluationNameUiMapping
    if($receipt.schema -cne 'classmngr-phase9-speaking-evaluation-workspace-preparation-v2' -or $receipt.status -cne 'passed' -or [int]$receipt.selectedEvaluation.studentCount -ne $ExpectedWorkspaceStudentCount){throw 'Workspace preparation receipt did not bind the selected 30-student evaluation.'}
    if($nameMapping.table -cne 'speaking_evaluations' -or $nameMapping.column -cne 'evaluation_name' -or [int]$nameMapping.evaluationId -ne 1 -or [int]$nameMapping.classId -ne 1 -or $nameMapping.beforeValue -cne 'Heavy Term 01' -or $nameMapping.afterValue -cne 'Winter' -or [int]$nameMapping.updateRowCount -ne 1 -or -not [bool]$nameMapping.onlySelectedEvaluationNameCellChanged -or [string]$nameMapping.workspaceSha256AfterMapping -cne [string]$receipt.workspaceSha256){throw 'Workspace receipt does not prove the approved single-cell evaluation-name mapping.'}
    $script:Manifest.workspace=[ordered]@{
        path=[IO.Path]::GetFullPath($workspacePath);bytes=[Int64](Get-Item -LiteralPath $workspacePath).Length;sha256=(Get-FileSha256 $workspacePath)
        preparationReceiptPath='workspace/preparation-receipt.json';preparationReceiptSha256=(Get-FileSha256 $receiptPath)
        counts=$receipt.counts;integrityCheck=$receipt.integrityCheck;foreignKeyErrors=$receipt.foreignKeyErrors
        selectedEvaluation=$receipt.selectedEvaluation;evaluationNameUiMapping=$receipt.evaluationNameUiMapping;speakingTableSha256=$receipt.speakingTableSha256
    }
    $script:WorkspaceReceipt=$receipt
    $script:ExpectedCurrentReportName=[string]$receipt.selectedEvaluation.reportNames[0]
    Write-JsonFile $ManifestPath $script:Manifest
    return $workspacePath
}

function Get-PeMachine {
    param([string]$Path)
    $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try{$reader=[IO.BinaryReader]::new($stream);$stream.Position=0x3c;$offset=$reader.ReadInt32();if($offset -lt 0 -or $offset -gt ($stream.Length-6)){throw 'PE header offset is outside the executable.'};$stream.Position=$offset+4;return [Int32]$reader.ReadUInt16()}
    finally{$stream.Dispose()}
}
function Start-TempWatcher {
    param([string]$Root)
    $script:Watcher=[IO.FileSystemWatcher]::new($Root,'*')
    $script:Watcher.IncludeSubdirectories=$false;$script:Watcher.NotifyFilter=[IO.NotifyFilters]::DirectoryName -bor [IO.NotifyFilters]::FileName
    $createdId="$RunId-speaking-temp-created";$deletedId="$RunId-speaking-temp-deleted"
    Register-ObjectEvent -InputObject $script:Watcher -EventName Created -SourceIdentifier $createdId | Out-Null
    Register-ObjectEvent -InputObject $script:Watcher -EventName Deleted -SourceIdentifier $deletedId | Out-Null
    $script:EventSourceIds=@($createdId,$deletedId);$script:Watcher.EnableRaisingEvents=$true
}
function Stop-TempWatcher {
    if($null -eq $script:Watcher){return}
    $script:Watcher.EnableRaisingEvents=$false;Start-Sleep -Milliseconds 150
    $events=@(Get-Event | Where-Object {$script:EventSourceIds -contains $_.SourceIdentifier})
    foreach($event in $events){
        $kind=if($event.SourceIdentifier -ceq $script:EventSourceIds[0]){'created'}else{'deleted'}
        $fullPath=[string]$event.SourceEventArgs.FullPath
        if([IO.Path]::GetFileName($fullPath).StartsWith('ClassMngr-speaking-evaluations-',[StringComparison]::OrdinalIgnoreCase)){
            [IO.File]::AppendAllText($WatcherPath,($kind+"`t"+$fullPath+"`t"+(Get-UtcTimestamp)+[Environment]::NewLine),[Text.UTF8Encoding]::new($false))
        }
        Remove-Event -EventIdentifier $event.EventIdentifier
    }
    foreach($id in $script:EventSourceIds){Unregister-Event -SourceIdentifier $id -ErrorAction SilentlyContinue}
    $script:Watcher.Dispose();$script:Watcher=$null
}

function Start-PackagedProcess {
    param([string]$WorkspacePath)
    $runtime=Join-Path $EvidenceDirectory 'runtime'
    foreach($name in @('settings','local-app-data','app-data','temp','diagnostics')){[void][IO.Directory]::CreateDirectory((Join-Path $runtime $name))}
    $settingsRoot=Join-Path $runtime 'settings'
    $settingsBefore=Get-CanonicalInventory $settingsRoot
    if($settingsBefore.fileCount -ne 0){throw 'Isolated settings root unexpectedly contains persisted preferences before packaged app startup.'}
    $settingsBeforePath=Join-Path $runtime 'settings-before-launch-inventory.json'
    Write-JsonFile $settingsBeforePath $settingsBefore
    $script:Manifest.runtimeSettingsProvenance=[ordered]@{
        rootRelativePath='runtime/settings';beforeLaunchInventoryPath='runtime/settings-before-launch-inventory.json'
        beforeLaunchInventorySha256=(Get-FileSha256 $settingsBeforePath);beforeLaunchFileCount=[Int64]$settingsBefore.fileCount
        beforeLaunchRootSha256=[string]$settingsBefore.sha256;maximumRosterRowsKey='options/maximumRosterRows'
        configuredMaximumRosterRows=$null;effectiveDefaultMaximumRosterRows=25
        defaultPolicySource='src/next/application/roster_row_display_policy.h';afterExitInventoryPath=$null
        afterExitInventorySha256=$null;afterExitFileCount=$null;afterExitRootSha256=$null
    }
    Write-JsonFile $ManifestPath $script:Manifest
    [IO.File]::WriteAllText($WatcherPath,'',[Text.UTF8Encoding]::new($false))
    $tracePath=Join-Path (Join-Path $runtime 'diagnostics') 'application-shutdown.jsonl'
    $stdoutPath=Join-Path (Join-Path $EvidenceDirectory 'logs') 'process-stdout.log';$stderrPath=Join-Path (Join-Path $EvidenceDirectory 'logs') 'process-stderr.log'
    [IO.File]::WriteAllText($stdoutPath,'',[Text.UTF8Encoding]::new($false));[IO.File]::WriteAllText($stderrPath,'',[Text.UTF8Encoding]::new($false))
    $runtimeTemp=Join-Path $runtime 'temp'
    $envMap=[ordered]@{
        CLASSMNGR_SETTINGS_ROOT=(Join-Path $runtime 'settings');CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH=[IO.Path]::GetFullPath($tracePath)
        LOCALAPPDATA=(Join-Path $runtime 'local-app-data');APPDATA=(Join-Path $runtime 'app-data')
        TEMP=$runtimeTemp;TMP=$runtimeTemp;QT_FORCE_STDERR_LOGGING='1';PATH="$($script:Manifest.package.root);$env:PATH"
    }
    $oldEnvironment=[ordered]@{}
    foreach($pair in $envMap.GetEnumerator()){$oldEnvironment[[string]$pair.Key]=[Environment]::GetEnvironmentVariable([string]$pair.Key,'Process');[Environment]::SetEnvironmentVariable([string]$pair.Key,[string]$pair.Value,'Process')}
    try{$process=Start-Process -FilePath ([string]$script:Manifest.application.path) -WorkingDirectory ([string]$script:Manifest.package.root) -ArgumentList ('"'+[IO.Path]::GetFullPath($WorkspacePath)+'"') -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -PassThru}
    finally{foreach($pair in $oldEnvironment.GetEnumerator()){[Environment]::SetEnvironmentVariable([string]$pair.Key,$pair.Value,'Process')}}
    if($null -eq $process){throw 'Unable to start the packaged Next process.'}
    $observedPath=[IO.Path]::GetFullPath([string]$process.MainModule.FileName)
    if($observedPath -cne [IO.Path]::GetFullPath([string]$script:Manifest.application.path) -or (Get-FileSha256 $observedPath) -cne [string]$script:Manifest.application.sha256 -or (Get-PeMachine $observedPath) -ne 0x8664){throw 'Live app path/hash/architecture differs from the measured packaged x64 Release app.'}
    $script:Context=[ordered]@{
        process=$process;stdoutPath=$stdoutPath;stderrPath=$stderrPath;seenOutputLines=[ordered]@{stdout=0;stderr=0}
        pageRouteEvents=[Collections.Generic.List[object]]::new();lifecycleRecords=[Collections.Generic.List[object]]::new()
        uiaElementOwners=[Collections.Generic.Dictionary[string,Int64]]::new([StringComparer]::Ordinal);uiaQueryMetrics=$script:Manifest.uiaQueryMetrics
        arrival=0;mainWindowHandle=0L;mainWindowTitle='';mainUiaRoot=$null;forcedTermination=$false;processExit=$null
    }
    $script:Manifest.process=[ordered]@{
        processId=[Int64]$process.Id;processImagePath=$observedPath;processImageSha256=(Get-FileSha256 $observedPath);architecture='x64'
        startedAtUtc=$process.StartTime.ToUniversalTime().ToString('o')
        commandLine=[ordered]@{executable=[string]$script:Manifest.application.path;workspaceArgument=[IO.Path]::GetFullPath($WorkspacePath)}
        workspacePath=[IO.Path]::GetFullPath($WorkspacePath);stdoutPath='logs/process-stdout.log';stderrPath='logs/process-stderr.log'
        shutdownTracePath='runtime/diagnostics/application-shutdown.jsonl';mainWindowHandle=$null;windowProcessId=$null;windowHandleProcessId=$null
    }
    Start-TempWatcher $runtimeTemp
    return $script:Context
}

function Drain-ProcessOutput {
    param([object]$Context)
    foreach($stream in @(@{name='stdout';path=$Context.stdoutPath},@{name='stderr';path=$Context.stderrPath})){
        if(-not(Test-Path -LiteralPath $stream.path)){continue}
        $lines=@(Get-Content -LiteralPath $stream.path -Encoding UTF8)
        for($index=[int]$Context.seenOutputLines[$stream.name];$index -lt $lines.Count;$index++){$line=$lines[$index];$Context.arrival++
            $arrival=[Int64]$Context.arrival
            Add-JsonLine $StartupLogPath ([ordered]@{arrivalIndex=$arrival;stream=$stream.name;observedAtUtc=Get-UtcTimestamp;line=[string]$line})
            if([string]$line -match '^ClassMngrNext navigation event=page-route outcome=completed cause=[^\s]+ from=[^\s]+ to=([^\s]+)(?:\s|$)'){
                $Context.pageRouteEvents.Add([ordered]@{arrivalIndex=$arrival;stream=$stream.name;line=[string]$line;targetPageId=[string]$Matches[1]})
            }
            if([string]$line -match '^ClassMngr lifecycle checkpoint=(\{.*\})$'){
                try{$record=$Matches[1] | ConvertFrom-Json -ErrorAction Stop;$Context.lifecycleRecords.Add([pscustomobject]@{arrivalIndex=[Int64]$Context.arrival;stream=$stream.name;line=[string]$line;record=$record})}
                catch{Add-JsonLine (Join-Path (Join-Path $EvidenceDirectory 'logs') 'lifecycle-parse-errors.jsonl') ([ordered]@{arrivalIndex=$Context.arrival;line=$line;error=$_.Exception.Message})}
            }
        }
        $Context.seenOutputLines[$stream.name]=$lines.Count
    }
}
function Test-ExpectedUiaAvailabilityRace {
    param([Exception]$Exception)
    $candidate=$Exception
    while($null -ne $candidate){
        if($candidate -is [System.Windows.Automation.ElementNotAvailableException]){return $true}
        if($candidate -is [Runtime.InteropServices.COMException] -and [UInt32]$candidate.ErrorCode -eq [UInt32]0x80040201){return $true}
        $candidate=$candidate.InnerException
    }
    return $false
}
function Get-UiaControlTypeValue {
    param([string]$ProgrammaticName)
    switch($ProgrammaticName){
        'ControlType.Window' {return [System.Windows.Automation.ControlType]::Window}
        'ControlType.Tree' {return [System.Windows.Automation.ControlType]::Tree}
        'ControlType.TreeItem' {return [System.Windows.Automation.ControlType]::TreeItem}
        'ControlType.List' {return [System.Windows.Automation.ControlType]::List}
        'ControlType.ListItem' {return [System.Windows.Automation.ControlType]::ListItem}
        'ControlType.ComboBox' {return [System.Windows.Automation.ControlType]::ComboBox}
        'ControlType.Button' {return [System.Windows.Automation.ControlType]::Button}
        'ControlType.Edit' {return [System.Windows.Automation.ControlType]::Edit}
        'ControlType.CheckBox' {return [System.Windows.Automation.ControlType]::CheckBox}
        'ControlType.Group' {return [System.Windows.Automation.ControlType]::Group}
        'ControlType.Text' {return [System.Windows.Automation.ControlType]::Text}
        default {throw "Unsupported bounded UIA ControlType '$ProgrammaticName'."}
    }
}
function Get-UiaRuntimeKey {
    param([System.Windows.Automation.AutomationElement]$Element)
    $parts=@($Element.GetRuntimeId() | ForEach-Object {[string]$_})
    if($parts.Count -eq 0){throw 'UIA target has no runtime identity.'}
    [string]::Join(',',([string[]]$parts))
}
function Get-VerifiedMainUiaRoot {
    param([object]$Context)
    $processId=[uint32]$Context.process.Id;$handle=[IntPtr]$Context.mainWindowHandle
    if($handle -eq [IntPtr]::Zero -or [uint32][P913ScreenshotNative]::GetWindowProcessId($handle) -ne $processId){throw 'Measured main-window HWND is not bound to the packaged PID.'}
    $lastRace=$null
    for($attempt=1;$attempt -le 4;$attempt++){
        try{$root=[System.Windows.Automation.AutomationElement]::FromHandle($handle);$current=$root.Current}
        catch{if(-not(Test-ExpectedUiaAvailabilityRace $_.Exception)){throw};$lastRace=$_.Exception;if($attempt -lt 4){Start-Sleep -Milliseconds 75};continue}
        if([uint32]$current.ProcessId -ne $processId -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.Window' -or [string]$current.AutomationId -cne [string]$script:UiaCatalog.mainWindow.automationId){throw 'Verified main-window UIA root identity differs from the measured PID/HWND.'}
        $Context.mainUiaRoot=$root
        return [pscustomobject]@{element=$root;handle=[Int64]$Context.mainWindowHandle}
    }
    throw "Verified main-window UIA root remained unavailable after three bounded retries: $($lastRace.Message)"
}
function Get-VerifiedDialogUiaRoot {
    param([object]$Context,[string]$ContainerAutomationId)
    $processId=[uint32]$Context.process.Id;$mainHandle=[Int64]$Context.mainWindowHandle
    $rootSpec=$null
    foreach($candidateSpec in $script:UiaCatalog.Values){if($candidateSpec.ContainsKey('isRoot') -and [bool]$candidateSpec['isRoot'] -and [string]$candidateSpec.automationId -ceq $ContainerAutomationId){$rootSpec=$candidateSpec;break}}
    if($null -eq $rootSpec -or $ContainerAutomationId -ceq $script:UiaCatalog.mainWindow.automationId){throw "UIA dialog container '$ContainerAutomationId' is not an approved top-level dialog."}
    foreach($handleValue in [P913ScreenshotNative]::GetTopLevelWindows($processId)){
        $handle=[IntPtr]$handleValue
        if([Int64]$handle -eq $mainHandle){continue}
        if([uint32][P913ScreenshotNative]::GetWindowProcessId($handle) -ne $processId){throw 'Enumerated dialog HWND changed process ownership.'}
        if(-not [P913ScreenshotNative]::IsVisibleWindow($handle)){continue}
        if([P913ScreenshotNative]::GetWindowCaption($handle,$processId) -cne [string]$rootSpec.name){continue}
        $lastRace=$null;$root=$null
        for($attempt=1;$attempt -le 4;$attempt++){
            try{$candidateRoot=[System.Windows.Automation.AutomationElement]::FromHandle($handle);$current=$candidateRoot.Current}
            catch{if(-not(Test-ExpectedUiaAvailabilityRace $_.Exception)){throw};$lastRace=$_.Exception;if($attempt -lt 4){Start-Sleep -Milliseconds 75};continue}
            if([uint32]$current.ProcessId -eq $processId -and [string]$current.ControlType.ProgrammaticName -ceq 'ControlType.Window' -and [string]$current.AutomationId -ceq $ContainerAutomationId -and [string]$current.Name -ceq [string]$rootSpec.name){$root=$candidateRoot;break}
            break
        }
        if($null -ne $root){return [pscustomobject]@{element=$root;handle=[Int64]$handle}}
        if($null -ne $lastRace -and $attempt -ge 4){throw "UIA top-level dialog HWND $([Int64]$handle) remained unavailable after three bounded retries: $($lastRace.Message)"}
    }
    return $null
}
function Get-UiaSearchRoot {
    param([object]$Context,[object]$Spec)
    if([string]$Spec.container -ceq 'main'){return (Get-VerifiedMainUiaRoot $Context)}
    return (Get-VerifiedDialogUiaRoot $Context ([string]$Spec.container))
}
function New-UiaTargetCondition {
    param([object]$Spec,[string]$Name)
    $conditions=[Collections.Generic.List[System.Windows.Automation.Condition]]::new()
    $conditions.Add([System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::AutomationIdProperty,[string]$Spec.automationId))
    $conditions.Add([System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty,(Get-UiaControlTypeValue ([string]$Spec.controlType))))
    if(-not [string]::IsNullOrEmpty($Name)){$conditions.Add([System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty,[string]$Name))}
    [System.Windows.Automation.AndCondition]::new([System.Windows.Automation.Condition[]]$conditions.ToArray())
}
function Add-UiaQueryMetric {
    param([object]$Context,[string]$SpecName,[string]$Container,[Int64]$RootHandle,[int]$RetryCount,[double]$ElapsedMilliseconds,[bool]$Found,[object]$Spec=$null,[object]$ExpectedName='', [object]$ObservedName=$null,[string]$QueryScope='Descendants',[string]$ParentAutomationId='', [bool]$PostQueryIdentityCheck=$false,[bool]$TargetEnabled=$false)
    $runnerProcess=[Diagnostics.Process]::GetCurrentProcess();$runnerProcess.Refresh();$runnerWorkingSet=[Int64]$runnerProcess.WorkingSet64
    $nameConditionApplied=-not [string]::IsNullOrEmpty([string]$ExpectedName)
    $conditionProperties=@('AutomationId','ControlType');if($nameConditionApplied){$conditionProperties+=@('Name')}
    $method='Top-level HWND enumeration filtered by measured PID and exact caption'
    if($RootHandle -gt 0){
        if($QueryScope -ceq 'Children'){$method='FindFirst immediate child with combined AutomationId + ControlType + Name conditions; post-query PID/owner/enabled check'}
        elseif($nameConditionApplied){$method='Exact root identity or FindFirst with combined AutomationId + ControlType + Name conditions'}
        else{$method='FindFirst with combined AutomationId + ControlType conditions; post-query Name/PID/owner/enabled check'}
    }
    $record=[ordered]@{
        selector=$SpecName;container=$Container;rootWindowHandle=$RootHandle
        rootWindowProcessId=if($RootHandle -gt 0){[Int64][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$RootHandle)}else{0}
        processId=[Int64]$Context.process.Id
        method=$method;queryScope=$QueryScope;parentAutomationId=$ParentAutomationId
        conditionProperties=$conditionProperties;nameConditionApplied=$nameConditionApplied
        expectedName=[string]$ExpectedName;observedName=if($null -eq $ObservedName){$null}else{[string]$ObservedName}
        postQueryIdentityCheck=$PostQueryIdentityCheck;targetEnabled=$TargetEnabled
        ancestorWindowHandle=$RootHandle;ancestorWindowProcessId=if($RootHandle -gt 0){[Int64][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$RootHandle)}else{0}
        attemptCount=$RetryCount;retryCount=[Math]::Max(0,$RetryCount-1)
        elapsedMilliseconds=[Math]::Round($ElapsedMilliseconds,3);resultCount=if($Found){1}else{0};materializedTreeElementCount=0
        runnerProcessId=[Int64]$runnerProcess.Id;runnerWorkingSetBytes=$runnerWorkingSet;runnerWorkingSetGuardBytesExclusive=[Int64]805306368
        capturedAtUtc=Get-UtcTimestamp
    }
    $Context.uiaQueryMetrics.Add($record)
    if($runnerWorkingSet -ge [Int64]805306368){throw "PowerShell harness working set reached the 768 MiB safety guard after UIA query '$SpecName': $runnerWorkingSet bytes."}
}
function Find-UiaTarget {
    param([object]$Context,[string]$SpecName,[object]$RequestedName)
    if(-not $script:UiaCatalog.ContainsKey($SpecName)){throw "No bounded UIA selector is registered for '$SpecName'."}
    if($SpecName -in @('speakingEvaluationNavigationItem','myWorkspaceNavigationItem')){
        $navigationTree=Find-UiaTarget $Context 'classMngrNextNavigationTree' $null
        if($null -eq $navigationTree){return $null}
        $navigationName=if($null -eq $RequestedName){[string]$script:UiaCatalog[$SpecName].name}else{[string]$RequestedName}
        return Find-UiaChildByName $Context $navigationTree $SpecName $navigationName
    }
    $spec=$script:UiaCatalog[$SpecName];$exactName=if($null -eq $RequestedName){[string]$spec.name}else{$RequestedName};$timer=[Diagnostics.Stopwatch]::StartNew()
    $rootInfo=Get-UiaSearchRoot $Context $spec
    if($null -eq $rootInfo){$timer.Stop();Add-UiaQueryMetric $Context $SpecName ([string]$spec.container) 0 0 $timer.Elapsed.TotalMilliseconds $false $spec $exactName;return $null}
    $condition=New-UiaTargetCondition $spec $exactName;$lastRace=$null
    for($attempt=1;$attempt -le 4;$attempt++){
        try{
            $rootCurrent=$rootInfo.element.Current
            $target=$null
            if([string]$rootCurrent.AutomationId -ceq [string]$spec.automationId -and [string]$rootCurrent.ControlType.ProgrammaticName -ceq [string]$spec.controlType -and [string]$rootCurrent.Name -ceq $exactName){$target=$rootInfo.element}
            else{$target=$rootInfo.element.FindFirst([System.Windows.Automation.TreeScope]::Descendants,$condition)}
            if($null -eq $target){$timer.Stop();Add-UiaQueryMetric $Context $SpecName ([string]$spec.container) ([Int64]$rootInfo.handle) $attempt $timer.Elapsed.TotalMilliseconds $false $spec $exactName;return $null}
            $targetCurrent=$target.Current
            $blankNameQuery=[string]::IsNullOrEmpty([string]$exactName)
            if([uint32]$targetCurrent.ProcessId -ne [uint32]$Context.process.Id -or [string]$targetCurrent.AutomationId -cne [string]$spec.automationId -or [string]$targetCurrent.ControlType.ProgrammaticName -cne [string]$spec.controlType -or [string]$targetCurrent.Name -cne [string]$exactName){throw "UIA FindFirst returned an element outside the post-query identity checks for '$SpecName'."}
            if($blankNameQuery -and -not [bool]$targetCurrent.IsEnabled){throw "Blank-Name UIA target '$SpecName' was not enabled after the bounded FindFirst query."}
            if([Int64]$rootInfo.handle -le 0 -or [uint32][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$rootInfo.handle) -ne [uint32]$Context.process.Id){throw "UIA target '$SpecName' has no verified PID-bound HWND ancestor."}
            $runtimeKey=Get-UiaRuntimeKey $target
            $Context.uiaElementOwners[$runtimeKey]=[Int64]$rootInfo.handle
            if($spec.ContainsKey('isRoot') -and [bool]$spec['isRoot']){$Context.uiaElementOwners[$runtimeKey]=[Int64]$rootInfo.handle}
            $timer.Stop();Add-UiaQueryMetric $Context $SpecName ([string]$spec.container) ([Int64]$rootInfo.handle) $attempt $timer.Elapsed.TotalMilliseconds $true $spec $exactName ([string]$targetCurrent.Name) 'Descendants' '' $true ([bool]$targetCurrent.IsEnabled)
            return $target
        }
        catch{
            if(-not(Test-ExpectedUiaAvailabilityRace $_.Exception)){throw}
            $lastRace=$_.Exception
            if($attempt -lt 4){Start-Sleep -Milliseconds 75}
        }
    }
    $timer.Stop();Add-UiaQueryMetric $Context $SpecName ([string]$spec.container) ([Int64]$rootInfo.handle) 4 $timer.Elapsed.TotalMilliseconds $false $spec $exactName
    throw "UIA FindFirst for '$SpecName' remained unavailable after three bounded retries: $($lastRace.Message)"
}
function Get-UiaSnapshot {
    param([System.Windows.Automation.AutomationElement]$Element)
    $c=$Element.Current;$r=$c.BoundingRectangle;$value=$null;$selected=$null;$toggle=$null
    try{$pattern=[System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern);$value=[string]$pattern.Current.Value}catch{}
    try{$pattern=[System.Windows.Automation.SelectionPattern]$Element.GetCurrentPattern([System.Windows.Automation.SelectionPattern]::Pattern);$items=$pattern.Current.GetSelection();if($items.Count -gt 0){$selected=[string]$items[0].Current.Name}}catch{}
    try{$pattern=[System.Windows.Automation.TogglePattern]$Element.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern);$toggle=[int]$pattern.Current.ToggleState}catch{}
    [ordered]@{
        name=[string]$c.Name;automationId=[string]$c.AutomationId;controlType=[string]$c.ControlType.ProgrammaticName
        className=[string]$c.ClassName;frameworkId=[string]$c.FrameworkId;processId=[Int64]$c.ProcessId
        nativeWindowHandle=[Int64]$c.NativeWindowHandle;isOffscreen=[bool]$c.IsOffscreen;isEnabled=[bool]$c.IsEnabled
        isKeyboardFocusable=[bool]$c.IsKeyboardFocusable;value=$value;selectedItem=$selected;toggleState=$toggle
        rectangle=[ordered]@{x=[double]$r.X;y=[double]$r.Y;width=[double]$r.Width;height=[double]$r.Height}
    }
}
function Get-OptionalUiaById {
    param([object]$Context,[string]$AutomationId,[string]$ControlType='')
    if(-not $script:UiaCatalog.ContainsKey($AutomationId)){throw "No bounded UIA selector is registered for AutomationId suffix '$AutomationId'."}
    $spec=$script:UiaCatalog[$AutomationId]
    if(-not [string]::IsNullOrEmpty($ControlType) -and [string]$spec.controlType -cne $ControlType){throw "Requested ControlType '$ControlType' conflicts with registered selector '$AutomationId'."}
    Find-UiaTarget $Context $AutomationId $null
}
function Get-UiaById {
    param([object]$Context,[string]$AutomationId,[string]$ControlType='')
    $element=Get-OptionalUiaById $Context $AutomationId $ControlType
    if($null -eq $element){throw "UIA did not find process-bound AutomationId '$AutomationId' type '$ControlType' with its registered Name condition."}
    return $element
}
function Get-UiaNameSelector {
    param([string]$Name,[string]$ControlType)
    if($ControlType -ceq 'ControlType.TreeItem' -and $Name -ceq 'Speaking Evaluation'){return 'speakingEvaluationNavigationItem'}
    if($ControlType -ceq 'ControlType.TreeItem' -and $Name -ceq 'My Workspace'){return 'myWorkspaceNavigationItem'}
    if($ControlType -ceq 'ControlType.ListItem' -and $Name -ceq 'Heavy Fixture Class 001'){return 'speakingEvaluationClassItem'}
    if($ControlType -ceq 'ControlType.Window' -and $Name -ceq 'Reports Ready'){return 'reportsReadyDialog'}
    throw "No bounded UIA name selector is registered for '$Name' type '$ControlType'."
}
function Get-OptionalUiaByName {
    param([object]$Context,[string]$Name,[string]$ControlType)
    $selector=Get-UiaNameSelector $Name $ControlType
    Find-UiaTarget $Context $selector $Name
}
function Get-UiaByName {
    param([object]$Context,[string]$Name,[string]$ControlType='')
    $element=Get-OptionalUiaByName $Context $Name $ControlType
    if($null -eq $element){throw "UIA did not find process-bound element named '$Name' type '$ControlType'."}
    return $element
}
function Find-UiaChildByName {
    param([object]$Context,[System.Windows.Automation.AutomationElement]$Parent,[string]$SpecName,[string]$Name)
    if(-not $script:UiaCatalog.ContainsKey($SpecName)){throw "No bounded UIA selector is registered for '$SpecName'."}
    $spec=$script:UiaCatalog[$SpecName];$timer=[Diagnostics.Stopwatch]::StartNew();$lastRace=$null
    $parentCurrent=$Parent.Current;$parentRuntimeKey=Get-UiaRuntimeKey $Parent
    $parentSpec=if($SpecName -ceq 'speakingEvaluationClassItem'){$script:UiaCatalog.speakingEvaluationClassList}elseif($SpecName -in @('speakingEvaluationNavigationItem','myWorkspaceNavigationItem')){$script:UiaCatalog.classMngrNextNavigationTree}else{$null}
    if($null -eq $parentSpec){throw "Immediate-child selector '$SpecName' is not registered under an approved UIA parent."}
    if([uint32]$parentCurrent.ProcessId -ne [uint32]$Context.process.Id -or [string]$parentCurrent.AutomationId -cne [string]$parentSpec.automationId -or [string]$parentCurrent.ControlType.ProgrammaticName -cne [string]$parentSpec.controlType -or [string]$parentCurrent.Name -cne [string]$parentSpec.name -or -not [bool]$parentCurrent.IsEnabled){throw "Immediate-child parent '$($SpecName)' failed exact PID/AutomationId/ControlType/Name/enabled identity checks."}
    if(-not $Context.uiaElementOwners.ContainsKey($parentRuntimeKey)){throw "Immediate-child parent '$($SpecName)' has no verified HWND ancestor mapping."}
    $ancestorHandle=[Int64]$Context.uiaElementOwners[$parentRuntimeKey]
    if($ancestorHandle -ne [Int64]$Context.mainWindowHandle -or [uint32][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$ancestorHandle) -ne [uint32]$Context.process.Id){throw "Immediate-child parent '$($SpecName)' is not under the measured main HWND/PID."}
    $owner=Get-WindowForElement $Context $Parent
    if([Int64]$owner.Current.NativeWindowHandle -ne $ancestorHandle -or [uint32]$owner.Current.ProcessId -ne [uint32]$Context.process.Id){throw "Immediate-child parent '$($SpecName)' HWND ancestor changed during lookup."}
    if([string]$spec.controlType -cnotin @('ControlType.ListItem','ControlType.TreeItem') -or [string]$spec.automationId -cne [string]$parentCurrent.AutomationId -or [string]$spec.container -cne 'main'){throw "Immediate child '$($SpecName)' is not bound to its registered main-window list/tree parent."}
    $condition=New-UiaTargetCondition $spec $Name
    for($attempt=1;$attempt -le 4;$attempt++){
        try{
            $target=$Parent.FindFirst([System.Windows.Automation.TreeScope]::Children,$condition)
            if($null -eq $target){$timer.Stop();Add-UiaQueryMetric $Context $SpecName 'main' $ancestorHandle $attempt $timer.Elapsed.TotalMilliseconds $false $spec $Name $null 'Children' ([string]$parentCurrent.AutomationId) $false $false;return $null}
            $current=$target.Current
            if([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.AutomationId -cne [string]$spec.automationId -or [string]$current.ControlType.ProgrammaticName -cne [string]$spec.controlType -or [string]$current.Name -cne $Name -or -not [bool]$current.IsEnabled){throw "Immediate child '$($SpecName)' failed its exact Name/AutomationId/ControlType/PID/enabled checks."}
            if([uint32][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$ancestorHandle) -ne [uint32]$Context.process.Id){throw "Immediate child '$($SpecName)' HWND ancestor is no longer bound to the measured PID."}
            if([Int64]$current.NativeWindowHandle -gt 0 -and [uint32][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$current.NativeWindowHandle) -ne [uint32]$Context.process.Id){throw "Immediate child '$($SpecName)' exposes a native HWND owned by another process."}
            $runtimeKey=Get-UiaRuntimeKey $target;$Context.uiaElementOwners[$runtimeKey]=$ancestorHandle
            $timer.Stop();Add-UiaQueryMetric $Context $SpecName 'main' $ancestorHandle $attempt $timer.Elapsed.TotalMilliseconds $true $spec $Name ([string]$current.Name) 'Children' ([string]$parentCurrent.AutomationId) $true ([bool]$current.IsEnabled)
            return $target
        }
        catch{
            if(-not(Test-ExpectedUiaAvailabilityRace $_.Exception)){throw}
            $lastRace=$_.Exception
            if($attempt -lt 4){Start-Sleep -Milliseconds 75}
        }
    }
    $timer.Stop();Add-UiaQueryMetric $Context $SpecName 'main' $ancestorHandle 4 $timer.Elapsed.TotalMilliseconds $false $spec $Name $null 'Children' ([string]$parentCurrent.AutomationId) $false $false
    throw "Immediate child lookup for '$($SpecName)' remained unavailable after three bounded retries: $($lastRace.Message)"
}
function Get-ElementLabel {
    param([System.Windows.Automation.AutomationElement]$Element)
    $snapshot=Get-UiaSnapshot $Element
    if(-not [string]::IsNullOrWhiteSpace([string]$snapshot.selectedItem)){return [string]$snapshot.selectedItem}
    if(-not [string]::IsNullOrWhiteSpace([string]$snapshot.value)){return [string]$snapshot.value}
    return [string]$snapshot.name
}
function Wait-UiaById {
    param([object]$Context,[string]$AutomationId,[int]$TimeoutSeconds,[string]$Description,[string]$ControlType='')
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds){Drain-ProcessOutput $Context;$Context.process.Refresh();if($Context.process.HasExited){throw "Process exited while waiting for $Description."};$element=Get-OptionalUiaById $Context $AutomationId $ControlType;if($null -ne $element){return $element};Start-Sleep -Milliseconds 100}
    throw "Timed out waiting for UIA element $AutomationId ($Description)."
}
function Wait-UiaByIdAfterRoute {
    param([object]$Context,[string]$AutomationId,[string]$Description,[int]$MaximumAttempts=60,[int]$IntervalMilliseconds=250)
    if($MaximumAttempts -lt 1 -or $MaximumAttempts -gt 60 -or $IntervalMilliseconds -ne 250){throw 'Post-route UIA readiness polling must stay at or below 60 attempts with a 250 ms interval.'}
    $spec=$script:UiaCatalog[$AutomationId];if($null -eq $spec){throw "No bounded UIA selector is registered for '$AutomationId'."}
    $timer=[Diagnostics.Stopwatch]::StartNew();$runnerProcess=[Diagnostics.Process]::GetCurrentProcess();$runnerProcess.Refresh();$Context.process.Refresh()
    $runnerStart=[Int64]$runnerProcess.WorkingSet64;$appStart=[Int64]$Context.process.WorkingSet64;$runnerMax=$runnerStart;$appMax=$appStart
    $queryCountStart=@($Context.uiaQueryMetrics).Count;$element=$null;$attempts=0;$status='timeout'
    for($attempt=1;$attempt -le $MaximumAttempts;$attempt++){
        $attempts=$attempt;Drain-ProcessOutput $Context;$Context.process.Refresh()
        if($Context.process.HasExited){$status='process-exited';break}
        $element=Get-OptionalUiaById $Context $AutomationId ([string]$spec.controlType)
        $runnerProcess.Refresh();$Context.process.Refresh();$runnerMax=[Math]::Max($runnerMax,[Int64]$runnerProcess.WorkingSet64)
        if(-not $Context.process.HasExited){$appMax=[Math]::Max($appMax,[Int64]$Context.process.WorkingSet64)}
        if($null -ne $element){$status='found';break}
        if($attempt -lt $MaximumAttempts){Start-Sleep -Milliseconds $IntervalMilliseconds}
    }
    $timer.Stop();$runnerProcess.Refresh();$Context.process.Refresh()
    $runnerEnd=[Int64]$runnerProcess.WorkingSet64;$appEnd=if($Context.process.HasExited){0L}else{[Int64]$Context.process.WorkingSet64}
    $runnerMax=[Math]::Max($runnerMax,$runnerEnd);$appMax=[Math]::Max($appMax,$appEnd)
    $waitRecord=[ordered]@{
        selector=$AutomationId;description=$Description;automationId=[string]$spec.automationId;controlType=[string]$spec.controlType;expectedName=[string]$spec.name
        processId=[Int64]$Context.process.Id;mainWindowHandle=[Int64]$Context.mainWindowHandle;mainWindowProcessId=if($Context.process.HasExited){0L}else{[Int64][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle)}
        status=$status;found=($null -ne $element);attemptCount=$attempts;maximumAttempts=$MaximumAttempts;intervalMilliseconds=$IntervalMilliseconds;elapsedMilliseconds=[Math]::Round($timer.Elapsed.TotalMilliseconds,3)
        queryCountStart=$queryCountStart;queryCountEnd=@($Context.uiaQueryMetrics).Count;materializedTreeElementCount=0
        appWorkingSetStartBytes=$appStart;appWorkingSetEndBytes=$appEnd;appWorkingSetMaxBytes=$appMax
        runnerWorkingSetStartBytes=$runnerStart;runnerWorkingSetEndBytes=$runnerEnd;runnerWorkingSetMaxBytes=$runnerMax;runnerProcessId=[Int64]$runnerProcess.Id
        capturedAtUtc=Get-UtcTimestamp
    }
    $script:Manifest.uiaWaitMetrics.Add($waitRecord)
    [void](Add-RunnerMetric $Context "uia-wait-$AutomationId-$status")
    if($null -ne $element){return $element}
    Write-JsonFile $ManifestPath $script:Manifest
    throw "Bounded post-route UIA wait failed for $AutomationId ($Description): status=$status attempts=$attempts elapsedMs=$([Math]::Round($timer.Elapsed.TotalMilliseconds,3))."
}
function Invoke-UiaQueryBurstDiagnostic {
    param([object]$Context,[int]$Iterations=300,[int]$IntervalMilliseconds=100)
    if($Iterations -ne 300 -or $IntervalMilliseconds -ne 100){throw 'Probe UIA memory diagnostic must remain fixed at 300 bounded iterations and a 100 ms interval.'}
    $selectors=@('speakingEvaluationClassList','speakingEvaluationChoice','speakingEvaluationSaveReports')
    $runnerProcess=[Diagnostics.Process]::GetCurrentProcess();$runnerProcess.Refresh();$Context.process.Refresh()
    $runnerStart=[Int64]$runnerProcess.WorkingSet64;$appStart=[Int64]$Context.process.WorkingSet64
    $runnerMax=$runnerStart;$appMax=$appStart;$queryCountStart=@($Context.uiaQueryMetrics).Count
    $startedAtUtc=Get-UtcTimestamp;$timer=[Diagnostics.Stopwatch]::StartNew();$completedIterations=0;$status='failed';$failureMessage=$null
    try{
        for($iteration=1;$iteration -le $Iterations;$iteration++){
            foreach($selector in $selectors){
                $element=Get-OptionalUiaById $Context $selector
                if($null -eq $element){throw "UIA query memory diagnostic could not find bounded selector '$selector' at iteration $iteration."}
                $current=$element.Current;$spec=$script:UiaCatalog[$selector]
                if([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.AutomationId -cne [string]$spec.automationId -or [string]$current.ControlType.ProgrammaticName -cne [string]$spec.controlType -or [string]$current.Name -cne [string]$spec.name -or -not [bool]$current.IsEnabled){throw "UIA query memory diagnostic identity/enabled check failed for '$selector'."}
            }
            $completedIterations=$iteration;$runnerProcess.Refresh();$Context.process.Refresh()
            $runnerMax=[Math]::Max($runnerMax,[Int64]$runnerProcess.WorkingSet64);$appMax=[Math]::Max($appMax,[Int64]$Context.process.WorkingSet64)
            if($runnerMax -ge [Int64]805306368){throw "Harness working set reached the 768 MiB safety guard during UIA diagnostic: $runnerMax bytes."}
            if($appMax -ge $WorkingSetLimit){throw "App working set reached the strict cap during UIA diagnostic: $appMax bytes."}
            if(($iteration % 30) -eq 0){[void](Add-RunnerMetric $Context "uia-query-memory-diagnostic-$iteration")}
            if($iteration -lt $Iterations){Start-Sleep -Milliseconds $IntervalMilliseconds}
        }
        $status='passed'
    }
    catch{$failureMessage=$_.Exception.Message}
    $timer.Stop();$runnerProcess.Refresh();$Context.process.Refresh()
    $runnerEnd=[Int64]$runnerProcess.WorkingSet64;$appEnd=if($Context.process.HasExited){0L}else{[Int64]$Context.process.WorkingSet64}
    $runnerMax=[Math]::Max($runnerMax,$runnerEnd);$appMax=[Math]::Max($appMax,$appEnd)
    $record=[ordered]@{
        status=$status;failureMessage=$failureMessage;iterations=$Iterations;completedIterations=$completedIterations;intervalMilliseconds=$IntervalMilliseconds
        selectors=$selectors;queriesPerIteration=$selectors.Count;expectedQueryCount=$Iterations*$selectors.Count;observedQueryCount=(@($Context.uiaQueryMetrics).Count-$queryCountStart)
        queryCountStart=$queryCountStart;queryCountEnd=@($Context.uiaQueryMetrics).Count;materializedTreeElementCount=0
        appProcessId=[Int64]$Context.process.Id;appWorkingSetStartBytes=$appStart;appWorkingSetMaxBytes=$appMax;appWorkingSetEndBytes=$appEnd
        runnerProcessId=[Int64]$runnerProcess.Id;runnerWorkingSetStartBytes=$runnerStart;runnerWorkingSetMaxBytes=$runnerMax;runnerWorkingSetEndBytes=$runnerEnd;runnerWorkingSetGuardBytesExclusive=[Int64]805306368
        elapsedMilliseconds=[Math]::Round($timer.Elapsed.TotalMilliseconds,3);startedAtUtc=$startedAtUtc;completedAtUtc=Get-UtcTimestamp
    }
    $script:Manifest.uiaQueryBurstDiagnostic=$record
    [void](Add-RunnerMetric $Context "uia-query-memory-diagnostic-$status")
    Write-JsonFile $ManifestPath $script:Manifest
    if($status -cne 'passed'){throw "Bounded UIA query memory diagnostic failed after $completedIterations/$Iterations iterations: $failureMessage"}
}
function Wait-UiaAbsent {
    param([object]$Context,[string]$AutomationId,[int]$TimeoutSeconds,[string]$Description)
    if(-not $script:UiaCatalog.ContainsKey($AutomationId)){throw "No bounded UIA selector is registered for '$AutomationId'."}
    $maximumAttempts=[Math]::Min(60,[Math]::Max(1,[int][Math]::Ceiling($TimeoutSeconds*10)))
    $spec=$script:UiaCatalog[$AutomationId];$timer=[Diagnostics.Stopwatch]::StartNew();$runnerProcess=[Diagnostics.Process]::GetCurrentProcess();$runnerProcess.Refresh();$Context.process.Refresh()
    $runnerStart=[Int64]$runnerProcess.WorkingSet64;$appStart=[Int64]$Context.process.WorkingSet64;$runnerMax=$runnerStart;$appMax=$appStart
    $queryCountStart=@($Context.uiaQueryMetrics).Count;$status='timeout';$attempts=0;$observedOffscreen=$false
    for($attempt=1;$attempt -le $maximumAttempts;$attempt++){
        $attempts=$attempt;Drain-ProcessOutput $Context;$Context.process.Refresh()
        if($Context.process.HasExited){$status='process-exited';break}
        $element=Get-OptionalUiaById $Context $AutomationId
        $runnerProcess.Refresh();$Context.process.Refresh();$runnerMax=[Math]::Max($runnerMax,[Int64]$runnerProcess.WorkingSet64)
        if(-not $Context.process.HasExited){$appMax=[Math]::Max($appMax,[Int64]$Context.process.WorkingSet64)}
        if($runnerMax -ge [Int64]805306368){$status='runner-memory-guard';break}
        if($null -eq $element){$status='not-found';break}
        $current=$element.Current
        if([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.AutomationId -cne [string]$spec.automationId -or [string]$current.ControlType.ProgrammaticName -cne [string]$spec.controlType){throw "UIA absence wait returned an element outside the exact selector '$AutomationId'."}
        if([bool]$current.IsOffscreen){$status='offscreen';$observedOffscreen=$true;break}
        if($attempt -lt $maximumAttempts){Start-Sleep -Milliseconds 100}
    }
    $timer.Stop();$runnerProcess.Refresh();$Context.process.Refresh();$runnerMax=[Math]::Max($runnerMax,[Int64]$runnerProcess.WorkingSet64)
    $appEnd=if($Context.process.HasExited){0L}else{[Int64]$Context.process.WorkingSet64};$appMax=[Math]::Max($appMax,$appEnd)
    $record=[ordered]@{
        selector=$AutomationId;description=$Description;automationId=[string]$spec.automationId;controlType=[string]$spec.controlType
        processId=[Int64]$Context.process.Id;mainWindowHandle=[Int64]$Context.mainWindowHandle;mainWindowProcessId=if($Context.process.HasExited){0L}else{[Int64][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle)}
        status=$status;absenceObserved=($status -in @('not-found','offscreen'));observedOffscreen=$observedOffscreen;attemptCount=$attempts;maximumAttempts=$maximumAttempts;intervalMilliseconds=100
        elapsedMilliseconds=[Math]::Round($timer.Elapsed.TotalMilliseconds,3);queryCountStart=$queryCountStart;queryCountEnd=@($Context.uiaQueryMetrics).Count;materializedTreeElementCount=0
        appWorkingSetStartBytes=$appStart;appWorkingSetMaxBytes=$appMax;appWorkingSetEndBytes=$appEnd
        runnerProcessId=[Int64]$runnerProcess.Id;runnerWorkingSetStartBytes=$runnerStart;runnerWorkingSetMaxBytes=$runnerMax;runnerWorkingSetEndBytes=[Int64]$runnerProcess.WorkingSet64
        runnerWorkingSetGuardBytesExclusive=[Int64]805306368;capturedAtUtc=Get-UtcTimestamp
    }
    $script:Manifest.uiaAbsenceWaitMetrics.Add($record)
    [void](Add-RunnerMetric $Context "uia-absence-$AutomationId-$status")
    Write-JsonFile $ManifestPath $script:Manifest
    if(-not [bool]$record.absenceObserved){throw "Bounded UIA absence wait did not observe $AutomationId absent ($Description): status=$status attempts=$attempts elapsedMs=$([Math]::Round($timer.Elapsed.TotalMilliseconds,3))."}
}
function Get-MainWindow {
    param([object]$Context)
    $process=$Context.process;$process.Refresh();$handle=$process.MainWindowHandle
    if($handle -eq [IntPtr]::Zero){return $null}
    $window=[System.Windows.Automation.AutomationElement]::FromHandle($handle)
    if($null -eq $window -or [Int64]$window.Current.ProcessId -ne [Int64]$process.Id){throw 'Main-window UIA root is not bound to the measured PID.'}
    return $window
}
function Wait-MainWindow {
    param([object]$Context,[int]$TimeoutSeconds)
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds){Drain-ProcessOutput $Context;$Context.process.Refresh();if($Context.process.HasExited){throw "Packaged Next exited during startup with code $($Context.process.ExitCode)."};$window=Get-MainWindow $Context;if($null -ne $window){$Context.mainWindowHandle=[Int64]$Context.process.MainWindowHandle.ToInt64();$Context.mainWindowTitle=[string]$Context.process.MainWindowTitle;$Context.mainUiaRoot=$window;$windowProcessId=[Int64]$window.Current.ProcessId;$script:Manifest.process.mainWindowHandle=$Context.mainWindowHandle;$script:Manifest.process.windowProcessId=$windowProcessId;$script:Manifest.process.windowHandleProcessId=$windowProcessId;return $window};Start-Sleep -Milliseconds 100}
    throw "No PID-bound main window appeared within $TimeoutSeconds seconds."
}
function Get-WindowForElement {
    param([object]$Context,[System.Windows.Automation.AutomationElement]$Target)
    $processId=[uint32]$Context.process.Id;$targetCurrent=$Target.Current;$runtimeKey=Get-UiaRuntimeKey $Target;$handle=[Int64]0
    if($Context.uiaElementOwners.ContainsKey($runtimeKey)){$handle=[Int64]$Context.uiaElementOwners[$runtimeKey]}
    elseif([string]$targetCurrent.AutomationId -ceq [string]$script:UiaCatalog.mainWindow.automationId -and [string]$targetCurrent.ControlType.ProgrammaticName -ceq 'ControlType.Window'){$handle=[Int64]$Context.mainWindowHandle}
    elseif([Int64]$targetCurrent.NativeWindowHandle -gt 0){$handle=[Int64]$targetCurrent.NativeWindowHandle}
    if($handle -le 0 -or [uint32][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$handle) -ne $processId){throw 'UIA element has no verified process-bound owner HWND.'}
    if($handle -eq [Int64]$Context.mainWindowHandle){$mainInfo=Get-VerifiedMainUiaRoot $Context;return $mainInfo.element}
    $window=[System.Windows.Automation.AutomationElement]::FromHandle([IntPtr]$handle);$current=$window.Current
    if([uint32]$current.ProcessId -ne $processId -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.Window'){throw 'UIA owning HWND did not return its measured process-bound Window root.'}
    return $window
}
function Save-UiaSnapshot {
    param([object]$Context,[string]$Name)
    if(-not $script:UiaSnapshotSpecs.ContainsKey($Name)){throw "No bounded UIA snapshot specification is registered for '$Name'."}
    $elements=[Collections.Generic.List[object]]::new()
    foreach($selector in $script:UiaSnapshotSpecs[$Name]){$element=Get-OptionalUiaById $Context $selector;if($null -ne $element){$elements.Add((Get-UiaSnapshot $element))}}
    $path=Join-Path (Join-Path $EvidenceDirectory 'uia') ($Name+'.json')
    Write-JsonFile $path ([ordered]@{checkpoint=$Name;coverage='bounded named UIA controls only';capturedAtUtc=Get-UtcTimestamp;processId=[Int64]$Context.process.Id;mainWindowHandle=[Int64]$Context.mainWindowHandle;elements=$elements.ToArray()})
    $record=[ordered]@{relativePath=(Get-RelativePath $EvidenceDirectory $path);sha256=(Get-FileSha256 $path);elementCount=$elements.Count;processId=[Int64]$Context.process.Id;capturedAtUtc=Get-UtcTimestamp}
    $script:Manifest.uiaSnapshots[$Name]=$record
    [void](Add-RunnerMetric $Context "uia-snapshot-$Name")
    return $record
}
function Get-UiCounts {
    param([object]$Context)
    $counts=[ordered]@{}
    foreach($id in @('nextSpeakingEvaluationReports','nextSpeakingEvaluationBatchReports','nextReportStudent','nextReportClose','nextReportSavePdf','nextReportScope','nextReportDirectory','speakingEvaluationReports','speakingEvaluationSaveReports','speakingEvaluationClassList','speakingEvaluationChoice','classMngrNextNavigationTree')){
        $element=Get-OptionalUiaById $Context $id;$counts[$id]=if($null -eq $element){0}else{1}
    }
    $elementCount=0;foreach($value in $counts.Values){$elementCount+=[int]$value}
    [ordered]@{capturedAtUtc=Get-UtcTimestamp;coverage='bounded named UIA controls only';processId=[Int64]$Context.process.Id;elements=$elementCount;controls=$counts}
}
function Get-PreviewContentStats {
    param([Drawing.Bitmap]$Bitmap)
    $left=[int][Math]::Floor($Bitmap.Width*0.10);$right=[int][Math]::Ceiling($Bitmap.Width*0.90)
    $top=[int][Math]::Floor($Bitmap.Height*0.35);$bottom=[int][Math]::Ceiling($Bitmap.Height*0.82)
    $step=4;$colors=[Collections.Generic.HashSet[int]]::new();$sampled=0;$nonWhite=0;$dark=0
    for($y=$top;$y -lt $bottom;$y+=$step){for($x=$left;$x -lt $right;$x+=$step){
        $pixel=$Bitmap.GetPixel($x,$y);$packed=($pixel.R -shl 16) -bor ($pixel.G -shl 8) -bor $pixel.B
        [void]$colors.Add($packed);$sampled++
        if(($pixel.R+$pixel.G+$pixel.B) -lt 735){$nonWhite++}
        if($pixel.R -lt 96 -and $pixel.G -lt 96 -and $pixel.B -lt 96){$dark++}
    }}
    [ordered]@{region='central-lower-preview-area';x=$left;y=$top;width=($right-$left);height=($bottom-$top);sampledPixels=$sampled;uniqueSampledColors=$colors.Count;nonWhiteSampledPixels=$nonWhite;darkSampledPixels=$dark}
}
function Get-ScreenshotForElement {
    param([object]$Context,[string]$Name,[System.Windows.Automation.AutomationElement]$Target)
    $window=Get-WindowForElement $Context $Target;$handle=[Int64]$window.Current.NativeWindowHandle
    if($handle -le 0){throw "Screenshot UIA root has no native HWND at $Name."}
    $expectedPid=[uint32]$Context.process.Id;$windowPid=[Int64]$window.Current.ProcessId
    if($windowPid -ne [Int64]$expectedPid -or [Int64][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$handle) -ne [Int64]$expectedPid){throw "Screenshot HWND/UIA process identity mismatch at $Name."}
    [P913ScreenshotNative]::RaiseWindowWithoutActivation([IntPtr]$handle,$expectedPid)
    $windowBounds=[P913ScreenshotNative]::GetWindowBounds([IntPtr]$handle,$expectedPid)
    $width=[int]$windowBounds[2];$height=[int]$windowBounds[3]
    if($width -le 0 -or $height -le 0){throw "Invalid screenshot bounds at $Name."}
    $bounds=[Drawing.Rectangle]::new([int]$windowBounds[0],[int]$windowBounds[1],$width,$height)
    $bitmap=[Drawing.Bitmap]::new($width,$height);$graphics=[Drawing.Graphics]::FromImage($bitmap);$hdc=[IntPtr]::Zero;$method='PrintWindow(PW_RENDERFULLCONTENT)';$contentStats=$null
    $path=Join-Path (Join-Path $EvidenceDirectory 'screenshots') ($Name+'.png')
    try{
        $hdc=$graphics.GetHdc()
        if(-not [P913ScreenshotNative]::RenderWindow([IntPtr]$handle,$expectedPid,$hdc,2)){
            $method='PrintWindow(PW_CLIENTONLY)'
            if(-not [P913ScreenshotNative]::RenderWindow([IntPtr]$handle,$expectedPid,$hdc,0)){throw "PrintWindow did not render the measured app window at $Name."}
        }
        $graphics.ReleaseHdc($hdc);$hdc=[IntPtr]::Zero
        $contentStats=Get-PreviewContentStats $bitmap
        if($Name -in @('direct-report-preview','batch-report-preview') -and ($contentStats.uniqueSampledColors -lt 5 -or $contentStats.darkSampledPixels -lt 10)){throw "Captured $Name image does not contain sufficient rendered report preview content: $($contentStats | ConvertTo-Json -Compress)."}
        $bitmap.Save($path,[Drawing.Imaging.ImageFormat]::Png)
    } finally {if($hdc -ne [IntPtr]::Zero){$graphics.ReleaseHdc($hdc)};$graphics.Dispose();$bitmap.Dispose()}
    $file=Get-Item -LiteralPath $path
    if([Int64][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$handle) -ne [Int64]$expectedPid){throw "Screenshot HWND ownership changed after capture at $Name."}
    $record=[ordered]@{checkpoint=$Name;relativePath=(Get-RelativePath $EvidenceDirectory $path);sha256=(Get-FileSha256 $path);sizeBytes=[Int64]$file.Length;width=$width;height=$height;captureMethod=$method;processId=[Int64]$Context.process.Id;windowHandle=$handle;windowProcessId=[Int64][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$handle);capturedAtUtc=Get-UtcTimestamp;previewContentStats=$contentStats}
    return $record
}
function Get-CheckBoxState {
    param([object]$Context,[string]$AutomationId)
    $element=Get-UiaById $Context $AutomationId 'ControlType.CheckBox'
    try{$pattern=[System.Windows.Automation.TogglePattern]$element.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern);return [int]$pattern.Current.ToggleState}catch{throw "UIA TogglePattern is unavailable for $AutomationId."}
}
function Set-UiaValue {
    param([object]$Context,[System.Windows.Automation.AutomationElement]$Element,[string]$Value,[string]$ActionKind)
    $snapshot=Get-UiaSnapshot $Element
    if([Int64]$snapshot.processId -ne [Int64]$Context.process.Id -or -not $snapshot.isEnabled -or $snapshot.isOffscreen){throw "UIA ValuePattern target PID/visibility check failed for $ActionKind."}
    $owner=Get-WindowForElement $Context $Element;$handle=[Int64]$owner.Current.NativeWindowHandle
    $pattern=[System.Windows.Automation.ValuePattern]$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $pattern.SetValue($Value)
    $after=Get-UiaSnapshot $Element
    if([string]$after.value -cne $Value){throw "UIA ValuePattern.SetValue did not retain the requested value for $ActionKind."}
    $action=[ordered]@{actionKind=$ActionKind;automationId=$snapshot.automationId;controlType=$snapshot.controlType;processId=[Int64]$Context.process.Id;windowHandle=$handle;windowProcessId=[Int64]$owner.Current.ProcessId;method='ValuePattern.SetValue';valueBefore=$snapshot.value;valueAfter=$after.value;issuedAtUtc=Get-UtcTimestamp;target=$snapshot}
    Add-JsonLine $UiaActionsPath $action;$script:Manifest.uiaActions[$ActionKind]=$action;return $action
}
function Invoke-UiaAction {
    param([object]$Context,[System.Windows.Automation.AutomationElement]$Element,[string]$ActionKind,[string]$Name,[string]$TargetPageId='',[switch]$UseSelectionPattern,[switch]$UseWindowTargetedClick)
    $snapshot=Get-UiaSnapshot $Element;$measuredProcessId=[Int64]$Context.process.Id
    if([Int64]$snapshot.processId -ne $measuredProcessId -or -not $snapshot.isEnabled -or $snapshot.isOffscreen){throw "UIA target PID/enabled/visibility check failed for $Name."}
    $owner=Get-WindowForElement $Context $Element;$ownerHandle=[Int64]$owner.Current.NativeWindowHandle
    if($ownerHandle -le 0 -or [Int64]$owner.Current.ProcessId -ne $measuredProcessId){throw "UIA target owner HWND is not bound to process $measuredProcessId."}
    $handle=$ownerHandle;$windowProcessId=[Int64]$owner.Current.ProcessId;$method='InvokePattern.Invoke';$pointer=$null;$pattern=$null;$selectionVerification=$null
    if($UseWindowTargetedClick){
        $expectedNavigationTarget=switch($ActionKind){
            'navigate-to-speaking-evaluation' { 'Speaking Evaluation' }
            'navigate-away-after-report-close' { 'My Workspace' }
            'navigate-to-my-workspace-probe' { 'My Workspace' }
            default { $null }
        }
        if($null -eq $expectedNavigationTarget -or [string]$snapshot.controlType -cne 'ControlType.TreeItem' -or [string]$snapshot.name -cne $expectedNavigationTarget -or [string]$snapshot.automationId -cne [string]$script:UiaCatalog.classMngrNextNavigationTree.automationId){throw 'Window-targeted click requires an exact registered Next navigation TreeItem identity.'}
        $expectedTargetPage=if($ActionKind -ceq 'navigate-to-speaking-evaluation'){'speaking_evaluation'}else{'my_workspace'}
        if([string]$TargetPageId -cne $expectedTargetPage){throw "Window-targeted click target page does not match the registered navigation action $ActionKind."}
        $handle=[Int64]$Context.mainWindowHandle
        if($handle -le 0 -or $handle -ne $ownerHandle){throw 'Navigation UIA owner is not the measured main-window HWND.'}
        $windowProcessId=[Int64][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$handle)
        if($windowProcessId -ne $measuredProcessId){throw 'Measured main-window HWND belongs to another process.'}
        $rect=$snapshot.rectangle
        if($rect.width -le 0 -or $rect.height -le 0){throw 'Navigation TreeItem has an empty UIA BoundingRectangle.'}
        $x=[int][Math]::Round($rect.x+$rect.width/2);$y=[int][Math]::Round($rect.y+$rect.height/2)
        $windowBounds=[P913ScreenshotNative]::GetWindowBounds([IntPtr]$handle,[uint32]$measuredProcessId)
        if($rect.x -lt $windowBounds[0] -or $rect.y -lt $windowBounds[1] -or ($rect.x+$rect.width) -gt ($windowBounds[0]+$windowBounds[2]) -or ($rect.y+$rect.height) -gt ($windowBounds[1]+$windowBounds[3])){throw 'Navigation TreeItem BoundingRectangle is not fully inside the measured main window.'}
        if($x -lt $windowBounds[0] -or $x -ge ($windowBounds[0]+$windowBounds[2]) -or $y -lt $windowBounds[1] -or $y -ge ($windowBounds[1]+$windowBounds[3])){throw 'UIA-derived navigation click point is outside the measured main window.'}
        $method='UIA-targeted window click'
        $pointer=[ordered]@{
            kind='window-targeted WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP at center of the same verified UIA target'
            actionType='WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP';screenX=$x;screenY=$y
            sourceTarget=[ordered]@{name=$snapshot.name;automationId=$snapshot.automationId;controlType=$snapshot.controlType;rectangle=$snapshot.rectangle}
            targetProcessId=$measuredProcessId;windowHandle=$handle;windowProcessId=$windowProcessId
            measuredMainWindowBounds=[ordered]@{x=[Int64]$windowBounds[0];y=[Int64]$windowBounds[1];width=[Int64]$windowBounds[2];height=[Int64]$windowBounds[3]}
        }
    } elseif($UseSelectionPattern){
        $pattern=[System.Windows.Automation.SelectionItemPattern]$Element.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern);$method='SelectionItemPattern.Select'
    } else {
        $pattern=[System.Windows.Automation.InvokePattern]$Element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    }
    $action=[ordered]@{
        actionKind=$ActionKind;expectedName=$Name;observedName=$snapshot.name;automationId=$snapshot.automationId;controlType=$snapshot.controlType
        processId=$measuredProcessId;windowHandle=$handle;windowProcessId=$windowProcessId
        targetPageId=if($TargetPageId){$TargetPageId}else{$null};method=$method;pointerAction=$pointer;issuedAtUtc=Get-UtcTimestamp
        target=$snapshot;verifiedByFollowingSnapshot=$false
    }
    Add-JsonLine $UiaActionsPath $action;$script:Manifest.uiaActions[$ActionKind]=$action
    if($UseWindowTargetedClick){[P913ScreenshotNative]::ClickScreenPoint([IntPtr]$handle,[uint32]$measuredProcessId,$x,$y)}elseif($UseSelectionPattern){
        $pattern.Select();$selected=[bool]$pattern.Current.IsSelected
        if($ActionKind -ceq 'select-fixture-class'){
            $selectionParent=Get-UiaById $Context 'speakingEvaluationClassList' 'ControlType.List'
            $parentAutomationId=[string]$selectionParent.Current.AutomationId
        }elseif($ActionKind -ceq 'navigate-away-after-report-close'){
            $selectionParent=Get-UiaById $Context 'classMngrNextNavigationTree' 'ControlType.Tree'
            $parentAutomationId=[string]$selectionParent.Current.AutomationId
        }else{throw "SelectionItemPattern is not authorized for UIA action '$ActionKind'."}
        $parentSelection=[System.Windows.Automation.SelectionPattern]$selectionParent.GetCurrentPattern([System.Windows.Automation.SelectionPattern]::Pattern)
        $selectedElements=$parentSelection.Current.GetSelection();$selectedNames=[Collections.Generic.List[string]]::new()
        foreach($selectedElement in $selectedElements){$selectedCurrent=$selectedElement.Current;if([uint32]$selectedCurrent.ProcessId -ne [uint32]$measuredProcessId){throw 'Selected parent-list/tree item belongs to a different process.'};$selectedNames.Add([string]$selectedCurrent.Name)}
        if(-not $selected -or $selectedNames.Count -ne 1 -or [string]$selectedNames[0] -cne $Name){throw 'SelectionItemPattern did not retain exactly the requested immediate child.'}
        $selectionVerification=[ordered]@{itemIsSelected=$selected;selectedChildCount=$selectedNames.Count;selectedChildNames=@($selectedNames.ToArray());selectedChildProcessId=$measuredProcessId;parentAutomationId=$parentAutomationId}
    }else{$pattern.Invoke()}
    if($null -ne $selectionVerification){$action.selectionVerification=$selectionVerification}
    $action.invocationReturnedAtUtc=Get-UtcTimestamp;$script:Manifest.uiaActions[$ActionKind]=$action;return $action
}
function Wait-ForPageRoute {
    param([object]$Context,[Int64]$AfterArrivalIndex,[string]$TargetPageId,[int]$TimeoutSeconds)
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds){
        Drain-ProcessOutput $Context
        foreach($entry in $Context.pageRouteEvents){
            if([Int64]$entry.arrivalIndex -le $AfterArrivalIndex){continue}
            if([string]$entry.targetPageId -ceq $TargetPageId){
                return [ordered]@{observed=$true;targetPageId=$TargetPageId;arrivalIndex=[Int64]$entry.arrivalIndex;stream=[string]$entry.stream;line=[string]$entry.line;processId=[Int64]$Context.process.Id;windowHandle=[Int64]$Context.mainWindowHandle;windowProcessId=[Int64][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle);observedAtUtc=Get-UtcTimestamp}
            }
        }
        $Context.process.Refresh();if($Context.process.HasExited){throw "Packaged Next exited before reporting page-route to $TargetPageId."}
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for a post-action page-route event to $TargetPageId."
}
function Add-RunnerMetric {
    param([object]$Context,[string]$Point)
    $runnerProcess=[Diagnostics.Process]::GetCurrentProcess();$runnerProcess.Refresh()
    $sample=[ordered]@{
        point=$Point;capturedAtUtc=Get-UtcTimestamp;runnerProcessId=[Int64]$runnerProcess.Id
        runnerWorkingSetBytes=[Int64]$runnerProcess.WorkingSet64;runnerPrivateBytesDiagnosticOnly=[Int64]$runnerProcess.PrivateMemorySize64
        runnerHandles=[Int64]$runnerProcess.HandleCount;runnerThreadCount=[Int64]$runnerProcess.Threads.Count
        measuredAppProcessId=[Int64]$Context.process.Id
    }
    $script:Manifest.runnerMetrics.Add($sample)
    [void](Write-RunnerMemoryCheckpoint "runner-metric-$Point" $Context)
    if([Int64]$sample.runnerWorkingSetBytes -ge [Int64]805306368){throw "PowerShell harness working set reached the 768 MiB safety guard at '$Point': $($sample.runnerWorkingSetBytes) bytes."}
    return $sample
}
function Assert-RunnerWorkingSet {
    param([object]$Context,[string]$Point)
    $sample=Write-RunnerMemoryCheckpoint $Point $Context
    if([Int64]$sample.runnerWorkingSetBytes -ge [Int64]805306368){throw "PowerShell harness working set reached the 768 MiB safety guard at '$Point': $($sample.runnerWorkingSetBytes) bytes."}
    return $sample
}
function Release-RouteObjects {
    param([object]$Context,[string]$Point)
    [GC]::Collect(2,[GCCollectionMode]::Forced,$true,$true)
    [GC]::WaitForPendingFinalizers()
    [GC]::Collect(2,[GCCollectionMode]::Forced,$true,$true)
    [void](Assert-RunnerWorkingSet $Context $Point)
}
function Get-RunnerMemorySummary {
    $reader=[IO.StreamReader]::new($RunnerMemoryPath)
    $count=0;$maximum=0L;$maximumPoint=$null;$last=0L
    try {
        while(-not $reader.EndOfStream){
            $line=$reader.ReadLine();if([string]::IsNullOrWhiteSpace($line)){continue}
            $sample=$line | ConvertFrom-Json -ErrorAction Stop
            $workingSet=[Int64]$sample.runnerWorkingSetBytes;$count++;$last=$workingSet
            if($workingSet -gt $maximum){$maximum=$workingSet;$maximumPoint=[string]$sample.point}
        }
    } finally {$reader.Dispose()}
    [ordered]@{
        path='runtime/runner-memory-checkpoints.jsonl';sampleCount=$count
        maxWorkingSetBytes=$maximum;maxPoint=$maximumPoint;lastRecordedWorkingSetBytes=$last
        guardExclusiveBytes=[Int64]805306368;captureBoundary='before-final-manifest-serialization'
    }
}
function Get-OperationPeakWorkingSet {
    $maximum=0L
    foreach($metric in $script:Manifest.processMetrics){
        if($metric.category -ceq 'operation'){
            $workingSet=[Int64]$metric.workingSetBytes
            if($workingSet -gt $maximum){$maximum=$workingSet}
        }
    }
    return [Int64]$maximum
}
function Resolve-SourceEvidenceFile {
    param([string]$Root,[string]$RelativePath)
    if([string]::IsNullOrWhiteSpace($RelativePath) -or [IO.Path]::IsPathRooted($RelativePath)){throw "Raw evidence path is not a relative file: $RelativePath"}
    $rootPath=[IO.Path]::GetFullPath($Root).TrimEnd([char]92,[char]47)+[IO.Path]::DirectorySeparatorChar
    $fullPath=[IO.Path]::GetFullPath((Join-Path $Root $RelativePath))
    if(-not $fullPath.StartsWith($rootPath,[StringComparison]::OrdinalIgnoreCase)){throw "Raw evidence path escaped its immutable source run: $RelativePath"}
    if(-not [IO.File]::Exists($fullPath)){throw "Required raw evidence file is missing: $RelativePath"}
    return $fullPath
}
function Get-RawProcessMetricSummary {
    param([string]$SourceRoot,[string]$RelativePath,[Int64]$ExpectedProcessId,[int]$ExpectedManifestCount)
    $path=Resolve-SourceEvidenceFile $SourceRoot $RelativePath
    $reader=[IO.StreamReader]::new($path);$count=0;$operationCount=0;$operationPeak=0L;$normalUseCount=0
    try{
        while(-not $reader.EndOfStream){
            $line=$reader.ReadLine();if([string]::IsNullOrWhiteSpace($line)){continue}
            $sample=$line | ConvertFrom-Json -ErrorAction Stop;$count++
            if([Int64]$sample.pid -ne $ExpectedProcessId -or [Int64]$sample.sampleIndex -ne ($count-1)){throw "Raw process metric identity/order mismatch at sample $count."}
            $workingSet=[Int64]$sample.workingSetBytes
            if($workingSet -le 0){throw "Raw process metric working set is invalid at sample $count."}
            if([string]$sample.category -ceq 'operation'){$operationCount++;if($workingSet -gt $operationPeak){$operationPeak=$workingSet}}
            elseif([string]$sample.category -ceq 'normal-use'){$normalUseCount++}
            else{throw "Raw process metric has an unrecognized category at sample $count."}
        }
    }finally{$reader.Dispose()}
    if($count -ne $ExpectedManifestCount -or $count -le 0 -or $operationCount -le 0 -or $normalUseCount -le 0){throw "Raw process metric counts are inconsistent: lines=$count manifest=$ExpectedManifestCount operation=$operationCount normalUse=$normalUseCount."}
    [ordered]@{path=$RelativePath;sha256=(Get-FileSha256 $path);sampleCount=[Int64]$count;operationSampleCount=[Int64]$operationCount;normalUseSampleCount=[Int64]$normalUseCount;operationPeakWorkingSetBytes=[Int64]$operationPeak}
}
function Get-RawRunnerMemorySummary {
    param([string]$SourceRoot,[string]$RelativePath,[Int64]$ExpectedAppProcessId)
    $path=Resolve-SourceEvidenceFile $SourceRoot $RelativePath
    $reader=[IO.StreamReader]::new($path);$count=0;$maximum=0L;$maximumPoint=$null;$last=0L;$runnerPid=0L
    try{
        while(-not $reader.EndOfStream){
            $line=$reader.ReadLine();if([string]::IsNullOrWhiteSpace($line)){continue}
            $sample=$line | ConvertFrom-Json -ErrorAction Stop;$count++
            $runnerProcessId=[Int64]$sample.runnerProcessId;$workingSet=[Int64]$sample.runnerWorkingSetBytes
            if($runnerProcessId -le 0 -or $runnerProcessId -eq $ExpectedAppProcessId -or $workingSet -le 0){throw "Raw runner memory checkpoint identity/value is invalid at sample $count."}
            if($runnerPid -eq 0){$runnerPid=$runnerProcessId}elseif($runnerPid -ne $runnerProcessId){throw 'Raw runner memory checkpoints contain more than one harness PID.'}
            $measuredAppId=if($null -eq $sample.measuredAppProcessId){0L}else{[Int64]$sample.measuredAppProcessId}
            if($measuredAppId -notin @(0L,$ExpectedAppProcessId)){throw "Raw runner memory sample is bound to unexpected app PID $measuredAppId."}
            $last=$workingSet
            if($workingSet -gt $maximum){$maximum=$workingSet;$maximumPoint=[string]$sample.point}
        }
    }finally{$reader.Dispose()}
    if($count -le 0 -or $maximum -ge [Int64]805306368){throw "Raw runner-memory sidecar is empty or reached the exclusive 768 MiB guard: count=$count max=$maximum."}
    [ordered]@{
        path=$RelativePath;sampleCount=[Int64]$count;maxWorkingSetBytes=[Int64]$maximum;maxPoint=$maximumPoint
        lastRecordedWorkingSetBytes=[Int64]$last;guardExclusiveBytes=[Int64]805306368
        captureBoundary='derived-from-immutable-runtime-checkpoints.jsonl';derivedFromRawSidecar=$true;runnerProcessId=$runnerPid
    }
}
function Invoke-FinalizeOnly {
    if(-not $FinalizeOnly -or [string]::IsNullOrWhiteSpace($FinalizeSourceDirectory)){throw 'Internal FinalizeOnly source configuration is missing.'}
    $sourceManifestPath=Join-Path $FinalizeSourceDirectory 'manifest.json'
    $sourceManifestHash=Get-FileSha256 $sourceManifestPath
    $source=Read-JsonFile $sourceManifestPath
    if([string]$source.schema -cne $Schema -or [string]$source.taskId -cne $TaskId){throw 'FinalizeOnly source manifest identity is not P9-13.'}
    if([string]$source.status -cne 'failed' -or [string]$source.failure.message -notmatch '(?i)property|workingSetBytes'){throw 'FinalizeOnly source is not the preserved post-exit summary-measurement failure this path is authorized to reconcile.'}
    $processId=[Int64]$source.process.processId
    $exit=$source.processExitEvidence;if($null -eq $exit){$exit=$source.processExit}
    if($processId -le 0 -or $null -eq $exit -or -not [bool]$exit.normalExit -or [Int64]$exit.exitCode -ne 0 -or [string]$exit.closeMethod -cne 'WindowPattern.Close' -or -not [bool]$exit.windowPatternCloseReturned){throw 'FinalizeOnly source lacks verified natural WindowPattern.Close exit code 0.'}
    $requiredCheckpoints=@('speaking-evaluation-page-ready','direct-preview-open','direct-preview-closed','batch-dialog-ready','batch-preview-open','batch-preview-closed','export-notice','export-complete','navigated-away')
    foreach($checkpointName in $requiredCheckpoints){if($null -eq $source.checkpoints.PSObject.Properties[$checkpointName]){throw "FinalizeOnly source is missing completed raw checkpoint '$checkpointName'."}}
    $writer=$source.writerLifecycle
    if([Int64]$writer.createCount -ne $ExpectedReportCount -or [Int64]$writer.releaseBeforeCount -ne $ExpectedReportCount -or [Int64]$writer.releaseAfterCount -ne $ExpectedReportCount -or [Int64]$writer.distinctWriterIds -ne $ExpectedReportCount){throw 'FinalizeOnly source writer lifecycle does not show 25 complete writer releases.'}
    $staging=$source.temporaryStaging
    $stagingAfterCount=if($null -eq $staging.after){0}else{@($staging.after).Count}
    $outputStagingAfterCount=if($null -eq $source.output.stagingDirectoriesAfter){0}else{@($source.output.stagingDirectoriesAfter).Count}
    if(-not [bool]$staging.absenceVerifiedAfterCompletion -or $stagingAfterCount -ne 0 -or $outputStagingAfterCount -ne 0){throw 'FinalizeOnly source does not prove staging cleanup.'}
    $away=$source.uiaActions.navigateAway;$awayRoute=$away.pageRouteEvent
    if([string]$away.method -cne 'UIA-targeted window click' -or [string]$away.target.name -cne 'My Workspace' -or -not [bool]$away.verifiedByFollowingPageRoute -or -not [bool]$awayRoute.observed -or [string]$awayRoute.targetPageId -cne 'my_workspace'){throw 'FinalizeOnly source lacks the completed My Workspace UIA route evidence.'}
    $archivePath=Resolve-SourceEvidenceFile $FinalizeSourceDirectory 'output/output.zip'
    if([Int64](Get-Item -LiteralPath $archivePath).Length -ne [Int64]$source.output.archiveBytes -or (Get-FileSha256 $archivePath) -cne [string]$source.output.archiveSha256){throw 'FinalizeOnly source archive size or SHA-256 differs from its persisted output manifest.'}
    $sourceMetricCount=@($source.processMetrics).Count
    $metricSummary=Get-RawProcessMetricSummary $FinalizeSourceDirectory ([string]$source.files.processMetrics) $processId $sourceMetricCount
    $runnerRelativePath=[string]$source.files.runnerMemoryCheckpoints
    $runnerSummary=Get-RawRunnerMemorySummary $FinalizeSourceDirectory $runnerRelativePath $processId

    $rawInputs=[Collections.Generic.List[object]]::new()
    $rawHashByPath=@{}
    $rawPaths=[Collections.Generic.List[string]]::new()
    foreach($relative in @(
        'manifest.json',[string]$source.files.processMetrics,$runnerRelativePath,
        [string]$source.logs.processStdoutPath,[string]$source.logs.processStderrPath,
        [string]$source.logs.applicationOutputPath,[string]$source.logs.uiaActionsPath,
        'output/output.zip',[string]$staging.watcherPath
    )){
        if(-not [string]::IsNullOrWhiteSpace($relative) -and -not $rawHashByPath.ContainsKey($relative)){$rawPaths.Add($relative)}
    }
    $shutdownTraceRelative='runtime/diagnostics/application-shutdown.jsonl'
    $shutdownTracePath=Join-Path $FinalizeSourceDirectory $shutdownTraceRelative
    $shutdownTraceExists=[IO.File]::Exists($shutdownTracePath)
    if($shutdownTraceExists -and -not $rawHashByPath.ContainsKey($shutdownTraceRelative)){$rawPaths.Add($shutdownTraceRelative)}
    foreach($relative in $rawPaths){
        $path=Resolve-SourceEvidenceFile $FinalizeSourceDirectory $relative
        $rawHash=Get-FileSha256 $path
        $rawHashByPath[$relative]=$rawHash
        $rawInputs.Add([ordered]@{path=$relative;sizeBytes=[Int64](Get-Item -LiteralPath $path).Length;sha256=$rawHash})
    }
    if([string]$rawHashByPath['manifest.json'] -cne $sourceManifestHash){throw 'FinalizeOnly source manifest changed during preflight.'}

    $logs=[ordered]@{
        processStdoutPath=[string]$source.logs.processStdoutPath;processStdoutSha256=[string]$rawHashByPath[[string]$source.logs.processStdoutPath]
        processStderrPath=[string]$source.logs.processStderrPath;processStderrSha256=[string]$rawHashByPath[[string]$source.logs.processStderrPath]
        applicationOutputPath=[string]$source.logs.applicationOutputPath;applicationOutputSha256=[string]$rawHashByPath[[string]$source.logs.applicationOutputPath]
        processMetricsPath=[string]$source.files.processMetrics;processMetricsSha256=[string]$metricSummary.sha256
        uiaActionsPath=[string]$source.logs.uiaActionsPath;uiaActionsSha256=[string]$rawHashByPath[[string]$source.logs.uiaActionsPath]
        runnerMemoryCheckpointsPath=$runnerRelativePath;runnerMemoryCheckpointsSha256=[string]$rawHashByPath[$runnerRelativePath]
        shutdownTracePath=$shutdownTraceRelative;shutdownTraceExists=$shutdownTraceExists
        shutdownTraceSha256=if($shutdownTraceExists){[string]$rawHashByPath[$shutdownTraceRelative]}else{$null}
    }
    $derivedValues=[ordered]@{
        operationPeakWorkingSetBytes=[Int64]$metricSummary.operationPeakWorkingSetBytes
        processMetricSampleCount=[Int64]$metricSummary.sampleCount
        runnerMemory=$runnerSummary
        logHashes=$logs
    }
    $reconciliation=[ordered]@{
        mode='post-exit-finalization-from-immutable-raw-evidence'
        sourceRunId=[string]$source.runId;sourceDirectory=$FinalizeSourceDirectory
        sourceManifestPath=$sourceManifestPath;sourceManifestSha256=$sourceManifestHash
        sourceStatus=[string]$source.status;sourceFailure=$source.failure
        finalizedAtUtc=Get-UtcTimestamp;finalizedManifestPath=$ManifestPath
        rawMeasurementsChanged=$false;rawInputHashesBefore=@($rawInputs.ToArray());rawInputHashesAfter=@($rawInputs.ToArray())
        derivedFields=@('operationPeakWorkingSetBytes','runnerMemoryCheckpoints','logs.*Sha256')
        derivedValues=$derivedValues
        validationPath=$ValidationPath;finalValidationPath=(Join-Path (Join-Path $EvidenceDirectory 'logs') 'final-validation.json')
    }
    $source.status='captured-awaiting-validation';$source.failure=$null;$source.validation=$null
    Add-Member -InputObject $source -MemberType NoteProperty -Name processExit -Value $exit -Force
    Add-Member -InputObject $source -MemberType NoteProperty -Name runnerMemoryCheckpoints -Value $runnerSummary -Force
    Add-Member -InputObject $source -MemberType NoteProperty -Name operationPeakWorkingSetBytes -Value ([Int64]$metricSummary.operationPeakWorkingSetBytes) -Force
    Add-Member -InputObject $source -MemberType NoteProperty -Name logs -Value $logs -Force
    Add-Member -InputObject $source -MemberType NoteProperty -Name reconciliation -Value $reconciliation -Force
    $script:Manifest=$source
    Write-JsonFile $ManifestPath $source
    Write-JsonFile $ReconciliationPath $reconciliation

    $validator=Join-Path $PSScriptRoot 'validate_packaged_next_speaking_evaluation_lifecycle.py'
    $validatorArguments=@($validator,'--evidence-directory',$FinalizeSourceDirectory,'--manifest-file',$ManifestPath,'--output',$ValidationPath,'--quiet')
    & python @validatorArguments 1> $ValidationStdoutPath 2> $ValidationStderrPath
    $validatorExit=[int]$LASTEXITCODE
    if($validatorExit -ne 0){throw "FinalizeOnly full evidence validator failed with exit code $validatorExit."}
    $source.status='passed';$source.validation=[ordered]@{
        exitCode=$validatorExit;path=$ValidationPath;sha256=(Get-FileSha256 $ValidationPath)
        stdoutPath=$ValidationStdoutPath;stdoutSha256=(Get-FileSha256 $ValidationStdoutPath)
        sourceEvidenceDirectory=$FinalizeSourceDirectory;manifestOverridePath=$ManifestPath
    }
    Write-JsonFile $ManifestPath $source

    $finalValidationPath=Join-Path (Join-Path $EvidenceDirectory 'logs') 'final-validation.json'
    $finalStdoutPath=Join-Path (Join-Path $EvidenceDirectory 'logs') 'final-validator-stdout.txt'
    $finalStderrPath=Join-Path (Join-Path $EvidenceDirectory 'logs') 'final-validator-stderr.txt'
    $finalArguments=@($validator,'--evidence-directory',$FinalizeSourceDirectory,'--manifest-file',$ManifestPath,'--output',$finalValidationPath,'--quiet')
    & python @finalArguments 1> $finalStdoutPath 2> $finalStderrPath
    $finalExit=[int]$LASTEXITCODE
    if($finalExit -ne 0){throw "FinalizeOnly final manifest revalidation failed with exit code $finalExit."}
    foreach($rawInput in $rawInputs){
        $rawPath=Resolve-SourceEvidenceFile $FinalizeSourceDirectory ([string]$rawInput.path)
        if((Get-FileSha256 $rawPath) -cne [string]$rawInput.sha256){throw "FinalizeOnly changed raw source evidence: $($rawInput.path)"}
    }
    $integrity=[ordered]@{
        status='passed';checkedAtUtc=Get-UtcTimestamp;sourceRunId=[string]$source.runId
        sourceManifestPath=$sourceManifestPath;sourceManifestSha256=$sourceManifestHash
        sourceEvidenceDirectory=$FinalizeSourceDirectory;rawMeasurementsChanged=$false
        checkedRawInputCount=$rawInputs.Count;rawInputHashes=@($rawInputs.ToArray())
        reconciledManifestPath=$ManifestPath;reconciledManifestSha256=(Get-FileSha256 $ManifestPath)
        finalValidatorPath=$finalValidationPath;finalValidatorSha256=(Get-FileSha256 $finalValidationPath)
    }
    Write-JsonFile $PostValidationIntegrityPath $integrity
    Write-Output "P9-13 FinalizeOnly reconciliation passed: $EvidenceDirectory"
}
function Complete-PostExitEvidence {
    param([object]$Context)
    $Context.process.Refresh()
    if(-not $Context.process.HasExited){throw 'Cannot fingerprint redirected process logs before natural app exit releases their file handles.'}
    $script:Manifest.logs=[ordered]@{
        processStdoutPath='logs/process-stdout.log';processStdoutSha256=(Get-FileSha256 $Context.stdoutPath)
        processStderrPath='logs/process-stderr.log';processStderrSha256=(Get-FileSha256 $Context.stderrPath)
        applicationOutputPath='application-output.jsonl';applicationOutputSha256=(Get-FileSha256 $StartupLogPath)
        processMetricsPath='process-metrics.jsonl';processMetricsSha256=(Get-FileSha256 $ProcessMetricsPath)
        uiaActionsPath='uia-actions.jsonl';uiaActionsSha256=(Get-FileSha256 $UiaActionsPath)
        runnerMemoryCheckpointsPath='runtime/runner-memory-checkpoints.jsonl'
        shutdownTracePath='runtime/diagnostics/application-shutdown.jsonl';shutdownTraceExists=(Test-Path -LiteralPath (Join-Path (Join-Path $EvidenceDirectory 'runtime') 'diagnostics\application-shutdown.jsonl'))
    }
    [void](Write-RunnerMemoryCheckpoint 'post-natural-app-exit-before-final-manifest' $Context)
    $script:Manifest.runnerMemoryCheckpoints=Get-RunnerMemorySummary
    $script:Manifest.operationPeakWorkingSetBytes=Get-OperationPeakWorkingSet
}
function Invoke-ValidatorToSidecar {
    param([string]$ValidatorPath,[string]$ResultPath,[string]$StdoutPath,[string]$StderrPath,[switch]$OutputArchiveOnly)
    $arguments=@($ValidatorPath,'--evidence-directory',$EvidenceDirectory,'--output',$ResultPath,'--quiet')
    if($OutputArchiveOnly){$arguments+= '--output-archive-only'}
    & python @arguments 1> $StdoutPath 2> $StderrPath
    return [int]$LASTEXITCODE
}
function Add-ProcessMetric {
    param([object]$Context,[string]$Point,[ValidateSet('normal-use','operation')][string]$Category)
    $p=$Context.process;$p.Refresh();if($p.HasExited){throw "Cannot sample $Point after process exit."}
    $sample=[ordered]@{
        point=$Point;category=$Category;sampleIndex=[Int64]@($script:Manifest.processMetrics).Count;capturedAtUtc=Get-UtcTimestamp;pid=[Int64]$p.Id
        workingSetBytes=[Int64]$p.WorkingSet64;privateBytes=[Int64]$p.PrivateMemorySize64;peakWorkingSetBytes=[Int64]$p.PeakWorkingSet64
        handles=[Int64]$p.HandleCount;threadCount=[Int64]$p.Threads.Count;cpuTimeMilliseconds=[Int64]$p.TotalProcessorTime.TotalMilliseconds
    }
    if($Category -ceq 'normal-use' -and $sample.workingSetBytes -ge $WorkingSetLimit){throw "$Point working set $($sample.workingSetBytes) is not strictly below $WorkingSetLimit bytes."}
    Add-JsonLine $ProcessMetricsPath $sample;$script:Manifest.processMetrics.Add($sample);return $sample
}
function Save-Checkpoint {
    param([object]$Context,[string]$Name,[ValidateSet('normal-use','operation')][string]$Category,[object]$Details)
    $metric=Add-ProcessMetric $Context $Name $Category
    $uia=Save-UiaSnapshot $Context $Name
    $checkpoint=[ordered]@{capturedAtUtc=Get-UtcTimestamp;category=$Category;metric=$metric;uia=$uia;details=$Details}
    $script:Manifest.checkpoints[$Name]=$checkpoint;Write-JsonFile $ManifestPath $script:Manifest;return $checkpoint
}
function Get-TempStagingDirectories {
    $root=Join-Path (Join-Path $EvidenceDirectory 'runtime') 'temp'
    if(-not(Test-Path -LiteralPath $root)){return @()}
    @((Get-ChildItem -LiteralPath $root -Directory -Force | Where-Object {$_.Name.StartsWith('ClassMngr-speaking-evaluations-',[StringComparison]::OrdinalIgnoreCase)} | ForEach-Object {[ordered]@{path=$_.FullName;createdAtUtc=$_.CreationTimeUtc.ToString('o');lastWriteAtUtc=$_.LastWriteTimeUtc.ToString('o')}}))
}
function Get-CloseObservation {
    param([object]$Context)
    $p=$Context.process;$p.Refresh();$closeCall=$null;$windowPatternClose=$null;$closeMethod=$null;$closeWindowHandle=$null;$waitCompleted=$false;$forced=$false
    if(-not $p.HasExited){
        if([Int64]$Context.mainWindowHandle -le 0){throw 'Cannot close the packaged process without its measured main-window HWND.'}
        $rootInfo=Get-VerifiedMainUiaRoot $Context
        $rootCurrent=$rootInfo.element.Current
        if([Int64]$rootInfo.handle -ne [Int64]$Context.mainWindowHandle -or [Int64]$rootCurrent.ProcessId -ne [Int64]$p.Id -or [Int64][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$rootInfo.handle) -ne [Int64]$p.Id){throw 'Natural-close WindowPattern root is not bound to the measured app HWND/PID.'}
        $windowPattern=[System.Windows.Automation.WindowPattern]$rootInfo.element.GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern)
        $windowPattern.Close();$windowPatternClose=$true;$closeMethod='WindowPattern.Close';$closeWindowHandle=[Int64]$rootInfo.handle
        $timer=[Diagnostics.Stopwatch]::StartNew();while($timer.Elapsed.TotalSeconds -lt 20 -and -not $p.HasExited){Drain-ProcessOutput $Context;$p.Refresh();if($p.HasExited){$waitCompleted=$true;break};[void]$p.WaitForExit(250)};$p.Refresh();if($p.HasExited){$waitCompleted=$true}
    }
    $exitCode=if($p.HasExited){[Int64]$p.ExitCode}else{$null};$Context.forcedTermination=$forced
    Drain-ProcessOutput $Context
    if($p.HasExited -and $null -ne $script:Manifest.runtimeSettingsProvenance){
        $settingsRoot=Join-Path (Join-Path $EvidenceDirectory 'runtime') 'settings'
        $settingsAfter=Get-CanonicalInventory $settingsRoot
        $settingsAfterPath=Join-Path (Join-Path $EvidenceDirectory 'runtime') 'settings-after-exit-inventory.json'
        Write-JsonFile $settingsAfterPath $settingsAfter
        $script:Manifest.runtimeSettingsProvenance.afterExitInventoryPath='runtime/settings-after-exit-inventory.json'
        $script:Manifest.runtimeSettingsProvenance.afterExitInventorySha256=Get-FileSha256 $settingsAfterPath
        $script:Manifest.runtimeSettingsProvenance.afterExitFileCount=[Int64]$settingsAfter.fileCount
        $script:Manifest.runtimeSettingsProvenance.afterExitRootSha256=[string]$settingsAfter.sha256
    }
    $result=[ordered]@{closeMethod=$closeMethod;windowPatternCloseReturned=$windowPatternClose;closeWindowHandle=$closeWindowHandle;closeMainWindowReturned=$closeCall;waitCompleted=$waitCompleted;exitCode=$exitCode;forcedTerminationForCleanup=$forced;normalExit=($waitCompleted -and -not $forced -and $exitCode -eq 0);observedAtUtc=Get-UtcTimestamp}
    $Context.processExit=$result;$script:Manifest.processExit=$result;return $result
}
function Wait-ForExportCompletion {
    param([object]$Context,[string]$ArchivePath,[int]$TimeoutSeconds)
    $timer=[Diagnostics.Stopwatch]::StartNew();$lastSample=[Diagnostics.Stopwatch]::StartNew();$lastRunnerSample=[Diagnostics.Stopwatch]::StartNew();$lastSize=-1L;$stable=0;$archiveSeen=$false;$completionProbeDone=$false
    while($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds){
        Drain-ProcessOutput $Context;$Context.process.Refresh();if($Context.process.HasExited){throw "Packaged Next exited during report export with code $($Context.process.ExitCode)."}
        if($lastSample.ElapsedMilliseconds -ge 125){[void](Add-ProcessMetric $Context 'batch-export-operation-sample' 'operation');$lastSample.Restart()}
        if($lastRunnerSample.ElapsedMilliseconds -ge 1000){$runnerSample=Add-RunnerMetric $Context 'batch-export-runner-memory-sample';$lastRunnerSample.Restart();if([Int64]$runnerSample.runnerWorkingSetBytes -ge [Int64]805306368){throw "PowerShell harness working set reached the 768 MiB safety guard during export completion: $($runnerSample.runnerWorkingSetBytes) bytes."}}
        if(Test-Path -LiteralPath $ArchivePath -PathType Leaf){$archiveSeen=$true;$size=[Int64](Get-Item -LiteralPath $ArchivePath).Length;if($size -gt 0 -and $size -eq $lastSize){$stable++}else{$stable=0};$lastSize=$size}
        if($archiveSeen -and $stable -ge 3 -and -not $completionProbeDone){
            $completionProbeDone=$true;$runnerProcess=[Diagnostics.Process]::GetCurrentProcess();$runnerProcess.Refresh();$Context.process.Refresh()
            $runnerBefore=[Int64]$runnerProcess.WorkingSet64;$appBefore=[Int64]$Context.process.WorkingSet64;$queryCountStart=@($Context.uiaQueryMetrics).Count;$probeTimer=[Diagnostics.Stopwatch]::StartNew()
            $noticeWindow=Get-OptionalUiaByName $Context 'Reports Ready' 'ControlType.Window'
            $batchDialog=$null;if($null -eq $noticeWindow){$batchDialog=Get-OptionalUiaById $Context 'nextSpeakingEvaluationBatchReports'}
            $probeTimer.Stop();$runnerProcess.Refresh();$Context.process.Refresh()
            $runnerAfter=[Int64]$runnerProcess.WorkingSet64;$appAfter=if($Context.process.HasExited){0L}else{[Int64]$Context.process.WorkingSet64}
            $probeRecord=[ordered]@{
                status=if($null -ne $noticeWindow){'reports-ready-notice-found'}elseif($null -eq $batchDialog){'batch-dialog-closed'}else{'batch-dialog-remained-open'}
                attemptCount=1;maximumAttempts=1;archiveStableCheckCount=3;queryCountStart=$queryCountStart;queryCountEnd=@($Context.uiaQueryMetrics).Count;materializedTreeElementCount=0
                processId=[Int64]$Context.process.Id;mainWindowHandle=[Int64]$Context.mainWindowHandle;mainWindowProcessId=[Int64][P913ScreenshotNative]::GetWindowProcessId([IntPtr]$Context.mainWindowHandle)
                noticeWindowFound=($null -ne $noticeWindow);batchDialogFound=($null -ne $batchDialog);runnerProcessId=[Int64]$runnerProcess.Id
                runnerWorkingSetBeforeBytes=$runnerBefore;runnerWorkingSetAfterBytes=$runnerAfter;appWorkingSetBeforeBytes=$appBefore;appWorkingSetAfterBytes=$appAfter
                elapsedMilliseconds=[Math]::Round($probeTimer.Elapsed.TotalMilliseconds,3);capturedAtUtc=Get-UtcTimestamp
            }
            $script:Manifest.exportCompletionUiaProbe=$probeRecord
            [void](Add-RunnerMetric $Context 'export-completion-uia-probe')
            Write-JsonFile $ManifestPath $script:Manifest
            if($runnerAfter -ge [Int64]805306368){throw "PowerShell harness working set reached the 768 MiB safety guard during the single export-completion UIA probe: $runnerAfter bytes."}
            if($null -ne $noticeWindow){return [ordered]@{archiveStable=$true;noticeWindow=$noticeWindow;archivePath=$ArchivePath;uiaCompletionAttempts=1}}
            if($null -eq $batchDialog){return [ordered]@{archiveStable=$true;noticeWindow=$null;archivePath=$ArchivePath;uiaCompletionAttempts=1}}
            throw 'Archive committed, but one bounded completion check found the batch dialog still open and no Reports Ready notice.'
        }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for archive commit and export completion after $TimeoutSeconds seconds."
}

function Invoke-SpeakingEvaluationRoute {
    param([object]$Context,[string]$WorkspacePath)
    $window=Wait-MainWindow $Context $ReadinessTimeoutSeconds
    Save-UiaSnapshot $Context 'startup-ui-tree-before-navigation' | Out-Null
    $navTree=Wait-UiaById $Context 'classMngrNextNavigationTree' $ReadinessTimeoutSeconds 'Next navigation tree' 'ControlType.Tree'
    $startupSummary=Get-UiCounts $Context
    if($startupSummary.controls.nextSpeakingEvaluationReports -ne 0 -or $startupSummary.controls.nextSpeakingEvaluationBatchReports -ne 0 -or $startupSummary.controls.nextReportStudent -ne 0){throw 'Startup unexpectedly loaded a speaking-report dialog or student preview.'}
    $script:Manifest.startupReady=[ordered]@{capturedAtUtc=Get-UtcTimestamp;mainWindowTitle=$Context.mainWindowTitle;mainWindowHandle=[Int64]$Context.mainWindowHandle;windowProcessId=[Int64]$window.Current.ProcessId;workspacePath=[IO.Path]::GetFullPath($WorkspacePath);reportsLoaded=$false;ui=$startupSummary;uia=Save-UiaSnapshot $Context 'startup-ready'}
    [void](Add-ProcessMetric $Context 'startup-ready' 'normal-use')
    [void](Add-RunnerMetric $Context 'startup-ready')
    Write-JsonFile $ManifestPath $script:Manifest

    $speakingNav=Get-UiaByName $Context 'Speaking Evaluation' 'ControlType.TreeItem'
    Drain-ProcessOutput $Context
    $navigationArrivalBefore=[Int64]$Context.arrival
    $script:Manifest.uiaActions.navigateToSpeakingEvaluation=Invoke-UiaAction $Context $speakingNav 'navigate-to-speaking-evaluation' 'Speaking Evaluation' 'speaking_evaluation' -UseWindowTargetedClick
    $pageRoute=Wait-ForPageRoute $Context $navigationArrivalBefore 'speaking_evaluation' $ReadinessTimeoutSeconds
    $script:Manifest.uiaActions.navigateToSpeakingEvaluation.logArrivalIndexBeforeAction=$navigationArrivalBefore
    $script:Manifest.uiaActions.navigateToSpeakingEvaluation.verifiedByFollowingPageRoute=$true
    $script:Manifest.uiaActions.navigateToSpeakingEvaluation.pageRouteEvent=$pageRoute
    Add-JsonLine $UiaActionsPath ([ordered]@{recordType='page-route-observation';associatedActionKind='navigate-to-speaking-evaluation';targetPageId='speaking_evaluation';observedPageRouteEvent=$pageRoute})
    Write-JsonFile $ManifestPath $script:Manifest
    Save-UiaSnapshot $Context 'speaking-evaluation-page-ui-tree-before-controls' | Out-Null
    $classList=Wait-UiaByIdAfterRoute $Context 'speakingEvaluationClassList' 'speaking-evaluation class list'
    $classItem=Find-UiaChildByName $Context $classList 'speakingEvaluationClassItem' 'Heavy Fixture Class 001'
    if($null -eq $classItem){throw 'Verified class list did not expose the expected immediate Heavy Fixture Class 001 ListItem.'}
    $script:Manifest.uiaActions.selectFixtureClass=Invoke-UiaAction $Context $classItem 'select-fixture-class' 'Heavy Fixture Class 001' 'speaking_evaluation' -UseSelectionPattern
    $choice=Wait-UiaByIdAfterRoute $Context 'speakingEvaluationChoice' 'speaking-evaluation selector'
    $evalName=Get-ElementLabel $choice
    if($evalName -cne 'Winter'){throw "UIA evaluation selector is '$evalName'; expected the approved UI-valid Winter evaluation."}
    $sourceStudentCount=[int]$script:WorkspaceReceipt.selectedEvaluation.studentCount
    if($sourceStudentCount -ne $ExpectedWorkspaceStudentCount){throw "Workspace receipt reports $sourceStudentCount source rows; expected all $ExpectedWorkspaceStudentCount."}
    $reportsButton=Get-UiaById $Context 'speakingEvaluationReports' 'ControlType.Button'
    if(-not $reportsButton.Current.IsEnabled){throw 'Speaking Evaluation Reports button is disabled for the selected fixture evaluation.'}
    $pageSummary=Get-UiCounts $Context
    $script:Manifest.checkpoints['speaking-evaluation-page-ready']=[ordered]@{capturedAtUtc=Get-UtcTimestamp;selectedClass='Heavy Fixture Class 001';selectedEvaluation=$evalName;sourceStudentCount=$sourceStudentCount;expectedSourceStudentCount=$ExpectedWorkspaceStudentCount;expectedExportReportCount=$ExpectedReportCount;classListAutomationId=$classList.Current.AutomationId;ui=$pageSummary;uia=Save-UiaSnapshot $Context 'speaking-evaluation-page-ready';metric=Add-ProcessMetric $Context 'speaking-evaluation-page-ready' 'normal-use'}
    [void](Add-RunnerMetric $Context 'speaking-evaluation-page-ready')
    Write-JsonFile $ManifestPath $script:Manifest
    $window=$null;$navTree=$null;$startupSummary=$null;$speakingNav=$null;$pageRoute=$null
    $classList=$null;$classItem=$null;$choice=$null;$pageSummary=$null
    [void](Assert-RunnerWorkingSet $Context 'speaking-evaluation-page-ready-released-uia-roots')
    if($ProbeOnly){Invoke-UiaQueryBurstDiagnostic $Context;return}

    $script:Manifest.uiaActions.openDirectPreview=Invoke-UiaAction $Context $reportsButton 'open-direct-report-preview' 'Reports' 'speaking_evaluation'
    $reportsButton=$null
    $direct=Wait-UiaById $Context 'nextSpeakingEvaluationReports' $ReadinessTimeoutSeconds 'direct speaking-report preview' 'ControlType.Window'
    $student=Get-UiaById $Context 'nextReportStudent' 'ControlType.ComboBox'
    $studentName=Get-ElementLabel $student
    if($studentName -cne $script:ExpectedCurrentReportName){throw "Direct preview selected report '$studentName'; expected $($script:ExpectedCurrentReportName)."}
    $directSummary=Get-UiCounts $Context
    if($directSummary.controls.nextReportClose -ne 1 -or $directSummary.controls.nextReportSavePdf -ne 1){throw 'Direct report preview lacks its explicit Close or Save PDF UIA control.'}
    $script:Manifest.checkpoints['direct-preview-open']=[ordered]@{capturedAtUtc=Get-UtcTimestamp;selectedStudent=$studentName;ui=$directSummary;uia=Save-UiaSnapshot $Context 'direct-preview-open';metric=Add-ProcessMetric $Context 'direct-preview-visible' 'operation'}
    $script:Manifest.screenshots+=,(Get-ScreenshotForElement $Context 'direct-report-preview' $direct)
    Write-JsonFile $ManifestPath $script:Manifest
    $closePreview=Get-UiaById $Context 'nextReportClose' 'ControlType.Button'
    $script:Manifest.uiaActions.closeDirectPreview=Invoke-UiaAction $Context $closePreview 'explicit-close-direct-report-preview' 'Close' 'speaking_evaluation'
    Wait-UiaAbsent $Context 'nextSpeakingEvaluationReports' 15 'explicit direct report preview close'
    $closedDirect=Get-UiCounts $Context
    if($closedDirect.controls.nextReportStudent -ne 0 -or $closedDirect.controls.nextReportClose -ne 0){throw 'Direct report preview controls remained exposed after Close.'}
    $script:Manifest.checkpoints['direct-preview-closed']=[ordered]@{capturedAtUtc=Get-UtcTimestamp;ui=$closedDirect;uia=Save-UiaSnapshot $Context 'direct-preview-closed';metric=Add-ProcessMetric $Context 'direct-preview-closed' 'normal-use'}
    Write-JsonFile $ManifestPath $script:Manifest
    $direct=$null;$student=$null;$closePreview=$null;$closedDirect=$null;$directSummary=$null
    Release-RouteObjects $Context 'direct-preview-uia-release'

    $saveBatch=Get-UiaById $Context 'speakingEvaluationSaveReports' 'ControlType.Button'
    if(-not $saveBatch.Current.IsEnabled){throw 'Save Reports As button is disabled for the selected fixture evaluation.'}
    $script:Manifest.uiaActions.openBatchDialog=Invoke-UiaAction $Context $saveBatch 'open-batch-save-dialog' ('Save Reports As'+[char]0x2026) 'speaking_evaluation'
    $batch=Wait-UiaById $Context 'nextSpeakingEvaluationBatchReports' $ReadinessTimeoutSeconds 'speaking-report batch save dialog' 'ControlType.Window'
    $scope=Get-UiaById $Context 'nextReportScope' 'ControlType.ComboBox';$scopeName=Get-ElementLabel $scope
    if($scopeName -notmatch '^All Students \(25\)$'){throw "Batch scope is '$scopeName'; expected All Students (25) under isolated default settings."}
    $renderer=Get-UiaById $Context 'nextReportRenderer' 'ControlType.ComboBox';$rendererName=Get-ElementLabel $renderer
    if($rendererName -notmatch 'Internal'){throw "Batch export renderer '$rendererName' is not the internal Qt PDF renderer."}
    $outputDirectory=Join-Path $EvidenceDirectory 'output';$directory=Get-UiaById $Context 'nextReportDirectory' 'ControlType.Edit'
    $script:Manifest.uiaActions.setBatchOutputDirectory=Set-UiaValue $Context $directory $outputDirectory 'set-unique-batch-output-directory'
    $keepState=Get-CheckBoxState $Context 'nextReportKeepPdfs';$openState=Get-CheckBoxState $Context 'nextReportOpenOutput'
    if($keepState -ne 0 -or $openState -ne 0){throw 'Batch dialog defaults would save individual PDFs or open Explorer; output contract requires ZIP-only and no external folder launch.'}
    $script:Manifest.checkpoints['batch-dialog-ready']=[ordered]@{
        capturedAtUtc=Get-UtcTimestamp;scope=$scopeName;renderer=$rendererName;keepIndividualPdfs=$false;openOutputFolder=$false
        outputDirectory=$outputDirectory;ui=Get-UiCounts $Context;uia=Save-UiaSnapshot $Context 'batch-dialog-ready'
        metric=Add-ProcessMetric $Context 'batch-dialog-ready' 'operation'
    }
    Write-JsonFile $ManifestPath $script:Manifest

    $batchPreviewButton=Get-UiaById $Context 'nextReportBatchPreview' 'ControlType.Button'
    $script:Manifest.uiaActions.openBatchPreview=Invoke-UiaAction $Context $batchPreviewButton 'open-batch-preview' 'Preview Reports' 'speaking_evaluation'
    $batchPreview=Wait-UiaById $Context 'nextSpeakingEvaluationReports' $ReadinessTimeoutSeconds 'nested report preview' 'ControlType.Window'
    $batchStudent=Get-UiaById $Context 'nextReportStudent' 'ControlType.ComboBox';$batchStudentName=Get-ElementLabel $batchStudent
    if($batchStudentName -cne $script:ExpectedCurrentReportName){throw "Batch preview selected report '$batchStudentName'; expected $($script:ExpectedCurrentReportName)."}
    $previewSummary=Get-UiCounts $Context
    if($previewSummary.controls.nextReportClose -ne 1 -or $previewSummary.controls.nextReportSavePdf -ne 1){throw 'Nested batch preview lacks its explicit Close or Save PDF UIA control.'}
    $script:Manifest.checkpoints['batch-preview-open']=[ordered]@{capturedAtUtc=Get-UtcTimestamp;selectedStudent=$batchStudentName;selectedScope=$scopeName;ui=$previewSummary;uia=Save-UiaSnapshot $Context 'batch-preview-open';metric=Add-ProcessMetric $Context 'batch-preview-visible' 'operation'}
    $script:Manifest.screenshots+=,(Get-ScreenshotForElement $Context 'batch-report-preview' $batchPreview)
    Write-JsonFile $ManifestPath $script:Manifest
    $batchPreviewClose=Get-UiaById $Context 'nextReportClose' 'ControlType.Button'
    $script:Manifest.uiaActions.closeBatchPreview=Invoke-UiaAction $Context $batchPreviewClose 'explicit-close-batch-preview' 'Close' 'speaking_evaluation'
    Wait-UiaAbsent $Context 'nextSpeakingEvaluationReports' 15 'explicit nested report preview close'
    $batch=Get-UiaById $Context 'nextSpeakingEvaluationBatchReports' 'ControlType.Window'
    if($null -eq $batch){throw 'Closing batch preview also closed the parent batch save dialog.'}
    $script:Manifest.checkpoints['batch-preview-closed']=[ordered]@{capturedAtUtc=Get-UtcTimestamp;ui=Get-UiCounts $Context;uia=Save-UiaSnapshot $Context 'batch-preview-closed';metric=Add-ProcessMetric $Context 'batch-preview-closed' 'normal-use'}
    Write-JsonFile $ManifestPath $script:Manifest
    $batch=$null;$batchPreview=$null;$batchStudent=$null;$batchPreviewButton=$null;$batchPreviewClose=$null
    $previewSummary=$null;$batchDialogReady=$null;$scope=$null;$renderer=$null;$directory=$null
    Release-RouteObjects $Context 'batch-preview-uia-release'

    $archiveName=[IO.Path]::GetFileName($outputDirectory)+'.zip';$archivePath=Join-Path $outputDirectory $archiveName
    if(Test-Path -LiteralPath $archivePath){throw 'Unique report output archive path unexpectedly exists before export.'}
    $beforeStaging=@(Get-TempStagingDirectories)
    $script:Manifest.output=[ordered]@{directory=$outputDirectory;archivePath=$archivePath;archiveRelativePath=(Get-RelativePath $EvidenceDirectory $archivePath);expectedSourceStudentCount=$ExpectedWorkspaceStudentCount;expectedPdfCount=$ExpectedReportCount;expectedScope=$scopeName;expectedReportNames=@($script:WorkspaceReceipt.selectedEvaluation.reportNames | Select-Object -First $ExpectedReportCount);excludedSourceRowLabels=@($script:WorkspaceReceipt.selectedEvaluation.sourceRowLabels | Select-Object -Skip $ExpectedReportCount);keepIndividualPdfFiles=$false;openedFolder=$false;stagingDirectoriesBefore=$beforeStaging}
    $exportButton=Get-UiaById $Context 'nextReportBatchExport' 'ControlType.Button'
    $script:Manifest.uiaActions.exportBatch=Invoke-UiaAction $Context $exportButton 'export-all-students-to-zip' 'Save As...' 'speaking_evaluation'
    Write-JsonFile $ManifestPath $script:Manifest
    $exportCompletion=Wait-ForExportCompletion $Context $archivePath $ExportTimeoutSeconds
    $script:Manifest.output.archiveSeenAtCompletion=$exportCompletion.archiveStable
    if($null -ne $exportCompletion.noticeWindow){
        $noticeElement=$exportCompletion.noticeWindow
        $noticeButton=Get-UiaById $Context 'qt_msgbox_buttonbox.QPushButton' 'ControlType.Button'
        $script:Manifest.checkpoints['export-notice']=[ordered]@{capturedAtUtc=Get-UtcTimestamp;title=[string]$noticeElement.Current.Name;uia=Save-UiaSnapshot $Context 'export-notice';metric=Add-ProcessMetric $Context 'export-notice-visible' 'operation'}
        $script:Manifest.uiaActions.dismissExportNotice=Invoke-UiaAction $Context $noticeButton 'dismiss-success-notice' 'OK' 'speaking_evaluation'
        Wait-UiaAbsent $Context 'qt_msgbox_buttonbox' 15 'success notice dismissal'
    }
    Wait-UiaAbsent $Context 'nextSpeakingEvaluationBatchReports' 15 'batch dialog completion after export'
    Drain-ProcessOutput $Context
    $afterStaging=@(Get-TempStagingDirectories)
    if($afterStaging.Count -ne 0){throw 'A ClassMngr speaking-evaluation QTemporaryDir remains after export completion.'}
    Stop-TempWatcher;Drain-ProcessOutput $Context
    $stagingEvents=@(Get-Content -LiteralPath $WatcherPath -Encoding UTF8 | Where-Object {$_ -match '^created\t' -or $_ -match '^deleted\t'})
    $script:Manifest.temporaryStaging=[ordered]@{
        environmentTempRoot=[IO.Path]::GetFullPath((Join-Path (Join-Path $EvidenceDirectory 'runtime') 'temp'))
        prefix='ClassMngr-speaking-evaluations-';watcherPath='runtime/temp-directory-events.tsv';watcherSha256=(Get-FileSha256 $WatcherPath)
        before=$beforeStaging;after=$afterStaging;observedEvents=$stagingEvents;absenceVerifiedAfterCompletion=($afterStaging.Count -eq 0)
    }
    $allFiles=@(Get-ChildItem -LiteralPath $outputDirectory -File -Force)
    if($allFiles.Count -ne 1 -or $allFiles[0].FullName -cne $archivePath){throw "Expected only the batch archive in output directory; found $($allFiles.Count) files."}
    $script:Manifest.output.archiveBytes=[Int64](Get-Item -LiteralPath $archivePath).Length
    $script:Manifest.output.archiveSha256=Get-FileSha256 $archivePath
    $script:Manifest.output.outputFiles=@($allFiles | ForEach-Object {[ordered]@{name=$_.Name;bytes=[Int64]$_.Length;sha256=(Get-FileSha256 $_.FullName)}})
    $script:Manifest.output.stagingDirectoriesAfter=$afterStaging
    $script:Manifest.checkpoints['export-complete']=[ordered]@{capturedAtUtc=Get-UtcTimestamp;ui=Get-UiCounts $Context;uia=Save-UiaSnapshot $Context 'export-complete';metric=Add-ProcessMetric $Context 'export-complete' 'normal-use'}
    Write-JsonFile $ManifestPath $script:Manifest
    $exportCompletion=$null;$noticeElement=$null;$noticeButton=$null;$exportButton=$null
    $afterStaging=$null;$stagingEvents=$null;$allFiles=$null;$beforeStaging=$null
    Release-RouteObjects $Context 'export-complete-uia-release'

    $navigationTree=Get-UiaById $Context 'classMngrNextNavigationTree' 'ControlType.Tree'
    $myWorkspace=Find-UiaChildByName $Context $navigationTree 'myWorkspaceNavigationItem' 'My Workspace'
    Drain-ProcessOutput $Context
    $navigateAwayArrivalBefore=[Int64]$Context.arrival
    $script:Manifest.uiaActions.navigateAway=Invoke-UiaAction $Context $myWorkspace 'navigate-away-after-report-close' 'My Workspace' 'my_workspace' -UseWindowTargetedClick
    $awayRoute=Wait-ForPageRoute $Context $navigateAwayArrivalBefore 'my_workspace' $ReadinessTimeoutSeconds
    $script:Manifest.uiaActions.navigateAway.logArrivalIndexBeforeAction=$navigateAwayArrivalBefore
    $script:Manifest.uiaActions.navigateAway.verifiedByFollowingPageRoute=$true
    $script:Manifest.uiaActions.navigateAway.pageRouteEvent=$awayRoute
    Add-JsonLine $UiaActionsPath ([ordered]@{recordType='page-route-observation';associatedActionKind='navigate-away-after-report-close';targetPageId='my_workspace';observedPageRouteEvent=$awayRoute})
    Write-JsonFile $ManifestPath $script:Manifest
    $timer=[Diagnostics.Stopwatch]::StartNew();$away=$false
    while($timer.Elapsed.TotalSeconds -lt $ReadinessTimeoutSeconds){Drain-ProcessOutput $Context;if($null -eq (Get-OptionalUiaById $Context 'speakingEvaluationReports') -and $null -eq (Get-OptionalUiaById $Context 'nextSpeakingEvaluationReports')){$away=$true;break};Start-Sleep -Milliseconds 100}
    if(-not $away){throw 'Speaking Evaluation page controls remained current after navigation to My Workspace.'}
    $retained=Get-UiCounts $Context
    if($retained.controls.nextSpeakingEvaluationReports -ne 0 -or $retained.controls.nextReportStudent -ne 0 -or $retained.controls.nextReportClose -ne 0){throw 'A speaking-report preview or its student control remained UIA-exposed after navigating away.'}
    $script:Manifest.checkpoints['navigated-away']=[ordered]@{
        capturedAtUtc=Get-UtcTimestamp;targetPage='my_workspace';reportPreviewRetained=$false;reportPreviewControlsRetained=$false
        pageRouteEvent=$awayRoute;ui=$retained;uia=Save-UiaSnapshot $Context 'navigated-away';metric=Add-ProcessMetric $Context 'navigated-away' 'normal-use'
    }
    $script:Manifest.screenshots+=,(Get-ScreenshotForElement $Context 'navigated-away' (Get-MainWindow $Context))
    $myWorkspace=$null;$navigationTree=$null;$retained=$null
    Release-RouteObjects $Context 'navigated-away-uia-release'
    $script:Manifest.lifecycleEvents=@($Context.lifecycleRecords | ForEach-Object {$_.record})
    $writerEvents=@($script:Manifest.lifecycleEvents | Where-Object {$_.owner -ceq 'pdf_generation' -and $_.event -in @('pdf-writer-create','pdf-writer-release-before','pdf-writer-release-after')})
    $script:Manifest.writerLifecycle=[ordered]@{
        createCount=@($writerEvents | Where-Object {$_.event -ceq 'pdf-writer-create'}).Count
        releaseBeforeCount=@($writerEvents | Where-Object {$_.event -ceq 'pdf-writer-release-before'}).Count
        releaseAfterCount=@($writerEvents | Where-Object {$_.event -ceq 'pdf-writer-release-after'}).Count
        distinctWriterIds=@($writerEvents | ForEach-Object {[Int64]$_.instance_id} | Sort-Object -Unique).Count
        allWriterEventsPath='application-output.jsonl'
    }
    $script:Manifest.logs=[ordered]@{
        processStdoutPath='logs/process-stdout.log';processStderrPath='logs/process-stderr.log'
        applicationOutputPath='application-output.jsonl';processMetricsPath='process-metrics.jsonl';uiaActionsPath='uia-actions.jsonl'
        runnerMemoryCheckpointsPath='runtime/runner-memory-checkpoints.jsonl'
        shutdownTracePath='runtime/diagnostics/application-shutdown.jsonl'
    }
    $script:Manifest.runnerMemoryCheckpoints=Get-RunnerMemorySummary
    $script:Manifest.operationPeakWorkingSetBytes=Get-OperationPeakWorkingSet
    Write-JsonFile $ManifestPath $script:Manifest
}

function Write-FailureAndClose {
    param([string]$Message)
    $script:Manifest.status='failed';$script:Manifest.failure=[ordered]@{message=$Message;capturedAtUtc=Get-UtcTimestamp}
    if($null -ne $script:Watcher){try{Stop-TempWatcher}catch{$script:Watcher.Dispose();$script:Watcher=$null}}
    if($null -ne $script:Context -and $null -eq $script:Context.processExit){try{$script:Manifest.processExitEvidence=Get-CloseObservation $script:Context}catch{$script:Manifest.processExitEvidence=[ordered]@{closeError=$_.Exception.Message;forcedTerminationForCleanup=$true;normalExit=$false}}}
    $script:Manifest.completedAtUtc=Get-UtcTimestamp
    try{if($null -ne $script:Context){$script:Manifest.lifecycleEvents=@($script:Context.lifecycleRecords | ForEach-Object {$_.record});$script:Manifest.logs=[ordered]@{processStdoutPath='logs/process-stdout.log';processStderrPath='logs/process-stderr.log';applicationOutputPath='application-output.jsonl';processMetricsPath='process-metrics.jsonl';uiaActionsPath='uia-actions.jsonl';runnerMemoryCheckpointsPath='runtime/runner-memory-checkpoints.jsonl'};$script:Manifest.runnerMemoryCheckpoints=Get-RunnerMemorySummary}}catch{}
    Write-JsonFile $ManifestPath $script:Manifest -AllowOverGuard
}

try {
    if($FinalizeOnly){
        Invoke-FinalizeOnly
        return
    }
    Initialize-Provenance
    [void](Write-RunnerMemoryCheckpoint 'after-provenance-before-gc' $null)
    [GC]::Collect(2,[GCCollectionMode]::Forced,$true,$true)
    [GC]::WaitForPendingFinalizers()
    [GC]::Collect(2,[GCCollectionMode]::Forced,$true,$true)
    [void](Assert-RunnerWorkingSet $null 'after-provenance-gc')
    $workspace=Prepare-Workspace
    [void](Assert-RunnerWorkingSet $null 'after-workspace-preparation')
    $context=Start-PackagedProcess $workspace
    [void](Assert-RunnerWorkingSet $context 'after-packaged-app-start')
    Invoke-SpeakingEvaluationRoute $context $workspace
    if($ProbeOnly){
        $navigationTreeProbe=Get-UiaById $context 'classMngrNextNavigationTree' 'ControlType.Tree'
        $myWorkspaceProbe=Find-UiaChildByName $context $navigationTreeProbe 'myWorkspaceNavigationItem' 'My Workspace'
        $myWorkspaceProbeSnapshot=Get-UiaSnapshot $myWorkspaceProbe
        $myWorkspaceProbeOwner=Get-WindowForElement $context $myWorkspaceProbe
        $myWorkspaceProbeQuery=$context.uiaQueryMetrics[$context.uiaQueryMetrics.Count-1]
        $script:Manifest.navigationItemProbe=[ordered]@{
            target=$myWorkspaceProbeSnapshot;processId=[Int64]$context.process.Id
            windowHandle=[Int64]$myWorkspaceProbeOwner.Current.NativeWindowHandle
            windowProcessId=[Int64]$myWorkspaceProbeOwner.Current.ProcessId
            query=$myWorkspaceProbeQuery;capturedAtUtc=Get-UtcTimestamp
        }
        [void](Add-RunnerMetric $context 'my-workspace-navigation-item-probe')
        Write-JsonFile $ManifestPath $script:Manifest
        Drain-ProcessOutput $context
        $probeAwayArrivalBefore=[Int64]$context.arrival
        $script:Manifest.uiaActions.navigateAwayProbe=Invoke-UiaAction $context $myWorkspaceProbe 'navigate-to-my-workspace-probe' 'My Workspace' 'my_workspace' -UseWindowTargetedClick
        $probeAwayRoute=Wait-ForPageRoute $context $probeAwayArrivalBefore 'my_workspace' $ReadinessTimeoutSeconds
        $script:Manifest.uiaActions.navigateAwayProbe.logArrivalIndexBeforeAction=$probeAwayArrivalBefore
        $script:Manifest.uiaActions.navigateAwayProbe.verifiedByFollowingPageRoute=$true
        $script:Manifest.uiaActions.navigateAwayProbe.pageRouteEvent=$probeAwayRoute
        Add-JsonLine $UiaActionsPath ([ordered]@{recordType='page-route-observation';associatedActionKind='navigate-to-my-workspace-probe';targetPageId='my_workspace';observedPageRouteEvent=$probeAwayRoute})
        $workspaceTitleProbe=Wait-UiaByIdAfterRoute $context 'myWorkspaceTitle' 'My Workspace probe page title'
        if([string]$workspaceTitleProbe.Current.Name -cne 'My Workspace'){throw 'My Workspace page title did not match the completed probe route.'}
        $probeWorkspaceSnapshot=Save-UiaSnapshot $context 'probe-my-workspace-after-navigation'
        $probeWorkspaceUi=Get-UiCounts $context
        if($probeWorkspaceUi.controls.speakingEvaluationReports -ne 0 -or $probeWorkspaceUi.controls.nextSpeakingEvaluationReports -ne 0 -or $probeWorkspaceUi.controls.nextReportStudent -ne 0){throw 'Report controls remained UIA-exposed after the no-export My Workspace navigation probe.'}
        $script:Manifest.checkpoints['probe-my-workspace-route']=[ordered]@{
            capturedAtUtc=Get-UtcTimestamp;targetPage='my_workspace';pageRouteEvent=$probeAwayRoute
            reportControlsRetained=$false;ui=$probeWorkspaceUi;uia=$probeWorkspaceSnapshot
            metric=Add-ProcessMetric $context 'probe-my-workspace-route' 'normal-use'
        }
        [void](Add-RunnerMetric $context 'probe-my-workspace-route')
        Write-JsonFile $ManifestPath $script:Manifest
        $navigationTreeProbe=$null;$myWorkspaceProbe=$null;$myWorkspaceProbeOwner=$null
        $workspaceTitleProbe=$null;$probeWorkspaceUi=$null;$probeWorkspaceSnapshot=$null
        Release-RouteObjects $context 'probe-my-workspace-route-uia-release'
        $exit=Get-CloseObservation $context
        $script:Manifest.processExitEvidence=$exit
        if(-not $exit.normalExit){throw "Packaged Next UIA probe did not exit naturally with code 0 (exit=$($exit.exitCode), forced=$($exit.forcedTerminationForCleanup))."}
        $script:Manifest.processExit=$exit;$script:Manifest.status='probe-captured-awaiting-validation';$script:Manifest.completedAtUtc=Get-UtcTimestamp
        $script:Manifest.lifecycleEvents=@($context.lifecycleRecords | ForEach-Object {$_.record})
        Complete-PostExitEvidence $context
        Write-JsonFile $ManifestPath $script:Manifest
        $validator=Join-Path $PSScriptRoot 'validate_packaged_next_speaking_evaluation_lifecycle.py'
        $probeValidatorExit=Invoke-ValidatorToSidecar $validator $ValidationPath $ValidationStdoutPath $ValidationStderrPath
        if($probeValidatorExit -ne 0){throw "P9-13 UIA probe validator failed with exit code $probeValidatorExit."}
        $script:Manifest.status='probe-passed';$script:Manifest.validation=[ordered]@{exitCode=$probeValidatorExit;path='logs/validation.json';sha256=(Get-FileSha256 $ValidationPath);stdoutPath='logs/validator-stdout.txt';stdoutSha256=(Get-FileSha256 $ValidationStdoutPath)}
        Write-JsonFile $ManifestPath $script:Manifest
        Write-Output "P9-13 bounded packaged Next UIA probe evidence: $EvidenceDirectory"
        return
    }
    $exit=Get-CloseObservation $context
    $script:Manifest.processExitEvidence=$exit
    if(-not $exit.normalExit){throw "Packaged Next did not exit naturally with code 0 (exit=$($exit.exitCode), forced=$($exit.forcedTerminationForCleanup))."}
    $script:Manifest.processExit=$exit
    $script:Manifest.status='captured-awaiting-validation';$script:Manifest.completedAtUtc=Get-UtcTimestamp
    Complete-PostExitEvidence $context
    Write-JsonFile $ManifestPath $script:Manifest
    $validator=Join-Path $PSScriptRoot 'validate_packaged_next_speaking_evaluation_lifecycle.py'
    $validatorExit=Invoke-ValidatorToSidecar $validator $ValidationPath $ValidationStdoutPath $ValidationStderrPath
    if($validatorExit -ne 0){throw "P9-13 evidence validator failed with exit code $validatorExit."}
    $script:Manifest.status='passed';$script:Manifest.validation=[ordered]@{exitCode=$validatorExit;path='logs/validation.json';sha256=(Get-FileSha256 $ValidationPath);stdoutPath='logs/validator-stdout.txt';stdoutSha256=(Get-FileSha256 $ValidationStdoutPath)}
    Write-JsonFile $ManifestPath $script:Manifest
    $recheckPath=Join-Path (Join-Path $EvidenceDirectory 'logs') 'final-validation.json'
    $recheckStdoutPath=Join-Path (Join-Path $EvidenceDirectory 'logs') 'final-validator-stdout.txt'
    $recheckStderrPath=Join-Path (Join-Path $EvidenceDirectory 'logs') 'final-validator-stderr.txt'
    $recheckExit=Invoke-ValidatorToSidecar $validator $recheckPath $recheckStdoutPath $recheckStderrPath
    if($recheckExit -ne 0){$script:Manifest.status='validation-failed';Write-JsonFile $ManifestPath $script:Manifest;throw "P9-13 final manifest revalidation failed with exit code $recheckExit."}
    Write-JsonFile $ManifestPath $script:Manifest
    Write-Output "P9-13 packaged Next speaking-evaluation lifecycle evidence: $EvidenceDirectory"
}
catch {
    $message=$_.Exception.Message
    if($FinalizeOnly){
        try{
            $errorPath=Join-Path (Join-Path $EvidenceDirectory 'logs') 'finalization-error.json'
            $errorRecord=[ordered]@{
                taskId=$TaskId;mode='post-exit-finalization-from-immutable-raw-evidence';status='failed'
                sourceDirectory=$FinalizeSourceDirectory;sourceManifestSha256=if($FinalizeSourceDirectory -and [IO.File]::Exists((Join-Path $FinalizeSourceDirectory 'manifest.json'))){Get-FileSha256 (Join-Path $FinalizeSourceDirectory 'manifest.json')}else{$null}
                message=$message;capturedAtUtc=Get-UtcTimestamp
            }
            Write-JsonFile $errorPath $errorRecord
        }catch{}
    }else{
        try{Write-FailureAndClose $message}catch{}
    }
    throw
}
