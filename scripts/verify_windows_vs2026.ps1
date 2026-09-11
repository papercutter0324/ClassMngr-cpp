[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')]
    [string] $WindowsSdkVersion = '10.0.26100.0'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$visualStudioVersionRange = '[18.0,19.0)'
$visualStudioToolset = 'v145'

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

function Resolve-VsWherePath {
    $programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
    $programFiles = [Environment]::GetEnvironmentVariable('ProgramFiles')

    foreach ($programFilesRoot in @($programFilesX86, $programFiles)) {
        if ([string]::IsNullOrWhiteSpace($programFilesRoot)) {
            continue
        }

        $candidate = Join-Path $programFilesRoot 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    return Resolve-CommandPath -Names @('vswhere.exe', 'vswhere')
}

function Resolve-VsInstallationPath {
    $vswhere = Resolve-VsWherePath
    if ($null -eq $vswhere) {
        throw 'vswhere.exe was not found; Visual Studio 2026 cannot be verified.'
    }

    $installationPaths = & $vswhere `
        -all `
        -products * `
        -version $visualStudioVersionRange `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath

    foreach ($candidate in $installationPaths) {
        $installationPath = ([string] $candidate).Trim()
        if ([string]::IsNullOrWhiteSpace($installationPath)) {
            continue
        }

        $msbuildPath = Join-Path $installationPath 'MSBuild\Current\Bin\MSBuild.exe'
        if (-not (Test-Path -LiteralPath $msbuildPath -PathType Leaf)) {
            continue
        }

        $supportsWinUI = $true
        foreach ($platform in @('x64', 'Win32')) {
            $toolsetPath = Join-Path $installationPath `
                "MSBuild\Microsoft\VC\v180\Platforms\$platform\PlatformToolsets\$visualStudioToolset"
            $windowsStoreToolsetPath = Join-Path $installationPath `
                "MSBuild\Microsoft\VC\v180\Application Type\Windows Store\10.0\Platforms\$platform\PlatformToolsets\$visualStudioToolset"
            if (-not (Test-Path -LiteralPath $toolsetPath -PathType Container) `
                -or -not (Test-Path -LiteralPath $windowsStoreToolsetPath -PathType Container)) {
                $supportsWinUI = $false
                break
            }
        }

        if ($supportsWinUI) {
            return $installationPath
        }
    }

    throw 'No Visual Studio 2026 installation with the v145 Windows Store C++ workflow was found.'
}

$installationPath = Resolve-VsInstallationPath
$msbuildPath = Join-Path $installationPath 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuildPath -PathType Leaf)) {
    throw "Visual Studio 2026 MSBuild was not found: $msbuildPath"
}

$msvcPlatformsRoot = Join-Path $installationPath 'MSBuild\Microsoft\VC\v180\Platforms'
foreach ($platform in @('x64', 'Win32')) {
    $toolsetPath = Join-Path $msvcPlatformsRoot `
        "$platform\PlatformToolsets\$visualStudioToolset"
    if (-not (Test-Path -LiteralPath $toolsetPath -PathType Container)) {
        throw "Visual Studio 2026 $visualStudioToolset toolset was not found for ${platform}: $toolsetPath"
    }
}

$windowsStorePlatformsRoot = Join-Path $installationPath `
    'MSBuild\Microsoft\VC\v180\Application Type\Windows Store\10.0\Platforms'
foreach ($platform in @('x64', 'Win32')) {
    $toolsetPath = Join-Path $windowsStorePlatformsRoot `
        "$platform\PlatformToolsets\$visualStudioToolset"
    if (-not (Test-Path -LiteralPath $toolsetPath -PathType Container)) {
        throw "Visual Studio 2026 Windows Store $visualStudioToolset toolset was not found for ${platform}: $toolsetPath"
    }
}

$msvcToolsRoot = Join-Path $installationPath 'VC\Tools\MSVC'
if (-not (Test-Path -LiteralPath $msvcToolsRoot -PathType Container)) {
    throw "Visual Studio 2026 MSVC tools were not found: $msvcToolsRoot"
}
$msvcVersions = @(Get-ChildItem -LiteralPath $msvcToolsRoot -Directory |
    Where-Object { $_.Name -match '^14\.5' })
if ($msvcVersions.Count -eq 0) {
    throw "No Visual Studio 2026 MSVC 14.5x toolset was found under: $msvcToolsRoot"
}

$programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
if ([string]::IsNullOrWhiteSpace($programFilesX86)) {
    $programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles')
}
if ([string]::IsNullOrWhiteSpace($programFilesX86)) {
    throw 'Could not locate Program Files to verify the Windows SDK.'
}

$sdkIncludePath = Join-Path $programFilesX86 "Windows Kits\10\Include\$WindowsSdkVersion"
if (-not (Test-Path -LiteralPath $sdkIncludePath -PathType Container)) {
    throw "The required Windows SDK was not found: $sdkIncludePath"
}

Write-Host "Visual Studio 2026 installation: $installationPath"
Write-Host "MSBuild: $msbuildPath"
Write-Host "MSVC 14.5x directories: $(@($msvcVersions | ForEach-Object { $_.Name }) -join ', ')"
Write-Host "v145 platform toolsets: x64 and Win32 (including Windows Store application targets)"
Write-Host "Windows SDK: $sdkIncludePath"
Write-Host 'Visual Studio 2026/v145 preflight passed.'
