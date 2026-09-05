[CmdletBinding()]
param(
    [string]$Executable = '',

    [string]$OutputDirectory = '',

    [ValidatePattern('^[A-Za-z0-9._-]+$')]
    [string]$ScenarioName = 'winui-shell',

    [string[]]$Arguments = @(),

    [ValidateRange(0, 600000)]
    [int]$SettleMilliseconds = 1000,

    [ValidateRange(1000, 120000)]
    [int]$WindowTimeoutMilliseconds = 30000,

    [ValidateRange(1000, 120000)]
    [int]$CloseTimeoutMilliseconds = 10000,

    [string]$WindowTitle = '',

    [string]$DialogTitle = '',

    [switch]$PlanOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$plan = [ordered]@{
    format = 'classmngr-winui-scenario-plan-v1'
    scenario = [ordered]@{
        name = $ScenarioName
        arguments = @($Arguments)
        settleMilliseconds = $SettleMilliseconds
        windowTimeoutMilliseconds = $WindowTimeoutMilliseconds
        closeTimeoutMilliseconds = $CloseTimeoutMilliseconds
        windowTitle = $WindowTitle
        dialogTitle = $DialogTitle
    }
    steps = @(
        'launch-process',
        'wait-for-winui-window',
        'settle-window',
        'capture-window',
        'capture-and-close-dialog-when-requested',
        'request-window-close',
        'wait-for-process-exit',
        'verify-window-and-dialog-release'
    )
}

if ($PlanOnly)
{
    $plan | ConvertTo-Json -Depth 8
    return
}

if ([string]::IsNullOrWhiteSpace($Executable))
{
    throw 'Executable is required unless -PlanOnly is specified.'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory))
{
    throw 'OutputDirectory is required unless -PlanOnly is specified.'
}
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf))
{
    throw "WinUI executable was not found: $Executable"
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$resolvedOutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

$mainCapturePath = Join-Path $resolvedOutputDirectory "$ScenarioName.png"
$metadataPath = Join-Path $resolvedOutputDirectory "$ScenarioName.json"
$dialogCapturePath = ''
if ([string]::IsNullOrWhiteSpace($DialogTitle))
{
}
else
{
    $dialogCapturePath = Join-Path $resolvedOutputDirectory "$ScenarioName-dialog.png"
}

foreach ($artifactPath in @($mainCapturePath, $metadataPath, $dialogCapturePath))
{
    if ((-not [string]::IsNullOrWhiteSpace($artifactPath)) -and
        (Test-Path -LiteralPath $artifactPath))
    {
        throw "Refusing to overwrite scenario artifact: $artifactPath"
    }
}

Add-Type -AssemblyName System.Drawing
if (-not ('ClassMngrWinUIScenario.NativeMethods' -as [type]))
{
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace ClassMngrWinUIScenario
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

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int GetWindowTextLength(IntPtr window);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int GetWindowText(
            IntPtr window,
            StringBuilder title,
            int capacity
            );

        [DllImport("user32.dll")]
        public static extern bool IsWindow(IntPtr window);

        [DllImport("user32.dll")]
        public static extern bool IsWindowVisible(IntPtr window);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool GetWindowRect(
            IntPtr window,
            out Rect rectangle
            );

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool PostMessage(
            IntPtr window,
            uint message,
            IntPtr wParam,
            IntPtr lParam
            );

        public static IntPtr FindProcessWindow(int processId, string title)
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

                    string candidateTitle = GetTitle(window);
                    if (!string.IsNullOrWhiteSpace(title)
                        && candidateTitle.IndexOf(
                            title,
                            StringComparison.OrdinalIgnoreCase
                            ) < 0)
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

        public static string GetTitle(IntPtr window)
        {
            int length = GetWindowTextLength(window);
            StringBuilder title = new StringBuilder(Math.Max(length + 1, 1));
            GetWindowText(window, title, title.Capacity);
            return title.ToString();
        }
    }
}
'@
}

$events = [System.Collections.Generic.List[object]]::new()
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$ownedProcess = $null
$mainWindowHandle = [IntPtr]::Zero
$dialogWindowHandle = [IntPtr]::Zero
$mainWindowReleased = $false
$dialogReleased = [string]::IsNullOrWhiteSpace($DialogTitle)
$processExited = $false
$forcedTermination = $false
$failureMessage = $null
$result = 'blocked'
$mainCapture = $null
$dialogCapture = $null

function Add-ScenarioEvent
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [string]$Detail = ''
    )

    $events.Add([ordered]@{
        sequence = $events.Count + 1
        name = $Name
        elapsedMs = [Math]::Round($stopwatch.Elapsed.TotalMilliseconds, 1)
        detail = $Detail
    }) | Out-Null
}

