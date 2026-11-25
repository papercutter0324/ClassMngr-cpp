Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$runnerPath = Join-Path $PSScriptRoot 'run_packaged_next_p9_19_classes_lifecycle.ps1'
$tokens = $null
$parseErrors = $null
$runnerAst = [System.Management.Automation.Language.Parser]::ParseFile($runnerPath, [ref]$tokens, [ref]$parseErrors)
if (@($parseErrors).Count -gt 0) { throw "P9-19 route runner did not parse: $($parseErrors[0].Message)" }
$initializeFunction = $runnerAst.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Initialize-P919PackageDirectory' }, $true)
$stageFunction = $runnerAst.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Stage-P919Package' }, $true)
if ($null -eq $initializeFunction -or $null -eq $stageFunction) { throw 'P9-19 package staging functions are missing.' }
$stageSource = $stageFunction.Extent.Text
$ensureIndex = $stageSource.IndexOf('Initialize-P919PackageDirectory', [StringComparison]::Ordinal)
$copyIndex = $stageSource.IndexOf('Copy-Item -LiteralPath ([string]$receipt.releaseExecutablePath)', [StringComparison]::Ordinal)
if ($ensureIndex -lt 0 -or $copyIndex -lt 0 -or $ensureIndex -gt $copyIndex) { throw 'Stage-P919Package must initialize package\bin before copying the executable.' }
$initializeScript = [ScriptBlock]::Create($initializeFunction.Extent.Text)
. $initializeScript

$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
$testRoot = Join-Path $tempRoot ('classmngr-p9-19-package-staging-' + [Guid]::NewGuid().ToString('N'))
$packageRoot = Join-Path (Join-Path $testRoot 'package') 'bin'
$created = $false
try {
    if (Test-Path -LiteralPath $packageRoot) { throw "Test package root unexpectedly exists: $packageRoot" }
    [void][IO.Directory]::CreateDirectory($testRoot)
    $created = $true
    $sourcePath = Join-Path $testRoot 'ClassMngr.exe'
    $destinationPath = Join-Path $packageRoot 'ClassMngrNext.exe'
    [IO.File]::WriteAllBytes($sourcePath, [byte[]]@(12,34,56,78))
    $resolvedPackageRoot = Initialize-P919PackageDirectory $packageRoot
    if ([IO.Path]::GetFullPath($resolvedPackageRoot) -cne [IO.Path]::GetFullPath($packageRoot) -or -not (Test-Path -LiteralPath $resolvedPackageRoot -PathType Container)) { throw 'Package directory initializer did not create and verify the nested package\bin directory.' }
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath
    if (-not (Test-Path -LiteralPath $destinationPath -PathType Leaf) -or (Get-FileHash -LiteralPath $destinationPath -Algorithm SHA256).Hash -cne (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash) { throw 'Executable copy failed after package\bin initialization.' }
    Write-Output (ConvertTo-Json -Compress ([ordered]@{ status='passed'; test='package-bin-created-and-verified-before-executable-copy'; packageRoot=$resolvedPackageRoot; copiedFile=$destinationPath }))
}
finally {
    if ($created -and (Test-Path -LiteralPath $testRoot)) {
        $resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)
        if (-not $resolvedTestRoot.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw "Refusing to remove test directory outside temp root: $resolvedTestRoot" }
        [IO.Directory]::Delete($resolvedTestRoot, $true)
    }
}