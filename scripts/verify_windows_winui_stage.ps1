[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $StageDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateSet('x64', 'Win32')]
    [string] $Platform
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-AbsolutePath {
    param([Parameter(Mandatory = $true)][string] $Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath(
        (Join-Path -Path (Get-Location).Path -ChildPath $Path)
    )
}

function Resolve-CommandPath {
    param([Parameter(Mandatory = $true)][string[]] $Names)

    foreach ($name in $Names) {
        $command = Get-Command -Name $name -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($null -ne $command) {
            if ($command.PSObject.Properties.Name -contains 'Path') {
                return $command.Path
            }
            return $command.Source
        }
    }

    return $null
}

function Resolve-VsInstallationPath {
    $programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
    $programFiles = [Environment]::GetEnvironmentVariable('ProgramFiles')
    $vswhere = $null
    foreach ($programFilesRoot in @($programFilesX86, $programFiles)) {
        if ([string]::IsNullOrWhiteSpace($programFilesRoot)) {
            continue
        }

        $candidate = Join-Path $programFilesRoot 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $vswhere = $candidate
            break
        }
    }
    if ($null -eq $vswhere) {
        $vswhere = Resolve-CommandPath -Names @('vswhere.exe', 'vswhere')
    }
    if ($null -eq $vswhere) {
        return $null
    }

    $installationPath = & $vswhere `
        -latest `
        -products * `
        -version '[18.0,19.0)' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath |
        Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace([string] $installationPath)) {
        return $null
    }

    return ([string] $installationPath).Trim()
}

function Resolve-DumpbinPath {
    $installationPath = Resolve-VsInstallationPath
    if ($null -ne $installationPath) {
        $platform = if ($Platform -eq 'Win32') { 'Win32' } else { 'x64' }
        $toolsetPath = Join-Path $installationPath `
            "MSBuild\Microsoft\VC\v180\Platforms\$platform\PlatformToolsets\v145"
        if (-not (Test-Path -LiteralPath $toolsetPath -PathType Container)) {
            throw "Visual Studio 2026 v145 toolset was not found for ${Platform}: $toolsetPath"
        }

        $msvcRoot = Join-Path $installationPath 'VC\Tools\MSVC'
        if (-not (Test-Path -LiteralPath $msvcRoot)) {
            throw "MSVC tools directory was not found: $msvcRoot"
        }

        $latestTools = Get-ChildItem -LiteralPath $msvcRoot -Directory |
            Where-Object { $_.Name -match '^14\.5' } |
            Sort-Object -Property Name -Descending |
            Select-Object -First 1
        if ($null -eq $latestTools) {
            throw "No VS 2026 MSVC 14.5x toolset directory was found under: $msvcRoot"
        }

        $candidates = @(
            (Join-Path $latestTools.FullName 'bin\Hostx64\x64\dumpbin.exe'),
            (Join-Path $latestTools.FullName 'bin\Hostx64\x86\dumpbin.exe')
        )
        foreach ($candidate in $candidates) {
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
        }

        throw "dumpbin.exe was not found under: $($latestTools.FullName)"
    }

    throw 'Visual Studio 2026 installation could not be resolved for dumpbin.exe.'
}

function Get-PeMachine {
    param([Parameter(Mandatory = $true)][string] $Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 64 `
        -or $bytes[0] -ne 0x4d `
        -or $bytes[1] -ne 0x5a) {
        throw "Not a PE image: $Path"
    }

    $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
    if ($peOffset -lt 0 -or $peOffset + 6 -gt $bytes.Length) {
        throw "Invalid PE header offset in: $Path"
    }
    if ($bytes[$peOffset] -ne 0x50 `
        -or $bytes[$peOffset + 1] -ne 0x45 `
        -or $bytes[$peOffset + 2] -ne 0x00 `
        -or $bytes[$peOffset + 3] -ne 0x00) {
        throw "PE signature was not found: $Path"
    }

    return [BitConverter]::ToUInt16($bytes, $peOffset + 4)
}

function Assert-Architecture {
    param(
        [Parameter(Mandatory = $true)][string] $Path,
        [Parameter(Mandatory = $true)][UInt16] $ExpectedMachine
    )

    $machine = Get-PeMachine -Path $Path
    if ($machine -ne $ExpectedMachine) {
        throw "Architecture mismatch in $Path. Expected machine 0x$('{0:X4}' -f $ExpectedMachine), found 0x$('{0:X4}' -f $machine)."
    }
}

function Assert-NoQtPayload {
    param([Parameter(Mandatory = $true)][string] $Root)

    $unexpected = Get-ChildItem -LiteralPath $Root -Recurse -Force |
        Where-Object {
            $relativePath = $_.FullName.Substring($Root.Length).TrimStart('\', '/')
            $_.Name -match '^Qt[0-9]+.*\.(dll|exe|pdb|lib)$' `
                -or $_.Name -ieq 'qt.conf' `
                -or $_.Name -ieq 'ClassMngr.exe' `
                -or $relativePath -match '(^|[\\/])(plugins|qml|translations)([\\/]|$)'
        }
    if ($null -ne $unexpected -and @($unexpected).Count -gt 0) {
        $names = @($unexpected | ForEach-Object { $_.FullName }) -join ', '
        throw "Qt payload was found in the WinUI stage: $names"
    }
}

