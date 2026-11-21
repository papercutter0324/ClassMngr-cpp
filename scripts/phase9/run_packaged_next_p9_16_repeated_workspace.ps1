<#
.SYNOPSIS
Runs the P9-16 packaged Windows x64 Release repeated workspace route.

.DESCRIPTION
Verifies the accepted P9-15 package ancestry, opens a pinned large_startup.sql
workspace through the isolated non-native Qt file dialog, and measures one
warm-up plus ten same-process close/reopen cycles. The fixture and UIA route
remain isolated under the run evidence directory.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = '',
    [string]$EvidenceRoot = 'artifacts/phase9/packaged-next-p9-16-repeated-workspace',
    [string]$PriorRunManifestPath = 'artifacts/phase9/packaged-next-p9-15-schedule-locale-theme/20261120T225845019Z-f75caa8036d44ba18c2c41ce06dbc621/manifest.json',
    [string]$BuildDirectory = 'build/windows-x64-release',
    [ValidateSet('dialog', 'recent')][string]$OpenMode = 'dialog',
    [ValidateSet('standard', 'fixture-size-ab')][string]$ExperimentMode = 'standard',
    [ValidateSet('large', 'tiny')][string]$FixtureSize = 'large',
    [string]$ExperimentPairId = '',
    [ValidateRange(30, 300)][int]$ReadinessTimeoutSeconds = 120,
    [ValidateRange(30, 180)][int]$ExitTimeoutSeconds = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$TaskId = 'P9_16_PACKAGED_NEXT_WORKSPACE_REPEATED_CYCLES'
$Schema = 'classmngr-phase9-p9-16-packaged-repeated-workspace-v2'
$IsFixtureSizeExperiment = $ExperimentMode -ceq 'fixture-size-ab'
$ArmRole = if (-not $IsFixtureSizeExperiment -and $OpenMode -ceq 'dialog') { 'primary-acceptance' } else { 'diagnostic-only' }
$RouteWorkflow = if ($IsFixtureSizeExperiment) { "Diagnostic fixture-size A/B arm=$FixtureSize; one QFileDialog warm-up, three Recent Files priming open/close cycles, five open/closed baseline windows, then twenty Recent Files close/reopen cycles." } elseif ($OpenMode -ceq 'dialog') { 'Primary: File > Close Teacher Profile and File > Open Teacher Profile dialog packaged Windows x64 Release' } else { 'Diagnostic: one dialog warm-up, then File > Recent Files > exact fixture path after each close, packaged Windows x64 Release' }
$PrimingCycleCount = if ($IsFixtureSizeExperiment) { 3 } else { 0 }
$MeasuredCycleCount = if ($IsFixtureSizeExperiment) { 20 } else { 10 }
if ($IsFixtureSizeExperiment -and ($OpenMode -cne 'recent' -or [string]::IsNullOrWhiteSpace($ExperimentPairId))) { throw 'The fixture-size A/B diagnostic requires Recent Files mode and a shared nonempty ExperimentPairId.' }
if (-not $IsFixtureSizeExperiment -and ($FixtureSize -cne 'large' -or -not [string]::IsNullOrWhiteSpace($ExperimentPairId))) { throw 'FixtureSize and ExperimentPairId are reserved for the fixture-size A/B diagnostic.' }
$StrictWorkingSetLimit = [Int64]262144000
$OperationPeakAdvisoryLimit = [Int64]536870912
$P916OwnedSources = @(
    'scripts/phase9/prepare_p9_16_workspace.py',
    'scripts/phase9/test_prepare_p9_16_workspace.py',
    'scripts/phase9/run_packaged_next_p9_16_repeated_workspace.ps1',
    'scripts/phase9/validate_packaged_next_p9_16_repeated_workspace.py',
    'scripts/phase9/test_validate_packaged_next_p9_16_repeated_workspace.py'
)
$P915Sources = @(
    'scripts/phase9/prepare_p9_15_schedule_workspace.py',
    'scripts/phase9/run_packaged_next_p9_15_schedule_locale_theme.ps1',
    'scripts/phase9/validate_packaged_next_p9_15_schedule_locale_theme.py',
    'scripts/phase9/test_validate_packaged_next_p9_15_schedule_locale_theme.py'
)
$P914Sources = @(
    'scripts/phase9/p9_14_packaged_lifecycle_common.ps1',
    'scripts/phase9/prepare_p9_14_staff_ai_workspaces.py',
    'scripts/phase9/run_packaged_next_p9_14_staff_directory_lifecycle.ps1',
    'scripts/phase9/run_packaged_next_p9_14_ai_batch_review_lifecycle.ps1',
    'scripts/phase9/validate_packaged_next_p9_14_lifecycle.py',
    'scripts/phase9/test_validate_packaged_next_p9_14_lifecycle.py'
)
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) { $RepositoryRoot = Join-Path $PSScriptRoot '../..' }
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$EvidenceRoot = if ([IO.Path]::IsPathRooted($EvidenceRoot)) { [IO.Path]::GetFullPath($EvidenceRoot) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $EvidenceRoot)) }
$PriorRunManifestPath = if ([IO.Path]::IsPathRooted($PriorRunManifestPath)) { [IO.Path]::GetFullPath($PriorRunManifestPath) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $PriorRunManifestPath)) }
$BuildDirectory = if ([IO.Path]::IsPathRooted($BuildDirectory)) { [IO.Path]::GetFullPath($BuildDirectory) } else { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $BuildDirectory)) }
if ($env:OS -cne 'Windows_NT') { throw 'P9-16 packaged UIA lifecycle must run locally on Windows.' }
. (Join-Path $PSScriptRoot 'p9_14_packaged_lifecycle_common.ps1')

function Get-P916InventoryRows([object[]]$Rows) { Get-P914InventoryFingerprint $Rows }

