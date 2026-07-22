param(
    [ValidateSet("auto", "windows-desktop-release", "windows-laptop-release")]
    [string]$X64Preset = "auto",

    [switch]$SkipArm64
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-CMakeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & cmake @Arguments

    if ($LASTEXITCODE -ne 0) {
        throw "cmake $($Arguments -join ' ') failed with exit code $LASTEXITCODE."
    }
}

function Set-QtPrefixDefault {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $value = [Environment]::GetEnvironmentVariable($Name)

    if ([string]::IsNullOrWhiteSpace($value)) {
        [Environment]::SetEnvironmentVariable($Name, $Path)
    }
}

function Test-DirectoryExists {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return [System.IO.Directory]::Exists($Path)
}

function Get-QtPresetPrefix {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PresetName
    )

    $presetFile = [System.IO.Path]::GetFullPath(
        [System.IO.Path]::Combine($PSScriptRoot, "..", "CMakePresets.json")
    )
    $presets = Get-Content -LiteralPath $presetFile -Raw | ConvertFrom-Json
    $preset = @($presets.configurePresets) |
        Where-Object { $_.name -eq $PresetName } |
        Select-Object -First 1

    if ($null -eq $preset) {
        throw "CMake preset '$PresetName' was not found."
    }

    $cacheVariables = $preset.PSObject.Properties["cacheVariables"]

    if ($null -eq $cacheVariables) {
        throw "CMake preset '$PresetName' does not define cacheVariables."
    }

    $prefixPath = $cacheVariables.Value.PSObject.Properties["CMAKE_PREFIX_PATH"]

    if ($null -eq $prefixPath) {
        throw "CMake preset '$PresetName' does not define CMAKE_PREFIX_PATH."
    }

    return $prefixPath.Value
}

function Get-ProjectVersion {
    $cmakeFile = [System.IO.Path]::GetFullPath(
        [System.IO.Path]::Combine($PSScriptRoot, "..", "CMakeLists.txt")
    )
    $cmakeText = Get-Content -LiteralPath $cmakeFile -Raw
    $match = [regex]::Match(
        $cmakeText,
        'project\s*\(\s*ClassMngr\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)',
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase
    )

    if (-not $match.Success) {
        throw "Unable to read the ClassMngr version from CMakeLists.txt."
    }

    return $match.Groups[1].Value
}

function Require-QtPrefix {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        throw "$Name must point at a Qt MSVC installation."
    }

    if (-not (Test-DirectoryExists -Path $Value)) {
        throw "$Name points to '$Value', but that path does not exist."
    }
}

$desktopQtPrefix = Get-QtPresetPrefix -PresetName "qt-windows-desktop"
$laptopQtPrefix = Get-QtPresetPrefix -PresetName "qt-windows-laptop"
$environmentX64QtPrefix = [Environment]::GetEnvironmentVariable(
    "QT_MSVC_X64_PREFIX"
)

$x64PresetQtPrefixes = @{
    "windows-desktop-release" = $desktopQtPrefix
    "windows-laptop-release" = $laptopQtPrefix
}

function Resolve-X64Preset {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Preset
    )

    if ($Preset -ne "auto") {
        return $Preset
    }

    if (-not [string]::IsNullOrWhiteSpace($environmentX64QtPrefix)) {
        return "windows-desktop-release"
    }

    if (Test-DirectoryExists -Path $desktopQtPrefix) {
        return "windows-desktop-release"
    }

    if (Test-DirectoryExists -Path $laptopQtPrefix) {
        return "windows-laptop-release"
    }

    throw "No Windows x64 Qt prefix was found. Set QT_MSVC_X64_PREFIX or install Qt at '$desktopQtPrefix' or '$laptopQtPrefix'."
}

