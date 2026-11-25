param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$RunRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$RunRoot = [IO.Path]::GetFullPath($RunRoot)
. (Join-Path $RepositoryRoot 'scripts/phase9/p9_14_packaged_lifecycle_common.ps1')

$expectedRoot = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'artifacts/phase9/p9-19-exact-release-builds')).TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
if (-not $RunRoot.StartsWith($expectedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'P9-19 RunRoot must be a fresh child of artifacts/phase9/p9-19-exact-release-builds.' }
if (Test-Path -LiteralPath $RunRoot) { throw "Refusing to overwrite P9-19 exact-source build evidence: $RunRoot" }
[void][IO.Directory]::CreateDirectory((Join-Path $RunRoot 'logs'))
$buildDirectory = Join-Path $RunRoot 'build'
$standardRelease = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'build/windows-x64-release'))
$preservedPrefix = $standardRelease.TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
if ([IO.Path]::GetFullPath($buildDirectory).StartsWith($preservedPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'P9-19 build output cannot use the preserved build/windows-x64-release tree.' }

$expectedGenerator = 'Visual Studio 17 2022'
$expectedPlatform = 'x64'
$expectedToolset = 'v143'
$expectedInstance = [IO.Path]::GetFullPath('C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools')
$expectedToolsetVersion = '14.44.35207'
$expectedCompilerVersion = '19.44.35230.0'
$expectedSdkVersion = '10.0.26100.0'
$includedRoots = @('CMakeLists.txt','CMakePresets.json','cmake','docs/qt-rewrite/phase-4-resource-inventory.json','licenses','resources','scripts/phase4','src','tests','third_party')
$harnessPaths = @(
    'scripts/phase9/p9_14_packaged_lifecycle_common.ps1',
    'scripts/phase9/p9_19_build_current_source_release.ps1',
    'scripts/phase9/prepare_p9_19_large_startup_fixture.py',
    'scripts/phase9/run_packaged_next_p9_19_classes_lifecycle.ps1',
    'scripts/phase9/validate_packaged_next_p9_19_classes_lifecycle.py',
    'scripts/phase9/test_prepare_p9_19_large_startup_fixture.py',
    'scripts/phase9/test_p9_19_package_staging.ps1',
    'scripts/phase9/p9_19_virtualized_selection.ps1',
    'scripts/phase9/test_p9_19_virtualized_selection.ps1',
    'scripts/phase9/test_validate_packaged_next_p9_19_classes_lifecycle.py'
)
$source = Get-P914SourceInventory $RepositoryRoot $includedRoots
$sourcePath = Join-Path $RunRoot 'source-files.json'
Write-P914Json $sourcePath $source
$harnessRows = [Collections.Generic.List[object]]::new()
foreach ($relative in $harnessPaths) {
    $path = Join-Path $RepositoryRoot $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required P9-19 harness source is missing before build: $relative" }
    $harnessRows.Add([ordered]@{ path = $relative; sizeBytes = [Int64](Get-Item -LiteralPath $path).Length; sha256 = Get-P914FileSha256 $path })
}
$harnessFiles = @($harnessRows.ToArray())
$harness = [ordered]@{ repositoryRoot = $RepositoryRoot; fileCount = $harnessFiles.Count; sha256 = Get-P914InventoryFingerprint $harnessFiles; files = $harnessFiles }
$harnessPath = Join-Path $RunRoot 'harness-files.json'
Write-P914Json $harnessPath $harness

function Get-P919CacheValue([string]$Path, [string]$Key) {
    $matches = @(Select-String -LiteralPath $Path -Pattern ('^' + [regex]::Escape($Key) + ':[^=]+='))
    if ($matches.Count -ne 1) { throw "Expected exactly one CMake cache entry '$Key'." }
    return ($matches[0].Line -split '=', 2)[1]
}
function Invoke-P919Command([string]$Executable, [string[]]$Arguments, [string]$LogName, [string]$WorkingDirectory) {
    $logPath = Join-Path (Join-Path $RunRoot 'logs') $LogName
    $saved = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $lines = @(& $Executable @Arguments 2>&1 | ForEach-Object { [string]$_ }); $code = [int]$LASTEXITCODE }
    finally { $ErrorActionPreference = $saved }
    [IO.File]::WriteAllLines($logPath, $lines, [Text.UTF8Encoding]::new($false))
    [ordered]@{ executable = [IO.Path]::GetFullPath($Executable); arguments = $Arguments; workingDirectory = $WorkingDirectory; logPath = [IO.Path]::GetFullPath($logPath); logSha256 = Get-P914FileSha256 $logPath; exitCode = $code }
}
function Get-P919GeneratedProjectToolchain([string]$Directory) {
    $projects = @(Get-ChildItem -LiteralPath $Directory -Filter '*.vcxproj' -File -Recurse)
    if ($projects.Count -eq 0) { throw "No generated VS projects found under $Directory" }
    $toolsets = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $sdks = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($project in $projects) {
        $document = [xml]::new(); $document.Load($project.FullName)
        foreach ($node in $document.SelectNodes("//*[local-name()='PlatformToolset']")) { if (-not [string]::IsNullOrWhiteSpace([string]$node.InnerText)) { [void]$toolsets.Add(([string]$node.InnerText).Trim()) } }
        foreach ($node in $document.SelectNodes("//*[local-name()='WindowsTargetPlatformVersion']")) { if (-not [string]::IsNullOrWhiteSpace([string]$node.InnerText)) { [void]$sdks.Add(([string]$node.InnerText).Trim()) } }
    }
    if ($toolsets.Count -ne 1 -or -not $toolsets.Contains($expectedToolset)) { throw "Generated projects do not consistently use $expectedToolset." }
    if ($sdks.Count -ne 1 -or -not $sdks.Contains($expectedSdkVersion)) { throw "Generated projects do not consistently use Windows SDK $expectedSdkVersion." }
    [ordered]@{ projectFileCount = [int]$projects.Count; platformToolset = $expectedToolset; windowsSdkVersion = $expectedSdkVersion }
}
function ConvertTo-P919CanonicalLf([byte[]]$Bytes) {
    $stream = [IO.MemoryStream]::new()
    for ($index = 0; $index -lt $Bytes.Length; $index++) {
        if ($Bytes[$index] -eq 13 -and $index + 1 -lt $Bytes.Length -and $Bytes[$index + 1] -eq 10) {
            $stream.WriteByte(10)
            $index++
        } else {
            $stream.WriteByte($Bytes[$index])
        }
    }
    $canonical = [byte[]]$stream.ToArray()
    $stream.Dispose()
    return ,$canonical
}
function New-P919BuildSourceSnapshot([string]$SourceRoot, [string]$SnapshotRoot, [string[]]$Roots) {
    [void][IO.Directory]::CreateDirectory($SnapshotRoot)
    foreach ($relative in $Roots) {
        $sourcePath = Join-Path $SourceRoot $relative
        $destinationPath = Join-Path $SnapshotRoot $relative
        if (-not (Test-Path -LiteralPath $sourcePath)) { throw "Build source snapshot root is missing: $relative" }
        if (Test-Path -LiteralPath $sourcePath -PathType Leaf) {
            [void][IO.Directory]::CreateDirectory((Split-Path -Parent $destinationPath))
            [IO.File]::Copy($sourcePath,$destinationPath)
        } else {
            $sourcePrefix = [IO.Path]::GetFullPath($sourcePath).TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
            foreach ($file in @(Get-ChildItem -LiteralPath $sourcePath -File -Recurse)) {
                if ($file.FullName -match '[\\/](?:__pycache__|\.pytest_cache|\.mypy_cache)[\\/]' -or $file.Extension -in @('.pyc','.pyo')) { continue }
                $suffix = $file.FullName.Substring($sourcePrefix.Length)
                $target = Join-Path $destinationPath $suffix
                [void][IO.Directory]::CreateDirectory((Split-Path -Parent $target))
                [IO.File]::Copy($file.FullName,$target)
            }
        }
    }

    $inventoryPath = Join-Path $SnapshotRoot 'docs/qt-rewrite/phase-4-resource-inventory.json'
    $inventory = Read-P914Json $inventoryPath
    $normalizations = [Collections.Generic.List[object]]::new()
    $seenResources = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $snapshotPrefix = [IO.Path]::GetFullPath($SnapshotRoot).TrimEnd([char[]]@('\','/')) + [IO.Path]::DirectorySeparatorChar
    foreach ($collection in @('document_catalog_metadata','document_bodies','resources','excluded_resources','build_inputs','calendar_qml_sources')) {
        foreach ($record in @($inventory.$collection)) {
            $relative = [string]$record.source_path
            if ([string]::IsNullOrWhiteSpace($relative) -or -not $seenResources.Add($relative)) { throw "Resource inventory has a missing or duplicate source path: $relative" }
            $path = [IO.Path]::GetFullPath((Join-Path $SnapshotRoot $relative))
            if (-not $path.StartsWith($snapshotPrefix,[StringComparison]::OrdinalIgnoreCase)) { throw "Resource inventory source path escaped the build snapshot: $relative" }
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Pinned resource source is missing from the build snapshot: $relative" }
            $expected = $record.source_file
            if ($null -eq $expected -or $null -eq $expected.bytes -or [string]::IsNullOrWhiteSpace([string]$expected.sha256)) { throw "Pinned resource identity is incomplete: $relative" }
            $raw = [IO.File]::ReadAllBytes($path)
            $rawSha = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($raw)).ToLowerInvariant()
            if ($raw.Length -eq [Int64]$expected.bytes -and $rawSha -ceq [string]$expected.sha256) { continue }
            $canonical = ConvertTo-P919CanonicalLf $raw
            $canonicalSha = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($canonical)).ToLowerInvariant()
            if ($canonical.Length -ne [Int64]$expected.bytes -or $canonicalSha -cne [string]$expected.sha256) { throw "Source differs from its exact inventory bytes beyond CRLF-to-LF checkout expansion: $relative" }
            [IO.File]::WriteAllBytes($path,$canonical)
            $normalizations.Add([ordered]@{ path = $relative; originalRawBytes = [Int64]$raw.Length; originalRawSha256 = $rawSha; buildSourceBytes = [Int64]$canonical.Length; buildSourceSha256 = $canonicalSha; normalization = 'CRLF-to-LF only; exact pinned source bytes and SHA-256 verified' })
        }
    }
    [ordered]@{ root = [IO.Path]::GetFullPath($SnapshotRoot); normalizedResourceCount = [int]$normalizations.Count; normalizedResources = @($normalizations.ToArray()) }
}