function Invoke-StageSmokeTest {
    param(
        [Parameter(Mandatory = $true)][string] $Executable,
        [Parameter(Mandatory = $true)][string] $WorkingDirectory,
        [Parameter(Mandatory = $true)][string] $Argument
    )

    Write-Host "Launching $Argument"
    $process = Start-Process `
        -FilePath $Executable `
        -ArgumentList @($Argument) `
        -WorkingDirectory $WorkingDirectory `
        -WindowStyle Hidden `
        -PassThru
    if (-not $process.WaitForExit(60000)) {
        $process.Kill()
        throw "WinUI smoke test timed out: $Argument"
    }
    if ($process.ExitCode -ne 0) {
        throw "WinUI smoke test failed ($($process.ExitCode)): $Argument"
    }
}

$stagePath = Get-AbsolutePath -Path $StageDirectory
if (-not (Test-Path -LiteralPath $stagePath -PathType Container)) {
    throw "WinUI stage directory was not found: $stagePath"
}

$expectedMachine = [UInt16] 0x8664
if ($Platform -eq 'Win32') {
    $expectedMachine = [UInt16] 0x014c
}

$executablePath = Join-Path $stagePath 'ClassMngrWinUI.exe'
if (-not (Test-Path -LiteralPath $executablePath -PathType Leaf)) {
    throw "WinUI executable was not found: $executablePath"
}
Assert-Architecture -Path $executablePath -ExpectedMachine $expectedMachine

$resourceManifestPath = Join-Path $stagePath 'resources\manifest.json'
if (-not (Test-Path -LiteralPath $resourceManifestPath -PathType Leaf)) {
    throw "Staged resource manifest was not found: $resourceManifestPath"
}
$resourceManifest = Get-Content -LiteralPath $resourceManifestPath -Raw |
    ConvertFrom-Json
if ($resourceManifest.format -ne 'classmngr-native-resources-v1' `
    -or $null -eq $resourceManifest.entries) {
    throw "Staged resource manifest has an unexpected format: $resourceManifestPath"
}

Assert-NoQtPayload -Root $stagePath

$nativePayload = Get-ChildItem -LiteralPath $stagePath -Recurse -File |
    Where-Object { $_.Extension -in @('.exe', '.dll') }
foreach ($payload in $nativePayload) {
    Assert-Architecture -Path $payload.FullName -ExpectedMachine $expectedMachine
}

$requiredRuntimeFiles = @(
    'Microsoft.WindowsAppRuntime.Bootstrap.dll',
    'Microsoft.WindowsAppRuntime.dll',
    'Microsoft.UI.Xaml.dll'
)
foreach ($runtimeFile in $requiredRuntimeFiles) {
    $runtimePath = Join-Path $stagePath $runtimeFile
    if (-not (Test-Path -LiteralPath $runtimePath -PathType Leaf)) {
        throw "Self-contained Windows App SDK runtime file was not staged: $runtimePath"
    }
}

$xbfFiles = @(Get-ChildItem -LiteralPath $stagePath -Recurse -File -Filter '*.xbf')
if ($xbfFiles.Count -eq 0) {
    throw "No compiled XAML (.xbf) resources were found in: $stagePath"
}
$priFiles = @(Get-ChildItem -LiteralPath $stagePath -Recurse -File -Filter '*.pri')
if ($priFiles.Count -eq 0) {
    throw "No MRT resource (.pri) file was found in: $stagePath"
}
$applicationPriPath = Join-Path $stagePath 'ClassMngrWinUI.pri'
if (-not (Test-Path -LiteralPath $applicationPriPath -PathType Leaf)) {
    throw "Application MRT resource index was not staged: $applicationPriPath"
}

$dumpbin = Resolve-DumpbinPath
$dependencyOutput = (& $dumpbin /nologo /dependents $executablePath 2>&1 |
    Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin failed while inspecting $executablePath."
}
if ($dependencyOutput -match '(?im)\bQt[0-9A-Za-z_]*\.(dll|lib)\b') {
    throw "Qt dependency found in WinUI executable imports: $dependencyOutput"
}

Invoke-StageSmokeTest `
    -Executable $executablePath `
    -WorkingDirectory $stagePath `
    -Argument '--phase1-manifest-test'
Invoke-StageSmokeTest `
    -Executable $executablePath `
    -WorkingDirectory $stagePath `
    -Argument '--phase1-smoke-test'
Invoke-StageSmokeTest `
    -Executable $executablePath `
    -WorkingDirectory $stagePath `
    -Argument '--phase1-input-test'
Invoke-StageSmokeTest `
    -Executable $executablePath `
    -WorkingDirectory $stagePath `
    -Argument '--phase1-theme-test'
Invoke-StageSmokeTest `
    -Executable $executablePath `
    -WorkingDirectory $stagePath `
    -Argument '--phase1-dpi-test'
Invoke-StageSmokeTest `
    -Executable $executablePath `
    -WorkingDirectory $stagePath `
    -Argument '--phase3-lifecycle-test'
Invoke-StageSmokeTest `
    -Executable $executablePath `
    -WorkingDirectory $stagePath `
    -Argument '--phase3-navigation-test'
Invoke-StageSmokeTest `
    -Executable $executablePath `
    -WorkingDirectory $stagePath `
    -Argument '--phase3-view-model-test'
Invoke-StageSmokeTest `
    -Executable $executablePath `
    -WorkingDirectory $stagePath `
    -Argument '--phase3-localization-test'
Invoke-StageSmokeTest `
    -Executable $executablePath `
    -WorkingDirectory $stagePath `
    -Argument '--phase3-dialog-test'

Write-Host "Verified WinUI stage ($Platform): $stagePath"