function Assert-P916OldSourcesUnchanged([object[]]$OldRows, [object[]]$CurrentRows) {
    $old = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    $now = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($row in $OldRows) { $old.Add([string]$row.path, $row) }
    foreach ($row in $CurrentRows) { $now.Add([string]$row.path, $row) }
    foreach ($path in $old.Keys) {
        if (-not $now.ContainsKey($path)) { throw "Previously accepted P9-15 source is missing: $path" }
        if ([Int64]$old[$path].sizeBytes -ne [Int64]$now[$path].sizeBytes -or [string]$old[$path].sha256 -cne [string]$now[$path].sha256) { throw "Previously accepted P9-15 source changed: $path" }
    }
    $added = @($now.Keys | Where-Object { -not $old.ContainsKey($_) } | ForEach-Object { ([string]$_).Replace('\', '/') } | Sort-Object)
    if (($added -join "`n") -cne (($P916OwnedSources | Sort-Object) -join "`n")) { throw "Unexpected source additions after accepted P9-15: $($added -join ', ')" }
}

function Initialize-P916ReusedPackage([string]$Repository, [string]$EvidenceDirectory, [string]$AcceptedManifestPath) {
    if (-not (Test-Path -LiteralPath $AcceptedManifestPath -PathType Leaf)) { throw "Accepted P9-15 manifest is missing: $AcceptedManifestPath" }
    $acceptedHash = Get-P914FileSha256 $AcceptedManifestPath
    $accepted = Read-P914Json $AcceptedManifestPath
    if ([string]$accepted.taskId -cne 'P9_15_PACKAGED_NEXT_SCHEDULE_LOCALE_THEME' -or [string]$accepted.status -cne 'passed') { throw 'P9-16 package ancestry is not a passed P9-15 route.' }
    $acceptedRoot = Split-Path -Parent $AcceptedManifestPath
    $acceptedProof = $accepted.packageProvenance
    $acceptedSourcePath = Join-Path $acceptedRoot ([string]$acceptedProof.sourceInventoryPath)
    if ((Get-P914FileSha256 $acceptedSourcePath) -cne [string]$acceptedProof.sourceInventoryFileSha256) { throw 'Accepted P9-15 source inventory changed.' }
    $acceptedSource = Read-P914Json $acceptedSourcePath
    $currentSource = Get-P914SourceInventory $Repository @($acceptedSource.includedRoots)
    Assert-P916OldSourcesUnchanged @($acceptedSource.files) @($currentSource.files)

    $p913Path = [IO.Path]::GetFullPath([string]$acceptedProof.priorP913ManifestPath)
    if (-not (Test-Path -LiteralPath $p913Path -PathType Leaf) -or (Get-P914FileSha256 $p913Path) -cne [string]$acceptedProof.priorP913ManifestSha256) { throw 'Accepted P9-13 source derivation ancestry changed or is missing.' }
    $p913 = Read-P914Json $p913Path
    if ([string]$p913.taskId -cne 'P9_13_NEXT_SPEAKING_REPORT_PACKAGED_LIFECYCLE' -or [string]$p913.status -cne 'passed') { throw 'P9-15 product source ancestry is not a passed P9-13 route.' }
    $p913Root = Split-Path -Parent $p913Path
    $derivedPath = Join-Path $p913Root ([string]$p913.source.derivedReceiptPath)
    if ((Get-P914FileSha256 $derivedPath) -cne [string]$p913.source.derivedReceiptSha256) { throw 'Accepted P9-13 source derivation receipt changed.' }
    $derived = Read-P914Json $derivedPath
    if ([string]$derived.status -cne 'verified-script-only-derivation' -or [string]$derived.source.productSourceSha256 -cne [string]$acceptedProof.productSourceSha256) { throw 'P9-13 derivation receipt does not support the accepted P9-15 product-source hash.' }

    $ignored = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $derived.source.allowedPaths) { [void]$ignored.Add([string]$path) }
    foreach ($path in $P914Sources) { [void]$ignored.Add([string]$path) }
    foreach ($path in $P915Sources) { [void]$ignored.Add([string]$path) }
    foreach ($path in $P916OwnedSources) { [void]$ignored.Add([string]$path) }
    $productRows = @($currentSource.files | Where-Object { -not $ignored.Contains([string]$_.path) })
    $productSha = Get-P916InventoryRows $productRows
    if ($productSha -cne [string]$acceptedProof.productSourceSha256) { throw 'Current product sources differ from the accepted P9-15 Release fingerprint; a local Release package is required.' }

    $packageRoot = [IO.Path]::GetFullPath([string]$acceptedProof.packageRoot)
    $package = Get-P914CanonicalInventory $packageRoot
    if ([string]$package.sha256 -cne [string]$acceptedProof.packageSha256 -or [int]$package.fileCount -ne [int]$acceptedProof.packageFileCount) { throw 'Current package tree differs from accepted P9-15 full inventory.' }
    $acceptedPackagePath = Join-Path $acceptedRoot ([string]$acceptedProof.packageInventoryPath)
    if ((Get-P914FileSha256 $acceptedPackagePath) -cne [string]$acceptedProof.packageInventoryFileSha256) { throw 'Accepted P9-15 package inventory file changed.' }
    $acceptedPackage = Read-P914Json $acceptedPackagePath
    if ((Get-P916InventoryRows @($acceptedPackage.files)) -cne [string]$package.sha256 -or [int]$acceptedPackage.fileCount -ne [int]$package.fileCount) { throw 'Current package files differ from accepted P9-15 complete package inventory.' }
    $appPath = Join-Path $packageRoot 'ClassMngrNext.exe'
    if ((Get-P914FileSha256 $appPath) -cne [string]$acceptedProof.applicationSha256 -or (Get-P914PeMachine $appPath) -ne 0x8664) { throw 'Accepted package executable hash or x64 architecture changed.' }
    $releasePath = [IO.Path]::GetFullPath([string]$acceptedProof.acceptedReleaseReceiptPath)
    if ((Get-P914FileSha256 $releasePath) -cne [string]$acceptedProof.acceptedReleaseReceiptSha256) { throw 'Accepted P9-15 Release receipt changed.' }
    $release = Read-P914Json $releasePath
    if ([string]$release.status -cne 'succeeded' -or [string]$release.configuration -cne 'Release' -or [string]$release.platform -cne 'x64' -or [int]$release.build.exitCode -ne 0 -or [string]$release.releaseExecutableSha256 -cne (Get-P914FileSha256 $appPath)) { throw 'Accepted Release receipt does not prove the package executable.' }
    $cachePath = [IO.Path]::GetFullPath([string]$acceptedProof.configuredCachePath)
    if ((Get-P914FileSha256 $cachePath) -cne [string]$acceptedProof.configuredCacheSha256 -or (Get-P914FileSha256 $cachePath) -cne [string]$release.cacheSha256After) { throw 'Configured Release cache differs from accepted build evidence.' }
    $resourceRoot = [IO.Path]::GetFullPath([string]$acceptedProof.resourceRoot)
    $resource = Get-P914CanonicalInventory $resourceRoot
    if ([string]$resource.sha256 -cne [string]$acceptedProof.resourceInventorySha256 -or [string]$resource.sha256 -cne [string]$release.generatedResources.stagedResourceInputs.sha256) { throw 'Staged resource inputs differ from accepted P9-15 Release evidence.' }

    $sourcePath = Join-Path $EvidenceDirectory 'source-files.json'; Write-P914Json $sourcePath $currentSource
    $packagePath = Join-Path $EvidenceDirectory 'package-files.json'; Write-P914Json $packagePath $package
    $resourcePath = Join-Path $EvidenceDirectory 'resource-files.json'; Write-P914Json $resourcePath $resource
    [ordered]@{
        reusedPackage = $true; reuseBasis = 'P9-15 accepted package reverified against current product sources, full package tree, x64 executable, staged resources, successful Release receipt, and configured cache'
        priorRunManifestPath = [IO.Path]::GetFullPath($AcceptedManifestPath); priorRunManifestSha256 = $acceptedHash; priorRunId = [string]$accepted.runId
        priorP913ManifestPath = $p913Path; priorP913ManifestSha256 = Get-P914FileSha256 $p913Path
        packageRoot = $packageRoot; packageSha256 = [string]$package.sha256; packageFileCount = [int]$package.fileCount
        packageInventoryPath = 'package-files.json'; packageInventoryFileSha256 = Get-P914FileSha256 $packagePath
        applicationPath = [IO.Path]::GetFullPath($appPath); applicationSha256 = Get-P914FileSha256 $appPath; architecture = 'x64'
        productSourceSha256 = $productSha; currentSourceSha256 = [string]$currentSource.sha256
        sourceInventoryPath = 'source-files.json'; sourceInventoryFileSha256 = Get-P914FileSha256 $sourcePath
        acceptedReleaseReceiptPath = $releasePath; acceptedReleaseReceiptSha256 = Get-P914FileSha256 $releasePath
        configuration = 'Release'; platform = 'x64'; generator = [string]$release.generator
        configuredCachePath = $cachePath; configuredCacheSha256 = Get-P914FileSha256 $cachePath
        resourceRoot = $resourceRoot; resourceInventoryPath = 'resource-files.json'
        resourceInventorySha256 = [string]$resource.sha256; resourceInventoryFileSha256 = Get-P914FileSha256 $resourcePath
        freshExactTreeRebuild = $false
    }
}

function Get-P916Package([string]$Repository, [string]$EvidenceDirectory, [string]$AcceptedManifestPath) {
    try { return Initialize-P916ReusedPackage $Repository $EvidenceDirectory $AcceptedManifestPath }
    catch {
        $reuseFailure = $_.Exception.Message
        return Initialize-P916LocalPackage $Repository $EvidenceDirectory $AcceptedManifestPath $reuseFailure
    }
}

function Initialize-P916LocalPackage([string]$Repository, [string]$EvidenceDirectory, [string]$AcceptedManifestPath, [string]$ReuseFailure) {
    $cachePath = Join-Path $BuildDirectory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { throw "Accepted P9-15 package could not be reused ('$ReuseFailure'); standard Release CMake cache is missing: $cachePath" }
    $cacheLines = @(Get-Content -LiteralPath $cachePath)
    $getCacheValue = { param($key) $matches = @($cacheLines | Where-Object { $_ -match "^$([regex]::Escape($key)):[^=]+=" }); if ($matches.Count -ne 1) { throw "Expected exactly one configured CMake value '$key'." }; ($matches[0] -split '=', 2)[1] }
    $homeDirectory = [IO.Path]::GetFullPath((& $getCacheValue 'CMAKE_HOME_DIRECTORY'))
    if ($homeDirectory -ine $Repository -or (& $getCacheValue 'CLASSMNGR_NEXT_RELEASE_PACKAGE') -cne 'ON' -or (& $getCacheValue 'CMAKE_GENERATOR') -notmatch '^Visual Studio' -or (& $getCacheValue 'CMAKE_GENERATOR_PLATFORM') -notin @('x64', '')) { throw 'Cannot rebuild fallback package: configured cache is not the standard Windows x64 Release package tree.' }
    $cmake = [IO.Path]::GetFullPath((& $getCacheValue 'CMAKE_COMMAND'))
    $qt6Dir = [IO.Path]::GetFullPath((& $getCacheValue 'Qt6_DIR'))
    $qtBin = [IO.Path]::GetFullPath((Join-Path $qt6Dir '../../../bin'))
    $windeploy = Join-Path $qtBin 'windeployqt.exe'
    if (-not (Test-Path -LiteralPath $cmake -PathType Leaf) -or -not (Test-Path -LiteralPath $windeploy -PathType Leaf)) { throw 'Fallback Release package tools are missing from the configured CMake/Qt installation.' }
    $packageRoot = Join-Path $EvidenceDirectory 'package'
    [void][IO.Directory]::CreateDirectory($packageRoot)
    $source = Get-P914SourceInventory $Repository @('CMakeLists.txt','CMakePresets.json','cmake','resources','scripts/phase9','src','tests','docs/qt-rewrite/visual-baseline/release/large-class-transfer-boundary')
    $p915 = Read-P914Json $AcceptedManifestPath
    $p915Proof = $p915.packageProvenance
    $p913Path = [IO.Path]::GetFullPath([string]$p915Proof.priorP913ManifestPath)
    if (-not (Test-Path -LiteralPath $p913Path -PathType Leaf) -or (Get-P914FileSha256 $p913Path) -cne [string]$p915Proof.priorP913ManifestSha256) { throw 'Fallback package cannot establish the accepted P9-13 source derivation ancestry.' }
    $p913 = Read-P914Json $p913Path
    if ([string]$p913.taskId -cne 'P9_13_NEXT_SPEAKING_REPORT_PACKAGED_LIFECYCLE' -or [string]$p913.status -cne 'passed') { throw 'Fallback package does not derive from the passed accepted P9-13 product-source ancestry.' }
    $derivedPath = Join-Path (Split-Path -Parent $p913Path) ([string]$p913.source.derivedReceiptPath)
    if ((Get-P914FileSha256 $derivedPath) -cne [string]$p913.source.derivedReceiptSha256) { throw 'Fallback package P9-13 source derivation receipt changed.' }
    $derived = Read-P914Json $derivedPath
    if ([string]$derived.status -cne 'verified-script-only-derivation') { throw 'Fallback package P9-13 source derivation is not verified.' }
    $ignored = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $derived.source.allowedPaths) { [void]$ignored.Add([string]$path) }
    foreach ($path in $P914Sources) { [void]$ignored.Add([string]$path) }
    foreach ($path in $P915Sources) { [void]$ignored.Add([string]$path) }
    foreach ($path in $P916OwnedSources) { [void]$ignored.Add([string]$path) }
    $productSourceSha = Get-P914InventoryFingerprint @($source.files | Where-Object { -not $ignored.Contains([string]$_.path) })

    $buildLog = Join-Path (Join-Path $EvidenceDirectory 'logs') 'local-release-build.log'
    $oldPath = [Environment]::GetEnvironmentVariable('PATH', 'Process')
    try {
        [Environment]::SetEnvironmentVariable('PATH', "$qtBin;$oldPath", 'Process')
        $buildLines = @(& $cmake --build $BuildDirectory --config Release --verbose --target ClassMngrNext ClassMngrNextResourcePackage 2>&1 | ForEach-Object { [string]$_ }); $buildCode = $LASTEXITCODE
    } finally { [Environment]::SetEnvironmentVariable('PATH', $oldPath, 'Process') }
    [IO.File]::WriteAllLines($buildLog, @("cmake --build $BuildDirectory --config Release --verbose --target ClassMngrNext ClassMngrNextResourcePackage", $buildLines, "exitCode=$buildCode"), [Text.UTF8Encoding]::new($false))
    if ($buildCode -ne 0) { throw "Fallback local Release build failed with exit code $buildCode." }

    $installLog = Join-Path (Join-Path $EvidenceDirectory 'logs') 'resource-install.log'
    $oldPath = [Environment]::GetEnvironmentVariable('PATH', 'Process')
    try {
        [Environment]::SetEnvironmentVariable('PATH', "$qtBin;$oldPath", 'Process')
        $installLines = @(& $cmake --install $BuildDirectory --config Release --prefix $packageRoot --component ClassMngrNextResources 2>&1 | ForEach-Object { [string]$_ }); $installCode = $LASTEXITCODE
    } finally { [Environment]::SetEnvironmentVariable('PATH', $oldPath, 'Process') }
    [IO.File]::WriteAllLines($installLog, $installLines, [Text.UTF8Encoding]::new($false))
    if ($installCode -ne 0) { throw "Fallback local Release resource install failed with exit code $installCode." }

    $releaseExe = Join-Path $BuildDirectory 'next-package-build/Release/ClassMngr.exe'
    $appPath = Join-Path $packageRoot 'ClassMngrNext.exe'
    if (-not (Test-Path -LiteralPath $releaseExe -PathType Leaf)) { throw "Fallback local Release executable is missing: $releaseExe" }
    Copy-Item -LiteralPath $releaseExe -Destination $appPath
    $deployLog = Join-Path (Join-Path $EvidenceDirectory 'logs') 'windeployqt.log'
    $oldPath = [Environment]::GetEnvironmentVariable('PATH', 'Process')
    try {
        [Environment]::SetEnvironmentVariable('PATH', "$qtBin;$oldPath", 'Process')
        $deployLines = @(& $windeploy --qmldir (Join-Path $Repository 'src/features/calendar/ui/qml') --skip-plugin-types qmltooling --include-plugins qoffscreen,qsqlite --exclude-plugins qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql --translations en,ko --compiler-runtime --dir $packageRoot $appPath 2>&1 | ForEach-Object { [string]$_ }); $deployCode = $LASTEXITCODE
    } finally { [Environment]::SetEnvironmentVariable('PATH', $oldPath, 'Process') }
    [IO.File]::WriteAllLines($deployLog, $deployLines, [Text.UTF8Encoding]::new($false))
    if ($deployCode -ne 0) { throw "Fallback cache-bound windeployqt failed with exit code $deployCode." }

    if ((Get-P914PeMachine $appPath) -ne 0x8664) { throw 'Fallback package executable is not Windows x64.' }
    $package = Get-P914CanonicalInventory $packageRoot
    $resourceRoot = Join-Path $packageRoot 'resources'
    $resource = Get-P914CanonicalInventory $resourceRoot
    $sourcePath = Join-Path $EvidenceDirectory 'source-files.json'; Write-P914Json $sourcePath $source
    $packagePath = Join-Path $EvidenceDirectory 'package-files.json'; Write-P914Json $packagePath $package
    $resourcePath = Join-Path $EvidenceDirectory 'resource-files.json'; Write-P914Json $resourcePath $resource
    $releaseReceiptPath = Join-Path $EvidenceDirectory 'local-release-package-receipt.json'
    $releaseReceipt = [ordered]@{
        schema = 'classmngr-phase9-p9-16-local-release-package-v1'; status = 'succeeded'; configuration = 'Release'; platform = 'x64'
        generator = (& $getCacheValue 'CMAKE_GENERATOR'); build = [ordered]@{ exitCode = $buildCode; command = 'cmake --build <standard-build-directory> --config Release --verbose --target ClassMngrNext ClassMngrNextResourcePackage' }
        resourceInstallExitCode = $installCode; deployExitCode = $deployCode; releaseExecutableSha256 = Get-P914FileSha256 $appPath
        cachePath = [IO.Path]::GetFullPath($cachePath); cacheSha256After = Get-P914FileSha256 $cachePath
        generatedResources = [ordered]@{ stagedResourceInputs = [ordered]@{ root = [IO.Path]::GetFullPath($resourceRoot); sha256 = [string]$resource.sha256; fileCount = [int]$resource.fileCount } }
        productSourceSha256 = $productSourceSha; packageSha256 = [string]$package.sha256; packageFileCount = [int]$package.fileCount
    }
    Write-P914Json $releaseReceiptPath $releaseReceipt
    [ordered]@{
        reusedPackage = $false; reuseFailureReason = $ReuseFailure; reuseBasis = 'Current accepted P9-15 package equivalence failed; rebuilt and staged a fresh local Windows x64 Release package using the standard configured build tree'
        priorRunManifestPath = [IO.Path]::GetFullPath($AcceptedManifestPath); priorRunManifestSha256 = Get-P914FileSha256 $AcceptedManifestPath; priorRunId = [string]$p915.runId
        priorP913ManifestPath = $p913Path; priorP913ManifestSha256 = Get-P914FileSha256 $p913Path
        packageRoot = [IO.Path]::GetFullPath($packageRoot); packageSha256 = [string]$package.sha256; packageFileCount = [int]$package.fileCount
        packageInventoryPath = 'package-files.json'; packageInventoryFileSha256 = Get-P914FileSha256 $packagePath
        applicationPath = [IO.Path]::GetFullPath($appPath); applicationSha256 = Get-P914FileSha256 $appPath; architecture = 'x64'
        productSourceSha256 = $productSourceSha; currentSourceSha256 = [string]$source.sha256
        sourceInventoryPath = 'source-files.json'; sourceInventoryFileSha256 = Get-P914FileSha256 $sourcePath
        acceptedReleaseReceiptPath = [IO.Path]::GetFullPath($releaseReceiptPath); acceptedReleaseReceiptSha256 = Get-P914FileSha256 $releaseReceiptPath
        releaseReceiptPath = 'local-release-package-receipt.json'; releaseReceiptSha256 = Get-P914FileSha256 $releaseReceiptPath
        configuration = 'Release'; platform = 'x64'; generator = [string](& $getCacheValue 'CMAKE_GENERATOR')
        configuredCachePath = [IO.Path]::GetFullPath($cachePath); configuredCacheSha256 = Get-P914FileSha256 $cachePath
        resourceRoot = [IO.Path]::GetFullPath($resourceRoot); resourceInventoryPath = 'resource-files.json'
        resourceInventorySha256 = [string]$resource.sha256; resourceInventoryFileSha256 = Get-P914FileSha256 $resourcePath
        freshExactTreeRebuild = $true
        localBuild = [ordered]@{
            buildLogPath = 'logs/local-release-build.log'; buildLogSha256 = Get-P914FileSha256 $buildLog; buildExitCode = $buildCode
            resourceInstallLogPath = 'logs/resource-install.log'; resourceInstallLogSha256 = Get-P914FileSha256 $installLog; resourceInstallExitCode = $installCode
            windeployqtPath = [IO.Path]::GetFullPath($windeploy); windeployqtSha256 = Get-P914FileSha256 $windeploy; deployLogPath = 'logs/windeployqt.log'; deployLogSha256 = Get-P914FileSha256 $deployLog; deployExitCode = $deployCode
        }
    }
}

function Invoke-P916Preparer([string]$EvidenceDirectory, [string[]]$Arguments, [string]$LogName) {
    $scriptPath = Join-Path $PSScriptRoot 'prepare_p9_16_workspace.py'
    $logPath = Join-Path (Join-Path $EvidenceDirectory 'logs') $LogName
    $lines = @(& python $scriptPath @Arguments 2>&1 | ForEach-Object { [string]$_ }); $code = $LASTEXITCODE
    [IO.File]::WriteAllLines($logPath, $lines, [Text.UTF8Encoding]::new($false))
    if ($code -ne 0) { throw "P9-16 fixture helper failed ($LogName) with exit code ${code}: $($lines -join ' ')" }
    $jsonLine = @($lines | Where-Object { $_.TrimStart().StartsWith('{') } | Select-Object -Last 1)
    if ($jsonLine.Count -ne 1) { throw "P9-16 fixture helper emitted no single JSON summary ($LogName)." }
    return ($jsonLine[0] | ConvertFrom-Json -ErrorAction Stop)
}

function Get-P916Workspace([string]$Repository, [string]$EvidenceDirectory, [string]$ProfileSize) {
    $base = Join-Path $Repository 'artifacts/phase9/packaged-next-class-transfer/20261119T155840703Z-892ae1169ba945f5be16ef08fa0a3fb1/workspace-snapshots/master-blank.tps'
    $seed = if ($ProfileSize -ceq 'large') { Join-Path $Repository 'tests/fixtures/workspaces/large_startup.sql' } else { $null }
    $workspace = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'repeated-workspace.tps'
    $receiptPath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'preparation-receipt.json'
    $prepArguments = [Collections.Generic.List[string]]::new()
    foreach ($argument in @('prepare', '--profile-size', $ProfileSize, '--base-workspace', $base, '--output', $workspace, '--receipt', $receiptPath)) { $prepArguments.Add([string]$argument) }
    if ($null -ne $seed) { $prepArguments.Insert(3, '--seed-sql'); $prepArguments.Insert(4, [string]$seed) }
    $prep = Invoke-P916Preparer $EvidenceDirectory $prepArguments.ToArray() 'workspace-preparation-output.txt'
    $receipt = Read-P914Json $receiptPath
    $beforePath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'before-snapshot.json'
    $before = Invoke-P916Preparer $EvidenceDirectory @('snapshot', '--phase', 'before', '--workspace', $workspace, '--preparation-receipt', $receiptPath, '--output', $beforePath) 'before-snapshot-output.txt'
    [ordered]@{
        workspacePath = [IO.Path]::GetFullPath($workspace); preparationPath = [IO.Path]::GetFullPath($receiptPath)
        preparation = $receipt; preparationSha256 = Get-P914FileSha256 $receiptPath
        beforePath = [IO.Path]::GetFullPath($beforePath); beforeSha256 = Get-P914FileSha256 $beforePath
        beforeSnapshot = Read-P914Json $beforePath; baseWorkspacePath = [string]$receipt.baseWorkspace
        seedSqlPath = if ($null -ne $receipt.seedSql) { [string]$receipt.seedSql } else { $null }; identity = $receipt.identity
    }
}

function Get-P916Settings([string]$EvidenceDirectory) {
    $settingsRoot = Join-Path (Join-Path $EvidenceDirectory 'runtime') 'settings'
    $profile = Join-Path (Join-Path $settingsRoot 'PaperCloud') 'ClassMngr.ini'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $profile))
    [IO.File]::WriteAllText($profile, "[options]`r`nlanguage=1`r`ntheme=1`r`n", [Text.UTF8Encoding]::new($false))
    $inventory = Get-P914CanonicalInventory $settingsRoot
    if ([int]$inventory.fileCount -ne 1 -or [IO.File]::ReadAllText($profile, [Text.Encoding]::UTF8) -cne "[options]`r`nlanguage=1`r`ntheme=1`r`n") { throw 'Fresh P9-16 isolated settings profile is not exactly English/Light.' }
    $path = Join-Path (Join-Path $EvidenceDirectory 'runtime') 'settings-before-launch-inventory.json'
    Write-P914Json $path $inventory
    $initialProfileCopy = Join-Path (Join-Path $EvidenceDirectory 'runtime') 'settings-before-launch.ini'
    Copy-Item -LiteralPath $profile -Destination $initialProfileCopy
    [ordered]@{
        root = [IO.Path]::GetFullPath($settingsRoot); settingsRootFromEnvironment = $true; globalDefaultChanged = $false
        globalDefaultsWereReadOrWritten = $false; preferenceActionsCount = 0
        beforeLaunchInventoryPath = 'runtime/settings-before-launch-inventory.json'
        beforeLaunchInventoryFileSha256 = Get-P914FileSha256 $path; beforeLaunchInventorySha256 = [string]$inventory.sha256
        initialPreferenceFile = 'runtime/settings/PaperCloud/ClassMngr.ini'; initialPreferenceFileSha256 = Get-P914FileSha256 $profile
        initialPreferenceSnapshotPath = 'runtime/settings-before-launch.ini'; initialPreferenceSnapshotSha256 = Get-P914FileSha256 $initialProfileCopy
        initialLanguagePreference = 'English'; initialThemePreference = 'Light'
    }
}