$cachePath = Join-Path $standardRelease 'CMakeCache.txt'
$qtDiscovery = $null
if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
    $qt6Directory = [IO.Path]::GetFullPath((Get-P919CacheValue $cachePath 'Qt6_DIR'))
    $qtDiscovery = [ordered]@{ method = 'preserved Release CMake cache'; referencePath = [IO.Path]::GetFullPath($cachePath); referenceSha256 = Get-P914FileSha256 $cachePath }
} else {
    $qtpaths = Get-Command qtpaths6.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1
    $prefixLines = @(& $qtpaths.Source --query QT_INSTALL_PREFIX 2>&1 | ForEach-Object { [string]$_ })
    $prefixExitCode = [int]$LASTEXITCODE
    if ($prefixExitCode -ne 0 -or $prefixLines.Count -ne 1) { throw 'Qt install prefix could not be read from qtpaths6.' }
    $qtPrefix = [IO.Path]::GetFullPath($prefixLines[0].Trim())
    $qt6Directory = [IO.Path]::GetFullPath((Join-Path $qtPrefix 'lib/cmake/Qt6'))
    $qtDiscovery = [ordered]@{ method = 'installed Qt 6 qtpaths6 QT_INSTALL_PREFIX fallback; no preserved Release CMake cache exists'; referencePath = [IO.Path]::GetFullPath($qtpaths.Source); referenceSha256 = Get-P914FileSha256 $qtpaths.Source; installPrefix = $qtPrefix }
}
$qt6Config = Join-Path $qt6Directory 'Qt6Config.cmake'
if (-not (Test-Path -LiteralPath $qt6Config -PathType Leaf)) { throw "Qt6 CMake config is missing: $qt6Config" }
$vcRedistPath = [IO.Path]::GetFullPath((Join-Path $expectedInstance 'VC/Redist/MSVC/v143/vc_redist.x64.exe'))
if (-not (Test-Path -LiteralPath $vcRedistPath -PathType Leaf)) { throw "Pinned VS2022 v143 x64 redistributable is missing: $vcRedistPath" }
$vcRedistSignature = Get-AuthenticodeSignature -LiteralPath $vcRedistPath
if ([string]$vcRedistSignature.Status -cne 'Valid' -or [string]$vcRedistSignature.SignerCertificate.Subject -notmatch '^CN=Microsoft Corporation,') { throw 'Pinned VS2022 v143 x64 redistributable is not signed by Microsoft.' }
$toolsetVersionPath = Join-Path $expectedInstance 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.v143.default.txt'
$toolsetVersion = (Get-Content -LiteralPath $toolsetVersionPath -Raw -Encoding ASCII).Trim()
if ($toolsetVersion -cne $expectedToolsetVersion) { throw "Expected VS2022 toolset $expectedToolsetVersion, found $toolsetVersion." }
$compilerPath = [IO.Path]::GetFullPath((Join-Path $expectedInstance "VC/Tools/MSVC/$toolsetVersion/bin/Hostx64/x64/cl.exe"))
$compiler = [Diagnostics.FileVersionInfo]::GetVersionInfo($compilerPath)
if ([string]$compiler.FileVersion -cne $expectedCompilerVersion) { throw "Expected compiler $expectedCompilerVersion, found $($compiler.FileVersion)." }
$sdkRoot = Join-Path 'C:\Program Files (x86)\Windows Kits\10\Include' $expectedSdkVersion
if (-not (Test-Path -LiteralPath $sdkRoot -PathType Container)) { throw "Pinned Windows SDK is missing: $sdkRoot" }
$cmake = [IO.Path]::GetFullPath((Get-Command cmake.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source)
$buildSourceDirectory = Join-Path $RunRoot 'build-source'
$buildSourceNormalization = New-P919BuildSourceSnapshot $RepositoryRoot $buildSourceDirectory $includedRoots
$buildSource = Get-P914SourceInventory $buildSourceDirectory $includedRoots
$originalRows = @{}; foreach ($row in $source.files) { $originalRows[[string]$row.path] = $row }
$snapshotRows = @{}; foreach ($row in $buildSource.files) { $snapshotRows[[string]$row.path] = $row }
if ($originalRows.Count -ne $snapshotRows.Count) { throw 'Canonical build-source snapshot file count differs from the current product source.' }
$allowedNormalizations = @{}; foreach ($row in @($buildSourceNormalization.normalizedResources)) { $allowedNormalizations[[string]$row.path] = $row }
foreach ($path in $originalRows.Keys) {
    if (-not $snapshotRows.ContainsKey($path)) { throw "Canonical build-source snapshot omitted current product source: $path" }
    $originalRow = $originalRows[$path]; $snapshotRow = $snapshotRows[$path]
    if ([Int64]$originalRow.sizeBytes -eq [Int64]$snapshotRow.sizeBytes -and [string]$originalRow.sha256 -ceq [string]$snapshotRow.sha256) { continue }
    if (-not $allowedNormalizations.ContainsKey($path)) { throw "Build-source snapshot changed product source outside verified resources: $path" }
    $normalization = $allowedNormalizations[$path]
    if ([Int64]$originalRow.sizeBytes -ne [Int64]$normalization.originalRawBytes -or [string]$originalRow.sha256 -cne [string]$normalization.originalRawSha256 -or [Int64]$snapshotRow.sizeBytes -ne [Int64]$normalization.buildSourceBytes -or [string]$snapshotRow.sha256 -cne [string]$normalization.buildSourceSha256) { throw "Build-source snapshot normalization receipt differs from source inventories: $path" }
}
$buildSourceInventoryPath = Join-Path $RunRoot 'build-source-files.json'
Write-P914Json $buildSourceInventoryPath $buildSource
$redistArgument = '-DCLASSMNGR_INSTALLER_VC_REDIST_FILE=' + $vcRedistPath
$configureArgs = @('-S',$buildSourceDirectory,'-B',$buildDirectory,'-G',$expectedGenerator,'-A',$expectedPlatform,'-T',$expectedToolset,
    "-DCMAKE_GENERATOR_INSTANCE=$expectedInstance", "-DCMAKE_SYSTEM_VERSION=$expectedSdkVersion", "-DQt6_DIR=$qt6Directory", $redistArgument,
    '-DBUILD_TESTING=ON','-DCLASSMNGR_NEXT_RELEASE_PACKAGE=ON','-DCLASSMNGR_PREFER_QT_ZLIB_FALLBACK=ON',
    '-DCLASSMNGR_RUN_WINDEPLOYQT=OFF','-DCLASSMNGR_RESOURCE_PACK_CHECK_ON_STARTUP=OFF',
    "-DCMAKE_INSTALL_PREFIX=$(Join-Path $RunRoot 'package')")
$started = Get-P914Utc
$configure = Invoke-P919Command $cmake $configureArgs 'configure.log' $buildSourceDirectory
$build = $null; $focusedBuild = $null; $focusedTests = $null; $generatedToolchain = $null; $failure = $null
$releaseExecutable = Join-Path $buildDirectory 'next-package-build/Release/ClassMngr.exe'
$assetsRoot = Join-Path $buildDirectory 'generated/ClassMngrNext/Resources/assets'
$translationsRoot = Join-Path $buildDirectory 'generated/ClassMngrNext/assets/translations'
$catalogPath = Join-Path $buildDirectory 'generated/ClassMngrNext/resource-catalog.json'
$generatedResources = $null; $status = 'failed'
try {
    if ([int]$configure.exitCode -ne 0) { throw "Fresh CMake configure failed ($($configure.exitCode))." }
    $configuredCache = Join-Path $buildDirectory 'CMakeCache.txt'
    foreach ($entry in @(@('CMAKE_GENERATOR',$expectedGenerator),@('CMAKE_GENERATOR_PLATFORM',$expectedPlatform),@('CMAKE_GENERATOR_TOOLSET',$expectedToolset),@('CMAKE_GENERATOR_INSTANCE',$expectedInstance),@('CMAKE_SYSTEM_VERSION',$expectedSdkVersion),@('CMAKE_HOME_DIRECTORY',$buildSourceDirectory),@('Qt6_DIR',$qt6Directory),@('CLASSMNGR_NEXT_RELEASE_PACKAGE','ON'),@('BUILD_TESTING','ON'))) {
        $actual = Get-P919CacheValue $configuredCache ([string]$entry[0])
        if ([string]$entry[0] -in @('CMAKE_GENERATOR_INSTANCE','CMAKE_HOME_DIRECTORY','Qt6_DIR')) { $actual = [IO.Path]::GetFullPath($actual); $expected = [IO.Path]::GetFullPath([string]$entry[1]) } else { $expected = [string]$entry[1] }
        if ($actual -ine $expected) { throw "CMake cache field $($entry[0]) differs from the pinned value." }
    }
    $generatedToolchain = Get-P919GeneratedProjectToolchain $buildDirectory
    $buildArgs = @('--build',$buildDirectory,'--config','Release','--verbose','--target','ClassMngrNext','ClassMngrNextResourcePackage','--','/m:4','/p:TrackFileAccess=false')
    $build = Invoke-P919Command $cmake $buildArgs 'release-build.log' $buildSourceDirectory
    if ([int]$build.exitCode -ne 0) { throw "Fresh VS2022 x64 Release build failed ($($build.exitCode))." }
    $targets = @('ClassMngrNextBootstrapTests','ClassMngrNextPageHostTests','ClassMngrNextClassesNavigatorPageTests','ClassMngrNextClassesNavigatorVisualCapture')
    $focusedBuildArgs = @('--build',$buildDirectory,'--config','Release','--verbose','--target') + $targets + @('--','/m:4','/p:TrackFileAccess=false')
    $focusedBuild = Invoke-P919Command $cmake $focusedBuildArgs 'focused-test-build.log' $buildSourceDirectory
    $ctest = [IO.Path]::GetFullPath((Get-Command ctest.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source)
    $ctestArgs = @('--test-dir',$buildDirectory,'-C','Release','--output-on-failure','-R','^(ClassMngrNextBootstrapTests|ClassMngrNextPageHostTests|ClassMngrNextClassesNavigatorPageTests|ClassMngrNextClassesNavigatorVisualCapture)$')
    if ([int]$focusedBuild.exitCode -eq 0) {
        $focusedTests = Invoke-P919Command $ctest $ctestArgs 'focused-ctest.log' $buildDirectory
        $focusedTests.testNames = $targets
    } else {
        $focusedTests = [ordered]@{ executable = $ctest; arguments = $ctestArgs; workingDirectory = $RepositoryRoot; logPath = $null; logSha256 = $null; exitCode = [int]$focusedBuild.exitCode; testNames = $targets; blockedBy = 'focused test target build failed' }
    }
    if (-not (Test-Path -LiteralPath $releaseExecutable -PathType Leaf) -or (Get-P914PeMachine $releaseExecutable) -ne 0x8664) { throw 'Release executable is missing or is not Windows x64.' }
    if ((Get-Content -LiteralPath $build.logPath -Raw -Encoding UTF8).IndexOf($compilerPath,[StringComparison]::OrdinalIgnoreCase) -lt 0) { throw 'Release build log does not identify the pinned VS2022 x64 compiler path.' }
    foreach ($path in @($assetsRoot,$translationsRoot,$catalogPath)) { if (-not (Test-Path -LiteralPath $path)) { throw "Generated Next resource output is missing: $path" } }
    $assets = Get-P914CanonicalInventory $assetsRoot; $translations = Get-P914CanonicalInventory $translationsRoot
    $generatedResources = [ordered]@{ assetsRoot = [IO.Path]::GetFullPath($assetsRoot); assetsSha256 = [string]$assets.sha256; assetsFileCount = [int]$assets.fileCount; translationsRoot = [IO.Path]::GetFullPath($translationsRoot); translationsSha256 = [string]$translations.sha256; translationsFileCount = [int]$translations.fileCount; catalogPath = [IO.Path]::GetFullPath($catalogPath); catalogSha256 = Get-P914FileSha256 $catalogPath }
    $sourceAfter = Get-P914SourceInventory $RepositoryRoot $includedRoots
    if ([string]$sourceAfter.sha256 -cne [string]$source.sha256 -or [int]$sourceAfter.fileCount -ne [int]$source.fileCount) { throw 'Product source changed during the exact-source Release build.' }
    $buildSourceAfter = Get-P914SourceInventory $buildSourceDirectory $includedRoots
    if ([string]$buildSourceAfter.sha256 -cne [string]$buildSource.sha256 -or [int]$buildSourceAfter.fileCount -ne [int]$buildSource.fileCount) { throw 'Canonical build-source snapshot changed during the Release build.' }
    $status = if ([int]$focusedTests.exitCode -eq 0) { 'succeeded' } else { 'failed' }
    if ($status -cne 'succeeded') { throw "Focused P9-19 CTest failed ($($focusedTests.exitCode))." }
} catch { $failure = [ordered]@{ message = $_.Exception.Message; capturedAtUtc = Get-P914Utc; category = 'build-or-toolchain' } }
$receipt = [ordered]@{
    schema = 'classmngr-phase9-p9-19-exact-release-build-v1'; taskId = 'P9_19_PACKAGED_NEXT_CLASSES_NAVIGATION'; status = $status
    startedAtUtc = $started; completedAtUtc = Get-P914Utc; repositoryRoot = $RepositoryRoot
    sourceInventoryPath = [IO.Path]::GetFullPath($sourcePath); sourceInventoryFileSha256 = Get-P914FileSha256 $sourcePath; sourceSha256 = [string]$source.sha256; sourceFileCount = [int]$source.fileCount; sourceIncludedRoots = $includedRoots
    harnessInventoryPath = [IO.Path]::GetFullPath($harnessPath); harnessInventoryFileSha256 = Get-P914FileSha256 $harnessPath; harnessSourceSha256 = [string]$harness.sha256; harnessSourceFileCount = [int]$harness.fileCount
    buildSourceRoot = [IO.Path]::GetFullPath($buildSourceDirectory); buildSourceInventoryPath = [IO.Path]::GetFullPath($buildSourceInventoryPath); buildSourceInventoryFileSha256 = Get-P914FileSha256 $buildSourceInventoryPath; buildSourceSha256 = [string]$buildSource.sha256; buildSourceFileCount = [int]$buildSource.fileCount; buildSourceNormalization = $buildSourceNormalization
    buildHelperPath = [IO.Path]::GetFullPath($PSCommandPath); buildHelperSha256 = Get-P914FileSha256 $PSCommandPath
    preservedStandardReleasePath = $standardRelease; buildDirectory = [IO.Path]::GetFullPath($buildDirectory)
    configuration = 'Release'; platform = 'x64'; generator = $expectedGenerator; generatorPlatform = $expectedPlatform; generatorToolset = $expectedToolset; generatorInstance = $expectedInstance; windowsSdkVersion = $expectedSdkVersion
    compiler = [ordered]@{ path = $compilerPath; fileVersion = [string]$compiler.FileVersion; productVersion = [string]$compiler.ProductVersion; sha256 = Get-P914FileSha256 $compilerPath; toolsetVersion = $toolsetVersion }
    generatedProjectToolchain = $generatedToolchain; qt6Directory = $qt6Directory; qt6ConfigSha256 = Get-P914FileSha256 $qt6Config; qtDiscovery = $qtDiscovery
    vcRedist = [ordered]@{ path = $vcRedistPath; fileVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($vcRedistPath).FileVersion; sha256 = Get-P914FileSha256 $vcRedistPath; signatureStatus = [string]$vcRedistSignature.Status; signer = [string]$vcRedistSignature.SignerCertificate.Subject; signerThumbprint = [string]$vcRedistSignature.SignerCertificate.Thumbprint }
    cmakeExecutable = $cmake; configure = $configure; build = $build; focusedTestBuild = $focusedBuild; focusedTests = $focusedTests
    releaseExecutablePath = [IO.Path]::GetFullPath($releaseExecutable); releaseExecutableSha256 = if (Test-Path -LiteralPath $releaseExecutable) { Get-P914FileSha256 $releaseExecutable } else { $null }; releaseExecutableMachine = if (Test-Path -LiteralPath $releaseExecutable) { Get-P914PeMachine $releaseExecutable } else { $null }
    generatedResources = $generatedResources; failure = $failure
}
$receiptPath = Join-Path $RunRoot 'release-build-receipt.json'
Write-P914Json $receiptPath $receipt
if ($status -cne 'succeeded') { throw "P9-19 exact-source Release build failed: $($failure.message). See $receiptPath" }
Write-Host (ConvertTo-Json -Compress ([ordered]@{ status = $status; runRoot = $RunRoot; receipt = $receiptPath; sourceSha256 = [string]$source.sha256; buildSourceSha256 = [string]$buildSource.sha256; releaseExecutableSha256 = [string]$receipt.releaseExecutableSha256; generator = $expectedGenerator; toolset = $expectedToolset; compilerVersion = [string]$compiler.FileVersion; windowsSdkVersion = $expectedSdkVersion }))
