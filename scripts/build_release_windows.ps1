$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$defaultQtRoot = "D:\Development\Qt\6.11.1"

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
    -Name "QT_MSVC_X64_PREFIX" `
    -Path (Join-Path $defaultQtRoot "msvc2022_64")
Set-QtPrefixDefault `
    -Name "QT_MSVC_ARM64_PREFIX" `
    -Path (Join-Path $defaultQtRoot "msvc2022_arm64")

Require-QtPrefix -Name "QT_MSVC_X64_PREFIX"
Require-QtPrefix -Name "QT_MSVC_ARM64_PREFIX"

Write-Host "Building Windows x64 release artifact"
cmake --workflow --preset windows-msvc-x64-release-artifacts

Write-Host "Building Windows ARM64 release artifact"
cmake --workflow --preset windows-msvc-arm64-release-artifacts

$x86Prefix = [Environment]::GetEnvironmentVariable("QT_MSVC_X86_PREFIX")

if ([string]::IsNullOrWhiteSpace($x86Prefix)) {
    Write-Host "Skipping optional Windows x86 release: QT_MSVC_X86_PREFIX is not set."
}
elseif (-not (Test-Path -LiteralPath $x86Prefix)) {
    throw "QT_MSVC_X86_PREFIX points to '$x86Prefix', but that path does not exist."
}
else {
    Write-Host "Building optional Windows x86 release artifact"
    cmake --workflow --preset windows-msvc-x86-release-artifacts
}
