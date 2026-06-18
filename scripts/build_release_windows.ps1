$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$presets = @(
    "windows-msvc-release",
    "windows-msvc-x86-release"
)

foreach ($preset in $presets) {
    Write-Host "Configuring $preset"
    cmake --preset $preset

    Write-Host "Building $preset"
    cmake --build --preset $preset --config Release
}