function Invoke-ReleasePreset {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Preset,
        [Parameter(Mandatory = $true)]
        [string]$QtPrefix
    )

    Write-Host "Building $Preset installer"

    $configureArguments = @(
        "--fresh",
        "--preset",
        $Preset,
        "-DCMAKE_PREFIX_PATH=$QtPrefix"
    )
    $signToolName = [Environment]::GetEnvironmentVariable(
        "CLASSMNGR_INSTALLER_SIGN_TOOL"
    )

    if (-not [string]::IsNullOrWhiteSpace($signToolName)) {
        $configureArguments +=
            "-DCLASSMNGR_INSTALLER_SIGN_TOOL=$signToolName"
    }

    Invoke-CMakeCommand -Arguments $configureArguments
    Invoke-CMakeCommand -Arguments @(
        "--build",
        "--preset",
        "$Preset-installer"
    )
}

$projectRoot = [System.IO.Path]::GetFullPath(
    [System.IO.Path]::Combine($PSScriptRoot, "..")
)
$outputDirectory = [System.IO.Path]::Combine($projectRoot, "dist")
$projectVersion = Get-ProjectVersion
$releaseArtifacts = [System.Collections.Generic.List[string]]::new()

$resolvedX64Preset = Resolve-X64Preset -Preset $X64Preset
$resolvedX64QtPrefix = if (
    [string]::IsNullOrWhiteSpace($environmentX64QtPrefix)
) {
    $x64PresetQtPrefixes[$resolvedX64Preset]
} else {
    $environmentX64QtPrefix
}

Require-QtPrefix -Name "Windows x64 Qt prefix" -Value $resolvedX64QtPrefix
$defaultArm64QtPrefix = [System.IO.Path]::Combine(
    [System.IO.Directory]::GetParent($resolvedX64QtPrefix).FullName,
    "msvc2022_arm64"
)
[Environment]::SetEnvironmentVariable(
    "QT_MSVC_X64_PREFIX",
    $resolvedX64QtPrefix
)
Invoke-ReleasePreset `
    -Preset $resolvedX64Preset `
    -QtPrefix $resolvedX64QtPrefix

$x64Installer = [System.IO.Path]::Combine(
    $outputDirectory,
    "ClassMngr-$projectVersion-win-x64.exe"
)

if (-not [System.IO.File]::Exists($x64Installer)) {
    throw "Expected x64 installer was not created: $x64Installer"
}

$releaseArtifacts.Add($x64Installer)

if (-not $SkipArm64) {
    if (Test-DirectoryExists -Path $defaultArm64QtPrefix) {
        Set-QtPrefixDefault `
            -Name "QT_MSVC_ARM64_PREFIX" `
            -Path $defaultArm64QtPrefix
    }

    $arm64QtPrefix = [Environment]::GetEnvironmentVariable(
        "QT_MSVC_ARM64_PREFIX"
    )

    if ([string]::IsNullOrWhiteSpace($arm64QtPrefix)) {
        Write-Host "Skipping Windows ARM64 release because QT_MSVC_ARM64_PREFIX is not set."
    } else {
        Require-QtPrefix `
            -Name "QT_MSVC_ARM64_PREFIX" `
            -Value $arm64QtPrefix
        Invoke-ReleasePreset `
            -Preset "windows-arm64-release" `
            -QtPrefix $arm64QtPrefix

        $arm64Installer = [System.IO.Path]::Combine(
            $outputDirectory,
            "ClassMngr-$projectVersion-win-arm64.exe"
        )

        if (-not [System.IO.File]::Exists($arm64Installer)) {
            throw "Expected ARM64 installer was not created: $arm64Installer"
        }

        $releaseArtifacts.Add($arm64Installer)
    }
}

$checksumLines = foreach ($artifact in $releaseArtifacts) {
    $hash = (Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([System.IO.Path]::GetFileName($artifact))"
}
$checksumsPath = [System.IO.Path]::Combine(
    $outputDirectory,
    "checksums-windows.txt"
)
[System.IO.File]::WriteAllLines(
    $checksumsPath,
    $checksumLines,
    [System.Text.UTF8Encoding]::new($false)
)

Write-Host "Windows release artifacts:"
$releaseArtifacts | ForEach-Object { Write-Host "  $_" }
Write-Host "  $checksumsPath"