function Start-P916App([string]$EvidenceDirectory, [string]$ApplicationPath) {
    $runtime = Join-Path $EvidenceDirectory 'runtime'
    $tracePath = Join-Path (Join-Path $runtime 'diagnostics') 'application-shutdown.jsonl'
    $stdoutPath = Join-Path (Join-Path $EvidenceDirectory 'logs') 'process-stdout.log'
    $stderrPath = Join-Path (Join-Path $EvidenceDirectory 'logs') 'process-stderr.log'
    [IO.File]::WriteAllText($stdoutPath, '', [Text.UTF8Encoding]::new($false)); [IO.File]::WriteAllText($stderrPath, '', [Text.UTF8Encoding]::new($false))
    $envMap = [ordered]@{
        CLASSMNGR_SETTINGS_ROOT = (Join-Path $runtime 'settings'); CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH = [IO.Path]::GetFullPath($tracePath)
        LOCALAPPDATA = (Join-Path $runtime 'local-app-data'); APPDATA = (Join-Path $runtime 'app-data')
        TEMP = (Join-Path $runtime 'temp'); TMP = (Join-Path $runtime 'temp'); QT_FORCE_STDERR_LOGGING = '1'
        PATH = "$((Split-Path -Parent $ApplicationPath));$env:PATH"
    }
    $oldEnvironment = [ordered]@{}
    foreach ($pair in $envMap.GetEnumerator()) { $oldEnvironment[[string]$pair.Key] = [Environment]::GetEnvironmentVariable([string]$pair.Key, 'Process'); [Environment]::SetEnvironmentVariable([string]$pair.Key, [string]$pair.Value, 'Process') }
    try { $process = Start-Process -FilePath $ApplicationPath -WorkingDirectory (Split-Path -Parent $ApplicationPath) -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -PassThru }
    finally { foreach ($pair in $oldEnvironment.GetEnumerator()) { [Environment]::SetEnvironmentVariable([string]$pair.Key, $pair.Value, 'Process') } }
    if ($null -eq $process) { throw 'Unable to start the packaged P9-16 x64 Release process.' }
    $observedPath = [IO.Path]::GetFullPath([string]$process.MainModule.FileName)
    if ($observedPath -cne [IO.Path]::GetFullPath($ApplicationPath) -or (Get-P914FileSha256 $observedPath) -cne (Get-P914FileSha256 $ApplicationPath) -or (Get-P914PeMachine $observedPath) -ne 0x8664) { throw 'Live process executable path/hash/architecture differs from the accepted package.' }
    [ordered]@{
        process = $process; stdoutPath = $stdoutPath; stderrPath = $stderrPath; seen = [ordered]@{ stdout = 0; stderr = 0 }; arrival = 0
        mainWindowHandle = 0L; mainWindowRoot = $null; ownerHandles = [Collections.Generic.Dictionary[string, Int64]]::new([StringComparer]::Ordinal)
        routeEvents = [Collections.Generic.List[object]]::new(); lifecycleEvents = [Collections.Generic.List[object]]::new(); outputLines = [Collections.Generic.List[object]]::new()
        forcedTermination = $false; closeAction = $null; processExit = $null; sampler = $null
        tempRoot = (Join-Path $runtime 'temp'); processImagePath = $observedPath; processImageSha256 = Get-P914FileSha256 $observedPath
        workspacePath = ''; dialogRoot = $null; dialogHandle = 0L
    }
}

function Get-P916MainRoot([object]$Context) {
    if ($null -eq $Context.mainWindowRoot -or [Int64]$Context.mainWindowHandle -le 0) { return $null }
    $handle = [IntPtr]$Context.mainWindowHandle
    if ([uint32][P914Native]::GetWindowProcessId($handle) -ne [uint32]$Context.process.Id) { throw 'Pinned P9-16 main HWND no longer belongs to the measured PID.' }
    $root = $Context.mainWindowRoot
    $current = $root.Current
    $validRoot = ([Int64]$current.ProcessId -eq [Int64]$Context.process.Id) -and
        ([string]$current.ControlType.ProgrammaticName -ceq 'ControlType.Window') -and
        ([string]$current.AutomationId -ceq 'QApplication.classMngrNextApplicationShell')
    if (-not $validRoot) {
        throw 'Pinned P9-16 main UIA root no longer matches the measured process and exact Next shell identity.'
    }
    return $root
}

