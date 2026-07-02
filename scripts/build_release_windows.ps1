$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$defaultQtRoot = "D:\Development\Qt\6.11.1"
$desktopQtPrefix = Join-Path $defaultQtRoot "msvc2022_64"

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

function Require-QtPrefix {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $value = [Environment]::GetEnvironmentVariable($Name)

    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "$Name must point at a Qt MSVC installation."
    }

    if (-not (Test-Path -LiteralPath $value)) {
        throw "$Name points to '$value', but that path does not exist."
    }
}

Set-QtPrefixDefault `
    -Name "QT_MSVC_ARM64_PREFIX" `
    -Path (Join-Path $defaultQtRoot "msvc2022_arm64")

if (-not (Test-Path -LiteralPath $desktopQtPrefix)) {
    throw "Desktop Qt prefix '$desktopQtPrefix' does not exist."
}

Require-QtPrefix -Name "QT_MSVC_ARM64_PREFIX"

Write-Host "Building Windows desktop release"
cmake --preset windows-desktop-release
cmake --build --preset windows-desktop-release
cmake --build --preset windows-desktop-release-install

Write-Host "Building Windows ARM64 release"
cmake --preset windows-arm64-release
cmake --build --preset windows-arm64-release
cmake --build --preset windows-arm64-release-install