function Wait-ForProcessWindow
{
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process]$Process,

        [string]$Title = '',

        [Parameter(Mandatory = $true)]
        [int]$TimeoutMilliseconds
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    while ([DateTime]::UtcNow -lt $deadline)
    {
        $Process.Refresh()
        if ($Process.HasExited)
        {
            throw "WinUI process exited before its window appeared (exit code $($Process.ExitCode))."
        }

        $window = [ClassMngrWinUIScenario.NativeMethods]::FindProcessWindow(
            $Process.Id,
            $Title
            )
        if ($window -ne [IntPtr]::Zero)
        {
            return $window
        }
        Start-Sleep -Milliseconds 100
    }

    throw "Timed out waiting for the WinUI window ($TimeoutMilliseconds ms)."
}

function Wait-ForWindowRelease
{
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Window,

        [Parameter(Mandatory = $true)]
        [int]$TimeoutMilliseconds
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    while ([DateTime]::UtcNow -lt $deadline)
    {
        if (-not [ClassMngrWinUIScenario.NativeMethods]::IsWindow($Window))
        {
            return $true
        }
        Start-Sleep -Milliseconds 100
    }
    return -not [ClassMngrWinUIScenario.NativeMethods]::IsWindow($Window)
}

function Get-WindowCapture
{
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Window,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $rectangle = [ClassMngrWinUIScenario.NativeMethods+Rect]::new()
    if (-not [ClassMngrWinUIScenario.NativeMethods]::GetWindowRect(
            $Window,
            [ref]$rectangle
            ))
    {
        throw "Could not read the bounds of window $Window."
    }

    $width = $rectangle.Right - $rectangle.Left
    $height = $rectangle.Bottom - $rectangle.Top
    if ($width -le 0 -or $height -le 0)
    {
        throw "WinUI window bounds are not capturable: $($width)x$($height)."
    }

    $bitmap = [System.Drawing.Bitmap]::new($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try
    {
        $graphics.CopyFromScreen(
            $rectangle.Left,
            $rectangle.Top,
            0,
            0,
            $bitmap.Size,
            [System.Drawing.CopyPixelOperation]::SourceCopy
            )
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally
    {
        $graphics.Dispose()
        $bitmap.Dispose()
    }

    return [ordered]@{
        file = [System.IO.Path]::GetFileName($Path)
        width = $width
        height = $height
        sha256 = ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash).ToLowerInvariant()
        method = 'CopyFromScreen'
    }
}

try
{
    Add-ScenarioEvent -Name 'launch-requested' -Detail $resolvedExecutable
    $startParameters = @{
        FilePath = $resolvedExecutable
        WorkingDirectory = [System.IO.Path]::GetDirectoryName($resolvedExecutable)
        PassThru = $true
        WindowStyle = 'Normal'
    }
    if (@($Arguments).Count -gt 0)
    {
        $startParameters.ArgumentList = @($Arguments)
    }

    $ownedProcess = Start-Process @startParameters
    Add-ScenarioEvent -Name 'process-started' -Detail "pid=$($ownedProcess.Id)"

    $mainWindowParameters = @{
        Process = $ownedProcess
        Title = $WindowTitle
        TimeoutMilliseconds = $WindowTimeoutMilliseconds
    }
    $mainWindowHandle = Wait-ForProcessWindow @mainWindowParameters
    Add-ScenarioEvent -Name 'window-shown' -Detail (
        "handle=$mainWindowHandle;title=" +
        [ClassMngrWinUIScenario.NativeMethods]::GetTitle($mainWindowHandle)
        )

    if ($SettleMilliseconds -gt 0)
    {
        Start-Sleep -Milliseconds $SettleMilliseconds
    }
    Add-ScenarioEvent -Name 'settled' -Detail "milliseconds=$SettleMilliseconds"

    Add-ScenarioEvent -Name 'capture-started' -Detail 'main-window'
    $mainCapture = Get-WindowCapture -Window $mainWindowHandle -Path $mainCapturePath
    Add-ScenarioEvent -Name 'captured' -Detail "main-window=$($mainCapture.file)"

    if (-not [string]::IsNullOrWhiteSpace($DialogTitle))
    {
        $dialogParameters = @{
            Process = $ownedProcess
            Title = $DialogTitle
            TimeoutMilliseconds = $WindowTimeoutMilliseconds
        }
        $dialogWindowHandle = Wait-ForProcessWindow @dialogParameters
        Add-ScenarioEvent -Name 'dialog-shown' -Detail (
            "handle=$dialogWindowHandle;title=" +
            [ClassMngrWinUIScenario.NativeMethods]::GetTitle($dialogWindowHandle)
            )
        $dialogCaptureParameters = @{
            Window = $dialogWindowHandle
            Path = $dialogCapturePath
        }
        $dialogCapture = Get-WindowCapture @dialogCaptureParameters
        Add-ScenarioEvent -Name 'dialog-captured' -Detail "file=$($dialogCapture.file)"
        if (-not [ClassMngrWinUIScenario.NativeMethods]::PostMessage(
                $dialogWindowHandle,
                0x0010,
                [IntPtr]::Zero,
                [IntPtr]::Zero
                ))
        {
            throw 'Could not request dialog close.'
        }
        $dialogReleaseParameters = @{
            Window = $dialogWindowHandle
            TimeoutMilliseconds = $CloseTimeoutMilliseconds
        }
        $dialogReleased = Wait-ForWindowRelease @dialogReleaseParameters
        Add-ScenarioEvent -Name 'dialog-released' -Detail "released=$dialogReleased"
        if (-not $dialogReleased)
        {
            throw 'The WinUI dialog window was not released after close.'
        }
    }

    Add-ScenarioEvent -Name 'close-requested' -Detail 'main-window'
    if (-not [ClassMngrWinUIScenario.NativeMethods]::PostMessage(
            $mainWindowHandle,
            0x0010,
            [IntPtr]::Zero,
            [IntPtr]::Zero
            ))
    {
        throw 'Could not request main-window close.'
    }
    if (-not $ownedProcess.WaitForExit($CloseTimeoutMilliseconds))
    {
        throw "WinUI process did not exit within $CloseTimeoutMilliseconds ms."
    }
    $processExited = $true
    Add-ScenarioEvent -Name 'process-exited' -Detail "exitCode=$($ownedProcess.ExitCode)"

    $mainReleaseParameters = @{
        Window = $mainWindowHandle
        TimeoutMilliseconds = $CloseTimeoutMilliseconds
    }
    $mainWindowReleased = Wait-ForWindowRelease @mainReleaseParameters
    Add-ScenarioEvent -Name 'resources-released' -Detail "mainWindowReleased=$mainWindowReleased"
    if (-not $mainWindowReleased)
    {
        throw 'The WinUI main window was not released after process exit.'
    }

    $result = 'passed'
}
catch
{
    $failureMessage = $_.Exception.Message
    $result = 'failed'
    Add-ScenarioEvent -Name 'scenario-failed' -Detail $failureMessage
}
finally
{
    if ($null -ne $ownedProcess)
    {
        try
        {
            $ownedProcess.Refresh()
            if (-not $ownedProcess.HasExited)
            {
                $forcedTermination = $true
                Add-ScenarioEvent -Name 'forced-cleanup' -Detail "pid=$($ownedProcess.Id)"
                $ownedProcess.Kill()
                $ownedProcess.WaitForExit(5000)
            }
            $ownedProcess.Refresh()
            $processExited = $ownedProcess.HasExited
        }
        catch
        {
            if ($null -eq $failureMessage)
            {
                $failureMessage = "Process cleanup failed: $($_.Exception.Message)"
                $result = 'failed'
            }
        }
    }

    if ($mainWindowHandle -ne [IntPtr]::Zero)
    {
        $mainWindowReleased = -not [ClassMngrWinUIScenario.NativeMethods]::IsWindow(
            $mainWindowHandle
            )
    }
    if ($dialogWindowHandle -ne [IntPtr]::Zero)
    {
        $dialogReleased = -not [ClassMngrWinUIScenario.NativeMethods]::IsWindow(
            $dialogWindowHandle
        )
    }

    $metadataProcessId = 0
    $metadataExitCode = $null
    if ($null -ne $ownedProcess)
    {
        $metadataProcessId = $ownedProcess.Id
        if ($processExited)
        {
            $metadataExitCode = $ownedProcess.ExitCode
        }
    }

    $metadata = [ordered]@{
        format = 'classmngr-winui-scenario-v1'
        capturedAtUtc = [DateTime]::UtcNow.ToString('o')
        scenario = [ordered]@{
            name = $ScenarioName
            arguments = @($Arguments)
            settleMilliseconds = $SettleMilliseconds
            windowTimeoutMilliseconds = $WindowTimeoutMilliseconds
            closeTimeoutMilliseconds = $CloseTimeoutMilliseconds
            windowTitle = $WindowTitle
            dialogTitle = $DialogTitle
        }
        host = [ordered]@{
            operatingSystem = [Environment]::OSVersion.VersionString
            architecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
            powershell = $PSVersionTable.PSVersion.ToString()
        }
        process = [ordered]@{
            executable = $resolvedExecutable
            id = $metadataProcessId
            exited = $processExited
            forcedTermination = $forcedTermination
            exitCode = $metadataExitCode
        }
        window = [ordered]@{
            title = $WindowTitle
            mainReleased = $mainWindowReleased
            capture = $mainCapture
        }
        dialog = [ordered]@{
            title = $DialogTitle
            requested = -not [string]::IsNullOrWhiteSpace($DialogTitle)
            released = $dialogReleased
            capture = $dialogCapture
        }
        steps = @($events.ToArray())
        result = [ordered]@{
            status = $result
            passed = $result -eq 'passed'
            failure = $failureMessage
        }
    }
    $json = $metadata | ConvertTo-Json -Depth 12
    [System.IO.File]::WriteAllText(
        $metadataPath,
        $json + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false)
        )
}

if ($null -ne $failureMessage)
{
    throw "WinUI scenario '$ScenarioName' failed: $failureMessage"
}

Write-Host "WinUI scenario '$ScenarioName' passed."
Write-Host "Capture: $mainCapturePath"
Write-Host "Metadata: $metadataPath"