function Find-P916Suffix([object]$Context, [string]$Suffix, [string]$ControlType = '', [string]$RootKey = 'main') {
    if ($RootKey -ceq 'main') { $root = Get-P916MainRoot $Context; if ($null -eq $root) { return $null }; $handle = [Int64]$Context.mainWindowHandle }
    elseif ($RootKey -ceq 'dialog') { $root = $Context.dialogRoot; $handle = [Int64]$Context.dialogHandle; if ($null -eq $root) { return $null } }
    else { throw "Unsupported P9-16 UIA root key: $RootKey" }
    $found = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new()
    $nodes = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new(); $nodes.Add($root)
    foreach ($node in $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) { $nodes.Add($node) }
    foreach ($node in $nodes) {
        $current = $node.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { continue }
        $id = [string]$current.AutomationId
        if ($id -cne $Suffix -and -not $id.EndsWith('.' + $Suffix, [StringComparison]::Ordinal)) { continue }
        if ($ControlType -and [string]$current.ControlType.ProgrammaticName -cne $ControlType) { continue }
        $Context.ownerHandles[(Get-P914RuntimeKey $node)] = $handle
        $found.Add($node)
    }
    if ($found.Count -gt 1) { throw "P9-16 UIA suffix '$Suffix' resolved to $($found.Count) controls." }
    if ($found.Count -eq 0) { return $null }
    return $found[0]
}

function Wait-P916Suffix([object]$Context, [string]$EvidenceDirectory, [string]$Suffix, [string]$ControlType, [int]$TimeoutSeconds, [string]$RootKey = 'main') {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "Measured app exited while waiting for UIA suffix '$Suffix'." }
        $element = Find-P916Suffix $Context $Suffix $ControlType $RootKey
        if ($null -ne $element) { return $element }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for PID-bound UIA suffix '$Suffix'."
}

function Find-P916ExactName([object]$Context, [string]$Name, [string]$ControlType) {
    $root = Get-P916MainRoot $Context
    if ($null -eq $root) { return $null }
    $condition = [System.Windows.Automation.AndCondition]::new([System.Windows.Automation.Condition[]]@(
        [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty, $Name),
        [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::$($ControlType.Replace('ControlType.', '')))
    ))
    $element = $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $condition)
    if ($null -eq $element) { return $null }
    $current = $element.Current
    if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.ControlType.ProgrammaticName -cne $ControlType -or [string]$current.Name -cne $Name) { throw "P9-16 exact-name UIA identity failed for '$Name'." }
    $Context.ownerHandles[(Get-P914RuntimeKey $element)] = [Int64]$Context.mainWindowHandle
    return $element
}

function Wait-P916ExactName([object]$Context, [string]$EvidenceDirectory, [string]$Name, [string]$ControlType, [int]$TimeoutSeconds) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "Measured app exited while waiting for '$Name'." }
        $element = Find-P916ExactName $Context $Name $ControlType
        if ($null -ne $element) { return $element }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for exact PID-bound UIA name '$Name'."
}

function Find-P916DialogName([object]$Context, [string]$Name, [string]$ControlType) {
    if ($null -eq $Context.dialogRoot -or [Int64]$Context.dialogHandle -le 0) { return $null }
    $type = [System.Windows.Automation.ControlType]::$($ControlType.Replace('ControlType.', ''))
    $condition = [System.Windows.Automation.AndCondition]::new([System.Windows.Automation.Condition[]]@(
        [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty, $Name),
        [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty, $type)
    ))
    $elements = $Context.dialogRoot.FindAll([System.Windows.Automation.TreeScope]::Descendants, $condition)
    if ($elements.Count -gt 1) { throw "Exact dialog UIA name '$Name' resolved to $($elements.Count) controls." }
    if ($elements.Count -eq 0) { return $null }
    $element = $elements[0]
    $current = $element.Current
    if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.Name -cne $Name -or [string]$current.ControlType.ProgrammaticName -cne $ControlType) { throw "Exact dialog UIA identity failed for '$Name'." }
    $Context.ownerHandles[(Get-P914RuntimeKey $element)] = [Int64]$Context.dialogHandle
    return $element
}

function Wait-P916DialogName([object]$Context, [string]$EvidenceDirectory, [string]$Name, [string]$ControlType, [int]$TimeoutSeconds) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw "Measured app exited while waiting for dialog control '$Name'." }
        $element = Find-P916DialogName $Context $Name $ControlType
        if ($null -ne $element -and [bool]$element.Current.IsEnabled -and -not [bool]$element.Current.IsOffscreen) { return $element }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for enabled exact dialog UIA name '$Name'."
}

function Add-P916Action([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$Kind, [System.Windows.Automation.AutomationElement]$Element, [string]$Method, [bool]$MainWindowAction = $false) {
    $action = Add-P914Action $Context $Manifest $EvidenceDirectory $Kind $Element $Method
    $action.mainWindowAction = $MainWindowAction
    return $action
}

function Invoke-P916BoundedClick([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$ActionKind, [System.Windows.Automation.AutomationElement]$Element, [Int64]$ExpectedHandle, [bool]$MainWindowAction = $false) {
    $current = $Element.Current
    $snapshot = Get-P914ControlSnapshot $Context $Element
    $ownerHandle = [Int64]$snapshot.ownerWindowHandle
    $measuredPid = [uint32]$Context.process.Id
    if ([uint32]$current.ProcessId -ne $measuredPid -or [Int64]$snapshot.ownerWindowProcessId -ne [Int64]$measuredPid -or $ownerHandle -ne $ExpectedHandle -or [uint32][P914Native]::GetWindowProcessId([IntPtr]$ownerHandle) -ne $measuredPid -or -not [bool]$current.IsEnabled -or [bool]$current.IsOffscreen) { throw "UIA click target failed PID/HWND/enabled/visible checks: $ActionKind" }
    $rect = $current.BoundingRectangle
    if ([double]::IsInfinity($rect.X) -or [double]::IsInfinity($rect.Y) -or [double]::IsNaN($rect.X) -or [double]::IsNaN($rect.Y) -or $rect.Width -le 0 -or $rect.Height -le 0) { throw "UIA click target has no nonempty measured rectangle: $ActionKind" }
    $bounds = [P914Native]::GetBounds([IntPtr]$ownerHandle, $measuredPid)
    if ($rect.X -lt $bounds[0] -or $rect.Y -lt $bounds[1] -or ($rect.X + $rect.Width) -gt ($bounds[0] + $bounds[2]) -or ($rect.Y + $rect.Height) -gt ($bounds[1] + $bounds[3])) { throw "UIA click target leaves its measured owner HWND: $ActionKind" }
    $x = [int][Math]::Round($rect.X + $rect.Width / 2); $y = [int][Math]::Round($rect.Y + $rect.Height / 2)
    $action = Add-P916Action $Context $Manifest $EvidenceDirectory $ActionKind $Element 'UIA-targeted window click' $MainWindowAction
    $action.pointerAction = [ordered]@{ kind = 'WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP at the center of the reacquired UIA BoundingRectangle'; actionType = 'WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP'; screenX = $x; screenY = $y; targetProcessId = [Int64]$measuredPid; windowHandle = $ownerHandle; windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$ownerHandle); measuredWindowBounds = [ordered]@{ x = [int]$bounds[0]; y = [int]$bounds[1]; width = [int]$bounds[2]; height = [int]$bounds[3] } }
    [P914Native]::ClickScreenPoint([IntPtr]$ownerHandle, $measuredPid, $x, $y)
    $action.pointerInputReturnedAtUtc = Get-P914Utc; $action.completedAtUtc = Get-P914Utc
    return $action
}

function Expand-P916FileMenu([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [int]$TimeoutSeconds) {
    $menu = Wait-P916ExactName $Context $EvidenceDirectory 'File' 'ControlType.MenuItem' $TimeoutSeconds
    $expanded = $false
    try {
        $pattern = [System.Windows.Automation.ExpandCollapsePattern]$menu.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
        if ($pattern.Current.ExpandCollapseState -ne [System.Windows.Automation.ExpandCollapseState]::Expanded) {
            $action = Add-P916Action $Context $Manifest $EvidenceDirectory 'open-file-menu' $menu 'ExpandCollapsePattern.Expand' $true
            $pattern.Expand(); $action.invocationReturnedAtUtc = Get-P914Utc
        }
        $expanded = $true
    } catch {
        $action = Add-P916Action $Context $Manifest $EvidenceDirectory 'open-file-menu' $menu 'InvokePattern.Invoke' $true
        ([System.Windows.Automation.InvokePattern]$menu.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)).Invoke()
        $action.invocationReturnedAtUtc = Get-P914Utc; $expanded = $true
    }
    if (-not $expanded) { throw 'File menu was not opened through its PID-bound UIA menu item.' }
}

function Collapse-P916FileMenu([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [int]$TimeoutSeconds) {
    $menu = Wait-P916ExactName $Context $EvidenceDirectory 'File' 'ControlType.MenuItem' $TimeoutSeconds
    $pattern = [System.Windows.Automation.ExpandCollapsePattern]$menu.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
    if ($pattern.Current.ExpandCollapseState -eq [System.Windows.Automation.ExpandCollapseState]::Expanded) {
        $action = Add-P916Action $Context $Manifest $EvidenceDirectory 'close-file-menu' $menu 'ExpandCollapsePattern.Collapse' $true
        $pattern.Collapse(); $action.invocationReturnedAtUtc = Get-P914Utc
    }
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt [Math]::Min($TimeoutSeconds, 10)) {
        if ($pattern.Current.ExpandCollapseState -eq [System.Windows.Automation.ExpandCollapseState]::Collapsed) { return }
        Start-Sleep -Milliseconds 50
    }
    throw 'File menu remained expanded after the UIA Collapse action.'
}

function Get-P916StateSnapshot([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$State, [string]$SelectedWorkspacePath = '', [object]$Identity = $null) {
    $root = Get-P916MainRoot $Context
    if ($null -eq $root) { throw 'Measured main window disappeared while observing workspace state.' }
    $mainHandle = [Int64]$Context.mainWindowHandle
    if ([uint32][P914Native]::GetWindowProcessId([IntPtr]$mainHandle) -ne [uint32]$Context.process.Id -or -not [P914Native]::IsVisible([IntPtr]$mainHandle)) { throw 'Measured main HWND changed or no longer belongs to the original PID.' }
    Expand-P916FileMenu $Context $Manifest $EvidenceDirectory 20
    $closeAction = Find-P916Suffix $Context 'classMngrNextCloseWorkspace' '' 'main'
    if ($null -eq $closeAction) { throw 'Exact classMngrNextCloseWorkspace QAction is absent from the expanded File menu.' }
    $closeSnapshot = Get-P914ControlSnapshot $Context $closeAction
    Collapse-P916FileMenu $Context $Manifest $EvidenceDirectory 10
    $page = $null
    if ($State -ceq 'open') {
        $page = Find-P916Suffix $Context 'pageTitle' 'ControlType.Text' 'main'
        if ($null -eq $page -or [string]$page.Current.Name -cne 'My Workspace' -or [bool]$page.Current.IsOffscreen) { throw 'Open state does not expose the visible My Workspace page title.' }
        if (-not [bool]$closeSnapshot.isEnabled) { throw 'Close Teacher Profile is disabled while the workspace is open.' }
    } else {
        $page = Find-P916Suffix $Context 'classMngrNextInitialSetupTitle' '' 'main'
        if ($null -eq $page -or [string]$page.Current.Name -cne 'Initial Setup' -or [bool]$page.Current.IsOffscreen) { throw 'Closed state does not expose the visible Initial Setup title.' }
        if ([bool]$closeSnapshot.isEnabled) { throw 'Close Teacher Profile remains enabled after workspace close.' }
    }
    [ordered]@{
        state = $State; currentPage = if ($State -ceq 'open') { 'My Workspace' } else { 'Initial Setup' }
        pageTitle = [string]$page.Current.Name; pageAutomationId = [string]$page.Current.AutomationId
        processId = [Int64]$Context.process.Id; mainWindowHandle = $mainHandle
        closeActionEnabled = [bool]$closeSnapshot.isEnabled; closeAction = $closeSnapshot
        selectedWorkspacePath = if ($SelectedWorkspacePath) { [IO.Path]::GetFullPath($SelectedWorkspacePath) } else { $null }
        identity = $Identity; observedAtUtc = Get-P914Utc
    }
}

function Get-P916VisibleWindows([object]$Context) {
    @(Get-P914VisibleTopLevelWindows $Context)
}

function Get-P916CleanStateCheck([object]$Context, [string]$EvidenceDirectory, [object]$State) {
    $windows = @(Get-P916VisibleWindows $Context)
    if ($windows.Count -ne 1 -or [Int64]$windows[0].windowHandle -ne [Int64]$Context.mainWindowHandle) { throw 'Pre-close workspace state contains an unexpected visible dialog or secondary window.' }
    if ($State.state -cne 'open' -or $State.currentPage -cne 'My Workspace' -or -not [bool]$State.closeActionEnabled) { throw 'Pre-close state is not the clean My Workspace state.' }
    [ordered]@{
        passed = $true; currentPage = [string]$State.currentPage; noUserEditsPerformed = $true
        unexpectedModalWindows = @(); visibleTopLevelWindowCount = $windows.Count
        saveOrPreferenceActionsSinceOpen = 0; pageInteractionsSinceOpen = 0
        basis = 'Harness interacted only with File menu and the isolated Open Teacher Profile dialog; no workspace fields, preference controls, or page actions were invoked.'
        checkedAtUtc = Get-P914Utc
    }
}

function Get-P916DialogRoot([object]$Context, [string]$EvidenceDirectory, [int]$TimeoutSeconds) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw 'Measured process exited while waiting for the Open Teacher Profile file dialog.' }
        $matches = [Collections.Generic.List[object]]::new()
        foreach ($handle in [P914Native]::GetTopLevelWindows([uint32]$Context.process.Id)) {
            if ([Int64]$handle.ToInt64() -eq [Int64]$Context.mainWindowHandle -or -not [P914Native]::IsVisible($handle)) { continue }
            if ([uint32][P914Native]::GetWindowProcessId($handle) -ne [uint32]$Context.process.Id) { throw 'P9-16 dialog HWND is not owned by the measured process.' }
            $root = [System.Windows.Automation.AutomationElement]::FromHandle($handle); $current = $root.Current
            $id = [string]$current.AutomationId
            if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id -or [string]$current.ControlType.ProgrammaticName -cne 'ControlType.Window') { continue }
            if ($id -cne 'classmngrNextFileDialog' -and -not $id.EndsWith('.classmngrNextFileDialog', [StringComparison]::Ordinal)) { continue }
            if ([string]$current.ClassName -cne 'QFileDialog') { throw "P9-16 Open Teacher Profile dialog is not the isolated Qt QFileDialog (class='$($current.ClassName)')." }
            $Context.ownerHandles[(Get-P914RuntimeKey $root)] = [Int64]$handle.ToInt64()
            $matches.Add([ordered]@{ root = $root; windowHandle = [Int64]$handle.ToInt64(); automationId = $id; name = [string]$current.Name; className = [string]$current.ClassName; controlType = [string]$current.ControlType.ProgrammaticName })
        }
        if ($matches.Count -gt 1) { throw 'P9-16 file dialog identity matched more than one visible top-level window.' }
        if ($matches.Count -eq 1) { $Context.dialogRoot = $matches[0].root; $Context.dialogHandle = [Int64]$matches[0].windowHandle; return $matches[0] }
        Start-Sleep -Milliseconds 100
    }
    throw 'Timed out waiting for exact PID-bound classmngrNextFileDialog QFileDialog.'
}

