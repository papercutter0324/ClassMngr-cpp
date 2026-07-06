param(
    [ValidateSet("auto", "windows-desktop-release", "windows-laptop-release")]
    [string]$X64Preset = "auto",

    [switch]$SkipArm64
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

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

function Require-QtPrefix {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $value = [Environment]::GetEnvironmentVariable($Name)

    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "$Name must point at a Qt MSVC installation."
    }

    if (-not (Test-DirectoryExists -Path $value)) {
        throw "$Name points to '$value', but that path does not exist."
    }
}

$desktopQtPrefix = Get-QtPresetPrefix -PresetName "qt-windows-desktop"
$laptopQtPrefix = Get-QtPresetPrefix -PresetName "qt-windows-laptop"
$desktopArm64QtPrefix = [System.IO.Path]::Combine(
    [System.IO.Directory]::GetParent($desktopQtPrefix).FullName,
    "msvc2022_arm64"
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

    if (Test-DirectoryExists -Path $desktopQtPrefix) {
        return "windows-desktop-release"
    }

    if (Test-DirectoryExists -Path $laptopQtPrefix) {
        return "windows-laptop-release"
    }

    throw "No Windows x64 Qt prefix was found. Expected '$desktopQtPrefix' for the desktop preset or '$laptopQtPrefix' for the laptop preset."
}

function Invoke-ReleasePreset {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Preset
    )

    Write-Host "Building $Preset"
    cmake --preset $Preset
    cmake --build --preset $Preset
    cmake --build --preset "$Preset-install"
}

$resolvedX64Preset = Resolve-X64Preset -Preset $X64Preset
$resolvedX64QtPrefix = $x64PresetQtPrefixes[$resolvedX64Preset]

if (-not (Test-DirectoryExists -Path $resolvedX64QtPrefix)) {
    throw "$resolvedX64Preset Qt prefix '$resolvedX64QtPrefix' does not exist."
}

Invoke-ReleasePreset -Preset $resolvedX64Preset

if (-not $SkipArm64) {
    if (Test-DirectoryExists -Path $desktopArm64QtPrefix) {
        Set-QtPrefixDefault `
            -Name "QT_MSVC_ARM64_PREFIX" `
            -Path $desktopArm64QtPrefix
    }

    $arm64QtPrefix = [Environment]::GetEnvironmentVariable("QT_MSVC_ARM64_PREFIX")

    if ([string]::IsNullOrWhiteSpace($arm64QtPrefix)) {
        Write-Host "Skipping Windows ARM64 release because QT_MSVC_ARM64_PREFIX is not set."
    } else {
        Require-QtPrefix -Name "QT_MSVC_ARM64_PREFIX"

        Write-Host "Building Windows ARM64 release"
        cmake --preset windows-arm64-release
        cmake --build --preset windows-arm64-release
        cmake --build --preset windows-arm64-release-install
    }
}
