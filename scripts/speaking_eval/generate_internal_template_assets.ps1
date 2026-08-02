param(
    [switch]$Check,
    [switch]$BootstrapPlaceholders,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments = @()
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$checkMode = $Check.IsPresent -or ($Arguments -contains '--check')
$bootstrapMode =
    $BootstrapPlaceholders.IsPresent `
    -or ($Arguments -contains '--bootstrap-placeholders')
$unsupportedArguments = @(
    $Arguments |
        Where-Object {
            $_ -ne '--check' -and $_ -ne '--bootstrap-placeholders'
        }
)
if ($unsupportedArguments.Count -gt 0) {
    throw "Unsupported argument(s): $($unsupportedArguments -join ', ')"
}

$compiler = Join-Path $PSScriptRoot 'compile_svg_template_assets.ps1'
if (-not (Test-Path -LiteralPath $compiler)) {
    throw "The SVG template compiler is missing: $compiler"
}

& $compiler `
    -Check:$checkMode `
    -BootstrapPlaceholders:$bootstrapMode