function Save-P916DialogTree([object]$Context, [string]$EvidenceDirectory, [string]$Name) {
    $root = $Context.dialogRoot
    $items = [Collections.Generic.List[object]]::new()
    $nodes = [Collections.Generic.List[System.Windows.Automation.AutomationElement]]::new(); $nodes.Add($root)
    foreach ($node in $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) { $nodes.Add($node) }
    foreach ($node in $nodes) {
        $current = $node.Current; $rect = $current.BoundingRectangle
        $items.Add([ordered]@{ name = [string]$current.Name; automationId = [string]$current.AutomationId; controlType = [string]$current.ControlType.ProgrammaticName; className = [string]$current.ClassName; processId = [Int64]$current.ProcessId; ownerWindowHandle = [Int64]$Context.dialogHandle; ownerWindowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$Context.dialogHandle); isEnabled = [bool]$current.IsEnabled; isOffscreen = [bool]$current.IsOffscreen; value = (Get-P914EditValue $node); rectangle = [ordered]@{ x = [double]$rect.X; y = [double]$rect.Y; width = [double]$rect.Width; height = [double]$rect.Height } })
    }
    $path = Join-Path (Join-Path $EvidenceDirectory 'uia') ($Name + '.json')
    $record = [ordered]@{ processId = [Int64]$Context.process.Id; mainWindowHandle = [Int64]$Context.mainWindowHandle; dialogWindowHandle = [Int64]$Context.dialogHandle; capturedAtUtc = Get-P914Utc; controls = $items.ToArray() }
    Write-P914Json $path $record
    [ordered]@{ relativePath = 'uia/' + (Split-Path -Leaf $path); sha256 = Get-P914FileSha256 $path; controlCount = $items.Count }
}

function Wait-P916DialogClosed([object]$Context, [string]$EvidenceDirectory, [int]$TimeoutSeconds) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Drain-P914Output $Context $EvidenceDirectory; $Context.process.Refresh()
        if ($Context.process.HasExited) { throw 'Measured process exited before the file dialog closed.' }
        $found = $false
        foreach ($handle in [P914Native]::GetTopLevelWindows([uint32]$Context.process.Id)) {
            if ([Int64]$handle.ToInt64() -eq [Int64]$Context.dialogHandle) { $found = [P914Native]::IsVisible($handle); break }
        }
        if (-not $found) { return }
        Start-Sleep -Milliseconds 100
    }
    throw 'Open Teacher Profile QFileDialog remained visible after its Open action.'
}

function Save-P916RecentMenuCapture([string]$EvidenceDirectory, [string]$CaptureName, [object]$Context, [object]$RecentMenu, [string]$WorkspacePath, [object[]]$VisibleItems, [object[]]$SelectedItems) {
    $relative = 'uia/' + $CaptureName + '.json'
    $path = Join-Path $EvidenceDirectory ($relative.Replace('/', [IO.Path]::DirectorySeparatorChar))
    Write-P914Json $path ([ordered]@{
        capturedAtUtc = Get-P914Utc
        processId = [Int64]$Context.process.Id
        mainWindowHandle = [Int64]$Context.mainWindowHandle
        expectedWorkspacePath = [IO.Path]::GetFullPath($WorkspacePath)
        recentFilesMenu = Get-P914ControlSnapshot $Context $RecentMenu
        visibleMenuItems = @($VisibleItems)
        selectedItem = @($SelectedItems)
    })
    return [ordered]@{ relativePath = $relative; sha256 = Get-P914FileSha256 $path }
}

function Find-P916RecentWorkspaceAction([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$WorkspacePath, [int]$TimeoutSeconds, [string]$CaptureName) {
    $recentMenu = Wait-P916ExactName $Context $EvidenceDirectory 'Recent Files' 'ControlType.MenuItem' $TimeoutSeconds
    if ($null -eq $recentMenu) { throw 'File menu does not expose the exact Recent Files UIA submenu.' }
    $expanded = $false
    try {
        $pattern = [System.Windows.Automation.ExpandCollapsePattern]$recentMenu.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
        if ($pattern.Current.ExpandCollapseState -ne [System.Windows.Automation.ExpandCollapseState]::Expanded) {
            $expandAction = Add-P916Action $Context $Manifest $EvidenceDirectory 'expand-recent-files-menu' $recentMenu 'ExpandCollapsePattern.Expand' $true
            $pattern.Expand()
            $expandAction.invocationReturnedAtUtc = Get-P914Utc
        }
        $watch = [Diagnostics.Stopwatch]::StartNew()
        while ($watch.Elapsed.TotalSeconds -lt [Math]::Min($TimeoutSeconds, 10)) {
            if ($pattern.Current.ExpandCollapseState -eq [System.Windows.Automation.ExpandCollapseState]::Expanded) { $expanded = $true; break }
            Start-Sleep -Milliseconds 50
        }
    } catch {
        $expandAction = Invoke-P916BoundedClick $Context $Manifest $EvidenceDirectory 'expand-recent-files-menu' $recentMenu ([Int64]$Context.mainWindowHandle) $true
        $expanded = $true
    }
    if (-not $expanded) { throw 'Recent Files submenu did not expand through its PID-bound UIA menu item.' }

    $expectedPath = [IO.Path]::GetFullPath($WorkspacePath)
    $visibleItems = [Collections.Generic.List[object]]::new()
    $matches = [Collections.Generic.List[object]]::new()
    $elements = $recentMenu.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($element in $elements) {
        $current = $element.Current
        if ([uint32]$current.ProcessId -ne [uint32]$Context.process.Id) { continue }
        $name = [string]$current.Name
        $helpText = ''
        try { $helpText = [string]$element.GetCurrentPropertyValue([System.Windows.Automation.AutomationElement]::HelpTextProperty) } catch { }
        $namePath = $name -replace '^\s*&?\d+\s+', ''
        $nameMatchesPath = [string]::Equals($namePath, $expectedPath, [StringComparison]::OrdinalIgnoreCase)
        $helpTextMatchesPath = [string]::Equals($helpText, $expectedPath, [StringComparison]::OrdinalIgnoreCase)
        $visibleItems.Add([ordered]@{
            name = $name; namePath = $namePath; helpText = $helpText
            automationId = [string]$current.AutomationId
            className = [string]$current.ClassName
            controlType = [string]$current.ControlType.ProgrammaticName
            processId = [Int64]$current.ProcessId
            nativeWindowHandle = [Int64]$current.NativeWindowHandle
            isEnabled = [bool]$current.IsEnabled
            isOffscreen = [bool]$current.IsOffscreen
            nameMatchesExpectedPath = $nameMatchesPath
            helpTextMatchesExpectedPath = $helpTextMatchesPath
        })
        if ([string]$current.ControlType.ProgrammaticName -cne 'ControlType.MenuItem' -or [bool]$current.IsOffscreen -or -not [bool]$current.IsEnabled) { continue }
        if (-not ($nameMatchesPath -or $helpTextMatchesPath)) { continue }
        $ownerHandle = [Int64]$Context.mainWindowHandle
        $nativeHandle = [Int64]$current.NativeWindowHandle
        if ($nativeHandle -gt 0 -and [uint32][P914Native]::GetWindowProcessId([IntPtr]$nativeHandle) -eq [uint32]$Context.process.Id) {
            $ownerHandle = $nativeHandle
        } else {
            $rect = $current.BoundingRectangle
            $smallestArea = [Double]::PositiveInfinity
            foreach ($candidateHandle in [P914Native]::GetTopLevelWindows([uint32]$Context.process.Id)) {
                if (-not [P914Native]::IsVisible($candidateHandle)) { continue }
                $bounds = [P914Native]::GetBounds($candidateHandle, [uint32]$Context.process.Id)
                if ($rect.X -lt $bounds[0] -or $rect.Y -lt $bounds[1] -or ($rect.X + $rect.Width) -gt ($bounds[0] + $bounds[2]) -or ($rect.Y + $rect.Height) -gt ($bounds[1] + $bounds[3])) { continue }
                $area = [Double]$bounds[2] * [Double]$bounds[3]
                if ($area -lt $smallestArea) { $smallestArea = $area; $ownerHandle = [Int64]$candidateHandle.ToInt64() }
            }
        }
        $Context.ownerHandles[(Get-P914RuntimeKey $element)] = $ownerHandle
        $matches.Add([pscustomobject]@{
            element = $element; name = $name; namePath = $namePath; helpText = $helpText
            nameMatchesExpectedPath = $nameMatchesPath; helpTextMatchesExpectedPath = $helpTextMatchesPath
            ownerWindowHandle = $ownerHandle; automationId = [string]$current.AutomationId
            className = [string]$current.ClassName; controlType = [string]$current.ControlType.ProgrammaticName
        })
    }
    $matchSummaries = @($matches | ForEach-Object {
        [ordered]@{ name = $_.name; namePath = $_.namePath; helpText = $_.helpText; automationId = $_.automationId; className = $_.className; controlType = $_.controlType; ownerWindowHandle = $_.ownerWindowHandle }
    })
    $capture = Save-P916RecentMenuCapture $EvidenceDirectory $CaptureName $Context $recentMenu $expectedPath @($visibleItems.ToArray()) -SelectedItems $matchSummaries
    if ($matches.Count -ne 1) { throw "Recent Files UIA submenu resolved $($matches.Count) enabled menu items matching the exact fixture path '$expectedPath'; capture=$($capture.relativePath)." }
    $selected = $matches[0]
    $target = Get-P914ControlSnapshot $Context $selected.element
    if ($target.name -cne $selected.name -or [Int64]$target.ownerWindowProcessId -ne [Int64]$Context.process.Id) { throw 'Exact Recent Files dynamic menu item changed identity before invocation.' }
    return [ordered]@{
        menu = $recentMenu
        element = $selected.element
        control = $target
        menuName = 'Recent Files'
        menuExpanded = $true
        expectedPath = $expectedPath
        itemName = $selected.name
        itemNamePath = $selected.namePath
        itemHelpText = $selected.helpText
        itemNameMatchesPath = [bool]$selected.nameMatchesExpectedPath
        itemHelpTextMatchesPath = [bool]$selected.helpTextMatchesExpectedPath
        capture = $capture
    }
}

function Complete-P916WorkspaceOpen([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$WorkspacePath, [string]$ReceiptPath, [int]$TimeoutSeconds, [string]$Purpose, [int]$CycleNumber, [Int64]$RouteArrival, [object]$OpenActionRecord, [string]$OpenMethod, [object]$DialogSession = $null, [object]$RecentSession = $null) {
    $cycleSuffix = if ($CycleNumber -gt 0) { '-' + $CycleNumber } else { '' }
    $event = Wait-P914Route $Context $EvidenceDirectory 'my_workspace' $RouteArrival $TimeoutSeconds
    $identityLogName = 'identity-' + $Purpose + $cycleSuffix + '.txt'
    $identityResult = Invoke-P916Preparer $EvidenceDirectory @('identity', '--workspace', $WorkspacePath, '--preparation-receipt', $ReceiptPath) $identityLogName
    $identityCounts = $identityResult.identity
    $expectedIdentity = $Manifest.fixture.preparationReceipt
    if ($identityCounts.classCount -ne $expectedIdentity.classCount -or $identityCounts.classInfoCount -ne $expectedIdentity.classInfoCount -or $identityCounts.scheduleEntryCount -ne $expectedIdentity.scheduleEntryCount) { throw 'Reopened workspace identity differs from its pinned profile-size fixture.' }
    $identityEvidence = [ordered]@{ classCount = [int]$identityCounts.classCount; classInfoCount = [int]$identityCounts.classInfoCount; scheduleEntryCount = [int]$identityCounts.scheduleEntryCount; workspaceTablesSha256 = [string]$identityResult.workspaceTablesSha256 }
    $state = Get-P916StateSnapshot $Context $Manifest $EvidenceDirectory 'open' $WorkspacePath $identityEvidence
    $state.openMethod = $OpenMethod
    $state.routeEvent = $event
    $state.routeArrivalIndex = [Int64]$event.arrivalIndex
    $state.cleanStateCheck = [ordered]@{ passed = $true; noUserEditsPerformed = $true; pageInteractionsSinceOpen = 0; preferenceActionsSinceOpen = 0; observedAtUtc = Get-P914Utc }
    if ($null -ne $DialogSession) {
        $DialogSession.routeEventArrivalIndex = [Int64]$event.arrivalIndex
        $DialogSession.completedAtUtc = Get-P914Utc
        $Manifest.route.fileDialogSessions.Add($DialogSession)
    }
    if ($null -ne $RecentSession) {
        $RecentSession.routeEventArrivalIndex = [Int64]$event.arrivalIndex
        $RecentSession.completedAtUtc = Get-P914Utc
        $Manifest.route.recentOpenSessions.Add($RecentSession)
    }
    Set-P914SamplerContext $Context 'normal-use' ($Purpose + '-workspace-open-settled')
    return [ordered]@{
        state = $state
        openActionIndex = [int]($Manifest.route.actions.IndexOf($OpenActionRecord))
        routeEvent = $event
        fileDialogSession = $DialogSession
        recentOpenSession = $RecentSession
    }
}

function Invoke-P916OpenWorkspace([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$WorkspacePath, [string]$ReceiptPath, [int]$TimeoutSeconds, [string]$Purpose, [int]$CycleNumber = 0) {
    Set-P914SamplerContext $Context 'operation' ($Purpose + '-open-workspace')
    $routeArrival = [Int64]$Context.arrival
    Expand-P916FileMenu $Context $Manifest $EvidenceDirectory $TimeoutSeconds
    $openActionElement = Wait-P916Suffix $Context $EvidenceDirectory 'classMngrNextOpenTeacherProfile' '' $TimeoutSeconds 'main'
    $openAction = $openActionElement.Current
    if ([string]$openAction.AutomationId -cne 'classMngrNextOpenTeacherProfile' -and -not [string]$openAction.AutomationId.EndsWith('.classMngrNextOpenTeacherProfile', [StringComparison]::Ordinal)) { throw 'Open Teacher Profile UIA action identity differs from the exact Next QAction.' }
    $openActionRecord = Invoke-P916BoundedClick $Context $Manifest $EvidenceDirectory 'open-teacher-profile' $openActionElement ([Int64]$Context.mainWindowHandle) $true
    $dialog = Get-P916DialogRoot $Context $EvidenceDirectory $TimeoutSeconds
    if ([Int64]$dialog.windowHandle -eq [Int64]$Context.mainWindowHandle) { throw 'Non-native file dialog unexpectedly reused the main HWND.' }
    $cycleSuffix = if ($CycleNumber -gt 0) { '-' + $CycleNumber } else { '' }
    $dialogTreeName = 'file-dialog-' + $Purpose + $cycleSuffix
    $treeRef = Save-P916DialogTree $Context $EvidenceDirectory $dialogTreeName
    $fileName = Wait-P916Suffix $Context $EvidenceDirectory 'fileNameEdit' '' $TimeoutSeconds 'dialog'
    if ([string]$fileName.Current.ClassName -cne 'QLineEdit') { throw 'File dialog path control is not a Qt QLineEdit.' }
    $pathAction = Add-P916Action $Context $Manifest $EvidenceDirectory 'set-workspace-dialog-path' $fileName 'ValuePattern.SetValue' $false
    $pathAction.valueAfter = [IO.Path]::GetFullPath($WorkspacePath)
    $pathAction.dialogWindowHandle = [Int64]$Context.dialogHandle
    $valuePattern = [System.Windows.Automation.ValuePattern]$fileName.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $valuePattern.SetValue([IO.Path]::GetFullPath($WorkspacePath))
    $pathAction.setReturnedAtUtc = Get-P914Utc
    $pathReadback = Get-P914EditValue $fileName
    if ($pathReadback.Trim('"') -cne [IO.Path]::GetFullPath($WorkspacePath)) { throw "Qt file dialog path readback differs from the pinned fixture: '$pathReadback'." }
    $openButton = Wait-P916DialogName $Context $EvidenceDirectory 'Open' 'ControlType.Button' $TimeoutSeconds
    if ([string]$openButton.Current.ClassName -cne 'QPushButton' -or -not [bool]$openButton.Current.IsEnabled) { throw 'Qt file dialog Open button is not an enabled QPushButton.' }
    $confirm = $null
    $invokePattern = $null
    try { $invokePattern = [System.Windows.Automation.InvokePattern]$openButton.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern) } catch { $invokePattern = $null }
    if ($null -ne $invokePattern) {
        $confirm = Add-P916Action $Context $Manifest $EvidenceDirectory 'confirm-workspace-file-dialog' $openButton 'InvokePattern.Invoke' $false
        $invokePattern.Invoke(); $confirm.invocationReturnedAtUtc = Get-P914Utc
    } else {
        $confirm = Invoke-P916BoundedClick $Context $Manifest $EvidenceDirectory 'confirm-workspace-file-dialog' $openButton ([Int64]$Context.dialogHandle) $false
    }
    $session = [ordered]@{
        purpose = $Purpose; cycleNumber = if ($CycleNumber -gt 0) { $CycleNumber } else { $null }
        automationId = [string]$dialog.automationId; name = [string]$dialog.name; className = [string]$dialog.className; controlType = [string]$dialog.controlType
        processId = [Int64]$Context.process.Id; ownerWindowHandle = [Int64]$dialog.windowHandle; ownerWindowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$dialog.windowHandle)
        mainWindowHandle = [Int64]$Context.mainWindowHandle; selectedWorkspacePath = [IO.Path]::GetFullPath($WorkspacePath)
        fileNameControl = Get-P914ControlSnapshot $Context $fileName; openButtonControl = Get-P914ControlSnapshot $Context $openButton
        pathReadback = $pathReadback; uiTree = $treeRef; openedAtUtc = Get-P914Utc
    }
    $Context.dialogRoot = $null; $Context.dialogHandle = 0L
    Wait-P916DialogClosed $Context $EvidenceDirectory $TimeoutSeconds
    return Complete-P916WorkspaceOpen $Context $Manifest $EvidenceDirectory $WorkspacePath $ReceiptPath $TimeoutSeconds $Purpose $CycleNumber $routeArrival $openActionRecord 'dialog' $session
}

function Invoke-P916OpenWorkspaceFromRecent([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [string]$WorkspacePath, [string]$ReceiptPath, [int]$TimeoutSeconds, [string]$Purpose, [int]$CycleNumber = 0) {
    Set-P914SamplerContext $Context 'operation' ($Purpose + '-open-workspace-recent')
    $routeArrival = [Int64]$Context.arrival
    Expand-P916FileMenu $Context $Manifest $EvidenceDirectory $TimeoutSeconds
    $captureSuffix = if ($CycleNumber -gt 0) { '-' + $CycleNumber } else { '' }
    $captureName = 'recent-menu-' + $Purpose + $captureSuffix
    $selection = Find-P916RecentWorkspaceAction $Context $Manifest $EvidenceDirectory $WorkspacePath $TimeoutSeconds $captureName
    if (-not ($selection.itemNameMatchesPath -or $selection.itemHelpTextMatchesPath)) { throw 'Recent Files UIA action does not expose the exact pinned absolute fixture path.' }
    $action = Invoke-P916BoundedClick $Context $Manifest $EvidenceDirectory 'open-recent-workspace' $selection.element ([Int64]$selection.control.ownerWindowHandle) $false
    $action.selectedWorkspacePath = [IO.Path]::GetFullPath($WorkspacePath)
    $action.recentFilesMenuName = [string]$selection.menuName
    $action.recentFilesMenuExpanded = [bool]$selection.menuExpanded
    $action.recentItemName = [string]$selection.itemName
    $action.recentItemNamePath = [string]$selection.itemNamePath
    $action.recentItemHelpText = [string]$selection.itemHelpText
    $action.recentItemNameMatchesPath = [bool]$selection.itemNameMatchesPath
    $action.recentItemHelpTextMatchesPath = [bool]$selection.itemHelpTextMatchesPath
    $action.recentItemClassName = [string]$selection.control.className
    $action.recentItemControlType = [string]$selection.control.controlType
    $action.recentItemDiscoveredUnderRecentMenu = $true
    $action.recentMenuCapture = $selection.capture
    $session = [ordered]@{
        purpose = $Purpose
        cycleNumber = if ($CycleNumber -gt 0) { $CycleNumber } else { $null }
        selectedWorkspacePath = [IO.Path]::GetFullPath($WorkspacePath)
        processId = [Int64]$Context.process.Id
        mainWindowHandle = [Int64]$Context.mainWindowHandle
        recentFilesMenuName = [string]$selection.menuName
        recentFilesMenuExpanded = [bool]$selection.menuExpanded
        recentMenuOwnerWindowHandle = [Int64]$Context.mainWindowHandle
        recentItemControl = $selection.control
        recentItemName = [string]$selection.itemName
        recentItemNamePath = [string]$selection.itemNamePath
        recentItemHelpText = [string]$selection.itemHelpText
        recentItemNameMatchesPath = [bool]$selection.itemNameMatchesPath
        recentItemHelpTextMatchesPath = [bool]$selection.itemHelpTextMatchesPath
        recentItemDiscoveredUnderRecentMenu = $true
        openActionIndex = [int]($Manifest.route.actions.IndexOf($action))
        uiTree = $selection.capture
        openedAtUtc = Get-P914Utc
    }
    return Complete-P916WorkspaceOpen $Context $Manifest $EvidenceDirectory $WorkspacePath $ReceiptPath $TimeoutSeconds $Purpose $CycleNumber $routeArrival $action 'recent' $null $session
}

function Invoke-P916CloseWorkspace([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [int]$TimeoutSeconds, [string]$Purpose, [int]$CycleNumber = 0) {
    $preState = Get-P916StateSnapshot $Context $Manifest $EvidenceDirectory 'open'
    $clean = Get-P916CleanStateCheck $Context $EvidenceDirectory $preState
    Set-P914SamplerContext $Context 'operation' ($Purpose + '-close-workspace')
    $routeArrival = [Int64]$Context.arrival
    Expand-P916FileMenu $Context $Manifest $EvidenceDirectory $TimeoutSeconds
    $closeElement = Wait-P916Suffix $Context $EvidenceDirectory 'classMngrNextCloseWorkspace' '' $TimeoutSeconds 'main'
    $target = $closeElement.Current
    if ([string]$target.AutomationId -cne 'classMngrNextCloseWorkspace' -and -not [string]$target.AutomationId.EndsWith('.classMngrNextCloseWorkspace', [StringComparison]::Ordinal)) { throw 'Close Teacher Profile UIA QAction identity differs from the exact Next action.' }
    if (-not [bool]$target.IsEnabled) { throw 'Close Teacher Profile action is disabled before a required close.' }
    $action = Invoke-P916BoundedClick $Context $Manifest $EvidenceDirectory 'close-teacher-profile' $closeElement ([Int64]$Context.mainWindowHandle) $true
    $event = Wait-P914Route $Context $EvidenceDirectory 'initial_setup' $routeArrival $TimeoutSeconds
    $state = Get-P916StateSnapshot $Context $Manifest $EvidenceDirectory 'closed'
    $state.routeEvent = $event; $state.routeArrivalIndex = [Int64]$event.arrivalIndex; $state.cleanStateCheck = $clean
    $state.cleanStateCheck.closePromptObserved = $false
    $state.cleanStateCheck.closedWithoutSavePrompt = $true
    $state.closedWithoutSavePrompt = $true
    $state.cleanStateCheck.beforeClosePage = $preState.currentPage
    $state.closeActionIndex = [int]($Manifest.route.actions.IndexOf($action))
    Set-P914SamplerContext $Context 'normal-use' ($Purpose + '-workspace-closed-settled')
    return $state
}

function Get-P916MemoryWindow([object]$Context, [object]$Manifest, [string]$Name, [string]$State, [string]$Phase, [int]$CycleNumber = 0) {
    $Context.process.Refresh(); if ($Context.process.HasExited) { throw "Cannot sample settled memory window after process exit: $Name" }
    Set-P914SamplerContext $Context 'normal-use' ($Phase + '-' + $State + '-' + $Name)
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $samples = [Collections.Generic.List[object]]::new()
    for ($index = 1; $index -le 10; $index++) {
        Start-Sleep -Milliseconds 100
        $Context.process.Refresh(); if ($Context.process.HasExited) { throw "Measured process exited during memory window $Name." }
        $workingSet = [Int64]$Context.process.WorkingSet64
        $privateBytes = [Int64]$Context.process.PrivateMemorySize64
        if ($workingSet -ge $StrictWorkingSetLimit) { throw "Settled normal-use working set reached strict exclusive cap in $Name sample ${index}: $workingSet bytes." }
        $samples.Add([ordered]@{
            sampleIndex = $index - 1; capturedAtUtc = Get-P914Utc; elapsedMilliseconds = [int]$watch.ElapsedMilliseconds
            workingSetBytes = $workingSet; privateBytes = $privateBytes
            handles = [Int64]$Context.process.HandleCount; threadCount = [Int64]$Context.process.Threads.Count
        })
    }
    $window = [ordered]@{
        windowId = $Name; state = $State; phase = $Phase; cycleNumber = if ($CycleNumber -gt 0) { $CycleNumber } else { $null }
        processId = [Int64]$Context.process.Id; mainWindowHandle = [Int64]$Context.mainWindowHandle
        sampleIntervalMilliseconds = 100; nominalDurationMilliseconds = 1000; startedAtUtc = $samples[0].capturedAtUtc
        completedAtUtc = Get-P914Utc; samples = $samples.ToArray()
    }
    $Manifest.memory.windows.Add($window)
    return $window
}

function Save-P916FinalWorkspaceSnapshot([string]$EvidenceDirectory, [object]$Fixture) {
    $afterPath = Join-Path (Join-Path $EvidenceDirectory 'workspace') 'after-snapshot.json'
    $summary = Invoke-P916Preparer $EvidenceDirectory @('snapshot', '--phase', 'after', '--workspace', $Fixture.workspacePath, '--preparation-receipt', $Fixture.preparationPath, '--output', $afterPath) 'after-snapshot-output.txt'
    [ordered]@{ relativePath = 'workspace/after-snapshot.json'; sha256 = Get-P914FileSha256 $afterPath; phase = 'after'; workspaceSha256 = [string](Read-P914Json $afterPath).workspaceSha256; identity = [ordered]@{ classCount = [int]$summary.classCount; classInfoCount = [int]$summary.classInfoCount; scheduleEntryCount = [int]$summary.scheduleEntryCount } }
}

function Invoke-P916Route([object]$Context, [object]$Manifest, [string]$EvidenceDirectory, [object]$Fixture, [int]$TimeoutSeconds) {
    # A blank startup keeps the first workspace open operation inside the
    # explicitly isolated non-native Qt file dialog.
    $initialArrival = [Int64]$Context.arrival
    $closedState = Wait-P916Suffix $Context $EvidenceDirectory 'classMngrNextInitialSetupTitle' '' $TimeoutSeconds 'main'
    if ($null -eq $closedState -or [string]$closedState.Current.Name -cne 'Initial Setup') { throw 'Blank startup did not reach Initial Setup before the first open.' }
    $initialState = Get-P916StateSnapshot $Context $Manifest $EvidenceDirectory 'closed'
    $initialState.startupRouteArrivalIndex = $initialArrival
    $Manifest.route.initialStartupState = $initialState

    $warmOpen = Invoke-P916OpenWorkspace $Context $Manifest $EvidenceDirectory $Fixture.workspacePath $Fixture.preparationPath $TimeoutSeconds 'warmup'
    $warmOpenWindow = Get-P916MemoryWindow $Context $Manifest 'warmup-open' 'open' 'warmup'
    $warmClose = Invoke-P916CloseWorkspace $Context $Manifest $EvidenceDirectory $TimeoutSeconds 'warmup'
    $warmCloseWindow = Get-P916MemoryWindow $Context $Manifest 'warmup-closed' 'closed' 'warmup'
    $Manifest.route.cycles.Add([ordered]@{
        cycleKind = 'warmup'; cycleNumber = 0; open = $warmOpen.state; close = $warmClose
        openActionIndex = $warmOpen.openActionIndex; closeActionIndex = [int]$warmClose.closeActionIndex
        openMemoryWindowId = [string]$warmOpenWindow.windowId; closeMemoryWindowId = [string]$warmCloseWindow.windowId
        completedAtUtc = Get-P914Utc
    })

    for ($prime = 1; $prime -le $PrimingCycleCount; $prime++) {
        $primePurpose = 'priming-' + $prime
        $primeOpen = Invoke-P916OpenWorkspaceFromRecent $Context $Manifest $EvidenceDirectory $Fixture.workspacePath $Fixture.preparationPath $TimeoutSeconds $primePurpose $prime
        $primeClose = Invoke-P916CloseWorkspace $Context $Manifest $EvidenceDirectory $TimeoutSeconds $primePurpose $prime
        $Manifest.route.cycles.Add([ordered]@{
            cycleKind = 'priming'; cycleNumber = $prime; open = $primeOpen.state; close = $primeClose
            openActionIndex = $primeOpen.openActionIndex; closeActionIndex = [int]$primeClose.closeActionIndex
            openMemoryWindowId = $null; closeMemoryWindowId = $null
            completedAtUtc = Get-P914Utc
        })
    }

    for ($index = 1; $index -le 5; $index++) { $null = Get-P916MemoryWindow $Context $Manifest ('baseline-closed-' + $index) 'closed' 'baseline' }
    if ($OpenMode -ceq 'recent') {
        $baselineOpen = Invoke-P916OpenWorkspaceFromRecent $Context $Manifest $EvidenceDirectory $Fixture.workspacePath $Fixture.preparationPath $TimeoutSeconds 'baseline-entry'
    } else {
        $baselineOpen = Invoke-P916OpenWorkspace $Context $Manifest $EvidenceDirectory $Fixture.workspacePath $Fixture.preparationPath $TimeoutSeconds 'baseline-entry'
    }
    $Manifest.route.baselineOpenEntry = $baselineOpen.state
    $Manifest.route.baselineOpenEntry.openActionIndex = $baselineOpen.openActionIndex
    $Manifest.route.baselineOpenEntry.openMode = $OpenMode
    if ($OpenMode -ceq 'dialog') { $Manifest.route.baselineOpenEntry.fileDialogSessionPurpose = 'baseline-entry' }
    for ($index = 1; $index -le 5; $index++) { $null = Get-P916MemoryWindow $Context $Manifest ('baseline-open-' + $index) 'open' 'baseline' }

    for ($cycle = 1; $cycle -le $MeasuredCycleCount; $cycle++) {
        $closed = Invoke-P916CloseWorkspace $Context $Manifest $EvidenceDirectory $TimeoutSeconds ('cycle-' + $cycle) $cycle
        $closedWindow = Get-P916MemoryWindow $Context $Manifest ('cycle-' + $cycle + '-closed') 'closed' 'cycle' $cycle
        if ($OpenMode -ceq 'recent') {
            $opened = Invoke-P916OpenWorkspaceFromRecent $Context $Manifest $EvidenceDirectory $Fixture.workspacePath $Fixture.preparationPath $TimeoutSeconds ('cycle-' + $cycle) $cycle
        } else {
            $opened = Invoke-P916OpenWorkspace $Context $Manifest $EvidenceDirectory $Fixture.workspacePath $Fixture.preparationPath $TimeoutSeconds ('cycle-' + $cycle) $cycle
        }
        $openWindow = Get-P916MemoryWindow $Context $Manifest ('cycle-' + $cycle + '-open') 'open' 'cycle' $cycle
        $Manifest.route.cycles.Add([ordered]@{
            cycleKind = 'measured'; cycleNumber = $cycle; close = $closed; open = $opened.state
            closeActionIndex = [int]$closed.closeActionIndex; openActionIndex = $opened.openActionIndex
            closeMemoryWindowId = [string]$closedWindow.windowId; openMemoryWindowId = [string]$openWindow.windowId
            completedAtUtc = Get-P914Utc
        })
    }
    $Manifest.route.primingCycleCount = $PrimingCycleCount
    $Manifest.route.measuredCycleCount = $MeasuredCycleCount
    $Manifest.route.warmupCycleCount = 1
    $Manifest.route.baselineWindowsPerState = 5
    $Manifest.route.memorySamplesPerWindow = 10
    $Manifest.route.memoryWindowIntervalMilliseconds = 100
    $Manifest.route.dedicatedWorkspaceCloseLifecycleEventClaimed = $false
    $Manifest.route.workflowCompletedAtUtc = Get-P914Utc
}

function Invoke-P916PackagedLifecycle {
    $runId = [DateTimeOffset]::UtcNow.ToString("yyyyMMdd'T'HHmmssfff'Z'") + '-' + [Guid]::NewGuid().ToString('N')
    $evidenceDirectory = Join-Path $EvidenceRoot $runId
    if (Test-Path -LiteralPath $evidenceDirectory) { throw "Refusing to overwrite P9-16 evidence: $evidenceDirectory" }
    foreach ($name in @('', 'logs', 'runtime', 'runtime/settings', 'runtime/local-app-data', 'runtime/app-data', 'runtime/temp', 'runtime/diagnostics', 'workspace', 'screenshots', 'uia', 'output')) { [void][IO.Directory]::CreateDirectory((Join-Path $evidenceDirectory $name)) }
    $manifestPath = Join-Path $evidenceDirectory 'manifest.json'
    $manifest = [ordered]@{
        schema = $Schema; taskId = $TaskId; runId = $runId; status = 'running'; startedAtUtc = Get-P914Utc; completedAtUtc = $null; repositoryRoot = $RepositoryRoot
        packageProvenance = $null; fixture = $null; settings = $null; process = $null; processExit = $null
        openMode = $OpenMode; acceptanceRole = $ArmRole
        experiment = [ordered]@{
            mode = $ExperimentMode
            kind = if ($IsFixtureSizeExperiment) { 'fixture-size-ab-diagnostic' } else { 'standard-repeated-workspace-route' }
            pairId = if ($IsFixtureSizeExperiment) { $ExperimentPairId } else { $null }
            fixtureSize = if ($IsFixtureSizeExperiment) { $FixtureSize } else { 'large' }
            primingCycleCount = $PrimingCycleCount; measuredCycleCount = $MeasuredCycleCount
            baselineWindowsPerState = 5; memorySamplesPerWindow = 10; memoryWindowIntervalMilliseconds = 100
        }
        route = [ordered]@{
            openMode = $OpenMode; acceptanceRole = $ArmRole
            experimentMode = $ExperimentMode; fixtureSize = if ($IsFixtureSizeExperiment) { $FixtureSize } else { 'large' }
            primingCycleCount = $PrimingCycleCount; measuredCycleCount = $MeasuredCycleCount
            workflow = $RouteWorkflow
            actions = [Collections.Generic.List[object]]::new(); cycles = [Collections.Generic.List[object]]::new()
            fileDialogSessions = [Collections.Generic.List[object]]::new(); eventLog = $null; actionLog = $null; routeEvents = @()
            recentOpenSessions = [Collections.Generic.List[object]]::new()
            baselineOpenEntry = $null; dedicatedWorkspaceCloseLifecycleEventClaimed = $false
        }
        memory = [ordered]@{
            windows = [Collections.Generic.List[object]]::new()
            thresholds = [ordered]@{ settledWorkingSetExclusiveBytes = $StrictWorkingSetLimit; strictAtOrAboveFails = $true; operationPeakWorkingSetExclusiveBytesAdvisory = $OperationPeakAdvisoryLimit; operationPeakAdvisoryOnly = $true; lifetimePeakDiagnosticOnly = $true; handlesAndThreadsDiagnosticOnly = $true }
            analysis = $null
        }
        cleanup = $null; failure = $null; validation = $null; applicationDiagnostics = $null
    }
    $failure = $null; $context = $null; $fixture = $null; $package = $null; $settings = $null; $completed = $false
    try {
        $package = Get-P916Package $RepositoryRoot $evidenceDirectory $PriorRunManifestPath
        $manifest.packageProvenance = $package
        $fixture = Get-P916Workspace $RepositoryRoot $evidenceDirectory $(if ($IsFixtureSizeExperiment) { $FixtureSize } else { 'large' })
        $manifest.fixture = [ordered]@{
            kind = 'repeated-workspace-open-close'; profileSize = [string]$fixture.preparation.profileSize; workspacePath = [string]$fixture.workspacePath
            preparationReceipt = [ordered]@{ relativePath = 'workspace/preparation-receipt.json'; sha256 = [string]$fixture.preparationSha256; workspaceSha256 = [string]$fixture.preparation.workspaceSha256; classCount = [int]$fixture.preparation.identity.classCount; classInfoCount = [int]$fixture.preparation.identity.classInfoCount; scheduleEntryCount = [int]$fixture.preparation.identity.scheduleEntryCount }
            beforeSnapshot = [ordered]@{ relativePath = 'workspace/before-snapshot.json'; sha256 = [string]$fixture.beforeSha256; phase = 'before' }
            afterSnapshot = $null; sourceBaseWorkspacePath = [string]$fixture.baseWorkspacePath; sourceSeedSqlPath = $fixture.seedSqlPath
        }
        $settings = Get-P916Settings $evidenceDirectory; $manifest.settings = $settings
        Write-P914Json $manifestPath $manifest

        $context = Start-P916App $evidenceDirectory ([string]$package.applicationPath)
        $context.workspacePath = [IO.Path]::GetFullPath([string]$fixture.workspacePath)
        $context.sampler = [P914ProcessSampler]::new($context.process)
        $null = Wait-P914MainRoot $context $evidenceDirectory $ReadinessTimeoutSeconds
        Drain-P914Output $context $evidenceDirectory
        $context.process.Refresh()
        $manifest.process = [ordered]@{
            processId = [Int64]$context.process.Id; processImagePath = [string]$context.processImagePath; processImageSha256 = [string]$context.processImageSha256
            architecture = 'x64'; startedAtUtc = $context.process.StartTime.ToUniversalTime().ToString('o'); workspacePath = [string]$fixture.workspacePath
            startedWithoutWorkspaceArgument = $true; commandLine = [ordered]@{ executable = [string]$package.applicationPath; workspaceArgument = $null }
            environmentIsolation = [ordered]@{ settingsRootFromEnvironment = $true; overriddenKeys = @('CLASSMNGR_SETTINGS_ROOT','CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH','LOCALAPPDATA','APPDATA','TEMP','TMP','QT_FORCE_STDERR_LOGGING','PATH'); globalDefaultsChanged = $false }
            stdoutPath = 'logs/process-stdout.log'; stderrPath = 'logs/process-stderr.log'; mainWindowHandle = [Int64]$context.mainWindowHandle
            windowProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$context.mainWindowHandle); hwndProcessId = [Int64][P914Native]::GetWindowProcessId([IntPtr]$context.mainWindowHandle)
            closeMethod = $null; forcedTermination = $false; metrics = $null; processMetricsPath = 'process-metrics.jsonl'
        }
        if ([Int64]$manifest.process.windowProcessId -ne [Int64]$context.process.Id -or [Int64]$manifest.process.hwndProcessId -ne [Int64]$context.process.Id) { throw 'Main Window HWND PID differs from the measured process.' }
        Set-P914SamplerContext $context 'normal-use' 'startup-ready-before-warmup'
        Invoke-P916Route $context $manifest $evidenceDirectory $fixture $ReadinessTimeoutSeconds
        Set-P914SamplerContext $context 'operation' 'final-natural-process-exit'
        $exit = Invoke-P914NaturalClose $context $manifest $evidenceDirectory $ExitTimeoutSeconds
        $manifest.processExit = $exit; $manifest.process.closeMethod = [string]$exit.closeMethod; $manifest.process.forcedTermination = $false
        $manifest.cleanup = Get-P914RuntimeCleanup $context $evidenceDirectory
        if (@($manifest.cleanup.workspaceSidecarFilesAfterExit).Count -ne 0) { throw 'Workspace sidecar files remain after the natural process exit.' }
        $manifest.fixture.afterSnapshot = Save-P916FinalWorkspaceSnapshot $evidenceDirectory $fixture
        $completed = $true
    } catch {
        $failure = [ordered]@{ message = $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = [string]$_.CategoryInfo.Category; fullyQualifiedErrorId = [string]$_.FullyQualifiedErrorId; position = [string]$_.InvocationInfo.PositionMessage; scriptStackTrace = [string]$_.ScriptStackTrace }
    } finally {
        if ($null -ne $context) {
            try { Drain-P914Output $context $evidenceDirectory } catch { }
            if ($null -ne $context.sampler) {
                try {
                    $context.process.Refresh()
                    if (-not $context.process.HasExited) {
                        try { $null = Invoke-P914NaturalClose $context $manifest $evidenceDirectory $ExitTimeoutSeconds; $manifest.processExit = $context.processExit; if ($null -ne $manifest.process) { $manifest.process.closeMethod = [string]$context.processExit.closeMethod } }
                        catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Natural close during cleanup failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
                    }
                    $context.sampler.Dispose()
                    $manifest.process.metrics = Get-P914SamplerSummary $context $evidenceDirectory
                    if ($null -ne $manifest.cleanup) { $manifest.cleanup = Get-P914RuntimeCleanup $context $evidenceDirectory }
                    elseif ($context.process.HasExited) { $manifest.cleanup = Get-P914RuntimeCleanup $context $evidenceDirectory }
                } catch { if ($null -eq $failure) { $failure = [ordered]@{ message = 'Final process evidence collection failed: ' + $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'cleanup' } } }
            }
            $manifest.route.routeEvents = Get-P914RouteSummary $context
            $manifest.route.completedAtUtc = Get-P914Utc
            $routePath = Join-Path $evidenceDirectory 'route-events.jsonl'; [IO.File]::WriteAllText($routePath, '', [Text.UTF8Encoding]::new($false))
            foreach ($item in $manifest.route.routeEvents) { Add-P914JsonLine $routePath $item }
            $actionPath = Join-Path $evidenceDirectory 'uia-actions.jsonl'; [IO.File]::WriteAllText($actionPath, '', [Text.UTF8Encoding]::new($false))
            foreach ($item in $manifest.route.actions) { Add-P914JsonLine $actionPath $item }
            $manifest.route.eventLog = [ordered]@{ relativePath = 'route-events.jsonl'; sha256 = Get-P914FileSha256 $routePath }
            $manifest.route.actionLog = [ordered]@{ relativePath = 'uia-actions.jsonl'; sha256 = Get-P914FileSha256 $actionPath }
        }
        $applicationOutputPath = Join-Path $evidenceDirectory 'application-output.jsonl'
        $processStderrPath = Join-Path (Join-Path $evidenceDirectory 'logs') 'process-stderr.log'
        if (Test-Path -LiteralPath $applicationOutputPath -PathType Leaf) {
            $manifest.applicationDiagnostics = [ordered]@{
                applicationOutput = [ordered]@{ relativePath = 'application-output.jsonl'; sha256 = Get-P914FileSha256 $applicationOutputPath }
                processStderr = if (Test-Path -LiteralPath $processStderrPath -PathType Leaf) { [ordered]@{ relativePath = 'logs/process-stderr.log'; sha256 = Get-P914FileSha256 $processStderrPath } } else { $null }
            }
        }
        $manifest.failure = $failure
        $manifest.status = if ($completed -and $null -eq $failure -and $null -ne $manifest.processExit -and [bool]$manifest.processExit.normalExit -and [int]$manifest.processExit.exitCode -eq 0) { 'evidence-collected' } else { 'failed' }
        $manifest.completedAtUtc = Get-P914Utc
        try { Write-P914Json $manifestPath $manifest } catch { }
        $validatorPath = Join-Path $PSScriptRoot 'validate_packaged_next_p9_16_repeated_workspace.py'
        $validationLog = Join-Path (Join-Path $evidenceDirectory 'logs') 'validator-output.txt'
        try {
            $validationLines = @(& python $validatorPath --evidence-dir $evidenceDirectory 2>&1 | ForEach-Object { [string]$_ }); $validationExit = $LASTEXITCODE
            [IO.File]::WriteAllLines($validationLog, $validationLines, [Text.UTF8Encoding]::new($false))
            $jsonStart = -1
            for ($lineIndex = 0; $lineIndex -lt $validationLines.Count; $lineIndex++) { if ($validationLines[$lineIndex].TrimStart().StartsWith('{')) { $jsonStart = $lineIndex; break } }
            $validationResult = $null
            if ($jsonStart -ge 0) {
                $jsonText = ($validationLines[$jsonStart..($validationLines.Count - 1)] -join [Environment]::NewLine)
                try { $validationResult = $jsonText | ConvertFrom-Json -ErrorAction Stop } catch { $validationResult = $null }
            }
            $validationStatus = if ($null -ne $validationResult -and [string]$validationResult.status -ceq 'diagnostic-only') { 'diagnostic-only' } elseif ($validationExit -eq 0) { 'passed' } elseif ($validationExit -eq 2) { 'inconclusive' } else { 'failed' }
            $evidenceIntegrityAndProvenanceStatus = $null; $evidenceValidationStatus = $null; $memoryStatus = $null
            if ($null -ne $validationResult) { $evidenceIntegrityAndProvenanceStatus = [string]$validationResult.evidenceIntegrityAndProvenanceStatus; $evidenceValidationStatus = [string]$validationResult.evidenceValidationStatus; $memoryStatus = [string]$validationResult.memoryStatus }
            $manifest.validation = [ordered]@{ status = $validationStatus; evidenceIntegrityAndProvenanceStatus = $evidenceIntegrityAndProvenanceStatus; evidenceValidationStatus = $evidenceValidationStatus; memoryStatus = $memoryStatus; exitCode = $validationExit; validatorPath = [IO.Path]::GetFullPath($validatorPath); validatorSha256 = Get-P914FileSha256 $validatorPath; result = $validationResult; logPath = 'logs/validator-output.txt'; logSha256 = Get-P914FileSha256 $validationLog }
        } catch { $manifest.validation = [ordered]@{ status = 'failed'; error = $_.Exception.Message } }
        if ($manifest.validation.status -ceq 'passed') { $manifest.status = 'passed' }
        elseif ($manifest.validation.status -ceq 'inconclusive') { $manifest.status = 'inconclusive' }
        elseif ($manifest.validation.status -ceq 'diagnostic-only') { $manifest.status = 'diagnostic-only' }
        else { $manifest.status = 'failed' }
        $manifest.completedAtUtc = Get-P914Utc
        try { Write-P914Json $manifestPath $manifest } catch { }
    }
    if ([string]$manifest.status -ceq 'passed') { Write-Output "P9-16 repeated workspace close/reopen passed: $evidenceDirectory"; return }
    if ([string]$manifest.status -ceq 'inconclusive') { Write-Error "P9-16 route completed; no-growth measurement is inconclusive. Evidence: $evidenceDirectory"; exit 2 }
    if ([string]$manifest.status -ceq 'diagnostic-only') { Write-Output "P9-16 Recent Files diagnostic completed; it is not primary acceptance evidence. Evidence: $evidenceDirectory"; return }
    [Console]::Error.WriteLine("P9-16 packaged repeated workspace route failed. Evidence preserved at $evidenceDirectory.")
    exit 1
}

Invoke-P916PackagedLifecycle
